// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -o - -emit-llvm -debug-info-kind=limited %s | FileCheck %s --check-prefix=LINUX
// RUN: %clang_cc1 -triple x86_64-unknown-windows-msvc -o - -emit-llvm -debug-info-kind=limited %s | FileCheck %s --check-prefix=WINDOWS
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -o - -emit-llvm -debug-info-kind=limited %s | FileCheck %s --check-prefix=LINUX32
// RUN: %clang_cc1 -triple armv7--linux-gnueabihf -o - -emit-llvm -debug-info-kind=limited %s | FileCheck %s --check-prefix=ARM
// RUN: %clang_cc1 -triple aarch64-pc-linux -o - -emit-llvm -debug-info-kind=limited %s | FileCheck %s --check-prefix=AARCH64

#ifdef __x86_64__

#ifdef __linux__
// LINUX: !DISubprogram({{.*}}"add_msabi", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_Win64,
__attribute__((ms_abi)) int add_msabi(int a, int b) {
  return a+b;
}

// LINUX: !DISubprogram({{.*}}"add_regcall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_X86RegCall,
__attribute__((regcall)) int add_regcall(int a, int b) {
  return a+b;
}

// LINUX: !DISubprogram({{.*}}"add_preserve_most", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_PreserveMost,
__attribute__((preserve_most)) int add_preserve_most(int a, int b) {
  return a+b;
}

// LINUX: !DISubprogram({{.*}}"add_preserve_all", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_PreserveAll,
__attribute__((preserve_all)) int add_preserve_all(int a, int b) {
  return a+b;
}

// LINUX: !DISubprogram({{.*}}"add_swiftcall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_Swift,
__attribute__((swiftcall)) int add_swiftcall(int a, int b) {
  return a+b;
}

// LINUX: !DISubprogram({{.*}}"add_inteloclbicc", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_IntelOclBicc,
__attribute__((intel_ocl_bicc)) int add_inteloclbicc(int a, int b) {
  return a+b;
}
#endif

#ifdef _WIN64
// WINDOWS: !DISubprogram({{.*}}"add_sysvabi", {{.*}}type: ![[FTY:[0-9]+]]
// WINDOWS: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_X86_64SysV,
__attribute__((sysv_abi)) int add_sysvabi(int a, int b) {
  return a+b;
}
#endif

#endif

#ifdef __i386__
// LINUX32: !DISubprogram({{.*}}"add_stdcall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX32: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_BORLAND_stdcall,
__attribute__((stdcall)) int add_stdcall(int a, int b) {
  return a+b;
}

// LINUX32: !DISubprogram({{.*}}"add_fastcall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX32: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_BORLAND_msfastcall,
__attribute__((fastcall)) int add_fastcall(int a, int b) {
  return a+b;
}

// LINUX32: !DISubprogram({{.*}}"add_thiscall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX32: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_BORLAND_thiscall,
__attribute__((thiscall)) int add_thiscall(int a, int b) {
  return a+b;
}

// LINUX32: !DISubprogram({{.*}}"add_vectorcall", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX32: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_vectorcall,
__attribute__((vectorcall)) int add_vectorcall(int a, int b) {
  return a+b;
}

// LINUX32: !DISubprogram({{.*}}"add_pascal", {{.*}}type: ![[FTY:[0-9]+]]
// LINUX32: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_BORLAND_pascal,
__attribute__((pascal)) int add_pascal(int a, int b) {
  return a+b;
}
#endif

#ifdef __arm__
// ARM: !DISubprogram({{.*}}"add_aapcs", {{.*}}type: ![[FTY:[0-9]+]]
// ARM: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_AAPCS,
__attribute__((pcs("aapcs"))) int add_aapcs(int a, int b) {
  return a+b;
}

// ARM: !DISubprogram({{.*}}"add_aapcs_vfp", {{.*}}type: ![[FTY:[0-9]+]]
// ARM: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_AAPCS_VFP,
__attribute__((pcs("aapcs-vfp"))) int add_aapcs_vfp(int a, int b) {
  return a+b;
}
#endif

#if defined(__aarch64__) && defined(__linux__)
// AARCH64: !DISubprogram({{.*}}"add_darwinabi", {{.*}}type: ![[FTY:[0-9]+]]
// AARCH64: ![[FTY]] = !DISubroutineType({{.*}}cc: DW_CC_LLVM_AArch64Darwin,
__attribute__((darwin_abi)) int add_darwinabi(int a, int b) {
  return a + b;
}
#endif
