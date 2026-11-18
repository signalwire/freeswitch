/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"

#if !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void TransposeWx16_LASX(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  const uint8_t* t;
  asm volatile(
      "srli.w     %[w], %[w], 4                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "move       %[t], %[src]                         \n"
      "vld        $vr0, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr1, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr2, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr3, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr4, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr5, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr6, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr7, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr8, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr9, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr10, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr11, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr12, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr13, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr14, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr15, %[t], 0                       \n"
      "addi.d     %[src], %[src], 16                   \n"

      "xvpermi.q  $xr0, $xr8, 0x02                     \n"
      "xvpermi.q  $xr1, $xr9, 0x02                     \n"
      "xvpermi.q  $xr2, $xr10, 0x02                    \n"
      "xvpermi.q  $xr3, $xr11, 0x02                    \n"
      "xvpermi.q  $xr4, $xr12, 0x02                    \n"
      "xvpermi.q  $xr5, $xr13, 0x02                    \n"
      "xvpermi.q  $xr6, $xr14, 0x02                    \n"
      "xvpermi.q  $xr7, $xr15, 0x02                    \n"

      "xvilvl.b   $xr8, $xr1, $xr0                     \n"
      "xvilvh.b   $xr9, $xr1, $xr0                     \n"
      "xvilvl.b   $xr10, $xr3, $xr2                    \n"
      "xvilvh.b   $xr11, $xr3, $xr2                    \n"
      "xvilvl.b   $xr12, $xr5, $xr4                    \n"
      "xvilvh.b   $xr13, $xr5, $xr4                    \n"
      "xvilvl.b   $xr14, $xr7, $xr6                    \n"
      "xvilvh.b   $xr15, $xr7, $xr6                    \n"

      "xvilvl.h   $xr0, $xr10, $xr8                    \n"
      "xvilvh.h   $xr1, $xr10, $xr8                    \n"
      "xvilvl.h   $xr2, $xr11, $xr9                    \n"
      "xvilvh.h   $xr3, $xr11, $xr9                    \n"
      "xvilvl.h   $xr4, $xr14, $xr12                   \n"
      "xvilvh.h   $xr5, $xr14, $xr12                   \n"
      "xvilvl.h   $xr6, $xr15, $xr13                   \n"
      "xvilvh.h   $xr7, $xr15, $xr13                   \n"

      "xvilvl.w   $xr8, $xr4, $xr0                     \n"
      "xvilvh.w   $xr9, $xr4, $xr0                     \n"
      "xvilvl.w   $xr10, $xr5, $xr1                    \n"
      "xvilvh.w   $xr11, $xr5, $xr1                    \n"
      "xvilvl.w   $xr12, $xr6, $xr2                    \n"
      "xvilvh.w   $xr13, $xr6, $xr2                    \n"
      "xvilvl.w   $xr14, $xr7, $xr3                    \n"
      "xvilvh.w   $xr15, $xr7, $xr3                    \n"

      "xvpermi.d  $xr0, $xr8, 0x08                     \n"
      "xvpermi.d  $xr1, $xr8, 0x0d                     \n"
      "xvpermi.d  $xr2, $xr9, 0x08                     \n"
      "xvpermi.d  $xr3, $xr9, 0x0d                     \n"
      "xvpermi.d  $xr4, $xr10, 0x08                    \n"
      "xvpermi.d  $xr5, $xr10, 0x0d                    \n"
      "xvpermi.d  $xr6, $xr11, 0x08                    \n"
      "xvpermi.d  $xr7, $xr11, 0x0d                    \n"
      "xvpermi.d  $xr8, $xr12, 0x08                    \n"
      "xvpermi.d  $xr9, $xr12, 0x0d                    \n"
      "xvpermi.d  $xr10, $xr13, 0x08                   \n"
      "xvpermi.d  $xr11, $xr13, 0x0d                   \n"
      "xvpermi.d  $xr12, $xr14, 0x08                   \n"
      "xvpermi.d  $xr13, $xr14, 0x0d                   \n"
      "xvpermi.d  $xr14, $xr15, 0x08                   \n"
      "xvpermi.d  $xr15, $xr15, 0x0d                   \n"

      "vst        $vr0, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr1, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr2, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr3, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr4, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr5, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr6, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr7, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr8, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr9, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr10, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr11, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr12, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr13, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr14, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vst        $vr15, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src),  // %[src]
        [dst] "+r"(dst),  // %[dst]
        [w] "+r"(width),  // %[w]
        [t] "=&r"(t)      // %[t]
      : [s] "r"(s),       // %[s]
        [d] "r"(d)        // %[d]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15");
}

void TransposeUVWx16_LASX(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst_a,
                          int dst_stride_a,
                          uint8_t* dst_b,
                          int dst_stride_b,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t da = dst_stride_a;
  const ptrdiff_t db = dst_stride_b;
  const uint8_t* t;
  asm volatile(
      "srli.w     %[w], %[w], 3                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "move       %[t], %[src]                         \n"
      "vld        $vr0, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr1, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr2, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr3, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr4, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr5, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr6, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr7, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr8, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr9, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr10, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr11, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr12, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr13, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr14, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr15, %[t], 0                       \n"
      "addi.d     %[src], %[src], 16                   \n"

      "xvpermi.q  $xr0, $xr8, 0x02                     \n"
      "xvpermi.q  $xr1, $xr9, 0x02                     \n"
      "xvpermi.q  $xr2, $xr10, 0x02                    \n"
      "xvpermi.q  $xr3, $xr11, 0x02                    \n"
      "xvpermi.q  $xr4, $xr12, 0x02                    \n"
      "xvpermi.q  $xr5, $xr13, 0x02                    \n"
      "xvpermi.q  $xr6, $xr14, 0x02                    \n"
      "xvpermi.q  $xr7, $xr15, 0x02                    \n"

      "xvilvl.b   $xr8, $xr1, $xr0                     \n"
      "xvilvh.b   $xr9, $xr1, $xr0                     \n"
      "xvilvl.b   $xr10, $xr3, $xr2                    \n"
      "xvilvh.b   $xr11, $xr3, $xr2                    \n"
      "xvilvl.b   $xr12, $xr5, $xr4                    \n"
      "xvilvh.b   $xr13, $xr5, $xr4                    \n"
      "xvilvl.b   $xr14, $xr7, $xr6                    \n"
      "xvilvh.b   $xr15, $xr7, $xr6                    \n"

      "xvilvl.h   $xr0, $xr10, $xr8                    \n"
      "xvilvh.h   $xr1, $xr10, $xr8                    \n"
      "xvilvl.h   $xr2, $xr11, $xr9                    \n"
      "xvilvh.h   $xr3, $xr11, $xr9                    \n"
      "xvilvl.h   $xr4, $xr14, $xr12                   \n"
      "xvilvh.h   $xr5, $xr14, $xr12                   \n"
      "xvilvl.h   $xr6, $xr15, $xr13                   \n"
      "xvilvh.h   $xr7, $xr15, $xr13                   \n"

      "xvilvl.w   $xr8, $xr4, $xr0                     \n"
      "xvilvh.w   $xr9, $xr4, $xr0                     \n"
      "xvilvl.w   $xr10, $xr5, $xr1                    \n"
      "xvilvh.w   $xr11, $xr5, $xr1                    \n"
      "xvilvl.w   $xr12, $xr6, $xr2                    \n"
      "xvilvh.w   $xr13, $xr6, $xr2                    \n"
      "xvilvl.w   $xr14, $xr7, $xr3                    \n"
      "xvilvh.w   $xr15, $xr7, $xr3                    \n"

      "xvpermi.d  $xr0, $xr8, 0x08                     \n"
      "xvpermi.d  $xr1, $xr8, 0x0d                     \n"
      "xvpermi.d  $xr2, $xr9, 0x08                     \n"
      "xvpermi.d  $xr3, $xr9, 0x0d                     \n"
      "xvpermi.d  $xr4, $xr10, 0x08                    \n"
      "xvpermi.d  $xr5, $xr10, 0x0d                    \n"
      "xvpermi.d  $xr6, $xr11, 0x08                    \n"
      "xvpermi.d  $xr7, $xr11, 0x0d                    \n"
      "xvpermi.d  $xr8, $xr12, 0x08                    \n"
      "xvpermi.d  $xr9, $xr12, 0x0d                    \n"
      "xvpermi.d  $xr10, $xr13, 0x08                   \n"
      "xvpermi.d  $xr11, $xr13, 0x0d                   \n"
      "xvpermi.d  $xr12, $xr14, 0x08                   \n"
      "xvpermi.d  $xr13, $xr14, 0x0d                   \n"
      "xvpermi.d  $xr14, $xr15, 0x08                   \n"
      "xvpermi.d  $xr15, $xr15, 0x0d                   \n"

      "vst        $vr0, %[dst_a], 0                    \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr1, %[dst_b], 0                    \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr2, %[dst_a], 0                    \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr3, %[dst_b], 0                    \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr4, %[dst_a], 0                    \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr5, %[dst_b], 0                    \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr6, %[dst_a], 0                    \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr7, %[dst_b], 0                    \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr8, %[dst_a], 0                    \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr9, %[dst_b], 0                    \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr10, %[dst_a], 0                   \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr11, %[dst_b], 0                   \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr12, %[dst_a], 0                   \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr13, %[dst_b], 0                   \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vst        $vr14, %[dst_a], 0                   \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vst        $vr15, %[dst_b], 0                   \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src),      // %[src]
        [dst_a] "+r"(dst_a),  // %[dst_a]
        [dst_b] "+r"(dst_b),  // %[dst_b]
        [w] "+r"(width),      // %[w]
        [t] "=&r"(t)          // %[t]
      : [s] "r"(s),           // %[s]
        [da] "r"(da),         // %[da]
        [db] "r"(db)          // %[db]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15");
}

void TransposeWx4_32_LASX(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  ptrdiff_t s = src_stride;
  ptrdiff_t s2 = s * 2;
  ptrdiff_t s3 = s + s2;
  ptrdiff_t s4 = s * 4;
  uint8_t* dst0 = dst;
  uint8_t* dst1 = dst0 + dst_stride;
  uint8_t* dst2 = dst1 + dst_stride;
  uint8_t* dst3 = dst2 + dst_stride;
  const uint8_t* src4;

  asm volatile(
      "srli.w     %[w], %[w], 3                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "vld        $vr0, %[src], 0                      \n"
      "vldx       $vr1, %[src], %[s]                   \n"
      "vldx       $vr2, %[src], %[s2]                  \n"
      "vldx       $vr3, %[src], %[s3]                  \n"
      "add.d      %[src4], %[src], %[s4]               \n"
      "vld        $vr4, %[src4], 0                     \n"
      "vldx       $vr5, %[src4], %[s]                  \n"
      "vldx       $vr6, %[src4], %[s2]                 \n"
      "vldx       $vr7, %[src4], %[s3]                 \n"
      "add.d      %[src], %[src4], %[s4]               \n"

      "xvpermi.q  $xr0, $xr4, 0x02                     \n"
      "xvpermi.q  $xr1, $xr5, 0x02                     \n"
      "xvpermi.q  $xr2, $xr6, 0x02                     \n"
      "xvpermi.q  $xr3, $xr7, 0x02                     \n"

      "xvilvl.w   $xr4, $xr1, $xr0                     \n"
      "xvilvh.w   $xr5, $xr1, $xr0                     \n"
      "xvilvl.w   $xr6, $xr3, $xr2                     \n"
      "xvilvh.w   $xr7, $xr3, $xr2                     \n"

      "xvilvl.d   $xr0, $xr6, $xr4                     \n"
      "xvilvh.d   $xr1, $xr6, $xr4                     \n"
      "xvilvl.d   $xr2, $xr7, $xr5                     \n"
      "xvilvh.d   $xr3, $xr7, $xr5                     \n"

      "xvst       $xr0, %[dst0], 0                     \n"
      "xvst       $xr1, %[dst1], 0                     \n"
      "xvst       $xr2, %[dst2], 0                     \n"
      "xvst       $xr3, %[dst3], 0                     \n"
      "addi.d     %[dst0], %[dst0], 32                 \n"
      "addi.d     %[dst1], %[dst1], 32                 \n"
      "addi.d     %[dst2], %[dst2], 32                 \n"
      "addi.d     %[dst3], %[dst3], 32                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src),     // %[src]
        [src4] "=&r"(src4),  // %[src4]
        [dst0] "+r"(dst0),   // %[dst0]
        [dst1] "+r"(dst1),   // %[dst1]
        [dst2] "+r"(dst2),   // %[dst2]
        [dst3] "+r"(dst3),   // %[dst3]
        [w] "+r"(width)      // %[w]
      : [s] "r"(s),          // %[s]
        [s2] "r"(s2),        // %[s2]
        [s3] "r"(s3),        // %[s3]
        [s4] "r"(s4)         // %[s4]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7");
}

void TransposeWx16_16_LASX(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* src_b = (const uint8_t*)src;
  uint8_t* dst_b = (uint8_t*)dst;
  const uint8_t* t;
  asm volatile(
      "srli.w     %[w], %[w], 3                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "move       %[t], %[src]                         \n"
      "vld        $vr0, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr1, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr2, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr3, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr4, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr5, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr6, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr7, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"

      "vld        $vr8, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr9, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr10, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr11, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr12, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr13, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr14, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr15, %[t], 0                       \n"
      "addi.d     %[src], %[src], 16                   \n"

      "xvpermi.q  $xr0, $xr8, 0x02                     \n"
      "xvpermi.q  $xr1, $xr9, 0x02                     \n"
      "xvpermi.q  $xr2, $xr10, 0x02                    \n"
      "xvpermi.q  $xr3, $xr11, 0x02                    \n"
      "xvpermi.q  $xr4, $xr12, 0x02                    \n"
      "xvpermi.q  $xr5, $xr13, 0x02                    \n"
      "xvpermi.q  $xr6, $xr14, 0x02                    \n"
      "xvpermi.q  $xr7, $xr15, 0x02                    \n"

      "xvilvl.h   $xr8, $xr1, $xr0                     \n"
      "xvilvh.h   $xr9, $xr1, $xr0                     \n"
      "xvilvl.h   $xr10, $xr3, $xr2                    \n"
      "xvilvh.h   $xr11, $xr3, $xr2                    \n"
      "xvilvl.h   $xr12, $xr5, $xr4                    \n"
      "xvilvh.h   $xr13, $xr5, $xr4                    \n"
      "xvilvl.h   $xr14, $xr7, $xr6                    \n"
      "xvilvh.h   $xr15, $xr7, $xr6                    \n"

      "xvilvl.w   $xr0, $xr10, $xr8                    \n"
      "xvilvh.w   $xr1, $xr10, $xr8                    \n"
      "xvilvl.w   $xr2, $xr11, $xr9                    \n"
      "xvilvh.w   $xr3, $xr11, $xr9                    \n"
      "xvilvl.w   $xr4, $xr14, $xr12                   \n"
      "xvilvh.w   $xr5, $xr14, $xr12                   \n"
      "xvilvl.w   $xr6, $xr15, $xr13                   \n"
      "xvilvh.w   $xr7, $xr15, $xr13                   \n"

      "xvilvl.d   $xr8, $xr4, $xr0                     \n"
      "xvilvh.d   $xr9, $xr4, $xr0                     \n"
      "xvilvl.d   $xr10, $xr5, $xr1                    \n"
      "xvilvh.d   $xr11, $xr5, $xr1                    \n"
      "xvilvl.d   $xr12, $xr6, $xr2                    \n"
      "xvilvh.d   $xr13, $xr6, $xr2                    \n"
      "xvilvl.d   $xr14, $xr7, $xr3                    \n"
      "xvilvh.d   $xr15, $xr7, $xr3                    \n"

      "xvst       $xr8, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr9, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr10, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr11, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr12, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr13, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr14, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr15, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src_b),  // %[src]
        [dst] "+r"(dst_b),  // %[dst]
        [w] "+r"(width),    // %[w]
        [t] "=&r"(t)        // %[t]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15");
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)
