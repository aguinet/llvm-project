// RUN: %clang_cc1 -triple aarch64-pc-linux %s -emit-llvm -o - |FileCheck %s
// Checks that correct LLVM IR is generated for functions that target
// the Darwin AArch64 ABI under Linux/AArch64. What we check:
// * sext/zext on return values and arguments
// * va_list forwarding from Darwin to Linux support

// CHECK: define aarch64_darwincc signext i16 @f1(i16 signext
__attribute__((darwin_abi)) short f1(short a) {
  return a + 1;
}

// CHECK: define aarch64_darwincc zeroext i16 @f2(i16 zeroext
__attribute__((darwin_abi)) unsigned short f2(unsigned short a) {
  return a + 1;
}

// CHECK: define aarch64_darwincc void @foo(i32 %n, ...)
// CHECK: call void @llvm.va_start
void vfoo(int n, __builtin_va_list *va);
__attribute((darwin_abi)) void foo(int n, ...) {
  __builtin_va_list va;
  __builtin_va_start(va, n);
  vfoo(n, &va);
  __builtin_va_end(va);
}
