; RUN: opt %s -aec -aec-always-profitable -S | FileCheck %s

; Generated from:
; static uint8_t lfsr(uint8_t v) {
;   const uint8_t bit = v>>7;
;   v <<= 1;
;   v ^= (-bit) & 0xA6;
;   return v;
; }
; 
; void vec(size_t n, uint8_t* out, uint8_t const* in)
; {
;   for (size_t i = 0; i < n; ++i) {
;     out[i] = lfsr(in[i]);
;   }
; }

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nofree norecurse nounwind uwtable
define dso_local void @vec(i64 %0, i8* nocapture %1, i8* nocapture readonly %2) local_unnamed_addr #0 {
  %4 = icmp eq i64 %0, 0
  br i1 %4, label %75, label %5

5:                                                ; preds = %3
  %6 = icmp ult i64 %0, 128
  br i1 %6, label %7, label %25

7:                                                ; preds = %73, %25, %5
  %8 = phi i64 [ 0, %25 ], [ 0, %5 ], [ %32, %73 ]
  %9 = xor i64 %8, -1
  %10 = and i64 %0, 1
  %11 = icmp eq i64 %10, 0
  br i1 %11, label %21, label %12

12:                                               ; preds = %7
  %13 = getelementptr inbounds i8, i8* %2, i64 %8
  %14 = load i8, i8* %13, align 1
  %15 = shl i8 %14, 1
  %16 = ashr i8 %14, 7
  %17 = and i8 %16, -90
  %18 = xor i8 %17, %15
  %19 = getelementptr inbounds i8, i8* %1, i64 %8
  store i8 %18, i8* %19, align 1
  %20 = or i64 %8, 1
  br label %21

21:                                               ; preds = %7, %12
  %22 = phi i64 [ %8, %7 ], [ %20, %12 ]
  %23 = sub i64 0, %0
  %24 = icmp eq i64 %9, %23
  br i1 %24, label %75, label %76

25:                                               ; preds = %5
  %26 = getelementptr i8, i8* %1, i64 %0
  %27 = getelementptr i8, i8* %2, i64 %0
  %28 = icmp ugt i8* %27, %1
  %29 = icmp ugt i8* %26, %2
  %30 = and i1 %28, %29
  br i1 %30, label %7, label %31

31:                                               ; preds = %25
  %32 = and i64 %0, -128
  br label %33

33:                                               ; preds = %33, %31
  %34 = phi i64 [ 0, %31 ], [ %71, %33 ]
  %35 = getelementptr inbounds i8, i8* %2, i64 %34
  %36 = bitcast i8* %35 to <32 x i8>*
  %37 = load <32 x i8>, <32 x i8>* %36, align 1
  %38 = getelementptr inbounds i8, i8* %35, i64 32
  %39 = bitcast i8* %38 to <32 x i8>*
  %40 = load <32 x i8>, <32 x i8>* %39, align 1
  %41 = getelementptr inbounds i8, i8* %35, i64 64
  %42 = bitcast i8* %41 to <32 x i8>*
  %43 = load <32 x i8>, <32 x i8>* %42, align 1
  %44 = getelementptr inbounds i8, i8* %35, i64 96
  %45 = bitcast i8* %44 to <32 x i8>*
  %46 = load <32 x i8>, <32 x i8>* %45, align 1
  ; CHECK: call <32 x i8> @llvm.x86.vgf2p8affineqb.256(<32 x i8> {{%.*}}, <32 x i8> bitcast (<4 x i64> <i64 36453225830817984, i64 36453225830817984, i64 36453225830817984, i64 36453225830817984> to <32 x i8>), i8 0)
  ; CHECK: call <32 x i8> @llvm.x86.vgf2p8affineqb.256(<32 x i8> {{%.*}}, <32 x i8> bitcast (<4 x i64> <i64 36453225830817984, i64 36453225830817984, i64 36453225830817984, i64 36453225830817984> to <32 x i8>), i8 0)
  ; CHECK: call <32 x i8> @llvm.x86.vgf2p8affineqb.256(<32 x i8> {{%.*}}, <32 x i8> bitcast (<4 x i64> <i64 36453225830817984, i64 36453225830817984, i64 36453225830817984, i64 36453225830817984> to <32 x i8>), i8 0)
  ; CHECK: call <32 x i8> @llvm.x86.vgf2p8affineqb.256(<32 x i8> {{%.*}}, <32 x i8> bitcast (<4 x i64> <i64 36453225830817984, i64 36453225830817984, i64 36453225830817984, i64 36453225830817984> to <32 x i8>), i8 0)

  %47 = shl <32 x i8> %37, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  %48 = shl <32 x i8> %40, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  %49 = shl <32 x i8> %43, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  %50 = shl <32 x i8> %46, <i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1, i8 1>
  %51 = ashr <32 x i8> %37, <i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7>
  %52 = ashr <32 x i8> %40, <i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7>
  %53 = ashr <32 x i8> %43, <i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7>
  %54 = ashr <32 x i8> %46, <i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7, i8 7>
  %55 = and <32 x i8> %51, <i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90>
  %56 = and <32 x i8> %52, <i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90>
  %57 = and <32 x i8> %53, <i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90>
  %58 = and <32 x i8> %54, <i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90, i8 -90>
  %59 = xor <32 x i8> %55, %47
  %60 = xor <32 x i8> %56, %48
  %61 = xor <32 x i8> %57, %49
  %62 = xor <32 x i8> %58, %50
  %63 = getelementptr inbounds i8, i8* %1, i64 %34
  %64 = bitcast i8* %63 to <32 x i8>*
  store <32 x i8> %59, <32 x i8>* %64, align 1
  %65 = getelementptr inbounds i8, i8* %63, i64 32
  %66 = bitcast i8* %65 to <32 x i8>*
  store <32 x i8> %60, <32 x i8>* %66, align 1
  %67 = getelementptr inbounds i8, i8* %63, i64 64
  %68 = bitcast i8* %67 to <32 x i8>*
  store <32 x i8> %61, <32 x i8>* %68, align 1
  %69 = getelementptr inbounds i8, i8* %63, i64 96
  %70 = bitcast i8* %69 to <32 x i8>*
  store <32 x i8> %62, <32 x i8>* %70, align 1
  %71 = add i64 %34, 128
  %72 = icmp eq i64 %71, %32
  br i1 %72, label %73, label %33

73:                                               ; preds = %33
  %74 = icmp eq i64 %32, %0
  br i1 %74, label %75, label %7

75:                                               ; preds = %21, %76, %73, %3
  ret void

76:                                               ; preds = %21, %76
  %77 = phi i64 [ %93, %76 ], [ %22, %21 ]
  %78 = getelementptr inbounds i8, i8* %2, i64 %77
  %79 = load i8, i8* %78, align 1
  %80 = shl i8 %79, 1
  %81 = ashr i8 %79, 7
  %82 = and i8 %81, -90
  %83 = xor i8 %82, %80
  %84 = getelementptr inbounds i8, i8* %1, i64 %77
  store i8 %83, i8* %84, align 1
  %85 = add nuw i64 %77, 1
  %86 = getelementptr inbounds i8, i8* %2, i64 %85
  %87 = load i8, i8* %86, align 1
  %88 = shl i8 %87, 1
  %89 = ashr i8 %87, 7
  %90 = and i8 %89, -90
  %91 = xor i8 %90, %88
  %92 = getelementptr inbounds i8, i8* %1, i64 %85
  store i8 %91, i8* %92, align 1
  %93 = add nuw i64 %77, 2
  %94 = icmp eq i64 %93, %0
  br i1 %94, label %75, label %76
}

attributes #0 = { nofree norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+avx,+avx2,+cx8,+fxsr,+gfni,+mmx,+popcnt,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "unsafe-fp-math"="false" "use-soft-float"="false" }
