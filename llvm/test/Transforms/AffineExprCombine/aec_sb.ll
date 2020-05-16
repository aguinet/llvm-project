; RUN: opt %s -aec -aec-always-profitable -S | FileCheck %s

; Generated from:
; static uint8_t SBLin[256] = { ... };
; static uint8_t rol2(uint8_t v) {
;  return (v<<2)^(v>>6);
; }
; uint8_t func2(uint8_t v) {
;   v = rol2(v);
;   v = SBLin[v];
;   v ^= 0xAA;
;   return v;
; }

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@SBLin = internal unnamed_addr constant [256 x i8] c"\00H\01I>v?wz2{3D\0CE\0D$l%m\1AR\1BS^\16_\17`(a)W\1FV\1Ei!h -e,d\13[\12Zs;r:M\05L\04\09A\08@7\7F6~b*c+\\\14]\15\18P\19Q&n'oF\0EG\0Fx0y1<t=u\02J\03K5}4|\0BC\0ABO\07N\06q9p8\11Y\10X/g.fk#j\22U\1DT\1C\A8\E0\A9\E1\96\DE\97\DF\D2\9A\D3\9B\EC\A4\ED\A5\8C\C4\8D\C5\B2\FA\B3\FB\F6\BE\F7\BF\C8\80\C9\81\FF\B7\FE\B6\C1\89\C0\88\85\CD\84\CC\BB\F3\BA\F2\DB\93\DA\92\E5\AD\E4\AC\A1\E9\A0\E8\9F\D7\9E\D6\CA\82\CB\83\F4\BC\F5\BD\B0\F8\B1\F9\8E\C6\8F\C7\EE\A6\EF\A7\D0\98\D1\99\94\DC\95\DD\AA\E2\AB\E3\9D\D5\9C\D4\A3\EB\A2\EA\E7\AF\E6\AE\D9\91\D8\90\B9\F1\B8\F0\87\CF\86\CE\C3\8B\C2\8A\FD\B5\FC\B4", align 16

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local zeroext i8 @func2(i8 zeroext %0) local_unnamed_addr #0 {
  ; CHECK: [[V0:%.*]] = insertelement <16 x i8> undef, i8 %0, i64 0
  ; CHECK: [[V1:%.*]] = call <16 x i8> @llvm.x86.vgf2p8affineqb.128(<16 x i8> [[V0]], <16 x i8> bitcast (<2 x i64> <i64 -8639296741139064288, i64 -8639296741139064288> to <16 x i8>), i8 -86)
  ; CHECK: extractelement <16 x i8> [[V1]], i64 0

  %2 = shl i8 %0, 2
  %3 = lshr i8 %0, 6
  %4 = or i8 %2, %3
  %5 = zext i8 %4 to i64
  %6 = getelementptr inbounds [256 x i8], [256 x i8]* @SBLin, i64 0, i64 %5
  %7 = load i8, i8* %6, align 1
  %8 = xor i8 %7, -86
  ret i8 %8
}

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+avx,+avx2,+avx512f,+cx8,+f16c,+fma,+fxsr,+gfni,+mmx,+popcnt,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "unsafe-fp-math"="false" "use-soft-float"="false" }
