; RUN: opt %s -aec -aec-always-profitable -S | FileCheck %s

; Generated from:
; static uint8_t rol2(uint8_t v) {
;   return (v<<2)^(v>>6);
; }
; 
; uint8_t func(uint8_t v) {
;   v = rol2(v);
;   v ^= 0xAA;
;   return v;
; }

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local zeroext i8 @func(i8 zeroext %0) local_unnamed_addr #0 {
  ; CHECK: [[V0:%.*]] = insertelement <16 x i8> undef, i8 %0, i64 0
  ; CHECK: [[V1:%.*]] = call <16 x i8> @llvm.x86.vgf2p8affineqb.128(<16 x i8> [[V0]], <16 x i8> bitcast (<2 x i64> <i64 4647715923615551520, i64 4647715923615551520> to <16 x i8>), i8 -86)
  ; CHECK: extractelement <16 x i8> [[V1]], i64 0

  %2 = shl i8 %0, 2
  %3 = lshr i8 %0, 6
  %4 = or i8 %2, %3
  %5 = xor i8 %4, -86
  ret i8 %5
}

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+avx,+avx2,+avx512f,+cx8,+f16c,+fma,+fxsr,+gfni,+mmx,+popcnt,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "unsafe-fp-math"="false" "use-soft-float"="false" }

