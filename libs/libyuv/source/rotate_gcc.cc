/*
 *  Copyright 2015 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#include <string.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for GCC and clang x86 and x64.
#if !defined(LIBYUV_DISABLE_X86) &&               \
    (defined(__x86_64__) || defined(__i386__)) && \
    !defined(LIBYUV_ENABLE_ROWWIN) &&             \
    (!defined(_MSC_VER) || defined(__clang__))

#if defined(HAS_TRANSPOSEWX8_AVX2) || defined(HAS_TRANSPOSEUVWX8_AVX2)
// Transpose 16x8 bytes using 2 lanes of AVX2. Source row r is in lane r / 4
// of ymm(r % 4). Byte and word unpacks produce 4-row column chunks in each
// lane; vperm2i128 + dword unpack join rows 0..3 and 4..7. Each 128-bit lane
// holds 8 bytes for dst_a (low 64 bits) and 8 bytes for dst_b (high 64 bits).
// Width is a multiple of 16 bytes.
static void TransposeWx8_Byte_AVX2(const uint8_t* src,
                                   int src_stride,
                                   uint8_t* dst_a,
                                   int dst_stride_a,
                                   uint8_t* dst_b,
                                   int dst_stride_b,
                                   int byte_width) {
  const ptrdiff_t d_a = dst_stride_a;
  const ptrdiff_t d_b = dst_stride_b;
  uintptr_t temp;
  asm volatile(
      "1:          \n"
      "vmovdqu     (%[src]),%%xmm0               \n"
      "vmovdqu     (%[src],%[src_stride]),%%xmm1 \n"
      "lea         (%[src],%[src_stride],2),%[temp] \n"
      "lea         0x10(%[src]),%[src]           \n"
      "vmovdqu     (%[temp]),%%xmm2              \n"
      "vmovdqu     (%[temp],%[src_stride]),%%xmm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm0,%%ymm0    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm1,%%ymm1 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm2,%%ymm2    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm3,%%ymm3 \n"

      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm4          \n"
      "vpunpckhbw  %%ymm1,%%ymm0,%%ymm5          \n"
      "vpunpcklbw  %%ymm3,%%ymm2,%%ymm6          \n"
      "vpunpckhbw  %%ymm3,%%ymm2,%%ymm7          \n"

      "vpunpcklwd  %%ymm6,%%ymm4,%%ymm0          \n"
      "vpunpckhwd  %%ymm6,%%ymm4,%%ymm1          \n"
      "vpunpcklwd  %%ymm7,%%ymm5,%%ymm2          \n"
      "vpunpckhwd  %%ymm7,%%ymm5,%%ymm3          \n"

      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm4    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm0,%%ymm5    \n"
      "vperm2i128  $0x20,%%ymm3,%%ymm2,%%ymm6    \n"
      "vperm2i128  $0x31,%%ymm3,%%ymm2,%%ymm7    \n"

      "vpunpckldq  %%ymm5,%%ymm4,%%ymm0          \n"
      "vpunpckhdq  %%ymm5,%%ymm4,%%ymm1          \n"
      "vpunpckldq  %%ymm7,%%ymm6,%%ymm2          \n"
      "vpunpckhdq  %%ymm7,%%ymm6,%%ymm3          \n"

      "vmovq       %%xmm0,(%[dst_a])             \n"
      "vmovhpd     %%xmm0,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vmovq       %%xmm1,(%[dst_a])             \n"
      "vmovhpd     %%xmm1,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vextracti128 $1,%%ymm0,%%xmm4             \n"
      "vextracti128 $1,%%ymm1,%%xmm5             \n"
      "vmovq       %%xmm4,(%[dst_a])             \n"
      "vmovhpd     %%xmm4,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vmovq       %%xmm5,(%[dst_a])             \n"
      "vmovhpd     %%xmm5,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vmovq       %%xmm2,(%[dst_a])             \n"
      "vmovhpd     %%xmm2,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vmovq       %%xmm3,(%[dst_a])             \n"
      "vmovhpd     %%xmm3,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vextracti128 $1,%%ymm2,%%xmm6             \n"
      "vextracti128 $1,%%ymm3,%%xmm7             \n"
      "vmovq       %%xmm6,(%[dst_a])             \n"
      "vmovhpd     %%xmm6,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "vmovq       %%xmm7,(%[dst_a])             \n"
      "vmovhpd     %%xmm7,(%[dst_b])             \n"
      "add         %[dst_stride_a],%[dst_a]      \n"
      "add         %[dst_stride_b],%[dst_b]      \n"
      "subl        $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src] "+r"(src), [dst_a] "+r"(dst_a), [dst_b] "+r"(dst_b),
        [width] "+rm"(byte_width), [temp] "=&r"(temp)
      : [src_stride] "r"((ptrdiff_t)src_stride),
        [dst_stride_a] "rm"(d_a),
        [dst_stride_b] "rm"(d_b)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // defined(HAS_TRANSPOSEWX8_AVX2) || defined(HAS_TRANSPOSEUVWX8_AVX2)

#if defined(HAS_TRANSPOSEWX8_AVX2)
void TransposeWx8_AVX2(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width) {
  TransposeWx8_Byte_AVX2(src, src_stride, dst, dst_stride * 2, dst + dst_stride,
                         dst_stride * 2, width);
}
#endif  // defined(HAS_TRANSPOSEWX8_AVX2)

#if defined(HAS_TRANSPOSEUVWX8_AVX2)
void TransposeUVWx8_AVX2(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst_a,
                         int dst_stride_a,
                         uint8_t* dst_b,
                         int dst_stride_b,
                         int width) {
  TransposeWx8_Byte_AVX2(src, src_stride, dst_a, dst_stride_a, dst_b,
                         dst_stride_b, width * 2);
}
#endif  // defined(HAS_TRANSPOSEUVWX8_AVX2)

#if defined(HAS_TRANSPOSEWX16_AVX512BW) || defined(HAS_TRANSPOSEUVWX16_AVX512BW)
// Dword permutes to gather 16 rows of a column from 4 lanes.
static const uint32_t kPermdTranspose_AVX512BW[32] = {
    0, 4, 8, 12, 2, 6, 10, 14, 16, 20, 24, 28, 18, 22, 26, 30,
    1, 5, 9, 13, 3, 7, 11, 15, 17, 21, 25, 29, 19, 23, 27, 31};

// Transpose 16x16 bytes held in 4 lanes of 4 zmm registers.
// Source row r is in lane r / 4 of zmm(r % 4). Outputs zmm0..zmm3 are 16
// destination rows, lane n of zmm0/zmm2 for dst_a and lane n of zmm1/zmm3
// for dst_b.
#define TRANSPOSE16X16_AVX512BW                  \
  "vpunpcklbw  %%zmm1,%%zmm0,%%zmm4          \n" \
  "vpunpckhbw  %%zmm1,%%zmm0,%%zmm5          \n" \
  "vpunpcklbw  %%zmm3,%%zmm2,%%zmm0          \n" \
  "vpunpckhbw  %%zmm3,%%zmm2,%%zmm1          \n" \
  "vpunpcklwd  %%zmm0,%%zmm4,%%zmm2          \n" \
  "vpunpckhwd  %%zmm0,%%zmm4,%%zmm3          \n" \
  "vpunpcklwd  %%zmm1,%%zmm5,%%zmm4          \n" \
  "vpunpckhwd  %%zmm1,%%zmm5,%%zmm5          \n" \
  "vmovdqa64   %%zmm2,%%zmm0                 \n" \
  "vpermt2d    %%zmm4,%%zmm6,%%zmm0          \n" \
  "vmovdqa64   %%zmm2,%%zmm1                 \n" \
  "vpermt2d    %%zmm4,%%zmm7,%%zmm1          \n" \
  "vmovdqa64   %%zmm3,%%zmm2                 \n" \
  "vpermt2d    %%zmm5,%%zmm6,%%zmm2          \n" \
  "vpermt2d    %%zmm5,%%zmm7,%%zmm3          \n"

// Transpose 16x16 bytes using 4 lanes of AVX512BW with tail masking.
// Can be used for 8-bit planar transpose (dst_a = even rows, dst_b = odd rows)
// or 16-bit UV split transpose (dst_a = U plane, dst_b = V plane).
// TODO(fbarchard): Port to rotate_win.cc using intrinsics.
// TODO(fbarchard): Use for ARGB (32 bit) and 16 bit channel transposes.
// TODO(fbarchard): Consider removing in favor of TransposeWx16_Byte_AVX2,
// which is within 10% of AVX512BW on most CPUs.
LIBYUV_TARGET_AVX512BW
static void TransposeWx16_Byte_AVX512BW(const uint8_t* src,
                                        int src_stride,
                                        uint8_t* dst_a,
                                        int dst_stride_a,
                                        uint8_t* dst_b,
                                        int dst_stride_b,
                                        int byte_width) {
  uintptr_t temp;
  asm volatile(
      "vmovdqu32   (%[shuf]),%%zmm6              \n"
      "vmovdqu32   0x40(%[shuf]),%%zmm7          \n"
      "sub         $0x10,%[width]                \n"
      "jl          2f                            \n"

      // Main loop: 16 source rows x 16 bytes.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src]),%%xmm0 \n"
      "vmovdqu     (%[src],%[src_stride]),%%xmm1 \n"
      "lea         (%[src],%[src_stride],2),%[temp] \n"
      "lea         0x10(%[src]),%[src] \n"
      "vmovdqu     (%[temp]),%%xmm2 \n"
      "vmovdqu     (%[temp],%[src_stride]),%%xmm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $1,(%[temp]),%%zmm0,%%zmm0 \n"
      "vinserti32x4 $1,(%[temp],%[src_stride]),%%zmm1,%%zmm1 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $1,(%[temp]),%%zmm2,%%zmm2 \n"
      "vinserti32x4 $1,(%[temp],%[src_stride]),%%zmm3,%%zmm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $2,(%[temp]),%%zmm0,%%zmm0 \n"
      "vinserti32x4 $2,(%[temp],%[src_stride]),%%zmm1,%%zmm1 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $2,(%[temp]),%%zmm2,%%zmm2 \n"
      "vinserti32x4 $2,(%[temp],%[src_stride]),%%zmm3,%%zmm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $3,(%[temp]),%%zmm0,%%zmm0 \n"
      "vinserti32x4 $3,(%[temp],%[src_stride]),%%zmm1,%%zmm1 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti32x4 $3,(%[temp]),%%zmm2,%%zmm2 \n"
      "vinserti32x4 $3,(%[temp],%[src_stride]),%%zmm3,%%zmm3 \n"

      TRANSPOSE16X16_AVX512BW

      "vmovdqu     %%xmm0,(%[dst_a])             \n"
      "vmovdqu     %%xmm1,(%[dst_b])             \n"
      "vextracti32x4 $1,%%zmm0,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti32x4 $1,%%zmm1,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vmovdqu     %%xmm2,(%[dst_a])             \n"
      "vmovdqu     %%xmm3,(%[dst_b])             \n"
      "vextracti32x4 $1,%%zmm2,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti32x4 $1,%%zmm3,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vextracti32x4 $2,%%zmm0,(%[dst_a])        \n"
      "vextracti32x4 $2,%%zmm1,(%[dst_b])        \n"
      "vextracti32x4 $3,%%zmm0,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti32x4 $3,%%zmm1,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vextracti32x4 $2,%%zmm2,(%[dst_a])        \n"
      "vextracti32x4 $2,%%zmm3,(%[dst_b])        \n"
      "vextracti32x4 $3,%%zmm2,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti32x4 $3,%%zmm3,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "sub         $0x10,%[width]                \n"
      "jge         1b                            \n"

      // Remainder: 1 to 15 bytes, masked loads.
      "2:          \n"
      "add         $0x10,%[width]                \n"
      "je          99f                           \n"
      "mov         $-1,%k[temp]                  \n"
      "bzhi        %k[width],%k[temp],%k[temp]   \n"
      "kmovw       %k[temp],%%k1                 \n"
      "vmovdqu8    (%[src]),%%xmm0%{%%k1%}%{z%} \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm1%{%%k1%}%{z%} \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm2%{%%k1%}%{z%} \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm3%{%%k1%}%{z%} \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $1,%%xmm4,%%zmm0,%%zmm0 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $1,%%xmm4,%%zmm1,%%zmm1 \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $1,%%xmm4,%%zmm2,%%zmm2 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $1,%%xmm4,%%zmm3,%%zmm3 \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $2,%%xmm4,%%zmm0,%%zmm0 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $2,%%xmm4,%%zmm1,%%zmm1 \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $2,%%xmm4,%%zmm2,%%zmm2 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $2,%%xmm4,%%zmm3,%%zmm3 \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $3,%%xmm4,%%zmm0,%%zmm0 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $3,%%xmm4,%%zmm1,%%zmm1 \n"
      "lea         (%[src],%[src_stride],2),%[src] \n"
      "vmovdqu8    (%[src]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $3,%%xmm4,%%zmm2,%%zmm2 \n"
      "vmovdqu8    (%[src],%[src_stride]),%%xmm4%{%%k1%}%{z%} \n"
      "vinserti32x4 $3,%%xmm4,%%zmm3,%%zmm3 \n"

      TRANSPOSE16X16_AVX512BW

      // Store 1 destination row per remaining byte.
      "vmovdqu     %%xmm0,(%[dst_a])             \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vmovdqu     %%xmm1,(%[dst_b])             \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $1,%%zmm0,(%[dst_a],%[dst_stride_a]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $1,%%zmm1,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vmovdqu     %%xmm2,(%[dst_a])             \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vmovdqu     %%xmm3,(%[dst_b])             \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $1,%%zmm2,(%[dst_a],%[dst_stride_a]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $1,%%zmm3,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $2,%%zmm0,(%[dst_a])        \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $2,%%zmm1,(%[dst_b])        \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $3,%%zmm0,(%[dst_a],%[dst_stride_a]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $3,%%zmm1,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $2,%%zmm2,(%[dst_a])        \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $2,%%zmm3,(%[dst_b])        \n"
      "sub         $0x1,%[width]                 \n"
      "je          99f                           \n"
      "vextracti32x4 $3,%%zmm2,(%[dst_a],%[dst_stride_a]) \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src] "+r"(src), [dst_a] "+r"(dst_a), [dst_b] "+r"(dst_b),
        [width] "+r"(byte_width), [temp] "=&r"(temp)
      : [src_stride] "r"((ptrdiff_t)src_stride),
        [dst_stride_a] "r"((ptrdiff_t)dst_stride_a),
        [dst_stride_b] "r"((ptrdiff_t)dst_stride_b),
        [shuf] "r"(kPermdTranspose_AVX512BW)
      : "memory", "cc", "k1", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
        "xmm6", "xmm7");
}
#undef TRANSPOSE16X16_AVX512BW
#endif  // defined(HAS_TRANSPOSEWX16_AVX512BW) ||
        // defined(HAS_TRANSPOSEUVWX16_AVX512BW)

#if defined(HAS_TRANSPOSEWX16_AVX512BW)
void TransposeWx16_AVX512BW(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width) {
  TransposeWx16_Byte_AVX512BW(src, src_stride, dst, dst_stride * 2,
                              dst + (width > 1 ? dst_stride : 0),
                              dst_stride * 2, width);
}
#endif  // defined(HAS_TRANSPOSEWX16_AVX512BW)

#if defined(HAS_TRANSPOSEUVWX16_AVX512BW)
void TransposeUVWx16_AVX512BW(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst_a,
                              int dst_stride_a,
                              uint8_t* dst_b,
                              int dst_stride_b,
                              int width) {
  TransposeWx16_Byte_AVX512BW(src, src_stride, dst_a, dst_stride_a, dst_b,
                              dst_stride_b, width * 2);
}
#endif  // defined(HAS_TRANSPOSEUVWX16_AVX512BW)

#if defined(__x86_64__)
#if defined(HAS_TRANSPOSEWX16_AVX2) || defined(HAS_TRANSPOSEUVWX16_AVX2)
// Transpose 16x16 bytes using 2 lanes of AVX2. Source row r is in lane r / 8
// of ymm(r % 8). 3 unpack stages leave each lane with 2 columns x 8 rows, and
// vpermq joins the rows 0..7 and 8..15 halves. Lane 0 of each output is an
// even column for dst_a and lane 1 an odd column for dst_b.
// Width is a multiple of 16 bytes.
static void TransposeWx16_Byte_AVX2(const uint8_t* src,
                                    int src_stride,
                                    uint8_t* dst_a,
                                    int dst_stride_a,
                                    uint8_t* dst_b,
                                    int dst_stride_b,
                                    int byte_width) {
  uintptr_t temp;
  asm volatile(
      "1:          \n"
      "vmovdqu     (%[src]),%%xmm0               \n"
      "vmovdqu     (%[src],%[src_stride]),%%xmm1 \n"
      "lea         (%[src],%[src_stride],2),%[temp] \n"
      "lea         0x10(%[src]),%[src]           \n"
      "vmovdqu     (%[temp]),%%xmm2              \n"
      "vmovdqu     (%[temp],%[src_stride]),%%xmm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vmovdqu     (%[temp]),%%xmm4              \n"
      "vmovdqu     (%[temp],%[src_stride]),%%xmm5 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vmovdqu     (%[temp]),%%xmm6              \n"
      "vmovdqu     (%[temp],%[src_stride]),%%xmm7 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm0,%%ymm0    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm1,%%ymm1 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm2,%%ymm2    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm3,%%ymm3 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm4,%%ymm4    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm5,%%ymm5 \n"
      "lea         (%[temp],%[src_stride],2),%[temp] \n"
      "vinserti128 $1,(%[temp]),%%ymm6,%%ymm6    \n"
      "vinserti128 $1,(%[temp],%[src_stride]),%%ymm7,%%ymm7 \n"

      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm8          \n"
      "vpunpckhbw  %%ymm1,%%ymm0,%%ymm9          \n"
      "vpunpcklbw  %%ymm3,%%ymm2,%%ymm10         \n"
      "vpunpckhbw  %%ymm3,%%ymm2,%%ymm11         \n"
      "vpunpcklbw  %%ymm5,%%ymm4,%%ymm12         \n"
      "vpunpckhbw  %%ymm5,%%ymm4,%%ymm13         \n"
      "vpunpcklbw  %%ymm7,%%ymm6,%%ymm14         \n"
      "vpunpckhbw  %%ymm7,%%ymm6,%%ymm15         \n"

      "vpunpcklwd  %%ymm10,%%ymm8,%%ymm0         \n"
      "vpunpckhwd  %%ymm10,%%ymm8,%%ymm1         \n"
      "vpunpcklwd  %%ymm11,%%ymm9,%%ymm2         \n"
      "vpunpckhwd  %%ymm11,%%ymm9,%%ymm3         \n"
      "vpunpcklwd  %%ymm14,%%ymm12,%%ymm4        \n"
      "vpunpckhwd  %%ymm14,%%ymm12,%%ymm5        \n"
      "vpunpcklwd  %%ymm15,%%ymm13,%%ymm6        \n"
      "vpunpckhwd  %%ymm15,%%ymm13,%%ymm7        \n"

      "vpunpckldq  %%ymm4,%%ymm0,%%ymm8          \n"
      "vpunpckhdq  %%ymm4,%%ymm0,%%ymm9          \n"
      "vpunpckldq  %%ymm5,%%ymm1,%%ymm10         \n"
      "vpunpckhdq  %%ymm5,%%ymm1,%%ymm11         \n"
      "vpunpckldq  %%ymm6,%%ymm2,%%ymm12         \n"
      "vpunpckhdq  %%ymm6,%%ymm2,%%ymm13         \n"
      "vpunpckldq  %%ymm7,%%ymm3,%%ymm14         \n"
      "vpunpckhdq  %%ymm7,%%ymm3,%%ymm15         \n"

      "vpermq      $0xd8,%%ymm8,%%ymm8           \n"
      "vpermq      $0xd8,%%ymm9,%%ymm9           \n"
      "vpermq      $0xd8,%%ymm10,%%ymm10         \n"
      "vpermq      $0xd8,%%ymm11,%%ymm11         \n"
      "vpermq      $0xd8,%%ymm12,%%ymm12         \n"
      "vpermq      $0xd8,%%ymm13,%%ymm13         \n"
      "vpermq      $0xd8,%%ymm14,%%ymm14         \n"
      "vpermq      $0xd8,%%ymm15,%%ymm15         \n"

      "vmovdqu     %%xmm8,(%[dst_a])             \n"
      "vextracti128 $1,%%ymm8,(%[dst_b])         \n"
      "vmovdqu     %%xmm9,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti128 $1,%%ymm9,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vmovdqu     %%xmm10,(%[dst_a])            \n"
      "vextracti128 $1,%%ymm10,(%[dst_b])        \n"
      "vmovdqu     %%xmm11,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti128 $1,%%ymm11,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vmovdqu     %%xmm12,(%[dst_a])            \n"
      "vextracti128 $1,%%ymm12,(%[dst_b])        \n"
      "vmovdqu     %%xmm13,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti128 $1,%%ymm13,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "vmovdqu     %%xmm14,(%[dst_a])            \n"
      "vextracti128 $1,%%ymm14,(%[dst_b])        \n"
      "vmovdqu     %%xmm15,(%[dst_a],%[dst_stride_a]) \n"
      "vextracti128 $1,%%ymm15,(%[dst_b],%[dst_stride_b]) \n"
      "lea         (%[dst_a],%[dst_stride_a],2),%[dst_a] \n"
      "lea         (%[dst_b],%[dst_stride_b],2),%[dst_b] \n"
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src] "+r"(src), [dst_a] "+r"(dst_a), [dst_b] "+r"(dst_b),
        [width] "+r"(byte_width), [temp] "=&r"(temp)
      : [src_stride] "r"((ptrdiff_t)src_stride),
        [dst_stride_a] "r"((ptrdiff_t)dst_stride_a),
        [dst_stride_b] "r"((ptrdiff_t)dst_stride_b)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
        "xmm15");
}
#endif  // defined(HAS_TRANSPOSEWX16_AVX2) || defined(HAS_TRANSPOSEUVWX16_AVX2)

#if defined(HAS_TRANSPOSEWX16_AVX2)
void TransposeWx16_AVX2(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width) {
  TransposeWx16_Byte_AVX2(src, src_stride, dst, dst_stride * 2,
                          dst + dst_stride, dst_stride * 2, width);
}
#endif  // defined(HAS_TRANSPOSEWX16_AVX2)

#if defined(HAS_TRANSPOSEUVWX16_AVX2)
void TransposeUVWx16_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst_a,
                          int dst_stride_a,
                          uint8_t* dst_b,
                          int dst_stride_b,
                          int width) {
  TransposeWx16_Byte_AVX2(src, src_stride, dst_a, dst_stride_a, dst_b,
                          dst_stride_b, width * 2);
}
#endif  // defined(HAS_TRANSPOSEUVWX16_AVX2)
#else   // !defined(__x86_64__)
#if defined(HAS_TRANSPOSEWX16_AVX2)
void TransposeWx16_AVX2(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width) {
  TransposeWx8_AVX2(src, src_stride, dst, dst_stride, width);
  TransposeWx8_AVX2(src + 8 * (ptrdiff_t)src_stride, src_stride, dst + 8,
                    dst_stride, width);
}
#endif  // defined(HAS_TRANSPOSEWX16_AVX2)

#if defined(HAS_TRANSPOSEUVWX16_AVX2)
void TransposeUVWx16_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst_a,
                          int dst_stride_a,
                          uint8_t* dst_b,
                          int dst_stride_b,
                          int width) {
  TransposeUVWx8_AVX2(src, src_stride, dst_a, dst_stride_a, dst_b, dst_stride_b,
                      width);
  TransposeUVWx8_AVX2(src + 8 * (ptrdiff_t)src_stride, src_stride, dst_a + 8,
                      dst_stride_a, dst_b + 8, dst_stride_b, width);
}
#endif  // defined(HAS_TRANSPOSEUVWX16_AVX2)
#endif  // defined(__x86_64__)

#if defined(HAS_TRANSPOSEWX16_16_AVX2)
#if defined(__x86_64__)
void TransposeWx16_16_AVX2(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* src_b = (const uint8_t*)src;
  uint8_t* dst_b = (uint8_t*)dst;
  const uint8_t* temp;
  asm volatile(
      "1:                                          \n"
      "vmovdqu      (%[src]),%%xmm0                \n"
      "vmovdqu      (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovdqu      (%[temp]),%%xmm2               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovdqu      (%[temp]),%%xmm4               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm5          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovdqu      (%[temp]),%%xmm6               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm7          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"

      "vinserti128  $1,(%[temp]),%%ymm0,%%ymm0     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm1,%%ymm1 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm2,%%ymm2     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm3,%%ymm3 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm4,%%ymm4     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm5,%%ymm5 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm6,%%ymm6     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm7,%%ymm7 \n"
      "add          $0x10,%[src]                   \n"

      // Stage 1: 16-bit unpack -> ymm8..ymm15
      "vpunpcklwd   %%ymm1,%%ymm0,%%ymm8           \n"
      "vpunpckhwd   %%ymm1,%%ymm0,%%ymm9           \n"
      "vpunpcklwd   %%ymm3,%%ymm2,%%ymm10          \n"
      "vpunpckhwd   %%ymm3,%%ymm2,%%ymm11          \n"
      "vpunpcklwd   %%ymm5,%%ymm4,%%ymm12          \n"
      "vpunpckhwd   %%ymm5,%%ymm4,%%ymm13          \n"
      "vpunpcklwd   %%ymm7,%%ymm6,%%ymm14          \n"
      "vpunpckhwd   %%ymm7,%%ymm6,%%ymm15          \n"

      // Stage 2: 32-bit unpack -> ymm0..ymm7
      "vpunpckldq   %%ymm10,%%ymm8,%%ymm0          \n"
      "vpunpckhdq   %%ymm10,%%ymm8,%%ymm1          \n"
      "vpunpckldq   %%ymm11,%%ymm9,%%ymm2          \n"
      "vpunpckhdq   %%ymm11,%%ymm9,%%ymm3          \n"
      "vpunpckldq   %%ymm14,%%ymm12,%%ymm4         \n"
      "vpunpckhdq   %%ymm14,%%ymm12,%%ymm5         \n"
      "vpunpckldq   %%ymm15,%%ymm13,%%ymm6         \n"
      "vpunpckhdq   %%ymm15,%%ymm13,%%ymm7         \n"

      // Stage 3: 64-bit unpack -> ymm8..ymm15
      "vpunpcklqdq  %%ymm4,%%ymm0,%%ymm8           \n"
      "vpunpckhqdq  %%ymm4,%%ymm0,%%ymm9           \n"
      "vpunpcklqdq  %%ymm5,%%ymm1,%%ymm10          \n"
      "vpunpckhqdq  %%ymm5,%%ymm1,%%ymm11          \n"
      "vpunpcklqdq  %%ymm6,%%ymm2,%%ymm12          \n"
      "vpunpckhqdq  %%ymm6,%%ymm2,%%ymm13          \n"
      "vpunpcklqdq  %%ymm7,%%ymm3,%%ymm14          \n"
      "vpunpckhqdq  %%ymm7,%%ymm3,%%ymm15          \n"

      "vmovdqu      %%ymm8,(%[dst])                \n"
      "vmovdqu      %%ymm9,(%[dst],%[d])           \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%ymm10,(%[dst])               \n"
      "vmovdqu      %%ymm11,(%[dst],%[d])          \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%ymm12,(%[dst])               \n"
      "vmovdqu      %%ymm13,(%[dst],%[d])          \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%ymm14,(%[dst])               \n"
      "vmovdqu      %%ymm15,(%[dst],%[d])          \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "sub          $0x8,%[w]                      \n"
      "jg           1b                             \n"
      "vzeroupper                                  \n"
      : [src] "+r"(src_b),  // %[src]
        [dst] "+r"(dst_b),  // %[dst]
        [w] "+r"(width),    // %[w]
        [temp] "=&r"(temp)  // %[temp]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
        "xmm15");
}
#endif  // defined(__x86_64__)
#endif  // defined(HAS_TRANSPOSEWX16_16_AVX2)

#if defined(HAS_TRANSPOSEWX8_16_AVX2)
void TransposeWx8_16_AVX2(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* src_b = (const uint8_t*)src;
  uint8_t* dst_b = (uint8_t*)dst;
  const uint8_t* temp;
  asm volatile(
      "1:                                          \n"
      "vmovdqu      (%[src]),%%xmm0                \n"
      "vmovdqu      (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovdqu      (%[temp]),%%xmm2               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm0,%%ymm0     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm1,%%ymm1 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm2,%%ymm2     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm3,%%ymm3 \n"
      "add          $0x10,%[src]                   \n"

      "vpunpcklwd   %%ymm1,%%ymm0,%%ymm4           \n"
      "vpunpckhwd   %%ymm1,%%ymm0,%%ymm5           \n"
      "vpunpcklwd   %%ymm3,%%ymm2,%%ymm6           \n"
      "vpunpckhwd   %%ymm3,%%ymm2,%%ymm7           \n"

      "vpunpckldq   %%ymm6,%%ymm4,%%ymm0           \n"
      "vpunpckhdq   %%ymm6,%%ymm4,%%ymm1           \n"
      "vpunpckldq   %%ymm7,%%ymm5,%%ymm2           \n"
      "vpunpckhdq   %%ymm7,%%ymm5,%%ymm3           \n"

      "vpermq       $0xd8,%%ymm0,%%ymm0            \n"
      "vpermq       $0xd8,%%ymm1,%%ymm1            \n"
      "vpermq       $0xd8,%%ymm2,%%ymm2            \n"
      "vpermq       $0xd8,%%ymm3,%%ymm3            \n"

      "vmovdqu      %%xmm0,(%[dst])                \n"
      "vextracti128 $1,%%ymm0,(%[dst],%[d])        \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%xmm1,(%[dst])                \n"
      "vextracti128 $1,%%ymm1,(%[dst],%[d])        \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%xmm2,(%[dst])                \n"
      "vextracti128 $1,%%ymm2,(%[dst],%[d])        \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%xmm3,(%[dst])                \n"
      "vextracti128 $1,%%ymm3,(%[dst],%[d])        \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "subl         $0x8,%[w]                      \n"
      "jg           1b                             \n"
      "vzeroupper                                  \n"
      : [src] "+r"(src_b),
        [dst] "+r"(dst_b),
#if defined(__i386__)
        [w] "+m"(width),
#else
        [w] "+rm"(width),
#endif
        [temp] "=&r"(temp)
      : [s] "r"(s),
        [d] "r"(d)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // defined(HAS_TRANSPOSEWX8_16_AVX2)

#if defined(HAS_TRANSPOSEWX16_16_AVX2) && !defined(__x86_64__)
void TransposeWx16_16_AVX2(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width) {
  TransposeWx8_16_AVX2(src, src_stride, dst, dst_stride, width);
  TransposeWx8_16_AVX2(src + 8 * (ptrdiff_t)src_stride, src_stride, dst + 8,
                       dst_stride, width);
}
#endif  // defined(HAS_TRANSPOSEWX16_16_AVX2) && !defined(__x86_64__)

#if defined(HAS_TRANSPOSEWX32_16_AVX512BW)
// Transpose 32 rows of 16 bit plane (arbitrary width) using 8x32 tiles in
// zmm0..zmm15 with k1-masked right tail.
LIBYUV_TARGET_AVX512BW
void TransposeWx32_16_AVX512BW(const uint16_t* src,
                               int src_stride,
                               uint16_t* dst,
                               int dst_stride,
                               int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* src_b = (const uint8_t*)src;
  uint8_t* dst_b = (uint8_t*)dst;
  const int n = width & ~7;
  if (n > 0) {
    int cnt = n;
    const uint8_t* temp;
    asm volatile(
        "1:                                          \n"
        "vmovdqu      (%[src]),%%xmm0                \n"
        "vmovdqu      (%[src],%[s]),%%xmm1           \n"
        "lea          (%[src],%[s],2),%[temp]        \n"
        "vmovdqu      (%[temp]),%%xmm2               \n"
        "vmovdqu      (%[temp],%[s]),%%xmm3          \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vmovdqu      (%[temp]),%%xmm4               \n"
        "vmovdqu      (%[temp],%[s]),%%xmm5          \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vmovdqu      (%[temp]),%%xmm6               \n"
        "vmovdqu      (%[temp],%[s]),%%xmm7          \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"

        "vinserti32x4 $1,(%[temp]),%%zmm0,%%zmm0     \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $1,(%[temp]),%%zmm2,%%zmm2     \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $1,(%[temp]),%%zmm4,%%zmm4     \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm5,%%zmm5 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $1,(%[temp]),%%zmm6,%%zmm6     \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm7,%%zmm7 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"

        "vinserti32x4 $2,(%[temp]),%%zmm0,%%zmm0     \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $2,(%[temp]),%%zmm2,%%zmm2     \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $2,(%[temp]),%%zmm4,%%zmm4     \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm5,%%zmm5 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $2,(%[temp]),%%zmm6,%%zmm6     \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm7,%%zmm7 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"

        "vinserti32x4 $3,(%[temp]),%%zmm0,%%zmm0     \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $3,(%[temp]),%%zmm2,%%zmm2     \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $3,(%[temp]),%%zmm4,%%zmm4     \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm5,%%zmm5 \n"
        "lea          (%[temp],%[s],2),%[temp]       \n"
        "vinserti32x4 $3,(%[temp]),%%zmm6,%%zmm6     \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm7,%%zmm7 \n"
        "add          $0x10,%[src]                   \n"

        // Stage 1: 16-bit unpack -> zmm8..zmm15
        "vpunpcklwd   %%zmm1,%%zmm0,%%zmm8           \n"
        "vpunpckhwd   %%zmm1,%%zmm0,%%zmm9           \n"
        "vpunpcklwd   %%zmm3,%%zmm2,%%zmm10          \n"
        "vpunpckhwd   %%zmm3,%%zmm2,%%zmm11          \n"
        "vpunpcklwd   %%zmm5,%%zmm4,%%zmm12          \n"
        "vpunpckhwd   %%zmm5,%%zmm4,%%zmm13          \n"
        "vpunpcklwd   %%zmm7,%%zmm6,%%zmm14          \n"
        "vpunpckhwd   %%zmm7,%%zmm6,%%zmm15          \n"

        // Stage 2: 32-bit unpack -> zmm0..zmm7
        "vpunpckldq   %%zmm10,%%zmm8,%%zmm0          \n"
        "vpunpckhdq   %%zmm10,%%zmm8,%%zmm1          \n"
        "vpunpckldq   %%zmm11,%%zmm9,%%zmm2          \n"
        "vpunpckhdq   %%zmm11,%%zmm9,%%zmm3          \n"
        "vpunpckldq   %%zmm14,%%zmm12,%%zmm4         \n"
        "vpunpckhdq   %%zmm14,%%zmm12,%%zmm5         \n"
        "vpunpckldq   %%zmm15,%%zmm13,%%zmm6         \n"
        "vpunpckhdq   %%zmm15,%%zmm13,%%zmm7         \n"

        // Stage 3: 64-bit unpack -> zmm8..zmm15
        "vpunpcklqdq  %%zmm4,%%zmm0,%%zmm8           \n"
        "vpunpckhqdq  %%zmm4,%%zmm0,%%zmm9           \n"
        "vpunpcklqdq  %%zmm5,%%zmm1,%%zmm10          \n"
        "vpunpckhqdq  %%zmm5,%%zmm1,%%zmm11          \n"
        "vpunpcklqdq  %%zmm6,%%zmm2,%%zmm12          \n"
        "vpunpckhqdq  %%zmm6,%%zmm2,%%zmm13          \n"
        "vpunpcklqdq  %%zmm7,%%zmm3,%%zmm14          \n"
        "vpunpckhqdq  %%zmm7,%%zmm3,%%zmm15          \n"

        "vmovdqu16    %%zmm8,(%[dst])                \n"
        "vmovdqu16    %%zmm9,(%[dst],%[d])           \n"
        "lea          (%[dst],%[d],2),%[dst]         \n"
        "vmovdqu16    %%zmm10,(%[dst])               \n"
        "vmovdqu16    %%zmm11,(%[dst],%[d])          \n"
        "lea          (%[dst],%[d],2),%[dst]         \n"
        "vmovdqu16    %%zmm12,(%[dst])               \n"
        "vmovdqu16    %%zmm13,(%[dst],%[d])          \n"
        "lea          (%[dst],%[d],2),%[dst]         \n"
        "vmovdqu16    %%zmm14,(%[dst])               \n"
        "vmovdqu16    %%zmm15,(%[dst],%[d])          \n"
        "lea          (%[dst],%[d],2),%[dst]         \n"
        "sub          $0x8,%[cnt]                    \n"
        "jg           1b                             \n"
        : [src] "+r"(src_b),  // %[src]
          [dst] "+r"(dst_b),  // %[dst]
          [cnt] "+r"(cnt),    // %[cnt]
          [temp] "=&r"(temp)  // %[temp]
        : [s] "r"(s),         // %[s]
          [d] "r"(d)          // %[d]
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
          "xmm14", "xmm15");
    width -= n;
  }
  if (width > 0) {
    const uint32_t col_mask = (1u << width) - 1;
    const uint8_t* temp;
    asm volatile(
        "kmovw        %[col_mask],%%k1                   \n"
        "vmovdqu16    (%[src]),%%xmm0%{%%k1%}%{z%}       \n"
        "vmovdqu16    (%[src],%[s]),%%xmm1%{%%k1%}%{z%}  \n"
        "lea          (%[src],%[s],2),%[temp]            \n"
        "vmovdqu16    (%[temp]),%%xmm2%{%%k1%}%{z%}      \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm3%{%%k1%}%{z%} \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm4%{%%k1%}%{z%}      \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm5%{%%k1%}%{z%} \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm6%{%%k1%}%{z%}      \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm7%{%%k1%}%{z%} \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $1,%%xmm16,%%zmm0,%%zmm0           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm16,%%zmm1,%%zmm1           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $1,%%xmm16,%%zmm2,%%zmm2           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm16,%%zmm3,%%zmm3           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $1,%%xmm16,%%zmm4,%%zmm4           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm16,%%zmm5,%%zmm5           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $1,%%xmm16,%%zmm6,%%zmm6           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm16,%%zmm7,%%zmm7           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $2,%%xmm16,%%zmm0,%%zmm0           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm16,%%zmm1,%%zmm1           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $2,%%xmm16,%%zmm2,%%zmm2           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm16,%%zmm3,%%zmm3           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $2,%%xmm16,%%zmm4,%%zmm4           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm16,%%zmm5,%%zmm5           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $2,%%xmm16,%%zmm6,%%zmm6           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm16,%%zmm7,%%zmm7           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $3,%%xmm16,%%zmm0,%%zmm0           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm16,%%zmm1,%%zmm1           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $3,%%xmm16,%%zmm2,%%zmm2           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm16,%%zmm3,%%zmm3           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $3,%%xmm16,%%zmm4,%%zmm4           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm16,%%zmm5,%%zmm5           \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu16    (%[temp]),%%xmm16%{%%k1%}%{z%}     \n"
        "vinserti32x4 $3,%%xmm16,%%zmm6,%%zmm6           \n"
        "vmovdqu16    (%[temp],%[s]),%%xmm16%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm16,%%zmm7,%%zmm7           \n"

        // Stage 1: 16-bit unpack -> zmm8..zmm15
        "vpunpcklwd   %%zmm1,%%zmm0,%%zmm8               \n"
        "vpunpckhwd   %%zmm1,%%zmm0,%%zmm9               \n"
        "vpunpcklwd   %%zmm3,%%zmm2,%%zmm10              \n"
        "vpunpckhwd   %%zmm3,%%zmm2,%%zmm11              \n"
        "vpunpcklwd   %%zmm5,%%zmm4,%%zmm12              \n"
        "vpunpckhwd   %%zmm5,%%zmm4,%%zmm13              \n"
        "vpunpcklwd   %%zmm7,%%zmm6,%%zmm14              \n"
        "vpunpckhwd   %%zmm7,%%zmm6,%%zmm15              \n"

        // Stage 2: 32-bit unpack -> zmm0..zmm7
        "vpunpckldq   %%zmm10,%%zmm8,%%zmm0              \n"
        "vpunpckhdq   %%zmm10,%%zmm8,%%zmm1              \n"
        "vpunpckldq   %%zmm11,%%zmm9,%%zmm2              \n"
        "vpunpckhdq   %%zmm11,%%zmm9,%%zmm3              \n"
        "vpunpckldq   %%zmm14,%%zmm12,%%zmm4             \n"
        "vpunpckhdq   %%zmm14,%%zmm12,%%zmm5             \n"
        "vpunpckldq   %%zmm15,%%zmm13,%%zmm6             \n"
        "vpunpckhdq   %%zmm15,%%zmm13,%%zmm7             \n"

        // Stage 3: 64-bit unpack -> zmm8..zmm15
        "vpunpcklqdq  %%zmm4,%%zmm0,%%zmm8               \n"
        "vpunpckhqdq  %%zmm4,%%zmm0,%%zmm9               \n"
        "vpunpcklqdq  %%zmm5,%%zmm1,%%zmm10              \n"
        "vpunpckhqdq  %%zmm5,%%zmm1,%%zmm11              \n"
        "vpunpcklqdq  %%zmm6,%%zmm2,%%zmm12              \n"
        "vpunpckhqdq  %%zmm6,%%zmm2,%%zmm13              \n"
        "vpunpcklqdq  %%zmm7,%%zmm3,%%zmm14              \n"
        "vpunpckhqdq  %%zmm7,%%zmm3,%%zmm15              \n"

        "vmovdqu16    %%zmm8,(%[dst])                    \n"
        "cmp          $1,%[width]                        \n"
        "jle          2f                                 \n"
        "vmovdqu16    %%zmm9,(%[dst],%[d])               \n"
        "cmp          $2,%[width]                        \n"
        "jle          2f                                 \n"
        "lea          (%[dst],%[d],2),%[dst]             \n"
        "vmovdqu16    %%zmm10,(%[dst])                   \n"
        "cmp          $3,%[width]                        \n"
        "jle          2f                                 \n"
        "vmovdqu16    %%zmm11,(%[dst],%[d])              \n"
        "cmp          $4,%[width]                        \n"
        "jle          2f                                 \n"
        "lea          (%[dst],%[d],2),%[dst]             \n"
        "vmovdqu16    %%zmm12,(%[dst])                   \n"
        "cmp          $5,%[width]                        \n"
        "jle          2f                                 \n"
        "vmovdqu16    %%zmm13,(%[dst],%[d])              \n"
        "cmp          $6,%[width]                        \n"
        "jle          2f                                 \n"
        "lea          (%[dst],%[d],2),%[dst]             \n"
        "vmovdqu16    %%zmm14,(%[dst])                   \n"
        "2:                                              \n"
        : [dst] "+r"(dst_b),         // %[dst]
          [temp] "=&r"(temp)         // %[temp]
        : [src] "r"(src_b),          // %[src]
          [s] "r"(s),                // %[s]
          [d] "r"(d),                // %[d]
          [col_mask] "r"(col_mask),  // %[col_mask]
          [width] "r"(width)         // %[width]
        : "memory", "cc", "k1", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
          "xmm14", "xmm15", "xmm16");
  }
  asm volatile("vzeroupper \n");
}
#endif  // defined(HAS_TRANSPOSEWX32_16_AVX512BW)

#if defined(HAS_TRANSPOSEWX4_32_AVX2)
// Transpose 32 bit values (ARGB). Read a column of 4 pixels across 8 rows per
// AVX2 iteration (with a 4-row VEX 128-bit tail) and write 4 destination rows.
void TransposeWx4_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  uintptr_t temp;
  asm volatile(
      "subl         $0x8,%[width]                  \n"
      "jl           2f                             \n"

      // Main loop: 8 source rows of 4 pixels -> 4 dst rows of 8 pixels (32 B).
      "1:                                          \n"
      "vmovdqu      (%[src]),%%xmm0                \n"
      "vmovdqu      (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovdqu      (%[temp]),%%xmm2               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm0,%%ymm0     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm1,%%ymm1 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm2,%%ymm2     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm3,%%ymm3 \n"
      "lea          (%[temp],%[s],2),%[src]        \n"

      "vpunpckldq   %%ymm1,%%ymm0,%%ymm4           \n"
      "vpunpckldq   %%ymm3,%%ymm2,%%ymm5           \n"
      "vpunpckhdq   %%ymm1,%%ymm0,%%ymm6           \n"
      "vpunpckhdq   %%ymm3,%%ymm2,%%ymm7           \n"

      "vpunpcklqdq  %%ymm5,%%ymm4,%%ymm0           \n"
      "vpunpckhqdq  %%ymm5,%%ymm4,%%ymm1           \n"
      "vpunpcklqdq  %%ymm7,%%ymm6,%%ymm2           \n"
      "vpunpckhqdq  %%ymm7,%%ymm6,%%ymm3           \n"

      "vmovdqu      %%ymm0,(%[dst])                \n"
      "vmovdqu      %%ymm1,(%[dst],%[d])           \n"
      "lea          (%[dst],%[d],2),%[temp]        \n"
      "vmovdqu      %%ymm2,(%[temp])               \n"
      "vmovdqu      %%ymm3,(%[temp],%[d])          \n"
      "add          $0x20,%[dst]                   \n"
      "subl         $0x8,%[width]                  \n"
      "jge          1b                             \n"

      // Tail: 4 source rows of 4 pixels -> 4 dst rows of 4 pixels (16 B).
      "2:                                          \n"
      "addl         $0x8,%[width]                  \n"
      "je           99f                            \n"
      "vmovdqu      (%[src]),%%xmm0                \n"
      "vmovdqu      (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovdqu      (%[temp]),%%xmm2               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm3          \n"

      "vpunpckldq   %%xmm1,%%xmm0,%%xmm4           \n"
      "vpunpckldq   %%xmm3,%%xmm2,%%xmm5           \n"
      "vpunpckhdq   %%xmm1,%%xmm0,%%xmm6           \n"
      "vpunpckhdq   %%xmm3,%%xmm2,%%xmm7           \n"

      "vpunpcklqdq  %%xmm5,%%xmm4,%%xmm0           \n"
      "vpunpckhqdq  %%xmm5,%%xmm4,%%xmm1           \n"
      "vpunpcklqdq  %%xmm7,%%xmm6,%%xmm2           \n"
      "vpunpckhqdq  %%xmm7,%%xmm6,%%xmm3           \n"

      "vmovdqu      %%xmm0,(%[dst])                \n"
      "vmovdqu      %%xmm1,(%[dst],%[d])           \n"
      "lea          (%[dst],%[d],2),%[temp]        \n"
      "vmovdqu      %%xmm2,(%[temp])               \n"
      "vmovdqu      %%xmm3,(%[temp],%[d])          \n"

      "99:                                         \n"
      "vzeroupper                                  \n"
      : [src] "+r"(src),       // %[src]
        [dst] "+r"(dst),       // %[dst]
        [width] "+rm"(width),  // %[width]
        [temp] "=&r"(temp)     // %[temp]
      : [s] "r"(s),            // %[s]
        [d] "r"(d)             // %[d]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // defined(HAS_TRANSPOSEWX4_32_AVX2)

#if defined(HAS_TRANSPOSEWX8_32_AVX2)
// Transpose 8 rows of 32 bit plane (width multiple of 4) using 4x8 YMM vertical
// tiles in ymm0..ymm7.
void TransposeWx8_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  const uint8_t* temp;
  asm volatile(
      "1:                                          \n"
      "vmovdqu      (%[src]),%%xmm0                \n"
      "vmovdqu      (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovdqu      (%[temp]),%%xmm2               \n"
      "vmovdqu      (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"

      "vinserti128  $1,(%[temp]),%%ymm0,%%ymm0     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm1,%%ymm1 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vinserti128  $1,(%[temp]),%%ymm2,%%ymm2     \n"
      "vinserti128  $1,(%[temp],%[s]),%%ymm3,%%ymm3 \n"
      "add          $0x10,%[src]                   \n"

      // Stage 1: 32-bit unpack across both 128-bit lanes
      "vpunpckldq   %%ymm1,%%ymm0,%%ymm4           \n"
      "vpunpckldq   %%ymm3,%%ymm2,%%ymm5           \n"
      "vpunpckhdq   %%ymm1,%%ymm0,%%ymm6           \n"
      "vpunpckhdq   %%ymm3,%%ymm2,%%ymm7           \n"

      // Stage 2: 64-bit unpack across both 128-bit lanes
      "vpunpcklqdq  %%ymm5,%%ymm4,%%ymm0           \n"
      "vpunpckhqdq  %%ymm5,%%ymm4,%%ymm1           \n"
      "vpunpcklqdq  %%ymm7,%%ymm6,%%ymm2           \n"
      "vpunpckhqdq  %%ymm7,%%ymm6,%%ymm3           \n"

      // Store 4 full 256-bit (32 B) rows = 8 32-bit pixels each
      "vmovdqu      %%ymm0,(%[dst])                \n"
      "vmovdqu      %%ymm1,(%[dst],%[d])           \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "vmovdqu      %%ymm2,(%[dst])                \n"
      "vmovdqu      %%ymm3,(%[dst],%[d])           \n"
      "lea          (%[dst],%[d],2),%[dst]         \n"
      "subl         $0x4,%[width]                  \n"
      "jg           1b                             \n"
      "vzeroupper                                  \n"
      : [src] "+r"(src),       // %[src]
        [dst] "+r"(dst),       // %[dst]
        [width] "+rm"(width),  // %[width]
        [temp] "=&r"(temp)     // %[temp]
      : [s] "r"(s),            // %[s]
        [d] "r"(d)             // %[d]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // defined(HAS_TRANSPOSEWX8_32_AVX2)

#if defined(HAS_TRANSPOSEWXH_32_AVX2)
// Transpose rem_w (1..3) columns x 8 rows of 32-bit pixels using AVX2.
static void TransposeRemWx8_32_AVX2(const uint8_t* src,
                                    ptrdiff_t s,
                                    uint8_t* dst,
                                    ptrdiff_t d,
                                    int rem_w) {
  const uint8_t* temp;
  asm volatile(
      "cmpl         $2,%[rem_w]                    \n"
      "jl           1f                             \n"
      "je           2f                             \n"

      // rem_w == 3: load 12 bytes per row from 8 rows.
      "vmovq        (%[src]),%%xmm0                \n"
      "vpinsrd      $2,8(%[src]),%%xmm0,%%xmm0     \n"
      "vmovq        (%[src],%[s]),%%xmm1           \n"
      "vpinsrd      $2,8(%[src],%[s]),%%xmm1,%%xmm1 \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovq        (%[temp]),%%xmm2               \n"
      "vpinsrd      $2,8(%[temp]),%%xmm2,%%xmm2    \n"
      "vmovq        (%[temp],%[s]),%%xmm3          \n"
      "vpinsrd      $2,8(%[temp],%[s]),%%xmm3,%%xmm3 \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovq        (%[temp]),%%xmm4               \n"
      "vpinsrd      $2,8(%[temp]),%%xmm4,%%xmm4    \n"
      "vinserti128  $1,%%xmm4,%%ymm0,%%ymm0        \n"
      "vmovq        (%[temp],%[s]),%%xmm5          \n"
      "vpinsrd      $2,8(%[temp],%[s]),%%xmm5,%%xmm5 \n"
      "vinserti128  $1,%%xmm5,%%ymm1,%%ymm1        \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovq        (%[temp]),%%xmm4               \n"
      "vpinsrd      $2,8(%[temp]),%%xmm4,%%xmm4    \n"
      "vinserti128  $1,%%xmm4,%%ymm2,%%ymm2        \n"
      "vmovq        (%[temp],%[s]),%%xmm5          \n"
      "vpinsrd      $2,8(%[temp],%[s]),%%xmm5,%%xmm5 \n"
      "vinserti128  $1,%%xmm5,%%ymm3,%%ymm3        \n"
      "jmp          3f                             \n"

      // rem_w == 2: load 8 bytes per row from 8 rows.
      "2:                                          \n"
      "vmovq        (%[src]),%%xmm0                \n"
      "vmovq        (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovq        (%[temp]),%%xmm2               \n"
      "vmovq        (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovq        (%[temp]),%%xmm4               \n"
      "vinserti128  $1,%%xmm4,%%ymm0,%%ymm0        \n"
      "vmovq        (%[temp],%[s]),%%xmm5          \n"
      "vinserti128  $1,%%xmm5,%%ymm1,%%ymm1        \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovq        (%[temp]),%%xmm4               \n"
      "vinserti128  $1,%%xmm4,%%ymm2,%%ymm2        \n"
      "vmovq        (%[temp],%[s]),%%xmm5          \n"
      "vinserti128  $1,%%xmm5,%%ymm3,%%ymm3        \n"
      "jmp          3f                             \n"

      // rem_w == 1: load 4 bytes per row from 8 rows.
      "1:                                          \n"
      "vmovd        (%[src]),%%xmm0                \n"
      "vmovd        (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovd        (%[temp]),%%xmm2               \n"
      "vmovd        (%[temp],%[s]),%%xmm3          \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovd        (%[temp]),%%xmm4               \n"
      "vinserti128  $1,%%xmm4,%%ymm0,%%ymm0        \n"
      "vmovd        (%[temp],%[s]),%%xmm5          \n"
      "vinserti128  $1,%%xmm5,%%ymm1,%%ymm1        \n"
      "lea          (%[temp],%[s],2),%[temp]       \n"
      "vmovd        (%[temp]),%%xmm4               \n"
      "vinserti128  $1,%%xmm4,%%ymm2,%%ymm2        \n"
      "vmovd        (%[temp],%[s]),%%xmm5          \n"
      "vinserti128  $1,%%xmm5,%%ymm3,%%ymm3        \n"

      "3:                                          \n"
      "vpunpckldq   %%ymm1,%%ymm0,%%ymm4           \n"
      "vpunpckldq   %%ymm3,%%ymm2,%%ymm5           \n"
      "vpunpcklqdq  %%ymm5,%%ymm4,%%ymm6           \n"
      "vmovdqu      %%ymm6,(%[dst])                \n"
      "cmpl         $1,%[rem_w]                    \n"
      "jle          4f                             \n"
      "vpunpckhqdq  %%ymm5,%%ymm4,%%ymm7           \n"
      "vmovdqu      %%ymm7,(%[dst],%[d])           \n"
      "cmpl         $2,%[rem_w]                    \n"
      "jle          4f                             \n"
      "vpunpckhdq   %%ymm1,%%ymm0,%%ymm4           \n"
      "vpunpckhdq   %%ymm3,%%ymm2,%%ymm5           \n"
      "vpunpcklqdq  %%ymm5,%%ymm4,%%ymm6           \n"
      "vmovdqu      %%ymm6,(%[dst],%[d],2)         \n"
      "4:                                          \n"
      "vzeroupper                                  \n"
      : [temp] "=&r"(temp)
      : [src] "r"(src),
        [s] "r"(s),
        [dst] "r"(dst),
        [d] "r"(d),
        [rem_w] "rm"(rem_w)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

// Transpose rem_w (1..3) columns x 4 rows of 32-bit pixels using VEX 128-bit.
static void TransposeRemWx4_32_AVX2(const uint8_t* src,
                                    ptrdiff_t s,
                                    uint8_t* dst,
                                    ptrdiff_t d,
                                    int rem_w) {
  const uint8_t* temp;
  asm volatile(
      "cmpl         $2,%[rem_w]                    \n"
      "jl           1f                             \n"
      "je           2f                             \n"

      // rem_w == 3: load 12 bytes per row from 4 rows.
      "vmovq        (%[src]),%%xmm0                \n"
      "vpinsrd      $2,8(%[src]),%%xmm0,%%xmm0     \n"
      "vmovq        (%[src],%[s]),%%xmm1           \n"
      "vpinsrd      $2,8(%[src],%[s]),%%xmm1,%%xmm1 \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovq        (%[temp]),%%xmm2               \n"
      "vpinsrd      $2,8(%[temp]),%%xmm2,%%xmm2    \n"
      "vmovq        (%[temp],%[s]),%%xmm3          \n"
      "vpinsrd      $2,8(%[temp],%[s]),%%xmm3,%%xmm3 \n"
      "jmp          3f                             \n"

      // rem_w == 2: load 8 bytes per row from 4 rows.
      "2:                                          \n"
      "vmovq        (%[src]),%%xmm0                \n"
      "vmovq        (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovq        (%[temp]),%%xmm2               \n"
      "vmovq        (%[temp],%[s]),%%xmm3          \n"
      "jmp          3f                             \n"

      // rem_w == 1: load 4 bytes per row from 4 rows.
      "1:                                          \n"
      "vmovd        (%[src]),%%xmm0                \n"
      "vmovd        (%[src],%[s]),%%xmm1           \n"
      "lea          (%[src],%[s],2),%[temp]        \n"
      "vmovd        (%[temp]),%%xmm2               \n"
      "vmovd        (%[temp],%[s]),%%xmm3          \n"

      "3:                                          \n"
      "vpunpckldq   %%xmm1,%%xmm0,%%xmm4           \n"
      "vpunpckldq   %%xmm3,%%xmm2,%%xmm5           \n"
      "vpunpcklqdq  %%xmm5,%%xmm4,%%xmm6           \n"
      "vmovdqu      %%xmm6,(%[dst])                \n"
      "cmpl         $1,%[rem_w]                    \n"
      "jle          4f                             \n"
      "vpunpckhqdq  %%xmm5,%%xmm4,%%xmm7           \n"
      "vmovdqu      %%xmm7,(%[dst],%[d])           \n"
      "cmpl         $2,%[rem_w]                    \n"
      "jle          4f                             \n"
      "vpunpckhdq   %%xmm1,%%xmm0,%%xmm4           \n"
      "vpunpckhdq   %%xmm3,%%xmm2,%%xmm5           \n"
      "vpunpcklqdq  %%xmm5,%%xmm4,%%xmm6           \n"
      "vmovdqu      %%xmm6,(%[dst],%[d],2)         \n"
      "4:                                          \n"
      : [temp] "=&r"(temp)
      : [src] "r"(src),
        [s] "r"(s),
        [dst] "r"(dst),
        [d] "r"(d),
        [rem_w] "rm"(rem_w)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

// Transpose arbitrary WxH 32-bit plane using AVX2 (4x8, 8x4, 4x4, and 4x{1..3}
// tiles plus exact remainder handling).
void TransposeWxH_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width,
                          int height) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  const int w4 = width & ~3;
  const int rem_w = width & 3;

  // Tier 1: 8-row strips using 4x8 YMM tiles + rem_w x 8 YMM tail.
  while (height >= 8) {
    if (w4 > 0) {
      TransposeWx8_32_AVX2(src, src_stride, dst, dst_stride, w4);
    }
    if (rem_w > 0) {
      TransposeRemWx8_32_AVX2(src + (ptrdiff_t)w4 * 4, s,
                              dst + (ptrdiff_t)w4 * d, d, rem_w);
    }
    src += 8 * s;
    dst += 32;
    height -= 8;
  }

  // Tier 2: 4-row strip using 8x4 YMM tiles + 4x4 XMM block + rem_w x 4 tail.
  if (height >= 4) {
    const int w8 = width & ~7;
    if (w8 > 0) {
      const uint8_t* src_p = src;
      uint8_t* dst_p = dst;
      int cnt = w8;
      const uint8_t* temp;
      asm volatile(
          "1:                                          \n"
          "vmovdqu      (%[src]),%%ymm0                \n"
          "vmovdqu      (%[src],%[s]),%%ymm1           \n"
          "lea          (%[src],%[s],2),%[temp]        \n"
          "vmovdqu      (%[temp]),%%ymm2               \n"
          "vmovdqu      (%[temp],%[s]),%%ymm3          \n"
          "add          $0x20,%[src]                   \n"

          "vpunpckldq   %%ymm1,%%ymm0,%%ymm4           \n"
          "vpunpckldq   %%ymm3,%%ymm2,%%ymm5           \n"
          "vpunpckhdq   %%ymm1,%%ymm0,%%ymm6           \n"
          "vpunpckhdq   %%ymm3,%%ymm2,%%ymm7           \n"

          "vpunpcklqdq  %%ymm5,%%ymm4,%%ymm0           \n"
          "vpunpckhqdq  %%ymm5,%%ymm4,%%ymm1           \n"
          "vpunpcklqdq  %%ymm7,%%ymm6,%%ymm2           \n"
          "vpunpckhqdq  %%ymm7,%%ymm6,%%ymm3           \n"

          "vmovdqu      %%xmm0,(%[dst])                \n"
          "vmovdqu      %%xmm1,(%[dst],%[d])           \n"
          "lea          (%[dst],%[d],2),%[dst]         \n"
          "vmovdqu      %%xmm2,(%[dst])                \n"
          "vmovdqu      %%xmm3,(%[dst],%[d])           \n"
          "lea          (%[dst],%[d],2),%[dst]         \n"
          "vextracti128 $1,%%ymm0,(%[dst])             \n"
          "vextracti128 $1,%%ymm1,(%[dst],%[d])        \n"
          "lea          (%[dst],%[d],2),%[dst]         \n"
          "vextracti128 $1,%%ymm2,(%[dst])             \n"
          "vextracti128 $1,%%ymm3,(%[dst],%[d])        \n"
          "lea          (%[dst],%[d],2),%[dst]         \n"
          "subl         $0x8,%[cnt]                    \n"
          "jg           1b                             \n"
          "vzeroupper                                  \n"
          : [src] "+r"(src_p),
            [dst] "+r"(dst_p),
            [cnt] "+rm"(cnt),
            [temp] "=&r"(temp)
          : [s] "r"(s),
            [d] "r"(d)
          : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
            "xmm6", "xmm7");
    }
    if (width & 4) {
      const uint8_t* src_p = src + (ptrdiff_t)w8 * 4;
      uint8_t* dst_p = dst + (ptrdiff_t)w8 * d;
      uintptr_t temp;
      asm volatile(
          "vmovdqu      (%[src]),%%xmm0                \n"
          "vmovdqu      (%[src],%[s]),%%xmm1           \n"
          "lea          (%[src],%[s],2),%[temp]        \n"
          "vmovdqu      (%[temp]),%%xmm2               \n"
          "vmovdqu      (%[temp],%[s]),%%xmm3          \n"

          "vpunpckldq   %%xmm1,%%xmm0,%%xmm4           \n"
          "vpunpckldq   %%xmm3,%%xmm2,%%xmm5           \n"
          "vpunpckhdq   %%xmm1,%%xmm0,%%xmm6           \n"
          "vpunpckhdq   %%xmm3,%%xmm2,%%xmm7           \n"

          "vpunpcklqdq  %%xmm5,%%xmm4,%%xmm0           \n"
          "vpunpckhqdq  %%xmm5,%%xmm4,%%xmm1           \n"
          "vpunpcklqdq  %%xmm7,%%xmm6,%%xmm2           \n"
          "vpunpckhqdq  %%xmm7,%%xmm6,%%xmm3           \n"

          "vmovdqu      %%xmm0,(%[dst])                \n"
          "vmovdqu      %%xmm1,(%[dst],%[d])           \n"
          "lea          (%[dst],%[d],2),%[temp]        \n"
          "vmovdqu      %%xmm2,(%[temp])               \n"
          "vmovdqu      %%xmm3,(%[temp],%[d])          \n"
          : [temp] "=&r"(temp)
          : [src] "r"(src_p),
            [dst] "r"(dst_p),
            [s] "r"(s),
            [d] "r"(d)
          : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
            "xmm7");
    }
    if (rem_w > 0) {
      TransposeRemWx4_32_AVX2(src + (ptrdiff_t)w4 * 4, s,
                              dst + (ptrdiff_t)w4 * d, d, rem_w);
    }
    src += 4 * s;
    dst += 16;
    height -= 4;
  }

  // Tier 3: 1..3 remaining source rows using 4 x height XMM tiles + corner.
  if (height > 0) {
    if (w4 > 0) {
      const uint8_t* src_p = src;
      uint8_t* dst_p = dst;
      int cnt = w4;
      if (height == 1) {
        asm volatile(
            "1:                                          \n"
            "vmovdqu      (%[src]),%%xmm0                \n"
            "add          $0x10,%[src]                   \n"
            "vmovd        %%xmm0,(%[dst])                \n"
            "vpextrd      $1,%%xmm0,(%[dst],%[d])        \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "vpextrd      $2,%%xmm0,(%[dst])             \n"
            "vpextrd      $3,%%xmm0,(%[dst],%[d])        \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "sub          $0x4,%[cnt]                    \n"
            "jg           1b                             \n"
            : [src] "+r"(src_p),
              [dst] "+r"(dst_p),
              [cnt] "+r"(cnt)
            : [d] "r"(d)
            : "memory", "cc", "xmm0");
      } else if (height == 2) {
        asm volatile(
            "1:                                          \n"
            "vmovdqu      (%[src]),%%xmm0                \n"
            "vmovdqu      (%[src],%[s]),%%xmm1           \n"
            "add          $0x10,%[src]                   \n"
            "vpunpckldq   %%xmm1,%%xmm0,%%xmm2           \n"
            "vpunpckhdq   %%xmm1,%%xmm0,%%xmm3           \n"
            "vmovq        %%xmm2,(%[dst])                \n"
            "vmovhpd      %%xmm2,(%[dst],%[d])           \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "vmovq        %%xmm3,(%[dst])                \n"
            "vmovhpd      %%xmm3,(%[dst],%[d])           \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "sub          $0x4,%[cnt]                    \n"
            "jg           1b                             \n"
            : [src] "+r"(src_p),
              [dst] "+r"(dst_p),
              [cnt] "+r"(cnt)
            : [s] "r"(s),
              [d] "r"(d)
            : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
      } else {
        asm volatile(
            "vpxor        %%xmm3,%%xmm3,%%xmm3           \n"
            "1:                                          \n"
            "vmovdqu      (%[src]),%%xmm0                \n"
            "vmovdqu      (%[src],%[s]),%%xmm1           \n"
            "vmovdqu      (%[src],%[s],2),%%xmm2         \n"
            "add          $0x10,%[src]                   \n"
            "vpunpckldq   %%xmm1,%%xmm0,%%xmm4           \n"
            "vpunpckldq   %%xmm3,%%xmm2,%%xmm5           \n"
            "vpunpckhdq   %%xmm1,%%xmm0,%%xmm6           \n"
            "vpunpckhdq   %%xmm3,%%xmm2,%%xmm7           \n"
            "vpunpcklqdq  %%xmm5,%%xmm4,%%xmm0           \n"
            "vpunpckhqdq  %%xmm5,%%xmm4,%%xmm1           \n"
            "vpunpcklqdq  %%xmm7,%%xmm6,%%xmm2           \n"
            "vmovq        %%xmm0,(%[dst])                \n"
            "vpextrd      $2,%%xmm0,8(%[dst])            \n"
            "vmovq        %%xmm1,(%[dst],%[d])           \n"
            "vpextrd      $2,%%xmm1,8(%[dst],%[d])       \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "vmovq        %%xmm2,(%[dst])                \n"
            "vpextrd      $2,%%xmm2,8(%[dst])            \n"
            "vpunpckhqdq  %%xmm7,%%xmm6,%%xmm4           \n"
            "vmovq        %%xmm4,(%[dst],%[d])           \n"
            "vpextrd      $2,%%xmm4,8(%[dst],%[d])       \n"
            "lea          (%[dst],%[d],2),%[dst]         \n"
            "sub          $0x4,%[cnt]                    \n"
            "jg           1b                             \n"
            : [src] "+r"(src_p),
              [dst] "+r"(dst_p),
              [cnt] "+r"(cnt)
            : [s] "r"(s),
              [d] "r"(d)
            : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
              "xmm6", "xmm7");
      }
    }
    for (int x = w4; x < width; ++x) {
      uint8_t* dst_row = dst + (ptrdiff_t)x * d;
      for (int y = 0; y < height; ++y) {
        memcpy(dst_row + y * 4, src + (ptrdiff_t)y * s + x * 4, 4);
      }
    }
  }
}
#endif  // defined(HAS_TRANSPOSEWXH_32_AVX2)

#if defined(HAS_TRANSPOSEWX16_32_AVX512BW)
// Transpose 16 rows of 32 bit plane (arbitrary width) using 4x16 ZMM vertical
// tiles in zmm0..zmm7 with k1-masked right tail.
LIBYUV_TARGET_AVX512BW
void TransposeWx16_32_AVX512BW(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  const int n = width & ~3;
  if (n > 0) {
    int cnt = n;
    const uint8_t* temp;
    asm volatile(
        "1:                                           \n"
        "vmovdqu      (%[src]),%%xmm0                 \n"
        "vmovdqu      (%[src],%[s]),%%xmm1            \n"
        "lea          (%[src],%[s],2),%[temp]         \n"
        "vmovdqu      (%[temp]),%%xmm2                \n"
        "vmovdqu      (%[temp],%[s]),%%xmm3           \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"

        "vinserti32x4 $1,(%[temp]),%%zmm0,%%zmm0      \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"
        "vinserti32x4 $1,(%[temp]),%%zmm2,%%zmm2      \n"
        "vinserti32x4 $1,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"

        "vinserti32x4 $2,(%[temp]),%%zmm0,%%zmm0      \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"
        "vinserti32x4 $2,(%[temp]),%%zmm2,%%zmm2      \n"
        "vinserti32x4 $2,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"

        "vinserti32x4 $3,(%[temp]),%%zmm0,%%zmm0      \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm1,%%zmm1 \n"
        "lea          (%[temp],%[s],2),%[temp]        \n"
        "vinserti32x4 $3,(%[temp]),%%zmm2,%%zmm2      \n"
        "vinserti32x4 $3,(%[temp],%[s]),%%zmm3,%%zmm3 \n"
        "add          $0x10,%[src]                    \n"

        // Stage 1: 32-bit unpack across all 4 128-bit lanes
        "vpunpckldq   %%zmm1,%%zmm0,%%zmm4             \n"
        "vpunpckldq   %%zmm3,%%zmm2,%%zmm5             \n"
        "vpunpckhdq   %%zmm1,%%zmm0,%%zmm6             \n"
        "vpunpckhdq   %%zmm3,%%zmm2,%%zmm7             \n"

        // Stage 2: 64-bit unpack across all 4 128-bit lanes
        "vpunpcklqdq  %%zmm5,%%zmm4,%%zmm0             \n"
        "vpunpckhqdq  %%zmm5,%%zmm4,%%zmm1             \n"
        "vpunpcklqdq  %%zmm7,%%zmm6,%%zmm2             \n"
        "vpunpckhqdq  %%zmm7,%%zmm6,%%zmm3             \n"

        // Store 4 full 512-bit (64 B) rows = 16 32-bit pixels each
        "vmovdqu32    %%zmm0,(%[dst])                 \n"
        "vmovdqu32    %%zmm1,(%[dst],%[d])            \n"
        "lea          (%[dst],%[d],2),%[dst]          \n"
        "vmovdqu32    %%zmm2,(%[dst])                 \n"
        "vmovdqu32    %%zmm3,(%[dst],%[d])            \n"
        "lea          (%[dst],%[d],2),%[dst]          \n"
        "sub          $0x4,%[cnt]                     \n"
        "jg           1b                              \n"
        : [src] "+r"(src),    // %[src]
          [dst] "+r"(dst),    // %[dst]
          [cnt] "+r"(cnt),    // %[cnt]
          [temp] "=&r"(temp)  // %[temp]
        : [s] "r"(s),         // %[s]
          [d] "r"(d)          // %[d]
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7");
    width -= n;
  }
  if (width > 0) {
    const uint32_t col_mask = (1u << width) - 1;
    const uint8_t* temp;
    asm volatile(
        "kmovw        %[col_mask],%%k1                   \n"
        "vmovdqu32    (%[src]),%%xmm0%{%%k1%}%{z%}       \n"
        "vmovdqu32    (%[src],%[s]),%%xmm1%{%%k1%}%{z%}  \n"
        "lea          (%[src],%[s],2),%[temp]            \n"
        "vmovdqu32    (%[temp]),%%xmm2%{%%k1%}%{z%}      \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm3%{%%k1%}%{z%} \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $1,%%xmm8,%%zmm0,%%zmm0            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm8,%%zmm1,%%zmm1            \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $1,%%xmm8,%%zmm2,%%zmm2            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $1,%%xmm8,%%zmm3,%%zmm3            \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $2,%%xmm8,%%zmm0,%%zmm0            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm8,%%zmm1,%%zmm1            \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $2,%%xmm8,%%zmm2,%%zmm2            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $2,%%xmm8,%%zmm3,%%zmm3            \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"

        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $3,%%xmm8,%%zmm0,%%zmm0            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm8,%%zmm1,%%zmm1            \n"
        "lea          (%[temp],%[s],2),%[temp]           \n"
        "vmovdqu32    (%[temp]),%%xmm8%{%%k1%}%{z%}      \n"
        "vinserti32x4 $3,%%xmm8,%%zmm2,%%zmm2            \n"
        "vmovdqu32    (%[temp],%[s]),%%xmm8%{%%k1%}%{z%} \n"
        "vinserti32x4 $3,%%xmm8,%%zmm3,%%zmm3            \n"

        "vpunpckldq   %%zmm1,%%zmm0,%%zmm4               \n"
        "vpunpckldq   %%zmm3,%%zmm2,%%zmm5               \n"
        "vpunpckhdq   %%zmm1,%%zmm0,%%zmm6               \n"
        "vpunpckhdq   %%zmm3,%%zmm2,%%zmm7               \n"

        "vpunpcklqdq  %%zmm5,%%zmm4,%%zmm0               \n"
        "vpunpckhqdq  %%zmm5,%%zmm4,%%zmm1               \n"
        "vpunpcklqdq  %%zmm7,%%zmm6,%%zmm2               \n"
        "vpunpckhqdq  %%zmm7,%%zmm6,%%zmm3               \n"

        "vmovdqu32    %%zmm0,(%[dst])                    \n"
        "cmp          $1,%[width]                        \n"
        "jle          2f                                 \n"
        "vmovdqu32    %%zmm1,(%[dst],%[d])               \n"
        "cmp          $2,%[width]                        \n"
        "jle          2f                                 \n"
        "lea          (%[dst],%[d],2),%[dst]             \n"
        "vmovdqu32    %%zmm2,(%[dst])                    \n"
        "2:                                              \n"
        : [dst] "+r"(dst),           // %[dst]
          [temp] "=&r"(temp)         // %[temp]
        : [src] "r"(src),            // %[src]
          [s] "r"(s),                // %[s]
          [d] "r"(d),                // %[d]
          [col_mask] "r"(col_mask),  // %[col_mask]
          [width] "r"(width)         // %[width]
        : "memory", "cc", "k1", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7", "xmm8");
  }
  asm volatile("vzeroupper \n");
}
#endif  // defined(HAS_TRANSPOSEWX16_32_AVX512BW)

#endif  // !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) ||
        // defined(__i386__)) && !defined(LIBYUV_ENABLE_ROWWIN)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
