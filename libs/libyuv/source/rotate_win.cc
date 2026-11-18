/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

// This module is for Visual C 32/64 bit, and for clang with
// LIBYUV_ENABLE_ROWWIN, which is also enabled for MemorySanitizer.
#if !defined(LIBYUV_DISABLE_X86) &&                                 \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
     defined(_M_IX86)) &&                                           \
    ((defined(_MSC_VER) && !defined(__clang__)) ||                  \
     defined(LIBYUV_ENABLE_ROWWIN))

#include <emmintrin.h>
#include <immintrin.h>  // For AVX2 and AVX512 intrinsics
#include <string.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Target attributes allow gcc and clang to build each function for its
// instruction set regardless of the baseline, which may lack SSE2 for 32 bit.
#if defined(__clang__) || defined(__GNUC__)
#define LIBYUV_TARGET_AVX2 __attribute__((target("avx2")))
#define LIBYUV_TARGET_AVX512BW \
  __attribute__((target("avx512bw,avx512vl,avx512f")))
#else
#define LIBYUV_TARGET_AVX2
#define LIBYUV_TARGET_AVX512BW
#endif  // defined(__clang__) || defined(__GNUC__)

#if defined(HAS_TRANSPOSEWX8_AVX2) || defined(HAS_TRANSPOSEUVWX8_AVX2)
// Transpose 8 rows x 16 bytes using 2 lanes of AVX2. Source row r is in
// lane r / 4 of register r % 4. Each 128-bit output lane holds 2 columns x 8
// rows: low 8 bytes (even column) for dst_a and high 8 bytes (odd column) for
// dst_b. Width is a multiple of 16 bytes.
LIBYUV_TARGET_AVX2
static void TransposeWx8_Byte_AVX2(const uint8_t* src,
                                   int src_stride,
                                   uint8_t* dst_a,
                                   int dst_stride_a,
                                   uint8_t* dst_b,
                                   int dst_stride_b,
                                   int byte_width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t sa = dst_stride_a;
  const ptrdiff_t sb = dst_stride_b;
  while (byte_width > 0) {
    __m256i r0 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src))),
        _mm_loadu_si128((const __m128i*)(src + s * 4)), 1);
    __m256i r1 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s))),
        _mm_loadu_si128((const __m128i*)(src + s * 5)), 1);
    __m256i r2 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 2))),
        _mm_loadu_si128((const __m128i*)(src + s * 6)), 1);
    __m256i r3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 3))),
        _mm_loadu_si128((const __m128i*)(src + s * 7)), 1);

    __m256i a0 = _mm256_unpacklo_epi8(r0, r1);
    __m256i a1 = _mm256_unpackhi_epi8(r0, r1);
    __m256i a2 = _mm256_unpacklo_epi8(r2, r3);
    __m256i a3 = _mm256_unpackhi_epi8(r2, r3);

    __m256i b0 = _mm256_unpacklo_epi16(a0, a2);
    __m256i b1 = _mm256_unpackhi_epi16(a0, a2);
    __m256i b2 = _mm256_unpacklo_epi16(a1, a3);
    __m256i b3 = _mm256_unpackhi_epi16(a1, a3);

    __m256i t0 = _mm256_permute2x128_si256(b0, b1, 0x20);
    __m256i t1 = _mm256_permute2x128_si256(b0, b1, 0x31);
    __m256i t2 = _mm256_permute2x128_si256(b2, b3, 0x20);
    __m256i t3 = _mm256_permute2x128_si256(b2, b3, 0x31);

    __m256i c0 = _mm256_unpacklo_epi32(t0, t1);
    __m256i c1 = _mm256_unpackhi_epi32(t0, t1);
    __m256i c2 = _mm256_unpacklo_epi32(t2, t3);
    __m256i c3 = _mm256_unpackhi_epi32(t2, t3);

    __m128i lo0 = _mm256_castsi256_si128(c0);
    __m128i lo1 = _mm256_castsi256_si128(c1);
    __m128i hi0 = _mm256_extracti128_si256(c0, 1);
    __m128i hi1 = _mm256_extracti128_si256(c1, 1);
    __m128i lo2 = _mm256_castsi256_si128(c2);
    __m128i lo3 = _mm256_castsi256_si128(c3);
    __m128i hi2 = _mm256_extracti128_si256(c2, 1);
    __m128i hi3 = _mm256_extracti128_si256(c3, 1);

    _mm_storel_epi64((__m128i*)(dst_a), lo0);
    _mm_storeh_pd((double*)(dst_b), _mm_castsi128_pd(lo0));
    _mm_storel_epi64((__m128i*)(dst_a + sa), lo1);
    _mm_storeh_pd((double*)(dst_b + sb), _mm_castsi128_pd(lo1));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 2), hi0);
    _mm_storeh_pd((double*)(dst_b + sb * 2), _mm_castsi128_pd(hi0));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 3), hi1);
    _mm_storeh_pd((double*)(dst_b + sb * 3), _mm_castsi128_pd(hi1));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 4), lo2);
    _mm_storeh_pd((double*)(dst_b + sb * 4), _mm_castsi128_pd(lo2));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 5), lo3);
    _mm_storeh_pd((double*)(dst_b + sb * 5), _mm_castsi128_pd(lo3));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 6), hi2);
    _mm_storeh_pd((double*)(dst_b + sb * 6), _mm_castsi128_pd(hi2));
    _mm_storel_epi64((__m128i*)(dst_a + sa * 7), hi3);
    _mm_storeh_pd((double*)(dst_b + sb * 7), _mm_castsi128_pd(hi3));

    src += 16;
    dst_a += sa * 8;
    dst_b += sb * 8;
    byte_width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX8_AVX2) || defined(HAS_TRANSPOSEUVWX8_AVX2)

#if defined(HAS_TRANSPOSEWX8_AVX2)
// Transpose 16x8. Width is a multiple of 16.
LIBYUV_TARGET_AVX2
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
// Transpose UV 8x8. Width is a multiple of 8.
LIBYUV_TARGET_AVX2
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

#if defined(HAS_TRANSPOSEWX16_AVX2) || defined(HAS_TRANSPOSEUVWX16_AVX2)
// Load row r into lane 0 and row r + 8 into lane 1.
#define LOAD2ROWS_AVX2(p, s8)                                       \
  _mm256_inserti128_si256(                                          \
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(p))), \
      _mm_loadu_si128((const __m128i*)((p) + (s8))), 1)

// Transpose 16x16 bytes using 2 lanes of AVX2. Source row r is in lane r / 8
// of register r % 8. 3 unpack stages leave each lane with 2 columns x 8 rows,
// and vpermq joins the rows 0..7 and 8..15 halves. Lane 0 of each output is an
// even column for dst_a and lane 1 an odd column for dst_b.
// Width is a multiple of 16 bytes.
LIBYUV_TARGET_AVX2
static void TransposeWx16_Byte_AVX2(const uint8_t* src,
                                    int src_stride,
                                    uint8_t* dst_a,
                                    int dst_stride_a,
                                    uint8_t* dst_b,
                                    int dst_stride_b,
                                    int byte_width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t sa = dst_stride_a;
  const ptrdiff_t sb = dst_stride_b;
  while (byte_width > 0) {
    __m256i r0 = LOAD2ROWS_AVX2(src, s * 8);
    __m256i r1 = LOAD2ROWS_AVX2(src + s, s * 8);
    __m256i r2 = LOAD2ROWS_AVX2(src + s * 2, s * 8);
    __m256i r3 = LOAD2ROWS_AVX2(src + s * 3, s * 8);
    __m256i r4 = LOAD2ROWS_AVX2(src + s * 4, s * 8);
    __m256i r5 = LOAD2ROWS_AVX2(src + s * 5, s * 8);
    __m256i r6 = LOAD2ROWS_AVX2(src + s * 6, s * 8);
    __m256i r7 = LOAD2ROWS_AVX2(src + s * 7, s * 8);

    __m256i a0 = _mm256_unpacklo_epi8(r0, r1);
    __m256i a1 = _mm256_unpackhi_epi8(r0, r1);
    __m256i a2 = _mm256_unpacklo_epi8(r2, r3);
    __m256i a3 = _mm256_unpackhi_epi8(r2, r3);
    __m256i a4 = _mm256_unpacklo_epi8(r4, r5);
    __m256i a5 = _mm256_unpackhi_epi8(r4, r5);
    __m256i a6 = _mm256_unpacklo_epi8(r6, r7);
    __m256i a7 = _mm256_unpackhi_epi8(r6, r7);

    __m256i b0 = _mm256_unpacklo_epi16(a0, a2);
    __m256i b1 = _mm256_unpackhi_epi16(a0, a2);
    __m256i b2 = _mm256_unpacklo_epi16(a1, a3);
    __m256i b3 = _mm256_unpackhi_epi16(a1, a3);
    __m256i b4 = _mm256_unpacklo_epi16(a4, a6);
    __m256i b5 = _mm256_unpackhi_epi16(a4, a6);
    __m256i b6 = _mm256_unpacklo_epi16(a5, a7);
    __m256i b7 = _mm256_unpackhi_epi16(a5, a7);

    __m256i c0 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(b0, b4), 0xd8);
    __m256i c1 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(b0, b4), 0xd8);
    __m256i c2 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(b1, b5), 0xd8);
    __m256i c3 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(b1, b5), 0xd8);
    __m256i c4 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(b2, b6), 0xd8);
    __m256i c5 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(b2, b6), 0xd8);
    __m256i c6 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(b3, b7), 0xd8);
    __m256i c7 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(b3, b7), 0xd8);

    _mm_storeu_si128((__m128i*)(dst_a), _mm256_castsi256_si128(c0));
    _mm_storeu_si128((__m128i*)(dst_b), _mm256_extracti128_si256(c0, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa), _mm256_castsi256_si128(c1));
    _mm_storeu_si128((__m128i*)(dst_b + sb), _mm256_extracti128_si256(c1, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 2), _mm256_castsi256_si128(c2));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 2),
                     _mm256_extracti128_si256(c2, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 3), _mm256_castsi256_si128(c3));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 3),
                     _mm256_extracti128_si256(c3, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 4), _mm256_castsi256_si128(c4));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 4),
                     _mm256_extracti128_si256(c4, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 5), _mm256_castsi256_si128(c5));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 5),
                     _mm256_extracti128_si256(c5, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 6), _mm256_castsi256_si128(c6));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 6),
                     _mm256_extracti128_si256(c6, 1));
    _mm_storeu_si128((__m128i*)(dst_a + sa * 7), _mm256_castsi256_si128(c7));
    _mm_storeu_si128((__m128i*)(dst_b + sb * 7),
                     _mm256_extracti128_si256(c7, 1));

    src += 16;
    dst_a += sa * 8;
    dst_b += sb * 8;
    byte_width -= 16;
  }
  _mm256_zeroupper();
}
#undef LOAD2ROWS_AVX2
#endif  // defined(HAS_TRANSPOSEWX16_AVX2) || defined(HAS_TRANSPOSEUVWX16_AVX2)

#if defined(HAS_TRANSPOSEWX16_AVX2)
LIBYUV_TARGET_AVX2
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
LIBYUV_TARGET_AVX2
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

#if defined(HAS_TRANSPOSEWX16_AVX512BW) || defined(HAS_TRANSPOSEUVWX16_AVX512BW)
// Dword permutes to gather 16 rows of a column from 4 lanes.
static const uint32_t kPermdTranspose_AVX512BW[32] = {
    0, 4, 8, 12, 2, 6, 10, 14, 16, 20, 24, 28, 18, 22, 26, 30,
    1, 5, 9, 13, 3, 7, 11, 15, 17, 21, 25, 29, 19, 23, 27, 31};

// Load 16 masked bytes from rows p, p + s4, p + s4 * 2, p + s4 * 3 into
// lanes 0..3.
#define LOAD4ROWS_AVX512BW(p, s4, k)                                \
  _mm512_inserti32x4(                                               \
      _mm512_inserti32x4(                                           \
          _mm512_inserti32x4(                                       \
              _mm512_castsi128_si512(_mm_maskz_loadu_epi8(k, (p))), \
              _mm_maskz_loadu_epi8(k, (p) + (s4)), 1),              \
          _mm_maskz_loadu_epi8(k, (p) + (s4) * 2), 2),              \
      _mm_maskz_loadu_epi8(k, (p) + (s4) * 3), 3)

// Store 16 destination rows. Lane n of o0/o2 is for dst_a and lane n of
// o1/o3 is for dst_b.
LIBYUV_TARGET_AVX512BW
static void Store16x16_AVX512BW(__m512i o0,
                                __m512i o1,
                                __m512i o2,
                                __m512i o3,
                                uint8_t* dst_a,
                                ptrdiff_t sa,
                                uint8_t* dst_b,
                                ptrdiff_t sb) {
  _mm_storeu_si128((__m128i*)(dst_a), _mm512_castsi512_si128(o0));
  _mm_storeu_si128((__m128i*)(dst_b), _mm512_castsi512_si128(o1));
  _mm_storeu_si128((__m128i*)(dst_a + sa), _mm512_extracti32x4_epi32(o0, 1));
  _mm_storeu_si128((__m128i*)(dst_b + sb), _mm512_extracti32x4_epi32(o1, 1));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 2), _mm512_castsi512_si128(o2));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 2), _mm512_castsi512_si128(o3));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 3),
                   _mm512_extracti32x4_epi32(o2, 1));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 3),
                   _mm512_extracti32x4_epi32(o3, 1));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 4),
                   _mm512_extracti32x4_epi32(o0, 2));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 4),
                   _mm512_extracti32x4_epi32(o1, 2));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 5),
                   _mm512_extracti32x4_epi32(o0, 3));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 5),
                   _mm512_extracti32x4_epi32(o1, 3));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 6),
                   _mm512_extracti32x4_epi32(o2, 2));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 6),
                   _mm512_extracti32x4_epi32(o3, 2));
  _mm_storeu_si128((__m128i*)(dst_a + sa * 7),
                   _mm512_extracti32x4_epi32(o2, 3));
  _mm_storeu_si128((__m128i*)(dst_b + sb * 7),
                   _mm512_extracti32x4_epi32(o3, 3));
}

// Transpose 16x16 bytes using 4 lanes of AVX512BW with tail masking.
// Source row r is in lane r / 4 of register r % 4.
// Can be used for 8-bit planar transpose (dst_a = even rows, dst_b = odd rows)
// or 16-bit UV split transpose (dst_a = U plane, dst_b = V plane).
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
  const ptrdiff_t s = src_stride;
  const ptrdiff_t sa = dst_stride_a;
  const ptrdiff_t sb = dst_stride_b;
  const __m512i perm0 = _mm512_loadu_si512(kPermdTranspose_AVX512BW);
  const __m512i perm1 = _mm512_loadu_si512(kPermdTranspose_AVX512BW + 16);
  while (byte_width > 0) {
    const int n = byte_width < 16 ? byte_width : 16;
    const __mmask16 k = (__mmask16)((1u << n) - 1);
    __m512i z0 = LOAD4ROWS_AVX512BW(src, s * 4, k);
    __m512i z1 = LOAD4ROWS_AVX512BW(src + s, s * 4, k);
    __m512i z2 = LOAD4ROWS_AVX512BW(src + s * 2, s * 4, k);
    __m512i z3 = LOAD4ROWS_AVX512BW(src + s * 3, s * 4, k);

    __m512i a0 = _mm512_unpacklo_epi8(z0, z1);
    __m512i a1 = _mm512_unpackhi_epi8(z0, z1);
    __m512i a2 = _mm512_unpacklo_epi8(z2, z3);
    __m512i a3 = _mm512_unpackhi_epi8(z2, z3);
    __m512i b0 = _mm512_unpacklo_epi16(a0, a2);
    __m512i b1 = _mm512_unpackhi_epi16(a0, a2);
    __m512i b2 = _mm512_unpacklo_epi16(a1, a3);
    __m512i b3 = _mm512_unpackhi_epi16(a1, a3);
    __m512i o0 = _mm512_permutex2var_epi32(b0, perm0, b2);
    __m512i o1 = _mm512_permutex2var_epi32(b0, perm1, b2);
    __m512i o2 = _mm512_permutex2var_epi32(b1, perm0, b3);
    __m512i o3 = _mm512_permutex2var_epi32(b1, perm1, b3);

    if (n == 16) {
      Store16x16_AVX512BW(o0, o1, o2, o3, dst_a, sa, dst_b, sb);
      dst_a += sa * 8;
      dst_b += sb * 8;
    } else {
      // Remainder: 1 to 15 bytes. Store 1 destination row per byte.
      SIMD_ALIGNED(uint8_t temp[16 * 16]);
      int i;
      Store16x16_AVX512BW(o0, o1, o2, o3, temp, 32, temp + 16, 32);
      for (i = 0; i < n; ++i) {
        uint8_t* dst = (i & 1) ? dst_b + sb * (i >> 1) : dst_a + sa * (i >> 1);
        _mm_storeu_si128((__m128i*)dst,
                         _mm_load_si128((const __m128i*)(temp + i * 16)));
      }
    }
    src += 16;
    byte_width -= 16;
  }
  _mm256_zeroupper();
}
#undef LOAD4ROWS_AVX512BW
#endif  // defined(HAS_TRANSPOSEWX16_AVX512BW) ||
        // defined(HAS_TRANSPOSEUVWX16_AVX512BW)

#if defined(HAS_TRANSPOSEWX16_AVX512BW)
LIBYUV_TARGET_AVX512BW
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
LIBYUV_TARGET_AVX512BW
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

#if defined(HAS_TRANSPOSEWX8_16_AVX2)
LIBYUV_TARGET_AVX2
void TransposeWx8_16_AVX2(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width > 0) {
    __m256i r0 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src))),
        _mm_loadu_si128((const __m128i*)(src + s * 4)), 1);
    __m256i r1 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s))),
        _mm_loadu_si128((const __m128i*)(src + s * 5)), 1);
    __m256i r2 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 2))),
        _mm_loadu_si128((const __m128i*)(src + s * 6)), 1);
    __m256i r3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 3))),
        _mm_loadu_si128((const __m128i*)(src + s * 7)), 1);

    __m256i t0 = _mm256_unpacklo_epi16(r0, r1);
    __m256i t1 = _mm256_unpackhi_epi16(r0, r1);
    __m256i t2 = _mm256_unpacklo_epi16(r2, r3);
    __m256i t3 = _mm256_unpackhi_epi16(r2, r3);

    __m256i u0 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(t0, t2), 0xd8);
    __m256i u1 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(t0, t2), 0xd8);
    __m256i u2 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(t1, t3), 0xd8);
    __m256i u3 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(t1, t3), 0xd8);

    _mm_storeu_si128((__m128i*)(dst), _mm256_castsi256_si128(u0));
    _mm_storeu_si128((__m128i*)(dst + d), _mm256_extracti128_si256(u0, 1));
    _mm_storeu_si128((__m128i*)(dst + d * 2), _mm256_castsi256_si128(u1));
    _mm_storeu_si128((__m128i*)(dst + d * 3), _mm256_extracti128_si256(u1, 1));
    _mm_storeu_si128((__m128i*)(dst + d * 4), _mm256_castsi256_si128(u2));
    _mm_storeu_si128((__m128i*)(dst + d * 5), _mm256_extracti128_si256(u2, 1));
    _mm_storeu_si128((__m128i*)(dst + d * 6), _mm256_castsi256_si128(u3));
    _mm_storeu_si128((__m128i*)(dst + d * 7), _mm256_extracti128_si256(u3, 1));
    src += 8;
    dst += d * 8;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX8_16_AVX2)

#if defined(HAS_TRANSPOSEWX16_16_AVX2)
LIBYUV_TARGET_AVX2
void TransposeWx16_16_AVX2(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width > 0) {
    __m256i r0 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src))),
        _mm_loadu_si128((const __m128i*)(src + s * 8)), 1);
    __m256i r1 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s))),
        _mm_loadu_si128((const __m128i*)(src + s * 9)), 1);
    __m256i r2 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 2))),
        _mm_loadu_si128((const __m128i*)(src + s * 10)), 1);
    __m256i r3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 3))),
        _mm_loadu_si128((const __m128i*)(src + s * 11)), 1);
    __m256i r4 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 4))),
        _mm_loadu_si128((const __m128i*)(src + s * 12)), 1);
    __m256i r5 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 5))),
        _mm_loadu_si128((const __m128i*)(src + s * 13)), 1);
    __m256i r6 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 6))),
        _mm_loadu_si128((const __m128i*)(src + s * 14)), 1);
    __m256i r7 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 7))),
        _mm_loadu_si128((const __m128i*)(src + s * 15)), 1);

    __m256i t0 = _mm256_unpacklo_epi16(r0, r1);
    __m256i t1 = _mm256_unpackhi_epi16(r0, r1);
    __m256i t2 = _mm256_unpacklo_epi16(r2, r3);
    __m256i t3 = _mm256_unpackhi_epi16(r2, r3);
    __m256i t4 = _mm256_unpacklo_epi16(r4, r5);
    __m256i t5 = _mm256_unpackhi_epi16(r4, r5);
    __m256i t6 = _mm256_unpacklo_epi16(r6, r7);
    __m256i t7 = _mm256_unpackhi_epi16(r6, r7);

    __m256i u0 = _mm256_unpacklo_epi32(t0, t2);
    __m256i u1 = _mm256_unpackhi_epi32(t0, t2);
    __m256i u2 = _mm256_unpacklo_epi32(t1, t3);
    __m256i u3 = _mm256_unpackhi_epi32(t1, t3);
    __m256i u4 = _mm256_unpacklo_epi32(t4, t6);
    __m256i u5 = _mm256_unpackhi_epi32(t4, t6);
    __m256i u6 = _mm256_unpacklo_epi32(t5, t7);
    __m256i u7 = _mm256_unpackhi_epi32(t5, t7);

    _mm256_storeu_si256((__m256i*)(dst), _mm256_unpacklo_epi64(u0, u4));
    _mm256_storeu_si256((__m256i*)(dst + d), _mm256_unpackhi_epi64(u0, u4));
    _mm256_storeu_si256((__m256i*)(dst + d * 2), _mm256_unpacklo_epi64(u1, u5));
    _mm256_storeu_si256((__m256i*)(dst + d * 3), _mm256_unpackhi_epi64(u1, u5));
    _mm256_storeu_si256((__m256i*)(dst + d * 4), _mm256_unpacklo_epi64(u2, u6));
    _mm256_storeu_si256((__m256i*)(dst + d * 5), _mm256_unpackhi_epi64(u2, u6));
    _mm256_storeu_si256((__m256i*)(dst + d * 6), _mm256_unpacklo_epi64(u3, u7));
    _mm256_storeu_si256((__m256i*)(dst + d * 7), _mm256_unpackhi_epi64(u3, u7));
    src += 8;
    dst += d * 8;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX16_16_AVX2)

#if defined(HAS_TRANSPOSEWX32_16_AVX512BW)
LIBYUV_TARGET_AVX512BW
void TransposeWx32_16_AVX512BW(const uint16_t* src,
                               int src_stride,
                               uint16_t* dst,
                               int dst_stride,
                               int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width >= 8) {
    __m512i r0 = _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src)));
    __m512i r1 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s)));
    __m512i r2 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 2)));
    __m512i r3 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 3)));
    __m512i r4 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 4)));
    __m512i r5 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 5)));
    __m512i r6 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 6)));
    __m512i r7 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 7)));

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 8)),
                            1);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 9)),
                            1);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 10)),
                            1);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 11)),
                            1);
    r4 = _mm512_inserti32x4(r4, _mm_loadu_si128((const __m128i*)(src + s * 12)),
                            1);
    r5 = _mm512_inserti32x4(r5, _mm_loadu_si128((const __m128i*)(src + s * 13)),
                            1);
    r6 = _mm512_inserti32x4(r6, _mm_loadu_si128((const __m128i*)(src + s * 14)),
                            1);
    r7 = _mm512_inserti32x4(r7, _mm_loadu_si128((const __m128i*)(src + s * 15)),
                            1);

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 16)),
                            2);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 17)),
                            2);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 18)),
                            2);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 19)),
                            2);
    r4 = _mm512_inserti32x4(r4, _mm_loadu_si128((const __m128i*)(src + s * 20)),
                            2);
    r5 = _mm512_inserti32x4(r5, _mm_loadu_si128((const __m128i*)(src + s * 21)),
                            2);
    r6 = _mm512_inserti32x4(r6, _mm_loadu_si128((const __m128i*)(src + s * 22)),
                            2);
    r7 = _mm512_inserti32x4(r7, _mm_loadu_si128((const __m128i*)(src + s * 23)),
                            2);

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 24)),
                            3);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 25)),
                            3);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 26)),
                            3);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 27)),
                            3);
    r4 = _mm512_inserti32x4(r4, _mm_loadu_si128((const __m128i*)(src + s * 28)),
                            3);
    r5 = _mm512_inserti32x4(r5, _mm_loadu_si128((const __m128i*)(src + s * 29)),
                            3);
    r6 = _mm512_inserti32x4(r6, _mm_loadu_si128((const __m128i*)(src + s * 30)),
                            3);
    r7 = _mm512_inserti32x4(r7, _mm_loadu_si128((const __m128i*)(src + s * 31)),
                            3);

    __m512i t0 = _mm512_unpacklo_epi16(r0, r1);
    __m512i t1 = _mm512_unpackhi_epi16(r0, r1);
    __m512i t2 = _mm512_unpacklo_epi16(r2, r3);
    __m512i t3 = _mm512_unpackhi_epi16(r2, r3);
    __m512i t4 = _mm512_unpacklo_epi16(r4, r5);
    __m512i t5 = _mm512_unpackhi_epi16(r4, r5);
    __m512i t6 = _mm512_unpacklo_epi16(r6, r7);
    __m512i t7 = _mm512_unpackhi_epi16(r6, r7);

    __m512i u0 = _mm512_unpacklo_epi32(t0, t2);
    __m512i u1 = _mm512_unpackhi_epi32(t0, t2);
    __m512i u2 = _mm512_unpacklo_epi32(t1, t3);
    __m512i u3 = _mm512_unpackhi_epi32(t1, t3);
    __m512i u4 = _mm512_unpacklo_epi32(t4, t6);
    __m512i u5 = _mm512_unpackhi_epi32(t4, t6);
    __m512i u6 = _mm512_unpacklo_epi32(t5, t7);
    __m512i u7 = _mm512_unpackhi_epi32(t5, t7);

    _mm512_storeu_si512((__m512i*)(dst), _mm512_unpacklo_epi64(u0, u4));
    _mm512_storeu_si512((__m512i*)(dst + d), _mm512_unpackhi_epi64(u0, u4));
    _mm512_storeu_si512((__m512i*)(dst + d * 2), _mm512_unpacklo_epi64(u1, u5));
    _mm512_storeu_si512((__m512i*)(dst + d * 3), _mm512_unpackhi_epi64(u1, u5));
    _mm512_storeu_si512((__m512i*)(dst + d * 4), _mm512_unpacklo_epi64(u2, u6));
    _mm512_storeu_si512((__m512i*)(dst + d * 5), _mm512_unpackhi_epi64(u2, u6));
    _mm512_storeu_si512((__m512i*)(dst + d * 6), _mm512_unpacklo_epi64(u3, u7));
    _mm512_storeu_si512((__m512i*)(dst + d * 7), _mm512_unpackhi_epi64(u3, u7));
    src += 8;
    dst += d * 8;
    width -= 8;
  }
  if (width > 0) {
    const __mmask8 col_mask = (1u << width) - 1;
    __m512i r0 = _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src));
    __m512i r1 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s));
    __m512i r2 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 2));
    __m512i r3 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 3));
    __m512i r4 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 4));
    __m512i r5 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 5));
    __m512i r6 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 6));
    __m512i r7 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi16(col_mask, src + s * 7));

    r0 =
        _mm512_inserti32x4(r0, _mm_maskz_loadu_epi16(col_mask, src + s * 8), 1);
    r1 =
        _mm512_inserti32x4(r1, _mm_maskz_loadu_epi16(col_mask, src + s * 9), 1);
    r2 = _mm512_inserti32x4(r2, _mm_maskz_loadu_epi16(col_mask, src + s * 10),
                            1);
    r3 = _mm512_inserti32x4(r3, _mm_maskz_loadu_epi16(col_mask, src + s * 11),
                            1);
    r4 = _mm512_inserti32x4(r4, _mm_maskz_loadu_epi16(col_mask, src + s * 12),
                            1);
    r5 = _mm512_inserti32x4(r5, _mm_maskz_loadu_epi16(col_mask, src + s * 13),
                            1);
    r6 = _mm512_inserti32x4(r6, _mm_maskz_loadu_epi16(col_mask, src + s * 14),
                            1);
    r7 = _mm512_inserti32x4(r7, _mm_maskz_loadu_epi16(col_mask, src + s * 15),
                            1);

    r0 = _mm512_inserti32x4(r0, _mm_maskz_loadu_epi16(col_mask, src + s * 16),
                            2);
    r1 = _mm512_inserti32x4(r1, _mm_maskz_loadu_epi16(col_mask, src + s * 17),
                            2);
    r2 = _mm512_inserti32x4(r2, _mm_maskz_loadu_epi16(col_mask, src + s * 18),
                            2);
    r3 = _mm512_inserti32x4(r3, _mm_maskz_loadu_epi16(col_mask, src + s * 19),
                            2);
    r4 = _mm512_inserti32x4(r4, _mm_maskz_loadu_epi16(col_mask, src + s * 20),
                            2);
    r5 = _mm512_inserti32x4(r5, _mm_maskz_loadu_epi16(col_mask, src + s * 21),
                            2);
    r6 = _mm512_inserti32x4(r6, _mm_maskz_loadu_epi16(col_mask, src + s * 22),
                            2);
    r7 = _mm512_inserti32x4(r7, _mm_maskz_loadu_epi16(col_mask, src + s * 23),
                            2);

    r0 = _mm512_inserti32x4(r0, _mm_maskz_loadu_epi16(col_mask, src + s * 24),
                            3);
    r1 = _mm512_inserti32x4(r1, _mm_maskz_loadu_epi16(col_mask, src + s * 25),
                            3);
    r2 = _mm512_inserti32x4(r2, _mm_maskz_loadu_epi16(col_mask, src + s * 26),
                            3);
    r3 = _mm512_inserti32x4(r3, _mm_maskz_loadu_epi16(col_mask, src + s * 27),
                            3);
    r4 = _mm512_inserti32x4(r4, _mm_maskz_loadu_epi16(col_mask, src + s * 28),
                            3);
    r5 = _mm512_inserti32x4(r5, _mm_maskz_loadu_epi16(col_mask, src + s * 29),
                            3);
    r6 = _mm512_inserti32x4(r6, _mm_maskz_loadu_epi16(col_mask, src + s * 30),
                            3);
    r7 = _mm512_inserti32x4(r7, _mm_maskz_loadu_epi16(col_mask, src + s * 31),
                            3);

    __m512i t0 = _mm512_unpacklo_epi16(r0, r1);
    __m512i t1 = _mm512_unpackhi_epi16(r0, r1);
    __m512i t2 = _mm512_unpacklo_epi16(r2, r3);
    __m512i t3 = _mm512_unpackhi_epi16(r2, r3);
    __m512i t4 = _mm512_unpacklo_epi16(r4, r5);
    __m512i t5 = _mm512_unpackhi_epi16(r4, r5);
    __m512i t6 = _mm512_unpacklo_epi16(r6, r7);
    __m512i t7 = _mm512_unpackhi_epi16(r6, r7);

    __m512i u0 = _mm512_unpacklo_epi32(t0, t2);
    __m512i u1 = _mm512_unpackhi_epi32(t0, t2);
    __m512i u2 = _mm512_unpacklo_epi32(t1, t3);
    __m512i u3 = _mm512_unpackhi_epi32(t1, t3);
    __m512i u4 = _mm512_unpacklo_epi32(t4, t6);
    __m512i u5 = _mm512_unpackhi_epi32(t4, t6);
    __m512i u6 = _mm512_unpacklo_epi32(t5, t7);
    __m512i u7 = _mm512_unpackhi_epi32(t5, t7);

    _mm512_storeu_si512((__m512i*)(dst), _mm512_unpacklo_epi64(u0, u4));
    if (width > 1) {
      _mm512_storeu_si512((__m512i*)(dst + d), _mm512_unpackhi_epi64(u0, u4));
    }
    if (width > 2) {
      _mm512_storeu_si512((__m512i*)(dst + d * 2),
                          _mm512_unpacklo_epi64(u1, u5));
    }
    if (width > 3) {
      _mm512_storeu_si512((__m512i*)(dst + d * 3),
                          _mm512_unpackhi_epi64(u1, u5));
    }
    if (width > 4) {
      _mm512_storeu_si512((__m512i*)(dst + d * 4),
                          _mm512_unpacklo_epi64(u2, u6));
    }
    if (width > 5) {
      _mm512_storeu_si512((__m512i*)(dst + d * 5),
                          _mm512_unpackhi_epi64(u2, u6));
    }
    if (width > 6) {
      _mm512_storeu_si512((__m512i*)(dst + d * 6),
                          _mm512_unpacklo_epi64(u3, u7));
    }
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX32_16_AVX512BW)

#if defined(HAS_TRANSPOSEWX4_32_AVX2)
// Transpose 32 bit values (ARGB). Read a column of 4 pixels across 8 rows per
// AVX2 iteration (with a 4-row 128-bit tail) and write 4 destination rows.
LIBYUV_TARGET_AVX2
void TransposeWx4_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width >= 8) {
    __m256i r0 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src))),
        _mm_loadu_si128((const __m128i*)(src + s * 4)), 1);
    __m256i r1 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s))),
        _mm_loadu_si128((const __m128i*)(src + s * 5)), 1);
    __m256i r2 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 2))),
        _mm_loadu_si128((const __m128i*)(src + s * 6)), 1);
    __m256i r3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 3))),
        _mm_loadu_si128((const __m128i*)(src + s * 7)), 1);

    __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
    __m256i t1 = _mm256_unpacklo_epi32(r2, r3);
    __m256i t2 = _mm256_unpackhi_epi32(r0, r1);
    __m256i t3 = _mm256_unpackhi_epi32(r2, r3);

    _mm256_storeu_si256((__m256i*)(dst), _mm256_unpacklo_epi64(t0, t1));
    _mm256_storeu_si256((__m256i*)(dst + d), _mm256_unpackhi_epi64(t0, t1));
    _mm256_storeu_si256((__m256i*)(dst + d * 2), _mm256_unpacklo_epi64(t2, t3));
    _mm256_storeu_si256((__m256i*)(dst + d * 3), _mm256_unpackhi_epi64(t2, t3));
    src += s * 8;
    dst += 32;
    width -= 8;
  }
  if (width > 0) {
    __m128i r0 = _mm_loadu_si128((const __m128i*)(src));
    __m128i r1 = _mm_loadu_si128((const __m128i*)(src + s));
    __m128i r2 = _mm_loadu_si128((const __m128i*)(src + s * 2));
    __m128i r3 = _mm_loadu_si128((const __m128i*)(src + s * 3));
    __m128i t0 = _mm_unpacklo_epi32(r0, r1);
    __m128i t1 = _mm_unpacklo_epi32(r2, r3);
    __m128i t2 = _mm_unpackhi_epi32(r0, r1);
    __m128i t3 = _mm_unpackhi_epi32(r2, r3);
    _mm_storeu_si128((__m128i*)(dst), _mm_unpacklo_epi64(t0, t1));
    _mm_storeu_si128((__m128i*)(dst + d), _mm_unpackhi_epi64(t0, t1));
    _mm_storeu_si128((__m128i*)(dst + d * 2), _mm_unpacklo_epi64(t2, t3));
    _mm_storeu_si128((__m128i*)(dst + d * 3), _mm_unpackhi_epi64(t2, t3));
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX4_32_AVX2)

#if defined(HAS_TRANSPOSEWX8_32_AVX2)
LIBYUV_TARGET_AVX2
void TransposeWx8_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width > 0) {
    __m256i r0 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src))),
        _mm_loadu_si128((const __m128i*)(src + s * 4)), 1);
    __m256i r1 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s))),
        _mm_loadu_si128((const __m128i*)(src + s * 5)), 1);
    __m256i r2 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 2))),
        _mm_loadu_si128((const __m128i*)(src + s * 6)), 1);
    __m256i r3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)(src + s * 3))),
        _mm_loadu_si128((const __m128i*)(src + s * 7)), 1);

    __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
    __m256i t1 = _mm256_unpacklo_epi32(r2, r3);
    __m256i t2 = _mm256_unpackhi_epi32(r0, r1);
    __m256i t3 = _mm256_unpackhi_epi32(r2, r3);

    _mm256_storeu_si256((__m256i*)(dst), _mm256_unpacklo_epi64(t0, t1));
    _mm256_storeu_si256((__m256i*)(dst + d), _mm256_unpackhi_epi64(t0, t1));
    _mm256_storeu_si256((__m256i*)(dst + d * 2), _mm256_unpacklo_epi64(t2, t3));
    _mm256_storeu_si256((__m256i*)(dst + d * 3), _mm256_unpackhi_epi64(t2, t3));
    src += 16;
    dst += d * 4;
    width -= 4;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX8_32_AVX2)

#if defined(HAS_TRANSPOSEWXH_32_AVX2)
// Load n (1..3) 32-bit pixels into low lanes of __m128i with zeroed upper
// lanes (safe at buffer boundaries and MemorySanitizer-clean).
LIBYUV_TARGET_AVX2
static __m128i LoadPartialRow_32_AVX2(const uint8_t* p, int n) {
  if (n == 1) {
    int v;
    memcpy(&v, p, 4);
    return _mm_cvtsi32_si128(v);
  }
  if (n == 2) {
    return _mm_loadl_epi64((const __m128i*)p);
  }
  int v;
  memcpy(&v, p + 8, 4);
  return _mm_insert_epi32(_mm_loadl_epi64((const __m128i*)p), v, 2);
}

// Transpose arbitrary WxH 32-bit plane using AVX2 (4x8, 8x4, 4x4, and 4x{1..3}
// tiles plus exact remainder handling).
LIBYUV_TARGET_AVX2
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
      const uint8_t* src_rem = src + (ptrdiff_t)w4 * 4;
      uint8_t* dst_rem = dst + (ptrdiff_t)w4 * d;
      __m256i r0 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(LoadPartialRow_32_AVX2(src_rem, rem_w)),
          LoadPartialRow_32_AVX2(src_rem + s * 4, rem_w), 1);
      __m256i r1 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(LoadPartialRow_32_AVX2(src_rem + s, rem_w)),
          LoadPartialRow_32_AVX2(src_rem + s * 5, rem_w), 1);
      __m256i r2 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(
              LoadPartialRow_32_AVX2(src_rem + s * 2, rem_w)),
          LoadPartialRow_32_AVX2(src_rem + s * 6, rem_w), 1);
      __m256i r3 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(
              LoadPartialRow_32_AVX2(src_rem + s * 3, rem_w)),
          LoadPartialRow_32_AVX2(src_rem + s * 7, rem_w), 1);

      __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
      __m256i t1 = _mm256_unpacklo_epi32(r2, r3);
      _mm256_storeu_si256((__m256i*)(dst_rem), _mm256_unpacklo_epi64(t0, t1));
      if (rem_w > 1) {
        _mm256_storeu_si256((__m256i*)(dst_rem + d),
                            _mm256_unpackhi_epi64(t0, t1));
      }
      if (rem_w > 2) {
        __m256i t2 = _mm256_unpackhi_epi32(r0, r1);
        __m256i t3 = _mm256_unpackhi_epi32(r2, r3);
        _mm256_storeu_si256((__m256i*)(dst_rem + d * 2),
                            _mm256_unpacklo_epi64(t2, t3));
      }
      _mm256_zeroupper();
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
      while (cnt > 0) {
        __m256i r0 = _mm256_loadu_si256((const __m256i*)(src_p));
        __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_p + s));
        __m256i r2 = _mm256_loadu_si256((const __m256i*)(src_p + s * 2));
        __m256i r3 = _mm256_loadu_si256((const __m256i*)(src_p + s * 3));

        __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
        __m256i t1 = _mm256_unpacklo_epi32(r2, r3);
        __m256i t2 = _mm256_unpackhi_epi32(r0, r1);
        __m256i t3 = _mm256_unpackhi_epi32(r2, r3);

        __m256i o0 = _mm256_unpacklo_epi64(t0, t1);
        __m256i o1 = _mm256_unpackhi_epi64(t0, t1);
        __m256i o2 = _mm256_unpacklo_epi64(t2, t3);
        __m256i o3 = _mm256_unpackhi_epi64(t2, t3);

        _mm_storeu_si128((__m128i*)(dst_p), _mm256_castsi256_si128(o0));
        _mm_storeu_si128((__m128i*)(dst_p + d), _mm256_castsi256_si128(o1));
        _mm_storeu_si128((__m128i*)(dst_p + d * 2), _mm256_castsi256_si128(o2));
        _mm_storeu_si128((__m128i*)(dst_p + d * 3), _mm256_castsi256_si128(o3));
        _mm_storeu_si128((__m128i*)(dst_p + d * 4),
                         _mm256_extracti128_si256(o0, 1));
        _mm_storeu_si128((__m128i*)(dst_p + d * 5),
                         _mm256_extracti128_si256(o1, 1));
        _mm_storeu_si128((__m128i*)(dst_p + d * 6),
                         _mm256_extracti128_si256(o2, 1));
        _mm_storeu_si128((__m128i*)(dst_p + d * 7),
                         _mm256_extracti128_si256(o3, 1));

        src_p += 32;
        dst_p += d * 8;
        cnt -= 8;
      }
      _mm256_zeroupper();
    }
    if (width & 4) {
      const uint8_t* src_p = src + (ptrdiff_t)w8 * 4;
      uint8_t* dst_p = dst + (ptrdiff_t)w8 * d;
      __m128i r0 = _mm_loadu_si128((const __m128i*)(src_p));
      __m128i r1 = _mm_loadu_si128((const __m128i*)(src_p + s));
      __m128i r2 = _mm_loadu_si128((const __m128i*)(src_p + s * 2));
      __m128i r3 = _mm_loadu_si128((const __m128i*)(src_p + s * 3));
      __m128i t0 = _mm_unpacklo_epi32(r0, r1);
      __m128i t1 = _mm_unpacklo_epi32(r2, r3);
      __m128i t2 = _mm_unpackhi_epi32(r0, r1);
      __m128i t3 = _mm_unpackhi_epi32(r2, r3);
      _mm_storeu_si128((__m128i*)(dst_p), _mm_unpacklo_epi64(t0, t1));
      _mm_storeu_si128((__m128i*)(dst_p + d), _mm_unpackhi_epi64(t0, t1));
      _mm_storeu_si128((__m128i*)(dst_p + d * 2), _mm_unpacklo_epi64(t2, t3));
      _mm_storeu_si128((__m128i*)(dst_p + d * 3), _mm_unpackhi_epi64(t2, t3));
    }
    if (rem_w > 0) {
      const uint8_t* src_rem = src + (ptrdiff_t)w4 * 4;
      uint8_t* dst_rem = dst + (ptrdiff_t)w4 * d;
      __m128i r0 = LoadPartialRow_32_AVX2(src_rem, rem_w);
      __m128i r1 = LoadPartialRow_32_AVX2(src_rem + s, rem_w);
      __m128i r2 = LoadPartialRow_32_AVX2(src_rem + s * 2, rem_w);
      __m128i r3 = LoadPartialRow_32_AVX2(src_rem + s * 3, rem_w);
      __m128i t0 = _mm_unpacklo_epi32(r0, r1);
      __m128i t1 = _mm_unpacklo_epi32(r2, r3);
      _mm_storeu_si128((__m128i*)(dst_rem), _mm_unpacklo_epi64(t0, t1));
      if (rem_w > 1) {
        _mm_storeu_si128((__m128i*)(dst_rem + d), _mm_unpackhi_epi64(t0, t1));
      }
      if (rem_w > 2) {
        __m128i t2 = _mm_unpackhi_epi32(r0, r1);
        __m128i t3 = _mm_unpackhi_epi32(r2, r3);
        _mm_storeu_si128((__m128i*)(dst_rem + d * 2),
                         _mm_unpacklo_epi64(t2, t3));
      }
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
        while (cnt > 0) {
          __m128i r0 = _mm_loadu_si128((const __m128i*)(src_p));
          int v0 = _mm_cvtsi128_si32(r0);
          int v1 = _mm_extract_epi32(r0, 1);
          int v2 = _mm_extract_epi32(r0, 2);
          int v3 = _mm_extract_epi32(r0, 3);
          memcpy(dst_p, &v0, 4);
          memcpy(dst_p + d, &v1, 4);
          memcpy(dst_p + d * 2, &v2, 4);
          memcpy(dst_p + d * 3, &v3, 4);
          src_p += 16;
          dst_p += d * 4;
          cnt -= 4;
        }
      } else if (height == 2) {
        while (cnt > 0) {
          __m128i r0 = _mm_loadu_si128((const __m128i*)(src_p));
          __m128i r1 = _mm_loadu_si128((const __m128i*)(src_p + s));
          __m128i t0 = _mm_unpacklo_epi32(r0, r1);
          __m128i t1 = _mm_unpackhi_epi32(r0, r1);
          _mm_storel_epi64((__m128i*)(dst_p), t0);
          _mm_storeh_pd((double*)(dst_p + d), _mm_castsi128_pd(t0));
          _mm_storel_epi64((__m128i*)(dst_p + d * 2), t1);
          _mm_storeh_pd((double*)(dst_p + d * 3), _mm_castsi128_pd(t1));
          src_p += 16;
          dst_p += d * 4;
          cnt -= 4;
        }
      } else {
        const __m128i zero = _mm_setzero_si128();
        while (cnt > 0) {
          __m128i r0 = _mm_loadu_si128((const __m128i*)(src_p));
          __m128i r1 = _mm_loadu_si128((const __m128i*)(src_p + s));
          __m128i r2 = _mm_loadu_si128((const __m128i*)(src_p + s * 2));
          __m128i t0 = _mm_unpacklo_epi32(r0, r1);
          __m128i t1 = _mm_unpacklo_epi32(r2, zero);
          __m128i t2 = _mm_unpackhi_epi32(r0, r1);
          __m128i t3 = _mm_unpackhi_epi32(r2, zero);
          __m128i o0 = _mm_unpacklo_epi64(t0, t1);
          __m128i o1 = _mm_unpackhi_epi64(t0, t1);
          __m128i o2 = _mm_unpacklo_epi64(t2, t3);
          __m128i o3 = _mm_unpackhi_epi64(t2, t3);
          int v0 = _mm_extract_epi32(o0, 2);
          int v1 = _mm_extract_epi32(o1, 2);
          int v2 = _mm_extract_epi32(o2, 2);
          int v3 = _mm_extract_epi32(o3, 2);
          _mm_storel_epi64((__m128i*)(dst_p), o0);
          memcpy(dst_p + 8, &v0, 4);
          _mm_storel_epi64((__m128i*)(dst_p + d), o1);
          memcpy(dst_p + d + 8, &v1, 4);
          _mm_storel_epi64((__m128i*)(dst_p + d * 2), o2);
          memcpy(dst_p + d * 2 + 8, &v2, 4);
          _mm_storel_epi64((__m128i*)(dst_p + d * 3), o3);
          memcpy(dst_p + d * 3 + 8, &v3, 4);
          src_p += 16;
          dst_p += d * 4;
          cnt -= 4;
        }
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
LIBYUV_TARGET_AVX512BW
void TransposeWx16_32_AVX512BW(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width >= 4) {
    __m512i r0 = _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src)));
    __m512i r1 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s)));
    __m512i r2 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 2)));
    __m512i r3 =
        _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(src + s * 3)));

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 4)),
                            1);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 5)),
                            1);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 6)),
                            1);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 7)),
                            1);

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 8)),
                            2);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 9)),
                            2);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 10)),
                            2);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 11)),
                            2);

    r0 = _mm512_inserti32x4(r0, _mm_loadu_si128((const __m128i*)(src + s * 12)),
                            3);
    r1 = _mm512_inserti32x4(r1, _mm_loadu_si128((const __m128i*)(src + s * 13)),
                            3);
    r2 = _mm512_inserti32x4(r2, _mm_loadu_si128((const __m128i*)(src + s * 14)),
                            3);
    r3 = _mm512_inserti32x4(r3, _mm_loadu_si128((const __m128i*)(src + s * 15)),
                            3);

    __m512i t0 = _mm512_unpacklo_epi32(r0, r1);
    __m512i t1 = _mm512_unpacklo_epi32(r2, r3);
    __m512i t2 = _mm512_unpackhi_epi32(r0, r1);
    __m512i t3 = _mm512_unpackhi_epi32(r2, r3);

    _mm512_storeu_si512((__m512i*)(dst), _mm512_unpacklo_epi64(t0, t1));
    _mm512_storeu_si512((__m512i*)(dst + d), _mm512_unpackhi_epi64(t0, t1));
    _mm512_storeu_si512((__m512i*)(dst + d * 2), _mm512_unpacklo_epi64(t2, t3));
    _mm512_storeu_si512((__m512i*)(dst + d * 3), _mm512_unpackhi_epi64(t2, t3));
    src += 16;
    dst += d * 4;
    width -= 4;
  }
  if (width > 0) {
    const __mmask8 col_mask = (1u << width) - 1;
    __m512i r0 = _mm512_castsi128_si512(_mm_maskz_loadu_epi32(col_mask, src));
    __m512i r1 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi32(col_mask, src + s));
    __m512i r2 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi32(col_mask, src + s * 2));
    __m512i r3 =
        _mm512_castsi128_si512(_mm_maskz_loadu_epi32(col_mask, src + s * 3));

    r0 =
        _mm512_inserti32x4(r0, _mm_maskz_loadu_epi32(col_mask, src + s * 4), 1);
    r1 =
        _mm512_inserti32x4(r1, _mm_maskz_loadu_epi32(col_mask, src + s * 5), 1);
    r2 =
        _mm512_inserti32x4(r2, _mm_maskz_loadu_epi32(col_mask, src + s * 6), 1);
    r3 =
        _mm512_inserti32x4(r3, _mm_maskz_loadu_epi32(col_mask, src + s * 7), 1);

    r0 =
        _mm512_inserti32x4(r0, _mm_maskz_loadu_epi32(col_mask, src + s * 8), 2);
    r1 =
        _mm512_inserti32x4(r1, _mm_maskz_loadu_epi32(col_mask, src + s * 9), 2);
    r2 = _mm512_inserti32x4(r2, _mm_maskz_loadu_epi32(col_mask, src + s * 10),
                            2);
    r3 = _mm512_inserti32x4(r3, _mm_maskz_loadu_epi32(col_mask, src + s * 11),
                            2);

    r0 = _mm512_inserti32x4(r0, _mm_maskz_loadu_epi32(col_mask, src + s * 12),
                            3);
    r1 = _mm512_inserti32x4(r1, _mm_maskz_loadu_epi32(col_mask, src + s * 13),
                            3);
    r2 = _mm512_inserti32x4(r2, _mm_maskz_loadu_epi32(col_mask, src + s * 14),
                            3);
    r3 = _mm512_inserti32x4(r3, _mm_maskz_loadu_epi32(col_mask, src + s * 15),
                            3);

    __m512i t0 = _mm512_unpacklo_epi32(r0, r1);
    __m512i t1 = _mm512_unpacklo_epi32(r2, r3);
    __m512i t2 = _mm512_unpackhi_epi32(r0, r1);
    __m512i t3 = _mm512_unpackhi_epi32(r2, r3);

    _mm512_storeu_si512((__m512i*)(dst), _mm512_unpacklo_epi64(t0, t1));
    if (width > 1) {
      _mm512_storeu_si512((__m512i*)(dst + d), _mm512_unpackhi_epi64(t0, t1));
    }
    if (width > 2) {
      _mm512_storeu_si512((__m512i*)(dst + d * 2),
                          _mm512_unpacklo_epi64(t2, t3));
    }
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_TRANSPOSEWX16_32_AVX512BW)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_X86) && (x86 or x64) &&
        // (Visual C or LIBYUV_ENABLE_ROWWIN)
