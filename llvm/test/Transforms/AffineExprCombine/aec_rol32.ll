; RUN: opt %s -aec -aec-always-profitable -S | FileCheck %s

; Generated from:
; static uint32_t rol2(uint32_t v) {
;   return (v<<2)^(v>>30);
; }
; 
; uint32_t func(uint32_t v) {
;   v = rol2(v);
;   return v ^ 0xAABBCCDD;
; }

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local i32 @func(i32 %0) local_unnamed_addr #0 {
  ; CHECK: shufflevector <32 x i8> %{{.*}}, <32 x i8> undef, <64 x i32> <i32 0, i32 4, i32 8, i32 12, i32 16, i32 20, i32 24, i32 28, i32 0, i32 4, i32 8, i32 12, i32 16, i32 20, i32 24, i32 28, i32 0, i32 4, i32 8, i32 12, i32 16, i32 20, i32 24, i32 28, i32 0, i32 4, i32 8, i32 12, i32 16, i32 20, i32 24, i32 28, i32 1, i32 5, i32 9, i32 13, i32 17, i32 21, i32 25, i32 29, i32 1, i32 5, i32 9, i32 13, i32 17, i32 21, i32 25, i32 29, i32 1, i32 5, i32 9, i32 13, i32 17, i32 21, i32 25, i32 29, i32 1, i32 5, i32 9, i32 13, i32 17, i32 21, i32 25, i32 29>
  ; CHECK: shufflevector <32 x i8> %{{.*}}, <32 x i8> undef, <64 x i32> <i32 2, i32 6, i32 10, i32 14, i32 18, i32 22, i32 26, i32 30, i32 2, i32 6, i32 10, i32 14, i32 18, i32 22, i32 26, i32 30, i32 2, i32 6, i32 10, i32 14, i32 18, i32 22, i32 26, i32 30, i32 2, i32 6, i32 10, i32 14, i32 18, i32 22, i32 26, i32 30, i32 3, i32 7, i32 11, i32 15, i32 19, i32 23, i32 27, i32 31, i32 3, i32 7, i32 11, i32 15, i32 19, i32 23, i32 27, i32 31, i32 3, i32 7, i32 11, i32 15, i32 19, i32 23, i32 27, i32 31, i32 3, i32 7, i32 11, i32 15, i32 19, i32 23, i32 27, i32 31>
  ; CHECK: call <64 x i8> @llvm.x86.vgf2p8affineqb.512(<64 x i8> %{{.*}}, <64 x i8> bitcast (<8 x i64> <i64 1108169199648, i64 4647714815446351872, i64 0, i64 0, i64 0, i64 1108169199648, i64 4647714815446351872, i64 0> to <64 x i8>), i8 0)
  ; CHECK: call <64 x i8> @llvm.x86.vgf2p8affineqb.512(<64 x i8> %{{.*}}, <64 x i8> bitcast (<8 x i64> <i64 0, i64 0, i64 1108169199648, i64 4647714815446351872, i64 4647714815446351872, i64 0, i64 0, i64 1108169199648> to <64 x i8>), i8 0)
  ; CHECK: shufflevector <8 x i64> %{{.*}}, <8 x i64> undef, <4 x i32> <i32 4, i32 5, i32 6, i32 7>
  ; CHECK: shufflevector <8 x i64> %{{.*}}, <8 x i64> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ; CHECK: shufflevector <32 x i8> %{{.*}}, <32 x i8> undef, <32 x i32> <i32 0, i32 8, i32 16, i32 24, i32 1, i32 9, i32 17, i32 25, i32 2, i32 10, i32 18, i32 26, i32 3, i32 11, i32 19, i32 27, i32 4, i32 12, i32 20, i32 28, i32 5, i32 13, i32 21, i32 29, i32 6, i32 14, i32 22, i32 30, i32 7, i32 15, i32 23, i32 31>
  ; CHECK: xor i32 %{{.*}}, -1430532899

  %2 = shl i32 %0, 2
  %3 = lshr i32 %0, 30
  %4 = or i32 %2, %3
  %5 = xor i32 %4, -1430532899
  ret i32 %5
}

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+avx,+avx2,+avx512bw,+avx512f,+cx8,+f16c,+fma,+fxsr,+gfni,+mmx,+popcnt,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "unsafe-fp-math"="false" "use-soft-float"="false" }
