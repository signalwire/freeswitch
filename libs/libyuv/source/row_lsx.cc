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

#if !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void CopyRow_LSX(const uint8_t* src, uint8_t* dst, int count) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [dst] "+r"(dst), [cnt] "+r"(count)
      :
      : "memory", "$vr0");
}

void SetRow_LSX(uint8_t* dst, uint8_t v8, int count) {
  asm volatile(
      "vreplgr2vr.b $vr0, %[val]\n\t"
      "1:\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst), [cnt] "+r"(count)
      : [val] "r"(v8)
      : "memory", "$vr0");
}

void ARGBSetRow_LSX(uint8_t* dst_argb, uint32_t v32, int width) {
  asm volatile(
      "vreplgr2vr.w $vr0, %[val]\n\t"
      "1:\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(v32)
      : "memory", "$vr0");
}

void MirrorRow_LSX(const uint8_t* src_y, uint8_t* dst_y, int width) {
  static const uint8_t kRev[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                   7,  6,  5,  4,  3,  2,  1, 0};
  src_y += width - 16;
  asm volatile(
      "vld $vr1, %[rev], 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_y), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [rev] "r"(kRev)
      : "memory", "$vr0", "$vr1");
}

void MirrorUVRow_LSX(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  static const uint8_t kRevUV[16] = {14, 15, 12, 13, 10, 11, 8, 9,
                                     6,  7,  4,  5,  2,  3,  0, 1};
  src_uv += width * 2 - 16;
  asm volatile(
      "vld $vr1, %[rev], 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uv), [dst] "+r"(dst_uv), [cnt] "+r"(width)
      : [rev] "r"(kRevUV)
      : "memory", "$vr0", "$vr1");
}

void ARGBMirrorRow_LSX(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  src_argb += width * 4 - 16;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vshuf4i.w $vr0, $vr0, 0x1b\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], -16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0");
}

void MirrorSplitUVRow_LSX(const uint8_t* src_uv,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width) {
  static const uint8_t kRevUV[16] = {14, 15, 12, 13, 10, 11, 8, 9,
                                     6,  7,  4,  5,  2,  3,  0, 1};
  src_uv += width * 2 - 32;
  asm volatile(
      "vld $vr4, %[rev], 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 16\n\t"
      "vld $vr1, %[src], 0\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr4\n\t"
      "vshuf.b $vr1, $vr1, $vr1, $vr4\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vst $vr2, %[du], 0\n\t"
      "vst $vr3, %[dv], 0\n\t"
      "addi.d %[src], %[src], -32\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [src] "+r"(src_uv), [du] "+r"(dst_u), [dv] "+r"(dst_v), [cnt] "+r"(width)
      : [rev] "r"(kRevUV)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void SplitUVRow_LSX(const uint8_t* src_uv,
                    uint8_t* dst_u,
                    uint8_t* dst_v,
                    int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vst $vr2, %[du], 0\n\t"
      "vst $vr3, %[dv], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[du], %[du], 16\n\t"
      "addi.d %[dv], %[dv], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [src] "+r"(src_uv), [du] "+r"(dst_u), [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void MergeUVRow_LSX(const uint8_t* src_u,
                    const uint8_t* src_v,
                    uint8_t* dst_uv,
                    int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[su], 0\n\t"
      "vld $vr1, %[sv], 0\n\t"
      "vilvl.b $vr2, $vr1, $vr0\n\t"
      "vilvh.b $vr3, $vr1, $vr0\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "vst $vr3, %[dst], 16\n\t"
      "addi.d %[su], %[su], 16\n\t"
      "addi.d %[sv], %[sv], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      :
      [su] "+r"(src_u), [sv] "+r"(src_v), [dst] "+r"(dst_uv), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void ARGBExtractAlphaRow_LSX(const uint8_t* src_argb,
                             uint8_t* dst_a,
                             int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickod.b $vr0, $vr1, $vr0\n\t"
      "vpickod.b $vr0, $vr0, $vr0\n\t"
      "vstelm.d $vr0, %[dst], 0, 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 8\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_a), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1");
}

void I422ToYUY2Row_LSX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_yuy2,
                       int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr1, %[su], 0\n\t"
      "vldrepl.d $vr2, %[sv], 0\n\t"
      "vilvl.b $vr3, $vr2, $vr1\n\t"
      "vilvl.b $vr4, $vr3, $vr0\n\t"
      "vilvh.b $vr5, $vr3, $vr0\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_yuy2), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5");
}

void I422ToUYVYRow_LSX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_uyvy,
                       int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr1, %[su], 0\n\t"
      "vldrepl.d $vr2, %[sv], 0\n\t"
      "vilvl.b $vr3, $vr2, $vr1\n\t"
      "vilvl.b $vr4, $vr0, $vr3\n\t"
      "vilvh.b $vr5, $vr0, $vr3\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_uyvy), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5");
}

void YUY2ToYRow_LSX(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2");
}

void UYVYToYRow_LSX(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickod.b $vr2, $vr1, $vr0\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2");
}

void YUY2ToUV422Row_LSX(const uint8_t* src_yuy2,
                        uint8_t* dst_u,
                        uint8_t* dst_v,
                        int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickod.b $vr2, $vr1, $vr0\n\t"
      "vpickev.b $vr3, $vr2, $vr2\n\t"
      "vpickod.b $vr4, $vr2, $vr2\n\t"
      "vstelm.d $vr3, %[du], 0, 0\n\t"
      "vstelm.d $vr4, %[dv], 0, 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void UYVYToUV422Row_LSX(const uint8_t* src_uyvy,
                        uint8_t* dst_u,
                        uint8_t* dst_v,
                        int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickev.b $vr3, $vr2, $vr2\n\t"
      "vpickod.b $vr4, $vr2, $vr2\n\t"
      "vstelm.d $vr3, %[du], 0, 0\n\t"
      "vstelm.d $vr4, %[dv], 0, 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void YUY2ToUVRow_LSX(const uint8_t* src_yuy2,
                     int src_stride_yuy2,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  const uint8_t* next_yuy2 = src_yuy2 + src_stride_yuy2;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s0], 16\n\t"
      "vld $vr2, %[s1], 0\n\t"
      "vld $vr3, %[s1], 16\n\t"
      "vpickod.b $vr0, $vr1, $vr0\n\t"
      "vpickod.b $vr2, $vr3, $vr2\n\t"
      "vavgr.bu $vr0, $vr0, $vr2\n\t"
      "vpickev.b $vr1, $vr0, $vr0\n\t"
      "vpickod.b $vr2, $vr0, $vr0\n\t"
      "vstelm.d $vr1, %[du], 0, 0\n\t"
      "vstelm.d $vr2, %[dv], 0, 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_yuy2), [s1] "+r"(next_yuy2), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void UYVYToUVRow_LSX(const uint8_t* src_uyvy,
                     int src_stride_uyvy,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  const uint8_t* next_uyvy = src_uyvy + src_stride_uyvy;
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s0], 16\n\t"
      "vld $vr2, %[s1], 0\n\t"
      "vld $vr3, %[s1], 16\n\t"
      "vpickev.b $vr0, $vr1, $vr0\n\t"
      "vpickev.b $vr2, $vr3, $vr2\n\t"
      "vavgr.bu $vr0, $vr0, $vr2\n\t"
      "vpickev.b $vr1, $vr0, $vr0\n\t"
      "vpickod.b $vr2, $vr0, $vr0\n\t"
      "vstelm.d $vr1, %[du], 0, 0\n\t"
      "vstelm.d $vr2, %[dv], 0, 0\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_uyvy), [s1] "+r"(next_uyvy), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void I422ToARGBRow_LSX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr20", "$vr21", "$vr22",
        "$vr23", "$vr24");
}

void I422ToRGBARow_LSX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr2, $vr5\n\t"
      "vilvl.b $vr7, $vr4, $vr3\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr20", "$vr21", "$vr22",
        "$vr23", "$vr24");
}

void I422AlphaToARGBRow_LSX(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            const uint8_t* src_a,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vldrepl.d $vr5, %[sa], 0\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[sa], %[sa], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width), [sa] "+r"(src_a)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr20", "$vr21", "$vr22",
        "$vr23", "$vr24");
}

void I422ToRGB24Row_LSX(const uint8_t* src_y,
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
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vld $vr12, %[s0], 0\n\t"
      "vld $vr13, %[s1], 0\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr4, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vshuf.b $vr10, $vr9, $vr8, $vr12\n\t"
      "vshuf.b $vr11, $vr9, $vr8, $vr13\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "vstelm.d $vr11, %[dst], 16, 0\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 24\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants), [s0] "r"(kShuf0), [s1] "r"(kShuf1)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr6", "$vr7",
        "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr20", "$vr21",
        "$vr22", "$vr23", "$vr24");
}

void I422ToRGB565Row_LSX(const uint8_t* src_y,
                         const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_argb,
                         const struct YuvConstants* yuvconstants,
                         int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr14, 31\n\t"
      "vsrli.h $vr14, $vr14, 8\n\t"
      "vldi $vr15, 63\n\t"
      "vsrli.h $vr15, $vr15, 8\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vsrli.b $vr3, $vr3, 2\n\t"
      "vsrli.b $vr4, $vr4, 3\n\t"
      "vilvl.b $vr2, $vr2, $vr2\n\t"
      "vilvl.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr4, $vr4, $vr4\n\t"
      "vand.v $vr2, $vr2, $vr14\n\t"
      "vand.v $vr3, $vr3, $vr15\n\t"
      "vand.v $vr4, $vr4, $vr14\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr4, $vr4, 11\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vor.v $vr2, $vr2, $vr4\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr10", "$vr11",
        "$vr14", "$vr15", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void I422ToARGB4444Row_LSX(const uint8_t* src_y,
                           const uint8_t* src_u,
                           const uint8_t* src_v,
                           uint8_t* dst_argb,
                           const struct YuvConstants* yuvconstants,
                           int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr14, 15\n\t"
      "vsrli.h $vr14, $vr14, 8\n\t"
      "vldi $vr15, 240\n\t"
      "vsrli.h $vr15, $vr15, 8\n\t"
      "vslli.h $vr15, $vr15, 8\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vsrli.b $vr2, $vr2, 4\n\t"
      "vsrli.b $vr3, $vr3, 4\n\t"
      "vsrli.b $vr4, $vr4, 4\n\t"
      "vilvl.b $vr2, $vr2, $vr2\n\t"
      "vilvl.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr4, $vr4, $vr4\n\t"
      "vand.v $vr2, $vr2, $vr14\n\t"
      "vand.v $vr3, $vr3, $vr14\n\t"
      "vand.v $vr4, $vr4, $vr14\n\t"
      "vslli.h $vr3, $vr3, 4\n\t"
      "vslli.h $vr4, $vr4, 8\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vor.v $vr2, $vr2, $vr4\n\t"
      "vor.v $vr2, $vr2, $vr15\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr10", "$vr11",
        "$vr14", "$vr15", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void I422ToARGB1555Row_LSX(const uint8_t* src_y,
                           const uint8_t* src_u,
                           const uint8_t* src_v,
                           uint8_t* dst_argb,
                           const struct YuvConstants* yuvconstants,
                           int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr14, 31\n\t"
      "vsrli.h $vr14, $vr14, 8\n\t"
      "vldi $vr15, 128\n\t"
      "vsrli.h $vr15, $vr15, 8\n\t"
      "vslli.h $vr15, $vr15, 8\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.w $vr10, %[su], 0\n\t"
      "vldrepl.w $vr11, %[sv], 0\n\t"
      "vilvl.b $vr10, $vr10, $vr10\n\t"
      "vilvl.b $vr11, $vr11, $vr11\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vsrli.b $vr3, $vr3, 3\n\t"
      "vsrli.b $vr4, $vr4, 3\n\t"
      "vilvl.b $vr2, $vr2, $vr2\n\t"
      "vilvl.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr4, $vr4, $vr4\n\t"
      "vand.v $vr2, $vr2, $vr14\n\t"
      "vand.v $vr3, $vr3, $vr14\n\t"
      "vand.v $vr4, $vr4, $vr14\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr4, $vr4, 10\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vor.v $vr2, $vr2, $vr4\n\t"
      "vor.v $vr2, $vr2, $vr15\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 4\n\t"
      "addi.d %[sv], %[sv], 4\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr10", "$vr11",
        "$vr14", "$vr15", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void NV12ToARGBRow_LSX(const uint8_t* src_y,
                       const uint8_t* src_uv,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr1, %[suv], 0\n\t"
      "vilvl.h $vr1, $vr1, $vr1\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[suv], %[suv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_uv), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void NV21ToARGBRow_LSX(const uint8_t* src_y,
                       const uint8_t* src_vu,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr1, %[suv], 0\n\t"
      "vshuf4i.b $vr1, $vr1, 0xb1\n\t"
      "vilvl.h $vr1, $vr1, $vr1\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[suv], %[suv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_vu), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void NV12ToRGB565Row_LSX(const uint8_t* src_y,
                         const uint8_t* src_uv,
                         uint8_t* dst_argb,
                         const struct YuvConstants* yuvconstants,
                         int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr14, 31\n\t"
      "vsrli.h $vr14, $vr14, 8\n\t"
      "vldi $vr15, 63\n\t"
      "vsrli.h $vr15, $vr15, 8\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr1, %[suv], 0\n\t"
      "vilvl.h $vr1, $vr1, $vr1\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vsrli.b $vr3, $vr3, 2\n\t"
      "vsrli.b $vr4, $vr4, 3\n\t"
      "vilvl.b $vr2, $vr2, $vr2\n\t"
      "vilvl.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr4, $vr4, $vr4\n\t"
      "vand.v $vr2, $vr2, $vr14\n\t"
      "vand.v $vr3, $vr3, $vr15\n\t"
      "vand.v $vr4, $vr4, $vr14\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr4, $vr4, 11\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vor.v $vr2, $vr2, $vr4\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[suv], %[suv], 8\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [suv] "+r"(src_uv), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr14", "$vr15",
        "$vr20", "$vr21", "$vr22", "$vr23", "$vr24");
}

void I444ToARGBRow_LSX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vldrepl.d $vr10, %[su], 0\n\t"
      "vldrepl.d $vr11, %[sv], 0\n\t"
      "vilvl.b $vr1, $vr11, $vr10\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[su], %[su], 8\n\t"
      "addi.d %[sv], %[sv], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [su] "+r"(src_u), [sv] "+r"(src_v),
        [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr20", "$vr21", "$vr22",
        "$vr23", "$vr24");
}

void I400ToARGBRow_LSX(const uint8_t* src_y,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vld $vr23, %[c], 96\n\t"
      "vld $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sy], 0\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vssrani.bu.h $vr2, $vr0, 6\n\t"
      "vilvl.b $vr6, $vr2, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr2\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr2", "$vr5", "$vr6", "$vr7", "$vr8", "$vr9",
        "$vr23", "$vr24");
}

void J400ToARGBRow_LSX(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  asm volatile(
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr2, %[sy], 0\n\t"
      "vilvl.b $vr6, $vr2, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr2\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sy] "+r"(src_y), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr2", "$vr5", "$vr6", "$vr7", "$vr8", "$vr9");
}

void YUY2ToARGBRow_LSX(const uint8_t* src_yuy2,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr10, %[src], 0\n\t"
      "vpickev.b $vr0, $vr10, $vr10\n\t"
      "vpickod.b $vr1, $vr10, $vr10\n\t"
      "vilvl.h $vr1, $vr1, $vr1\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_yuy2), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr20", "$vr21", "$vr22", "$vr23",
        "$vr24");
}

void UYVYToARGBRow_LSX(const uint8_t* src_uyvy,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  asm volatile(
      "vldrepl.w $vr20, %[c], 0\n\t"
      "vldrepl.w $vr21, %[c], 32\n\t"
      "vldrepl.w $vr22, %[c], 64\n\t"
      "vldrepl.h $vr23, %[c], 96\n\t"
      "vldrepl.h $vr24, %[c], 128\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr10, %[src], 0\n\t"
      "vpickod.b $vr0, $vr10, $vr10\n\t"
      "vpickev.b $vr1, $vr10, $vr10\n\t"
      "vilvl.h $vr1, $vr1, $vr1\n\t"
      "vilvl.b $vr0, $vr0, $vr0\n\t"
      "vxori.b $vr1, $vr1, 0x80\n\t"
      "vmuh.hu $vr0, $vr0, $vr23\n\t"
      "vadd.h $vr0, $vr0, $vr24\n\t"
      "vmulwev.h.bu.b $vr2, $vr20, $vr1\n\t"
      "vmulwod.h.bu.b $vr4, $vr22, $vr1\n\t"
      "vmulwev.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vmaddwod.h.bu.b $vr3, $vr21, $vr1\n\t"
      "vsadd.h $vr2, $vr0, $vr2\n\t"
      "vsadd.h $vr4, $vr0, $vr4\n\t"
      "vssub.h $vr3, $vr0, $vr3\n\t"
      "vssrani.bu.h $vr2, $vr2, 6\n\t"
      "vssrani.bu.h $vr3, $vr3, 6\n\t"
      "vssrani.bu.h $vr4, $vr4, 6\n\t"
      "vilvl.b $vr6, $vr3, $vr2\n\t"
      "vilvl.b $vr7, $vr5, $vr4\n\t"
      "vilvl.h $vr8, $vr7, $vr6\n\t"
      "vilvh.h $vr9, $vr7, $vr6\n\t"
      "vst $vr8, %[dst], 0\n\t"
      "vst $vr9, %[dst], 16\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_uyvy), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [c] "r"(yuvconstants)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr20", "$vr21", "$vr22", "$vr23",
        "$vr24");
}

static LIBYUV_NOINLINE void ARGBTo24Helper_LSX(const uint8_t* src_argb,
                                               uint8_t* dst_24,
                                               const uint8_t* shuf,
                                               int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vld $vr2, %[src], 32\n\t"
      "vld $vr3, %[src], 48\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr4\n\t"
      "vshuf.b $vr1, $vr1, $vr1, $vr4\n\t"
      "vshuf.b $vr2, $vr2, $vr2, $vr4\n\t"
      "vshuf.b $vr3, $vr3, $vr3, $vr4\n\t"
      "vstelm.d $vr0, %[dst], 0, 0\n\t"
      "vstelm.w $vr0, %[dst], 8, 2\n\t"
      "vstelm.w $vr1, %[dst], 12, 0\n\t"
      "vstelm.w $vr1, %[dst], 16, 1\n\t"
      "vstelm.w $vr1, %[dst], 20, 2\n\t"
      "vstelm.d $vr2, %[dst], 24, 0\n\t"
      "vstelm.w $vr2, %[dst], 32, 2\n\t"
      "vstelm.w $vr3, %[dst], 36, 0\n\t"
      "vstelm.w $vr3, %[dst], 40, 1\n\t"
      "vstelm.w $vr3, %[dst], 44, 2\n\t"
      "addi.d %[src], %[src], 64\n\t"
      "addi.d %[dst], %[dst], 48\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_24), [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void ARGBToRGB24Row_LSX(const uint8_t* src_argb,
                        uint8_t* dst_rgb24,
                        int width) {
  static const uint8_t kShuf[16] = {0,  1,  2,  4,  5, 6, 8, 9,
                                    10, 12, 13, 14, 0, 0, 0, 0};
  ARGBTo24Helper_LSX(src_argb, dst_rgb24, kShuf, width);
}

void ARGBToRAWRow_LSX(const uint8_t* src_argb, uint8_t* dst_raw, int width) {
  static const uint8_t kShuf[16] = {2, 1,  0,  6,  5, 4, 10, 9,
                                    8, 14, 13, 12, 0, 0, 0,  0};
  ARGBTo24Helper_LSX(src_argb, dst_raw, kShuf, width);
}

static LIBYUV_NOINLINE void RGB24ToARGBHelper_LSX(const uint8_t* src_24,
                                                  uint8_t* dst_argb,
                                                  const uint8_t* shuf,
                                                  int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "vldi $vr6, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 12\n\t"
      "vld $vr2, %[src], 24\n\t"
      "vld $vr3, %[src], 32\n\t"
      "vshuf.b $vr0, $vr6, $vr0, $vr4\n\t"
      "vshuf.b $vr1, $vr6, $vr1, $vr4\n\t"
      "vshuf.b $vr2, $vr6, $vr2, $vr4\n\t"
      "vshuf.b $vr3, $vr6, $vr3, $vr5\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "vst $vr2, %[dst], 32\n\t"
      "vst $vr3, %[dst], 48\n\t"
      "addi.d %[src], %[src], 48\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_24), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6");
}

void RGB24ToARGBRow_LSX(const uint8_t* src_rgb24,
                        uint8_t* dst_argb,
                        int width) {
  static const uint8_t kShuf[32] = {0,  1,  2,  16, 3,  4,  5,  16, 6,  7, 8,
                                    16, 9,  10, 11, 16, 4,  5,  6,  16, 7, 8,
                                    9,  16, 10, 11, 12, 16, 13, 14, 15, 16};
  RGB24ToARGBHelper_LSX(src_rgb24, dst_argb, kShuf, width);
}

void RAWToARGBRow_LSX(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  static const uint8_t kShuf[32] = {2,  1,  0,  16, 5,  4,  3,  16, 8,  7, 6,
                                    16, 11, 10, 9,  16, 6,  5,  4,  16, 9, 8,
                                    7,  16, 12, 11, 10, 16, 15, 14, 13, 16};
  RGB24ToARGBHelper_LSX(src_raw, dst_argb, kShuf, width);
}

void RAWToRGB24Row_LSX(const uint8_t* src_raw, uint8_t* dst_rgb24, int width) {
  static const uint8_t kShuf[32] = {2,  1,  0,  5,  4,  3,  8, 7, 6, 11, 10,
                                    9,  0,  0,  0,  0,  6,  5, 4, 9, 8,  7,
                                    12, 11, 10, 15, 14, 13, 0, 0, 0, 0};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 12\n\t"
      "vld $vr2, %[src], 24\n\t"
      "vld $vr3, %[src], 32\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr4\n\t"
      "vshuf.b $vr1, $vr1, $vr1, $vr4\n\t"
      "vshuf.b $vr2, $vr2, $vr2, $vr4\n\t"
      "vshuf.b $vr3, $vr3, $vr3, $vr5\n\t"
      "vstelm.d $vr0, %[dst], 0, 0\n\t"
      "vstelm.w $vr0, %[dst], 8, 2\n\t"
      "vstelm.w $vr1, %[dst], 12, 0\n\t"
      "vstelm.w $vr1, %[dst], 16, 1\n\t"
      "vstelm.w $vr1, %[dst], 20, 2\n\t"
      "vstelm.d $vr2, %[dst], 24, 0\n\t"
      "vstelm.w $vr2, %[dst], 32, 2\n\t"
      "vstelm.w $vr3, %[dst], 36, 0\n\t"
      "vstelm.w $vr3, %[dst], 40, 1\n\t"
      "vstelm.w $vr3, %[dst], 44, 2\n\t"
      "addi.d %[src], %[src], 48\n\t"
      "addi.d %[dst], %[dst], 48\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_raw), [dst] "+r"(dst_rgb24), [cnt] "+r"(width)
      : [shuf] "r"(kShuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5");
}

void ARGBToRGB565Row_LSX(const uint8_t* src_argb, uint8_t* dst_rgb, int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vsrli.b $vr3, $vr3, 2\n\t"
      "vand.v $vr4, $vr2, $vr6\n\t"
      "vsrli.h $vr2, $vr2, 8\n\t"
      "vand.v $vr3, $vr3, $vr7\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr2, $vr2, 11\n\t"
      "vor.v $vr4, $vr4, $vr3\n\t"
      "vor.v $vr4, $vr4, $vr2\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr6", "$vr7");
}

void ARGBToRGB565DitherRow_LSX(const uint8_t* src_argb,
                               uint8_t* dst_rgb,
                               uint32_t dither4,
                               int width) {
  asm volatile(
      "vreplgr2vr.w $vr5, %[dith]\n\t"
      "vilvl.b $vr5, $vr5, $vr5\n\t"
      "vilvl.h $vr5, $vr5, $vr5\n\t"
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vsadd.bu $vr0, $vr0, $vr5\n\t"
      "vpickev.b $vr2, $vr0, $vr0\n\t"
      "vpickod.b $vr3, $vr0, $vr0\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vsrli.b $vr3, $vr3, 2\n\t"
      "vand.v $vr4, $vr2, $vr6\n\t"
      "vsrli.h $vr2, $vr2, 8\n\t"
      "vand.v $vr3, $vr3, $vr7\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr2, $vr2, 11\n\t"
      "vor.v $vr4, $vr4, $vr3\n\t"
      "vor.v $vr4, $vr4, $vr2\n\t"
      "vstelm.d $vr4, %[dst], 0, 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 8\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      : [dith] "r"(dither4)
      : "memory", "$vr0", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6", "$vr7");
}

void ARGBToARGB1555Row_LSX(const uint8_t* src_argb,
                           uint8_t* dst_rgb,
                           int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vsrli.b $vr2, $vr2, 3\n\t"
      "vand.v $vr4, $vr2, $vr6\n\t"
      "vsrli.h $vr2, $vr2, 8\n\t"
      "vsrli.h $vr5, $vr3, 15\n\t"
      "vsrli.b $vr3, $vr3, 3\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vslli.h $vr3, $vr3, 5\n\t"
      "vslli.h $vr2, $vr2, 10\n\t"
      "vslli.h $vr5, $vr5, 15\n\t"
      "vor.v $vr4, $vr4, $vr3\n\t"
      "vor.v $vr4, $vr4, $vr2\n\t"
      "vor.v $vr4, $vr4, $vr5\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6");
}

void ARGBToARGB4444Row_LSX(const uint8_t* src_argb,
                           uint8_t* dst_rgb,
                           int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vsrli.b $vr2, $vr2, 4\n\t"
      "vsrli.b $vr3, $vr3, 4\n\t"
      "vslli.b $vr3, $vr3, 4\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_rgb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void RGB565ToARGBRow_LSX(const uint8_t* src_rgb565,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "vldi $vr5, 0xff\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr1, $vr1, $vr1\n\t"
      "vpickev.b $vr2, $vr2, $vr2\n\t"
      "vpickev.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr8, $vr2, $vr1\n\t"
      "vilvl.b $vr9, $vr5, $vr3\n\t"
      "vilvl.h $vr10, $vr9, $vr8\n\t"
      "vilvh.h $vr11, $vr9, $vr8\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "vst $vr11, %[dst], 16\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgb565), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11");
}

void ARGB1555ToARGBRow_LSX(const uint8_t* src_argb1555,
                           uint8_t* dst_argb,
                           int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr1, $vr1, $vr1\n\t"
      "vpickev.b $vr2, $vr2, $vr2\n\t"
      "vpickev.b $vr3, $vr3, $vr3\n\t"
      "vpickev.b $vr5, $vr5, $vr5\n\t"
      "vilvl.b $vr8, $vr2, $vr1\n\t"
      "vilvl.b $vr9, $vr5, $vr3\n\t"
      "vilvl.h $vr10, $vr9, $vr8\n\t"
      "vilvh.h $vr11, $vr9, $vr8\n\t"
      "vst $vr10, %[dst], 0\n\t"
      "vst $vr11, %[dst], 16\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb1555), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr8", "$vr9", "$vr10", "$vr11");
}

void ARGB4444ToARGBRow_LSX(const uint8_t* src_argb4444,
                           uint8_t* dst_argb,
                           int width) {
  asm volatile(
      "vldi $vr6, 0x0f\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.b $vr2, $vr0, 4\n\t"
      "vslli.b $vr3, $vr1, 4\n\t"
      "vor.v $vr1, $vr1, $vr3\n\t"
      "vslli.b $vr3, $vr2, 4\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vilvl.b $vr4, $vr2, $vr1\n\t"
      "vilvh.b $vr5, $vr2, $vr1\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb4444), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6");
}

void ARGBMultiplyRow_LSX(const uint8_t* src_argb,
                         const uint8_t* src_argb1,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vmulwev.h.bu $vr2, $vr0, $vr1\n\t"
      "vmulwod.h.bu $vr3, $vr0, $vr1\n\t"
      "vsrari.h $vr2, $vr2, 8\n\t"
      "vsrari.h $vr3, $vr3, 8\n\t"
      "vpackev.b $vr2, $vr3, $vr2\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3");
}

void ARGBAddRow_LSX(const uint8_t* src_argb,
                    const uint8_t* src_argb1,
                    uint8_t* dst_argb,
                    int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vsadd.bu $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1");
}

void ARGBSubtractRow_LSX(const uint8_t* src_argb,
                         const uint8_t* src_argb1,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vssub.bu $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1");
}

void ARGBAttenuateRow_LSX(const uint8_t* src_argb,
                          uint8_t* dst_argb,
                          int width) {
  static const uint8_t kAlpha[16] = {3,  3,  3,  16, 7,  7,  7,  16,
                                     11, 11, 11, 16, 15, 15, 15, 16};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "vldi $vr5, 0xff\n\t"
      "vldi $vr6, 255\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vshuf.b $vr1, $vr5, $vr0, $vr4\n\t"
      "vmulwev.h.bu $vr2, $vr0, $vr1\n\t"
      "vmulwod.h.bu $vr3, $vr0, $vr1\n\t"
      "vadd.h $vr2, $vr2, $vr6\n\t"
      "vadd.h $vr3, $vr3, $vr6\n\t"
      "vpackod.b $vr2, $vr3, $vr2\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(kAlpha)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6");
}

void ARGBBlendRow_LSX(const uint8_t* src_argb0,
                      const uint8_t* src_argb1,
                      uint8_t* dst_argb,
                      int width) {
  static const uint8_t kAlpha[16] = {3,  3,  3,  3,  7,  7,  7,  7,
                                     11, 11, 11, 11, 15, 15, 15, 15};
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "vldi $vr5, 0xff\n\t"
      "vslli.w $vr5, $vr5, 24\n\t"
      "vldi $vr6, 1\n\t"
      "vslli.h $vr6, $vr6, 8\n\t"
      "vldi $vr8, 0\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vshuf.b $vr2, $vr0, $vr0, $vr4\n\t"
      "vilvl.b $vr3, $vr8, $vr2\n\t"
      "vilvh.b $vr2, $vr8, $vr2\n\t"
      "vsub.h $vr3, $vr6, $vr3\n\t"
      "vsub.h $vr2, $vr6, $vr2\n\t"
      "vilvl.b $vr7, $vr8, $vr1\n\t"
      "vilvh.b $vr1, $vr8, $vr1\n\t"
      "vmul.h $vr7, $vr7, $vr3\n\t"
      "vmul.h $vr1, $vr1, $vr2\n\t"
      "vsrlni.b.h $vr1, $vr7, 8\n\t"
      "vsadd.bu $vr0, $vr0, $vr1\n\t"
      "vor.v $vr0, $vr0, $vr5\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb0), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [shuf] "r"(kAlpha)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8");
}

void ARGBOrRow_LSX(const uint8_t* src_argb,
                   uint8_t* dst_argb,
                   uint32_t value,
                   int width) {
  asm volatile(
      "vreplgr2vr.w $vr1, %[val]\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vor.v $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(value)
      : "memory", "$vr0", "$vr1");
}

void ARGBAlphaBlendRow_LSX(const uint8_t* src_argb0,
                           const uint8_t* src_argb1,
                           uint8_t* dst_argb,
                           int width) {
  static const uint8_t kShufRatio[16] = {0, 0, 0, 0, 4,  4,  4,  4,
                                         8, 8, 8, 8, 12, 12, 12, 12};
  asm volatile(
      "vld $vr8, %[shuf], 0\n\t"
      "vldi $vr9, 1\n\t"
      "vsrli.h $vr10, $vr9, 8\n\t"
      "vldi $vr11, 0xff\n\t"
      "vsrli.w $vr11, $vr11, 8\n\t"
      "vldi $vr12, 0\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vsrli.w $vr2, $vr0, 24\n\t"
      "vsrli.w $vr3, $vr1, 24\n\t"
      "vmul.h $vr4, $vr3, $vr2\n\t"
      "vadd.h $vr4, $vr4, $vr10\n\t"
      "vmuh.hu $vr4, $vr4, $vr9\n\t"
      "vsub.w $vr4, $vr2, $vr4\n\t"
      "vadd.w $vr4, $vr3, $vr4\n\t"
      "vseq.w $vr5, $vr3, $vr12\n\t"
      "vsrli.w $vr5, $vr5, 24\n\t"
      "vor.v $vr6, $vr2, $vr5\n\t"
      "vor.v $vr7, $vr4, $vr5\n\t"
      "vslli.w $vr5, $vr6, 8\n\t"
      "vsub.w $vr5, $vr5, $vr6\n\t"
      "vffint.s.wu $vr5, $vr5\n\t"
      "vffint.s.wu $vr7, $vr7\n\t"
      "vfdiv.s $vr5, $vr5, $vr7\n\t"
      "vftintrz.wu.s $vr5, $vr5\n\t"
      "vshuf.b $vr5, $vr5, $vr5, $vr8\n\t"
      "vnor.v $vr6, $vr5, $vr5\n\t"
      "vmulwev.h.bu $vr2, $vr1, $vr6\n\t"
      "vmulwod.h.bu $vr3, $vr1, $vr6\n\t"
      "vmulwev.h.bu $vr7, $vr0, $vr5\n\t"
      "vmulwod.h.bu $vr0, $vr0, $vr5\n\t"
      "vadd.h $vr2, $vr2, $vr7\n\t"
      "vadd.h $vr3, $vr3, $vr0\n\t"
      "vadd.h $vr2, $vr2, $vr10\n\t"
      "vadd.h $vr3, $vr3, $vr10\n\t"
      "vmuh.hu $vr2, $vr2, $vr9\n\t"
      "vmuh.hu $vr3, $vr3, $vr9\n\t"
      "vslli.h $vr3, $vr3, 8\n\t"
      "vor.v $vr2, $vr2, $vr3\n\t"
      "vand.v $vr2, $vr2, $vr11\n\t"
      "vslli.w $vr4, $vr4, 24\n\t"
      "vor.v $vr2, $vr2, $vr4\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb0), [s1] "+r"(src_argb1), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      : [shuf] "r"(kShufRatio)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12");
}

void ARGBShadeRow_LSX(const uint8_t* src_argb,
                      uint8_t* dst_argb,
                      int width,
                      uint32_t value) {
  asm volatile(
      "vreplgr2vr.w $vr4, %[val]\n\t"
      "vilvl.b $vr5, $vr4, $vr4\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vilvl.b $vr1, $vr0, $vr0\n\t"
      "vilvh.b $vr2, $vr0, $vr0\n\t"
      "vmuh.hu $vr1, $vr1, $vr5\n\t"
      "vmuh.hu $vr2, $vr2, $vr5\n\t"
      "vsrlni.b.h $vr2, $vr1, 8\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [val] "r"(value)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr4", "$vr5");
}

void ARGBShuffleRow_LSX(const uint8_t* src_argb,
                        uint8_t* dst_argb,
                        const uint8_t* shuffler,
                        int width) {
  asm volatile(
      "vld $vr4, %[shuf], 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vshuf.b $vr0, $vr0, $vr0, $vr4\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [shuf] "r"(shuffler)
      : "memory", "$vr0", "$vr4");
}

void ARGBGrayRow_LSX(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  asm volatile(
      "vldi $vr4, 29\n\t"
      "vldi $vr5, 150\n\t"
      "vldi $vr6, 77\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vld $vr1, %[src], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vmulwev.h.bu $vr7, $vr2, $vr4\n\t"
      "vmaddwod.h.bu $vr7, $vr2, $vr6\n\t"
      "vmaddwev.h.bu $vr7, $vr3, $vr5\n\t"
      "vsrlri.h $vr7, $vr7, 8\n\t"
      "vsrli.h $vr3, $vr3, 8\n\t"
      "vslli.h $vr3, $vr3, 8\n\t"
      "vor.v $vr3, $vr7, $vr3\n\t"
      "vslli.h $vr8, $vr7, 8\n\t"
      "vor.v $vr7, $vr7, $vr8\n\t"
      "vilvl.h $vr0, $vr3, $vr7\n\t"
      "vilvh.h $vr1, $vr3, $vr7\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8");
}

void ARGBSepiaRow_LSX(uint8_t* dst_argb, int width) {
  asm volatile(
      "vldi $vr4, 17\n\t"
      "vldi $vr5, 68\n\t"
      "vldi $vr6, 35\n\t"
      "vldi $vr7, 22\n\t"
      "vldi $vr8, 88\n\t"
      "vldi $vr9, 45\n\t"
      "vldi $vr10, 24\n\t"
      "vldi $vr11, 98\n\t"
      "vldi $vr12, 50\n\t"
      "1:\n\t"
      "vld $vr0, %[dst], 0\n\t"
      "vld $vr1, %[dst], 16\n\t"
      "vpickev.b $vr2, $vr1, $vr0\n\t"
      "vpickod.b $vr3, $vr1, $vr0\n\t"
      "vmulwev.h.bu $vr13, $vr2, $vr4\n\t"
      "vmaddwod.h.bu $vr13, $vr2, $vr6\n\t"
      "vmaddwev.h.bu $vr13, $vr3, $vr5\n\t"
      "vmulwev.h.bu $vr14, $vr2, $vr7\n\t"
      "vmaddwod.h.bu $vr14, $vr2, $vr9\n\t"
      "vmaddwev.h.bu $vr14, $vr3, $vr8\n\t"
      "vmulwev.h.bu $vr15, $vr2, $vr10\n\t"
      "vmaddwod.h.bu $vr15, $vr2, $vr12\n\t"
      "vmaddwev.h.bu $vr15, $vr3, $vr11\n\t"
      "vsrli.h $vr13, $vr13, 7\n\t"
      "vsrli.h $vr14, $vr14, 7\n\t"
      "vsrli.h $vr15, $vr15, 7\n\t"
      "vsat.hu $vr13, $vr13, 7\n\t"
      "vsat.hu $vr14, $vr14, 7\n\t"
      "vsat.hu $vr15, $vr15, 7\n\t"
      "vpickev.b $vr13, $vr13, $vr13\n\t"
      "vpickev.b $vr14, $vr14, $vr14\n\t"
      "vpickev.b $vr15, $vr15, $vr15\n\t"
      "vpickod.b $vr3, $vr3, $vr3\n\t"
      "vilvl.b $vr2, $vr14, $vr13\n\t"
      "vilvl.b $vr3, $vr3, $vr15\n\t"
      "vilvl.h $vr0, $vr3, $vr2\n\t"
      "vilvh.h $vr1, $vr3, $vr2\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15");
}

void ARGBColorMatrixRow_LSX(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            const int8_t* matrix_argb,
                            int width) {
  asm volatile(
      "vldrepl.w $vr4, %[mat], 0\n\t"
      "vldrepl.w $vr5, %[mat], 4\n\t"
      "vldrepl.w $vr6, %[mat], 8\n\t"
      "vldrepl.w $vr7, %[mat], 12\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vmulwev.h.bu.b $vr8, $vr0, $vr4\n\t"
      "vmaddwod.h.bu.b $vr8, $vr0, $vr4\n\t"
      "vmulwev.h.bu.b $vr9, $vr0, $vr5\n\t"
      "vmaddwod.h.bu.b $vr9, $vr0, $vr5\n\t"
      "vmulwev.h.bu.b $vr10, $vr0, $vr6\n\t"
      "vmaddwod.h.bu.b $vr10, $vr0, $vr6\n\t"
      "vmulwev.h.bu.b $vr11, $vr0, $vr7\n\t"
      "vmaddwod.h.bu.b $vr11, $vr0, $vr7\n\t"
      "vhaddw.w.h $vr8, $vr8, $vr8\n\t"
      "vhaddw.w.h $vr9, $vr9, $vr9\n\t"
      "vhaddw.w.h $vr10, $vr10, $vr10\n\t"
      "vhaddw.w.h $vr11, $vr11, $vr11\n\t"
      "vilvl.w $vr12, $vr9, $vr8\n\t"
      "vilvh.w $vr13, $vr9, $vr8\n\t"
      "vilvl.w $vr14, $vr11, $vr10\n\t"
      "vilvh.w $vr15, $vr11, $vr10\n\t"
      "vilvl.d $vr8, $vr14, $vr12\n\t"
      "vilvh.d $vr9, $vr14, $vr12\n\t"
      "vilvl.d $vr10, $vr15, $vr13\n\t"
      "vilvh.d $vr11, $vr15, $vr13\n\t"
      "vssrani.h.w $vr9, $vr8, 6\n\t"
      "vssrani.h.w $vr11, $vr10, 6\n\t"
      "vssrani.bu.h $vr11, $vr9, 0\n\t"
      "vst $vr11, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [mat] "r"(matrix_argb)
      : "memory", "$vr0", "$vr4", "$vr5", "$vr6", "$vr7", "$vr8", "$vr9",
        "$vr10", "$vr11", "$vr12", "$vr13", "$vr14", "$vr15");
}

void ARGBQuantizeRow_LSX(uint8_t* dst_argb,
                         int scale,
                         int interval_size,
                         int interval_offset,
                         int width) {
  asm volatile(
      "vreplgr2vr.h $vr4, %[scl]\n\t"
      "vreplgr2vr.b $vr5, %[isz]\n\t"
      "vreplgr2vr.b $vr6, %[ioff]\n\t"
      "vldi $vr7, 0xff\n\t"
      "vslli.w $vr7, $vr7, 24\n\t"
      "1:\n\t"
      "vld $vr0, %[dst], 0\n\t"
      "vilvl.b $vr1, $vr0, $vr0\n\t"
      "vilvh.b $vr2, $vr0, $vr0\n\t"
      "vsrli.h $vr1, $vr1, 8\n\t"
      "vsrli.h $vr2, $vr2, 8\n\t"
      "vmuh.hu $vr1, $vr1, $vr4\n\t"
      "vmuh.hu $vr2, $vr2, $vr4\n\t"
      "vsrlni.b.h $vr2, $vr1, 0\n\t"
      "vmul.b $vr2, $vr2, $vr5\n\t"
      "vadd.b $vr2, $vr2, $vr6\n\t"
      "vand.v $vr0, $vr0, $vr7\n\t"
      "vandn.v $vr2, $vr7, $vr2\n\t"
      "vor.v $vr2, $vr2, $vr0\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -4\n\t"
      "bnez %[cnt], 1b\n\t"
      : [dst] "+r"(dst_argb), [cnt] "+r"(width)
      : [scl] "r"(scale), [isz] "r"(interval_size), [ioff] "r"(interval_offset)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr4", "$vr5", "$vr6", "$vr7");
}

void SobelRow_LSX(const uint8_t* src_sobelx,
                  const uint8_t* src_sobely,
                  uint8_t* dst_argb,
                  int width) {
  asm volatile(
      "vldi $vr4, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sx], 0\n\t"
      "vldrepl.d $vr1, %[sy], 0\n\t"
      "vsadd.bu $vr0, $vr0, $vr1\n\t"
      "vilvl.b $vr2, $vr0, $vr0\n\t"
      "vilvl.b $vr3, $vr4, $vr0\n\t"
      "vilvl.h $vr0, $vr3, $vr2\n\t"
      "vilvh.h $vr1, $vr3, $vr2\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "vst $vr1, %[dst], 16\n\t"
      "addi.d %[sx], %[sx], 8\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void SobelToPlaneRow_LSX(const uint8_t* src_sobelx,
                         const uint8_t* src_sobely,
                         uint8_t* dst_y,
                         int width) {
  asm volatile(
      "1:\n\t"
      "vld $vr0, %[sx], 0\n\t"
      "vld $vr1, %[sy], 0\n\t"
      "vsadd.bu $vr0, $vr0, $vr1\n\t"
      "vst $vr0, %[dst], 0\n\t"
      "addi.d %[sx], %[sx], 16\n\t"
      "addi.d %[sy], %[sy], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_y),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1");
}

void SobelXYRow_LSX(const uint8_t* src_sobelx,
                    const uint8_t* src_sobely,
                    uint8_t* dst_argb,
                    int width) {
  asm volatile(
      "vldi $vr4, 0xff\n\t"
      "1:\n\t"
      "vldrepl.d $vr0, %[sx], 0\n\t"
      "vldrepl.d $vr1, %[sy], 0\n\t"
      "vsadd.bu $vr2, $vr0, $vr1\n\t"
      "vilvl.b $vr1, $vr2, $vr1\n\t"
      "vilvl.b $vr0, $vr4, $vr0\n\t"
      "vilvl.h $vr2, $vr0, $vr1\n\t"
      "vilvh.h $vr3, $vr0, $vr1\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "vst $vr3, %[dst], 16\n\t"
      "addi.d %[sx], %[sx], 8\n\t"
      "addi.d %[sy], %[sy], 8\n\t"
      "addi.d %[dst], %[dst], 32\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sx] "+r"(src_sobelx), [sy] "+r"(src_sobely), [dst] "+r"(dst_argb),
        [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void InterpolateRow_LSX(uint8_t* dst_ptr,
                        const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        int width,
                        int32_t source_y_fraction) {
  int y1_fraction = source_y_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  if (y1_fraction == 0) {
    CopyRow_LSX(src_ptr, dst_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    asm volatile(
        "1:\n\t"
        "vld $vr0, %[s0], 0\n\t"
        "vld $vr1, %[s1], 0\n\t"
        "vavgr.bu $vr0, $vr0, $vr1\n\t"
        "vst $vr0, %[dst], 0\n\t"
        "addi.d %[s0], %[s0], 16\n\t"
        "addi.d %[s1], %[s1], 16\n\t"
        "addi.d %[dst], %[dst], 16\n\t"
        "addi.d %[cnt], %[cnt], -16\n\t"
        "bnez %[cnt], 1b\n\t"
        : [s0] "+r"(src_ptr), [s1] "+r"(src_ptr1), [dst] "+r"(dst_ptr),
          [cnt] "+r"(width)
        :
        : "memory", "$vr0", "$vr1");
    return;
  }
  uint16_t y_frac = (uint16_t)((256 - y1_fraction) + (y1_fraction << 8));
  asm volatile(
      "vreplgr2vr.h $vr3, %[yf]\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vld $vr1, %[s1], 0\n\t"
      "vilvl.b $vr2, $vr1, $vr0\n\t"
      "vilvh.b $vr4, $vr1, $vr0\n\t"
      "vmulwev.h.bu $vr0, $vr2, $vr3\n\t"
      "vmaddwod.h.bu $vr0, $vr2, $vr3\n\t"
      "vmulwev.h.bu $vr1, $vr4, $vr3\n\t"
      "vmaddwod.h.bu $vr1, $vr4, $vr3\n\t"
      "vsrarni.b.h $vr1, $vr0, 8\n\t"
      "vst $vr1, %[dst], 0\n\t"
      "addi.d %[s0], %[s0], 16\n\t"
      "addi.d %[s1], %[s1], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_ptr), [s1] "+r"(src_ptr1), [dst] "+r"(dst_ptr),
        [cnt] "+r"(width)
      : [yf] "r"(y_frac)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
}

void HalfFloatRow_LSX(const uint16_t* src,
                      uint16_t* dst,
                      float scale,
                      int width) {
  const float bias = 1.9259299444e-34f;  // 2^-112, see HalfFloatRow_C
  asm volatile(
      "vldrepl.w $vr3, %[scale], 0\n\t"
      "vldrepl.w $vr5, %[bias], 0\n\t"
      "vldi $vr4, 0\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vilvl.h $vr1, $vr4, $vr0\n\t"
      "vilvh.h $vr2, $vr4, $vr0\n\t"
      "vffint.s.wu $vr1, $vr1\n\t"
      "vffint.s.wu $vr2, $vr2\n\t"
      "vfmul.s $vr1, $vr1, $vr3\n\t"
      "vfmul.s $vr2, $vr2, $vr3\n\t"
      "vfmul.s $vr1, $vr1, $vr5\n\t"
      "vfmul.s $vr2, $vr2, $vr5\n\t"
      "vsrlni.h.w $vr2, $vr1, 13\n\t"
      "vst $vr2, %[dst], 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [dst] "+r"(dst), [cnt] "+r"(width)
      : [scale] "r"(&scale), [bias] "r"(&bias)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5");
}

void ARGBToYMatrixRow_LSX(const uint8_t* src_argb,
                          uint8_t* dst_y,
                          int width,
                          const struct ArgbConstants* c) {
  asm volatile(
      "vldrepl.b $vr0, %[c], 0\n\t"
      "vldrepl.b $vr1, %[c], 1\n\t"
      "vldrepl.b $vr2, %[c], 2\n\t"
      "vldrepl.b $vr3, %[c], 3\n\t"
      "vldrepl.h $vr4, %[c], 96\n\t"
      "1:\n\t"
      "vld $vr5, %[src], 0\n\t"
      "vld $vr6, %[src], 16\n\t"
      "vor.v $vr9, $vr4, $vr4\n\t"
      "vpickev.b $vr7, $vr6, $vr5\n\t"
      "vpickod.b $vr8, $vr6, $vr5\n\t"
      "vmaddwev.h.bu $vr9, $vr7, $vr0\n\t"
      "vmaddwev.h.bu $vr9, $vr8, $vr1\n\t"
      "vmaddwod.h.bu $vr9, $vr7, $vr2\n\t"
      "vmaddwod.h.bu $vr9, $vr8, $vr3\n\t"
      "vpickod.b $vr9, $vr9, $vr9\n\t"
      "vstelm.d $vr9, %[dst], 0, 0\n\t"
      "addi.d %[src], %[src], 32\n\t"
      "addi.d %[dst], %[dst], 8\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [c] "r"(c)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9");
}

void RGBToYMatrixRow_LSX(const uint8_t* src_rgba,
                         uint8_t* dst_y,
                         int width,
                         const struct ArgbConstants* c) {
  static const uint8_t kShuf[32] = {0, 1, 2,  0,  3,  4, 5,  0,  6,  7, 8,
                                    0, 9, 10, 11, 0,  4, 5,  6,  0,  7, 8,
                                    9, 0, 10, 11, 12, 0, 13, 14, 15, 0};
  asm volatile(
      "vldrepl.b $vr0, %[c], 0\n\t"
      "vldrepl.b $vr1, %[c], 1\n\t"
      "vldrepl.b $vr2, %[c], 2\n\t"
      "vldrepl.h $vr3, %[c], 96\n\t"
      "vld $vr4, %[shuf], 0\n\t"
      "vld $vr5, %[shuf], 16\n\t"
      "1:\n\t"
      "vld $vr6, %[src], 0\n\t"
      "vld $vr7, %[src], 12\n\t"
      "vld $vr8, %[src], 24\n\t"
      "vld $vr9, %[src], 32\n\t"
      "vshuf.b $vr6, $vr6, $vr6, $vr4\n\t"
      "vshuf.b $vr7, $vr7, $vr7, $vr4\n\t"
      "vshuf.b $vr8, $vr8, $vr8, $vr4\n\t"
      "vshuf.b $vr9, $vr9, $vr9, $vr5\n\t"
      "vor.v $vr12, $vr3, $vr3\n\t"
      "vor.v $vr13, $vr3, $vr3\n\t"
      "vpickev.b $vr10, $vr7, $vr6\n\t"
      "vpickod.b $vr11, $vr7, $vr6\n\t"
      "vmaddwev.h.bu $vr12, $vr10, $vr0\n\t"
      "vmaddwev.h.bu $vr12, $vr11, $vr1\n\t"
      "vmaddwod.h.bu $vr12, $vr10, $vr2\n\t"
      "vpickev.b $vr10, $vr9, $vr8\n\t"
      "vpickod.b $vr11, $vr9, $vr8\n\t"
      "vmaddwev.h.bu $vr13, $vr10, $vr0\n\t"
      "vmaddwev.h.bu $vr13, $vr11, $vr1\n\t"
      "vmaddwod.h.bu $vr13, $vr10, $vr2\n\t"
      "vpickod.b $vr12, $vr13, $vr12\n\t"
      "vst $vr12, %[dst], 0\n\t"
      "addi.d %[src], %[src], 48\n\t"
      "addi.d %[dst], %[dst], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgba), [dst] "+r"(dst_y), [cnt] "+r"(width)
      : [c] "r"(c), [shuf] "r"(kShuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13");
}

void RGB565ToYRow_LSX(const uint8_t* src_rgb565, uint8_t* dst_y, int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "vldi $vr8, 25\n\t"
      "vsrli.h $vr8, $vr8, 8\n\t"
      "vldi $vr9, 129\n\t"
      "vsrli.h $vr9, $vr9, 8\n\t"
      "vldi $vr10, 66\n\t"
      "vsrli.h $vr10, $vr10, 8\n\t"
      "vldi $vr11, 33\n\t"
      "vsrli.h $vr11, $vr11, 8\n\t"
      "vslli.h $vr11, $vr11, 7\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vmul.h $vr1, $vr1, $vr8\n\t"
      "vmadd.h $vr1, $vr2, $vr9\n\t"
      "vmadd.h $vr1, $vr3, $vr10\n\t"
      "vadd.h $vr1, $vr1, $vr11\n\t"
      "vpickod.b $vr1, $vr1, $vr1\n\t"
      "vstelm.d $vr1, %[dst], 0, 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 8\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_rgb565), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr6", "$vr7",
        "$vr8", "$vr9", "$vr10", "$vr11");
}

void ARGB1555ToYRow_LSX(const uint8_t* src_argb1555,
                        uint8_t* dst_y,
                        int width) {
  asm volatile(
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr8, 25\n\t"
      "vsrli.h $vr8, $vr8, 8\n\t"
      "vldi $vr9, 129\n\t"
      "vsrli.h $vr9, $vr9, 8\n\t"
      "vldi $vr10, 66\n\t"
      "vsrli.h $vr10, $vr10, 8\n\t"
      "vldi $vr11, 33\n\t"
      "vsrli.h $vr11, $vr11, 8\n\t"
      "vslli.h $vr11, $vr11, 7\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vmul.h $vr1, $vr1, $vr8\n\t"
      "vmadd.h $vr1, $vr2, $vr9\n\t"
      "vmadd.h $vr1, $vr3, $vr10\n\t"
      "vadd.h $vr1, $vr1, $vr11\n\t"
      "vpickod.b $vr1, $vr1, $vr1\n\t"
      "vstelm.d $vr1, %[dst], 0, 0\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[dst], %[dst], 8\n\t"
      "addi.d %[cnt], %[cnt], -8\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src_argb1555), [dst] "+r"(dst_y), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr8", "$vr9", "$vr10", "$vr11");
}

void RGB565ToUVRow_LSX(const uint8_t* src_rgb565,
                       int src_stride_rgb565,
                       uint8_t* dst_u,
                       uint8_t* dst_v,
                       int width) {
  const uint8_t* next_rgb565 = src_rgb565 + src_stride_rgb565;
  asm volatile(
      "vldi $vr20, 112\n\t"
      "vsrli.h $vr20, $vr20, 8\n\t"
      "vldi $vr21, 74\n\t"
      "vsrli.h $vr21, $vr21, 8\n\t"
      "vldi $vr22, 38\n\t"
      "vsrli.h $vr22, $vr22, 8\n\t"
      "vldi $vr23, 94\n\t"
      "vsrli.h $vr23, $vr23, 8\n\t"
      "vldi $vr24, 18\n\t"
      "vsrli.h $vr24, $vr24, 8\n\t"
      "vldi $vr25, 128\n\t"
      "vsrli.h $vr25, $vr25, 8\n\t"
      "vslli.h $vr25, $vr25, 8\n\t"
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vor.v $vr10, $vr1, $vr1\n\t"
      "vor.v $vr11, $vr2, $vr2\n\t"
      "vor.v $vr12, $vr3, $vr3\n\t"
      "vld $vr0, %[s0], 16\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr10, $vr1, $vr10\n\t"
      "vpickev.b $vr11, $vr2, $vr11\n\t"
      "vpickev.b $vr12, $vr3, $vr12\n\t"
      "vld $vr0, %[s1], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vor.v $vr13, $vr1, $vr1\n\t"
      "vor.v $vr14, $vr2, $vr2\n\t"
      "vor.v $vr15, $vr3, $vr3\n\t"
      "vld $vr0, %[s1], 16\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr7\n\t"
      "vsrli.h $vr3, $vr0, 11\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 2\n\t"
      "vsrli.h $vr2, $vr2, 4\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr5, $vr3, $vr15\n\t"
      "vpickev.b $vr4, $vr2, $vr14\n\t"
      "vpickev.b $vr3, $vr1, $vr13\n\t"
      "vor.v $vr0, $vr10, $vr10\n\t"
      "vor.v $vr1, $vr11, $vr11\n\t"
      "vor.v $vr2, $vr12, $vr12\n\t"
      "vhaddw.hu.bu $vr0, $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr3, $vr3, $vr3\n\t"
      "vadd.h $vr0, $vr0, $vr3\n\t"
      "vsrari.h $vr0, $vr0, 2\n\t"
      "vhaddw.hu.bu $vr1, $vr1, $vr1\n\t"
      "vhaddw.hu.bu $vr4, $vr4, $vr4\n\t"
      "vadd.h $vr1, $vr1, $vr4\n\t"
      "vsrari.h $vr1, $vr1, 2\n\t"
      "vhaddw.hu.bu $vr2, $vr2, $vr2\n\t"
      "vhaddw.hu.bu $vr5, $vr5, $vr5\n\t"
      "vadd.h $vr2, $vr2, $vr5\n\t"
      "vsrari.h $vr2, $vr2, 2\n\t"
      "vor.v $vr6, $vr25, $vr25\n\t"
      "vor.v $vr7, $vr25, $vr25\n\t"
      "vmadd.h $vr6, $vr0, $vr20\n\t"
      "vmsub.h $vr6, $vr1, $vr21\n\t"
      "vmsub.h $vr6, $vr2, $vr22\n\t"
      "vmadd.h $vr7, $vr2, $vr20\n\t"
      "vmsub.h $vr7, $vr1, $vr23\n\t"
      "vmsub.h $vr7, $vr0, $vr24\n\t"
      "vpickod.b $vr6, $vr7, $vr6\n\t"
      "vstelm.d $vr6, %[du], 0, 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "vldi $vr7, 63\n\t"
      "vsrli.h $vr7, $vr7, 8\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_rgb565), [s1] "+r"(next_rgb565), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14", "$vr15", "$vr20",
        "$vr21", "$vr22", "$vr23", "$vr24", "$vr25");
}

void ARGB1555ToUVRow_LSX(const uint8_t* src_argb1555,
                         int src_stride_argb1555,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  const uint8_t* next_argb1555 = src_argb1555 + src_stride_argb1555;
  asm volatile(
      "vldi $vr20, 112\n\t"
      "vsrli.h $vr20, $vr20, 8\n\t"
      "vldi $vr21, 74\n\t"
      "vsrli.h $vr21, $vr21, 8\n\t"
      "vldi $vr22, 38\n\t"
      "vsrli.h $vr22, $vr22, 8\n\t"
      "vldi $vr23, 94\n\t"
      "vsrli.h $vr23, $vr23, 8\n\t"
      "vldi $vr24, 18\n\t"
      "vsrli.h $vr24, $vr24, 8\n\t"
      "vldi $vr25, 128\n\t"
      "vsrli.h $vr25, $vr25, 8\n\t"
      "vslli.h $vr25, $vr25, 8\n\t"
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "1:\n\t"
      "vld $vr0, %[s0], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vor.v $vr10, $vr1, $vr1\n\t"
      "vor.v $vr11, $vr2, $vr2\n\t"
      "vor.v $vr12, $vr3, $vr3\n\t"
      "vld $vr0, %[s0], 16\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr10, $vr1, $vr10\n\t"
      "vpickev.b $vr11, $vr2, $vr11\n\t"
      "vpickev.b $vr12, $vr3, $vr12\n\t"
      "vld $vr0, %[s1], 0\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vor.v $vr13, $vr1, $vr1\n\t"
      "vor.v $vr14, $vr2, $vr2\n\t"
      "vor.v $vr15, $vr3, $vr3\n\t"
      "vld $vr0, %[s1], 16\n\t"
      "vand.v $vr1, $vr0, $vr6\n\t"
      "vsrli.h $vr2, $vr0, 5\n\t"
      "vand.v $vr2, $vr2, $vr6\n\t"
      "vsrli.h $vr3, $vr0, 10\n\t"
      "vand.v $vr3, $vr3, $vr6\n\t"
      "vsrai.h $vr5, $vr0, 15\n\t"
      "vslli.h $vr4, $vr1, 3\n\t"
      "vsrli.h $vr1, $vr1, 2\n\t"
      "vor.v $vr1, $vr4, $vr1\n\t"
      "vslli.h $vr4, $vr2, 3\n\t"
      "vsrli.h $vr2, $vr2, 2\n\t"
      "vor.v $vr2, $vr4, $vr2\n\t"
      "vslli.h $vr4, $vr3, 3\n\t"
      "vsrli.h $vr3, $vr3, 2\n\t"
      "vor.v $vr3, $vr4, $vr3\n\t"
      "vpickev.b $vr5, $vr3, $vr15\n\t"
      "vpickev.b $vr4, $vr2, $vr14\n\t"
      "vpickev.b $vr3, $vr1, $vr13\n\t"
      "vor.v $vr0, $vr10, $vr10\n\t"
      "vor.v $vr1, $vr11, $vr11\n\t"
      "vor.v $vr2, $vr12, $vr12\n\t"
      "vhaddw.hu.bu $vr0, $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr3, $vr3, $vr3\n\t"
      "vadd.h $vr0, $vr0, $vr3\n\t"
      "vsrari.h $vr0, $vr0, 2\n\t"
      "vhaddw.hu.bu $vr1, $vr1, $vr1\n\t"
      "vhaddw.hu.bu $vr4, $vr4, $vr4\n\t"
      "vadd.h $vr1, $vr1, $vr4\n\t"
      "vsrari.h $vr1, $vr1, 2\n\t"
      "vhaddw.hu.bu $vr2, $vr2, $vr2\n\t"
      "vhaddw.hu.bu $vr5, $vr5, $vr5\n\t"
      "vadd.h $vr2, $vr2, $vr5\n\t"
      "vsrari.h $vr2, $vr2, 2\n\t"
      "vor.v $vr6, $vr25, $vr25\n\t"
      "vor.v $vr7, $vr25, $vr25\n\t"
      "vmadd.h $vr6, $vr0, $vr20\n\t"
      "vmsub.h $vr6, $vr1, $vr21\n\t"
      "vmsub.h $vr6, $vr2, $vr22\n\t"
      "vmadd.h $vr7, $vr2, $vr20\n\t"
      "vmsub.h $vr7, $vr1, $vr23\n\t"
      "vmsub.h $vr7, $vr0, $vr24\n\t"
      "vpickod.b $vr6, $vr7, $vr6\n\t"
      "vstelm.d $vr6, %[du], 0, 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "vldi $vr6, 31\n\t"
      "vsrli.h $vr6, $vr6, 8\n\t"
      "addi.d %[s0], %[s0], 32\n\t"
      "addi.d %[s1], %[s1], 32\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src_argb1555), [s1] "+r"(next_argb1555), [du] "+r"(dst_u),
        [dv] "+r"(dst_v), [cnt] "+r"(width)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14", "$vr15", "$vr20",
        "$vr21", "$vr22", "$vr23", "$vr24", "$vr25");
}

static LIBYUV_NOINLINE void XRGBToUVHelper_LSX(const uint8_t* src,
                                               int stride,
                                               uint8_t* dst_u,
                                               uint8_t* dst_v,
                                               const uint8_t* shuf,
                                               int width) {
  const uint8_t* next = src + stride;
  asm volatile(
      "vldi $vr20, 112\n\t"
      "vsrli.h $vr20, $vr20, 8\n\t"
      "vldi $vr21, 74\n\t"
      "vsrli.h $vr21, $vr21, 8\n\t"
      "vldi $vr22, 38\n\t"
      "vsrli.h $vr22, $vr22, 8\n\t"
      "vldi $vr23, 94\n\t"
      "vsrli.h $vr23, $vr23, 8\n\t"
      "vldi $vr24, 18\n\t"
      "vsrli.h $vr24, $vr24, 8\n\t"
      "vldi $vr25, 128\n\t"
      "vsrli.h $vr25, $vr25, 8\n\t"
      "vslli.h $vr25, $vr25, 8\n\t"
      "vld $vr16, %[shuf], 0\n\t"
      "1:\n\t"
      "vld $vr8, %[s0], 0\n\t"
      "vld $vr9, %[s0], 16\n\t"
      "vld $vr10, %[s0], 32\n\t"
      "vld $vr11, %[s0], 48\n\t"
      "vshuf.b $vr8, $vr8, $vr8, $vr16\n\t"
      "vshuf.b $vr9, $vr9, $vr9, $vr16\n\t"
      "vshuf.b $vr10, $vr10, $vr10, $vr16\n\t"
      "vshuf.b $vr11, $vr11, $vr11, $vr16\n\t"
      "vpickev.b $vr12, $vr9, $vr8\n\t"
      "vpickod.b $vr13, $vr9, $vr8\n\t"
      "vpickev.b $vr14, $vr11, $vr10\n\t"
      "vpickod.b $vr15, $vr11, $vr10\n\t"
      "vpickev.b $vr0, $vr14, $vr12\n\t"
      "vpickod.b $vr2, $vr14, $vr12\n\t"
      "vpickev.b $vr1, $vr15, $vr13\n\t"
      "vld $vr8, %[s1], 0\n\t"
      "vld $vr9, %[s1], 16\n\t"
      "vld $vr10, %[s1], 32\n\t"
      "vld $vr11, %[s1], 48\n\t"
      "vshuf.b $vr8, $vr8, $vr8, $vr16\n\t"
      "vshuf.b $vr9, $vr9, $vr9, $vr16\n\t"
      "vshuf.b $vr10, $vr10, $vr10, $vr16\n\t"
      "vshuf.b $vr11, $vr11, $vr11, $vr16\n\t"
      "vpickev.b $vr12, $vr9, $vr8\n\t"
      "vpickod.b $vr13, $vr9, $vr8\n\t"
      "vpickev.b $vr14, $vr11, $vr10\n\t"
      "vpickod.b $vr15, $vr11, $vr10\n\t"
      "vpickev.b $vr3, $vr14, $vr12\n\t"
      "vpickod.b $vr5, $vr14, $vr12\n\t"
      "vpickev.b $vr4, $vr15, $vr13\n\t"
      "vhaddw.hu.bu $vr0, $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr3, $vr3, $vr3\n\t"
      "vadd.h $vr0, $vr0, $vr3\n\t"
      "vsrari.h $vr0, $vr0, 2\n\t"
      "vhaddw.hu.bu $vr1, $vr1, $vr1\n\t"
      "vhaddw.hu.bu $vr4, $vr4, $vr4\n\t"
      "vadd.h $vr1, $vr1, $vr4\n\t"
      "vsrari.h $vr1, $vr1, 2\n\t"
      "vhaddw.hu.bu $vr2, $vr2, $vr2\n\t"
      "vhaddw.hu.bu $vr5, $vr5, $vr5\n\t"
      "vadd.h $vr2, $vr2, $vr5\n\t"
      "vsrari.h $vr2, $vr2, 2\n\t"
      "vor.v $vr6, $vr25, $vr25\n\t"
      "vor.v $vr7, $vr25, $vr25\n\t"
      "vmadd.h $vr6, $vr0, $vr20\n\t"
      "vmsub.h $vr6, $vr1, $vr21\n\t"
      "vmsub.h $vr6, $vr2, $vr22\n\t"
      "vmadd.h $vr7, $vr2, $vr20\n\t"
      "vmsub.h $vr7, $vr1, $vr23\n\t"
      "vmsub.h $vr7, $vr0, $vr24\n\t"
      "vpickod.b $vr6, $vr7, $vr6\n\t"
      "vstelm.d $vr6, %[du], 0, 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "addi.d %[s0], %[s0], 64\n\t"
      "addi.d %[s1], %[s1], 64\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src), [s1] "+r"(next), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15", "$vr16", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24", "$vr25");
}

void BGRAToUVRow_LSX(const uint8_t* src_bgra,
                     int src_stride_bgra,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  static const uint8_t kShuf[16] = {3,  2,  1, 0, 7,  6,  5,  4,
                                    11, 10, 9, 8, 15, 14, 13, 12};
  XRGBToUVHelper_LSX(src_bgra, src_stride_bgra, dst_u, dst_v, kShuf, width);
}

void ABGRToUVRow_LSX(const uint8_t* src_abgr,
                     int src_stride_abgr,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  static const uint8_t kShuf[16] = {2,  1, 0, 3,  6,  5,  4,  7,
                                    10, 9, 8, 11, 14, 13, 12, 15};
  XRGBToUVHelper_LSX(src_abgr, src_stride_abgr, dst_u, dst_v, kShuf, width);
}

void RGBAToUVRow_LSX(const uint8_t* src_rgba,
                     int src_stride_rgba,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  static const uint8_t kShuf[16] = {1, 2,  3,  0, 5,  6,  7,  4,
                                    9, 10, 11, 8, 13, 14, 15, 12};
  XRGBToUVHelper_LSX(src_rgba, src_stride_rgba, dst_u, dst_v, kShuf, width);
}

static LIBYUV_NOINLINE void RGB24ToUVHelper_LSX(const uint8_t* src,
                                                int stride,
                                                uint8_t* dst_u,
                                                uint8_t* dst_v,
                                                const uint8_t* shuf,
                                                int width) {
  const uint8_t* next = src + stride;
  asm volatile(
      "vldi $vr20, 112\n\t"
      "vsrli.h $vr20, $vr20, 8\n\t"
      "vldi $vr21, 74\n\t"
      "vsrli.h $vr21, $vr21, 8\n\t"
      "vldi $vr22, 38\n\t"
      "vsrli.h $vr22, $vr22, 8\n\t"
      "vldi $vr23, 94\n\t"
      "vsrli.h $vr23, $vr23, 8\n\t"
      "vldi $vr24, 18\n\t"
      "vsrli.h $vr24, $vr24, 8\n\t"
      "vldi $vr25, 128\n\t"
      "vsrli.h $vr25, $vr25, 8\n\t"
      "vslli.h $vr25, $vr25, 8\n\t"
      "vld $vr16, %[shuf], 0\n\t"
      "vld $vr17, %[shuf], 16\n\t"
      "1:\n\t"
      "vld $vr8, %[s0], 0\n\t"
      "vld $vr9, %[s0], 12\n\t"
      "vld $vr10, %[s0], 24\n\t"
      "vld $vr11, %[s0], 32\n\t"
      "vshuf.b $vr8, $vr8, $vr8, $vr16\n\t"
      "vshuf.b $vr9, $vr9, $vr9, $vr16\n\t"
      "vshuf.b $vr10, $vr10, $vr10, $vr16\n\t"
      "vshuf.b $vr11, $vr11, $vr11, $vr17\n\t"
      "vpickev.b $vr12, $vr9, $vr8\n\t"
      "vpickod.b $vr13, $vr9, $vr8\n\t"
      "vpickev.b $vr14, $vr11, $vr10\n\t"
      "vpickod.b $vr15, $vr11, $vr10\n\t"
      "vpickev.b $vr0, $vr14, $vr12\n\t"
      "vpickod.b $vr2, $vr14, $vr12\n\t"
      "vpickev.b $vr1, $vr15, $vr13\n\t"
      "vld $vr8, %[s1], 0\n\t"
      "vld $vr9, %[s1], 12\n\t"
      "vld $vr10, %[s1], 24\n\t"
      "vld $vr11, %[s1], 32\n\t"
      "vshuf.b $vr8, $vr8, $vr8, $vr16\n\t"
      "vshuf.b $vr9, $vr9, $vr9, $vr16\n\t"
      "vshuf.b $vr10, $vr10, $vr10, $vr16\n\t"
      "vshuf.b $vr11, $vr11, $vr11, $vr17\n\t"
      "vpickev.b $vr12, $vr9, $vr8\n\t"
      "vpickod.b $vr13, $vr9, $vr8\n\t"
      "vpickev.b $vr14, $vr11, $vr10\n\t"
      "vpickod.b $vr15, $vr11, $vr10\n\t"
      "vpickev.b $vr3, $vr14, $vr12\n\t"
      "vpickod.b $vr5, $vr14, $vr12\n\t"
      "vpickev.b $vr4, $vr15, $vr13\n\t"
      "vhaddw.hu.bu $vr0, $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr3, $vr3, $vr3\n\t"
      "vadd.h $vr0, $vr0, $vr3\n\t"
      "vsrari.h $vr0, $vr0, 2\n\t"
      "vhaddw.hu.bu $vr1, $vr1, $vr1\n\t"
      "vhaddw.hu.bu $vr4, $vr4, $vr4\n\t"
      "vadd.h $vr1, $vr1, $vr4\n\t"
      "vsrari.h $vr1, $vr1, 2\n\t"
      "vhaddw.hu.bu $vr2, $vr2, $vr2\n\t"
      "vhaddw.hu.bu $vr5, $vr5, $vr5\n\t"
      "vadd.h $vr2, $vr2, $vr5\n\t"
      "vsrari.h $vr2, $vr2, 2\n\t"
      "vor.v $vr6, $vr25, $vr25\n\t"
      "vor.v $vr7, $vr25, $vr25\n\t"
      "vmadd.h $vr6, $vr0, $vr20\n\t"
      "vmsub.h $vr6, $vr1, $vr21\n\t"
      "vmsub.h $vr6, $vr2, $vr22\n\t"
      "vmadd.h $vr7, $vr2, $vr20\n\t"
      "vmsub.h $vr7, $vr1, $vr23\n\t"
      "vmsub.h $vr7, $vr0, $vr24\n\t"
      "vpickod.b $vr6, $vr7, $vr6\n\t"
      "vstelm.d $vr6, %[du], 0, 0\n\t"
      "vstelm.d $vr6, %[dv], 0, 1\n\t"
      "addi.d %[s0], %[s0], 48\n\t"
      "addi.d %[s1], %[s1], 48\n\t"
      "addi.d %[du], %[du], 8\n\t"
      "addi.d %[dv], %[dv], 8\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [s0] "+r"(src), [s1] "+r"(next), [du] "+r"(dst_u), [dv] "+r"(dst_v),
        [cnt] "+r"(width)
      : [shuf] "r"(shuf)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15", "$vr16", "$vr17", "$vr20", "$vr21", "$vr22", "$vr23", "$vr24",
        "$vr25");
}

void RGB24ToUVRow_LSX(const uint8_t* src_rgb24,
                      int src_stride_rgb24,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  static const uint8_t kShuf[32] = {0, 1, 2,  0,  3,  4, 5,  0,  6,  7, 8,
                                    0, 9, 10, 11, 0,  4, 5,  6,  0,  7, 8,
                                    9, 0, 10, 11, 12, 0, 13, 14, 15, 0};
  RGB24ToUVHelper_LSX(src_rgb24, src_stride_rgb24, dst_u, dst_v, kShuf, width);
}

void RAWToUVRow_LSX(const uint8_t* src_raw,
                    int src_stride_raw,
                    uint8_t* dst_u,
                    uint8_t* dst_v,
                    int width) {
  static const uint8_t kShuf[32] = {2, 1,  0,  0,  5,  4, 3,  0,  8,  7, 6,
                                    0, 11, 10, 9,  0,  6, 5,  4,  0,  9, 8,
                                    7, 0,  12, 11, 10, 0, 15, 14, 13, 0};
  RGB24ToUVHelper_LSX(src_raw, src_stride_raw, dst_u, dst_v, kShuf, width);
}

#ifdef HAS_SSIM4X4ROW_LSX
void Ssim4x4Row_LSX(const uint8_t* src_a,
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
      "vxor.v $vr8, $vr8, $vr8\n\t"
      "vxor.v $vr9, $vr9, $vr9\n\t"
      "vxor.v $vr10, $vr10, $vr10\n\t"
      "vxor.v $vr11, $vr11, $vr11\n\t"
      "2:\n\t"
      "vld $vr0, %[pa], 0\n\t"
      "vld $vr1, %[pb], 0\n\t"
      "add.d %[pa], %[pa], %[sta]\n\t"
      "add.d %[pb], %[pb], %[stb]\n\t"
      "vhaddw.hu.bu $vr2, $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr3, $vr1, $vr1\n\t"
      "vadd.h $vr8, $vr8, $vr2\n\t"
      "vadd.h $vr9, $vr9, $vr3\n\t"
      "vmulwev.h.bu $vr2, $vr0, $vr1\n\t"
      "vmulwod.h.bu $vr3, $vr0, $vr1\n\t"
      "vhaddw.wu.hu $vr2, $vr2, $vr2\n\t"
      "vhaddw.wu.hu $vr3, $vr3, $vr3\n\t"
      "vadd.w $vr11, $vr11, $vr2\n\t"
      "vadd.w $vr11, $vr11, $vr3\n\t"
      "vmulwev.h.bu $vr2, $vr0, $vr0\n\t"
      "vmulwod.h.bu $vr3, $vr0, $vr0\n\t"
      "vmulwev.h.bu $vr4, $vr1, $vr1\n\t"
      "vmulwod.h.bu $vr5, $vr1, $vr1\n\t"
      "vhaddw.wu.hu $vr2, $vr2, $vr2\n\t"
      "vhaddw.wu.hu $vr3, $vr3, $vr3\n\t"
      "vhaddw.wu.hu $vr4, $vr4, $vr4\n\t"
      "vhaddw.wu.hu $vr5, $vr5, $vr5\n\t"
      "vadd.w $vr2, $vr2, $vr3\n\t"
      "vadd.w $vr4, $vr4, $vr5\n\t"
      "vadd.w $vr10, $vr10, $vr2\n\t"
      "vadd.w $vr10, $vr10, $vr4\n\t"
      "addi.d %[r], %[r], -1\n\t"
      "bnez %[r], 2b\n\t"
      "vhaddw.wu.hu $vr8, $vr8, $vr8\n\t"
      "vhaddw.wu.hu $vr9, $vr9, $vr9\n\t"
      "vilvl.w $vr0, $vr9, $vr8\n\t"
      "vilvh.w $vr1, $vr9, $vr8\n\t"
      "vilvl.w $vr2, $vr11, $vr10\n\t"
      "vilvh.w $vr3, $vr11, $vr10\n\t"
      "vilvl.d $vr4, $vr2, $vr0\n\t"
      "vilvh.d $vr5, $vr2, $vr0\n\t"
      "vilvl.d $vr6, $vr3, $vr1\n\t"
      "vilvh.d $vr7, $vr3, $vr1\n\t"
      "vst $vr4, %[dst], 0\n\t"
      "vst $vr5, %[dst], 16\n\t"
      "vst $vr6, %[dst], 32\n\t"
      "vst $vr7, %[dst], 48\n\t"
      "addi.d %[sa], %[sa], 16\n\t"
      "addi.d %[sb], %[sb], 16\n\t"
      "addi.d %[dst], %[dst], 64\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [dst] "+r"(dst), [cnt] "+r"(width),
        [pa] "=&r"(pa), [pb] "=&r"(pb), [r] "=&r"(r)
      : [sta] "r"((ptrdiff_t)stride_a), [stb] "r"((ptrdiff_t)stride_b)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11");
}
#endif  // HAS_SSIM4X4ROW_LSX

#ifdef HAS_SSIMWINDOWROW_LSX
float SsimWindowRow_LSX(const uint32_t* row0,
                        const uint32_t* row1,
                        int num_windows) {
  const uint32_t* row0_next = row0 + 4;
  const uint32_t* row1_next = row1 + 4;
  float total;
  uint32_t c1 = 26634;
  uint32_t c2 = 239708;
  uint32_t c0 = 0;
  asm volatile(
      "vreplgr2vr.w $vr6, %[c0]\n\t"
      "vreplgr2vr.w $vr4, %[c1]\n\t"
      "vreplgr2vr.w $vr5, %[c2]\n\t"
      "1:\n\t"
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
      "addi.d %[r0], %[r0], 16\n\t"
      "addi.d %[r0n], %[r0n], 16\n\t"
      "addi.d %[r1], %[r1], 16\n\t"
      "addi.d %[r1n], %[r1n], 16\n\t"
      "addi.d %[cnt], %[cnt], -1\n\t"
      "bnez %[cnt], 1b\n\t"
      "vstelm.w $vr6, %[tot], 0, 0\n\t"
      : [r0] "+r"(row0), [r0n] "+r"(row0_next), [r1] "+r"(row1),
        [r1n] "+r"(row1_next), [cnt] "+r"(num_windows)
      : [c0] "r"(c0), [c1] "r"(c1), [c2] "r"(c2), [tot] "r"(&total)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7");
  return total;
}
#endif  // HAS_SSIMWINDOWROW_LSX

#ifdef HAS_ARGBDETECTROW_LSX
uint32_t ARGBDetectRow_LSX(const uint8_t* argb, int width) {
  uint32_t diff = 0;
  uint32_t rgb_mask = 0x00ffff00u;
  asm volatile(
      "vreplgr2vr.w $vr2, %[rgb_mask]\n\t"
      "vldi $vr3, -1\n\t"
      "addi.d %[w], %[w], -4\n\t"
      "bltz %[w], 2f\n\t"
      "1:\n\t"
      "vld $vr0, %[argb], 0\n\t"
      "addi.d %[argb], %[argb], 16\n\t"
      "vor.v $vr0, $vr0, $vr2\n\t"
      "vseq.b $vr0, $vr0, $vr3\n\t"
      "vmskltz.b $vr0, $vr0\n\t"
      "vpickve2gr.hu %[diff], $vr0, 0\n\t"
      "nor %[diff], %[diff], $zero\n\t"
      "bstrpick.w %[diff], %[diff], 15, 0\n\t"
      "bnez %[diff], 4f\n\t"
      "addi.d %[w], %[w], -4\n\t"
      "bgez %[w], 1b\n\t"
      "2:\n\t"
      "addi.d %[w], %[w], 4\n\t"
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
      : [argb] "+r"(argb), [w] "+r"(width), [diff] "+r"(diff)
      : [rgb_mask] "r"(rgb_mask)
      : "memory", "$vr0", "$vr2", "$vr3");
  if (diff) {
    return (__builtin_ctz(diff) & 3) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
  }
  return 0;
}
#endif  // HAS_ARGBDETECTROW_LSX

#ifdef HAS_HAMMINGDISTANCE_LSX
uint32_t HammingDistance_LSX(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  uint32_t diff;
  asm volatile(
      "vxor.v $vr4, $vr4, $vr4\n\t"
      "1:\n\t"
      "vld $vr0, %[sa], 0\n\t"
      "vld $vr1, %[sb], 0\n\t"
      "vxor.v $vr0, $vr0, $vr1\n\t"
      "vpcnt.b $vr0, $vr0\n\t"
      "vhaddw.hu.bu $vr0, $vr0, $vr0\n\t"
      "vhaddw.wu.hu $vr0, $vr0, $vr0\n\t"
      "vadd.w $vr4, $vr4, $vr0\n\t"
      "addi.d %[sa], %[sa], 16\n\t"
      "addi.d %[sb], %[sb], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      "vhaddw.du.wu $vr4, $vr4, $vr4\n\t"
      "vhaddw.qu.du $vr4, $vr4, $vr4\n\t"
      "vpickve2gr.wu %[diff], $vr4, 0\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [cnt] "+r"(count), [diff] "=r"(diff)
      :
      : "memory", "$vr0", "$vr1", "$vr4");
  return diff;
}
#endif  // HAS_HAMMINGDISTANCE_LSX

#ifdef HAS_SUMSQUAREERROR_LSX
uint32_t SumSquareError_LSX(const uint8_t* src_a,
                            const uint8_t* src_b,
                            int count) {
  uint32_t sse;
  asm volatile(
      "vxor.v $vr4, $vr4, $vr4\n\t"
      "1:\n\t"
      "vld $vr0, %[sa], 0\n\t"
      "vld $vr1, %[sb], 0\n\t"
      "vabsd.bu $vr0, $vr0, $vr1\n\t"
      "vmulwev.h.bu $vr2, $vr0, $vr0\n\t"
      "vmulwod.h.bu $vr3, $vr0, $vr0\n\t"
      "vhaddw.wu.hu $vr2, $vr2, $vr2\n\t"
      "vhaddw.wu.hu $vr3, $vr3, $vr3\n\t"
      "vadd.w $vr4, $vr4, $vr2\n\t"
      "vadd.w $vr4, $vr4, $vr3\n\t"
      "addi.d %[sa], %[sa], 16\n\t"
      "addi.d %[sb], %[sb], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      "vhaddw.du.wu $vr4, $vr4, $vr4\n\t"
      "vhaddw.qu.du $vr4, $vr4, $vr4\n\t"
      "vpickve2gr.wu %[sse], $vr4, 0\n\t"
      : [sa] "+r"(src_a), [sb] "+r"(src_b), [cnt] "+r"(count), [sse] "=r"(sse)
      :
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4");
  return sse;
}
#endif  // HAS_SUMSQUAREERROR_LSX

#ifdef HAS_HASHDJB2_LSX
static const uint32_t kDjb2Muls_LSX[16] = {
    0x0c3525e1, 0xa3476dc1, 0x3b4039a1, 0x4f5f0981, 0x30f35d61, 0x855cb541,
    0x040a9121, 0x747c7101, 0xec41d4e1, 0x4cfa3cc1, 0x025528a1, 0x00121881,
    0x00008c61, 0x00000441, 0x00000021, 0x00000001,
};

uint32_t HashDjb2_LSX(const uint8_t* src, int count, uint32_t seed) {
  uint32_t hash = seed;
  uint32_t tmp;
  uint32_t c16 = 0x92d9e201u;
  asm volatile(
      "vld $vr4, %[muls], 0\n\t"
      "vld $vr5, %[muls], 16\n\t"
      "vld $vr6, %[muls], 32\n\t"
      "vld $vr7, %[muls], 48\n\t"
      "1:\n\t"
      "vld $vr0, %[src], 0\n\t"
      "vsllwil.hu.bu $vr1, $vr0, 0\n\t"
      "vexth.hu.bu $vr2, $vr0\n\t"
      "vsllwil.wu.hu $vr0, $vr1, 0\n\t"
      "vexth.wu.hu $vr1, $vr1\n\t"
      "vsllwil.wu.hu $vr3, $vr2, 0\n\t"
      "vexth.wu.hu $vr2, $vr2\n\t"
      "vmul.w $vr0, $vr0, $vr4\n\t"
      "vmadd.w $vr0, $vr1, $vr5\n\t"
      "vmadd.w $vr0, $vr3, $vr6\n\t"
      "vmadd.w $vr0, $vr2, $vr7\n\t"
      "vhaddw.du.wu $vr0, $vr0, $vr0\n\t"
      "vhaddw.qu.du $vr0, $vr0, $vr0\n\t"
      "vpickve2gr.wu %[tmp], $vr0, 0\n\t"
      "mul.w %[hash], %[hash], %[c16]\n\t"
      "add.w %[hash], %[hash], %[tmp]\n\t"
      "addi.d %[src], %[src], 16\n\t"
      "addi.d %[cnt], %[cnt], -16\n\t"
      "bnez %[cnt], 1b\n\t"
      : [src] "+r"(src), [cnt] "+r"(count), [hash] "+r"(hash), [tmp] "=&r"(tmp)
      : [muls] "r"(kDjb2Muls_LSX), [c16] "r"(c16)
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7");
  return hash;
}
#endif  // HAS_HASHDJB2_LSX

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)
