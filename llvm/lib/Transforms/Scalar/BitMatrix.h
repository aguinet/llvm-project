#ifndef LLVM_BIT_MATRIX_H
#define LLVM_BIT_MATRIX_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <llvm/Support/Endian.h>
#include <llvm/ADT/SmallVector.h>

namespace llvm {

#ifndef NDEBUG
class raw_ostream;
#endif

struct BitMatrix
{
  BitMatrix(unsigned NCols, unsigned NRows);
  BitMatrix(BitMatrix const& BM) = default;
  BitMatrix(BitMatrix&&) = default;
  BitMatrix& operator=(BitMatrix const&) = default;
  BitMatrix& operator=(BitMatrix&&) = default;

  uint8_t* matrix() { return &M_[0]; }
  uint8_t const* matrix() const { return &M_[0]; }

  unsigned cols() const { 
    return NCols_;
  }
  unsigned rows() const {
    return NRows_;
  }

  void setZero();

  void setRow(size_t R, uint64_t V) {
    V &= inMask();
    V = support::endian::byte_swap<uint64_t, support::little>(V);
    memcpy(rowData(R), &V, inBytes());
  }

  uint64_t row(size_t R) const {
    uint64_t V = 0;
    memcpy(&V, rowData(R), inBytes());
    return support::endian::byte_swap<uint64_t, support::little>(V);
  }

  bool isSquare() const {
    return NCols_ == NRows_;
  }

  bool isSquare(unsigned N) const {
    return isSquare() && NCols_ == N;
  }

  bool getBit(unsigned Col, unsigned Row) const {
    return (row(Row) >> Col) & 1;
  }

  uint64_t toIntel8x8Repr() const;

  BitMatrix transpose() const;
  
  void reverseRows();

  BitMatrix sub8x8Matrix(unsigned Col, unsigned Row) const;

#ifndef NDEBUG
  void dump(raw_ostream& OS) const;
#endif

  uint64_t inMask() const {
    return mask(NCols_);
  }
  uint64_t outMask() const {
    return mask(NRows_);
  }

private:
  unsigned inBytes() const {
    return (NCols_+7)/8;
  }
  unsigned outBytes() const {
    return (NRows_+7)/8;
  }
  unsigned matrixBytes() const {
    return inBytes()*NRows_;
  }

  uint8_t* rowData(size_t R) {
    return &M_[R*inBytes()];
  }
  uint8_t const* rowData(size_t R) const {
    return &M_[R*inBytes()];
  }

  static uint64_t mask(unsigned Bits) {
    return (1ULL<<Bits)-1;
  }

  unsigned NCols_;
  unsigned NRows_;
  // No heap allocation for 8x8 matrices
  SmallVector<uint8_t, 8> M_;
};

} // llvm

#endif
