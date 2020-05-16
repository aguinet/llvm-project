#define DEBUG_TYPE "aec"

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Pass.h>
#include <llvm/Support/KnownBits.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/AffineExprCombine.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/InitializePasses.h>

#ifdef NDEBUG
#include <llvm/Support/Format.h>
#endif

#include "BitMatrix.h"

using namespace llvm::PatternMatch;
using namespace llvm;

namespace {

// This should only be used for testing/debugging purposes.
cl::opt<bool>
AECAlwaysProfitable("aec-always-profitable", 
  cl::desc("Always beleive applying AEC is profitable"),
  cl::init(false), cl::Hidden);


// Based on information in
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/ and
// https://rizmediateknologi.blogspot.com/2019/08/the-ice-lake-benchmark-preview-inside_1.html
// TODO: that's information we should be able to gather from LLVM itself?
static constexpr size_t X86LatencyInsert = 3;
static constexpr size_t X86LatencyExtract = 3;
static constexpr size_t X86LatencyGFNI = 3;

static ConstantInt* intOrSplatConstant(Value* V)
{
  if (!V->getType()->isIntOrIntVectorTy()) {
    return nullptr;
  }
  if (auto* CI = dyn_cast<ConstantInt>(V)) {
    return CI;
  }
  if (auto* C = dyn_cast<Constant>(V)) {
    return dyn_cast_or_null<ConstantInt>(C->getSplatValue());
  }
  return nullptr;
}

struct Expr
{
  Expr(Instruction* Out):
    Out_(Out)
  { }

  LLVMContext& getContext() const {
    return Out_->getContext();
  }

  Module& getModule() const {
    return *Out_->getModule();
  }

  bool add(Instruction* I) {
    if (KnownInstrs_.insert(I).second) {
      Instrs_.push_back(I);
      return true;
    }
    return false;
  }

  bool contains(Instruction* I) const {
    return KnownInstrs_.count(I);
  }

  void addInput(Value* V, Instruction* From = nullptr) {
    Inputs_.insert(V);
  }

  Value* singleInput() const {
    if (Inputs_.size() != 1) {
      return nullptr;
    }
    return *Inputs_.begin();
  }

  bool isVectorExpr() const {
    return Out_->getType()->isVectorTy();
  }

  bool empty() const {
    return Inputs_.empty() || Instrs_.empty();
  }

  void sort()
  {
    // TODO: this might be inefficient as hell as comesBefore can take linear
    // time if the result isn't already in cache.
    std::sort(std::begin(Instrs_), std::end(Instrs_),
      [](Instruction* A, Instruction* B) { return A->comesBefore(B); });
  }

#ifndef NDEBUG
  void dump() const {
    dump(dbgs());
  }
#endif

  void dump(raw_ostream& OS) const {
    OS << "AEC: inputs:\n";
    for (auto* V: Inputs_) {
      OS << "AEC:  " << *V << "\n";
    }
    OS << "AEC: output: " << *Out_ << "\n";
    OS << "AEC: instrs:\n";
    for (auto* I: iterInstrs()) {
      OS << "AEC:  " << *I << "\n";
    }
  }

  bool isProfitable() const {
    if (AECAlwaysProfitable) {
      return Instrs_.size() >= 2;
    }

    // TODO: this should use the LLVM cost model
    // TODO: a more precise estimation would be to compute the number of
    // parallel dataflow within the instruction set, and compute the maximum
    // latency (something that might already be done in llvm-mca).
    const size_t InstrsCount = Instrs_.size();
    if (isVectorExpr()) {
      return InstrsCount >= X86LatencyGFNI;
    }
    return InstrsCount >= (X86LatencyInsert+X86LatencyGFNI+X86LatencyExtract);
  }

  unsigned getInputSizeBits() const {
    return getSizeInBits(Inputs_);
  }
  unsigned getOutputSizeBits() const {
    return cast<IntegerType>(Out_->getType()->getScalarType())->getBitWidth();
  }

  auto const& inputs() const {
    return Inputs_;
  }

  auto const& instrs() const {
    return Instrs_;
  }

  Instruction* output() const {
    return Out_;
  }

  iterator_range<SmallVectorImpl<Instruction*>::const_iterator> iterInstrs() const {
    return make_range(Instrs_.begin(), Instrs_.end());
  }

  bool reduceToOneInput() {
    Value* NewInput = findOneInput();
    if (!NewInput) {
      LLVM_DEBUG(dbgs() << "AEC: reduceOneInput: couldn't find a unique input\n");
      return false;
    }
    LLVM_DEBUG(dbgs() << "AEC: reduceOneInput: expression will use " << *NewInput << " as input\n");
    Inputs_.clear();
    Inputs_.insert(NewInput);
    // Remove instructions until this one
    auto It = Instrs_.begin();
    for (; It != Instrs_.end(); ++It) {
      auto* I = *It;
      KnownInstrs_.erase(I);
      if (I == NewInput) {
        break;
      }
    }
    Instrs_.erase(Instrs_.begin(), ++It);
    return true;
  }

  Instruction* getOnlyUser(Value* I) const {
    Instruction* Ret = nullptr;
    size_t NInsideUsers = 0;
    for (User* U: I->users()) {
      if (auto* UI = dyn_cast<Instruction>(U)) {
        if (KnownInstrs_.count(UI)) {
          if ((++NInsideUsers) > 1) {
            return nullptr;
          }
          Ret = UI;
        }
      }
    }
    return Ret;
  }
private:
  Value* findOneInput() const {
    // Try and reduce the number of inputs by finding a common successor to
    // these inputs.

    // We support graphs like this:
    //   in0   in1   in2
    //    |     |     |
    //   ...   ...   ...
    //    |     \     /
    //           binop0
    //    |        |
    //   ...      ...
    //    \        / 
    //      binop1
    //        |
    //       ...
    //
    // It means that if one SSA value between the inputs and binop1 is used
    // twice, then we give up.

    // The algorithm taints each input, and find the first instruction that
    // will receive all the taints. On each iteration, we verify that the new
    // tainted values are only used once. Taint is propagated at each loop
    // iteration for each input, and we choose a hard limit where we won't go
    // further not to explode compilataion time.

    const size_t NInputs = Inputs_.size();
    if (NInputs >= 32) {
      // Do not go above 32 taints...
      return nullptr;
    }

    DenseMap<Value*, uint32_t> Taints;
    constexpr size_t LimitIteration = 8;

    SmallVector<Value*, 8> WorkList;
    SmallVector<Value*, 8> CurList;
    WorkList.reserve(NInputs);
    auto ItInputs = std::begin(Inputs_);
    for (size_t T = 0; T < Inputs_.size(); ++T) {
      auto* In = *(ItInputs++);
      Taints[In] = uint32_t(1)<<T;
      WorkList.push_back(In);
    }

    const uint32_t AllInputsMask = (1ULL<<NInputs)-1;

    size_t Iter = 0;
    while ((Iter++) < LimitIteration && !WorkList.empty()) {
      CurList = WorkList;
      WorkList.clear();
      for (Value* Inst: CurList) {
        const auto TI = Taints[Inst];
        if (TI == AllInputsMask) {
          // We found our candidate!
          return Inst;
        }
        auto* Next = getOnlyUser(Inst);
        if (!Next) {
          LLVM_DEBUG(dbgs() << "AEC: findOneInput: for " << *Inst << ", too many or no successor\n");
          continue;
        }
        LLVM_DEBUG(dbgs() << "AEC: findOneInput: for " << *Inst << ", unique successor is " << *Next << "\n");
        Taints[Next] |= TI;
        WorkList.push_back(Next);
      }
    }

    return nullptr;
  }

  template <class T>
  static unsigned getSizeInBits(SmallPtrSetImpl<T*> const& Vals) {
    unsigned Ret = 0;
    for (auto* V: Vals) {
      auto* IT = cast<IntegerType>(V->getType()->getScalarType());
      Ret += IT->getBitWidth();
    }
    return Ret;
  }

  SmallPtrSet<Value*, 2> Inputs_;
  Instruction* Out_;
  SmallVector<Instruction*, 32> Instrs_;
  SmallPtrSet<Instruction*, 32> KnownInstrs_;
};

struct ExprScalarEvaluator
{
  ExprScalarEvaluator(Expr const& E):
    E_(E)
  { }

  LLVMContext& getContext() const {
    return E_.getContext();
  }

  ConstantInt* operator()(uint8_t V) {
    auto& Ctx = getContext();
    return (*this)(ConstantInt::get(Type::getInt8Ty(Ctx), V));
  }

  ConstantInt* operator()(ConstantInt* In) {
    assert(E_.singleInput());
    Vals_.clear();
    Vals_[E_.singleInput()] = In;

    for (auto* I: E_.iterInstrs()) {
      Constant* V = eval(*I);
      if (!V) {
        return nullptr;
      }
      Vals_[I] = V;
    }
    return dyn_cast<ConstantInt>(Vals_[E_.output()]);
  }

private:
  Constant* getVal(Value* V) const {
    if (auto* CI = intOrSplatConstant(V)) {
      return CI;
    }
    auto It = Vals_.find(V);
    assert(It != Vals_.end());
    return It->second;
  }

  Constant* eval(Instruction& I) {
    if (auto* BO = dyn_cast<BinaryOperator>(&I)) {
      return evalBO(*BO);
    }
    if (auto* GEP = dyn_cast<GetElementPtrInst>(&I)) {
      return evalGEP(*GEP);
    }
    if (auto* LI = dyn_cast<LoadInst>(&I)) {
      return evalLoad(*LI);
    }
    if (auto* LI = dyn_cast<ZExtInst>(&I)) {
      return evalZExt(*LI);
    }
    if (auto* SI = dyn_cast<SExtInst>(&I)) {
      return evalSExt(*SI);
    }

    return nullptr;
  }

  ConstantInt* evalBO(BinaryOperator& BO) const {
    auto* A = getVal(BO.getOperand(0));
    auto* B = getVal(BO.getOperand(1));
    return dyn_cast<ConstantInt>(ConstantExpr::get(BO.getOpcode(), A, B));
  }

  Constant* evalGEP(GetElementPtrInst& GEP) const {
    SmallVector<Constant*, 2> Indices;
    Indices.reserve(GEP.getNumIndices());
    for (auto& U: GEP.indices()) {
      Indices.push_back(getVal(U.get()));
    }
    return ConstantExpr::getGetElementPtr(GEP.getSourceElementType(),
      cast<Constant>(GEP.getPointerOperand()),
      Indices,
      GEP.isInBounds());
  }

  ConstantInt* evalLoad(LoadInst& LI) const {
    // All these assumptions have been verified by isLoadFromAffineGV
    auto* GEP = cast<ConstantExpr>(getVal(LI.getPointerOperand()));
    auto* C = cast<GlobalVariable>(GEP->getOperand(0))->getInitializer();
    return cast<ConstantInt>(ConstantFoldLoadThroughGEPConstantExpr(C, GEP));
  }

  ConstantInt* evalZExt(ZExtInst& ZI) const {
    return cast<ConstantInt>(ConstantExpr::getZExt(
      getVal(ZI.getOperand(0)),
      ZI.getDestTy()->getScalarType()));
  }

  ConstantInt* evalSExt(SExtInst& SI) const {
    return cast<ConstantInt>(ConstantExpr::getSExt(
      getVal(SI.getOperand(0)),
      SI.getDestTy()->getScalarType()));
  }

  Expr const& E_;
  DenseMap<Value*, Constant*> Vals_;
};

struct AffineFunc
{
  AffineFunc(BitMatrix&& M, uint64_t V):
    M_(std::move(M))
  {
    setCst(V);
  }
  AffineFunc(AffineFunc&&) = default;
  AffineFunc& operator=(AffineFunc&&) = default;

  BitMatrix& matrix() { return M_; }
  BitMatrix const& matrix() const { return M_; }

  void setCst(uint64_t V) {
    C_ = V & M_.outMask();
  }

  uint64_t cst() const { return C_; }

  bool isSquare(unsigned N) const { return M_.isSquare(N); }

  void reverseRows() { M_.reverseRows(); }

private:
  BitMatrix M_;
  uint64_t C_;
};

static bool hasFeature(Function const& F, StringRef const AddFeature, StringRef const RemFeature)
{
  Attribute TFAttr = F.getFnAttribute("target-features");
  if (TFAttr.hasAttribute(Attribute::None)) {
    return false;
  }
  SmallVector<StringRef, 16> Features;
  TFAttr.getValueAsString().split(Features, ',');
  for (auto Feature : Features) {
    if (Feature == RemFeature) {
      return false;
    }
    if (Feature == AddFeature) {
      return true;
    }
  }

  return false;
}

bool supported(Function const& F)
{
  // For now, we only support the GFNI AVX512 extension. Search
  // for it in the target-feature of the function F.
  return hasFeature(F, "+gfni", "-gfni"); 
}

//
// Instruction filtering
// The idea is to verify that the instructions we select have an affine
// behavior.
//

// This is used to gather which basic block can be interesting (in
// canBeProfitable). To have this process as fast as possible, this only
// verifies properties on a per-instruction basis, and does not go beyond that.
// For this reason, we might select instructions that generate non-affine
// behavior.
// isSupportedInstr() is the sound way to do this and is used during
// instruction selection.
static bool maybeSupportedInstr(Instruction& I) {
  if (auto* BO = dyn_cast<BinaryOperator>(&I)) {
    const auto Op = BO->getOpcode();
    if (!I.getType()->isIntOrIntVectorTy()) {
      return false;
    }

    switch (Op) {
      case Instruction::LShr:
      case Instruction::AShr:
      case Instruction::Shl:
        return intOrSplatConstant(BO->getOperand(1));
      case Instruction::Xor:
      case Instruction::Or:
      case Instruction::And:
      case Instruction::Sub:
        return true;
      default:
        break;
    }
  }
  return isa<ZExtInst>(I) || isa<SExtInst>(I) ||
         isa<GetElementPtrInst>(I) || isa<LoadInst>(I);
}

static bool isAndOrLinear(BinaryOperator& BO)
{
  auto* A = BO.getOperand(0);
  auto* B = BO.getOperand(1);

  // Simple case is when one the arguments is an integer.
  if (intOrSplatConstant(A) || intOrSplatConstant(B)) {
    return true;
  }

  const unsigned BitWidth = cast<IntegerType>(BO.getType()->getScalarType())->getBitWidth();

  // Compute known bits of the arguments
  const auto& DL = BO.getModule()->getDataLayout();
  KnownBits KBA(BitWidth), KBB(BitWidth);
  computeKnownBits(A, KBA, DL);
  computeKnownBits(B, KBB, DL);

  if (KBA.isConstant() || KBB.isConstant()) {
    return true;
  }

  // Masks of known bits
  const APInt MA = (KBA.Zero) | (KBA.One);
  const APInt MB = (KBB.Zero) | (KBB.One);

  // This is a linear operation iif there is no common unknown bits between A
  // and B.
  const bool Ret = ((~MA) & (~MB)).isNullValue();
  return Ret;
}

static bool isSubLinear(BinaryOperator& BO)
{
  Value* X;
  if (!match(&BO, m_Sub(m_Zero(), m_Value(X)))) {
    return false;
  }

  if (intOrSplatConstant(X)) {
    return true;
  }

  const unsigned BitWidth = cast<IntegerType>(BO.getType()->getScalarType())->getBitWidth();
  const auto& DL = BO.getModule()->getDataLayout();
  KnownBits KBX(BitWidth);
  computeKnownBits(X, KBX, DL);

  if (KBX.isConstant()) {
    return true;
  }

  // Checks that we have 000...0x (x being the LSB).
  // x can't be known, otherwise X would be a constant (checked above).
  return (~KBX.Zero).isOneValue() && KBX.One.isNullValue();
}

static bool isLoadFromAffineGV(LoadInst& LI)
{
  // This is a very naïve analysis and does not handle many cases, like arrays
  // in structs, or pointer arithmetic.
  auto* Ptr = LI.getPointerOperand();
  auto* GEP = dyn_cast<GetElementPtrInst>(Ptr);
  if (!GEP) {
    return false;
  }

  if (GEP->getNumIndices() != 2) {
    return false;
  }
  auto* FirstIdx = dyn_cast<ConstantInt>(*GEP->idx_begin());
  if (!FirstIdx || !FirstIdx->isZero()) {
    return false;
  }

  Ptr = GEP->getPointerOperand();
  auto* GV = dyn_cast<GlobalVariable>(Ptr);
  if (!GV) {
    return false;
  }
  if (!GV->isConstant() || !GV->hasDefinitiveInitializer()) {
    return false;
  }
  auto* GVTy = dyn_cast<ArrayType>(GV->getType()->getElementType());
  if (!GVTy) {
    return false;
  }

  auto& Ctx = LI.getContext();
  if (GVTy->getElementType() != Type::getInt8Ty(Ctx)) {
    return false;
  }

  const size_t NElts = GVTy->getNumElements();
  if (NElts > 256) {
    return false;
  }

  // If this isn't a power of two, we will miss some values
  if (!isPowerOf2_64(NElts)) {
    return false;
  }

  Constant const* Init = GV->getInitializer();
  if (isa<ConstantAggregateZero>(Init)) {
    return true;
  }
  if (auto* CDA = dyn_cast<ConstantDataArray>(Init)) {
    // Is this an affine substitution box?
    const uint8_t V0 = CDA->getElementAsInteger(0);
    for (size_t i = 0; i < NElts; ++i) {
      for (size_t j = i; j < NElts; ++j) {
        const uint8_t A = CDA->getElementAsInteger(i);
        const uint8_t B = CDA->getElementAsInteger(j);
        const uint8_t Xor = CDA->getElementAsInteger(i^j);
        if ((A^B^V0) != Xor) {
          return false;
        }
      }
    }
    return true;
  }

  return false;
}

static bool isSupportedInstr(Instruction& I, Expr const* E = nullptr) {
  if (!maybeSupportedInstr(I)) {
    return false;
  }

  bool Ret = true;
  if (auto* BO = dyn_cast<BinaryOperator>(&I)) {
    const auto Op = BO->getOpcode();
    switch (Op) {
      case Instruction::Sub:
        // For sub, verify that we do (sub 0, X), where (X & ~1) == 0, which is
        // a way to broadcast the bit (X & 1). This is a linear transformation.
        Ret = isSubLinear(*BO);
        break;
      case Instruction::Or:
      case Instruction::And:
        // The idea is to verify, using the known-bits analysis, that no
        // non-linear behavior arises from these instructions.
        // For instance, a rol<2> of a 8-bit integer instruction would appear as:
        //   %1 = shl %0, 2
        //   %2 = lshr %0, 6
        //   %3 = or %1 %2
        // In this case, we do have a linear transformation.
        Ret = isAndOrLinear(*BO);
        break;
      default:
        break;
    }
  }
  else
  if (auto* GEP = dyn_cast<GetElementPtrInst>(&I)) {
    // Verify that this is GEP used by at least one load we support
    Ret = false;
    if (E) {
      for (auto* U: GEP->users()) {
        if (auto* I = dyn_cast<Instruction>(U)) {
          if (E->contains(I)) {
            Ret = true;
            break;
          }
        }
      }
    }
  }
  else
  if (auto* LI = dyn_cast<LoadInst>(&I)) {
    // We basically want to support this kind of code:
    // static uint8_t GV[T] = { ... }
    // uint8_t foo(uint8_t v) { return GV[v]; }
    //
    // with:
    // * T <= 256 and GV an array such as:
    // * For every x,y < T, GV[x^y] == GV[x]^GV[y]^GV[0]
    Ret = isLoadFromAffineGV(*LI);
  }

  if (!Ret) {
    LLVM_DEBUG(dbgs() << "AEC: instruction happened to be not supported: " << I << "\n");
  }
  return Ret;
}

// This is a very dumb cost estimation to filter out basic blocks where there
// is close to no chance to get some profit. The place for a more "precise"
// analysis is Expr::isProfitable.
static bool canBeProfitable(BasicBlock& BB)
{
  if (AECAlwaysProfitable) {
    return true;
  }

  size_t Count = 0;
  for (Instruction& I: BB) {
    if (maybeSupportedInstr(I)) {
      ++Count;
    }
  }

  // TODO: the threshold should be determined related to a selected backend.
  // For now, this is hardcoded for X86.
  return Count >= X86LatencyGFNI;
}

//
// Affine function generation
//


static Optional<AffineFunc> exprToAffine(Expr const& E)
{
  const unsigned InBits = E.getInputSizeBits();
  const unsigned OutBits = E.getOutputSizeBits();

  auto& Ctx = E.getContext();
  // Transform E into M*X+C
  ExprScalarEvaluator Eval(E);
  auto* InTy = Type::getIntNTy(Ctx, InBits);
  ConstantInt* CCst = Eval(ConstantInt::get(InTy, 0));
  if (!CCst) {
    return {};
  }
  const uint64_t Cst = CCst->getZExtValue();

  // We write the transposed matrix, so inverse the input and output for now.
  BitMatrix M(OutBits, InBits);
  M.setZero();

  for (unsigned B = 0; B < InBits; ++B) {
    uint64_t V = uint64_t(1)<<B;
    ConstantInt* CV = Eval(ConstantInt::get(InTy, V));
    if (!CV) {
      return {};
    }
    V = CV->getZExtValue() ^ Cst;
    M.setRow(B, V);
  }

  auto TM = M.transpose();

#ifndef NDEBUG
  LLVM_DEBUG(dbgs() << "AEC: exprToAffine: C = " << format_hex(Cst, (OutBits+7)/8) << ", M = \n");
  LLVM_DEBUG(TM.dump(dbgs()));
#endif

  return {AffineFunc{std::move(TM), Cst}};
}

// 
// GFNI instructions generation
// TODO: put this into a "GFNI backend", so that we could easily adapt our
// transformation for other architectures if necessary.
//

static Function* getGFNIIntrin(Function& F, unsigned VecSizeBytes)
{
  Intrinsic::ID GF2Intr;
  switch (VecSizeBytes) {
    case 16:
      GF2Intr = Intrinsic::x86_vgf2p8affineqb_128;
      break;
    case 32:
      // According to clang/include/Basic/BuiltinsX86.def, this also requires AVX.
      GF2Intr = Intrinsic::x86_vgf2p8affineqb_256;
      if (!hasFeature(F, "+avx", "-avx")) {
        LLVM_DEBUG(dbgs() << "AEC: can't replace instructions because AVX isn't available\n");
        return nullptr;
      }
      break;
    case 64:
    {
      GF2Intr = Intrinsic::x86_vgf2p8affineqb_512;
      // According to clang/include/Basic/BuiltinsX86.def, this also requires AVX512BW.
      if (!hasFeature(F, "+avx512bw", "-avx512bw")) {
        LLVM_DEBUG(dbgs() << "AEC: can't replace instructions because AVX512BW isn't available\n");
        return nullptr;
      }
      break;
    }
    default:
      return nullptr;
  }
  return Intrinsic::getDeclaration(F.getParent(), GF2Intr);
}

static Value* computeLinearPartVector32x32(AffineFunc const& AffFunc, IRBuilder<>& IRB, Value* In)
{
  auto* F = IRB.GetInsertBlock()->getParent();
  auto* GF2p8affineFunc = getGFNIIntrin(*F, 64);
  if (!GF2p8affineFunc) {
    return nullptr;
  }

  auto& Ctx = IRB.getContext();
  Type* I8Ty  = Type::getInt8Ty (Ctx);
  Type* I32Ty = Type::getInt32Ty(Ctx);
  Type* I64Ty = Type::getInt64Ty(Ctx);

  Type* Veci32x8Ty = VectorType::get(I32Ty, 8);
  Type* Veci8x32Ty = VectorType::get(I8Ty, 32);
  assert(In->getType() == Veci32x8Ty);

  // We first need to split the 32x32 matrix into 16 sub-matrixes of 8x8
  std::array<std::array<uint64_t, 4>, 4> Sub8x8Ms;
  for (size_t R = 0; R < 4; ++R) {
    for (size_t C = 0; C < 4; ++C) {
      const auto M = AffFunc.matrix().sub8x8Matrix(8*C, 8*R);
      LLVM_DEBUG(dbgs() << "subMatrix(" << R << "," << C << ") = \n");
      LLVM_DEBUG(M.dump(dbgs()));
      Sub8x8Ms[R][C] = M.toIntel8x8Repr();
    }
  }

  Type* Veci8x64Ty = VectorType::get(I8Ty, 64);
  Type* Veci64x8Ty = VectorType::get(I64Ty, 8);


  // With Y0..7 the 8 32-bit integers we want to process, we need two 512-bits vectors like this:
  //   (with Yx=X0,x .. X3,x)
  //
  // < ------------ Vector -------------- >
  //   0                                63
  // (X0,0 ... X0,7)*4 | (X1,0 ... X1,7)*4
  // (X2,0 ... X2,7)*4 | (X3,0 ... X3,7)*4
  // Don't forget integers are little-endian on X86

  constexpr uint32_t Mask0[] = {
    // X0,*
    0, 4, 8, 12, 16, 20, 24, 28,
    // X0,*
    0, 4, 8, 12, 16, 20, 24, 28,
    // X0,*
    0, 4, 8, 12, 16, 20, 24, 28,
    // X0,*
    0, 4, 8, 12, 16, 20, 24, 28,
    // X1,*
    1, 5, 9, 13, 17, 21, 25, 29,
    // X1,*
    1, 5, 9, 13, 17, 21, 25, 29,
    // X1,*
    1, 5, 9, 13, 17, 21, 25, 29,
    // X1,*
    1, 5, 9, 13, 17, 21, 25, 29
  };
 
  constexpr uint32_t Mask1[] = {
    // X2,*
    2, 6, 10, 14, 18, 22, 26, 30,
    // X2,*
    2, 6, 10, 14, 18, 22, 26, 30,
    // X2,*
    2, 6, 10, 14, 18, 22, 26, 30,
    // X2,*
    2, 6, 10, 14, 18, 22, 26, 30,
    // X3,*
    3, 7, 11, 15, 19, 23, 27, 31,
    // X3,*
    3, 7, 11, 15, 19, 23, 27, 31,
    // X3,*
    3, 7, 11, 15, 19, 23, 27, 31,
    // X3,*
    3, 7, 11, 15, 19, 23, 27, 31,
  };
  auto* TIn0 = IRB.CreateShuffleVector(
    IRB.CreateBitCast(In, Veci8x32Ty),
    UndefValue::get(Veci8x32Ty),
    ConstantDataVector::get(Ctx, Mask0));
  auto* TIn1 = IRB.CreateShuffleVector(
    IRB.CreateBitCast(In, Veci8x32Ty),
    UndefValue::get(Veci8x32Ty),
    ConstantDataVector::get(Ctx, Mask1));

  auto* M0 = ConstantDataVector::get(Ctx,
    makeArrayRef({
      Sub8x8Ms[0][0], Sub8x8Ms[1][0], Sub8x8Ms[2][0], Sub8x8Ms[3][0],
      Sub8x8Ms[0][1], Sub8x8Ms[1][1], Sub8x8Ms[2][1], Sub8x8Ms[3][1]}));
  auto* M1 = ConstantDataVector::get(Ctx,
    makeArrayRef({
      Sub8x8Ms[0][2], Sub8x8Ms[1][2], Sub8x8Ms[2][2], Sub8x8Ms[3][2],
      Sub8x8Ms[0][3], Sub8x8Ms[1][3], Sub8x8Ms[2][3], Sub8x8Ms[3][3]}));
  auto* R0 = IRB.CreateCall(GF2p8affineFunc, {
    TIn0,
    IRB.CreateBitCast(M0, Veci8x64Ty),
    ConstantInt::get(I8Ty, 0)
  });
  auto* R1 = IRB.CreateCall(GF2p8affineFunc, {
    TIn1,
    IRB.CreateBitCast(M1, Veci8x64Ty),
    ConstantInt::get(I8Ty, 0)
  });
  auto* L0 = IRB.CreateBitCast(
    IRB.CreateXor(R0, R1),
    Veci64x8Ty);

  // L = xor low 128 bits of L0 with high 128 bits of L0
  // Then we need to transpose back to 8 32-bit integers

  auto* Undefi64x8 = UndefValue::get(Veci64x8Ty);
  auto* L = IRB.CreateXor(
    IRB.CreateShuffleVector(L0, Undefi64x8,
      ConstantDataVector::get(Ctx, makeArrayRef({0U, 1U, 2U, 3U}))),
    IRB.CreateShuffleVector(L0, Undefi64x8,
      ConstantDataVector::get(Ctx, makeArrayRef({4U, 5U, 6U, 7U})))
  );

  constexpr uint32_t TransposeBack[] = {
     0,  8, 16, 24,
     1,  9, 17, 25,
     2, 10, 18, 26,
     3, 11, 19, 27,
     4, 12, 20, 28,
     5, 13, 21, 29,
     6, 14, 22, 30,
     7, 15, 23, 31};

  auto* Ret = IRB.CreateShuffleVector(
    IRB.CreateBitCast(L, Veci8x32Ty),
    UndefValue::get(Veci8x32Ty),
    ConstantDataVector::get(Ctx, TransposeBack));

  return IRB.CreateBitCast(Ret, Veci32x8Ty);
}

static bool replaceVectorExpr32x32(AffineFunc const& AffFunc, Expr const& E)
{
  auto& Ctx = E.getContext();
  Value* In = E.singleInput();
  Instruction* Out = E.output();

  IRBuilder<> IRB(Out);

  Type* Veci32x8Ty  = VectorType::get(Type::getInt32Ty(Ctx), 8);
  if (In->getType() != Veci32x8Ty) {
    return false;
  }

  Value* Ret = computeLinearPartVector32x32(AffFunc, IRB, In);
  if (!Ret) {
    return false;
  }
  Ret = IRB.CreateXor(Ret, ConstantInt::get(Veci32x8Ty, AffFunc.cst()));
  Out->replaceAllUsesWith(Ret);
  return true;
}

static bool replaceScalarExpr32x32(AffineFunc const& AffFunc, Expr const& E)
{
  // Create a i32x8 vector with only the input and zeros, feed that to
  // computeLinearPartVector32x32, and get back our i32
  auto& Ctx = E.getContext();
  auto* I32Ty = Type::getInt32Ty(Ctx);
  auto* Veci32x8 = VectorType::get(I32Ty, 8);
  Instruction* Out = E.output();
  IRBuilder<> IRB(Out);
  Value* IRV = IRB.CreateInsertElement(UndefValue::get(Veci32x8), E.singleInput(), uint64_t(0));
  IRV = computeLinearPartVector32x32(AffFunc, IRB, IRV);
  if (!IRV) {
    if (auto* I = dyn_cast<Instruction>(IRV)) {
      I->eraseFromParent();
    }
    return false;
  }
  IRV = IRB.CreateExtractElement(IRV, uint64_t(0));
  IRV = IRB.CreateXor(IRV, ConstantInt::get(I32Ty, AffFunc.cst()));

  Out->replaceAllUsesWith(IRV);
  return true;
}

static Value* computeAffineVector8x8(AffineFunc const& AffFunc, IRBuilder<>& IRB, Value* In)
{
  const auto C = AffFunc.cst();
  auto& Ctx = IRB.getContext();

  auto* VecTy = cast<VectorType>(In->getType());
  if (VecTy->getElementType() != Type::getInt8Ty(Ctx)) {
    return nullptr;
  }

  const unsigned NumBytes = VecTy->getNumElements();
  if (NumBytes % 8 != 0) {
    return nullptr;
  }

  Type* I64Ty = Type::getInt64Ty(Ctx);
  Type* I8Ty  = Type::getInt8Ty (Ctx);

  uint64_t VM = AffFunc.matrix().toIntel8x8Repr();
  auto* IRM = ConstantVector::getSplat(ElementCount{NumBytes/8, false}, ConstantInt::get(I64Ty, VM));

  auto* F = IRB.GetInsertBlock()->getParent();
  auto* GF2p8affineFunc = getGFNIIntrin(*F, NumBytes);
  return IRB.CreateCall(GF2p8affineFunc, {
    In,
    IRB.CreateBitCast(IRM, VecTy),
    ConstantInt::get(I8Ty, C)
  });
}

static bool replaceVectorExpr8x8(AffineFunc const& AffFunc, Expr const& E)
{
  IRBuilder<> IRB(E.output());
  auto* V = computeAffineVector8x8(AffFunc, IRB, E.singleInput());
  if (!V) {
    return false;
  }
  E.output()->replaceAllUsesWith(V);
  return true;
}

static bool replaceScalarExpr8x8(AffineFunc const& AffFunc, Expr const& E)
{
  auto& Ctx = E.getContext();
  Type* I64Ty = Type::getInt64Ty(Ctx);
  Type* I8Ty  = Type::getInt8Ty (Ctx);
  Type* Veci8x16Ty = VectorType::get(I8Ty, 16);
  IRBuilder<> IRB(E.output());

  auto* IRV = IRB.CreateInsertElement(UndefValue::get(Veci8x16Ty), E.singleInput(), uint64_t(0));
  auto* IRValVec = computeAffineVector8x8(AffFunc, IRB, IRV);
  if (!IRValVec) {
    if (auto* I = dyn_cast<Instruction>(IRV)) {
      I->eraseFromParent();
    }
    return false;
  }
  auto* IRRet = IRB.CreateExtractElement(IRValVec, ConstantInt::get(I64Ty, 0));

  E.output()->replaceAllUsesWith(IRRet);

  return true;
}

static bool replaceScalarExpr(AffineFunc const& AffFunc, Expr const& E)
{
  if (AffFunc.isSquare(8)) {
    return replaceScalarExpr8x8(AffFunc, E);
  }
  if (AffFunc.isSquare(32)) {
    return replaceScalarExpr32x32(AffFunc, E);
  }
  return false;
}

static bool replaceVectorExpr(AffineFunc const& AffFunc, Expr const& E)
{
  if (AffFunc.isSquare(8)) {
    return replaceVectorExpr8x8(AffFunc, E);
  }
  if (AffFunc.isSquare(32)) {
    return replaceVectorExpr32x32(AffFunc, E);
  }
  return false;
}

static void localDCE(ArrayRef<Instruction*> Is)
{
  for (Instruction* I: make_range(Is.rbegin(), Is.rend())) {
    if (isInstructionTriviallyDead(I)) {
      I->eraseFromParent();
    }
  }
}

// 
// Bottom-up instruction selection algorithm
//

static Instruction* nextValidInstr(BasicBlock::reverse_iterator& It, BasicBlock::reverse_iterator End) {
  while (It != End) {
    auto& I = *(It++);
    if (isSupportedInstr(I)) {
      return &I;
    }
  }
  return nullptr;
}

static Expr findLongestExpr(Instruction* I, BasicBlock::reverse_iterator& It, BasicBlock::reverse_iterator ItEnd)
{
  Expr Ret(I);

  SmallVector<Instruction*, 16> WorkList;
  WorkList.push_back(I);

  BasicBlock* CurBB = I->getParent();

  while (!WorkList.empty()) {
    auto* I = WorkList.back();
    WorkList.pop_back();
    if (!Ret.add(I)) {
      continue;
    }

    for (auto& U: I->operands()) {
      auto* A = U.get();
      if (isa<Argument>(A) || isa<PHINode>(A)) {
        Ret.addInput(A);
        continue;
      }
      auto* UI = dyn_cast<Instruction>(A);
      if (!UI) {
        continue;
      }

      if (UI->getParent() != CurBB) {
        Ret.addInput(A);
        continue;
      }

      if (isSupportedInstr(*UI, &Ret)) {
        WorkList.push_back(UI);
      }
      else {
        Ret.addInput(UI);
      }
    }
  }

  Ret.sort();
  return Ret;
}

static void advanceBBIt(BasicBlock::reverse_iterator& It, BasicBlock::reverse_iterator ItEnd, Expr const& E)
{
  while (It != ItEnd && E.contains(&*It)) {
    ++It;
  }
}

bool affineExprCombine(BasicBlock& BB, OptimizationRemarkEmitter& ORE)
{
  auto It = BB.rbegin();
  bool Ret = false;
  while (It != BB.rend()) {
    auto* I = nextValidInstr(It, BB.rend());
    if (I == nullptr) {
      break;
    }

    auto Expr = findLongestExpr(I, It, BB.rend());
    if (Expr.empty()) {
      LLVM_DEBUG(dbgs() << "AEC: expr is empty, discard\n");
      continue;
    }
    advanceBBIt(It, BB.rend(), Expr);
    LLVM_DEBUG(Expr.dump());
    if (Expr.inputs().size() != 1) {
      if (Expr.reduceToOneInput()) {
        LLVM_DEBUG(dbgs() << "AEC: expr has been reduce to one input. New expression:\n");
        LLVM_DEBUG(Expr.dump());
      }
      else {
        LLVM_DEBUG(dbgs() << "AEC: not supporting more than 1 input (got " << Expr.inputs().size() << "), discard\n");
        continue;
      }
    }
    if (!Expr.isProfitable()) {
      LLVM_DEBUG(dbgs() << "AEC: combining expression isn't profitable, discard\n");
      continue;
    }

    const auto MayAffFunc = exprToAffine(Expr);
    if (!MayAffFunc) {
      LLVM_DEBUG(dbgs() << "AEC: unable to evaluate expression to an affine function, discard\n");
      continue;
    }
    bool Mod; 
    if (Expr.isVectorExpr()) {
      Mod = replaceVectorExpr(*MayAffFunc, Expr);
    }
    else {
      Mod = replaceScalarExpr(*MayAffFunc, Expr);
    }
    if (Mod) {
      ORE.emit([&]() {
        auto* Out = Expr.output();
        return OptimizationRemark(DEBUG_TYPE, "InstructionsCombined",
          Out->getDebugLoc(), Out->getParent()) << "affine instruction combined";
      });

      Ret = true;
      localDCE(Expr.instrs());
    }
  }
  return Ret;
}

bool affineExprCombine(Function& F, DominatorTree const& DT, OptimizationRemarkEmitter& ORE)
{
  if (!supported(F)) {
    return false;
  }
  // For now we work on a per BB basis.
  bool Ret = false;
  LLVM_DEBUG(dbgs() << "AEC: Function: " << F.getName() << "\n");
  for (auto& BB: F) {
    // Ignore unreachable and non-profitable basic block
    if (!DT.isReachableFromEntry(&BB) ||
        !canBeProfitable(BB)) {
      continue;
    }

    Ret |= affineExprCombine(BB, ORE);
  }
  LLVM_DEBUG(dbgs() << "AEC: End function " << F.getName() << "\n");
  return Ret;
}

// Pass manager boilerplate below here.

struct AffineExprCombineLegacyPass: public FunctionPass {
  static char ID;
  AffineExprCombineLegacyPass() : FunctionPass(ID) {
    initializeAffineExprCombineLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<OptimizationRemarkEmitterWrapperPass>();
    AU.setPreservesCFG();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    auto& ORE = getAnalysis<OptimizationRemarkEmitterWrapperPass>().getORE();
    return affineExprCombine(F, DT, ORE);
  }
};
} // namespace

char AffineExprCombineLegacyPass::ID = 0;
INITIALIZE_PASS_BEGIN(AffineExprCombineLegacyPass, "aec",
                      "Combine affine expression into hardware-provided instructions", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(OptimizationRemarkEmitterWrapperPass)
INITIALIZE_PASS_END(AffineExprCombineLegacyPass, "aec",
                    "Combine affine expression into hardware-provided instructions", false,
                    false)

FunctionPass *llvm::createAffineExprCombinePass() {
  return new AffineExprCombineLegacyPass{};
}

PreservedAnalyses AffineExprCombinePass::run(Function &F,
                                       FunctionAnalysisManager &FAM) {
  auto& DT = FAM.getResult<DominatorTreeAnalysis>(F);
  auto& ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  if (!affineExprCombine(F, DT, ORE))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<GlobalsAA>();
  return PA;
}
