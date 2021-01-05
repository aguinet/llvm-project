// RUN: %clang_cc1 -fsyntax-only -verify -triple aarch64-pc-linux-gnu %s
// Based on ms_abi-sysv_abi.c

// CC qualifier can be applied only to functions
int __attribute__((darwin_abi)) var1;   // expected-warning{{'darwin_abi' only applies to function types; type here is 'int'}}
void __attribute__((darwin_abi)) foo(); // valid declaration

// CC qualifier attribute does not take any argument
void __attribute__((darwin_abi("arg"))) foo0(void); // expected-error{{'darwin_abi' attribute takes no arguments}}

// Different CC qualifiers are not compatible
void __attribute__((darwin_abi, sysv_abi)) foo1(void); // expected-error{{cdecl and darwin_abi attributes are not compatible}}
void __attribute__((darwin_abi)) foo2();               // expected-note{{previous declaration is here}}
void __attribute__((sysv_abi)) foo2(void);             // expected-error{{function declared 'cdecl' here was previously declared 'darwin_abi'}}

void bar(int i, int j) __attribute__((darwin_abi, cdecl)); // expected-error{{cdecl and darwin_abi attributes are not compatible}}
