/*
 *  Copyright 2022 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/compare_row.h"
#include "libyuv/row.h"
#include "libyuv/video_common.h"

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

#if !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void CopyRow_LASX(const uint8_t* src, uint8_t* dst, int count) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [dst] "+r"(dst), [cnt] "+r"(count)
      :
      : "memory", "$xr0");
}

void SetRow_LASX(uint8_t* dst, uint8_t v8, int count) {
  asm volatile(
      "xvreplgr2vr.b $xr0, %[val]\n\t"
      "1:\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst), [cnt] "+r"(count)
      : [val] "r"(v8)
      : "memory", "$xr0");
}

void ARGBSetRow_LASX(uint8_t* dst_argb, uint32_t v32, int width) {
  asm volatile(
      "xvreplgr2vr.w $xr0, %[val]\n\t"
      "1:\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(v32)
      : "memory", "$xr0");
}

void MirrorRow_LASX(const uint8_t* src_y, uint8_t* dst_y, int width) {
  static const uint8_t kRev[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                   7,  6,  5,  4,  3,  2,  1, 0};
  src_y += width - 32;
  asm volatile(
      "vld $vr1, %[rev], 0\n\t"
      "xvpermi.q $xr1, $xr1, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 16\n\t"
      "vld $vr31, %[src], 0\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_y), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [rev] "r"(kRev)
      : "memory", "$xr0", "$xr1", "$xr31");
}

void MirrorUVRow_LASX(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  static const uint8_t kRevUV[16] = {14, 15, 12, 13, 10, 11, 8, 9,
                                     6,  7,  4,  5,  2,  3,  0, 1};
  src_uv += width * 2 - 32;
  asm volatile(
      "vld $vr1, %[rev], 0\n\t"
      "xvpermi.q $xr1, $xr1, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 16\n\t"
      "vld $vr31, %[src], 0\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uv), [dst] "+r"(dst_uv), [cnt] "+r"(width)
      : [rev] "r"(kRevUV)
      : "memory", "$xr0", "$xr1", "$xr31");
}

void ARGBMirrorRow_LASX(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  src_argb += width * 4 - 32;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 16\n\t"
      "vld $vr31, %[src], 0\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvshuf4i.w $xr0, $xr0, 0x1b\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr31");
}

void MirrorSplitUVRow_LASX(const uint8_t* src_uv,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width) {
  static const uint8_t kRevUV[16] = {14, 15, 12, 13, 10, 11, 8, 9,
                                     6,  7,  4,  5,  2,  3,  0, 1};
  src_uv += width * 2 - 64;
  asm volatile(
      "vld $vr4, %[rev], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 48\n\t"
      "vld $vr31, %[src], 16\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 32\n\t"
      "vld $vr31, %[src], 0\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr4\n\t"
      "xvshuf.b $xr1, $xr1, $xr1, $xr4\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvst $xr2, %[du], 0\n\t"
      "xvst $xr3, %[dv], 0\n\t"
      "addi.d %[src], %[src], -64\n\t"
      "addi.d %[du], %[du], 32\n\t"
      "addi.d %[dv], %[dv], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [src] "+r"(src_uv), [du] "+r"(dst_u), [dv] "+r"(dst_v), [cnt] "+r"(width)
      : [rev] "r"(kRevUV)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void SplitUVRow_LASX(const uint8_t* src_uv,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvst $xr2, %[du], 0\n\t"
      "xvst $xr3, %[dv], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[du], %[du], 32\n\t"
      "addi.d %[dv], %[dv], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [src] "+r"(src_uv), [du] "+r"(dst_u), [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr31");
}

void MergeUVRow_LASX(const uint8_t* src_u,
                     const uint8_t* src_v,
                     uint8_t* dst_uv,
                     int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[su], 0\n\t"
      "xvld $xr1, %[sv], 0\n\t"
      "xvilvl.b $xr2, $xr1, $xr0\n\t"
      "xvilvh.b $xr3, $xr1, $xr0\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr3, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[su], %[su], 32\n\t"
      "addi.d %[sv], %[sv], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [su] "+r"(src_u), [sv] "+r"(src_v), [dst] "+r"(dst_uv), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr31");
}

void ARGBExtractAlphaRow_LASX(const uint8_t* src_argb,
                              uint8_t* dst_a,
                              int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickod.b $xr0, $xr1, $xr0\n\t"
      "xvpickod.b $xr0, $xr0, $xr0\n\t"
      "xvpermi.d $xr31, $xr0, 0xd8\n\t"
      "vst $vr31, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_a), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr31");
}

void I422ToYUY2Row_LASX(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_yuy2,
                        int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[sy], 0\n\t"
      "vld $vr1, %[su], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "vld $vr2, %[sv], 0\n\t"
      "xvpermi.d $xr2, $xr2, 0xd8\n\t"
      "xvilvl.b $xr3, $xr2, $xr1\n\t"
      "xvilvl.b $xr4, $xr3, $xr0\n\t"
      "xvilvh.b $xr5, $xr3, $xr0\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr4, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr5, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 32\n\t"
      "addi.d %[su], %[su], 16\n\t"
      "addi.d %[sv], %[sv], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_yuy2), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr31");
}

void I422ToUYVYRow_LASX(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_uyvy,
                        int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[sy], 0\n\t"
      "vld $vr1, %[su], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "vld $vr2, %[sv], 0\n\t"
      "xvpermi.d $xr2, $xr2, 0xd8\n\t"
      "xvilvl.b $xr3, $xr2, $xr1\n\t"
      "xvilvl.b $xr4, $xr0, $xr3\n\t"
      "xvilvh.b $xr5, $xr0, $xr3\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr4, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr5, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 32\n\t"
      "addi.d %[su], %[su], 16\n\t"
      "addi.d %[sv], %[sv], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_uyvy), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr31");
}

void YUY2ToYRow_LASX(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr31");
}

void UYVYToYRow_LASX(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickod.b $xr2, $xr1, $xr0\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr31");
}

void YUY2ToUV422Row_LASX(const uint8_t* src_yuy2,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickod.b $xr2, $xr1, $xr0\n\t"
      "xvpickev.b $xr3, $xr2, $xr2\n\t"
      "xvpickod.b $xr4, $xr2, $xr2\n\t"
      "xvpermi.d $xr31, $xr3, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "xvpermi.d $xr31, $xr4, 0xd8\n\t"
      "vst $vr31, %[dv], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void UYVYToUV422Row_LASX(const uint8_t* src_uyvy,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickev.b $xr3, $xr2, $xr2\n\t"
      "xvpickod.b $xr4, $xr2, $xr2\n\t"
      "xvpermi.d $xr31, $xr3, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "xvpermi.d $xr31, $xr4, 0xd8\n\t"
      "vst $vr31, %[dv], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void YUY2ToUVRow_LASX(const uint8_t* src_yuy2,
                      int src_stride_yuy2,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  const uint8_t* next_yuy2 = src_yuy2 + src_stride_yuy2;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr31, %[s0], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[s0], 16\n\t"
      "vld $vr31, %[s0], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "vld $vr2, %[s1], 0\n\t"
      "vld $vr31, %[s1], 32\n\t"
      "xvpermi.q $xr2, $xr31, 0x02\n\t"
      "vld $vr3, %[s1], 16\n\t"
      "vld $vr31, %[s1], 48\n\t"
      "xvpermi.q $xr3, $xr31, 0x02\n\t"
      "xvpickod.b $xr0, $xr1, $xr0\n\t"
      "xvpickod.b $xr2, $xr3, $xr2\n\t"
      "xvavgr.bu $xr0, $xr0, $xr2\n\t"
      "xvpickev.b $xr1, $xr0, $xr0\n\t"
      "xvpickod.b $xr2, $xr0, $xr0\n\t"
      "xvpermi.d $xr31, $xr1, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "xvpermi.d $xr31, $xr2, 0xd8\n\t"
      "vst $vr31, %[dv], 0\n\t"
      "addi.d %[s0], %[s0], 64\n\t"
      "addi.d %[s1], %[s1], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_yuy2), [s1] "+r"(next_yuy2), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr31");
}

void UYVYToUVRow_LASX(const uint8_t* src_uyvy,
                      int src_stride_uyvy,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  const uint8_t* next_uyvy = src_uyvy + src_stride_uyvy;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr31, %[s0], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[s0], 16\n\t"
      "vld $vr31, %[s0], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "vld $vr2, %[s1], 0\n\t"
      "vld $vr31, %[s1], 32\n\t"
      "xvpermi.q $xr2, $xr31, 0x02\n\t"
      "vld $vr3, %[s1], 16\n\t"
      "vld $vr31, %[s1], 48\n\t"
      "xvpermi.q $xr3, $xr31, 0x02\n\t"
      "xvpickev.b $xr0, $xr1, $xr0\n\t"
      "xvpickev.b $xr2, $xr3, $xr2\n\t"
      "xvavgr.bu $xr0, $xr0, $xr2\n\t"
      "xvpickev.b $xr1, $xr0, $xr0\n\t"
      "xvpickod.b $xr2, $xr0, $xr0\n\t"
      "xvpermi.d $xr31, $xr1, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "xvpermi.d $xr31, $xr2, 0xd8\n\t"
      "vst $vr31, %[dv], 0\n\t"
      "addi.d %[s0], %[s0], 64\n\t"
      "addi.d %[s1], %[s1], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_uyvy), [s1] "+r"(next_uyvy), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr31");
}

void I422ToARGBRow_LASX(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr20", "$xr21", "$xr22",
        "$xr23", "$xr24", "$xr31");
}

void I422ToRGBARow_LASX(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr2, $xr5\n\t"
      "xvilvl.b $xr7, $xr4, $xr3\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr20", "$xr21", "$xr22",
        "$xr23", "$xr24", "$xr31");
}

void I422AlphaToARGBRow_LASX(const uint8_t* src_y,
                             const uint8_t* src_u,
                             const uint8_t* src_v,
                             const uint8_t* src_a,
                             uint8_t* dst_argb,
                             const struct YuvConstants* yuvconstants,
                             int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "vld $vr5, %[sa], 0\n\t"
      "xvpermi.d $xr5, $xr5, 0xd8\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[sa], %[sa], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width), [sa] "+r"(src_a)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr20", "$xr21", "$xr22",
        "$xr23", "$xr24", "$xr31");
}

void I422ToRGB24Row_LASX(const uint8_t* src_y,
                         const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_argb,
                         const struct YuvConstants* yuvconstants,
                         int width) {
  static const uint8_t kShuf0[16] = {0,  1,  2,  4,  5,  6,  8,  9,
                                     10, 12, 13, 14, 16, 17, 18, 20};
  static const uint8_t kShuf1[16] = {21, 22, 24, 25, 26, 28, 29, 30,
                                     0,  0,  0,  0,  0,  0,  0,  0};
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "vld $vr12, %[s0], 0\n\t"
      "xvpermi.q $xr12, $xr12, 0x00\n\t"
      "vld $vr13, %[s1], 0\n\t"
      "xvpermi.q $xr13, $xr13, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr4, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "xvshuf.b $xr10, $xr9, $xr8, $xr12\n\t"
      "xvshuf.b $xr11, $xr9, $xr8, $xr13\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr10, 0x01\n\t"
      "vst $vr31, %[dst], 24\n\t"
      "vstelm.d $vr11, %[dst], 16, 0\n\t"
      "xvpermi.q $xr31, $xr11, 0x01\n\t"
      "vstelm.d $vr31, %[dst], 40, 0\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 48\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants), [s0] "r"(kShuf0), [s1] "r"(kShuf1)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr6", "$xr7",
        "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr20", "$xr21",
        "$xr22", "$xr23", "$xr24", "$xr31");
}

void I422ToRGB565Row_LASX(const uint8_t* src_y,
                          const uint8_t* src_u,
                          const uint8_t* src_v,
                          uint8_t* dst_argb,
                          const struct YuvConstants* yuvconstants,
                          int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr14, 31\n\t"
      "xvsrli.h $xr14, $xr14, 8\n\t"
      "xvldi $xr15, 63\n\t"
      "xvsrli.h $xr15, $xr15, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvsrli.b $xr3, $xr3, 2\n\t"
      "xvsrli.b $xr4, $xr4, 3\n\t"
      "xvilvl.b $xr2, $xr2, $xr2\n\t"
      "xvilvl.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr4, $xr4, $xr4\n\t"
      "xvand.v $xr2, $xr2, $xr14\n\t"
      "xvand.v $xr3, $xr3, $xr15\n\t"
      "xvand.v $xr4, $xr4, $xr14\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr4, $xr4, 11\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvor.v $xr2, $xr2, $xr4\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr10", "$xr11",
        "$xr14", "$xr15", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24", "$xr31");
}

void I422ToARGB4444Row_LASX(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr14, 15\n\t"
      "xvsrli.h $xr14, $xr14, 8\n\t"
      "xvldi $xr15, 240\n\t"
      "xvsrli.h $xr15, $xr15, 8\n\t"
      "xvslli.h $xr15, $xr15, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvsrli.b $xr2, $xr2, 4\n\t"
      "xvsrli.b $xr3, $xr3, 4\n\t"
      "xvsrli.b $xr4, $xr4, 4\n\t"
      "xvilvl.b $xr2, $xr2, $xr2\n\t"
      "xvilvl.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr4, $xr4, $xr4\n\t"
      "xvand.v $xr2, $xr2, $xr14\n\t"
      "xvand.v $xr3, $xr3, $xr14\n\t"
      "xvand.v $xr4, $xr4, $xr14\n\t"
      "xvslli.h $xr3, $xr3, 4\n\t"
      "xvslli.h $xr4, $xr4, 8\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvor.v $xr2, $xr2, $xr4\n\t"
      "xvor.v $xr2, $xr2, $xr15\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr10", "$xr11",
        "$xr14", "$xr15", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24", "$xr31");
}

void I422ToARGB1555Row_LASX(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr14, 31\n\t"
      "xvsrli.h $xr14, $xr14, 8\n\t"
      "xvldi $xr15, 128\n\t"
      "xvsrli.h $xr15, $xr15, 8\n\t"
      "xvslli.h $xr15, $xr15, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvldrepl.d $xr10, %[su], 0\n\t"
      "xvshuf4i.w $xr31, $xr10, 0x01\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "xvldrepl.d $xr11, %[sv], 0\n\t"
      "xvshuf4i.w $xr31, $xr11, 0x01\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvilvl.b $xr10, $xr10, $xr10\n\t"
      "xvilvl.b $xr11, $xr11, $xr11\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvsrli.b $xr3, $xr3, 3\n\t"
      "xvsrli.b $xr4, $xr4, 3\n\t"
      "xvilvl.b $xr2, $xr2, $xr2\n\t"
      "xvilvl.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr4, $xr4, $xr4\n\t"
      "xvand.v $xr2, $xr2, $xr14\n\t"
      "xvand.v $xr3, $xr3, $xr14\n\t"
      "xvand.v $xr4, $xr4, $xr14\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr4, $xr4, 10\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvor.v $xr2, $xr2, $xr4\n\t"
      "xvor.v $xr2, $xr2, $xr15\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr10", "$xr11",
        "$xr14", "$xr15", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24", "$xr31");
}

void NV12ToARGBRow_LASX(const uint8_t* src_y,
                        const uint8_t* src_uv,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr1, %[suv], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "xvilvl.h $xr1, $xr1, $xr1\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[suv], %[suv], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_uv), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24",
        "$xr31");
}

void NV21ToARGBRow_LASX(const uint8_t* src_y,
                        const uint8_t* src_vu,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr1, %[suv], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "xvshuf4i.b $xr1, $xr1, 0xb1\n\t"
      "xvilvl.h $xr1, $xr1, $xr1\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[suv], %[suv], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_vu), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24",
        "$xr31");
}

void NV12ToRGB565Row_LASX(const uint8_t* src_y,
                          const uint8_t* src_uv,
                          uint8_t* dst_argb,
                          const struct YuvConstants* yuvconstants,
                          int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr14, 31\n\t"
      "xvsrli.h $xr14, $xr14, 8\n\t"
      "xvldi $xr15, 63\n\t"
      "xvsrli.h $xr15, $xr15, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr1, %[suv], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "xvilvl.h $xr1, $xr1, $xr1\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvsrli.b $xr3, $xr3, 2\n\t"
      "xvsrli.b $xr4, $xr4, 3\n\t"
      "xvilvl.b $xr2, $xr2, $xr2\n\t"
      "xvilvl.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr4, $xr4, $xr4\n\t"
      "xvand.v $xr2, $xr2, $xr14\n\t"
      "xvand.v $xr3, $xr3, $xr15\n\t"
      "xvand.v $xr4, $xr4, $xr14\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr4, $xr4, 11\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvor.v $xr2, $xr2, $xr4\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[suv], %[suv], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_uv), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr14", "$xr15",
        "$xr20", "$xr21", "$xr22", "$xr23", "$xr24");
}

void I444ToARGBRow_LASX(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr10, %[su], 0\n\t"
      "xvpermi.d $xr10, $xr10, 0xd8\n\t"
      "vld $vr11, %[sv], 0\n\t"
      "xvpermi.d $xr11, $xr11, 0xd8\n\t"
      "xvilvl.b $xr1, $xr11, $xr10\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 16\n\t"
      "addi.d %[sv], %[sv], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr20", "$xr21", "$xr22",
        "$xr23", "$xr24", "$xr31");
}

void I400ToARGBRow_LASX(const uint8_t* src_y,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "vld $vr23, %[c], 96\n\t"
      "xvpermi.q $xr23, $xr23, 0x00\n\t"
      "vld $vr24, %[c], 128\n\t"
      "xvpermi.q $xr24, $xr24, 0x00\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvssrani.bu.h $xr2, $xr0, 6\n\t"
      "xvilvl.b $xr6, $xr2, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr2\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr2", "$xr5", "$xr6", "$xr7", "$xr8", "$xr9",
        "$xr23", "$xr24", "$xr31");
}

void J400ToARGBRow_LASX(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  asm volatile(
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr2, %[sy], 0\n\t"
      "xvpermi.d $xr2, $xr2, 0xd8\n\t"
      "xvilvl.b $xr6, $xr2, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr2\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr2", "$xr5", "$xr6", "$xr7", "$xr8", "$xr9", "$xr31");
}

void YUY2ToARGBRow_LASX(const uint8_t* src_yuy2,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "xvld $xr10, %[src], 0\n\t"
      "xvpickev.b $xr0, $xr10, $xr10\n\t"
      "xvpickod.b $xr1, $xr10, $xr10\n\t"
      "xvilvl.h $xr1, $xr1, $xr1\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr20", "$xr21", "$xr22", "$xr23",
        "$xr24", "$xr31");
}

void UYVYToARGBRow_LASX(const uint8_t* src_uyvy,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "xvldrepl.w $xr20, %[c], 0\n\t"
      "xvldrepl.w $xr21, %[c], 32\n\t"
      "xvldrepl.w $xr22, %[c], 64\n\t"
      "xvldrepl.h $xr23, %[c], 96\n\t"
      "xvldrepl.h $xr24, %[c], 128\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "xvld $xr10, %[src], 0\n\t"
      "xvpickod.b $xr0, $xr10, $xr10\n\t"
      "xvpickev.b $xr1, $xr10, $xr10\n\t"
      "xvilvl.h $xr1, $xr1, $xr1\n\t"
      "xvilvl.b $xr0, $xr0, $xr0\n\t"
      "xvxori.b $xr1, $xr1, 0x80\n\t"
      "xvmuh.hu $xr0, $xr0, $xr23\n\t"
      "xvadd.h $xr0, $xr0, $xr24\n\t"
      "xvmulwev.h.bu.b $xr2, $xr20, $xr1\n\t"
      "xvmulwod.h.bu.b $xr4, $xr22, $xr1\n\t"
      "xvmulwev.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvmaddwod.h.bu.b $xr3, $xr21, $xr1\n\t"
      "xvsadd.h $xr2, $xr0, $xr2\n\t"
      "xvsadd.h $xr4, $xr0, $xr4\n\t"
      "xvssub.h $xr3, $xr0, $xr3\n\t"
      "xvssrani.bu.h $xr2, $xr2, 6\n\t"
      "xvssrani.bu.h $xr3, $xr3, 6\n\t"
      "xvssrani.bu.h $xr4, $xr4, 6\n\t"
      "xvilvl.b $xr6, $xr3, $xr2\n\t"
      "xvilvl.b $xr7, $xr5, $xr4\n\t"
      "xvilvl.h $xr8, $xr7, $xr6\n\t"
      "xvilvh.h $xr9, $xr7, $xr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr8, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr9, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr20", "$xr21", "$xr22", "$xr23",
        "$xr24", "$xr31");
}

static LIBYUV_NOINLINE void ARGBTo24Helper_LASX(const uint8_t* src_argb,
                                                uint8_t* dst_24,
                                                const uint8_t* shuf,
                                                int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 64\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 80\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "vld $vr2, %[src], 32\n\t"
      "vld $vr31, %[src], 96\n\t"
      "xvpermi.q $xr2, $xr31, 0x02\n\t"
      "vld $vr3, %[src], 48\n\t"
      "vld $vr31, %[src], 112\n\t"
      "xvpermi.q $xr3, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr4\n\t"
      "xvshuf.b $xr1, $xr1, $xr1, $xr4\n\t"
      "xvshuf.b $xr2, $xr2, $xr2, $xr4\n\t"
      "xvshuf.b $xr3, $xr3, $xr3, $xr4\n\t"
      "vstelm.d $vr0, %[dst], 0, 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vstelm.d $vr31, %[dst], 48, 0\n\t"
      "vstelm.w $vr0, %[dst], 8, 2\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 56, 2\n\t"
      "vstelm.w $vr1, %[dst], 12, 0\n\t"
      "vstelm.w $vr1, %[dst], 16, 1\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 60, 0\n\t"
      "vstelm.w $vr31, %[dst], 64, 1\n\t"
      "vstelm.w $vr1, %[dst], 20, 2\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 68, 2\n\t"
      "vstelm.d $vr2, %[dst], 24, 0\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vstelm.d $vr31, %[dst], 72, 0\n\t"
      "vstelm.w $vr2, %[dst], 32, 2\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 80, 2\n\t"
      "vstelm.w $vr3, %[dst], 36, 0\n\t"
      "vstelm.w $vr3, %[dst], 40, 1\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 84, 0\n\t"
      "vstelm.w $vr31, %[dst], 88, 1\n\t"
      "vstelm.w $vr3, %[dst], 44, 2\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 92, 2\n\t"
      "addi.d %[src], %[src], 128\n\t"
      "addi.d %[dst], %[dst], 96\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_24), [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void ARGBToRGB24Row_LASX(const uint8_t* src_argb,
                         uint8_t* dst_rgb24,
                         int width) {
  static const uint8_t kShuf[16] = {0,  1,  2,  4,  5, 6, 8, 9,
                                    10, 12, 13, 14, 0, 0, 0, 0};
  ARGBTo24Helper_LASX(src_argb, dst_rgb24, kShuf, width);
}

void ARGBToRAWRow_LASX(const uint8_t* src_argb, uint8_t* dst_raw, int width) {
  static const uint8_t kShuf[16] = {2, 1,  0,  6,  5, 4, 10, 9,
                                    8, 14, 13, 12, 0, 0, 0,  0};
  ARGBTo24Helper_LASX(src_argb, dst_raw, kShuf, width);
}

static LIBYUV_NOINLINE void RGB24ToARGBHelper_LASX(const uint8_t* src_24,
                                                   uint8_t* dst_argb,
                                                   const uint8_t* shuf,
                                                   int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "xvpermi.q $xr5, $xr5, 0x00\n\t"
      "xvldi $xr6, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 12\n\t"
      "vld $vr31, %[src], 60\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "vld $vr2, %[src], 24\n\t"
      "vld $vr31, %[src], 72\n\t"
      "xvpermi.q $xr2, $xr31, 0x02\n\t"
      "vld $vr3, %[src], 32\n\t"
      "vld $vr31, %[src], 80\n\t"
      "xvpermi.q $xr3, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr6, $xr0, $xr4\n\t"
      "xvshuf.b $xr1, $xr6, $xr1, $xr4\n\t"
      "xvshuf.b $xr2, $xr6, $xr2, $xr4\n\t"
      "xvshuf.b $xr3, $xr6, $xr3, $xr5\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vst $vr31, %[dst], 64\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vst $vr31, %[dst], 80\n\t"
      "vst $vr2, %[dst], 32\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vst $vr31, %[dst], 96\n\t"
      "vst $vr3, %[dst], 48\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vst $vr31, %[dst], 112\n\t"
      "addi.d %[src], %[src], 96\n\t"
      "addi.d %[dst], %[dst], 128\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_24), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr31");
}

void RGB24ToARGBRow_LASX(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width) {
  static const uint8_t kShuf[32] = {0,  1,  2,  16, 3,  4,  5,  16, 6,  7, 8,
                                    16, 9,  10, 11, 16, 4,  5,  6,  16, 7, 8,
                                    9,  16, 10, 11, 12, 16, 13, 14, 15, 16};
  RGB24ToARGBHelper_LASX(src_rgb24, dst_argb, kShuf, width);
}

void RAWToARGBRow_LASX(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  static const uint8_t kShuf[32] = {2,  1,  0,  16, 5,  4,  3,  16, 8,  7, 6,
                                    16, 11, 10, 9,  16, 6,  5,  4,  16, 9, 8,
                                    7,  16, 12, 11, 10, 16, 15, 14, 13, 16};
  RGB24ToARGBHelper_LASX(src_raw, dst_argb, kShuf, width);
}

void RAWToRGB24Row_LASX(const uint8_t* src_raw, uint8_t* dst_rgb24, int width) {
  static const uint8_t kShuf[32] = {2,  1,  0,  5,  4,  3,  8, 7, 6, 11, 10,
                                    9,  0,  0,  0,  0,  6,  5, 4, 9, 8,  7,
                                    12, 11, 10, 15, 14, 13, 0, 0, 0, 0};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "xvpermi.q $xr5, $xr5, 0x00\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 12\n\t"
      "vld $vr31, %[src], 60\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "vld $vr2, %[src], 24\n\t"
      "vld $vr31, %[src], 72\n\t"
      "xvpermi.q $xr2, $xr31, 0x02\n\t"
      "vld $vr3, %[src], 32\n\t"
      "vld $vr31, %[src], 80\n\t"
      "xvpermi.q $xr3, $xr31, 0x02\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr4\n\t"
      "xvshuf.b $xr1, $xr1, $xr1, $xr4\n\t"
      "xvshuf.b $xr2, $xr2, $xr2, $xr4\n\t"
      "xvshuf.b $xr3, $xr3, $xr3, $xr5\n\t"
      "vstelm.d $vr0, %[dst], 0, 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vstelm.d $vr31, %[dst], 48, 0\n\t"
      "vstelm.w $vr0, %[dst], 8, 2\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 56, 2\n\t"
      "vstelm.w $vr1, %[dst], 12, 0\n\t"
      "vstelm.w $vr1, %[dst], 16, 1\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 60, 0\n\t"
      "vstelm.w $vr31, %[dst], 64, 1\n\t"
      "vstelm.w $vr1, %[dst], 20, 2\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 68, 2\n\t"
      "vstelm.d $vr2, %[dst], 24, 0\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vstelm.d $vr31, %[dst], 72, 0\n\t"
      "vstelm.w $vr2, %[dst], 32, 2\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 80, 2\n\t"
      "vstelm.w $vr3, %[dst], 36, 0\n\t"
      "vstelm.w $vr3, %[dst], 40, 1\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 84, 0\n\t"
      "vstelm.w $vr31, %[dst], 88, 1\n\t"
      "vstelm.w $vr3, %[dst], 44, 2\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vstelm.w $vr31, %[dst], 92, 2\n\t"
      "addi.d %[src], %[src], 96\n\t"
      "addi.d %[dst], %[dst], 96\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_raw), [dst] "+r"(dst_rgb24), [cnt] "+r"(width)
      : [shuf] "r"(kShuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr31");
}

void ARGBToRGB565Row_LASX(const uint8_t* src_argb,
                          uint8_t* dst_rgb,
                          int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvsrli.b $xr3, $xr3, 2\n\t"
      "xvand.v $xr4, $xr2, $xr6\n\t"
      "xvsrli.h $xr2, $xr2, 8\n\t"
      "xvand.v $xr3, $xr3, $xr7\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr2, $xr2, 11\n\t"
      "xvor.v $xr4, $xr4, $xr3\n\t"
      "xvor.v $xr4, $xr4, $xr2\n\t"
      "xvst $xr4, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr6", "$xr7",
        "$xr31");
}

void ARGBToRGB565DitherRow_LASX(const uint8_t* src_argb,
                                uint8_t* dst_rgb,
                                uint32_t dither4,
                                int width) {
  asm volatile(
      "xvreplgr2vr.w $xr5, %[dith]\n\t"
      "xvilvl.b $xr5, $xr5, $xr5\n\t"
      "xvilvl.h $xr5, $xr5, $xr5\n\t"
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvsadd.bu $xr0, $xr0, $xr5\n\t"
      "xvpickev.b $xr2, $xr0, $xr0\n\t"
      "xvpickod.b $xr3, $xr0, $xr0\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvsrli.b $xr3, $xr3, 2\n\t"
      "xvand.v $xr4, $xr2, $xr6\n\t"
      "xvsrli.h $xr2, $xr2, 8\n\t"
      "xvand.v $xr3, $xr3, $xr7\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr2, $xr2, 11\n\t"
      "xvor.v $xr4, $xr4, $xr3\n\t"
      "xvor.v $xr4, $xr4, $xr2\n\t"
      "xvpermi.d $xr31, $xr4, 0xd8\n\t"
      "vst $vr31, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      : [dith] "r"(dither4)
      : "memory", "$xr0", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6", "$xr7",
        "$xr31");
}

void ARGBToARGB1555Row_LASX(const uint8_t* src_argb,
                            uint8_t* dst_rgb,
                            int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvsrli.b $xr2, $xr2, 3\n\t"
      "xvand.v $xr4, $xr2, $xr6\n\t"
      "xvsrli.h $xr2, $xr2, 8\n\t"
      "xvsrli.h $xr5, $xr3, 15\n\t"
      "xvsrli.b $xr3, $xr3, 3\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvslli.h $xr3, $xr3, 5\n\t"
      "xvslli.h $xr2, $xr2, 10\n\t"
      "xvslli.h $xr5, $xr5, 15\n\t"
      "xvor.v $xr4, $xr4, $xr3\n\t"
      "xvor.v $xr4, $xr4, $xr2\n\t"
      "xvor.v $xr4, $xr4, $xr5\n\t"
      "xvst $xr4, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr31");
}

void ARGBToARGB4444Row_LASX(const uint8_t* src_argb,
                            uint8_t* dst_rgb,
                            int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvsrli.b $xr2, $xr2, 4\n\t"
      "xvsrli.b $xr3, $xr3, 4\n\t"
      "xvslli.b $xr3, $xr3, 4\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr31");
}

void RGB565ToARGBRow_LASX(const uint8_t* src_rgb565,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "xvldi $xr5, 0xff\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr1, $xr1, $xr1\n\t"
      "xvpickev.b $xr2, $xr2, $xr2\n\t"
      "xvpickev.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr8, $xr2, $xr1\n\t"
      "xvilvl.b $xr9, $xr5, $xr3\n\t"
      "xvilvl.h $xr10, $xr9, $xr8\n\t"
      "xvilvh.h $xr11, $xr9, $xr8\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr10, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr11, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr11, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgb565), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr31");
}

void ARGB1555ToARGBRow_LASX(const uint8_t* src_argb1555,
                            uint8_t* dst_argb,
                            int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr1, $xr1, $xr1\n\t"
      "xvpickev.b $xr2, $xr2, $xr2\n\t"
      "xvpickev.b $xr3, $xr3, $xr3\n\t"
      "xvpickev.b $xr5, $xr5, $xr5\n\t"
      "xvilvl.b $xr8, $xr2, $xr1\n\t"
      "xvilvl.b $xr9, $xr5, $xr3\n\t"
      "xvilvl.h $xr10, $xr9, $xr8\n\t"
      "xvilvh.h $xr11, $xr9, $xr8\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr10, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr11, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr11, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb1555), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr8", "$xr9", "$xr10", "$xr11", "$xr31");
}

void ARGB4444ToARGBRow_LASX(const uint8_t* src_argb4444,
                            uint8_t* dst_argb,
                            int width) {
  asm volatile(
      "xvldi $xr6, 0x0f\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.b $xr2, $xr0, 4\n\t"
      "xvslli.b $xr3, $xr1, 4\n\t"
      "xvor.v $xr1, $xr1, $xr3\n\t"
      "xvslli.b $xr3, $xr2, 4\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvilvl.b $xr4, $xr2, $xr1\n\t"
      "xvilvh.b $xr5, $xr2, $xr1\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr4, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr5, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb4444), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr31");
}

void ARGBMultiplyRow_LASX(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvmulwev.h.bu $xr2, $xr0, $xr1\n\t"
      "xvmulwod.h.bu $xr3, $xr0, $xr1\n\t"
      "xvsrari.h $xr2, $xr2, 8\n\t"
      "xvsrari.h $xr3, $xr3, 8\n\t"
      "xvpackev.b $xr2, $xr3, $xr2\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3");
}

void ARGBAddRow_LASX(const uint8_t* src_argb,
                     const uint8_t* src_argb1,
                     uint8_t* dst_argb,
                     int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvsadd.bu $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1");
}

void ARGBSubtractRow_LASX(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvssub.bu $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1");
}

void ARGBAttenuateRow_LASX(const uint8_t* src_argb,
                           uint8_t* dst_argb,
                           int width) {
  static const uint8_t kAlpha[16] = {3,  3,  3,  16, 7,  7,  7,  16,
                                     11, 11, 11, 16, 15, 15, 15, 16};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "xvldi $xr5, 0xff\n\t"
      "xvldi $xr6, 255\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvshuf.b $xr1, $xr5, $xr0, $xr4\n\t"
      "xvmulwev.h.bu $xr2, $xr0, $xr1\n\t"
      "xvmulwod.h.bu $xr3, $xr0, $xr1\n\t"
      "xvadd.h $xr2, $xr2, $xr6\n\t"
      "xvadd.h $xr3, $xr3, $xr6\n\t"
      "xvpackod.b $xr2, $xr3, $xr2\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(kAlpha)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6");
}

void ARGBBlendRow_LASX(const uint8_t* src_argb0,
                       const uint8_t* src_argb1,
                       uint8_t* dst_argb,
                       int width) {
  static const uint8_t kAlpha[16] = {3,  3,  3,  3,  7,  7,  7,  7,
                                     11, 11, 11, 11, 15, 15, 15, 15};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "xvldi $xr5, 0xff\n\t"
      "xvslli.w $xr5, $xr5, 24\n\t"
      "xvldi $xr6, 1\n\t"
      "xvslli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr8, 0\n\t"
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvshuf.b $xr2, $xr0, $xr0, $xr4\n\t"
      "xvilvl.b $xr3, $xr8, $xr2\n\t"
      "xvilvh.b $xr2, $xr8, $xr2\n\t"
      "xvsub.h $xr3, $xr6, $xr3\n\t"
      "xvsub.h $xr2, $xr6, $xr2\n\t"
      "xvilvl.b $xr7, $xr8, $xr1\n\t"
      "xvilvh.b $xr1, $xr8, $xr1\n\t"
      "xvmul.h $xr7, $xr7, $xr3\n\t"
      "xvmul.h $xr1, $xr1, $xr2\n\t"
      "xvsrlni.b.h $xr1, $xr7, 8\n\t"
      "xvsadd.bu $xr0, $xr0, $xr1\n\t"
      "xvor.v $xr0, $xr0, $xr5\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb0), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [shuf] "r"(kAlpha)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8");
}

void ARGBOrRow_LASX(const uint8_t* src_argb,
                    uint8_t* dst_argb,
                    uint32_t value,
                    int width) {
  asm volatile(
      "xvreplgr2vr.w $xr1, %[val]\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvor.v $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(value)
      : "memory", "$xr0", "$xr1");
}

void ARGBAlphaBlendRow_LASX(const uint8_t* src_argb0,
                            const uint8_t* src_argb1,
                            uint8_t* dst_argb,
                            int width) {
  static const uint8_t kShufRatio[16] = {0, 0, 0, 0, 4,  4,  4,  4,
                                         8, 8, 8, 8, 12, 12, 12, 12};
  asm volatile(
      "vld $vr8, %[shuf], 0\n\t"
      "xvpermi.q $xr8, $xr8, 0x00\n\t"
      "xvldi $xr9, 1\n\t"
      "xvsrli.h $xr10, $xr9, 8\n\t"
      "xvldi $xr11, 0xff\n\t"
      "xvsrli.w $xr11, $xr11, 8\n\t"
      "xvldi $xr12, 0\n\t"
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvsrli.w $xr2, $xr0, 24\n\t"
      "xvsrli.w $xr3, $xr1, 24\n\t"
      "xvmul.h $xr4, $xr3, $xr2\n\t"
      "xvadd.h $xr4, $xr4, $xr10\n\t"
      "xvmuh.hu $xr4, $xr4, $xr9\n\t"
      "xvsub.w $xr4, $xr2, $xr4\n\t"
      "xvadd.w $xr4, $xr3, $xr4\n\t"
      "xvseq.w $xr5, $xr3, $xr12\n\t"
      "xvsrli.w $xr5, $xr5, 24\n\t"
      "xvor.v $xr6, $xr2, $xr5\n\t"
      "xvor.v $xr7, $xr4, $xr5\n\t"
      "xvslli.w $xr5, $xr6, 8\n\t"
      "xvsub.w $xr5, $xr5, $xr6\n\t"
      "xvffint.s.wu $xr5, $xr5\n\t"
      "xvffint.s.wu $xr7, $xr7\n\t"
      "xvfdiv.s $xr5, $xr5, $xr7\n\t"
      "xvftintrz.wu.s $xr5, $xr5\n\t"
      "xvshuf.b $xr5, $xr5, $xr5, $xr8\n\t"
      "xvnor.v $xr6, $xr5, $xr5\n\t"
      "xvmulwev.h.bu $xr2, $xr1, $xr6\n\t"
      "xvmulwod.h.bu $xr3, $xr1, $xr6\n\t"
      "xvmulwev.h.bu $xr7, $xr0, $xr5\n\t"
      "xvmulwod.h.bu $xr0, $xr0, $xr5\n\t"
      "xvadd.h $xr2, $xr2, $xr7\n\t"
      "xvadd.h $xr3, $xr3, $xr0\n\t"
      "xvadd.h $xr2, $xr2, $xr10\n\t"
      "xvadd.h $xr3, $xr3, $xr10\n\t"
      "xvmuh.hu $xr2, $xr2, $xr9\n\t"
      "xvmuh.hu $xr3, $xr3, $xr9\n\t"
      "xvslli.h $xr3, $xr3, 8\n\t"
      "xvor.v $xr2, $xr2, $xr3\n\t"
      "xvand.v $xr2, $xr2, $xr11\n\t"
      "xvslli.w $xr4, $xr4, 24\n\t"
      "xvor.v $xr2, $xr2, $xr4\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb0), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [shuf] "r"(kShufRatio)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12");
}

void ARGBShadeRow_LASX(const uint8_t* src_argb,
                       uint8_t* dst_argb,
                       int width,
                       uint32_t value) {
  asm volatile(
      "xvreplgr2vr.w $xr4, %[val]\n\t"
      "xvilvl.b $xr5, $xr4, $xr4\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvilvl.b $xr1, $xr0, $xr0\n\t"
      "xvilvh.b $xr2, $xr0, $xr0\n\t"
      "xvmuh.hu $xr1, $xr1, $xr5\n\t"
      "xvmuh.hu $xr2, $xr2, $xr5\n\t"
      "xvsrlni.b.h $xr2, $xr1, 8\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(value)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr4", "$xr5");
}

void ARGBShuffleRow_LASX(const uint8_t* src_argb,
                         uint8_t* dst_argb,
                         const uint8_t* shuffler,
                         int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvshuf.b $xr0, $xr0, $xr0, $xr4\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(shuffler)
      : "memory", "$xr0", "$xr4");
}

void ARGBGrayRow_LASX(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  asm volatile(
      "xvldi $xr4, 29\n\t"
      "xvldi $xr5, 150\n\t"
      "xvldi $xr6, 77\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvmulwev.h.bu $xr7, $xr2, $xr4\n\t"
      "xvmaddwod.h.bu $xr7, $xr2, $xr6\n\t"
      "xvmaddwev.h.bu $xr7, $xr3, $xr5\n\t"
      "xvsrlri.h $xr7, $xr7, 8\n\t"
      "xvsrli.h $xr3, $xr3, 8\n\t"
      "xvslli.h $xr3, $xr3, 8\n\t"
      "xvor.v $xr3, $xr7, $xr3\n\t"
      "xvslli.h $xr8, $xr7, 8\n\t"
      "xvor.v $xr7, $xr7, $xr8\n\t"
      "xvilvl.h $xr0, $xr3, $xr7\n\t"
      "xvilvh.h $xr1, $xr3, $xr7\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr31");
}

void ARGBSepiaRow_LASX(uint8_t* dst_argb, int width) {
  asm volatile(
      "xvldi $xr4, 17\n\t"
      "xvldi $xr5, 68\n\t"
      "xvldi $xr6, 35\n\t"
      "xvldi $xr7, 22\n\t"
      "xvldi $xr8, 88\n\t"
      "xvldi $xr9, 45\n\t"
      "xvldi $xr10, 24\n\t"
      "xvldi $xr11, 98\n\t"
      "xvldi $xr12, 50\n\t"
      "1:\n\t"
      "vld $vr0, %[dst], 0\n\t"
      "vld $vr31, %[dst], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "vld $vr1, %[dst], 16\n\t"
      "vld $vr31, %[dst], 48\n\t"
      "xvpermi.q $xr1, $xr31, 0x02\n\t"
      "xvpickev.b $xr2, $xr1, $xr0\n\t"
      "xvpickod.b $xr3, $xr1, $xr0\n\t"
      "xvmulwev.h.bu $xr13, $xr2, $xr4\n\t"
      "xvmaddwod.h.bu $xr13, $xr2, $xr6\n\t"
      "xvmaddwev.h.bu $xr13, $xr3, $xr5\n\t"
      "xvmulwev.h.bu $xr14, $xr2, $xr7\n\t"
      "xvmaddwod.h.bu $xr14, $xr2, $xr9\n\t"
      "xvmaddwev.h.bu $xr14, $xr3, $xr8\n\t"
      "xvmulwev.h.bu $xr15, $xr2, $xr10\n\t"
      "xvmaddwod.h.bu $xr15, $xr2, $xr12\n\t"
      "xvmaddwev.h.bu $xr15, $xr3, $xr11\n\t"
      "xvsrli.h $xr13, $xr13, 7\n\t"
      "xvsrli.h $xr14, $xr14, 7\n\t"
      "xvsrli.h $xr15, $xr15, 7\n\t"
      "xvsat.hu $xr13, $xr13, 7\n\t"
      "xvsat.hu $xr14, $xr14, 7\n\t"
      "xvsat.hu $xr15, $xr15, 7\n\t"
      "xvpickev.b $xr13, $xr13, $xr13\n\t"
      "xvpickev.b $xr14, $xr14, $xr14\n\t"
      "xvpickev.b $xr15, $xr15, $xr15\n\t"
      "xvpickod.b $xr3, $xr3, $xr3\n\t"
      "xvilvl.b $xr2, $xr14, $xr13\n\t"
      "xvilvl.b $xr3, $xr3, $xr15\n\t"
      "xvilvl.h $xr0, $xr3, $xr2\n\t"
      "xvilvh.h $xr1, $xr3, $xr2\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15", "$xr31");
}

void ARGBColorMatrixRow_LASX(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             const int8_t* matrix_argb,
                             int width) {
  asm volatile(
      "vldrepl.w $vr4, %[mat], 0\n\t"
      "vldrepl.w $vr31, %[mat], 0\n\t"
      "xvpermi.q $xr4, $xr31, 0x02\n\t"
      "vldrepl.w $vr5, %[mat], 4\n\t"
      "vldrepl.w $vr31, %[mat], 4\n\t"
      "xvpermi.q $xr5, $xr31, 0x02\n\t"
      "vldrepl.w $vr6, %[mat], 8\n\t"
      "vldrepl.w $vr31, %[mat], 8\n\t"
      "xvpermi.q $xr6, $xr31, 0x02\n\t"
      "vldrepl.w $vr7, %[mat], 12\n\t"
      "vldrepl.w $vr31, %[mat], 12\n\t"
      "xvpermi.q $xr7, $xr31, 0x02\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvmulwev.h.bu.b $xr8, $xr0, $xr4\n\t"
      "xvmaddwod.h.bu.b $xr8, $xr0, $xr4\n\t"
      "xvmulwev.h.bu.b $xr9, $xr0, $xr5\n\t"
      "xvmaddwod.h.bu.b $xr9, $xr0, $xr5\n\t"
      "xvmulwev.h.bu.b $xr10, $xr0, $xr6\n\t"
      "xvmaddwod.h.bu.b $xr10, $xr0, $xr6\n\t"
      "xvmulwev.h.bu.b $xr11, $xr0, $xr7\n\t"
      "xvmaddwod.h.bu.b $xr11, $xr0, $xr7\n\t"
      "xvhaddw.w.h $xr8, $xr8, $xr8\n\t"
      "xvhaddw.w.h $xr9, $xr9, $xr9\n\t"
      "xvhaddw.w.h $xr10, $xr10, $xr10\n\t"
      "xvhaddw.w.h $xr11, $xr11, $xr11\n\t"
      "xvilvl.w $xr12, $xr9, $xr8\n\t"
      "xvilvh.w $xr13, $xr9, $xr8\n\t"
      "xvilvl.w $xr14, $xr11, $xr10\n\t"
      "xvilvh.w $xr15, $xr11, $xr10\n\t"
      "xvilvl.d $xr8, $xr14, $xr12\n\t"
      "xvilvh.d $xr9, $xr14, $xr12\n\t"
      "xvilvl.d $xr10, $xr15, $xr13\n\t"
      "xvilvh.d $xr11, $xr15, $xr13\n\t"
      "xvssrani.h.w $xr9, $xr8, 6\n\t"
      "xvssrani.h.w $xr11, $xr10, 6\n\t"
      "xvssrani.bu.h $xr11, $xr9, 0\n\t"
      "xvst $xr11, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [mat] "r"(matrix_argb)
      : "memory", "$xr0", "$xr4", "$xr5", "$xr6", "$xr7", "$xr8", "$xr9",
        "$xr10", "$xr11", "$xr12", "$xr13", "$xr14", "$xr15", "$xr31");
}

void ARGBQuantizeRow_LASX(uint8_t* dst_argb,
                          int scale,
                          int interval_size,
                          int interval_offset,
                          int width) {
  asm volatile(
      "xvreplgr2vr.h $xr4, %[scl]\n\t"
      "xvreplgr2vr.b $xr5, %[isz]\n\t"
      "xvreplgr2vr.b $xr6, %[ioff]\n\t"
      "xvldi $xr7, 0xff\n\t"
      "xvslli.w $xr7, $xr7, 24\n\t"
      "1:\n\t"
      "xvld $xr0, %[dst], 0\n\t"
      "xvilvl.b $xr1, $xr0, $xr0\n\t"
      "xvilvh.b $xr2, $xr0, $xr0\n\t"
      "xvsrli.h $xr1, $xr1, 8\n\t"
      "xvsrli.h $xr2, $xr2, 8\n\t"
      "xvmuh.hu $xr1, $xr1, $xr4\n\t"
      "xvmuh.hu $xr2, $xr2, $xr4\n\t"
      "xvsrlni.b.h $xr2, $xr1, 0\n\t"
      "xvmul.b $xr2, $xr2, $xr5\n\t"
      "xvadd.b $xr2, $xr2, $xr6\n\t"
      "xvand.v $xr0, $xr0, $xr7\n\t"
      "xvandn.v $xr2, $xr7, $xr2\n\t"
      "xvor.v $xr2, $xr2, $xr0\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [scl] "r"(scale), [isz] "r"(interval_size), [ioff] "r"(interval_offset)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr4", "$xr5", "$xr6", "$xr7");
}

void SobelRow_LASX(const uint8_t* src_sobelx,
                   const uint8_t* src_sobely,
                   uint8_t* dst_argb,
                   int width) {
  asm volatile(
      "xvldi $xr4, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sx], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr1, %[sy], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "xvsadd.bu $xr0, $xr0, $xr1\n\t"
      "xvilvl.b $xr2, $xr0, $xr0\n\t"
      "xvilvl.b $xr3, $xr4, $xr0\n\t"
      "xvilvl.h $xr0, $xr3, $xr2\n\t"
      "xvilvh.h $xr1, $xr3, $xr2\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr0, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr1, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sx], %[sx], 16\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void SobelToPlaneRow_LASX(const uint8_t* src_sobelx,
                          const uint8_t* src_sobely,
                          uint8_t* dst_y,
                          int width) {
  asm volatile(
      "1:\n\t"
      "xvld $xr0, %[sx], 0\n\t"
      "xvld $xr1, %[sy], 0\n\t"
      "xvsadd.bu $xr0, $xr0, $xr1\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "addi.d %[sx], %[sx], 32\n\t"
      "addi.d %[sy], %[sy], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_y),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1");
}

void SobelXYRow_LASX(const uint8_t* src_sobelx,
                     const uint8_t* src_sobely,
                     uint8_t* dst_argb,
                     int width) {
  asm volatile(
      "xvldi $xr4, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[sx], 0\n\t"
      "xvpermi.d $xr0, $xr0, 0xd8\n\t"
      "vld $vr1, %[sy], 0\n\t"
      "xvpermi.d $xr1, $xr1, 0xd8\n\t"
      "xvsadd.bu $xr2, $xr0, $xr1\n\t"
      "xvilvl.b $xr1, $xr2, $xr1\n\t"
      "xvilvl.b $xr0, $xr4, $xr0\n\t"
      "xvilvl.h $xr2, $xr0, $xr1\n\t"
      "xvilvh.h $xr3, $xr0, $xr1\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "xvpermi.q $xr31, $xr2, 0x01\n\t"
      "vst $vr31, %[dst], 32\n\t"
      "vst $vr3, %[dst], 16\n\t"
      "xvpermi.q $xr31, $xr3, 0x01\n\t"
      "vst $vr31, %[dst], 48\n\t"
      "addi.d %[sx], %[sx], 16\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr31");
}

void InterpolateRow_LASX(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int width,
                         int32_t source_y_fraction) {
  int y1_fraction = source_y_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  if (y1_fraction == 0) {
    CopyRow_LASX(src_ptr, dst_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    asm volatile(
        "1:\n\t"
        "xvld $xr0, %[s0], 0\n\t"
        "xvld $xr1, %[s1], 0\n\t"
        "xvavgr.bu $xr0, $xr0, $xr1\n\t"
        "xvst $xr0, %[dst], 0\n\t"
        "addi.d %[s0], %[s0], 32\n\t"
        "addi.d %[s1], %[s1], 32\n\t"
        "addi.d %[dst], %[dst], 32\n\t"
        "addi.d %[cnt], %[cnt], -32\n\t"
        "bnez %[cnt], 1b\n\t"
        : [s0] "+r"(src_ptr), [s1] "+r"(src_ptr1), [dst] "+r"(dst_ptr),
          [cnt] "+r"(width)
        :
        : "memory", "$xr0", "$xr1");
    return;
  }
  uint16_t y_frac = (uint16_t)((256 - y1_fraction) + (y1_fraction << 8));
  asm volatile(
      "xvreplgr2vr.h $xr3, %[yf]\n\t"
      "1:\n\t"
      "xvld $xr0, %[s0], 0\n\t"
      "xvld $xr1, %[s1], 0\n\t"
      "xvilvl.b $xr2, $xr1, $xr0\n\t"
      "xvilvh.b $xr4, $xr1, $xr0\n\t"
      "xvmulwev.h.bu $xr0, $xr2, $xr3\n\t"
      "xvmaddwod.h.bu $xr0, $xr2, $xr3\n\t"
      "xvmulwev.h.bu $xr1, $xr4, $xr3\n\t"
      "xvmaddwod.h.bu $xr1, $xr4, $xr3\n\t"
      "xvsrarni.b.h $xr1, $xr0, 8\n\t"
      "xvst $xr1, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_ptr), [s1] "+r"(src_ptr1), [dst] "+r"(dst_ptr),
        [cnt] "+r"(width)
      : [yf] "r"(y_frac)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4");
}

void HalfFloatRow_LASX(const uint16_t* src,
                       uint16_t* dst,
                       float scale,
                       int width) {
  const float bias = 1.9259299444e-34f;  // 2^-112, see HalfFloatRow_C
  asm volatile(
      "xvldrepl.w $xr3, %[scale], 0\n\t"
      "xvldrepl.w $xr5, %[bias], 0\n\t"
      "xvldi $xr4, 0\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvilvl.h $xr1, $xr4, $xr0\n\t"
      "xvilvh.h $xr2, $xr4, $xr0\n\t"
      "xvffint.s.wu $xr1, $xr1\n\t"
      "xvffint.s.wu $xr2, $xr2\n\t"
      "xvfmul.s $xr1, $xr1, $xr3\n\t"
      "xvfmul.s $xr2, $xr2, $xr3\n\t"
      "xvfmul.s $xr1, $xr1, $xr5\n\t"
      "xvfmul.s $xr2, $xr2, $xr5\n\t"
      "xvsrlni.h.w $xr2, $xr1, 13\n\t"
      "xvst $xr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [dst] "+r"(dst), [cnt] "+r"(width)
      : [scale] "r"(&scale), [bias] "r"(&bias)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5");
}

void ARGBToYMatrixRow_LASX(const uint8_t* src_argb,
                           uint8_t* dst_y,
                           int width,
                           const struct ArgbConstants* c) {
  asm volatile(
      "xvldrepl.b $xr0, %[c], 0\n\t"
      "xvldrepl.b $xr1, %[c], 1\n\t"
      "xvldrepl.b $xr2, %[c], 2\n\t"
      "xvldrepl.b $xr3, %[c], 3\n\t"
      "xvldrepl.h $xr4, %[c], 96\n\t"
      "1:\n\t"
      "vld $vr5, %[src], 0\n\t"
      "vld $vr31, %[src], 32\n\t"
      "xvpermi.q $xr5, $xr31, 0x02\n\t"
      "vld $vr6, %[src], 16\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr6, $xr31, 0x02\n\t"
      "xvor.v $xr9, $xr4, $xr4\n\t"
      "xvpickev.b $xr7, $xr6, $xr5\n\t"
      "xvpickod.b $xr8, $xr6, $xr5\n\t"
      "xvmaddwev.h.bu $xr9, $xr7, $xr0\n\t"
      "xvmaddwev.h.bu $xr9, $xr8, $xr1\n\t"
      "xvmaddwod.h.bu $xr9, $xr7, $xr2\n\t"
      "xvmaddwod.h.bu $xr9, $xr8, $xr3\n\t"
      "xvpickod.b $xr9, $xr9, $xr9\n\t"
      "xvpermi.d $xr31, $xr9, 0xd8\n\t"
      "vst $vr31, %[dst], 0\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [c] "r"(c)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr31");
}

void RGBToYMatrixRow_LASX(const uint8_t* src_rgba,
                          uint8_t* dst_y,
                          int width,
                          const struct ArgbConstants* c) {
  static const uint8_t kShuf[32] = {0, 1, 2,  0,  3,  4, 5,  0,  6,  7, 8,
                                    0, 9, 10, 11, 0,  4, 5,  6,  0,  7, 8,
                                    9, 0, 10, 11, 12, 0, 13, 14, 15, 0};
  asm volatile(
      "xvldrepl.b $xr0, %[c], 0\n\t"
      "xvldrepl.b $xr1, %[c], 1\n\t"
      "xvldrepl.b $xr2, %[c], 2\n\t"
      "xvldrepl.h $xr3, %[c], 96\n\t"
      "vld $vr4, %[shuf], 0\n\t"
      "xvpermi.q $xr4, $xr4, 0x00\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "xvpermi.q $xr5, $xr5, 0x00\n\t"
      "1:\n\t"
      "vld $vr6, %[src], 0\n\t"
      "vld $vr31, %[src], 48\n\t"
      "xvpermi.q $xr6, $xr31, 0x02\n\t"
      "vld $vr7, %[src], 12\n\t"
      "vld $vr31, %[src], 60\n\t"
      "xvpermi.q $xr7, $xr31, 0x02\n\t"
      "vld $vr8, %[src], 24\n\t"
      "vld $vr31, %[src], 72\n\t"
      "xvpermi.q $xr8, $xr31, 0x02\n\t"
      "vld $vr9, %[src], 32\n\t"
      "vld $vr31, %[src], 80\n\t"
      "xvpermi.q $xr9, $xr31, 0x02\n\t"
      "xvshuf.b $xr6, $xr6, $xr6, $xr4\n\t"
      "xvshuf.b $xr7, $xr7, $xr7, $xr4\n\t"
      "xvshuf.b $xr8, $xr8, $xr8, $xr4\n\t"
      "xvshuf.b $xr9, $xr9, $xr9, $xr5\n\t"
      "xvor.v $xr12, $xr3, $xr3\n\t"
      "xvor.v $xr13, $xr3, $xr3\n\t"
      "xvpickev.b $xr10, $xr7, $xr6\n\t"
      "xvpickod.b $xr11, $xr7, $xr6\n\t"
      "xvmaddwev.h.bu $xr12, $xr10, $xr0\n\t"
      "xvmaddwev.h.bu $xr12, $xr11, $xr1\n\t"
      "xvmaddwod.h.bu $xr12, $xr10, $xr2\n\t"
      "xvpickev.b $xr10, $xr9, $xr8\n\t"
      "xvpickod.b $xr11, $xr9, $xr8\n\t"
      "xvmaddwev.h.bu $xr13, $xr10, $xr0\n\t"
      "xvmaddwev.h.bu $xr13, $xr11, $xr1\n\t"
      "xvmaddwod.h.bu $xr13, $xr10, $xr2\n\t"
      "xvpickod.b $xr12, $xr13, $xr12\n\t"
      "xvst $xr12, %[dst], 0\n\t"
      "addi.d %[src], %[src], 96\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgba), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [c] "r"(c), [shuf] "r"(kShuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr31");
}

void RGB565ToYRow_LASX(const uint8_t* src_rgb565, uint8_t* dst_y, int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "xvldi $xr8, 25\n\t"
      "xvsrli.h $xr8, $xr8, 8\n\t"
      "xvldi $xr9, 129\n\t"
      "xvsrli.h $xr9, $xr9, 8\n\t"
      "xvldi $xr10, 66\n\t"
      "xvsrli.h $xr10, $xr10, 8\n\t"
      "xvldi $xr11, 33\n\t"
      "xvsrli.h $xr11, $xr11, 8\n\t"
      "xvslli.h $xr11, $xr11, 7\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvmul.h $xr1, $xr1, $xr8\n\t"
      "xvmadd.h $xr1, $xr2, $xr9\n\t"
      "xvmadd.h $xr1, $xr3, $xr10\n\t"
      "xvadd.h $xr1, $xr1, $xr11\n\t"
      "xvpickod.b $xr1, $xr1, $xr1\n\t"
      "xvpermi.d $xr31, $xr1, 0xd8\n\t"
      "vst $vr31, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgb565), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr6", "$xr7",
        "$xr8", "$xr9", "$xr10", "$xr11", "$xr31");
}

void ARGB1555ToYRow_LASX(const uint8_t* src_argb1555,
                         uint8_t* dst_y,
                         int width) {
  asm volatile(
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr8, 25\n\t"
      "xvsrli.h $xr8, $xr8, 8\n\t"
      "xvldi $xr9, 129\n\t"
      "xvsrli.h $xr9, $xr9, 8\n\t"
      "xvldi $xr10, 66\n\t"
      "xvsrli.h $xr10, $xr10, 8\n\t"
      "xvldi $xr11, 33\n\t"
      "xvsrli.h $xr11, $xr11, 8\n\t"
      "xvslli.h $xr11, $xr11, 7\n\t"
      "1:\n\t"
      "xvld $xr0, %[src], 0\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvmul.h $xr1, $xr1, $xr8\n\t"
      "xvmadd.h $xr1, $xr2, $xr9\n\t"
      "xvmadd.h $xr1, $xr3, $xr10\n\t"
      "xvadd.h $xr1, $xr1, $xr11\n\t"
      "xvpickod.b $xr1, $xr1, $xr1\n\t"
      "xvpermi.d $xr31, $xr1, 0xd8\n\t"
      "vst $vr31, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb1555), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr8", "$xr9", "$xr10", "$xr11", "$xr31");
}

void RGB565ToUVRow_LASX(const uint8_t* src_rgb565,
                        int src_stride_rgb565,
                        uint8_t* dst_u,
                        uint8_t* dst_v,
                        int width) {
  const uint8_t* next_rgb565 = src_rgb565 + src_stride_rgb565;
  asm volatile(
      "xvldi $xr20, 112\n\t"
      "xvsrli.h $xr20, $xr20, 8\n\t"
      "xvldi $xr21, 74\n\t"
      "xvsrli.h $xr21, $xr21, 8\n\t"
      "xvldi $xr22, 38\n\t"
      "xvsrli.h $xr22, $xr22, 8\n\t"
      "xvldi $xr23, 94\n\t"
      "xvsrli.h $xr23, $xr23, 8\n\t"
      "xvldi $xr24, 18\n\t"
      "xvsrli.h $xr24, $xr24, 8\n\t"
      "xvldi $xr25, 128\n\t"
      "xvsrli.h $xr25, $xr25, 8\n\t"
      "xvslli.h $xr25, $xr25, 8\n\t"
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr31, %[s0], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvor.v $xr10, $xr1, $xr1\n\t"
      "xvor.v $xr11, $xr2, $xr2\n\t"
      "xvor.v $xr12, $xr3, $xr3\n\t"
      "vld $vr0, %[s0], 16\n\t"
      "vld $vr31, %[s0], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr10, $xr1, $xr10\n\t"
      "xvpickev.b $xr11, $xr2, $xr11\n\t"
      "xvpickev.b $xr12, $xr3, $xr12\n\t"
      "vld $vr0, %[s1], 0\n\t"
      "vld $vr31, %[s1], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvor.v $xr13, $xr1, $xr1\n\t"
      "xvor.v $xr14, $xr2, $xr2\n\t"
      "xvor.v $xr15, $xr3, $xr3\n\t"
      "vld $vr0, %[s1], 16\n\t"
      "vld $vr31, %[s1], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr7\n\t"
      "xvsrli.h $xr3, $xr0, 11\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 2\n\t"
      "xvsrli.h $xr2, $xr2, 4\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr5, $xr3, $xr15\n\t"
      "xvpickev.b $xr4, $xr2, $xr14\n\t"
      "xvpickev.b $xr3, $xr1, $xr13\n\t"
      "xvor.v $xr0, $xr10, $xr10\n\t"
      "xvor.v $xr1, $xr11, $xr11\n\t"
      "xvor.v $xr2, $xr12, $xr12\n\t"
      "xvhaddw.hu.bu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr3, $xr3, $xr3\n\t"
      "xvadd.h $xr0, $xr0, $xr3\n\t"
      "xvsrari.h $xr0, $xr0, 2\n\t"
      "xvhaddw.hu.bu $xr1, $xr1, $xr1\n\t"
      "xvhaddw.hu.bu $xr4, $xr4, $xr4\n\t"
      "xvadd.h $xr1, $xr1, $xr4\n\t"
      "xvsrari.h $xr1, $xr1, 2\n\t"
      "xvhaddw.hu.bu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.hu.bu $xr5, $xr5, $xr5\n\t"
      "xvadd.h $xr2, $xr2, $xr5\n\t"
      "xvsrari.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr6, $xr25, $xr25\n\t"
      "xvor.v $xr7, $xr25, $xr25\n\t"
      "xvmadd.h $xr6, $xr0, $xr20\n\t"
      "xvmsub.h $xr6, $xr1, $xr21\n\t"
      "xvmsub.h $xr6, $xr2, $xr22\n\t"
      "xvmadd.h $xr7, $xr2, $xr20\n\t"
      "xvmsub.h $xr7, $xr1, $xr23\n\t"
      "xvmsub.h $xr7, $xr0, $xr24\n\t"
      "xvpickod.b $xr6, $xr7, $xr6\n\t"
      "xvpermi.d $xr31, $xr6, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "xvpermi.q $xr31, $xr6, 0x01\n\t"
      "vstelm.d $vr31, %[dv], 8, 1\n\t"
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "xvldi $xr7, 63\n\t"
      "xvsrli.h $xr7, $xr7, 8\n\t"
      "addi.d %[s0], %[s0], 64\n\t"
      "addi.d %[s1], %[s1], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_rgb565), [s1] "+r"(next_rgb565), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14", "$xr15", "$xr20",
        "$xr21", "$xr22", "$xr23", "$xr24", "$xr25", "$xr31");
}

void ARGB1555ToUVRow_LASX(const uint8_t* src_argb1555,
                          int src_stride_argb1555,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width) {
  const uint8_t* next_argb1555 = src_argb1555 + src_stride_argb1555;
  asm volatile(
      "xvldi $xr20, 112\n\t"
      "xvsrli.h $xr20, $xr20, 8\n\t"
      "xvldi $xr21, 74\n\t"
      "xvsrli.h $xr21, $xr21, 8\n\t"
      "xvldi $xr22, 38\n\t"
      "xvsrli.h $xr22, $xr22, 8\n\t"
      "xvldi $xr23, 94\n\t"
      "xvsrli.h $xr23, $xr23, 8\n\t"
      "xvldi $xr24, 18\n\t"
      "xvsrli.h $xr24, $xr24, 8\n\t"
      "xvldi $xr25, 128\n\t"
      "xvsrli.h $xr25, $xr25, 8\n\t"
      "xvslli.h $xr25, $xr25, 8\n\t"
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr31, %[s0], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvor.v $xr10, $xr1, $xr1\n\t"
      "xvor.v $xr11, $xr2, $xr2\n\t"
      "xvor.v $xr12, $xr3, $xr3\n\t"
      "vld $vr0, %[s0], 16\n\t"
      "vld $vr31, %[s0], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr10, $xr1, $xr10\n\t"
      "xvpickev.b $xr11, $xr2, $xr11\n\t"
      "xvpickev.b $xr12, $xr3, $xr12\n\t"
      "vld $vr0, %[s1], 0\n\t"
      "vld $vr31, %[s1], 32\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvor.v $xr13, $xr1, $xr1\n\t"
      "xvor.v $xr14, $xr2, $xr2\n\t"
      "xvor.v $xr15, $xr3, $xr3\n\t"
      "vld $vr0, %[s1], 16\n\t"
      "vld $vr31, %[s1], 48\n\t"
      "xvpermi.q $xr0, $xr31, 0x02\n\t"
      "xvand.v $xr1, $xr0, $xr6\n\t"
      "xvsrli.h $xr2, $xr0, 5\n\t"
      "xvand.v $xr2, $xr2, $xr6\n\t"
      "xvsrli.h $xr3, $xr0, 10\n\t"
      "xvand.v $xr3, $xr3, $xr6\n\t"
      "xvsrai.h $xr5, $xr0, 15\n\t"
      "xvslli.h $xr4, $xr1, 3\n\t"
      "xvsrli.h $xr1, $xr1, 2\n\t"
      "xvor.v $xr1, $xr4, $xr1\n\t"
      "xvslli.h $xr4, $xr2, 3\n\t"
      "xvsrli.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr2, $xr4, $xr2\n\t"
      "xvslli.h $xr4, $xr3, 3\n\t"
      "xvsrli.h $xr3, $xr3, 2\n\t"
      "xvor.v $xr3, $xr4, $xr3\n\t"
      "xvpickev.b $xr5, $xr3, $xr15\n\t"
      "xvpickev.b $xr4, $xr2, $xr14\n\t"
      "xvpickev.b $xr3, $xr1, $xr13\n\t"
      "xvor.v $xr0, $xr10, $xr10\n\t"
      "xvor.v $xr1, $xr11, $xr11\n\t"
      "xvor.v $xr2, $xr12, $xr12\n\t"
      "xvhaddw.hu.bu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr3, $xr3, $xr3\n\t"
      "xvadd.h $xr0, $xr0, $xr3\n\t"
      "xvsrari.h $xr0, $xr0, 2\n\t"
      "xvhaddw.hu.bu $xr1, $xr1, $xr1\n\t"
      "xvhaddw.hu.bu $xr4, $xr4, $xr4\n\t"
      "xvadd.h $xr1, $xr1, $xr4\n\t"
      "xvsrari.h $xr1, $xr1, 2\n\t"
      "xvhaddw.hu.bu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.hu.bu $xr5, $xr5, $xr5\n\t"
      "xvadd.h $xr2, $xr2, $xr5\n\t"
      "xvsrari.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr6, $xr25, $xr25\n\t"
      "xvor.v $xr7, $xr25, $xr25\n\t"
      "xvmadd.h $xr6, $xr0, $xr20\n\t"
      "xvmsub.h $xr6, $xr1, $xr21\n\t"
      "xvmsub.h $xr6, $xr2, $xr22\n\t"
      "xvmadd.h $xr7, $xr2, $xr20\n\t"
      "xvmsub.h $xr7, $xr1, $xr23\n\t"
      "xvmsub.h $xr7, $xr0, $xr24\n\t"
      "xvpickod.b $xr6, $xr7, $xr6\n\t"
      "xvpermi.d $xr31, $xr6, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "xvpermi.q $xr31, $xr6, 0x01\n\t"
      "vstelm.d $vr31, %[dv], 8, 1\n\t"
      "xvldi $xr6, 31\n\t"
      "xvsrli.h $xr6, $xr6, 8\n\t"
      "addi.d %[s0], %[s0], 64\n\t"
      "addi.d %[s1], %[s1], 64\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb1555), [s1] "+r"(next_argb1555), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14", "$xr15", "$xr20",
        "$xr21", "$xr22", "$xr23", "$xr24", "$xr25", "$xr31");
}

static LIBYUV_NOINLINE void XRGBToUVHelper_LASX(const uint8_t* src,
                                                int stride,
                                                uint8_t* dst_u,
                                                uint8_t* dst_v,
                                                const uint8_t* shuf,
                                                int width) {
  const uint8_t* next = src + stride;
  asm volatile(
      "xvldi $xr20, 112\n\t"
      "xvsrli.h $xr20, $xr20, 8\n\t"
      "xvldi $xr21, 74\n\t"
      "xvsrli.h $xr21, $xr21, 8\n\t"
      "xvldi $xr22, 38\n\t"
      "xvsrli.h $xr22, $xr22, 8\n\t"
      "xvldi $xr23, 94\n\t"
      "xvsrli.h $xr23, $xr23, 8\n\t"
      "xvldi $xr24, 18\n\t"
      "xvsrli.h $xr24, $xr24, 8\n\t"
      "xvldi $xr25, 128\n\t"
      "xvsrli.h $xr25, $xr25, 8\n\t"
      "xvslli.h $xr25, $xr25, 8\n\t"
      "vld $vr16, %[shuf], 0\n\t"
      "xvpermi.q $xr16, $xr16, 0x00\n\t"
      "1:\n\t"
      "vld $vr8, %[s0], 0\n\t"
      "vld $vr31, %[s0], 64\n\t"
      "xvpermi.q $xr8, $xr31, 0x02\n\t"
      "vld $vr9, %[s0], 16\n\t"
      "vld $vr31, %[s0], 80\n\t"
      "xvpermi.q $xr9, $xr31, 0x02\n\t"
      "vld $vr10, %[s0], 32\n\t"
      "vld $vr31, %[s0], 96\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "vld $vr11, %[s0], 48\n\t"
      "vld $vr31, %[s0], 112\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvshuf.b $xr8, $xr8, $xr8, $xr16\n\t"
      "xvshuf.b $xr9, $xr9, $xr9, $xr16\n\t"
      "xvshuf.b $xr10, $xr10, $xr10, $xr16\n\t"
      "xvshuf.b $xr11, $xr11, $xr11, $xr16\n\t"
      "xvpickev.b $xr12, $xr9, $xr8\n\t"
      "xvpickod.b $xr13, $xr9, $xr8\n\t"
      "xvpickev.b $xr14, $xr11, $xr10\n\t"
      "xvpickod.b $xr15, $xr11, $xr10\n\t"
      "xvpickev.b $xr0, $xr14, $xr12\n\t"
      "xvpickod.b $xr2, $xr14, $xr12\n\t"
      "xvpickev.b $xr1, $xr15, $xr13\n\t"
      "vld $vr8, %[s1], 0\n\t"
      "vld $vr31, %[s1], 64\n\t"
      "xvpermi.q $xr8, $xr31, 0x02\n\t"
      "vld $vr9, %[s1], 16\n\t"
      "vld $vr31, %[s1], 80\n\t"
      "xvpermi.q $xr9, $xr31, 0x02\n\t"
      "vld $vr10, %[s1], 32\n\t"
      "vld $vr31, %[s1], 96\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "vld $vr11, %[s1], 48\n\t"
      "vld $vr31, %[s1], 112\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvshuf.b $xr8, $xr8, $xr8, $xr16\n\t"
      "xvshuf.b $xr9, $xr9, $xr9, $xr16\n\t"
      "xvshuf.b $xr10, $xr10, $xr10, $xr16\n\t"
      "xvshuf.b $xr11, $xr11, $xr11, $xr16\n\t"
      "xvpickev.b $xr12, $xr9, $xr8\n\t"
      "xvpickod.b $xr13, $xr9, $xr8\n\t"
      "xvpickev.b $xr14, $xr11, $xr10\n\t"
      "xvpickod.b $xr15, $xr11, $xr10\n\t"
      "xvpickev.b $xr3, $xr14, $xr12\n\t"
      "xvpickod.b $xr5, $xr14, $xr12\n\t"
      "xvpickev.b $xr4, $xr15, $xr13\n\t"
      "xvhaddw.hu.bu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr3, $xr3, $xr3\n\t"
      "xvadd.h $xr0, $xr0, $xr3\n\t"
      "xvsrari.h $xr0, $xr0, 2\n\t"
      "xvhaddw.hu.bu $xr1, $xr1, $xr1\n\t"
      "xvhaddw.hu.bu $xr4, $xr4, $xr4\n\t"
      "xvadd.h $xr1, $xr1, $xr4\n\t"
      "xvsrari.h $xr1, $xr1, 2\n\t"
      "xvhaddw.hu.bu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.hu.bu $xr5, $xr5, $xr5\n\t"
      "xvadd.h $xr2, $xr2, $xr5\n\t"
      "xvsrari.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr6, $xr25, $xr25\n\t"
      "xvor.v $xr7, $xr25, $xr25\n\t"
      "xvmadd.h $xr6, $xr0, $xr20\n\t"
      "xvmsub.h $xr6, $xr1, $xr21\n\t"
      "xvmsub.h $xr6, $xr2, $xr22\n\t"
      "xvmadd.h $xr7, $xr2, $xr20\n\t"
      "xvmsub.h $xr7, $xr1, $xr23\n\t"
      "xvmsub.h $xr7, $xr0, $xr24\n\t"
      "xvpickod.b $xr6, $xr7, $xr6\n\t"
      "xvpermi.d $xr31, $xr6, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "xvpermi.q $xr31, $xr6, 0x01\n\t"
      "vstelm.d $vr31, %[dv], 8, 1\n\t"
      "addi.d %[s0], %[s0], 128\n\t"
      "addi.d %[s1], %[s1], 128\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src), [s1] "+r"(next), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15", "$xr16", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24", "$xr25",
        "$xr31");
}

void BGRAToUVRow_LASX(const uint8_t* src_bgra,
                      int src_stride_bgra,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  static const uint8_t kShuf[16] = {3,  2,  1, 0, 7,  6,  5,  4,
                                    11, 10, 9, 8, 15, 14, 13, 12};
  XRGBToUVHelper_LASX(src_bgra, src_stride_bgra, dst_u, dst_v, kShuf, width);
}

void ABGRToUVRow_LASX(const uint8_t* src_abgr,
                      int src_stride_abgr,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  static const uint8_t kShuf[16] = {2,  1, 0, 3,  6,  5,  4,  7,
                                    10, 9, 8, 11, 14, 13, 12, 15};
  XRGBToUVHelper_LASX(src_abgr, src_stride_abgr, dst_u, dst_v, kShuf, width);
}

void RGBAToUVRow_LASX(const uint8_t* src_rgba,
                      int src_stride_rgba,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  static const uint8_t kShuf[16] = {1, 2,  3,  0, 5,  6,  7,  4,
                                    9, 10, 11, 8, 13, 14, 15, 12};
  XRGBToUVHelper_LASX(src_rgba, src_stride_rgba, dst_u, dst_v, kShuf, width);
}

static LIBYUV_NOINLINE void RGB24ToUVHelper_LASX(const uint8_t* src,
                                                 int stride,
                                                 uint8_t* dst_u,
                                                 uint8_t* dst_v,
                                                 const uint8_t* shuf,
                                                 int width) {
  const uint8_t* next = src + stride;
  asm volatile(
      "xvldi $xr20, 112\n\t"
      "xvsrli.h $xr20, $xr20, 8\n\t"
      "xvldi $xr21, 74\n\t"
      "xvsrli.h $xr21, $xr21, 8\n\t"
      "xvldi $xr22, 38\n\t"
      "xvsrli.h $xr22, $xr22, 8\n\t"
      "xvldi $xr23, 94\n\t"
      "xvsrli.h $xr23, $xr23, 8\n\t"
      "xvldi $xr24, 18\n\t"
      "xvsrli.h $xr24, $xr24, 8\n\t"
      "xvldi $xr25, 128\n\t"
      "xvsrli.h $xr25, $xr25, 8\n\t"
      "xvslli.h $xr25, $xr25, 8\n\t"
      "vld $vr16, %[shuf], 0\n\t"
      "xvpermi.q $xr16, $xr16, 0x00\n\t"
      "vld $vr17, %[shuf], 16\n\t"
      "xvpermi.q $xr17, $xr17, 0x00\n\t"
      "1:\n\t"
      "vld $vr8, %[s0], 0\n\t"
      "vld $vr31, %[s0], 48\n\t"
      "xvpermi.q $xr8, $xr31, 0x02\n\t"
      "vld $vr9, %[s0], 12\n\t"
      "vld $vr31, %[s0], 60\n\t"
      "xvpermi.q $xr9, $xr31, 0x02\n\t"
      "vld $vr10, %[s0], 24\n\t"
      "vld $vr31, %[s0], 72\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "vld $vr11, %[s0], 32\n\t"
      "vld $vr31, %[s0], 80\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvshuf.b $xr8, $xr8, $xr8, $xr16\n\t"
      "xvshuf.b $xr9, $xr9, $xr9, $xr16\n\t"
      "xvshuf.b $xr10, $xr10, $xr10, $xr16\n\t"
      "xvshuf.b $xr11, $xr11, $xr11, $xr17\n\t"
      "xvpickev.b $xr12, $xr9, $xr8\n\t"
      "xvpickod.b $xr13, $xr9, $xr8\n\t"
      "xvpickev.b $xr14, $xr11, $xr10\n\t"
      "xvpickod.b $xr15, $xr11, $xr10\n\t"
      "xvpickev.b $xr0, $xr14, $xr12\n\t"
      "xvpickod.b $xr2, $xr14, $xr12\n\t"
      "xvpickev.b $xr1, $xr15, $xr13\n\t"
      "vld $vr8, %[s1], 0\n\t"
      "vld $vr31, %[s1], 48\n\t"
      "xvpermi.q $xr8, $xr31, 0x02\n\t"
      "vld $vr9, %[s1], 12\n\t"
      "vld $vr31, %[s1], 60\n\t"
      "xvpermi.q $xr9, $xr31, 0x02\n\t"
      "vld $vr10, %[s1], 24\n\t"
      "vld $vr31, %[s1], 72\n\t"
      "xvpermi.q $xr10, $xr31, 0x02\n\t"
      "vld $vr11, %[s1], 32\n\t"
      "vld $vr31, %[s1], 80\n\t"
      "xvpermi.q $xr11, $xr31, 0x02\n\t"
      "xvshuf.b $xr8, $xr8, $xr8, $xr16\n\t"
      "xvshuf.b $xr9, $xr9, $xr9, $xr16\n\t"
      "xvshuf.b $xr10, $xr10, $xr10, $xr16\n\t"
      "xvshuf.b $xr11, $xr11, $xr11, $xr17\n\t"
      "xvpickev.b $xr12, $xr9, $xr8\n\t"
      "xvpickod.b $xr13, $xr9, $xr8\n\t"
      "xvpickev.b $xr14, $xr11, $xr10\n\t"
      "xvpickod.b $xr15, $xr11, $xr10\n\t"
      "xvpickev.b $xr3, $xr14, $xr12\n\t"
      "xvpickod.b $xr5, $xr14, $xr12\n\t"
      "xvpickev.b $xr4, $xr15, $xr13\n\t"
      "xvhaddw.hu.bu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr3, $xr3, $xr3\n\t"
      "xvadd.h $xr0, $xr0, $xr3\n\t"
      "xvsrari.h $xr0, $xr0, 2\n\t"
      "xvhaddw.hu.bu $xr1, $xr1, $xr1\n\t"
      "xvhaddw.hu.bu $xr4, $xr4, $xr4\n\t"
      "xvadd.h $xr1, $xr1, $xr4\n\t"
      "xvsrari.h $xr1, $xr1, 2\n\t"
      "xvhaddw.hu.bu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.hu.bu $xr5, $xr5, $xr5\n\t"
      "xvadd.h $xr2, $xr2, $xr5\n\t"
      "xvsrari.h $xr2, $xr2, 2\n\t"
      "xvor.v $xr6, $xr25, $xr25\n\t"
      "xvor.v $xr7, $xr25, $xr25\n\t"
      "xvmadd.h $xr6, $xr0, $xr20\n\t"
      "xvmsub.h $xr6, $xr1, $xr21\n\t"
      "xvmsub.h $xr6, $xr2, $xr22\n\t"
      "xvmadd.h $xr7, $xr2, $xr20\n\t"
      "xvmsub.h $xr7, $xr1, $xr23\n\t"
      "xvmsub.h $xr7, $xr0, $xr24\n\t"
      "xvpickod.b $xr6, $xr7, $xr6\n\t"
      "xvpermi.d $xr31, $xr6, 0xd8\n\t"
      "vst $vr31, %[du], 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "xvpermi.q $xr31, $xr6, 0x01\n\t"
      "vstelm.d $vr31, %[dv], 8, 1\n\t"
      "addi.d %[s0], %[s0], 96\n\t"
      "addi.d %[s1], %[s1], 96\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src), [s1] "+r"(next), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15", "$xr16", "$xr17", "$xr20", "$xr21", "$xr22", "$xr23", "$xr24",
        "$xr25", "$xr31");
}

void RGB24ToUVRow_LASX(const uint8_t* src_rgb24,
                       int src_stride_rgb24,
                       uint8_t* dst_u,
                       uint8_t* dst_v,
                       int width) {
  static const uint8_t kShuf[32] = {0, 1, 2,  0,  3,  4, 5,  0,  6,  7, 8,
                                    0, 9, 10, 11, 0,  4, 5,  6,  0,  7, 8,
                                    9, 0, 10, 11, 12, 0, 13, 14, 15, 0};
  RGB24ToUVHelper_LASX(src_rgb24, src_stride_rgb24, dst_u, dst_v, kShuf, width);
}

void RAWToUVRow_LASX(const uint8_t* src_raw,
                     int src_stride_raw,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  static const uint8_t kShuf[32] = {2, 1,  0,  0,  5,  4, 3,  0,  8,  7, 6,
                                    0, 11, 10, 9,  0,  6, 5,  4,  0,  9, 8,
                                    7, 0,  12, 11, 10, 0, 15, 14, 13, 0};
  RGB24ToUVHelper_LASX(src_raw, src_stride_raw, dst_u, dst_v, kShuf, width);
}

#ifdef HAS_SSIM4X4ROW_LASX
void Ssim4x4Row_LASX(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b,
                     uint32_t* dst,
                     int width) {
  const uint8_t* pa;
  const uint8_t* pb;
  int r;
  asm volatile(
      "1:\n\t"
      "move %[pa], %[sa]\n\t"
      "move %[pb], %[sb]\n\t"
      "li.d %[r], 4\n\t"
      "xvxor.v $xr8, $xr8, $xr8\n\t"
      "xvxor.v $xr9, $xr9, $xr9\n\t"
      "xvxor.v $xr10, $xr10, $xr10\n\t"
      "xvxor.v $xr11, $xr11, $xr11\n\t"
      "2:\n\t"
      "xvld $xr0, %[pa], 0\n\t"
      "xvld $xr1, %[pb], 0\n\t"
      "add.d %[pa], %[pa], %[sta]\n\t"
      "add.d %[pb], %[pb], %[stb]\n\t"
      "xvhaddw.hu.bu $xr2, $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr3, $xr1, $xr1\n\t"
      "xvadd.h $xr8, $xr8, $xr2\n\t"
      "xvadd.h $xr9, $xr9, $xr3\n\t"
      "xvmulwev.h.bu $xr2, $xr0, $xr1\n\t"
      "xvmulwod.h.bu $xr3, $xr0, $xr1\n\t"
      "xvhaddw.wu.hu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.wu.hu $xr3, $xr3, $xr3\n\t"
      "xvadd.w $xr11, $xr11, $xr2\n\t"
      "xvadd.w $xr11, $xr11, $xr3\n\t"
      "xvmulwev.h.bu $xr2, $xr0, $xr0\n\t"
      "xvmulwod.h.bu $xr3, $xr0, $xr0\n\t"
      "xvmulwev.h.bu $xr4, $xr1, $xr1\n\t"
      "xvmulwod.h.bu $xr5, $xr1, $xr1\n\t"
      "xvhaddw.wu.hu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.wu.hu $xr3, $xr3, $xr3\n\t"
      "xvhaddw.wu.hu $xr4, $xr4, $xr4\n\t"
      "xvhaddw.wu.hu $xr5, $xr5, $xr5\n\t"
      "xvadd.w $xr2, $xr2, $xr3\n\t"
      "xvadd.w $xr4, $xr4, $xr5\n\t"
      "xvadd.w $xr10, $xr10, $xr2\n\t"
      "xvadd.w $xr10, $xr10, $xr4\n\t"
      "addi.d %[r], %[r], -1\n\t"
      "bnez %[r], 2b\n\t"
      "xvhaddw.wu.hu $xr8, $xr8, $xr8\n\t"
      "xvhaddw.wu.hu $xr9, $xr9, $xr9\n\t"
      "xvilvl.w $xr0, $xr9, $xr8\n\t"
      "xvilvh.w $xr1, $xr9, $xr8\n\t"
      "xvilvl.w $xr2, $xr11, $xr10\n\t"
      "xvilvh.w $xr3, $xr11, $xr10\n\t"
      "xvilvl.d $xr4, $xr2, $xr0\n\t"
      "xvilvh.d $xr5, $xr2, $xr0\n\t"
      "xvilvl.d $xr6, $xr3, $xr1\n\t"
      "xvilvh.d $xr7, $xr3, $xr1\n\t"
      "xvor.v $xr0, $xr5, $xr5\n\t"
      "xvpermi.q $xr0, $xr4, 0x20\n\t"
      "xvpermi.q $xr5, $xr4, 0x31\n\t"
      "xvor.v $xr1, $xr7, $xr7\n\t"
      "xvpermi.q $xr1, $xr6, 0x20\n\t"
      "xvpermi.q $xr7, $xr6, 0x31\n\t"
      "xvst $xr0, %[dst], 0\n\t"
      "xvst $xr1, %[dst], 32\n\t"
      "xvst $xr5, %[dst], 64\n\t"
      "xvst $xr7, %[dst], 96\n\t"
      "addi.d %[sa], %[sa], 32\n\t"
      "addi.d %[sb], %[sb], 32\n\t"
      "addi.d %[dst], %[dst], 128\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [dst] "+r"(dst), [cnt] "+r"(width),
        [pa] "=&r"(pa), [pb] "=&r"(pb), [r] "=&r"(r)
      : [sta] "r"((ptrdiff_t)stride_a), [stb] "r"((ptrdiff_t)stride_b)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11");
}
#endif  // HAS_SSIM4X4ROW_LASX

#ifdef HAS_SSIMWINDOWROW_LASX
float SsimWindowRow_LASX(const uint32_t* row0,
                         const uint32_t* row1,
                         int num_windows) {
  const uint32_t* row0_next = row0 + 4;
  const uint32_t* row1_next = row1 + 4;
  float total;
  uint32_t c1 = 26634;
  uint32_t c2 = 239708;
  uint32_t c0 = 0;
  asm volatile(
      "xvreplgr2vr.w $xr6, %[c0]\n\t"
      "xvreplgr2vr.w $xr4, %[c1]\n\t"
      "xvreplgr2vr.w $xr5, %[c2]\n\t"
      "addi.d %[cnt], %[cnt], -2\n\t"
      "bltz %[cnt], 2f\n\t"
      "1:\n\t"
      "xvld $xr0, %[r0], 0\n\t"
      "xvld $xr1, %[r0n], 0\n\t"
      "xvld $xr2, %[r1], 0\n\t"
      "xvld $xr3, %[r1n], 0\n\t"
      "xvadd.w $xr0, $xr0, $xr1\n\t"
      "xvadd.w $xr2, $xr2, $xr3\n\t"
      "xvadd.w $xr0, $xr0, $xr2\n\t"
      "xvshuf4i.w $xr1, $xr0, 0x00\n\t"
      "xvshuf4i.w $xr2, $xr0, 0x55\n\t"
      "xvshuf4i.w $xr3, $xr0, 0xaa\n\t"
      "xvshuf4i.w $xr0, $xr0, 0xff\n\t"
      "xvmul.w $xr7, $xr1, $xr2\n\t"
      "xvslli.w $xr7, $xr7, 1\n\t"
      "xvmul.w $xr1, $xr1, $xr1\n\t"
      "xvmadd.w $xr1, $xr2, $xr2\n\t"
      "xvslli.w $xr0, $xr0, 7\n\t"
      "xvsub.w $xr0, $xr0, $xr7\n\t"
      "xvadd.w $xr7, $xr7, $xr4\n\t"
      "xvadd.w $xr0, $xr0, $xr5\n\t"
      "xvslli.w $xr3, $xr3, 6\n\t"
      "xvsub.w $xr3, $xr3, $xr1\n\t"
      "xvadd.w $xr1, $xr1, $xr4\n\t"
      "xvadd.w $xr3, $xr3, $xr5\n\t"
      "xvffint.s.w $xr7, $xr7\n\t"
      "xvffint.s.w $xr0, $xr0\n\t"
      "xvffint.s.w $xr1, $xr1\n\t"
      "xvffint.s.w $xr3, $xr3\n\t"
      "xvfmul.s $xr7, $xr7, $xr0\n\t"
      "xvfmul.s $xr1, $xr1, $xr3\n\t"
      "xvfdiv.s $xr7, $xr7, $xr1\n\t"
      "xvfadd.s $xr6, $xr6, $xr7\n\t"
      "addi.d %[r0], %[r0], 32\n\t"
      "addi.d %[r0n], %[r0n], 32\n\t"
      "addi.d %[r1], %[r1], 32\n\t"
      "addi.d %[r1n], %[r1n], 32\n\t"
      "addi.d %[cnt], %[cnt], -2\n\t"
      "bgez %[cnt], 1b\n\t"
      "xvpermi.q $xr0, $xr6, 0x01\n\t"
      "vfadd.s $vr6, $vr6, $vr0\n\t"
      "2:\n\t"
      "addi.d %[cnt], %[cnt], 2\n\t"
      "beqz %[cnt], 3f\n\t"
      "vld $vr0, %[r0], 0\n\t"
      "vld $vr1, %[r0n], 0\n\t"
      "vld $vr2, %[r1], 0\n\t"
      "vld $vr3, %[r1n], 0\n\t"
      "vadd.w $vr0, $vr0, $vr1\n\t"
      "vadd.w $vr2, $vr2, $vr3\n\t"
      "vadd.w $vr0, $vr0, $vr2\n\t"
      "vshuf4i.w $vr1, $vr0, 0x00\n\t"
      "vshuf4i.w $vr2, $vr0, 0x55\n\t"
      "vshuf4i.w $vr3, $vr0, 0xaa\n\t"
      "vshuf4i.w $vr0, $vr0, 0xff\n\t"
      "vmul.w $vr7, $vr1, $vr2\n\t"
      "vslli.w $vr7, $vr7, 1\n\t"
      "vmul.w $vr1, $vr1, $vr1\n\t"
      "vmadd.w $vr1, $vr2, $vr2\n\t"
      "vslli.w $vr0, $vr0, 7\n\t"
      "vsub.w $vr0, $vr0, $vr7\n\t"
      "vadd.w $vr7, $vr7, $vr4\n\t"
      "vadd.w $vr0, $vr0, $vr5\n\t"
      "vslli.w $vr3, $vr3, 6\n\t"
      "vsub.w $vr3, $vr3, $vr1\n\t"
      "vadd.w $vr1, $vr1, $vr4\n\t"
      "vadd.w $vr3, $vr3, $vr5\n\t"
      "vffint.s.w $vr7, $vr7\n\t"
      "vffint.s.w $vr0, $vr0\n\t"
      "vffint.s.w $vr1, $vr1\n\t"
      "vffint.s.w $vr3, $vr3\n\t"
      "vfmul.s $vr7, $vr7, $vr0\n\t"
      "vfmul.s $vr1, $vr1, $vr3\n\t"
      "vfdiv.s $vr7, $vr7, $vr1\n\t"
      "vfadd.s $vr6, $vr6, $vr7\n\t"
      "3:\n\t"
      "vstelm.w $vr6, %[tot], 0, 0\n\t"
      : [r0] "+r"(row0), [r0n] "+r"(row0_next), [r1] "+r"(row1),
        [r1n] "+r"(row1_next), [cnt] "+r"(num_windows)
      : [c0] "r"(c0), [c1] "r"(c1), [c2] "r"(c2), [tot] "r"(&total)
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7");
  return total;
}
#endif  // HAS_SSIMWINDOWROW_LASX

#ifdef HAS_ARGBDETECTROW_LASX
uint32_t ARGBDetectRow_LASX(const uint8_t* argb, int width) {
  uint32_t diff = 0;
  uint32_t tmp;
  uint32_t rgb_mask = 0x00ffff00u;
  asm volatile(
      "xvreplgr2vr.w $xr2, %[rgb_mask]\n\t"
      "xvldi $xr3, -1\n\t"
      "addi.d %[w], %[w], -8\n\t"
      "bltz %[w], 2f\n\t"
      "1:\n\t"
      "xvld $xr0, %[argb], 0\n\t"
      "addi.d %[argb], %[argb], 32\n\t"
      "xvor.v $xr0, $xr0, $xr2\n\t"
      "xvseq.b $xr0, $xr0, $xr3\n\t"
      "xvmskltz.b $xr0, $xr0\n\t"
      "xvpickve2gr.wu %[diff], $xr0, 0\n\t"
      "xvpickve2gr.wu %[tmp], $xr0, 4\n\t"
      "bstrins.w %[diff], %[tmp], 31, 16\n\t"
      "nor %[diff], %[diff], $zero\n\t"
      "bnez %[diff], 4f\n\t"
      "addi.d %[w], %[w], -8\n\t"
      "bgez %[w], 1b\n\t"
      "2:\n\t"
      "addi.d %[w], %[w], 8\n\t"
      "beqz %[w], 4f\n\t"
      "3:\n\t"
      "vldrepl.w $vr0, %[argb], 0\n\t"
      "addi.d %[argb], %[argb], 4\n\t"
      "vor.v $vr0, $vr0, $vr2\n\t"
      "vseq.b $vr0, $vr0, $vr3\n\t"
      "vmskltz.b $vr0, $vr0\n\t"
      "vpickve2gr.hu %[diff], $vr0, 0\n\t"
      "nor %[diff], %[diff], $zero\n\t"
      "bstrpick.w %[diff], %[diff], 15, 0\n\t"
      "bnez %[diff], 4f\n\t"
      "addi.d %[w], %[w], -1\n\t"
      "bnez %[w], 3b\n\t"
      "4:\n\t"
      : [argb] "+r"(argb), [w] "+r"(width), [diff] "+r"(diff), [tmp] "=&r"(tmp)
      : [rgb_mask] "r"(rgb_mask)
      : "memory", "$xr0", "$xr2", "$xr3");
  if (diff) {
    return (__builtin_ctz(diff) & 3) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
  }
  return 0;
}
#endif  // HAS_ARGBDETECTROW_LASX

#ifdef HAS_HAMMINGDISTANCE_LASX
uint32_t HammingDistance_LASX(const uint8_t* src_a,
                              const uint8_t* src_b,
                              int count) {
  uint32_t diff;
  asm volatile(
      "xvxor.v $xr4, $xr4, $xr4\n\t"
      "1:\n\t"
      "xvld $xr0, %[sa], 0\n\t"
      "xvld $xr1, %[sb], 0\n\t"
      "xvxor.v $xr0, $xr0, $xr1\n\t"
      "xvpcnt.b $xr0, $xr0\n\t"
      "xvhaddw.hu.bu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.wu.hu $xr0, $xr0, $xr0\n\t"
      "xvadd.w $xr4, $xr4, $xr0\n\t"
      "addi.d %[sa], %[sa], 32\n\t"
      "addi.d %[sb], %[sb], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      "xvhaddw.du.wu $xr4, $xr4, $xr4\n\t"
      "xvhaddw.qu.du $xr4, $xr4, $xr4\n\t"
      "xvpermi.q $xr0, $xr4, 0x01\n\t"
      "vadd.w $vr4, $vr4, $vr0\n\t"
      "vpickve2gr.wu %[diff], $vr4, 0\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [cnt] "+r"(count), [diff] "=r"(diff)
      :
      : "memory", "$xr0", "$xr1", "$xr4");
  return diff;
}
#endif  // HAS_HAMMINGDISTANCE_LASX

#ifdef HAS_SUMSQUAREERROR_LASX
uint32_t SumSquareError_LASX(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  uint32_t sse;
  asm volatile(
      "xvxor.v $xr4, $xr4, $xr4\n\t"
      "1:\n\t"
      "xvld $xr0, %[sa], 0\n\t"
      "xvld $xr1, %[sb], 0\n\t"
      "xvabsd.bu $xr0, $xr0, $xr1\n\t"
      "xvmulwev.h.bu $xr2, $xr0, $xr0\n\t"
      "xvmulwod.h.bu $xr3, $xr0, $xr0\n\t"
      "xvhaddw.wu.hu $xr2, $xr2, $xr2\n\t"
      "xvhaddw.wu.hu $xr3, $xr3, $xr3\n\t"
      "xvadd.w $xr4, $xr4, $xr2\n\t"
      "xvadd.w $xr4, $xr4, $xr3\n\t"
      "addi.d %[sa], %[sa], 32\n\t"
      "addi.d %[sb], %[sb], 32\n\t"
      "addi.d %[cnt], %[cnt], -32\n\t"
      "bnez %[cnt], 1b\n\t"
      "xvhaddw.du.wu $xr4, $xr4, $xr4\n\t"
      "xvhaddw.qu.du $xr4, $xr4, $xr4\n\t"
      "xvpermi.q $xr0, $xr4, 0x01\n\t"
      "vadd.w $vr4, $vr4, $vr0\n\t"
      "vpickve2gr.wu %[sse], $vr4, 0\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [cnt] "+r"(count), [sse] "=r"(sse)
      :
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4");
  return sse;
}
#endif  // HAS_SUMSQUAREERROR_LASX

#ifdef HAS_HASHDJB2_LASX
static const uint32_t kDjb2Muls_LASX[16] = {
    0x0c3525e1, 0xa3476dc1, 0x3b4039a1, 0x4f5f0981, 0x30f35d61, 0x855cb541,
    0x040a9121, 0x747c7101, 0xec41d4e1, 0x4cfa3cc1, 0x025528a1, 0x00121881,
    0x00008c61, 0x00000441, 0x00000021, 0x00000001,
};

uint32_t HashDjb2_LASX(const uint8_t* src, int count, uint32_t seed) {
  uint32_t hash = seed;
  uint32_t tmp;
  uint32_t c16 = 0x92d9e201u;
  asm volatile(
      "xvld $xr4, %[muls], 0\n\t"
      "xvld $xr5, %[muls], 32\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vbsrl.v $vr1, $vr0, 8\n\t"
      "vext2xv.wu.bu $xr0, $xr0\n\t"
      "vext2xv.wu.bu $xr1, $xr1\n\t"
      "xvmul.w $xr0, $xr0, $xr4\n\t"
      "xvmadd.w $xr0, $xr1, $xr5\n\t"
      "xvhaddw.du.wu $xr0, $xr0, $xr0\n\t"
      "xvhaddw.qu.du $xr0, $xr0, $xr0\n\t"
      "xvpermi.q $xr1, $xr0, 0x01\n\t"
      "vadd.w $vr0, $vr0, $vr1\n\t"
      "vpickve2gr.wu %[tmp], $vr0, 0\n\t"
      "mul.w %[hash], %[hash], %[c16]\n\t"
      "add.w %[hash], %[hash], %[tmp]\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [cnt] "+r"(count), [hash] "+r"(hash), [tmp] "=&r"(tmp)
      : [muls] "r"(kDjb2Muls_LASX), [c16] "r"(c16)
      : "memory", "$xr0", "$xr1", "$xr4", "$xr5");
  return hash;
}
#endif  // HAS_HASHDJB2_LASX

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)
