// RUN: %clang_cc1 -triple aarch64-pc-linux-gnu -mconstructor-aliases %s -emit-llvm -O1 -o - | FileCheck %s

struct A {
  __attribute__((darwin_abi)) A();
  __attribute__((darwin_abi)) ~A();
};

// CHECK: define aarch64_darwincc %struct.A* @_ZN1AC2Ev(%struct.A* readnone returned %[[THIS:.*]])
A::A() {
  // CHECK: ret %struct.A* %[[THIS]]
}

// CHECK: define aarch64_darwincc %struct.A* @_ZN1AD2Ev(%struct.A* readnone returned %[[THIS:.*]])
A::~A() {
  // CHECK: ret %struct.A* %[[THIS]]
}
