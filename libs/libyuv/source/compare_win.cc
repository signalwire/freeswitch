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

// This module is for Visual C 32/64 bit, and for clang with
// LIBYUV_ENABLE_ROWWIN, which is also enabled for MemorySanitizer.
#if !defined(LIBYUV_DISABLE_X86) &&                                 \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
     defined(_M_IX86)) &&                                           \
    ((defined(_MSC_VER) && !defined(__clang__)) ||                  \
     defined(LIBYUV_ENABLE_ROWWIN))

#include <immintrin.h>  // For SSE2 to AVX512 intrinsics
#include <string.h>     // For memcpy
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Target attributes allow gcc and clang to build each function for its
// instruction set regardless of the baseline.
#if defined(__clang__) || defined(__GNUC__)
#define LIBYUV_TARGET_SSE2 __attribute__((target("sse2")))
#define LIBYUV_TARGET_SSE41 __attribute__((target("sse4.1")))
#define LIBYUV_TARGET_SSE42 __attribute__((target("sse4.2,popcnt")))
#define LIBYUV_TARGET_AVX2 __attribute__((target("avx2")))
#define LIBYUV_TARGET_AVX512BW \
  __attribute__((target("avx512bw,avx512vl,avx512f")))
#define LIBYUV_TARGET_AVX512VNNI \
  __attribute__((target("avx512vnni,avx512bw,avx512vl,avx512f")))
#else
#define LIBYUV_TARGET_SSE2
#define LIBYUV_TARGET_SSE41
#define LIBYUV_TARGET_SSE42
#define LIBYUV_TARGET_AVX2
#define LIBYUV_TARGET_AVX512BW
#define LIBYUV_TARGET_AVX512VNNI
#endif  // defined(__clang__) || defined(__GNUC__)

#if defined(HAS_HAMMINGDISTANCE_SSE42)
// Count is a multiple of 8.
LIBYUV_TARGET_SSE42
uint32_t HammingDistance_SSE42(const uint8_t* src_a,
                               const uint8_t* src_b,
                               int count) {
  uint32_t diff = 0u;
  int i;
  for (i = 0; i < count; i += 8) {
    uint64_t a;
    uint64_t b;
    memcpy(&a, src_a + i, 8);
    memcpy(&b, src_b + i, 8);
#if defined(__x86_64__) || defined(_M_X64)
    diff += (uint32_t)_mm_popcnt_u64(a ^ b);
#else
    diff += _mm_popcnt_u32((uint32_t)(a ^ b)) +
            _mm_popcnt_u32((uint32_t)((a ^ b) >> 32));
#endif
  }
  return diff;
}
#endif  // HAS_HAMMINGDISTANCE_SSE42

#if defined(HAS_HAMMINGDISTANCE_AVX512BW) ||   \
    defined(HAS_HAMMINGDISTANCE_AVX512VNNI) || \
    defined(HAS_SUMSQUAREERROR_AVX512BW) ||    \
    defined(HAS_SUMSQUAREERROR_AVX512VNNI) ||  \
    defined(HAS_HASHDJB2_AVX512BW) || defined(HAS_SSIM4X4ROW_AVX512BW)
// Mask of the low n bytes, for n = 0 to 64.
static __mmask64 LowBytesMask64(int n) {
  return (__mmask64)(n >= 64 ? ~0ULL : (1ULL << n) - 1);
}
#endif

#if defined(HAS_HAMMINGDISTANCE_AVX2)
// Count is a multiple of 64.
LIBYUV_TARGET_AVX2
uint32_t HammingDistance_AVX2(const uint8_t* src_a,
                              const uint8_t* src_b,
                              int count) {
  const __m256i kNibbleMask = _mm256_set1_epi8(15);
  const __m256i kBitCount = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4));
  const __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256();
  __m128i sum128;
  int i;
  for (i = 0; i < count; i += 64) {
    __m256i x0 =
        _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(src_a + i)),
                         _mm256_loadu_si256((const __m256i*)(src_b + i)));
    __m256i x1 =
        _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(src_a + i + 32)),
                         _mm256_loadu_si256((const __m256i*)(src_b + i + 32)));
    __m256i c0 = _mm256_add_epi8(
        _mm256_shuffle_epi8(kBitCount, _mm256_and_si256(x0, kNibbleMask)),
        _mm256_shuffle_epi8(
            kBitCount,
            _mm256_and_si256(_mm256_srli_epi16(x0, 4), kNibbleMask)));
    __m256i c1 = _mm256_add_epi8(
        _mm256_shuffle_epi8(kBitCount, _mm256_and_si256(x1, kNibbleMask)),
        _mm256_shuffle_epi8(
            kBitCount,
            _mm256_and_si256(_mm256_srli_epi16(x1, 4), kNibbleMask)));
    sum = _mm256_add_epi64(sum, _mm256_sad_epu8(_mm256_add_epi8(c0, c1), zero));
  }
  sum128 = _mm_add_epi64(_mm256_castsi256_si128(sum),
                         _mm256_extracti128_si256(sum, 1));
  sum128 = _mm_add_epi64(sum128, _mm_unpackhi_epi64(sum128, sum128));
  _mm256_zeroupper();
  return (uint32_t)_mm_cvtsi128_si32(sum128);
}
#endif  // HAS_HAMMINGDISTANCE_AVX2

#if defined(HAS_HAMMINGDISTANCE_AVX512BW)
// Bit count of 64 bytes, summed to 8 qwords.
LIBYUV_TARGET_AVX512BW
static __m512i BitCount64_AVX512BW(__m512i x) {
  const __m512i kNibbleMask = _mm512_set1_epi8(15);
  const __m512i kBitCount = _mm512_broadcast_i32x4(
      _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4));
  __m512i lo = _mm512_shuffle_epi8(kBitCount, _mm512_and_si512(x, kNibbleMask));
  __m512i hi = _mm512_shuffle_epi8(
      kBitCount, _mm512_and_si512(_mm512_srli_epi16(x, 4), kNibbleMask));
  return _mm512_sad_epu8(_mm512_add_epi8(lo, hi), _mm512_setzero_si512());
}

// Process 64 bytes per loop and the remainder of 1 to 63 bytes with masked
// loads that read zeros, so any count is supported.
LIBYUV_TARGET_AVX512BW
uint32_t HammingDistance_AVX512BW(const uint8_t* src_a,
                                  const uint8_t* src_b,
                                  int count) {
  __m512i sum = _mm512_setzero_si512();
  while (count >= 64) {
    sum = _mm512_add_epi64(
        sum, BitCount64_AVX512BW(_mm512_xor_si512(_mm512_loadu_si512(src_a),
                                                  _mm512_loadu_si512(src_b))));
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  if (count > 0) {
    const __mmask64 k = LowBytesMask64(count);
    sum = _mm512_add_epi64(sum, BitCount64_AVX512BW(_mm512_xor_si512(
                                    _mm512_maskz_loadu_epi8(k, src_a),
                                    _mm512_maskz_loadu_epi8(k, src_b))));
  }
  return (uint32_t)_mm512_reduce_add_epi64(sum);
}
#endif  // HAS_HAMMINGDISTANCE_AVX512BW

#if defined(HAS_HAMMINGDISTANCE_AVX512VNNI)
// Bit count of 64 bytes accumulated into 16 dwords with vpdpbusd.
LIBYUV_TARGET_AVX512VNNI
static __m512i BitCount64_AVX512VNNI(__m512i x, __m512i sum) {
  const __m512i kNibbleMask = _mm512_set1_epi8(15);
  const __m512i kOnes = _mm512_set1_epi8(1);
  const __m512i kBitCount = _mm512_broadcast_i32x4(
      _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4));
  __m512i lo = _mm512_shuffle_epi8(kBitCount, _mm512_and_si512(x, kNibbleMask));
  __m512i hi = _mm512_shuffle_epi8(
      kBitCount, _mm512_and_si512(_mm512_srli_epi16(x, 4), kNibbleMask));
  return _mm512_dpbusd_epi32(sum, kOnes, _mm512_add_epi8(lo, hi));
}

// Process 64 bytes per loop and the remainder of 1 to 63 bytes with masked
// loads that read zeros, so any count is supported.
LIBYUV_TARGET_AVX512VNNI
uint32_t HammingDistance_AVX512VNNI(const uint8_t* src_a,
                                    const uint8_t* src_b,
                                    int count) {
  __m512i sum = _mm512_setzero_si512();
  while (count >= 64) {
    sum = BitCount64_AVX512VNNI(
        _mm512_xor_si512(_mm512_loadu_si512(src_a), _mm512_loadu_si512(src_b)),
        sum);
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  if (count > 0) {
    const __mmask64 k = LowBytesMask64(count);
    sum = BitCount64_AVX512VNNI(
        _mm512_xor_si512(_mm512_maskz_loadu_epi8(k, src_a),
                         _mm512_maskz_loadu_epi8(k, src_b)),
        sum);
  }
  return (uint32_t)_mm512_reduce_add_epi32(sum);
}
#endif  // HAS_HAMMINGDISTANCE_AVX512VNNI

#if defined(HAS_SUMSQUAREERROR_SSE2)
// Count is a multiple of 16.
LIBYUV_TARGET_SSE2
uint32_t SumSquareError_SSE2(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  int i;
  for (i = 0; i < count; i += 16) {
    __m128i a = _mm_loadu_si128((const __m128i*)(src_a + i));
    __m128i b = _mm_loadu_si128((const __m128i*)(src_b + i));
    __m128i d = _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
    __m128i lo = _mm_unpacklo_epi8(d, zero);
    __m128i hi = _mm_unpackhi_epi8(d, zero);
    sum = _mm_add_epi32(sum, _mm_madd_epi16(lo, lo));
    sum = _mm_add_epi32(sum, _mm_madd_epi16(hi, hi));
  }
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xee));
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x01));
  return (uint32_t)_mm_cvtsi128_si32(sum);
}
#endif  // HAS_SUMSQUAREERROR_SSE2

#if defined(HAS_SUMSQUAREERROR_AVX2)
// Count is a multiple of 32.
LIBYUV_TARGET_AVX2
uint32_t SumSquareError_AVX2(const uint8_t* src_a,
                             const uint8_t* src_b,
                             int count) {
  const __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256();
  __m128i sum128;
  int i;
  for (i = 0; i < count; i += 32) {
    __m256i a = _mm256_loadu_si256((const __m256i*)(src_a + i));
    __m256i b = _mm256_loadu_si256((const __m256i*)(src_b + i));
    __m256i d = _mm256_or_si256(_mm256_subs_epu8(a, b), _mm256_subs_epu8(b, a));
    __m256i lo = _mm256_unpacklo_epi8(d, zero);
    __m256i hi = _mm256_unpackhi_epi8(d, zero);
    sum = _mm256_add_epi32(sum, _mm256_madd_epi16(lo, lo));
    sum = _mm256_add_epi32(sum, _mm256_madd_epi16(hi, hi));
  }
  sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum),
                         _mm256_extracti128_si256(sum, 1));
  sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0xee));
  sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0x01));
  _mm256_zeroupper();
  return (uint32_t)_mm_cvtsi128_si32(sum128);
}
#endif  // HAS_SUMSQUAREERROR_AVX2

#if defined(HAS_SUMSQUAREERROR_AVX512BW)
// Sum of squared differences of 64 bytes, as 16 dwords.
LIBYUV_TARGET_AVX512BW
static __m512i SumSquare64_AVX512BW(__m512i a, __m512i b) {
  const __m512i zero = _mm512_setzero_si512();
  __m512i d = _mm512_or_si512(_mm512_subs_epu8(a, b), _mm512_subs_epu8(b, a));
  __m512i lo = _mm512_unpacklo_epi8(d, zero);
  __m512i hi = _mm512_unpackhi_epi8(d, zero);
  return _mm512_add_epi32(_mm512_madd_epi16(lo, lo), _mm512_madd_epi16(hi, hi));
}

// Process 64 bytes per loop and the remainder of 1 to 63 bytes with masked
// loads that read zeros, so any count is supported.
LIBYUV_TARGET_AVX512BW
uint32_t SumSquareError_AVX512BW(const uint8_t* src_a,
                                 const uint8_t* src_b,
                                 int count) {
  __m512i sum = _mm512_setzero_si512();
  while (count >= 64) {
    sum =
        _mm512_add_epi32(sum, SumSquare64_AVX512BW(_mm512_loadu_si512(src_a),
                                                   _mm512_loadu_si512(src_b)));
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  if (count > 0) {
    const __mmask64 k = LowBytesMask64(count);
    sum = _mm512_add_epi32(
        sum, SumSquare64_AVX512BW(_mm512_maskz_loadu_epi8(k, src_a),
                                  _mm512_maskz_loadu_epi8(k, src_b)));
  }
  return (uint32_t)_mm512_reduce_add_epi32(sum);
}
#endif  // HAS_SUMSQUAREERROR_AVX512BW

#if defined(HAS_SUMSQUAREERROR_AVX512VNNI)
// Process 64 bytes per loop and the remainder of 1 to 63 bytes with masked
// loads that read zeros, so any count is supported.
LIBYUV_TARGET_AVX512VNNI
uint32_t SumSquareError_AVX512VNNI(const uint8_t* src_a,
                                   const uint8_t* src_b,
                                   int count) {
  const __m512i zero = _mm512_setzero_si512();
  __m512i sum0 = _mm512_setzero_si512();
  __m512i sum1 = _mm512_setzero_si512();
  while (count >= 64) {
    __m512i a = _mm512_loadu_si512(src_a);
    __m512i b = _mm512_loadu_si512(src_b);
    __m512i d = _mm512_or_si512(_mm512_subs_epu8(a, b), _mm512_subs_epu8(b, a));
    __m512i lo = _mm512_unpacklo_epi8(d, zero);
    __m512i hi = _mm512_unpackhi_epi8(d, zero);
    sum0 = _mm512_dpwssd_epi32(sum0, lo, lo);
    sum1 = _mm512_dpwssd_epi32(sum1, hi, hi);
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  if (count > 0) {
    const __mmask64 k = LowBytesMask64(count);
    __m512i a = _mm512_maskz_loadu_epi8(k, src_a);
    __m512i b = _mm512_maskz_loadu_epi8(k, src_b);
    __m512i d = _mm512_or_si512(_mm512_subs_epu8(a, b), _mm512_subs_epu8(b, a));
    __m512i lo = _mm512_unpacklo_epi8(d, zero);
    __m512i hi = _mm512_unpackhi_epi8(d, zero);
    sum0 = _mm512_dpwssd_epi32(sum0, lo, lo);
    sum1 = _mm512_dpwssd_epi32(sum1, hi, hi);
  }
  return (uint32_t)_mm512_reduce_add_epi32(_mm512_add_epi32(sum0, sum1));
}
#endif  // HAS_SUMSQUAREERROR_AVX512VNNI

#if defined(HAS_HASHDJB2_SSE41)
// 33 ^ (15 - i) for byte i of 16.
static const uint32_t kHashMul16[16] = {
    0x0c3525e1, 0xa3476dc1, 0x3b4039a1, 0x4f5f0981,  // 33 ^ 15 .. 12
    0x30f35d61, 0x855cb541, 0x040a9121, 0x747c7101,  // 33 ^ 11 .. 8
    0xec41d4e1, 0x4cfa3cc1, 0x025528a1, 0x00121881,  // 33 ^ 7 .. 4
    0x00008c61, 0x00000441, 0x00000021, 0x00000001,  // 33 ^ 3 .. 0
};
static const uint32_t kHash16x33 = 0x92d9e201;  // 33 ^ 16

// Count is a multiple of 16.
LIBYUV_TARGET_SSE41
uint32_t HashDjb2_SSE41(const uint8_t* src, int count, uint32_t seed) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i mul0 = _mm_loadu_si128((const __m128i*)(kHashMul16));
  const __m128i mul1 = _mm_loadu_si128((const __m128i*)(kHashMul16 + 4));
  const __m128i mul2 = _mm_loadu_si128((const __m128i*)(kHashMul16 + 8));
  const __m128i mul3 = _mm_loadu_si128((const __m128i*)(kHashMul16 + 12));
  int i;
  for (i = 0; i < count; i += 16) {
    __m128i s = _mm_loadu_si128((const __m128i*)(src + i));
    __m128i lo = _mm_unpacklo_epi8(s, zero);
    __m128i hi = _mm_unpackhi_epi8(s, zero);
    __m128i sum = _mm_add_epi32(
        _mm_add_epi32(_mm_mullo_epi32(_mm_unpacklo_epi16(lo, zero), mul0),
                      _mm_mullo_epi32(_mm_unpackhi_epi16(lo, zero), mul1)),
        _mm_add_epi32(_mm_mullo_epi32(_mm_unpacklo_epi16(hi, zero), mul2),
                      _mm_mullo_epi32(_mm_unpackhi_epi16(hi, zero), mul3)));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x0e));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x01));
    seed = seed * kHash16x33 + (uint32_t)_mm_cvtsi128_si32(sum);
  }
  return seed;
}
#endif  // HAS_HASHDJB2_SSE41

#if defined(HAS_HASHDJB2_AVX2) || defined(HAS_HASHDJB2_AVX512BW)
// HashDjb2 is hash = hash * 33 ^ n + sum(src[i] * 33 ^ (n - 1 - i)).
// The 32 bit multipliers 33 ^ (63 - i) are split into 16 bit halves so that
// vpmaddwd can be used instead of vpmulld, which is 2 uops and 10 cycles.
// m = hi * 65536 + lo, with lo treated as signed and hi adjusted by the carry,
// so src * m = src * lo + ((src * hi) << 16) modulo 2 ^ 32.
// The first 64 are lo and the next 64 are hi.
static const uint16_t kHashMulLoHi[128] = {
    // lo for 33 ^ 63 .. 33 ^ 0
    0x0be1, 0x93c1, 0x9fa1, 0xaf81, 0x4361, 0xdb41, 0xf721, 0x1701,  //
    0xbae1, 0x62c1, 0x8ea1, 0xbe81, 0x7261, 0x2a41, 0x6621, 0xa601,  //
    0x69e1, 0x31c1, 0x7da1, 0xcd81, 0xa161, 0x7941, 0xd521, 0x3501,  //
    0x18e1, 0x00c1, 0x6ca1, 0xdc81, 0xd061, 0xc841, 0x4421, 0xc401,  //
    0xc7e1, 0xcfc1, 0x5ba1, 0xeb81, 0xff61, 0x1741, 0xb321, 0x5301,  //
    0x76e1, 0x9ec1, 0x4aa1, 0xfa81, 0x2e61, 0x6641, 0x2221, 0xe201,  //
    0x25e1, 0x6dc1, 0x39a1, 0x0981, 0x5d61, 0xb541, 0x9121, 0x7101,  //
    0xd4e1, 0x3cc1, 0x28a1, 0x1881, 0x8c61, 0x0441, 0x0021, 0x0001,  //
    // hi for 33 ^ 63 .. 33 ^ 0
    0xd8be, 0xef4c, 0x6458, 0x6022, 0x40f9, 0x193e, 0x6d5f, 0x31dc,  //
    0x0945, 0xc238, 0x1567, 0xa38f, 0x3380, 0x301b, 0x664e, 0x031a,  //
    0xd18c, 0xb8c6, 0x79f6, 0xae5d, 0x0549, 0xc219, 0x7a3f, 0x1338,  //
    0x0095, 0xc1f5, 0xa107, 0xaf8c, 0x0552, 0x3e39, 0x3830, 0x1138,  //
    0x655f, 0xccc5, 0x9999, 0xd61c, 0x829c, 0x1379, 0x2f23, 0xac18,  //
    0xcee9, 0xc836, 0x72ac, 0x510d, 0xcc27, 0xb0da, 0xee16, 0x92da,  //
    0x0c35, 0xa347, 0x3b40, 0x4f5f, 0x30f3, 0x855d, 0x040b, 0x747c,  //
    0xec42, 0x4cfa, 0x0255, 0x0012, 0x0001, 0x0000, 0x0000, 0x0000,  //
};
#endif

#if defined(HAS_HASHDJB2_AVX2)
// lo + (hi << 16), then add 8 dwords.
LIBYUV_TARGET_AVX2
static uint32_t HashSum_AVX2(__m256i lo, __m256i hi) {
  __m256i s = _mm256_add_epi32(lo, _mm256_slli_epi32(hi, 16));
  __m128i s128 =
      _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1));
  s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0x0e));
  s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0x01));
  return (uint32_t)_mm_cvtsi128_si32(s128);
}

// Count is a multiple of 32. Process 64 bytes per loop with one horizontal
// add, then 32 bytes if needed.
LIBYUV_TARGET_AVX2
uint32_t HashDjb2_AVX2(const uint8_t* src, int count, uint32_t seed) {
  const __m256i* m = (const __m256i*)kHashMulLoHi;
  while (count >= 64) {
    __m256i w0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src));
    __m256i w1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 16)));
    __m256i w2 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 32)));
    __m256i w3 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 48)));
    __m256i lo = _mm256_add_epi32(
        _mm256_add_epi32(_mm256_madd_epi16(w0, _mm256_loadu_si256(m + 0)),
                         _mm256_madd_epi16(w1, _mm256_loadu_si256(m + 1))),
        _mm256_add_epi32(_mm256_madd_epi16(w2, _mm256_loadu_si256(m + 2)),
                         _mm256_madd_epi16(w3, _mm256_loadu_si256(m + 3))));
    __m256i hi = _mm256_add_epi32(
        _mm256_add_epi32(_mm256_madd_epi16(w0, _mm256_loadu_si256(m + 4)),
                         _mm256_madd_epi16(w1, _mm256_loadu_si256(m + 5))),
        _mm256_add_epi32(_mm256_madd_epi16(w2, _mm256_loadu_si256(m + 6)),
                         _mm256_madd_epi16(w3, _mm256_loadu_si256(m + 7))));
    seed = seed * 0xf07f8801u + HashSum_AVX2(lo, hi);  // 33 ^ 64
    src += 64;
    count -= 64;
  }
  if (count > 0) {
    __m256i w0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src));
    __m256i w1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 16)));
    __m256i lo =
        _mm256_add_epi32(_mm256_madd_epi16(w0, _mm256_loadu_si256(m + 2)),
                         _mm256_madd_epi16(w1, _mm256_loadu_si256(m + 3)));
    __m256i hi =
        _mm256_add_epi32(_mm256_madd_epi16(w0, _mm256_loadu_si256(m + 6)),
                         _mm256_madd_epi16(w1, _mm256_loadu_si256(m + 7)));
    seed = seed * 0x1137c401u + HashSum_AVX2(lo, hi);  // 33 ^ 32
  }
  _mm256_zeroupper();
  return seed;
}
#endif  // HAS_HASHDJB2_AVX2

#if defined(HAS_HASHDJB2_AVX512BW)
static uint32_t Pow33(int n) {
  uint32_t r = 1u;
  uint32_t b = 33u;
  while (n) {
    if (n & 1) {
      r *= b;
    }
    b *= b;
    n >>= 1;
  }
  return r;
}

// Sum of 64 bytes, zero extended to words in w0 and w1, times 33 ^ (63 - i).
LIBYUV_TARGET_AVX512BW
static uint32_t HashSum64_AVX512BW(__m512i w0, __m512i w1) {
  const __m512i* m = (const __m512i*)kHashMulLoHi;
  __m512i lo =
      _mm512_add_epi32(_mm512_madd_epi16(w0, _mm512_loadu_si512(m)),
                       _mm512_madd_epi16(w1, _mm512_loadu_si512(m + 1)));
  __m512i hi =
      _mm512_add_epi32(_mm512_madd_epi16(w0, _mm512_loadu_si512(m + 2)),
                       _mm512_madd_epi16(w1, _mm512_loadu_si512(m + 3)));
  return (uint32_t)_mm512_reduce_add_epi32(
      _mm512_add_epi32(lo, _mm512_slli_epi32(hi, 16)));
}

// hash = hash * 33 ^ 64 + sum(src[i] * 33 ^ (63 - i)) for each 64 bytes.
// The remainder of n = 1 to 63 bytes is loaded into the high n bytes with a
// masked load ending at src + count, so the same multipliers apply, and
// hash is multiplied by 33 ^ n.
LIBYUV_TARGET_AVX512BW
uint32_t HashDjb2_AVX512BW(const uint8_t* src, int count, uint32_t seed) {
  int n;
  while (count >= 64) {
    seed = seed * 0xf07f8801u +  // 33 ^ 64
           HashSum64_AVX512BW(
               _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)src)),
               _mm512_cvtepu8_epi16(
                   _mm256_loadu_si256((const __m256i*)(src + 32))));
    src += 64;
    count -= 64;
  }
  n = count;
  if (n) {
    const __mmask64 k = (__mmask64)~LowBytesMask64(64 - n);
    __m512i s = _mm512_maskz_loadu_epi8(k, src + n - 64);
    seed = seed * Pow33(n) +
           HashSum64_AVX512BW(
               _mm512_cvtepu8_epi16(_mm512_castsi512_si256(s)),
               _mm512_cvtepu8_epi16(_mm512_extracti64x4_epi64(s, 1)));
  }
  return seed;
}
#endif  // HAS_HASHDJB2_AVX512BW

#if defined(HAS_SSIM4X4ROW_SSE2)
// Width is a multiple of 16.
LIBYUV_TARGET_SSE2
void Ssim4x4Row_SSE2(const uint8_t* src_a,
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
  const __m128i kMaskFF = _mm_set1_epi16(0x00ff);
  const __m128i kOnes16 = _mm_set1_epi16(1);
  int x;
  for (x = 0; x < width; x += 16) {
    __m128i acc_a = _mm_setzero_si128();
    __m128i acc_b = _mm_setzero_si128();
    __m128i acc_sq = _mm_setzero_si128();
    __m128i acc_axb = _mm_setzero_si128();
#define SSIM_ROW_SSE2_WIN(ar, br)                                              \
  do {                                                                         \
    __m128i va = _mm_loadu_si128((const __m128i*)((ar) + x));                  \
    __m128i vb = _mm_loadu_si128((const __m128i*)((br) + x));                  \
    __m128i a_ev = _mm_and_si128(va, kMaskFF);                                 \
    __m128i a_od = _mm_srli_epi16(va, 8);                                      \
    __m128i b_ev = _mm_and_si128(vb, kMaskFF);                                 \
    __m128i b_od = _mm_srli_epi16(vb, 8);                                      \
    acc_a = _mm_add_epi16(acc_a, _mm_add_epi16(a_ev, a_od));                   \
    acc_b = _mm_add_epi16(acc_b, _mm_add_epi16(b_ev, b_od));                   \
    acc_sq = _mm_add_epi32(acc_sq, _mm_add_epi32(_mm_madd_epi16(a_ev, a_ev),   \
                                                 _mm_madd_epi16(a_od, a_od))); \
    acc_sq = _mm_add_epi32(acc_sq, _mm_add_epi32(_mm_madd_epi16(b_ev, b_ev),   \
                                                 _mm_madd_epi16(b_od, b_od))); \
    acc_axb =                                                                  \
        _mm_add_epi32(acc_axb, _mm_add_epi32(_mm_madd_epi16(a_ev, b_ev),       \
                                             _mm_madd_epi16(a_od, b_od)));     \
  } while (0)
    SSIM_ROW_SSE2_WIN(a0, b0);
    SSIM_ROW_SSE2_WIN(a1, b1);
    SSIM_ROW_SSE2_WIN(a2, b2);
    SSIM_ROW_SSE2_WIN(a3, b3);
#undef SSIM_ROW_SSE2_WIN
    acc_a = _mm_madd_epi16(acc_a, kOnes16);
    acc_b = _mm_madd_epi16(acc_b, kOnes16);
    {
      __m128i t0 = _mm_unpacklo_epi32(acc_a, acc_b);
      __m128i t1 = _mm_unpackhi_epi32(acc_a, acc_b);
      __m128i t2 = _mm_unpacklo_epi32(acc_sq, acc_axb);
      __m128i t3 = _mm_unpackhi_epi32(acc_sq, acc_axb);
      _mm_storeu_si128((__m128i*)(dst + 0), _mm_unpacklo_epi64(t0, t2));
      _mm_storeu_si128((__m128i*)(dst + 4), _mm_unpackhi_epi64(t0, t2));
      _mm_storeu_si128((__m128i*)(dst + 8), _mm_unpacklo_epi64(t1, t3));
      _mm_storeu_si128((__m128i*)(dst + 12), _mm_unpackhi_epi64(t1, t3));
    }
    dst += 16;
  }
}
#endif  // HAS_SSIM4X4ROW_SSE2

#if defined(HAS_SSIM4X4ROW_AVX2)
// Width is a multiple of 32.
LIBYUV_TARGET_AVX2
void Ssim4x4Row_AVX2(const uint8_t* src_a,
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
  const __m256i kMaskFF = _mm256_set1_epi16(0x00ff);
  const __m256i kOnes16 = _mm256_set1_epi16(1);
  int x;
  for (x = 0; x < width; x += 32) {
    __m256i acc_a = _mm256_setzero_si256();
    __m256i acc_b = _mm256_setzero_si256();
    __m256i acc_sq = _mm256_setzero_si256();
    __m256i acc_axb = _mm256_setzero_si256();
#define SSIM_ROW_AVX2_WIN(ar, br)                                  \
  do {                                                             \
    __m256i va = _mm256_loadu_si256((const __m256i*)((ar) + x));   \
    __m256i vb = _mm256_loadu_si256((const __m256i*)((br) + x));   \
    __m256i a_ev = _mm256_and_si256(va, kMaskFF);                  \
    __m256i a_od = _mm256_srli_epi16(va, 8);                       \
    __m256i b_ev = _mm256_and_si256(vb, kMaskFF);                  \
    __m256i b_od = _mm256_srli_epi16(vb, 8);                       \
    acc_a = _mm256_add_epi16(acc_a, _mm256_add_epi16(a_ev, a_od)); \
    acc_b = _mm256_add_epi16(acc_b, _mm256_add_epi16(b_ev, b_od)); \
    acc_sq = _mm256_add_epi32(                                     \
        acc_sq, _mm256_add_epi32(_mm256_madd_epi16(a_ev, a_ev),    \
                                 _mm256_madd_epi16(a_od, a_od)));  \
    acc_sq = _mm256_add_epi32(                                     \
        acc_sq, _mm256_add_epi32(_mm256_madd_epi16(b_ev, b_ev),    \
                                 _mm256_madd_epi16(b_od, b_od)));  \
    acc_axb = _mm256_add_epi32(                                    \
        acc_axb, _mm256_add_epi32(_mm256_madd_epi16(a_ev, b_ev),   \
                                  _mm256_madd_epi16(a_od, b_od))); \
  } while (0)
    SSIM_ROW_AVX2_WIN(a0, b0);
    SSIM_ROW_AVX2_WIN(a1, b1);
    SSIM_ROW_AVX2_WIN(a2, b2);
    SSIM_ROW_AVX2_WIN(a3, b3);
#undef SSIM_ROW_AVX2_WIN
    acc_a = _mm256_madd_epi16(acc_a, kOnes16);
    acc_b = _mm256_madd_epi16(acc_b, kOnes16);
    {
      __m256i t0 = _mm256_unpacklo_epi32(acc_a, acc_b);
      __m256i t1 = _mm256_unpackhi_epi32(acc_a, acc_b);
      __m256i t2 = _mm256_unpacklo_epi32(acc_sq, acc_axb);
      __m256i t3 = _mm256_unpackhi_epi32(acc_sq, acc_axb);
      __m256i u0 = _mm256_unpacklo_epi64(t0, t2);
      __m256i u1 = _mm256_unpackhi_epi64(t0, t2);
      __m256i u2 = _mm256_unpacklo_epi64(t1, t3);
      __m256i u3 = _mm256_unpackhi_epi64(t1, t3);
      _mm256_storeu_si256((__m256i*)(dst + 0),
                          _mm256_permute2x128_si256(u0, u1, 0x20));
      _mm256_storeu_si256((__m256i*)(dst + 8),
                          _mm256_permute2x128_si256(u2, u3, 0x20));
      _mm256_storeu_si256((__m256i*)(dst + 16),
                          _mm256_permute2x128_si256(u0, u1, 0x31));
      _mm256_storeu_si256((__m256i*)(dst + 24),
                          _mm256_permute2x128_si256(u2, u3, 0x31));
    }
    dst += 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SSIM4X4ROW_AVX2

#if defined(HAS_SSIM4X4ROW_AVX512BW)
// Width is a multiple of 4.
LIBYUV_TARGET_AVX512BW
void Ssim4x4Row_AVX512BW(const uint8_t* src_a,
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
  const __m512i kMaskFF = _mm512_set1_epi16(0x00ff);
  const __m512i kOnes16 = _mm512_set1_epi16(1);
#define SSIM_ROW_AVX512_WIN(va, vb)                                \
  do {                                                             \
    __m512i a_ev = _mm512_and_si512(va, kMaskFF);                  \
    __m512i a_od = _mm512_srli_epi16(va, 8);                       \
    __m512i b_ev = _mm512_and_si512(vb, kMaskFF);                  \
    __m512i b_od = _mm512_srli_epi16(vb, 8);                       \
    acc_a = _mm512_add_epi16(acc_a, _mm512_add_epi16(a_ev, a_od)); \
    acc_b = _mm512_add_epi16(acc_b, _mm512_add_epi16(b_ev, b_od)); \
    acc_sq = _mm512_add_epi32(                                     \
        acc_sq, _mm512_add_epi32(_mm512_madd_epi16(a_ev, a_ev),    \
                                 _mm512_madd_epi16(a_od, a_od)));  \
    acc_sq = _mm512_add_epi32(                                     \
        acc_sq, _mm512_add_epi32(_mm512_madd_epi16(b_ev, b_ev),    \
                                 _mm512_madd_epi16(b_od, b_od)));  \
    acc_axb = _mm512_add_epi32(                                    \
        acc_axb, _mm512_add_epi32(_mm512_madd_epi16(a_ev, b_ev),   \
                                  _mm512_madd_epi16(a_od, b_od))); \
  } while (0)
#define SSIM_TRANSPOSE_AVX512_WIN()                    \
  acc_a = _mm512_madd_epi16(acc_a, kOnes16);           \
  acc_b = _mm512_madd_epi16(acc_b, kOnes16);           \
  __m512i t0 = _mm512_unpacklo_epi32(acc_a, acc_b);    \
  __m512i t1 = _mm512_unpackhi_epi32(acc_a, acc_b);    \
  __m512i t2 = _mm512_unpacklo_epi32(acc_sq, acc_axb); \
  __m512i t3 = _mm512_unpackhi_epi32(acc_sq, acc_axb); \
  __m512i u0 = _mm512_unpacklo_epi64(t0, t2);          \
  __m512i u1 = _mm512_unpackhi_epi64(t0, t2);          \
  __m512i u2 = _mm512_unpacklo_epi64(t1, t3);          \
  __m512i u3 = _mm512_unpackhi_epi64(t1, t3);          \
  __m512i v0 = _mm512_shuffle_i32x4(u0, u1, 0x44);     \
  __m512i v1 = _mm512_shuffle_i32x4(u2, u3, 0x44);     \
  __m512i v2 = _mm512_shuffle_i32x4(u0, u1, 0xee);     \
  __m512i v3 = _mm512_shuffle_i32x4(u2, u3, 0xee);     \
  __m512i out0 = _mm512_shuffle_i32x4(v0, v1, 0x88);   \
  __m512i out1 = _mm512_shuffle_i32x4(v0, v1, 0xdd);   \
  __m512i out2 = _mm512_shuffle_i32x4(v2, v3, 0x88);   \
  __m512i out3 = _mm512_shuffle_i32x4(v2, v3, 0xdd)

  while (width >= 64) {
    __m512i acc_a = _mm512_setzero_si512();
    __m512i acc_b = _mm512_setzero_si512();
    __m512i acc_sq = _mm512_setzero_si512();
    __m512i acc_axb = _mm512_setzero_si512();
    SSIM_ROW_AVX512_WIN(_mm512_loadu_si512(a0), _mm512_loadu_si512(b0));
    SSIM_ROW_AVX512_WIN(_mm512_loadu_si512(a1), _mm512_loadu_si512(b1));
    SSIM_ROW_AVX512_WIN(_mm512_loadu_si512(a2), _mm512_loadu_si512(b2));
    SSIM_ROW_AVX512_WIN(_mm512_loadu_si512(a3), _mm512_loadu_si512(b3));
    SSIM_TRANSPOSE_AVX512_WIN();
    _mm512_storeu_si512(dst + 0, out0);
    _mm512_storeu_si512(dst + 16, out1);
    _mm512_storeu_si512(dst + 32, out2);
    _mm512_storeu_si512(dst + 48, out3);
    a0 += 64;
    a1 += 64;
    a2 += 64;
    a3 += 64;
    b0 += 64;
    b1 += 64;
    b2 += 64;
    b3 += 64;
    dst += 64;
    width -= 64;
  }
  if (width > 0) {
    const __mmask16 load_mask = (__mmask16)((1U << (width >> 2)) - 1U);
    const uint64_t store_mask = LowBytesMask64(width);
    __m512i acc_a = _mm512_setzero_si512();
    __m512i acc_b = _mm512_setzero_si512();
    __m512i acc_sq = _mm512_setzero_si512();
    __m512i acc_axb = _mm512_setzero_si512();
    SSIM_ROW_AVX512_WIN(_mm512_maskz_loadu_epi32(load_mask, a0),
                        _mm512_maskz_loadu_epi32(load_mask, b0));
    SSIM_ROW_AVX512_WIN(_mm512_maskz_loadu_epi32(load_mask, a1),
                        _mm512_maskz_loadu_epi32(load_mask, b1));
    SSIM_ROW_AVX512_WIN(_mm512_maskz_loadu_epi32(load_mask, a2),
                        _mm512_maskz_loadu_epi32(load_mask, b2));
    SSIM_ROW_AVX512_WIN(_mm512_maskz_loadu_epi32(load_mask, a3),
                        _mm512_maskz_loadu_epi32(load_mask, b3));
    SSIM_TRANSPOSE_AVX512_WIN();
    _mm512_mask_storeu_epi32(dst + 0, (__mmask16)store_mask, out0);
    _mm512_mask_storeu_epi32(dst + 16, (__mmask16)(store_mask >> 16), out1);
    _mm512_mask_storeu_epi32(dst + 32, (__mmask16)(store_mask >> 32), out2);
    _mm512_mask_storeu_epi32(dst + 48, (__mmask16)(store_mask >> 48), out3);
  }
#undef SSIM_ROW_AVX512_WIN
#undef SSIM_TRANSPOSE_AVX512_WIN
}
#endif  // HAS_SSIM4X4ROW_AVX512BW

#if defined(HAS_SSIMWINDOWROW_SSE2)
// num_windows is a multiple of 4.
LIBYUV_TARGET_SSE2
float SsimWindowRow_SSE2(const uint32_t* row0,
                         const uint32_t* row1,
                         int num_windows) {
  const __m128i vc1 = _mm_set1_epi32(26634);
  const __m128i vc2 = _mm_set1_epi32(239708);
  __m128 total = _mm_setzero_ps();
  __m128i t0 = _mm_add_epi32(_mm_loadu_si128((const __m128i*)row0),
                             _mm_loadu_si128((const __m128i*)row1));
  int w;
  for (w = 0; w < num_windows; w += 4) {
    __m128i t1 = _mm_add_epi32(_mm_loadu_si128((const __m128i*)(row0 + 4)),
                               _mm_loadu_si128((const __m128i*)(row1 + 4)));
    __m128i t2 = _mm_add_epi32(_mm_loadu_si128((const __m128i*)(row0 + 8)),
                               _mm_loadu_si128((const __m128i*)(row1 + 8)));
    __m128i t3 = _mm_add_epi32(_mm_loadu_si128((const __m128i*)(row0 + 12)),
                               _mm_loadu_si128((const __m128i*)(row1 + 12)));
    __m128i t4 = _mm_add_epi32(_mm_loadu_si128((const __m128i*)(row0 + 16)),
                               _mm_loadu_si128((const __m128i*)(row1 + 16)));
    row0 += 16;
    row1 += 16;
    __m128i w0 = _mm_add_epi32(t0, t1);
    __m128i w1 = _mm_add_epi32(t1, t2);
    __m128i w2 = _mm_add_epi32(t2, t3);
    __m128i w3 = _mm_add_epi32(t3, t4);
    t0 = t4;

    __m128i u0 = _mm_unpacklo_epi32(w0, w1);
    __m128i u1 = _mm_unpackhi_epi32(w0, w1);
    __m128i u2 = _mm_unpacklo_epi32(w2, w3);
    __m128i u3 = _mm_unpackhi_epi32(w2, w3);
    __m128i sum_a = _mm_unpacklo_epi64(u0, u2);
    __m128i sum_b = _mm_unpackhi_epi64(u0, u2);
    __m128i sum_sq = _mm_unpacklo_epi64(u1, u3);
    __m128i sum_axb = _mm_unpackhi_epi64(u1, u3);

    __m128i ab2 = _mm_slli_epi32(_mm_madd_epi16(sum_a, sum_b), 1);
    __m128i a2b2 = _mm_add_epi32(_mm_madd_epi16(sum_a, sum_a),
                                 _mm_madd_epi16(sum_b, sum_b));
    __m128i n1 = _mm_add_epi32(ab2, vc1);
    __m128i n2 =
        _mm_add_epi32(_mm_sub_epi32(_mm_slli_epi32(sum_axb, 7), ab2), vc2);
    __m128i d1 = _mm_add_epi32(a2b2, vc1);
    __m128i d2 =
        _mm_add_epi32(_mm_sub_epi32(_mm_slli_epi32(sum_sq, 6), a2b2), vc2);

    __m128 num = _mm_mul_ps(_mm_cvtepi32_ps(n1), _mm_cvtepi32_ps(n2));
    __m128 den = _mm_mul_ps(_mm_cvtepi32_ps(d1), _mm_cvtepi32_ps(d2));
    total = _mm_add_ps(total, _mm_div_ps(num, den));
  }
  total = _mm_add_ps(total, _mm_movehl_ps(total, total));
  total = _mm_add_ss(total, _mm_shuffle_ps(total, total, 1));
  return _mm_cvtss_f32(total);
}
#endif  // HAS_SSIMWINDOWROW_SSE2

#if defined(HAS_SSIMWINDOWROW_AVX2)
// num_windows is a multiple of 8.
LIBYUV_TARGET_AVX2
float SsimWindowRow_AVX2(const uint32_t* row0,
                         const uint32_t* row1,
                         int num_windows) {
  const __m256i vc1 = _mm256_set1_epi32(26634);
  const __m256i vc2 = _mm256_set1_epi32(239708);
  __m256 total = _mm256_setzero_ps();
  __m128 total128;
  int w;
  for (w = 0; w < num_windows; w += 8) {
    __m256i e01 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 0)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 0)));
    __m256i o12 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 4)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 4)));
    __m256i e23 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 8)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 8)));
    __m256i o34 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 12)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 12)));
    __m256i e45 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 16)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 16)));
    __m256i o56 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 20)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 20)));
    __m256i e67 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 24)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 24)));
    __m256i o78 =
        _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)(row0 + 28)),
                         _mm256_loadu_si256((const __m256i*)(row1 + 28)));
    row0 += 32;
    row1 += 32;
    __m256i w0 = _mm256_add_epi32(e01, o12);
    __m256i w1 = _mm256_add_epi32(e23, o34);
    __m256i w2 = _mm256_add_epi32(e45, o56);
    __m256i w3 = _mm256_add_epi32(e67, o78);

    __m256i u0 = _mm256_unpacklo_epi32(w0, w1);
    __m256i u1 = _mm256_unpackhi_epi32(w0, w1);
    __m256i u2 = _mm256_unpacklo_epi32(w2, w3);
    __m256i u3 = _mm256_unpackhi_epi32(w2, w3);
    __m256i sum_a = _mm256_unpacklo_epi64(u0, u2);
    __m256i sum_b = _mm256_unpackhi_epi64(u0, u2);
    __m256i sum_sq = _mm256_unpacklo_epi64(u1, u3);
    __m256i sum_axb = _mm256_unpackhi_epi64(u1, u3);

    __m256i ab2 = _mm256_slli_epi32(_mm256_madd_epi16(sum_a, sum_b), 1);
    __m256i a2b2 = _mm256_add_epi32(_mm256_madd_epi16(sum_a, sum_a),
                                    _mm256_madd_epi16(sum_b, sum_b));
    __m256i n1 = _mm256_add_epi32(ab2, vc1);
    __m256i n2 = _mm256_add_epi32(
        _mm256_sub_epi32(_mm256_slli_epi32(sum_axb, 7), ab2), vc2);
    __m256i d1 = _mm256_add_epi32(a2b2, vc1);
    __m256i d2 = _mm256_add_epi32(
        _mm256_sub_epi32(_mm256_slli_epi32(sum_sq, 6), a2b2), vc2);

    __m256 num = _mm256_mul_ps(_mm256_cvtepi32_ps(n1), _mm256_cvtepi32_ps(n2));
    __m256 den = _mm256_mul_ps(_mm256_cvtepi32_ps(d1), _mm256_cvtepi32_ps(d2));
    total = _mm256_add_ps(total, _mm256_div_ps(num, den));
  }
  total128 = _mm_add_ps(_mm256_castps256_ps128(total),
                        _mm256_extractf128_ps(total, 1));
  total128 = _mm_add_ps(total128, _mm_movehl_ps(total128, total128));
  total128 = _mm_add_ss(total128, _mm_shuffle_ps(total128, total128, 1));
  _mm256_zeroupper();
  return _mm_cvtss_f32(total128);
}
#endif  // HAS_SSIMWINDOWROW_AVX2

#if defined(HAS_SSIMWINDOWROW_AVX512BW)
LIBYUV_TARGET_AVX512BW
float SsimWindowRow_AVX512BW(const uint32_t* row0,
                             const uint32_t* row1,
                             int num_windows) {
  const __m512i vc1 = _mm512_set1_epi32(26634);
  const __m512i vc2 = _mm512_set1_epi32(239708);
  __m512 total = _mm512_setzero_ps();
#define SSIM_WINDOW_AVX512_WIN(w0, w1, w2, w3)                                \
  __m512i u0 = _mm512_unpacklo_epi32(w0, w1);                                 \
  __m512i u1 = _mm512_unpackhi_epi32(w0, w1);                                 \
  __m512i u2 = _mm512_unpacklo_epi32(w2, w3);                                 \
  __m512i u3 = _mm512_unpackhi_epi32(w2, w3);                                 \
  __m512i sum_a = _mm512_unpacklo_epi64(u0, u2);                              \
  __m512i sum_b = _mm512_unpackhi_epi64(u0, u2);                              \
  __m512i sum_sq = _mm512_unpacklo_epi64(u1, u3);                             \
  __m512i sum_axb = _mm512_unpackhi_epi64(u1, u3);                            \
  __m512i ab2 = _mm512_slli_epi32(_mm512_madd_epi16(sum_a, sum_b), 1);        \
  __m512i a2b2 = _mm512_add_epi32(_mm512_madd_epi16(sum_a, sum_a),            \
                                  _mm512_madd_epi16(sum_b, sum_b));           \
  __m512i n1 = _mm512_add_epi32(ab2, vc1);                                    \
  __m512i n2 = _mm512_add_epi32(                                              \
      _mm512_sub_epi32(_mm512_slli_epi32(sum_axb, 7), ab2), vc2);             \
  __m512i d1 = _mm512_add_epi32(a2b2, vc1);                                   \
  __m512i d2 = _mm512_add_epi32(                                              \
      _mm512_sub_epi32(_mm512_slli_epi32(sum_sq, 6), a2b2), vc2);             \
  __m512 num = _mm512_mul_ps(_mm512_cvtepi32_ps(n1), _mm512_cvtepi32_ps(n2)); \
  __m512 den = _mm512_mul_ps(_mm512_cvtepi32_ps(d1), _mm512_cvtepi32_ps(d2)); \
  __m512 ssim = _mm512_div_ps(num, den)

  while (num_windows >= 16) {
    __m512i t0 = _mm512_add_epi32(_mm512_loadu_si512(row0 + 0),
                                  _mm512_loadu_si512(row1 + 0));
    __m512i t1 = _mm512_add_epi32(_mm512_loadu_si512(row0 + 16),
                                  _mm512_loadu_si512(row1 + 16));
    __m512i t2 = _mm512_add_epi32(_mm512_loadu_si512(row0 + 32),
                                  _mm512_loadu_si512(row1 + 32));
    __m512i t3 = _mm512_add_epi32(_mm512_loadu_si512(row0 + 48),
                                  _mm512_loadu_si512(row1 + 48));
    __m512i t4 = _mm512_add_epi32(_mm512_loadu_si512(row0 + 52),
                                  _mm512_loadu_si512(row1 + 52));
    row0 += 64;
    row1 += 64;
    num_windows -= 16;
    __m512i w0 = _mm512_add_epi32(t0, _mm512_alignr_epi32(t1, t0, 4));
    __m512i w1 = _mm512_add_epi32(t1, _mm512_alignr_epi32(t2, t1, 4));
    __m512i w2 = _mm512_add_epi32(t2, _mm512_alignr_epi32(t3, t2, 4));
    __m512i w3 = _mm512_add_epi32(t3, t4);
    SSIM_WINDOW_AVX512_WIN(w0, w1, w2, w3);
    total = _mm512_add_ps(total, ssim);
  }
  if (num_windows > 0) {
    const uint64_t load_mask = ~0ULL >> (60 - num_windows * 4);
    const __mmask16 win_mask =
        (__mmask16)(((0x01110U >> (16 - (num_windows & 3) * 4))
                     << (num_windows >> 2)) +
                    (0x1111U << (num_windows >> 2)) - 0x1111U);
    const __mmask16 m0 = (__mmask16)load_mask;
    const __mmask16 m1 = (__mmask16)(load_mask >> 16);
    const __mmask16 m2 = (__mmask16)(load_mask >> 32);
    const __mmask16 m3 = (__mmask16)(load_mask >> 48);
    __m512i t0 = _mm512_add_epi32(_mm512_maskz_loadu_epi32(m0, row0 + 0),
                                  _mm512_maskz_loadu_epi32(m0, row1 + 0));
    __m512i t1 = _mm512_add_epi32(_mm512_maskz_loadu_epi32(m1, row0 + 16),
                                  _mm512_maskz_loadu_epi32(m1, row1 + 16));
    __m512i t2 = _mm512_add_epi32(_mm512_maskz_loadu_epi32(m2, row0 + 32),
                                  _mm512_maskz_loadu_epi32(m2, row1 + 32));
    __m512i t3 = _mm512_add_epi32(_mm512_maskz_loadu_epi32(m3, row0 + 48),
                                  _mm512_maskz_loadu_epi32(m3, row1 + 48));
    __m512i w0 = _mm512_add_epi32(t0, _mm512_alignr_epi32(t1, t0, 4));
    __m512i w1 = _mm512_add_epi32(t1, _mm512_alignr_epi32(t2, t1, 4));
    __m512i w2 = _mm512_add_epi32(t2, _mm512_alignr_epi32(t3, t2, 4));
    __m512i w3 = _mm512_add_epi32(t3, _mm512_alignr_epi32(t3, t3, 4));
    SSIM_WINDOW_AVX512_WIN(w0, w1, w2, w3);
    total = _mm512_mask_add_ps(total, win_mask, total, ssim);
  }
#undef SSIM_WINDOW_AVX512_WIN
  return _mm512_reduce_add_ps(total);
}
#endif  // HAS_SSIMWINDOWROW_AVX512BW

#if defined(HAS_ARGBDETECTROW_AVX2)
LIBYUV_TARGET_AVX2
uint32_t ARGBDetectRow_AVX2(const uint8_t* argb, int width) {
  if (width >= 8) {
    const __m256i mask_rgb = _mm256_set1_epi32(0x00ffff00);
    const __m256i ones = _mm256_set1_epi8(-1);
    do {
      __m256i v =
          _mm256_or_si256(_mm256_loadu_si256((const __m256i*)argb), mask_rgb);
      uint32_t diff =
          ~(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, ones));
      if (diff) {
#if defined(_MSC_VER) && !defined(__clang__)
        unsigned long bit;  // NOLINT(runtime/int)
        _BitScanForward(&bit, diff);
#else
        int bit = __builtin_ctz(diff);
#endif
        _mm256_zeroupper();
        return (bit & 3) == 0 ? FOURCC_BGRA : FOURCC_ARGB;
      }
      argb += 32;
      width -= 8;
    } while (width >= 8);
    _mm256_zeroupper();
  }
  if (width > 0) {
    const __m128i mask_rgb = _mm_set1_epi32(0x00ffff00);
    const __m128i ones = _mm_set1_epi8(-1);
    do {
      __m128i v = _mm_or_si128(_mm_loadu_si32(argb), mask_rgb);
      uint32_t diff =
          (~(uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, ones))) & 0x9u;
      if (diff) {
        return (diff & 1u) ? FOURCC_BGRA : FOURCC_ARGB;
      }
      argb += 4;
      width -= 1;
    } while (width > 0);
  }
  return 0;
}
#endif  // HAS_ARGBDETECTROW_AVX2

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_X86) && (x86 or x64) &&
        // (Visual C or LIBYUV_ENABLE_ROWWIN)
