/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
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

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#include <wasm_simd128.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Add the 4 32 bit lanes.
static inline uint32_t SumLanes_WASMSIMD(v128_t sum) {
  sum = wasm_i32x4_add(sum, wasm_i64x2_shuffle(sum, sum, 1, 0));
  sum = wasm_i32x4_add(sum, wasm_i32x4_shuffle(sum, sum, 1, 0, 3, 2));
  return wasm_u32x4_extract_lane(sum, 0);
}

#if defined(HAS_HAMMINGDISTANCE_WASMSIMD)
// Bit counts of 64 bytes are added as bytes, then accumulated into 32 bit
// lanes using relaxed i8x16 dot product when available or pairwise widening
// adds. Count is a multiple of 64.
uint32_t HammingDistance_WASMSIMD(const uint8_t* src_a,
                                  const uint8_t* src_b,
                                  int count) {
  v128_t sum = wasm_i32x4_splat(0);
#ifdef __wasm_relaxed_simd__
  const v128_t ones = wasm_i8x16_splat(1);
#endif
  while (count > 0) {
    v128_t c0 = wasm_i8x16_popcnt(
        wasm_v128_xor(wasm_v128_load(src_a), wasm_v128_load(src_b)));
    v128_t c1 = wasm_i8x16_popcnt(
        wasm_v128_xor(wasm_v128_load(src_a + 16), wasm_v128_load(src_b + 16)));
    v128_t c2 = wasm_i8x16_popcnt(
        wasm_v128_xor(wasm_v128_load(src_a + 32), wasm_v128_load(src_b + 32)));
    v128_t c3 = wasm_i8x16_popcnt(
        wasm_v128_xor(wasm_v128_load(src_a + 48), wasm_v128_load(src_b + 48)));
    v128_t c = wasm_i8x16_add(wasm_i8x16_add(c0, c1), wasm_i8x16_add(c2, c3));
#ifdef __wasm_relaxed_simd__
    sum = wasm_i32x4_relaxed_dot_i8x16_i7x16_add(c, ones, sum);
#else
    sum = wasm_i32x4_add(sum, wasm_u32x4_extadd_pairwise_u16x8(
                                  wasm_u16x8_extadd_pairwise_u8x16(c)));
#endif
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  return SumLanes_WASMSIMD(sum);
}
#endif  // HAS_HAMMINGDISTANCE_WASMSIMD

#if defined(HAS_SUMSQUAREERROR_WASMSIMD)
// Differences are widened to signed 16 bits and squared with i32x4.dot.
// Count is a multiple of 16.
uint32_t SumSquareError_WASMSIMD(const uint8_t* src_a,
                                 const uint8_t* src_b,
                                 int count) {
  v128_t sum = wasm_i32x4_splat(0);
  while (count > 0) {
    v128_t a = wasm_v128_load(src_a);
    v128_t b = wasm_v128_load(src_b);
    v128_t lo = wasm_i16x8_sub(wasm_u16x8_extend_low_u8x16(a),
                               wasm_u16x8_extend_low_u8x16(b));
    v128_t hi = wasm_i16x8_sub(wasm_u16x8_extend_high_u8x16(a),
                               wasm_u16x8_extend_high_u8x16(b));
    sum = wasm_i32x4_add(sum, wasm_i32x4_add(wasm_i32x4_dot_i16x8(lo, lo),
                                             wasm_i32x4_dot_i16x8(hi, hi)));
    src_a += 16;
    src_b += 16;
    count -= 16;
  }
  return SumLanes_WASMSIMD(sum);
}
#endif  // HAS_SUMSQUAREERROR_WASMSIMD

#if defined(HAS_HASHDJB2_WASMSIMD)
static const uint32_t kHashMul_WASMSIMD[24] = {
    // 33 ^ (30 - 2 * i) for byte pair i of 32 bytes.
    0xccc4cfc1, 0xd61beb81, 0x13791741, 0xac185301,  // 33 ^ 30 .. 24
    0xc8359ec1, 0x510cfa81, 0xb0da6641, 0x92d9e201,  // 33 ^ 22 .. 16
    0xa3476dc1, 0x4f5f0981, 0x855cb541, 0x747c7101,  // 33 ^ 14 .. 8
    0x4cfa3cc1, 0x00121881, 0x00000441, 0x00000001,  // 33 ^ 6 .. 0
    0x1137c401, 0x1137c401, 0x1137c401, 0x1137c401,  // 33 ^ 32
    0x00010021, 0x00010021, 0x00010021, 0x00010021,  // 16 bit 33, 1
};

// Sum of 16 bytes times 33 ^ n, as 4 lanes. Byte pairs are combined as
// src[2 * i] * 33 + src[2 * i + 1] with i32x4.dot, then multiplied by
// 33 ^ n for the pair.
static inline v128_t HashMul16_WASMSIMD(const uint8_t* src,
                                        const v128_t* m,
                                        v128_t k33) {
  v128_t s = wasm_v128_load(src);
  v128_t lo = wasm_i32x4_dot_i16x8(wasm_u16x8_extend_low_u8x16(s), k33);
  v128_t hi = wasm_i32x4_dot_i16x8(wasm_u16x8_extend_high_u8x16(s), k33);
  return wasm_i32x4_add(wasm_i32x4_mul(lo, m[0]), wasm_i32x4_mul(hi, m[1]));
}

// HashDjb2 of n bytes is hash * 33 ^ n + sum(src[i] * 33 ^ (n - 1 - i)).
// Products are summed per lane, multiplying the sums by 33 ^ 32 for each 32
// bytes, so one horizontal add is done at the end.
// Count is a multiple of 16.
uint32_t HashDjb2_WASMSIMD(const uint8_t* src, int count, uint32_t seed) {
  const v128_t* m = (const v128_t*)kHashMul_WASMSIMD;
  v128_t sum = wasm_i32x4_make(0, 0, 0, (int32_t)seed);
  // Load constants into Wasm locals outside the loop, instead of v128.const
  // in the loop.
  asm("" : "+r"(m));
  const v128_t k33_32 = m[4];
  const v128_t k33 = m[5];
  if (count & 16) {
    // Lane 3 of m[1] is 33 ^ 16, and lanes 0..2 of sum are 0.
    sum = wasm_i32x4_add(wasm_i32x4_mul(sum, m[1]),
                         HashMul16_WASMSIMD(src, m + 2, k33));
    src += 16;
    count -= 16;
  }
  while (count > 0) {
    sum = wasm_i32x4_add(
        wasm_i32x4_mul(sum, k33_32),
        wasm_i32x4_add(HashMul16_WASMSIMD(src, m, k33),
                       HashMul16_WASMSIMD(src + 16, m + 2, k33)));
    src += 32;
    count -= 32;
  }
  return SumLanes_WASMSIMD(sum);
}
#endif  // HAS_HASHDJB2_WASMSIMD

#if defined(HAS_SSIM4X4ROW_WASMSIMD)
void Ssim4x4Row_WASMSIMD(const uint8_t* src_a,
                         int stride_a,
                         const uint8_t* src_b,
                         int stride_b,
                         uint32_t* dst,
                         int width) {
  const uint8_t* src_a1 = src_a + stride_a;
  const uint8_t* src_a2 = src_a1 + stride_a;
  const uint8_t* src_a3 = src_a2 + stride_a;
  const uint8_t* src_b1 = src_b + stride_b;
  const uint8_t* src_b2 = src_b1 + stride_b;
  const uint8_t* src_b3 = src_b2 + stride_b;
  const v128_t mask = wasm_i16x8_splat(0x00ff);
  for (; width > 0; width -= 16) {
    v128_t a0 = wasm_v128_load(src_a);
    v128_t a1 = wasm_v128_load(src_a1);
    v128_t a2 = wasm_v128_load(src_a2);
    v128_t a3 = wasm_v128_load(src_a3);
    v128_t b0 = wasm_v128_load(src_b);
    v128_t b1 = wasm_v128_load(src_b1);
    v128_t b2 = wasm_v128_load(src_b2);
    v128_t b3 = wasm_v128_load(src_b3);

    v128_t a0_ev = wasm_v128_and(a0, mask);
    v128_t a0_od = wasm_u16x8_shr(a0, 8);
    v128_t a1_ev = wasm_v128_and(a1, mask);
    v128_t a1_od = wasm_u16x8_shr(a1, 8);
    v128_t a2_ev = wasm_v128_and(a2, mask);
    v128_t a2_od = wasm_u16x8_shr(a2, 8);
    v128_t a3_ev = wasm_v128_and(a3, mask);
    v128_t a3_od = wasm_u16x8_shr(a3, 8);

    v128_t b0_ev = wasm_v128_and(b0, mask);
    v128_t b0_od = wasm_u16x8_shr(b0, 8);
    v128_t b1_ev = wasm_v128_and(b1, mask);
    v128_t b1_od = wasm_u16x8_shr(b1, 8);
    v128_t b2_ev = wasm_v128_and(b2, mask);
    v128_t b2_od = wasm_u16x8_shr(b2, 8);
    v128_t b3_ev = wasm_v128_and(b3, mask);
    v128_t b3_od = wasm_u16x8_shr(b3, 8);

    v128_t sum_a_16 =
        wasm_i16x8_add(wasm_i16x8_add(wasm_i16x8_add(a0_ev, a0_od),
                                      wasm_i16x8_add(a1_ev, a1_od)),
                       wasm_i16x8_add(wasm_i16x8_add(a2_ev, a2_od),
                                      wasm_i16x8_add(a3_ev, a3_od)));
    v128_t sum_b_16 =
        wasm_i16x8_add(wasm_i16x8_add(wasm_i16x8_add(b0_ev, b0_od),
                                      wasm_i16x8_add(b1_ev, b1_od)),
                       wasm_i16x8_add(wasm_i16x8_add(b2_ev, b2_od),
                                      wasm_i16x8_add(b3_ev, b3_od)));

    v128_t sum_a = wasm_u32x4_extadd_pairwise_u16x8(sum_a_16);
    v128_t sum_b = wasm_u32x4_extadd_pairwise_u16x8(sum_b_16);

    v128_t sum_axb = wasm_i32x4_add(
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a0_ev, b0_ev),
                                      wasm_i32x4_dot_i16x8(a0_od, b0_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(a1_ev, b1_ev),
                                      wasm_i32x4_dot_i16x8(a1_od, b1_od))),
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a2_ev, b2_ev),
                                      wasm_i32x4_dot_i16x8(a2_od, b2_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(a3_ev, b3_ev),
                                      wasm_i32x4_dot_i16x8(a3_od, b3_od))));

    v128_t sq0 =
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a0_ev, a0_ev),
                                      wasm_i32x4_dot_i16x8(a0_od, a0_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(b0_ev, b0_ev),
                                      wasm_i32x4_dot_i16x8(b0_od, b0_od)));
    v128_t sq1 =
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a1_ev, a1_ev),
                                      wasm_i32x4_dot_i16x8(a1_od, a1_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(b1_ev, b1_ev),
                                      wasm_i32x4_dot_i16x8(b1_od, b1_od)));
    v128_t sq2 =
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a2_ev, a2_ev),
                                      wasm_i32x4_dot_i16x8(a2_od, a2_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(b2_ev, b2_ev),
                                      wasm_i32x4_dot_i16x8(b2_od, b2_od)));
    v128_t sq3 =
        wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_dot_i16x8(a3_ev, a3_ev),
                                      wasm_i32x4_dot_i16x8(a3_od, a3_od)),
                       wasm_i32x4_add(wasm_i32x4_dot_i16x8(b3_ev, b3_ev),
                                      wasm_i32x4_dot_i16x8(b3_od, b3_od)));
    v128_t sum_sq_ab =
        wasm_i32x4_add(wasm_i32x4_add(sq0, sq1), wasm_i32x4_add(sq2, sq3));

    v128_t t0 = wasm_i32x4_shuffle(sum_a, sum_b, 0, 4, 1, 5);
    v128_t t1 = wasm_i32x4_shuffle(sum_a, sum_b, 2, 6, 3, 7);
    v128_t t2 = wasm_i32x4_shuffle(sum_sq_ab, sum_axb, 0, 4, 1, 5);
    v128_t t3 = wasm_i32x4_shuffle(sum_sq_ab, sum_axb, 2, 6, 3, 7);

    wasm_v128_store(dst, wasm_i64x2_shuffle(t0, t2, 0, 2));
    wasm_v128_store(dst + 4, wasm_i64x2_shuffle(t0, t2, 1, 3));
    wasm_v128_store(dst + 8, wasm_i64x2_shuffle(t1, t3, 0, 2));
    wasm_v128_store(dst + 12, wasm_i64x2_shuffle(t1, t3, 1, 3));

    src_a += 16;
    src_a1 += 16;
    src_a2 += 16;
    src_a3 += 16;
    src_b += 16;
    src_b1 += 16;
    src_b2 += 16;
    src_b3 += 16;
    dst += 16;
  }
}
#endif  // HAS_SSIM4X4ROW_WASMSIMD

#if defined(HAS_SSIMWINDOWROW_WASMSIMD)
float SsimWindowRow_WASMSIMD(const uint32_t* row0,
                             const uint32_t* row1,
                             int num_windows) {
  const v128_t vc1 = wasm_i32x4_splat(26634);
  const v128_t vc2 = wasm_i32x4_splat(239708);
  v128_t total = wasm_f32x4_splat(0.0f);
  v128_t t0 = wasm_i32x4_add(wasm_v128_load(row0), wasm_v128_load(row1));
  int w;
  for (w = 0; w < num_windows; w += 4) {
    v128_t t1 =
        wasm_i32x4_add(wasm_v128_load(row0 + 4), wasm_v128_load(row1 + 4));
    v128_t t2 =
        wasm_i32x4_add(wasm_v128_load(row0 + 8), wasm_v128_load(row1 + 8));
    v128_t t3 =
        wasm_i32x4_add(wasm_v128_load(row0 + 12), wasm_v128_load(row1 + 12));
    v128_t t4 =
        wasm_i32x4_add(wasm_v128_load(row0 + 16), wasm_v128_load(row1 + 16));
    row0 += 16;
    row1 += 16;
    v128_t w0 = wasm_i32x4_add(t0, t1);
    v128_t w1 = wasm_i32x4_add(t1, t2);
    v128_t w2 = wasm_i32x4_add(t2, t3);
    v128_t w3 = wasm_i32x4_add(t3, t4);
    t0 = t4;

    v128_t u0 = wasm_i32x4_shuffle(w0, w1, 0, 4, 1, 5);
    v128_t u1 = wasm_i32x4_shuffle(w0, w1, 2, 6, 3, 7);
    v128_t u2 = wasm_i32x4_shuffle(w2, w3, 0, 4, 1, 5);
    v128_t u3 = wasm_i32x4_shuffle(w2, w3, 2, 6, 3, 7);
    v128_t sum_a = wasm_i64x2_shuffle(u0, u2, 0, 2);
    v128_t sum_b = wasm_i64x2_shuffle(u0, u2, 1, 3);
    v128_t sum_sq = wasm_i64x2_shuffle(u1, u3, 0, 2);
    v128_t sum_axb = wasm_i64x2_shuffle(u1, u3, 1, 3);

    v128_t ab2 = wasm_i32x4_shl(wasm_i32x4_dot_i16x8(sum_a, sum_b), 1);
    v128_t a2b2 = wasm_i32x4_add(wasm_i32x4_dot_i16x8(sum_a, sum_a),
                                 wasm_i32x4_dot_i16x8(sum_b, sum_b));
    v128_t n1 = wasm_i32x4_add(ab2, vc1);
    v128_t n2 =
        wasm_i32x4_add(wasm_i32x4_sub(wasm_i32x4_shl(sum_axb, 7), ab2), vc2);
    v128_t d1 = wasm_i32x4_add(a2b2, vc1);
    v128_t d2 =
        wasm_i32x4_add(wasm_i32x4_sub(wasm_i32x4_shl(sum_sq, 6), a2b2), vc2);

    v128_t num = wasm_f32x4_mul(wasm_f32x4_convert_i32x4(n1),
                                wasm_f32x4_convert_i32x4(n2));
    v128_t den = wasm_f32x4_mul(wasm_f32x4_convert_i32x4(d1),
                                wasm_f32x4_convert_i32x4(d2));
    total = wasm_f32x4_add(total, wasm_f32x4_div(num, den));
  }
  total = wasm_f32x4_add(total, wasm_i64x2_shuffle(total, total, 1, 0));
  total = wasm_f32x4_add(total, wasm_i32x4_shuffle(total, total, 1, 0, 3, 2));
  return wasm_f32x4_extract_lane(total, 0);
}
#endif  // HAS_SSIMWINDOWROW_WASMSIMD

#if defined(HAS_ARGBDETECTROW_WASMSIMD)
uint32_t ARGBDetectRow_WASMSIMD(const uint8_t* argb, int width) {
  const v128_t mask_rgb = wasm_i32x4_splat(0x00ffff00);
  const v128_t ones = wasm_i8x16_splat(-1);
  while (width >= 4) {
    v128_t v = wasm_v128_or(wasm_v128_load(argb), mask_rgb);
    uint32_t diff =
        (~(uint32_t)wasm_i8x16_bitmask(wasm_i8x16_eq(v, ones))) & 0xffffu;
    if (diff) {
      return (__builtin_ctz(diff) & 3) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
    }
    argb += 16;
    width -= 4;
  }
  while (width > 0) {
    v128_t v = wasm_v128_or(wasm_v128_load32_zero(argb), mask_rgb);
    uint32_t diff =
        (~(uint32_t)wasm_i8x16_bitmask(wasm_i8x16_eq(v, ones))) & 0x9u;
    if (diff) {
      return (diff & 1u) ? FOURCC_BGRA : FOURCC_ARGB;
    }
    argb += 4;
    width -= 1;
  }
  return 0;
}
#endif  // HAS_ARGBDETECTROW_WASMSIMD

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
