; RUN: opt %s -aec -aec-always-profitable -S | FileCheck %s

; Generated from:
; static uint8_t lfsr(uint8_t v) {
;   const uint8_t bit = v&1;
;   v >>= 1;
;   v ^= (-bit) & 0xA6;
;   return v;
; }
; 
; uint8_t rnd(uint8_t v) {
;   for (size_t i = 0; i < 8; ++i) {
;     v = lfsr(v);
;   }
;   return v^0xBB;
; }

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local zeroext i8 @rnd(i8 zeroext %0) local_unnamed_addr #0 {
  ; CHECK: [[V0:%.*]] = insertelement <16 x i8> undef, i8 %0, i64 0
  ; CHECK: [[V1:%.*]] = call <16 x i8> @llvm.x86.vgf2p8affineqb.128(<16 x i8> [[V0]], <16 x i8> bitcast (<2 x i64> <i64 8642312187577458107, i64 8642312187577458107> to <16 x i8>), i8 -69)
  ; CHECK: extractelement <16 x i8> [[V1]], i64 0

  %2 = and i8 %0, 1
  %3 = lshr i8 %0, 1
  %4 = sub nsw i8 0, %2
  %5 = and i8 %4, -90
  %6 = xor i8 %5, %3
  %7 = and i8 %3, 1
  %8 = lshr i8 %6, 1
  %9 = sub nsw i8 0, %7
  %10 = and i8 %9, -90
  %11 = xor i8 %10, %8
  %12 = and i8 %8, 1
  %13 = lshr i8 %11, 1
  %14 = sub nsw i8 0, %12
  %15 = and i8 %14, -90
  %16 = xor i8 %15, %13
  %17 = and i8 %13, 1
  %18 = lshr i8 %16, 1
  %19 = sub nsw i8 0, %17
  %20 = and i8 %19, -90
  %21 = xor i8 %20, %18
  %22 = and i8 %18, 1
  %23 = lshr i8 %21, 1
  %24 = sub nsw i8 0, %22
  %25 = and i8 %24, -90
  %26 = xor i8 %25, %23
  %27 = and i8 %23, 1
  %28 = lshr i8 %26, 1
  %29 = sub nsw i8 0, %27
  %30 = and i8 %29, -90
  %31 = xor i8 %30, %28
  %32 = and i8 %28, 1
  %33 = lshr i8 %31, 1
  %34 = sub nsw i8 0, %32
  %35 = and i8 %34, -90
  %36 = xor i8 %35, %33
  %37 = and i8 %33, 1
  %38 = lshr i8 %36, 1
  %39 = sub nsw i8 0, %37
  %40 = and i8 %39, -90
  %41 = xor i8 %40, %38
  %42 = xor i8 %41, -69
  ret i8 %42
}

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+avx,+avx2,+avx512f,+cx8,+f16c,+fma,+fxsr,+gfni,+mmx,+popcnt,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "unsafe-fp-math"="false" "use-soft-float"="false" }
