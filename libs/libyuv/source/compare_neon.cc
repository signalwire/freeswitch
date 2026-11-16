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

#if !defined(LIBYUV_DISABLE_NEON) && defined(__ARM_NEON__) && \
    !defined(__aarch64__)

// 256 bits at a time
// uses short accumulator which restricts count to 131 KB
uint32_t HammingDistance_NEON(const uint8_t* src_a,
                              const uint8_t* src_b,
                              int count) {
  uint32_t diff;

  asm volatile(
      "vmov.u16    q4, #0                        \n"  // accumulator

      "1:          \n"
      "vld1.8      {q0, q1}, [%0]!               \n"
      "vld1.8      {q2, q3}, [%1]!               \n"
      "veor.32     q0, q0, q2                    \n"
      "veor.32     q1, q1, q3                    \n"
      "vcnt.i8     q0, q0                        \n"
      "vcnt.i8     q1, q1                        \n"
      "subs        %2, %2, #32                   \n"
      "vadd.u8     q0, q0, q1                    \n"  // 16 byte counts
      "vpadal.u8   q4, q0                        \n"  // 8 shorts
      "bgt         1b                            \n"

      "vpaddl.u16  q0, q4                        \n"  // 4 ints
      "vpadd.u32   d0, d0, d1                    \n"
      "vpadd.u32   d0, d0, d0                    \n"
      "vmov.32     %3, d0[0]                     \n"

      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(diff)
      :
      : "cc", "q0", "q1", "q2", "q3", "q4");
  return diff;
}

uint32_t SumSquareError_NEON(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  uint32_t sse;
  asm volatile(
      "vmov.u8     q8, #0                        \n"
      "vmov.u8     q10, #0                       \n"
      "vmov.u8     q9, #0                        \n"
      "vmov.u8     q11, #0                       \n"

      "1:          \n"
      "vld1.8      {q0}, [%0]!                   \n"
      "vld1.8      {q1}, [%1]!                   \n"
      "subs        %2, %2, #16                   \n"
      "vsubl.u8    q2, d0, d2                    \n"
      "vsubl.u8    q3, d1, d3                    \n"
      "vmlal.s16   q8, d4, d4                    \n"
      "vmlal.s16   q9, d6, d6                    \n"
      "vmlal.s16   q10, d5, d5                   \n"
      "vmlal.s16   q11, d7, d7                   \n"
      "bgt         1b                            \n"

      "vadd.u32    q8, q8, q9                    \n"
      "vadd.u32    q10, q10, q11                 \n"
      "vadd.u32    q11, q8, q10                  \n"
      "vpaddl.u32  q1, q11                       \n"
      "vadd.u64    d0, d2, d3                    \n"
      "vmov.32     %3, d0[0]                     \n"
      : "+r"(src_a), "+r"(src_b), "+r"(count), "=r"(sse)
      :
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11");
  return sse;
}

#define SSIM_ROW_NEON32(LOAD_A, LOAD_B, PADD_U8, PADD_U16)         \
  LOAD_A                                                           \
  LOAD_B                                                           \
  PADD_U8 "    q8, q0                         \n" PADD_U8          \
          "    q9, q1                         \n"                  \
          "vmull.u8    q2, d0, d0                     \n"          \
          "vmull.u8    q3, d1, d1                     \n" PADD_U16 \
          "   q10, q2                        \n" PADD_U16          \
          "   q11, q3                        \n"                   \
          "vmull.u8    q2, d2, d2                     \n"          \
          "vmull.u8    q3, d3, d3                     \n"          \
          "vpadal.u16  q10, q2                        \n"          \
          "vpadal.u16  q11, q3                        \n"          \
          "vmull.u8    q2, d0, d2                     \n"          \
          "vmull.u8    q3, d1, d3                     \n" PADD_U16 \
          "   q12, q2                        \n" PADD_U16          \
          "   q13, q3                        \n"

void Ssim4x4Row_NEON(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b,
                     uint32_t* dst,
                     int width) {
  asm volatile(
      "1:                                        \n"  //
      SSIM_ROW_NEON32("vld1.8 {d0, d1}, [%[src_a]], %[stride_a]\n",
                      "vld1.8 {d2, d3}, [%[src_b]], %[stride_b]\n", "vpaddl.u8",
                      "vpaddl.u16")  //
      SSIM_ROW_NEON32("vld1.8 {d0, d1}, [%[src_a]], %[stride_a]\n",
                      "vld1.8 {d2, d3}, [%[src_b]], %[stride_b]\n", "vpadal.u8",
                      "vpadal.u16")  //
      SSIM_ROW_NEON32("vld1.8 {d0, d1}, [%[src_a]], %[stride_a]\n",
                      "vld1.8 {d2, d3}, [%[src_b]], %[stride_b]\n", "vpadal.u8",
                      "vpadal.u16")  //
      SSIM_ROW_NEON32("vld1.8 {d0, d1}, [%[src_a]]\n",
                      "vld1.8 {d2, d3}, [%[src_b]]\n", "vpadal.u8",
                      "vpadal.u16")  //
      "sub         %[src_a], %[src_a], %[stride_a], lsl #1 \n"
      "sub         %[src_a], %[src_a], %[stride_a]         \n"
      "add         %[src_a], %[src_a], #16                 \n"
      "sub         %[src_b], %[src_b], %[stride_b], lsl #1 \n"
      "sub         %[src_b], %[src_b], %[stride_b]         \n"
      "add         %[src_b], %[src_b], #16                 \n"
      "vpaddl.u16  q8, q8                        \n"
      "vpaddl.u16  q9, q9                        \n"
      "vpadd.u32   d20, d20, d21                 \n"
      "vpadd.u32   d21, d22, d23                 \n"
      "vpadd.u32   d22, d24, d25                 \n"
      "vpadd.u32   d23, d26, d27                 \n"
      "subs        %[width], %[width], #16       \n"
      "vst4.32     {d16, d18, d20, d22}, [%[dst]]! \n"
      "vst4.32     {d17, d19, d21, d23}, [%[dst]]! \n"
      "bgt         1b                            \n"
      : [src_a] "+r"(src_a), [src_b] "+r"(src_b), [dst] "+r"(dst),
        [width] "+r"(width)
      : [stride_a] "r"(stride_a), [stride_b] "r"(stride_b)
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12",
        "q13");
}
#undef SSIM_ROW_NEON32

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

uint32_t HashDjb2_NEON(const uint8_t* src, int count, uint32_t seed) {
  uint32_t hash = seed;
  const uint32_t c16 = 0x92d9e201;  // 33^16
  const void* muls0 = &kDjb2Multiplicands[0];
  const void* muls1 = &kDjb2Multiplicands[2];
  uint32_t tmp;
  asm volatile(
      "vld1.32     {d16, d17, d18, d19}, [%[muls0]] \n"
      "vld1.32     {d20, d21, d22, d23}, [%[muls1]] \n"
      "1:                                        \n"
      "vld1.8      {d0, d1}, [%[src]]!           \n"
      "subs        %[count], %[count], #16       \n"
      "vmovl.u8    q1, d0                        \n"
      "vmovl.u8    q2, d1                        \n"
      "vmovl.u16   q0, d2                        \n"
      "vmovl.u16   q1, d3                        \n"
      "vmovl.u16   q3, d5                        \n"
      "vmovl.u16   q2, d4                        \n"
      "vmul.i32    q0, q0, q8                    \n"
      "vmla.i32    q0, q1, q9                    \n"
      "vmla.i32    q0, q2, q10                   \n"
      "vmla.i32    q0, q3, q11                   \n"
      "vpadd.i32   d0, d0, d1                    \n"
      "vpadd.i32   d0, d0, d0                    \n"
      "vmov.32     %[tmp], d0[0]                 \n"
      "mla         %[hash], %[hash], %[c16], %[tmp] \n"
      "bgt         1b                            \n"
      :
      [src] "+r"(src), [hash] "+r"(hash), [count] "+r"(count), [tmp] "=&r"(tmp)
      : [muls0] "r"(muls0), [muls1] "r"(muls1), [c16] "r"(c16)
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11");
  return hash;
}

float SsimWindowRow_NEON(const uint32_t* row0,
                         const uint32_t* row1,
                         int num_windows) {
  const uint32_t* row0_next = row0 + 4;
  const uint32_t* row1_next = row1 + 4;
  float total;
  asm volatile(
      "vmov.i32    q12, #0                       \n"
      "vdup.32     q13, %[c1]                    \n"
      "vdup.32     q14, %[c2]                    \n"
      "1:                                        \n"
      "vld4.32     {d0, d2, d4, d6}, [%[row0]]!  \n"
      "vld4.32     {d1, d3, d5, d7}, [%[row0]]!  \n"
      "vld4.32     {d16, d18, d20, d22}, [%[row0_next]]! \n"
      "vld4.32     {d17, d19, d21, d23}, [%[row0_next]]! \n"
      "vadd.i32    q0, q0, q8                    \n"
      "vadd.i32    q1, q1, q9                    \n"
      "vadd.i32    q2, q2, q10                   \n"
      "vadd.i32    q3, q3, q11                   \n"
      "vld4.32     {d16, d18, d20, d22}, [%[row1]]! \n"
      "vld4.32     {d17, d19, d21, d23}, [%[row1]]! \n"
      "vadd.i32    q0, q0, q8                    \n"
      "vadd.i32    q1, q1, q9                    \n"
      "vadd.i32    q2, q2, q10                   \n"
      "vadd.i32    q3, q3, q11                   \n"
      "vld4.32     {d16, d18, d20, d22}, [%[row1_next]]! \n"
      "vld4.32     {d17, d19, d21, d23}, [%[row1_next]]! \n"
      "vadd.i32    q0, q0, q8                    \n"
      "vadd.i32    q1, q1, q9                    \n"
      "vadd.i32    q2, q2, q10                   \n"
      "vadd.i32    q3, q3, q11                   \n"
      "vmul.i32    q8, q0, q1                    \n"
      "vshl.i32    q8, q8, #1                    \n"
      "vmul.i32    q9, q0, q0                    \n"
      "vmla.i32    q9, q1, q1                    \n"
      "vshl.i32    q3, q3, #7                    \n"
      "vsub.i32    q3, q3, q8                    \n"
      "vadd.i32    q8, q8, q13                   \n"
      "vadd.i32    q3, q3, q14                   \n"
      "vshl.i32    q2, q2, #6                    \n"
      "vsub.i32    q2, q2, q9                    \n"
      "vadd.i32    q9, q9, q13                   \n"
      "vadd.i32    q2, q2, q14                   \n"
      "vcvt.f32.s32 q8, q8                       \n"
      "vcvt.f32.s32 q3, q3                       \n"
      "vcvt.f32.s32 q9, q9                       \n"
      "vcvt.f32.s32 q2, q2                       \n"
      "vmul.f32    q3, q8, q3                    \n"
      "vmul.f32    q2, q9, q2                    \n"
      "vdiv.f32    s12, s12, s8                  \n"
      "vdiv.f32    s13, s13, s9                  \n"
      "vdiv.f32    s14, s14, s10                 \n"
      "vdiv.f32    s15, s15, s11                 \n"
      "subs        %[num_windows], %[num_windows], #4 \n"
      "vadd.f32    q12, q12, q3                  \n"
      "bgt         1b                            \n"
      "vpadd.f32   d0, d24, d25                  \n"
      "vpadd.f32   d0, d0, d0                    \n"
      "vmov.f32    %[total], s0                  \n"
      : [row0] "+r"(row0), [row0_next] "+r"(row0_next), [row1] "+r"(row1),
        [row1_next] "+r"(row1_next), [num_windows] "+r"(num_windows),
        [total] "=r"(total)
      : [c1] "r"(26634), [c2] "r"(239708)
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12",
        "q13", "q14");
  return total;
}

uint32_t ARGBDetectRow_NEON(const uint8_t* argb, int width) {
  uint32_t diff_lo = 0;
  uint32_t diff_hi = 0;
  uint32_t tmp;
  asm volatile(
      "vmov.i32    q2, #0x00ff0000               \n"
      "vorr.i32    q2, #0x0000ff00               \n"
      "vmov.i8     q3, #0xff                     \n"
      "subs        %[width], %[width], #4        \n"
      "blt         2f                            \n"
      "1:                                        \n"
      "vld1.8      {q0}, [%[argb]]!              \n"
      "vorr        q0, q0, q2                    \n"
      "vceq.i8     q0, q0, q3                    \n"
      "vmvn        q0, q0                        \n"
      "vshrn.i16   d0, q0, #4                    \n"
      "vmov        %[diff_lo], %[diff_hi], d0    \n"
      "orrs        %[tmp], %[diff_lo], %[diff_hi] \n"
      "bne         99f                           \n"
      "subs        %[width], %[width], #4        \n"
      "bge         1b                            \n"
      "2:                                        \n"
      "adds        %[width], %[width], #4        \n"
      "ble         99f                           \n"
      "3:                                        \n"
      "vld1.32     {d0[0]}, [%[argb]]!           \n"
      "vorr        d0, d0, d4                    \n"
      "vceq.i8     d0, d0, d6                    \n"
      "vmvn        d0, d0                        \n"
      "vshrn.i16   d0, q0, #4                    \n"
      "vmov.u16    %[diff_lo], d0[0]             \n"
      "cmp         %[diff_lo], #0                \n"
      "bne         99f                           \n"
      "subs        %[width], %[width], #1        \n"
      "bgt         3b                            \n"
      "99:                                       \n"
      : [argb] "+r"(argb), [width] "+r"(width), [diff_lo] "+r"(diff_lo),
        [diff_hi] "+r"(diff_hi), [tmp] "=&r"(tmp)
      :
      : "memory", "cc", "q0", "q2", "q3");
  if (diff_lo) {
    return (__builtin_ctz(diff_lo) & 12) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
  }
  if (diff_hi) {
    return (__builtin_ctz(diff_hi) & 12) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
  }
  return 0;
}

#endif  // defined(__ARM_NEON__) && !defined(__aarch64__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
