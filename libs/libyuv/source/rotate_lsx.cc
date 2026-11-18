/*
 *  Copyright 2022 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"

#if !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void TransposeWx8_LSX(const uint8_t* src,
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
      "addi.d     %[src], %[src], 16                   \n"

      "vilvl.b    $vr8, $vr1, $vr0                     \n"
      "vilvh.b    $vr9, $vr1, $vr0                     \n"
      "vilvl.b    $vr10, $vr3, $vr2                    \n"
      "vilvh.b    $vr11, $vr3, $vr2                    \n"
      "vilvl.b    $vr12, $vr5, $vr4                    \n"
      "vilvh.b    $vr13, $vr5, $vr4                    \n"
      "vilvl.b    $vr14, $vr7, $vr6                    \n"
      "vilvh.b    $vr15, $vr7, $vr6                    \n"

      "vilvl.h    $vr0, $vr10, $vr8                    \n"
      "vilvh.h    $vr1, $vr10, $vr8                    \n"
      "vilvl.h    $vr2, $vr11, $vr9                    \n"
      "vilvh.h    $vr3, $vr11, $vr9                    \n"
      "vilvl.h    $vr4, $vr14, $vr12                   \n"
      "vilvh.h    $vr5, $vr14, $vr12                   \n"
      "vilvl.h    $vr6, $vr15, $vr13                   \n"
      "vilvh.h    $vr7, $vr15, $vr13                   \n"

      "vilvl.w    $vr8, $vr4, $vr0                     \n"
      "vilvh.w    $vr9, $vr4, $vr0                     \n"
      "vilvl.w    $vr10, $vr5, $vr1                    \n"
      "vilvh.w    $vr11, $vr5, $vr1                    \n"
      "vilvl.w    $vr12, $vr6, $vr2                    \n"
      "vilvh.w    $vr13, $vr6, $vr2                    \n"
      "vilvl.w    $vr14, $vr7, $vr3                    \n"
      "vilvh.w    $vr15, $vr7, $vr3                    \n"

      "vstelm.d   $vr8, %[dst], 0, 0                   \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr8, %[dst], 0, 1                   \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr9, %[dst], 0, 0                   \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr9, %[dst], 0, 1                   \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr10, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr10, %[dst], 0, 1                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr11, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr11, %[dst], 0, 1                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr12, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr12, %[dst], 0, 1                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr13, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr13, %[dst], 0, 1                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr14, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr14, %[dst], 0, 1                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr15, %[dst], 0, 0                  \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "vstelm.d   $vr15, %[dst], 0, 1                  \n"
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
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15");
}

void TransposeUVWx8_LSX(const uint8_t* src,
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
      "addi.d     %[src], %[src], 16                   \n"

      "vilvl.b    $vr8, $vr1, $vr0                     \n"
      "vilvh.b    $vr9, $vr1, $vr0                     \n"
      "vilvl.b    $vr10, $vr3, $vr2                    \n"
      "vilvh.b    $vr11, $vr3, $vr2                    \n"
      "vilvl.b    $vr12, $vr5, $vr4                    \n"
      "vilvh.b    $vr13, $vr5, $vr4                    \n"
      "vilvl.b    $vr14, $vr7, $vr6                    \n"
      "vilvh.b    $vr15, $vr7, $vr6                    \n"

      "vilvl.h    $vr0, $vr10, $vr8                    \n"
      "vilvh.h    $vr1, $vr10, $vr8                    \n"
      "vilvl.h    $vr2, $vr11, $vr9                    \n"
      "vilvh.h    $vr3, $vr11, $vr9                    \n"
      "vilvl.h    $vr4, $vr14, $vr12                   \n"
      "vilvh.h    $vr5, $vr14, $vr12                   \n"
      "vilvl.h    $vr6, $vr15, $vr13                   \n"
      "vilvh.h    $vr7, $vr15, $vr13                   \n"

      "vilvl.w    $vr8, $vr4, $vr0                     \n"
      "vilvh.w    $vr9, $vr4, $vr0                     \n"
      "vilvl.w    $vr10, $vr5, $vr1                    \n"
      "vilvh.w    $vr11, $vr5, $vr1                    \n"
      "vilvl.w    $vr12, $vr6, $vr2                    \n"
      "vilvh.w    $vr13, $vr6, $vr2                    \n"
      "vilvl.w    $vr14, $vr7, $vr3                    \n"
      "vilvh.w    $vr15, $vr7, $vr3                    \n"

      "vstelm.d   $vr8, %[dst_a], 0, 0                 \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr8, %[dst_b], 0, 1                 \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr9, %[dst_a], 0, 0                 \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr9, %[dst_b], 0, 1                 \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr10, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr10, %[dst_b], 0, 1                \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr11, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr11, %[dst_b], 0, 1                \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr12, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr12, %[dst_b], 0, 1                \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr13, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr13, %[dst_b], 0, 1                \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr14, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr14, %[dst_b], 0, 1                \n"
      "add.d      %[dst_b], %[dst_b], %[db]            \n"
      "vstelm.d   $vr15, %[dst_a], 0, 0                \n"
      "add.d      %[dst_a], %[dst_a], %[da]            \n"
      "vstelm.d   $vr15, %[dst_b], 0, 1                \n"
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
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15");
}

void TransposeWx4_32_LSX(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t s2 = s * 2;
  const ptrdiff_t s3 = s + s2;
  const ptrdiff_t s4 = s * 4;
  uint8_t* dst0 = dst;
  uint8_t* dst1 = dst0 + dst_stride;
  uint8_t* dst2 = dst1 + dst_stride;
  uint8_t* dst3 = dst2 + dst_stride;
  asm volatile(
      "srli.w     %[w], %[w], 2                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "vld        $vr0, %[src], 0                      \n"
      "vldx       $vr1, %[src], %[s]                   \n"
      "vldx       $vr2, %[src], %[s2]                  \n"
      "vldx       $vr3, %[src], %[s3]                  \n"
      "add.d      %[src], %[src], %[s4]                \n"

      "vilvl.w    $vr4, $vr1, $vr0                     \n"
      "vilvh.w    $vr5, $vr1, $vr0                     \n"
      "vilvl.w    $vr6, $vr3, $vr2                     \n"
      "vilvh.w    $vr7, $vr3, $vr2                     \n"

      "vilvl.d    $vr0, $vr6, $vr4                     \n"
      "vilvh.d    $vr1, $vr6, $vr4                     \n"
      "vilvl.d    $vr2, $vr7, $vr5                     \n"
      "vilvh.d    $vr3, $vr7, $vr5                     \n"

      "vst        $vr0, %[dst0], 0                     \n"
      "vst        $vr1, %[dst1], 0                     \n"
      "vst        $vr2, %[dst2], 0                     \n"
      "vst        $vr3, %[dst3], 0                     \n"
      "addi.d     %[dst0], %[dst0], 16                 \n"
      "addi.d     %[dst1], %[dst1], 16                 \n"
      "addi.d     %[dst2], %[dst2], 16                 \n"
      "addi.d     %[dst3], %[dst3], 16                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src),    // %[src]
        [dst0] "+r"(dst0),  // %[dst0]
        [dst1] "+r"(dst1),  // %[dst1]
        [dst2] "+r"(dst2),  // %[dst2]
        [dst3] "+r"(dst3),  // %[dst3]
        [w] "+r"(width)     // %[w]
      : [s] "r"(s),         // %[s]
        [s2] "r"(s2),       // %[s2]
        [s3] "r"(s3),       // %[s3]
        [s4] "r"(s4)        // %[s4]
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7");
}

void TransposeWx8_16_LSX(const uint16_t* src,
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
      "addi.d     %[src], %[src], 16                   \n"

      "vilvl.h    $vr8, $vr1, $vr0                     \n"
      "vilvh.h    $vr9, $vr1, $vr0                     \n"
      "vilvl.h    $vr10, $vr3, $vr2                    \n"
      "vilvh.h    $vr11, $vr3, $vr2                    \n"
      "vilvl.h    $vr12, $vr5, $vr4                    \n"
      "vilvh.h    $vr13, $vr5, $vr4                    \n"
      "vilvl.h    $vr14, $vr7, $vr6                    \n"
      "vilvh.h    $vr15, $vr7, $vr6                    \n"

      "vilvl.w    $vr0, $vr10, $vr8                    \n"
      "vilvh.w    $vr1, $vr10, $vr8                    \n"
      "vilvl.w    $vr2, $vr11, $vr9                    \n"
      "vilvh.w    $vr3, $vr11, $vr9                    \n"
      "vilvl.w    $vr4, $vr14, $vr12                   \n"
      "vilvh.w    $vr5, $vr14, $vr12                   \n"
      "vilvl.w    $vr6, $vr15, $vr13                   \n"
      "vilvh.w    $vr7, $vr15, $vr13                   \n"

      "vilvl.d    $vr8, $vr4, $vr0                     \n"
      "vilvh.d    $vr9, $vr4, $vr0                     \n"
      "vilvl.d    $vr10, $vr5, $vr1                    \n"
      "vilvh.d    $vr11, $vr5, $vr1                    \n"
      "vilvl.d    $vr12, $vr6, $vr2                    \n"
      "vilvh.d    $vr13, $vr6, $vr2                    \n"
      "vilvl.d    $vr14, $vr7, $vr3                    \n"
      "vilvh.d    $vr15, $vr7, $vr3                    \n"

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
      : [src] "+r"(src_b),  // %[src]
        [dst] "+r"(dst_b),  // %[dst]
        [w] "+r"(width),    // %[w]
        [t] "=&r"(t)        // %[t]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "$vr0", "$vr1", "$vr2", "$vr3", "$vr4", "$vr5", "$vr6",
        "$vr7", "$vr8", "$vr9", "$vr10", "$vr11", "$vr12", "$vr13", "$vr14",
        "$vr15");
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)
