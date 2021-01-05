// RUN: %clang_cc1 -fsyntax-only -verify -triple aarch64-unknown-windows-msvc %s

void __attribute__((darwin_abi)) foo(void); // expected-warning{{'darwin_abi' calling convention is not supported for this target}}
