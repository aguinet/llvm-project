// RUN: %clang_cc1 %s -S -emit-llvm -o - -O1 -triple aarch64-pc-linux | FileCheck %s
// Verify that "when passing parameters to a function, Apple platforms ignore
// empty structures unless those structures have a nontrivial destructor or
// copy constructor." when using darwin_abi

struct Empty {};

__attribute__((darwin_abi)) void foo(int n, Empty E);
// CHECK: @_Z3bari(i32 %[[ARG:[[:alnum:]_]+]])
void bar(int n) {
  // CHECK: call aarch64_darwincc void @_Z3fooi5Empty(i32 %[[ARG]])
  return foo(n, Empty{});
}

// CHECK: declare aarch64_darwincc void @_Z3fooi5Empty(i32)
