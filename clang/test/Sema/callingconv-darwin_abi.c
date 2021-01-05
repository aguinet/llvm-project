// RUN: %clang_cc1 -fsyntax-only -verify -triple aarch64-pc-linux-gnu %s

void __attribute__((sysv_abi)) foo(void);
void (*pfoo)(void) = foo; // valid declaration

void __attribute__((darwin_abi)) bar(void);
void (*pbar)(void) = bar;                                   // expected-warning{{incompatible function pointer types}}
void(__attribute__((darwin_abi)) * pbar_valid)(void) = bar; // valid declaration

void(__attribute__((darwin_abi)) * pfoo2)(void) = foo; // expected-warning{{incompatible function pointer types}}
