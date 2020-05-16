#include <cassert>

#ifndef NDEBUG
#include <llvm/Support/raw_ostream.h>
#endif

#include "BitMatrix.h"

namespace llvm {

BitMatrix::BitMatrix(unsigned NCols, unsigned NRows):
  NCols_(NCols),
  NRows_(NRows)
{
  // Keep, as much as possible, a compact representation of the matrix.
  M_.resize(matrixBytes());
}

void BitMatrix::setZero() {
  std::fill(M_.begin(), M_.end(), 0);
}

uint64_t BitMatrix::toIntel8x8Repr() const {
  assert(cols() == 8 && rows() == 8);
  // What we need to do is reverseRows.
  // We do that by copying our dense 8x8 matrix into a 64-bit integer and
  // byte-swapping it to big endian. This gives what Intel expects for GF2*
  // instructions.
  uint64_t Ret = 0;
  memcpy(&Ret, &M_[0], sizeof(Ret));
  return support::endian::byte_swap<uint64_t, support::big>(Ret);
}

BitMatrix BitMatrix::transpose() const {
  BitMatrix Ret(NRows_, NCols_);

  // Naïve transposition algorithm
  for (size_t r = 0; r < Ret.rows(); ++r) {
    uint64_t RV = 0;
    for (size_t c = 0; c < Ret.cols(); ++c) {
      RV |= (uint64_t)(getBit(r,c)) << c;
    }
    Ret.setRow(r, RV);
  }
  return Ret;
}

void BitMatrix::reverseRows() {
  size_t R = 0;
  size_t Last = NRows_;
  while ((R != Last) && (R != (--Last))) {
    const auto V = row(R);
    setRow(R, row(Last));
    setRow(Last, V);
    ++R;
  }
}

BitMatrix BitMatrix::sub8x8Matrix(unsigned Col, unsigned Row) const
{
  assert((Col+8) <= cols());
  assert((Row+8) <= rows());
  BitMatrix Ret(8,8);

  for (size_t J = 0; J < 8; ++J) {
    const uint64_t V = (row(J+Row) >> Col) & 0xFF;
    Ret.setRow(J, V);
  }
  return Ret;
}

#ifndef NDEBUG
void BitMatrix::dump(raw_ostream& OS) const {
  for (size_t R = 0; R < rows(); ++R) {
    OS << "( ";
    for (size_t C = 0; C < cols(); ++C) {
      OS << getBit(C, R) << " ";
    }
    OS << ") " << ((R == rows()/2) ? "*":" ") <<  " (x" << R << ")\n";
  }
}
#endif

} // llvm
