// RUN: %clang_cc1 -triple aarch64-pc-linux -emit-llvm %s -o - |FileCheck %s
// Check that va_arg used inside a function with the darwin_abi attribute still
// uses the Linux ABI lowering.

// CHECK: define internal aarch64_darwincc i32 @vfoo(i32 %n, %struct.__va_list* %[[VA:[[:alnum:]_]+]])
// CHECK: getelementptr inbounds %struct.__va_list, %struct.__va_list* %[[VA]], i32 0, i32 3
__attribute__((darwin_abi, noinline)) static int vfoo(int n, __builtin_va_list va) {
  int res = 0;
  for (int i = 0; i < n; ++i) {
    res += __builtin_va_arg(va, int);
  }
  return res;
}

int foo(int n, ...) {
  __builtin_va_list va;
  __builtin_va_start(va, n);
  const int res = vfoo(n, va);
  __builtin_va_end(va);
  return res;
}
