/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
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

#if !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)

// 256 bits at a time
// uses short accumulator which restricts count to 131 KB
uint32_t HammingDistance_NEON(const uint8_t* src_a,
                              const uint8_t* src_b,
                              int count) {
  uint32_t diff;
  asm volatile(
      "movi        v4.8h, #0                     \n"

      "1:          \n"
      "ld1         {v0.16b, v1.16b}, [%0], #32   \n"
      "ld1         {v2.16b, v3.16b}, [%1], #32   \n"
      "eor         v0.16b, v0.16b, v2.16b        \n"
      "prfm        pldl1keep, [%0, 448]          \n"  // prefetch 7 lines ahead
      "eor         v1.16b, v1.16b, v3.16b        \n"
      "cnt         v0.16b, v0.16b                \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "cnt         v1.16b, v1.16b                \n"
      "subs        %w2, %w2, #32                 \n"
      "add         v0.16b, v0.16b, v1.16b        \n"
      "uadalp      v4.8h, v0.16b                 \n"
      "b.gt        1b                            \n"

      "uaddlv      s4, v4.8h                     \n"
      "fmov        %w3, s4                       \n"
      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(diff)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4");
  return diff;
}

uint32_t SumSquareError_NEON(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  uint32_t sse;
  asm volatile(
      "movi        v16.16b, #0                   \n"
      "movi        v17.16b, #0                   \n"
      "movi        v18.16b, #0                   \n"
      "movi        v19.16b, #0                   \n"

      "1:          \n"
      "ld1         {v0.16b}, [%0], #16           \n"
      "ld1         {v1.16b}, [%1], #16           \n"
      "subs        %w2, %w2, #16                 \n"
      "usubl       v2.8h, v0.8b, v1.8b           \n"
      "usubl2      v3.8h, v0.16b, v1.16b         \n"
      "prfm        pldl1keep, [%0, 448]          \n"  // prefetch 7 lines ahead
      "smlal       v16.4s, v2.4h, v2.4h          \n"
      "smlal       v17.4s, v3.4h, v3.4h          \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "smlal2      v18.4s, v2.8h, v2.8h          \n"
      "smlal2      v19.4s, v3.8h, v3.8h          \n"
      "b.gt        1b                            \n"

      "add         v16.4s, v16.4s, v17.4s        \n"
      "add         v18.4s, v18.4s, v19.4s        \n"
      "add         v19.4s, v16.4s, v18.4s        \n"
      "addv        s0, v19.4s                    \n"
      "fmov        %w3, s0                       \n"
      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(sse)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19");
  return sse;
}

static const uvec32 kDjb2Multiplicands[] = {
    {0x0c3525e1,   // 33^15
     0xa3476dc1,   // 33^14
     0x3b4039a1,   // 33^13
     0x4f5f0981},  // 33^12
    {0x30f35d61,   // 33^11
     0x855cb541,   // 33^10
     0x040a9121,   // 33^9
     0x747c7101},  // 33^8
    {0xec41d4e1,   // 33^7
     0x4cfa3cc1,   // 33^6
     0x025528a1,   // 33^5
     0x00121881},  // 33^4
    {0x00008c61,   // 33^3
     0x00000441,   // 33^2
     0x00000021,   // 33^1
     0x00000001},  // 33^0
};

static const uvec32 kDjb2WidenIndices[] = {
    {0xffffff00U, 0xffffff01U, 0xffffff02U, 0xffffff03U},
    {0xffffff04U, 0xffffff05U, 0xffffff06U, 0xffffff07U},
    {0xffffff08U, 0xffffff09U, 0xffffff0aU, 0xffffff0bU},
    {0xffffff0cU, 0xffffff0dU, 0xffffff0eU, 0xffffff0fU},
};

uint32_t HashDjb2_NEON(const uint8_t* src, int count, uint32_t seed) {
  uint32_t hash = seed;
  const uint32_t c16 = 0x92d9e201;  // 33^16
  uint32_t tmp, tmp2;
  asm("ld1         {v16.4s, v17.4s, v18.4s, v19.4s}, [%[kIdx]] \n"
      "ld1         {v4.4s, v5.4s, v6.4s, v7.4s}, [%[kMuls]] \n"

      // count is always a multiple of 16.
      // maintain two accumulators, reduce and then final sum in scalar since
      // this has better performance on little cores.
      "1:          \n"
      "ldr         q0, [%[src]], #16             \n"
      "subs        %w[count], %w[count], #16     \n"
      "tbl         v3.16b, {v0.16b}, v19.16b     \n"
      "tbl         v2.16b, {v0.16b}, v18.16b     \n"
      "tbl         v1.16b, {v0.16b}, v17.16b     \n"
      "tbl         v0.16b, {v0.16b}, v16.16b     \n"
      "mul         v3.4s, v3.4s, v7.4s           \n"
      "mul         v2.4s, v2.4s, v6.4s           \n"
      "mla         v3.4s, v1.4s, v5.4s           \n"
      "mla         v2.4s, v0.4s, v4.4s           \n"
      "addv        s1, v3.4s                     \n"
      "addv        s0, v2.4s                     \n"
      "fmov        %w[tmp2], s1                  \n"
      "fmov        %w[tmp], s0                   \n"
      "add         %w[tmp], %w[tmp], %w[tmp2]    \n"
      "madd        %w[hash], %w[hash], %w[c16], %w[tmp] \n"
      "b.gt        1b                            \n"
      : [src] "+r"(src),                  // %[src]
        [hash] "+r"(hash),                // %[hash]
        [count] "+r"(count),              // %[count]
        [tmp] "=&r"(tmp),                 // %[tmp]
        [tmp2] "=&r"(tmp2)                // %[tmp2]
      : [kMuls] "r"(kDjb2Multiplicands),  // %[kMuls]
        [kIdx] "r"(kDjb2WidenIndices),    // %[kIdx]
        [c16] "r"(c16)                    // %[c16]
      : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16",
        "v17", "v18", "v19");
  return hash;
}

uint32_t HammingDistance_NEON_DotProd(const uint8_t* src_a,
                                      const uint8_t* src_b,
                                      int count) {
  uint32_t diff;
  asm volatile(
      "movi        v4.4s, #0                     \n"
      "movi        v5.4s, #0                     \n"
      "movi        v6.16b, #1                    \n"

      "1:          \n"
      "ldp         q0, q1, [%0], #32             \n"
      "ldp         q2, q3, [%1], #32             \n"
      "eor         v0.16b, v0.16b, v2.16b        \n"
      "prfm        pldl1keep, [%0, 448]          \n"  // prefetch 7 lines ahead
      "eor         v1.16b, v1.16b, v3.16b        \n"
      "cnt         v0.16b, v0.16b                \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "cnt         v1.16b, v1.16b                \n"
      "subs        %w2, %w2, #32                 \n"
      "udot        v4.4s, v0.16b, v6.16b         \n"
      "udot        v5.4s, v1.16b, v6.16b         \n"
      "b.gt        1b                            \n"

      "add         v0.4s, v4.4s, v5.4s           \n"
      "addv        s0, v0.4s                     \n"
      "fmov        %w3, s0                       \n"
      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(diff)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6");
  return diff;
}

uint32_t SumSquareError_NEON_DotProd(const uint8_t* src_a,
                                     const uint8_t* src_b,
                                     int count) {
  // count is guaranteed to be a multiple of 32.
  uint32_t sse;
  asm volatile(
      "movi        v4.4s, #0                     \n"
      "movi        v5.4s, #0                     \n"

      "1:          \n"
      "ldp         q0, q2, [%0], #32             \n"
      "ldp         q1, q3, [%1], #32             \n"
      "subs        %w2, %w2, #32                 \n"
      "uabd        v0.16b, v0.16b, v1.16b        \n"
      "uabd        v1.16b, v2.16b, v3.16b        \n"
      "prfm        pldl1keep, [%0, 448]          \n"  // prefetch 7 lines ahead
      "udot        v4.4s, v0.16b, v0.16b         \n"
      "udot        v5.4s, v1.16b, v1.16b         \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "b.gt        1b                            \n"

      "add         v0.4s, v4.4s, v5.4s           \n"
      "addv        s0, v0.4s                     \n"
      "fmov        %w3, s0                       \n"
      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(sse)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5");
  return sse;
}

#define SSIM_ROW_NEON64(VA, VB, PADD_U8, PADD_U16)                      \
  PADD_U8 "    v4.8h, " #VA ".16b                  \n" PADD_U8          \
          "    v5.8h, " #VB                                             \
          ".16b                  \n"                                    \
          "umull       v6.8h, " #VA ".8b, " #VA                         \
          ".8b       \n"                                                \
          "umull2      v7.8h, " #VA ".16b, " #VA ".16b     \n" PADD_U16 \
          "   v20.4s, v6.8h                       \n" PADD_U16          \
          "   v21.4s, v7.8h                       \n"                   \
          "umull       v6.8h, " #VB ".8b, " #VB                         \
          ".8b       \n"                                                \
          "umull2      v7.8h, " #VB ".16b, " #VB                        \
          ".16b     \n"                                                 \
          "uadalp      v20.4s, v6.8h                       \n"          \
          "uadalp      v21.4s, v7.8h                       \n"          \
          "umull       v6.8h, " #VA ".8b, " #VB                         \
          ".8b       \n"                                                \
          "umull2      v7.8h, " #VA ".16b, " #VB ".16b     \n" PADD_U16 \
          "   v22.4s, v6.8h                       \n" PADD_U16          \
          "   v23.4s, v7.8h                       \n"

void Ssim4x4Row_NEON(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b,
                     uint32_t* dst,
                     int width) {
  const ptrdiff_t sa = stride_a;
  const ptrdiff_t sb = stride_b;
  const uint8_t* a0 = src_a;
  const uint8_t* a1 = a0 + sa;
  const uint8_t* a2 = a1 + sa;
  const uint8_t* a3 = a2 + sa;
  const uint8_t* b0 = src_b;
  const uint8_t* b1 = b0 + sb;
  const uint8_t* b2 = b1 + sb;
  const uint8_t* b3 = b2 + sb;
  asm volatile(
      "1:                                        \n"
      "ldr         q0, [%[a0]], #16              \n"
      "ldr         q1, [%[b0]], #16              \n"
      "ldr         q2, [%[a1]], #16              \n"
      "ldr         q3, [%[b1]], #16              \n"  //
      SSIM_ROW_NEON64(v0, v1, "uaddlp", "uaddlp")     //
      SSIM_ROW_NEON64(v2, v3, "uadalp", "uadalp")     //
      "ldr         q0, [%[a2]], #16              \n"
      "ldr         q1, [%[b2]], #16              \n"
      "ldr         q2, [%[a3]], #16              \n"
      "ldr         q3, [%[b3]], #16              \n"  //
      SSIM_ROW_NEON64(v0, v1, "uadalp", "uadalp")     //
      SSIM_ROW_NEON64(v2, v3, "uadalp", "uadalp")     //
      "uaddlp      v16.4s, v4.8h                 \n"
      "uaddlp      v17.4s, v5.8h                 \n"
      "addp        v18.4s, v20.4s, v21.4s        \n"
      "addp        v19.4s, v22.4s, v23.4s        \n"
      "subs        %w[width], %w[width], #16     \n"
      "st4         {v16.4s, v17.4s, v18.4s, v19.4s}, [%[dst]], #64 \n"
      "b.gt        1b                            \n"
      : [a0] "+r"(a0), [a1] "+r"(a1), [a2] "+r"(a2), [a3] "+r"(a3),
        [b0] "+r"(b0), [b1] "+r"(b1), [b2] "+r"(b2), [b3] "+r"(b3),
        [dst] "+r"(dst), [width] "+r"(width)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#undef SSIM_ROW_NEON64

void Ssim4x4Row_NEON_DotProd(const uint8_t* src_a,
                             int stride_a,
                             const uint8_t* src_b,
                             int stride_b,
                             uint32_t* dst,
                             int width) {
  const ptrdiff_t sa = stride_a;
  const ptrdiff_t sb = stride_b;
  const uint8_t* a0 = src_a;
  const uint8_t* a1 = a0 + sa;
  const uint8_t* a2 = a1 + sa;
  const uint8_t* a3 = a2 + sa;
  const uint8_t* b0 = src_b;
  const uint8_t* b1 = b0 + sb;
  const uint8_t* b2 = b1 + sb;
  const uint8_t* b3 = b2 + sb;
  asm volatile(
      "movi        v6.16b, #1                    \n"
      "1:                                        \n"
      "movi        v16.4s, #0                    \n"
      "movi        v17.4s, #0                    \n"
      "movi        v18.4s, #0                    \n"
      "movi        v19.4s, #0                    \n"
      "ldr         q0, [%[a0]], #16              \n"
      "ldr         q1, [%[b0]], #16              \n"
      "ldr         q2, [%[a1]], #16              \n"
      "ldr         q3, [%[b1]], #16              \n"
      "udot        v16.4s, v0.16b, v6.16b        \n"
      "udot        v17.4s, v1.16b, v6.16b        \n"
      "udot        v18.4s, v0.16b, v0.16b        \n"
      "udot        v18.4s, v1.16b, v1.16b        \n"
      "udot        v19.4s, v0.16b, v1.16b        \n"
      "udot        v16.4s, v2.16b, v6.16b        \n"
      "udot        v17.4s, v3.16b, v6.16b        \n"
      "udot        v18.4s, v2.16b, v2.16b        \n"
      "udot        v18.4s, v3.16b, v3.16b        \n"
      "udot        v19.4s, v2.16b, v3.16b        \n"
      "ldr         q0, [%[a2]], #16              \n"
      "ldr         q1, [%[b2]], #16              \n"
      "ldr         q2, [%[a3]], #16              \n"
      "ldr         q3, [%[b3]], #16              \n"
      "udot        v16.4s, v0.16b, v6.16b        \n"
      "udot        v17.4s, v1.16b, v6.16b        \n"
      "udot        v18.4s, v0.16b, v0.16b        \n"
      "udot        v18.4s, v1.16b, v1.16b        \n"
      "udot        v19.4s, v0.16b, v1.16b        \n"
      "udot        v16.4s, v2.16b, v6.16b        \n"
      "udot        v17.4s, v3.16b, v6.16b        \n"
      "udot        v18.4s, v2.16b, v2.16b        \n"
      "udot        v18.4s, v3.16b, v3.16b        \n"
      "udot        v19.4s, v2.16b, v3.16b        \n"
      "subs        %w[width], %w[width], #16     \n"
      "st4         {v16.4s, v17.4s, v18.4s, v19.4s}, [%[dst]], #64 \n"
      "b.gt        1b                            \n"
      : [a0] "+r"(a0), [a1] "+r"(a1), [a2] "+r"(a2), [a3] "+r"(a3),
        [b0] "+r"(b0), [b1] "+r"(b1), [b2] "+r"(b2), [b3] "+r"(b3),
        [dst] "+r"(dst), [width] "+r"(width)
      :
      : "memory", "cc", "v0", "v1", "v2", "v3", "v6", "v16", "v17", "v18",
        "v19");
}

float SsimWindowRow_NEON(const uint32_t* row0,
                         const uint32_t* row1,
                         int num_windows) {
  const uint32_t* row0_next = row0 + 4;
  const uint32_t* row1_next = row1 + 4;
  float total;
  asm volatile(
      "movi        v24.4s, #0                    \n"
      "dup         v25.4s, %w[c1]                \n"
      "dup         v26.4s, %w[c2]                \n"
      "1:                                        \n"
      "ld4         {v0.4s, v1.4s, v2.4s, v3.4s}, [%[row0]], #64 \n"
      "ld4         {v4.4s, v5.4s, v6.4s, v7.4s}, [%[row0_next]], #64 \n"
      "ld4         {v16.4s, v17.4s, v18.4s, v19.4s}, [%[row1]], #64 \n"
      "ld4         {v20.4s, v21.4s, v22.4s, v23.4s}, [%[row1_next]], #64 \n"
      "add         v0.4s, v0.4s, v4.4s           \n"
      "add         v1.4s, v1.4s, v5.4s           \n"
      "add         v2.4s, v2.4s, v6.4s           \n"
      "add         v3.4s, v3.4s, v7.4s           \n"
      "add         v16.4s, v16.4s, v20.4s        \n"
      "add         v17.4s, v17.4s, v21.4s        \n"
      "add         v18.4s, v18.4s, v22.4s        \n"
      "add         v19.4s, v19.4s, v23.4s        \n"
      "add         v0.4s, v0.4s, v16.4s          \n"
      "add         v1.4s, v1.4s, v17.4s          \n"
      "add         v2.4s, v2.4s, v18.4s          \n"
      "add         v3.4s, v3.4s, v19.4s          \n"
      "mul         v4.4s, v0.4s, v1.4s           \n"
      "shl         v4.4s, v4.4s, #1              \n"
      "mul         v5.4s, v0.4s, v0.4s           \n"
      "mla         v5.4s, v1.4s, v1.4s           \n"
      "shl         v3.4s, v3.4s, #7              \n"
      "sub         v3.4s, v3.4s, v4.4s           \n"
      "add         v4.4s, v4.4s, v25.4s          \n"
      "add         v3.4s, v3.4s, v26.4s          \n"
      "shl         v2.4s, v2.4s, #6              \n"
      "sub         v2.4s, v2.4s, v5.4s           \n"
      "add         v5.4s, v5.4s, v25.4s          \n"
      "add         v2.4s, v2.4s, v26.4s          \n"
      "scvtf       v4.4s, v4.4s                  \n"
      "scvtf       v3.4s, v3.4s                  \n"
      "scvtf       v5.4s, v5.4s                  \n"
      "scvtf       v2.4s, v2.4s                  \n"
      "fmul        v4.4s, v4.4s, v3.4s           \n"
      "fmul        v5.4s, v5.4s, v2.4s           \n"
      "fdiv        v4.4s, v4.4s, v5.4s           \n"
      "subs        %w[num_windows], %w[num_windows], #4 \n"
      "fadd        v24.4s, v24.4s, v4.4s         \n"
      "b.gt        1b                            \n"
      "faddp       v24.4s, v24.4s, v24.4s        \n"
      "faddp       %s[total], v24.2s             \n"
      : [row0] "+r"(row0), [row0_next] "+r"(row0_next), [row1] "+r"(row1),
        [row1_next] "+r"(row1_next), [num_windows] "+r"(num_windows),
        [total] "=w"(total)
      : [c1] "r"(26634), [c2] "r"(239708)
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26");
  return total;
}

uint32_t ARGBDetectRow_NEON(const uint8_t* argb, int width) {
  uint64_t diff = 0;
  asm volatile(
      "movi        v2.2d, #0x00ffff0000ffff00    \n"
      "movi        v3.16b, #0xff                 \n"
      "subs        %w[width], %w[width], #4      \n"
      "b.lt        2f                            \n"
      "1:                                        \n"
      "ldr         q0, [%[argb]], #16            \n"
      "orr         v0.16b, v0.16b, v2.16b        \n"
      "cmeq        v0.16b, v0.16b, v3.16b        \n"
      "mvn         v0.16b, v0.16b                \n"
      "shrn        v0.8b, v0.8h, #4              \n"
      "fmov        %x[diff], d0                  \n"
      "cbnz        %x[diff], 99f                 \n"
      "subs        %w[width], %w[width], #4      \n"
      "b.ge        1b                            \n"
      "2:                                        \n"
      "adds        %w[width], %w[width], #4      \n"
      "b.le        99f                           \n"
      "3:                                        \n"
      "ldr         s0, [%[argb]], #4             \n"
      "orr         v0.8b, v0.8b, v2.8b           \n"
      "cmeq        v0.8b, v0.8b, v3.8b           \n"
      "mvn         v0.8b, v0.8b                  \n"
      "shrn        v0.8b, v0.8h, #4              \n"
      "fmov        %x[diff], d0                  \n"
      "and         %x[diff], %x[diff], #0xffff   \n"
      "cbnz        %x[diff], 99f                 \n"
      "subs        %w[width], %w[width], #1      \n"
      "b.gt        3b                            \n"
      "99:                                       \n"
      : [argb] "+r"(argb), [width] "+r"(width), [diff] "+r"(diff)
      :
      : "memory", "cc", "v0", "v2", "v3");
  if (diff) {
    return (__builtin_ctzll(diff) & 12) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
  }
  return 0;
}

#endif  // !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
