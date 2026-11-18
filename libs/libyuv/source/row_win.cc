/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_from_argb.h"  // For ArgbConstants
#include "libyuv/row.h"

// This module is for Visual C 32/64 bit
#if !defined(LIBYUV_DISABLE_X86) &&                                 \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
     defined(_M_IX86)) &&                                           \
    ((defined(_MSC_VER) && !defined(__clang__)) ||                  \
     defined(LIBYUV_ENABLE_ROWWIN))

#include <emmintrin.h>
#include <immintrin.h>  // For AVX2 intrinsics
#include <string.h>     // For memcpy
#include <tmmintrin.h>  // For _mm_maddubs_epi16
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define LIBYUV_TARGET_SSSE3 __attribute__((target("ssse3")))
#define LIBYUV_TARGET_AVX __attribute__((target("avx")))
#define LIBYUV_TARGET_AVX2 __attribute__((target("avx2")))
#define LIBYUV_TARGET_AVX2_FMA __attribute__((target("avx2,fma")))
#define LIBYUV_TARGET_AVX512BW \
  __attribute__((target("avx512bw,avx512vl,avx512f")))
#define LIBYUV_TARGET_AVX512VBMI \
  __attribute__((target("avx512vbmi,avx512bw,avx512vl,avx512f")))
#define LIBYUV_TARGET_AVX512VNNI \
  __attribute__((target("avx512vnni,avx512bw,avx512vl,avx512f")))
#define LIBYUV_TARGET_AVX512VNNIVBMI \
  __attribute__((target("avx512vnni,avx512vbmi,avx512bw,avx512vl,avx512f")))
#else
#define LIBYUV_TARGET_SSSE3
#define LIBYUV_TARGET_AVX
#define LIBYUV_TARGET_AVX2
#define LIBYUV_TARGET_AVX2_FMA
#define LIBYUV_TARGET_AVX512BW
#define LIBYUV_TARGET_AVX512VBMI
#define LIBYUV_TARGET_AVX512VNNI
#define LIBYUV_TARGET_AVX512VNNIVBMI
#endif  // defined(__clang__) || defined(__GNUC__)

#if defined(__x86_64__) || defined(_M_X64)
// Under MemorySanitizer, use portable lane-by-lane C loops for single-source
// variable permutes and 512-bit float-to-int truncation so shadow propagation
// relies only on standard C memory/array semantics rather than target-specific
// LLVM intrinsic modeling. In non-MSan builds, call the native AVX-512
// intrinsics directly.
LIBYUV_TARGET_AVX512VBMI
static inline __m512i PermutexvarEpi8(__m512i idx, __m512i v) {
#if defined(LIBYUV_MSAN)
  uint8_t idx_arr[64];
  uint8_t v_arr[64];
  uint8_t out_arr[64];
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 64; ++i) {
    out_arr[i] = v_arr[idx_arr[i] & 63];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_permutexvar_epi8(idx, v);
#endif
}

LIBYUV_TARGET_AVX512VBMI
static inline __m512i MaskPermutexvarEpi8(__m512i src,
                                          __mmask64 k,
                                          __m512i idx,
                                          __m512i v) {
#if defined(LIBYUV_MSAN)
  uint8_t src_arr[64];
  uint8_t idx_arr[64];
  uint8_t v_arr[64];
  uint8_t out_arr[64];
  _mm512_storeu_si512((__m512i*)src_arr, src);
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 64; ++i) {
    out_arr[i] = ((k >> i) & 1) ? v_arr[idx_arr[i] & 63] : src_arr[i];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_mask_permutexvar_epi8(src, k, idx, v);
#endif
}

LIBYUV_TARGET_AVX512BW
static inline __m512i PermutexvarEpi16(__m512i idx, __m512i v) {
#if defined(LIBYUV_MSAN)
  uint16_t idx_arr[32];
  uint16_t v_arr[32];
  uint16_t out_arr[32];
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 32; ++i) {
    out_arr[i] = v_arr[idx_arr[i] & 31];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_permutexvar_epi16(idx, v);
#endif
}

LIBYUV_TARGET_AVX512BW
static inline __m512i PermutexvarEpi32(__m512i idx, __m512i v) {
#if defined(LIBYUV_MSAN)
  uint32_t idx_arr[16];
  uint32_t v_arr[16];
  uint32_t out_arr[16];
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 16; ++i) {
    out_arr[i] = v_arr[idx_arr[i] & 15];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_permutexvar_epi32(idx, v);
#endif
}

LIBYUV_TARGET_AVX512BW
static inline __m512i PermutexvarEpi64(__m512i idx, __m512i v) {
#if defined(LIBYUV_MSAN)
  uint64_t idx_arr[8];
  uint64_t v_arr[8];
  uint64_t out_arr[8];
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 8; ++i) {
    out_arr[i] = v_arr[idx_arr[i] & 7];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_permutexvar_epi64(idx, v);
#endif
}

LIBYUV_TARGET_AVX512BW
static inline __m512i CvttpsEpi32(__m512 v) {
#if defined(LIBYUV_MSAN)
  float v_arr[16];
  int32_t out_arr[16];
  _mm512_storeu_ps(v_arr, v);
  for (int i = 0; i < 16; ++i) {
    out_arr[i] = (int32_t)v_arr[i];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_cvttps_epi32(v);
#endif
}
#endif  // defined(__x86_64__) || defined(_M_X64)

// Read 8 UV from 444
#define READYUV444                                    \
  xmm3 = _mm_loadl_epi64((__m128i*)u_buf);            \
  xmm1 = _mm_loadl_epi64((__m128i*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);               \
  u_buf += 8;                                         \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);            \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);               \
  y_buf += 8;

// Read 8 UV from 444, With 8 Alpha.
#define READYUVA444                                   \
  xmm3 = _mm_loadl_epi64((__m128i*)u_buf);            \
  xmm1 = _mm_loadl_epi64((__m128i*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);               \
  u_buf += 8;                                         \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);            \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);               \
  y_buf += 8;                                         \
  xmm5 = _mm_loadl_epi64((__m128i*)a_buf);            \
  a_buf += 8;

// Read 4 UV from 422, upsample to 8 UV.
#define READYUV422                                        \
  xmm3 = _mm_cvtsi32_si128(*(uint32_t*)u_buf);            \
  xmm1 = _mm_cvtsi32_si128(*(uint32_t*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);                   \
  xmm3 = _mm_unpacklo_epi16(xmm3, xmm3);                  \
  u_buf += 4;                                             \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);                \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);                   \
  y_buf += 8;

// Read 4 UV from 422, upsample to 8 UV.  With 8 Alpha.
#define READYUVA422                                       \
  xmm3 = _mm_cvtsi32_si128(*(uint32_t*)u_buf);            \
  xmm1 = _mm_cvtsi32_si128(*(uint32_t*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);                   \
  xmm3 = _mm_unpacklo_epi16(xmm3, xmm3);                  \
  u_buf += 4;                                             \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);                \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);                   \
  y_buf += 8;                                             \
  xmm5 = _mm_loadl_epi64((__m128i*)a_buf);                \
  a_buf += 8;

// Convert 8 pixels: 8 UV and 8 Y.
#define YUVTORGB(yuvconstants)                                      \
  xmm3 = _mm_sub_epi8(xmm3, _mm_set1_epi8((char)0x80));             \
  xmm4 = _mm_mulhi_epu16(xmm4, *(__m128i*)yuvconstants->kYToRgb);   \
  xmm4 = _mm_add_epi16(xmm4, *(__m128i*)yuvconstants->kYBiasToRgb); \
  xmm0 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToB, xmm3);  \
  xmm1 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToG, xmm3);  \
  xmm2 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToR, xmm3);  \
  xmm0 = _mm_adds_epi16(xmm4, xmm0);                                \
  xmm1 = _mm_subs_epi16(xmm4, xmm1);                                \
  xmm2 = _mm_adds_epi16(xmm4, xmm2);                                \
  xmm0 = _mm_srai_epi16(xmm0, 6);                                   \
  xmm1 = _mm_srai_epi16(xmm1, 6);                                   \
  xmm2 = _mm_srai_epi16(xmm2, 6);                                   \
  xmm0 = _mm_packus_epi16(xmm0, xmm0);                              \
  xmm1 = _mm_packus_epi16(xmm1, xmm1);                              \
  xmm2 = _mm_packus_epi16(xmm2, xmm2);

// Store 8 ARGB values.
#define STOREARGB                                    \
  xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);              \
  xmm2 = _mm_unpacklo_epi8(xmm2, xmm5);              \
  xmm1 = _mm_loadu_si128(&xmm0);                     \
  xmm0 = _mm_unpacklo_epi16(xmm0, xmm2);             \
  xmm1 = _mm_unpackhi_epi16(xmm1, xmm2);             \
  _mm_storeu_si128((__m128i*)dst_argb, xmm0);        \
  _mm_storeu_si128((__m128i*)(dst_argb + 16), xmm1); \
  dst_argb += 32;

// Store 8 AR30 values.
#define STOREAR30                                    \
  xmm1 = _mm_and_si128(xmm1, xmm7);                  \
  xmm2 = _mm_and_si128(xmm2, xmm7);                  \
  xmm0 = _mm_srli_epi16(xmm0, 4);                    \
  xmm3 = _mm_slli_epi16(xmm1, 6);                    \
  xmm0 = _mm_or_si128(xmm0, xmm3);                   \
  xmm1 = _mm_srli_epi16(xmm1, 10);                   \
  xmm2 = _mm_or_si128(xmm2, xmm6);                   \
  xmm2 = _mm_or_si128(xmm2, xmm1);                   \
  xmm1 = _mm_unpackhi_epi16(xmm0, xmm2);             \
  xmm0 = _mm_unpacklo_epi16(xmm0, xmm2);             \
  _mm_storeu_si128((__m128i*)dst_ar30, xmm0);        \
  _mm_storeu_si128((__m128i*)(dst_ar30 + 16), xmm1); \
  dst_ar30 += 32;

// Convert 32 ARGB pixels (128 bytes) to 32 UV444 values.
#if defined(HAS_ARGBTOYMATRIXROW_AVX2) || defined(HAS_ARGBTOUV444MATRIXROW_AVX2)
LIBYUV_TARGET_AVX2
void ARGBToUV444MatrixRow_AVX2(const uint8_t* src_argb,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width,
                               const struct ArgbConstants* c) {
  __m256i ymm_u =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m256i ymm_v =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m256i ymm5 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kAddUV));
  __m256i perm_mask = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src_argb + 64));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src_argb + 96));
    src_argb += 128;

    __m256i ymm0_u = _mm256_maddubs_epi16(ymm0, ymm_u);
    __m256i ymm1_u = _mm256_maddubs_epi16(ymm1, ymm_u);
    __m256i ymm2_u = _mm256_maddubs_epi16(ymm2, ymm_u);
    __m256i ymm3_u = _mm256_maddubs_epi16(ymm3, ymm_u);

    __m256i ymm0_v = _mm256_maddubs_epi16(ymm0, ymm_v);
    __m256i ymm1_v = _mm256_maddubs_epi16(ymm1, ymm_v);
    __m256i ymm2_v = _mm256_maddubs_epi16(ymm2, ymm_v);
    __m256i ymm3_v = _mm256_maddubs_epi16(ymm3, ymm_v);

    ymm0_u = _mm256_hadd_epi16(ymm0_u, ymm1_u);
    ymm2_u = _mm256_hadd_epi16(ymm2_u, ymm3_u);

    ymm0_v = _mm256_hadd_epi16(ymm0_v, ymm1_v);
    ymm2_v = _mm256_hadd_epi16(ymm2_v, ymm3_v);

    ymm0_u = _mm256_sub_epi16(ymm5, ymm0_u);
    ymm2_u = _mm256_sub_epi16(ymm5, ymm2_u);

    ymm0_v = _mm256_sub_epi16(ymm5, ymm0_v);
    ymm2_v = _mm256_sub_epi16(ymm5, ymm2_v);

    ymm0_u = _mm256_srli_epi16(ymm0_u, 8);
    ymm2_u = _mm256_srli_epi16(ymm2_u, 8);

    ymm0_v = _mm256_srli_epi16(ymm0_v, 8);
    ymm2_v = _mm256_srli_epi16(ymm2_v, 8);

    ymm0_u = _mm256_packus_epi16(ymm0_u, ymm2_u);
    ymm0_u = _mm256_permutevar8x32_epi32(ymm0_u, perm_mask);

    ymm0_v = _mm256_packus_epi16(ymm0_v, ymm2_v);
    ymm0_v = _mm256_permutevar8x32_epi32(ymm0_v, perm_mask);

    _mm256_storeu_si256((__m256i*)dst_u, ymm0_u);
    _mm256_storeu_si256((__m256i*)dst_v, ymm0_v);
    dst_u += 32;
    dst_v += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_ARGBTOYMATRIXROW_AVX2) ||
        // defined(HAS_ARGBTOUV444MATRIXROW_AVX2)

#if defined(HAS_ARGBTOYMATRIXROW_AVX2)
LIBYUV_TARGET_AVX2
void ARGBToYMatrixRow_AVX2(const uint8_t* src_argb,
                           uint8_t* dst_y,
                           int width,
                           const struct ArgbConstants* c) {
  __m256i ymm5 = _mm256_set1_epi8((char)0x80);
  __m256i ymm4 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m256i ymm7 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kAddY));
  __m256i ymm6 = _mm256_maddubs_epi16(ymm4, ymm5);
  ymm6 = _mm256_hadd_epi16(ymm6, ymm6);
  ymm7 = _mm256_sub_epi16(ymm7, ymm6);
  __m256i perm_mask = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src_argb + 64));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src_argb + 96));
    src_argb += 128;

    ymm0 = _mm256_sub_epi8(ymm0, ymm5);
    ymm1 = _mm256_sub_epi8(ymm1, ymm5);
    ymm2 = _mm256_sub_epi8(ymm2, ymm5);
    ymm3 = _mm256_sub_epi8(ymm3, ymm5);

    ymm0 = _mm256_maddubs_epi16(ymm4, ymm0);
    ymm1 = _mm256_maddubs_epi16(ymm4, ymm1);
    ymm2 = _mm256_maddubs_epi16(ymm4, ymm2);
    ymm3 = _mm256_maddubs_epi16(ymm4, ymm3);

    ymm0 = _mm256_hadd_epi16(ymm0, ymm1);
    ymm2 = _mm256_hadd_epi16(ymm2, ymm3);

    ymm0 = _mm256_add_epi16(ymm0, ymm7);
    ymm2 = _mm256_add_epi16(ymm2, ymm7);

    ymm0 = _mm256_srli_epi16(ymm0, 8);
    ymm2 = _mm256_srli_epi16(ymm2, 8);

    ymm0 = _mm256_packus_epi16(ymm0, ymm2);
    ymm0 = _mm256_permutevar8x32_epi32(ymm0, perm_mask);

    _mm256_storeu_si256((__m256i*)dst_y, ymm0);
    dst_y += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_ARGBTOYMATRIXROW_AVX2)

#ifdef HAS_RAWTOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void RAWToARGBRow_AVX2(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  __m256i ymm_alpha = _mm256_set1_epi32(0xff000000);
  __m128i shuf_low =
      _mm_set_epi8(-1, 9, 10, 11, -1, 6, 7, 8, -1, 3, 4, 5, -1, 0, 1, 2);
  __m128i shuf_high =
      _mm_set_epi8(-1, 13, 14, 15, -1, 10, 11, 12, -1, 7, 8, 9, -1, 4, 5, 6);
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(shuf_low);
  __m256i ymm_shuf2 = _mm256_broadcastsi128_si256(shuf_high);

  while (width > 0) {
    __m128i xmm0 = _mm_loadu_si128((const __m128i*)src_raw);
    __m256i ymm0 = _mm256_castsi128_si256(xmm0);
    ymm0 = _mm256_inserti128_si256(
        ymm0, _mm_loadu_si128((const __m128i*)(src_raw + 12)), 1);

    __m128i xmm1 = _mm_loadu_si128((const __m128i*)(src_raw + 24));
    __m256i ymm1 = _mm256_castsi128_si256(xmm1);
    ymm1 = _mm256_inserti128_si256(
        ymm1, _mm_loadu_si128((const __m128i*)(src_raw + 36)), 1);

    __m128i xmm2 = _mm_loadu_si128((const __m128i*)(src_raw + 48));
    __m256i ymm2 = _mm256_castsi128_si256(xmm2);
    ymm2 = _mm256_inserti128_si256(
        ymm2, _mm_loadu_si128((const __m128i*)(src_raw + 60)), 1);

    __m128i xmm3 = _mm_loadu_si128((const __m128i*)(src_raw + 68));
    __m256i ymm3 = _mm256_castsi128_si256(xmm3);
    ymm3 = _mm256_inserti128_si256(
        ymm3, _mm_loadu_si128((const __m128i*)(src_raw + 80)), 1);

    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf2);

    ymm0 = _mm256_or_si256(ymm0, ymm_alpha);
    ymm1 = _mm256_or_si256(ymm1, ymm_alpha);
    ymm2 = _mm256_or_si256(ymm2, ymm_alpha);
    ymm3 = _mm256_or_si256(ymm3, ymm_alpha);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);
    _mm256_storeu_si256((__m256i*)(dst_argb + 64), ymm2);
    _mm256_storeu_si256((__m256i*)(dst_argb + 96), ymm3);

    src_raw += 96;
    dst_argb += 128;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_RAWTOARGBROW_AVX2

#if defined(HAS_RAWTOARGBROW_AVX512BW) ||   \
    defined(HAS_RGB24TOARGBROW_AVX512BW) || \
    defined(HAS_RAWTORGB24ROW_AVX512BW) ||  \
    defined(HAS_RGBTOYMATRIXROW_AVX512BW)
static const uint64_t kPermqRGB24ToARGB0_AVX512BW[8] = {0, 1, 1, 2, 3, 4, 4, 5};
static const uint64_t kPermqRGB24ToARGB1_AVX512BW[8] = {6, 7,  7,  8,
                                                        9, 10, 10, 11};
static const uint64_t kPermqRGB24ToARGB2_AVX512BW[8] = {4, 5, 5, 6, 7, 8, 8, 9};
static const uint64_t kPermqRGB24ToARGB3_AVX512BW[8] = {2, 3, 3, 4, 5, 6, 6, 7};
#endif  // defined(HAS_RAWTOARGBROW_AVX512BW) ||
        // defined(HAS_RGB24TOARGBROW_AVX512BW) ||
        // defined(HAS_RAWTORGB24ROW_AVX512BW) ||
        // defined(HAS_RGBTOYMATRIXROW_AVX512BW)

#if defined(HAS_RGB24TOARGBROW_AVX512BW) || \
    defined(HAS_RGBTOYMATRIXROW_AVX512BW)
static const uint8_t kShuffleMaskRGB24ToARGB_AVX512BW[32] = {
    0u,   1u,   2u,  128u, 3u,   4u,   5u,  128u, 6u,   7u,  8u,
    128u, 9u,   10u, 11u,  128u, 4u,   5u,  6u,   128u, 7u,  8u,
    9u,   128u, 10u, 11u,  12u,  128u, 13u, 14u,  15u,  128u};
#endif  // defined(HAS_RGB24TOARGBROW_AVX512BW) ||
        // defined(HAS_RGBTOYMATRIXROW_AVX512BW)

#if defined(HAS_RAWTOARGBROW_AVX512BW)
static const uint8_t kShuffleMaskRAWToARGB_AVX512BW[32] = {
    2u,   1u,   0u,  128u, 5u,   4u,   3u,  128u, 8u,   7u,  6u,
    128u, 11u,  10u, 9u,   128u, 6u,   5u,  4u,   128u, 9u,  8u,
    7u,   128u, 12u, 11u,  10u,  128u, 15u, 14u,  13u,  128u};
#endif  // defined(HAS_RAWTOARGBROW_AVX512BW)

#if defined(HAS_RAWTORGB24ROW_SSSE3)
// Shuffle table for converting RAW to RGB24.  First 8.
static const uint8_t kShuffleMaskRAWToRGB24_0[16] = {
    2u,   1u,   0u,   5u,   4u,   3u,   8u,   7u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};

// Shuffle table for converting RAW to RGB24.  Middle 8.
static const uint8_t kShuffleMaskRAWToRGB24_1[16] = {
    2u,   7u,   6u,   5u,   10u,  9u,   8u,   13u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};

// Shuffle table for converting RAW to RGB24.  Last 8.
static const uint8_t kShuffleMaskRAWToRGB24_2[16] = {
    8u,   7u,   12u,  11u,  10u,  15u,  14u,  13u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};
#endif  // defined(HAS_RAWTORGB24ROW_SSSE3)

#if defined(HAS_RAWTORGB24ROW_AVX2) || defined(HAS_RAWTORGB24ROW_AVX512BW)
static const uint8_t kShuffleMaskRAWToRGB24_AVX2[32] = {
    2u,  1u,   0u,   5u,   4u,   3u,  8u,   7u,   6u,   11u, 10u,
    9u,  128u, 128u, 128u, 128u, 6u,  5u,   4u,   9u,   8u,  7u,
    12u, 11u,  10u,  15u,  14u,  13u, 128u, 128u, 128u, 128u};
#endif  // defined(HAS_RAWTORGB24ROW_AVX2) ||
        // defined(HAS_RAWTORGB24ROW_AVX512BW)

#if defined(HAS_RAWTORGB24ROW_AVX2)
static const uint32_t kPermd0_AVX2[8] = {0, 1, 2, 4, 5, 6, 3, 7};
static const uint32_t kPermd1_AVX2[8] = {2, 4, 5, 6, 3, 7, 0, 1};
static const uint32_t kPermd2_AVX2[8] = {5, 6, 3, 7, 0, 1, 2, 4};
#endif  // defined(HAS_RAWTORGB24ROW_AVX2)

#if defined(HAS_RAWTORGB24ROW_AVX512BW) ||  \
    defined(HAS_ARGBTORGB24ROW_AVX512BW) || \
    defined(HAS_ARGBTORAWROW_AVX512BW)
static const uint32_t kPermdRGB24_0_AVX512BW[16] = {
    0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20};
static const uint32_t kPermdRGB24_1_AVX512BW[16] = {
    5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20, 21, 22, 24, 25};
static const uint32_t kPermdRGB24_2_AVX512BW[16] = {
    10, 12, 13, 14, 16, 17, 18, 20, 21, 22, 24, 25, 26, 28, 29, 30};
#endif
#if defined(HAS_RAWTORGB24ROW_AVX512VBMI)
static const uint8_t kPermRAWToRGB24_0[64] = {
    0x02, 0x01, 0x00, 0x05, 0x04, 0x03, 0x08, 0x07, 0x06, 0x0b, 0x0a,
    0x09, 0x0e, 0x0d, 0x0c, 0x11, 0x10, 0x0f, 0x14, 0x13, 0x12, 0x17,
    0x16, 0x15, 0x1a, 0x19, 0x18, 0x1d, 0x1c, 0x1b, 0x20, 0x1f, 0x1e,
    0x23, 0x22, 0x21, 0x26, 0x25, 0x24, 0x29, 0x28, 0x27, 0x2c, 0x2b,
    0x2a, 0x2f, 0x2e, 0x2d, 0x32, 0x31, 0x30, 0x35, 0x34, 0x33, 0x38,
    0x37, 0x36, 0x3b, 0x3a, 0x39, 0x3e, 0x3d, 0x3c, 0x41};
static const uint8_t kPermRAWToRGB24_1[64] = {
    0x00, 0x7f, 0x04, 0x03, 0x02, 0x07, 0x06, 0x05, 0x0a, 0x09, 0x08,
    0x0d, 0x0c, 0x0b, 0x10, 0x0f, 0x0e, 0x13, 0x12, 0x11, 0x16, 0x15,
    0x14, 0x19, 0x18, 0x17, 0x1c, 0x1b, 0x1a, 0x1f, 0x1e, 0x1d, 0x22,
    0x21, 0x20, 0x25, 0x24, 0x23, 0x28, 0x27, 0x26, 0x2b, 0x2a, 0x29,
    0x2e, 0x2d, 0x2c, 0x31, 0x30, 0x2f, 0x34, 0x33, 0x32, 0x37, 0x36,
    0x35, 0x3a, 0x39, 0x38, 0x3d, 0x3c, 0x3b, 0x00, 0x3f};
static const uint8_t kPermRAWToRGB24_2[64] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x40, 0x3f};
static const uint8_t kPermRAWToRGB24_3[64] = {
    0x3e, 0x43, 0x42, 0x41, 0x46, 0x45, 0x44, 0x49, 0x48, 0x47, 0x4c,
    0x4b, 0x4a, 0x4f, 0x4e, 0x4d, 0x52, 0x51, 0x50, 0x55, 0x54, 0x53,
    0x58, 0x57, 0x56, 0x5b, 0x5a, 0x59, 0x5e, 0x5d, 0x5c, 0x61, 0x60,
    0x5f, 0x64, 0x63, 0x62, 0x67, 0x66, 0x65, 0x6a, 0x69, 0x68, 0x6d,
    0x6c, 0x6b, 0x70, 0x6f, 0x6e, 0x73, 0x72, 0x71, 0x76, 0x75, 0x74,
    0x79, 0x78, 0x77, 0x7c, 0x7b, 0x7a, 0x7f, 0x7e, 0x7d};
#endif  // defined(HAS_RAWTORGB24ROW_AVX512VBMI)

#if defined(HAS_RAWTOARGBROW_AVX512BW) || defined(HAS_RGB24TOARGBROW_AVX512BW)
LIBYUV_TARGET_AVX512BW
static void RGBToARGBRow_AVX512BW(const uint8_t* src_raw,
                                  uint8_t* dst_argb,
                                  const uint32_t* shuffler,
                                  int width) {
  __m512i zmm_alpha = _mm512_set1_epi32(0xff000000);
  __m512i zmm_q0 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB0_AVX512BW);
  __m512i zmm_q1 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB1_AVX512BW);
  __m512i zmm_q2 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB2_AVX512BW);
  __m512i zmm_q3 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB3_AVX512BW);
  __m512i zmm_shuf =
      _mm512_broadcast_i64x4(_mm256_loadu_si256((const __m256i*)shuffler));

  while (width >= 64) {
    __m512i zmm_raw0 = _mm512_loadu_si512((const __m512i*)src_raw);
    __m512i zmm_raw1 = _mm512_loadu_si512((const __m512i*)(src_raw + 64));
    __m512i zmm_raw2 = _mm512_loadu_si512((const __m512i*)(src_raw + 128));

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_or_si512(zmm0, zmm_alpha);
    zmm1 = _mm512_or_si512(zmm1, zmm_alpha);
    zmm2 = _mm512_or_si512(zmm2, zmm_alpha);
    zmm3 = _mm512_or_si512(zmm3, zmm_alpha);

    _mm512_storeu_si512(dst_argb, zmm0);
    _mm512_storeu_si512(dst_argb + 64, zmm1);
    _mm512_storeu_si512(dst_argb + 128, zmm2);
    _mm512_storeu_si512(dst_argb + 192, zmm3);

    src_raw += 192;
    dst_argb += 256;
    width -= 64;
  }
  if (width > 0) {
    uint64_t dst_mask = (1ULL << width) - 1ULL;
    int src_bytes = width * 3;
    __mmask64 src_mask0 =
        (src_bytes >= 64) ? ~0ULL : ((1ULL << src_bytes) - 1ULL);
    __mmask64 src_mask1 =
        (src_bytes >= 128)
            ? ~0ULL
            : ((src_bytes > 64) ? ((1ULL << (src_bytes - 64)) - 1ULL) : 0ULL);
    __mmask64 src_mask2 =
        (src_bytes > 128) ? ((1ULL << (src_bytes - 128)) - 1ULL) : 0ULL;

    __m512i zmm_raw0 = _mm512_maskz_loadu_epi8(src_mask0, src_raw);
    __m512i zmm_raw1 = _mm512_maskz_loadu_epi8(src_mask1, src_raw + 64);
    __m512i zmm_raw2 = _mm512_maskz_loadu_epi8(src_mask2, src_raw + 128);

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_or_si512(zmm0, zmm_alpha);
    zmm1 = _mm512_or_si512(zmm1, zmm_alpha);
    zmm2 = _mm512_or_si512(zmm2, zmm_alpha);
    zmm3 = _mm512_or_si512(zmm3, zmm_alpha);

    _mm512_mask_storeu_epi32(dst_argb, (__mmask16)dst_mask, zmm0);
    _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(dst_mask >> 16), zmm1);
    _mm512_mask_storeu_epi32(dst_argb + 128, (__mmask16)(dst_mask >> 32), zmm2);
    _mm512_mask_storeu_epi32(dst_argb + 192, (__mmask16)(dst_mask >> 48), zmm3);
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_RAWTOARGBROW_AVX512BW) ||
        // defined(HAS_RGB24TOARGBROW_AVX512BW)

#ifdef HAS_RAWTOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void RAWToARGBRow_AVX512BW(const uint8_t* src_raw,
                           uint8_t* dst_argb,
                           int width) {
  RGBToARGBRow_AVX512BW(src_raw, dst_argb,
                        (const uint32_t*)kShuffleMaskRAWToARGB_AVX512BW, width);
}
#endif  // HAS_RAWTOARGBROW_AVX512BW

#ifdef HAS_RGB24TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void RGB24ToARGBRow_AVX512BW(const uint8_t* src_rgb24,
                             uint8_t* dst_argb,
                             int width) {
  RGBToARGBRow_AVX512BW(src_rgb24, dst_argb,
                        (const uint32_t*)kShuffleMaskRGB24ToARGB_AVX512BW,
                        width);
}
#endif  // HAS_RGB24TOARGBROW_AVX512BW

#ifdef HAS_RAWTORGB24ROW_SSSE3
LIBYUV_TARGET_SSSE3
void RAWToRGB24Row_SSSE3(const uint8_t* src_raw,
                         uint8_t* dst_rgb24,
                         int width) {
  __m128i shuf0 = _mm_loadu_si128((const __m128i*)kShuffleMaskRAWToRGB24_0);
  __m128i shuf1 = _mm_loadu_si128((const __m128i*)kShuffleMaskRAWToRGB24_1);
  __m128i shuf2 = _mm_loadu_si128((const __m128i*)kShuffleMaskRAWToRGB24_2);

  while (width > 0) {
    __m128i raw0 = _mm_loadu_si128((const __m128i*)src_raw);
    __m128i raw1 = _mm_loadu_si128((const __m128i*)(src_raw + 4));
    __m128i raw2 = _mm_loadu_si128((const __m128i*)(src_raw + 8));

    raw0 = _mm_shuffle_epi8(raw0, shuf0);
    raw1 = _mm_shuffle_epi8(raw1, shuf1);
    raw2 = _mm_shuffle_epi8(raw2, shuf2);

    _mm_storel_epi64((__m128i*)dst_rgb24, raw0);
    _mm_storel_epi64((__m128i*)(dst_rgb24 + 8), raw1);
    _mm_storel_epi64((__m128i*)(dst_rgb24 + 16), raw2);

    src_raw += 24;
    dst_rgb24 += 24;
    width -= 8;
  }
}
#endif  // HAS_RAWTORGB24ROW_SSSE3

#ifdef HAS_RAWTORGB24ROW_AVX2
LIBYUV_TARGET_AVX2
void RAWToRGB24Row_AVX2(const uint8_t* src_raw, uint8_t* dst_rgb24, int width) {
  __m256i ymm_shuf =
      _mm256_loadu_si256((const __m256i*)kShuffleMaskRAWToRGB24_AVX2);
  __m256i ymm_p0 = _mm256_loadu_si256((const __m256i*)kPermd0_AVX2);
  __m256i ymm_p1 = _mm256_loadu_si256((const __m256i*)kPermd1_AVX2);
  __m256i ymm_p2 = _mm256_loadu_si256((const __m256i*)kPermd2_AVX2);

  while (width > 0) {
    __m256i raw0 = _mm256_loadu_si256((const __m256i*)src_raw);
    __m256i raw1 = _mm256_loadu_si256((const __m256i*)(src_raw + 32));
    __m256i raw2 = _mm256_loadu_si256((const __m256i*)(src_raw + 64));

    __m256i m01 = _mm256_permute2x128_si256(raw0, raw1, 0x21);
    __m256i b0 = _mm256_permute4x64_epi64(raw0, 0x94);
    __m256i m12 = _mm256_permute2x128_si256(raw1, raw2, 0x21);
    __m256i b3 = _mm256_permute4x64_epi64(raw2, 0xe9);
    __m256i b1 = _mm256_permute4x64_epi64(m01, 0xe9);
    __m256i b2 = _mm256_permute4x64_epi64(m12, 0x94);

    b0 = _mm256_shuffle_epi8(b0, ymm_shuf);
    b1 = _mm256_shuffle_epi8(b1, ymm_shuf);
    b2 = _mm256_shuffle_epi8(b2, ymm_shuf);
    b3 = _mm256_shuffle_epi8(b3, ymm_shuf);

    __m256i d0 = _mm256_permutevar8x32_epi32(b0, ymm_p0);
    __m256i d1 = _mm256_permutevar8x32_epi32(b1, ymm_p1);
    __m256i d2 = _mm256_permutevar8x32_epi32(b2, ymm_p2);
    __m256i d3 = _mm256_permutevar8x32_epi32(b3, ymm_p0);

    // Dst0 = blend(d0, d1)
    __m256i dst0 = _mm256_blend_epi32(d0, d1, 0xc0);
    _mm256_storeu_si256((__m256i*)dst_rgb24, dst0);

    // Dst1 = blend(d1, d2)
    __m256i dst1 = _mm256_blend_epi32(d1, d2, 0xf0);
    d3 = _mm256_permute4x64_epi64(d3, 0x90);
    _mm256_storeu_si256((__m256i*)(dst_rgb24 + 32), dst1);

    // Dst2 = blend(d2, d3)
    __m256i dst2 = _mm256_blend_epi32(d2, d3, 0xfc);
    _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64), dst2);

    src_raw += 96;
    dst_rgb24 += 96;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_RAWTORGB24ROW_AVX2

#ifdef HAS_RAWTORGB24ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void RAWToRGB24Row_AVX512BW(const uint8_t* src_raw,
                            uint8_t* dst_rgb24,
                            int width) {
  __m512i zmm_q0 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB0_AVX512BW);
  __m512i zmm_q1 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB1_AVX512BW);
  __m512i zmm_q2 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB2_AVX512BW);
  __m512i zmm_q3 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB3_AVX512BW);
  __m512i zmm_shuf = _mm512_broadcast_i64x4(
      _mm256_loadu_si256((const __m256i*)kShuffleMaskRAWToRGB24_AVX2));
  __m512i zmm_d0 = _mm512_loadu_si512((const __m512i*)kPermdRGB24_0_AVX512BW);
  __m512i zmm_d1 = _mm512_loadu_si512((const __m512i*)kPermdRGB24_1_AVX512BW);
  __m512i zmm_d2 = _mm512_loadu_si512((const __m512i*)kPermdRGB24_2_AVX512BW);

  while (width >= 64) {
    __m512i zmm_raw0 = _mm512_loadu_si512((const __m512i*)src_raw);
    __m512i zmm_raw1 = _mm512_loadu_si512((const __m512i*)(src_raw + 64));
    __m512i zmm_raw2 = _mm512_loadu_si512((const __m512i*)(src_raw + 128));

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    __m512i dst0 = _mm512_permutex2var_epi32(zmm0, zmm_d0, zmm1);
    __m512i dst1 = _mm512_permutex2var_epi32(zmm1, zmm_d1, zmm2);
    __m512i dst2 = _mm512_permutex2var_epi32(zmm2, zmm_d2, zmm3);

    _mm512_storeu_si512((__m512i*)dst_rgb24, dst0);
    _mm512_storeu_si512((__m512i*)(dst_rgb24 + 64), dst1);
    _mm512_storeu_si512((__m512i*)(dst_rgb24 + 128), dst2);

    src_raw += 192;
    dst_rgb24 += 192;
    width -= 64;
  }
  if (width > 0) {
    int nbytes = width * 3;
    __mmask64 k0 =
        (nbytes >= 64) ? ~(__mmask64)0 : (__mmask64)((1ULL << nbytes) - 1ULL);
    __mmask64 k1 = (nbytes <= 64) ? 0
                   : (nbytes >= 128)
                       ? ~(__mmask64)0
                       : (__mmask64)((1ULL << (nbytes - 64)) - 1ULL);
    __mmask64 k2 =
        (nbytes <= 128) ? 0 : (__mmask64)((1ULL << (nbytes - 128)) - 1ULL);
    __m512i zmm_raw0 = _mm512_maskz_loadu_epi8(k0, src_raw);
    __m512i zmm_raw1 =
        k1 ? _mm512_maskz_loadu_epi8(k1, src_raw + 64) : _mm512_setzero_si512();
    __m512i zmm_raw2 = k2 ? _mm512_maskz_loadu_epi8(k2, src_raw + 128)
                          : _mm512_setzero_si512();

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    __m512i dst0 = _mm512_permutex2var_epi32(zmm0, zmm_d0, zmm1);
    __m512i dst1 = _mm512_permutex2var_epi32(zmm1, zmm_d1, zmm2);
    __m512i dst2 = _mm512_permutex2var_epi32(zmm2, zmm_d2, zmm3);

    _mm512_mask_storeu_epi8(dst_rgb24, k0, dst0);
    if (k1) {
      _mm512_mask_storeu_epi8(dst_rgb24 + 64, k1, dst1);
    }
    if (k2) {
      _mm512_mask_storeu_epi8(dst_rgb24 + 128, k2, dst2);
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_RAWTORGB24ROW_AVX512BW

#ifdef HAS_RAWTORGB24ROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void RAWToRGB24Row_AVX512VBMI(const uint8_t* src_raw,
                              uint8_t* dst_rgb24,
                              int width) {
  __m512i zmm_p0 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGB24_0);
  __m512i zmm_p1 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGB24_1);
  __m512i zmm_p2 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGB24_2);
  __m512i zmm_p3 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGB24_3);

  while (width >= 64) {
    __m512i zmm_raw0 = _mm512_loadu_si512((const __m512i*)src_raw);
    __m512i zmm_raw1 = _mm512_loadu_si512((const __m512i*)(src_raw + 64));
    __m512i zmm_raw2 = _mm512_loadu_si512((const __m512i*)(src_raw + 128));

    __m512i tmp1 = _mm512_permutex2var_epi8(zmm_raw1, zmm_p1, zmm_raw0);
    __m512i dst0 = _mm512_permutex2var_epi8(zmm_raw0, zmm_p0, zmm_raw1);
    __m512i dst2 = _mm512_permutex2var_epi8(zmm_raw1, zmm_p3, zmm_raw2);
    __m512i dst1 = _mm512_permutex2var_epi8(tmp1, zmm_p2, zmm_raw2);

    _mm512_storeu_si512((__m512i*)dst_rgb24, dst0);
    _mm512_storeu_si512((__m512i*)(dst_rgb24 + 64), dst1);
    _mm512_storeu_si512((__m512i*)(dst_rgb24 + 128), dst2);

    src_raw += 192;
    dst_rgb24 += 192;
    width -= 64;
  }
  if (width > 0) {
    int nbytes = width * 3;
    __mmask64 k0 =
        (nbytes >= 64) ? ~(__mmask64)0 : (__mmask64)((1ULL << nbytes) - 1ULL);
    __mmask64 k1 = (nbytes <= 64) ? 0
                   : (nbytes >= 128)
                       ? ~(__mmask64)0
                       : (__mmask64)((1ULL << (nbytes - 64)) - 1ULL);
    __mmask64 k2 =
        (nbytes <= 128) ? 0 : (__mmask64)((1ULL << (nbytes - 128)) - 1ULL);
    __m512i zmm_raw0 = _mm512_maskz_loadu_epi8(k0, src_raw);
    __m512i zmm_raw1 =
        k1 ? _mm512_maskz_loadu_epi8(k1, src_raw + 64) : _mm512_setzero_si512();
    __m512i zmm_raw2 = k2 ? _mm512_maskz_loadu_epi8(k2, src_raw + 128)
                          : _mm512_setzero_si512();

    __m512i tmp1 = _mm512_permutex2var_epi8(zmm_raw1, zmm_p1, zmm_raw0);
    __m512i dst0 = _mm512_permutex2var_epi8(zmm_raw0, zmm_p0, zmm_raw1);
    __m512i dst2 = _mm512_permutex2var_epi8(zmm_raw1, zmm_p3, zmm_raw2);
    __m512i dst1 = _mm512_permutex2var_epi8(tmp1, zmm_p2, zmm_raw2);

    _mm512_mask_storeu_epi8(dst_rgb24, k0, dst0);
    if (k1) {
      _mm512_mask_storeu_epi8(dst_rgb24 + 64, k1, dst1);
    }
    if (k2) {
      _mm512_mask_storeu_epi8(dst_rgb24 + 128, k2, dst2);
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_RAWTORGB24ROW_AVX512VBMI

#if defined(HAS_RGBTOYMATRIXROW_AVX512BW) ||        \
    defined(HAS_ARGBTOYMATRIXROW_AVX512BW) ||       \
    defined(HAS_ARGBTOUV444MATRIXROW_AVX512BW) ||   \
    defined(HAS_ARGBTOUVMATRIXROW_AVX512BW) ||      \
    defined(HAS_ARGBTOYMATRIXROW_AVX512VNNI) ||     \
    defined(HAS_ARGBTOUV444MATRIXROW_AVX512VNNI) || \
    defined(HAS_ARGBTOUVMATRIXROW_AVX512VNNI)
static const uint32_t kPermdARGBToY_AVX512BW[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                                    2, 6, 10, 14, 3, 7, 11, 15};
#endif

#ifdef HAS_RGBTOYMATRIXROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void RGBToYMatrixRow_AVX512BW(const uint8_t* src_rgb,
                              uint8_t* dst_y,
                              int width,
                              const struct ArgbConstants* c) {
  __m512i zmm_q0 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB0_AVX512BW);
  __m512i zmm_q1 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB1_AVX512BW);
  __m512i zmm_q2 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB2_AVX512BW);
  __m512i zmm_q3 =
      _mm512_loadu_si512((const __m512i*)kPermqRGB24ToARGB3_AVX512BW);
  __m512i zmm_shuf = _mm512_broadcast_i64x4(
      _mm256_loadu_si256((const __m256i*)kShuffleMaskRGB24ToARGB_AVX512BW));
  __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);

  __m512i zmm_80 = _mm512_set1_epi8((char)0x80);
  __m512i zmm_one = _mm512_set1_epi16(1);
  __m512i zmm_coeff =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m512i zmm_add =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddY));

  // Compute bias adjusted for 0x80 subtraction:
  // bias = kAddY - pmaddwd(pmaddubsw(kRGBToY, 0x80), 1)
  __m512i zmm_bias = _mm512_maddubs_epi16(zmm_coeff, zmm_80);
  zmm_bias = _mm512_madd_epi16(zmm_bias, zmm_one);
  zmm_bias = _mm512_packs_epi32(zmm_bias, zmm_bias);
  zmm_add = _mm512_sub_epi16(zmm_add, zmm_bias);

  while (width >= 64) {
    __m512i zmm_raw0 = _mm512_loadu_si512((const __m512i*)src_rgb);
    __m512i zmm_raw1 = _mm512_loadu_si512((const __m512i*)(src_rgb + 64));
    __m512i zmm_raw2 = _mm512_loadu_si512((const __m512i*)(src_rgb + 128));

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_xor_si512(zmm0, zmm_80);
    zmm1 = _mm512_xor_si512(zmm1, zmm_80);
    zmm2 = _mm512_xor_si512(zmm2, zmm_80);
    zmm3 = _mm512_xor_si512(zmm3, zmm_80);

    zmm0 = _mm512_maddubs_epi16(zmm_coeff, zmm0);
    zmm1 = _mm512_maddubs_epi16(zmm_coeff, zmm1);
    zmm2 = _mm512_maddubs_epi16(zmm_coeff, zmm2);
    zmm3 = _mm512_maddubs_epi16(zmm_coeff, zmm3);

    zmm0 = _mm512_madd_epi16(zmm0, zmm_one);
    zmm1 = _mm512_madd_epi16(zmm1, zmm_one);
    zmm0 = _mm512_packs_epi32(zmm0, zmm1);

    zmm2 = _mm512_madd_epi16(zmm2, zmm_one);
    zmm3 = _mm512_madd_epi16(zmm3, zmm_one);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);

    zmm0 = _mm512_add_epi16(zmm0, zmm_add);
    zmm2 = _mm512_add_epi16(zmm2, zmm_add);

    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm2 = _mm512_srli_epi16(zmm2, 8);

    zmm0 = _mm512_packus_epi16(zmm0, zmm2);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);

    _mm512_storeu_si512((__m512i*)dst_y, zmm0);

    src_rgb += 192;
    dst_y += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 ky = (__mmask64)((1ULL << width) - 1ULL);
    int nbytes = width * 3;
    __mmask64 k0 =
        (nbytes >= 64) ? ~(__mmask64)0 : (__mmask64)((1ULL << nbytes) - 1ULL);
    __mmask64 k1 = (nbytes <= 64) ? 0
                   : (nbytes >= 128)
                       ? ~(__mmask64)0
                       : (__mmask64)((1ULL << (nbytes - 64)) - 1ULL);
    __mmask64 k2 =
        (nbytes <= 128) ? 0 : (__mmask64)((1ULL << (nbytes - 128)) - 1ULL);
    __m512i zmm_raw0 = _mm512_maskz_loadu_epi8(k0, src_rgb);
    __m512i zmm_raw1 =
        k1 ? _mm512_maskz_loadu_epi8(k1, src_rgb + 64) : _mm512_setzero_si512();
    __m512i zmm_raw2 = k2 ? _mm512_maskz_loadu_epi8(k2, src_rgb + 128)
                          : _mm512_setzero_si512();

    __m512i zmm0 = PermutexvarEpi64(zmm_q0, zmm_raw0);
    __m512i zmm1 = _mm512_permutex2var_epi64(zmm_raw0, zmm_q1, zmm_raw1);
    __m512i zmm2 = _mm512_permutex2var_epi64(zmm_raw1, zmm_q2, zmm_raw2);
    __m512i zmm3 = PermutexvarEpi64(zmm_q3, zmm_raw2);

    zmm0 = _mm512_xor_si512(_mm512_shuffle_epi8(zmm0, zmm_shuf), zmm_80);
    zmm1 = _mm512_xor_si512(_mm512_shuffle_epi8(zmm1, zmm_shuf), zmm_80);
    zmm2 = _mm512_xor_si512(_mm512_shuffle_epi8(zmm2, zmm_shuf), zmm_80);
    zmm3 = _mm512_xor_si512(_mm512_shuffle_epi8(zmm3, zmm_shuf), zmm_80);

    zmm0 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm0), zmm_one);
    zmm1 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm1), zmm_one);
    zmm0 = _mm512_packs_epi32(zmm0, zmm1);

    zmm2 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm2), zmm_one);
    zmm3 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm3), zmm_one);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);

    zmm0 = _mm512_srli_epi16(_mm512_add_epi16(zmm0, zmm_add), 8);
    zmm2 = _mm512_srli_epi16(_mm512_add_epi16(zmm2, zmm_add), 8);

    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_mask_storeu_epi8(dst_y, ky, zmm0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_RGBTOYMATRIXROW_AVX512BW

#ifdef HAS_ARGBTOYMATRIXROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBToYMatrixRow_AVX512VNNI(const uint8_t* src_argb,
                                 uint8_t* dst_y,
                                 int width,
                                 const struct ArgbConstants* c) {
  __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);
  __m512i zmm_80 = _mm512_set1_epi8((char)0x80);
  __m512i zmm_coeff =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m512i zmm_add =
      _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)c->kAddY));

  __m512i zmm_bias =
      _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_coeff, zmm_80);
  zmm_add = _mm512_sub_epi32(zmm_add, zmm_bias);

  while (width >= 64) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm2 = _mm512_loadu_si512((const __m512i*)(src_argb + 128));
    __m512i zmm3 = _mm512_loadu_si512((const __m512i*)(src_argb + 192));

    zmm0 = _mm512_xor_si512(zmm0, zmm_80);
    zmm1 = _mm512_xor_si512(zmm1, zmm_80);
    zmm2 = _mm512_xor_si512(zmm2, zmm_80);
    zmm3 = _mm512_xor_si512(zmm3, zmm_80);

    zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm0);
    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm1);
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm2);
    zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm3);

    zmm0 = _mm512_packus_epi32(zmm0, zmm1);
    zmm2 = _mm512_packus_epi32(zmm2, zmm3);

    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm2 = _mm512_srli_epi16(zmm2, 8);

    zmm0 = _mm512_packus_epi16(zmm0, zmm2);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);

    _mm512_storeu_si512((__m512i*)dst_y, zmm0);

    src_argb += 256;
    dst_y += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i zmm0 =
        _mm512_xor_si512(_mm512_maskz_loadu_epi32(k0, src_argb), zmm_80);
    __m512i zmm1 =
        _mm512_xor_si512(k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm2 =
        _mm512_xor_si512(k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm3 =
        _mm512_xor_si512(k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                            : _mm512_setzero_si512(),
                         zmm_80);

    zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm0);
    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm1);
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm2);
    zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm3);

    zmm0 = _mm512_srli_epi16(_mm512_packus_epi32(zmm0, zmm1), 8);
    zmm2 = _mm512_srli_epi16(_mm512_packus_epi32(zmm2, zmm3), 8);

    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_mask_storeu_epi8(dst_y, (__mmask64)mask, zmm0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOYMATRIXROW_AVX512VNNI

#if defined(HAS_ARGBTOYMATRIXROW_AVX512VNNIVBMI) ||     \
    defined(HAS_ARGBTOUV444MATRIXROW_AVX512VNNIVBMI) || \
    defined(HAS_ARGBTOUVMATRIXROW_AVX512VNNIVBMI)
// AVX512VNNIVBMI kernels use both AVX512VNNI (vpdpbusd) and AVX512VBMI
// (vpermt2b) and require both CPU features.  There are no AVX512VBMI
// versions without AVX512VNNI.

// vpermt2b indices selecting byte 1 of each int32 from 2 vectors.  The 32
// byte pattern is broadcast so each 256 bit half holds the same 32 results.
// Clang constant folds the indices and lowers _mm512_permutex2var_epi8 to a
// longer vpshufb + vpor + vpermd sequence, so hide the indices from it.
#if defined(__clang__)
#define LIBYUV_HIDE_CONSTANT(v) __asm__("" : "+v"(v))
#else
#define LIBYUV_HIDE_CONSTANT(v)
#endif
static const uint8_t kPermbDWordByte1_AVX512VNNIVBMI[32] = {
    1,  5,  9,  13, 17, 21, 25, 29, 33, 37,  41,  45,  49,  53,  57,  61,
    65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125};
#endif

#ifdef HAS_ARGBTOYMATRIXROW_AVX512VNNIVBMI
// Requires AVX512VNNI and AVX512VBMI.
// The 32 bit sums are kAddY + dot(kRGBToY, argb) which is always in
// [0, 65535], so Y is byte 1 of each int32 and no saturation is needed.
LIBYUV_TARGET_AVX512VNNIVBMI
void ARGBToYMatrixRow_AVX512VNNIVBMI(const uint8_t* src_argb,
                                     uint8_t* dst_y,
                                     int width,
                                     const struct ArgbConstants* c) {
  __m512i zmm_perm = _mm512_broadcast_i64x4(
      _mm256_loadu_si256((const __m256i*)kPermbDWordByte1_AVX512VNNIVBMI));
  LIBYUV_HIDE_CONSTANT(zmm_perm);
  __m512i zmm_80 = _mm512_set1_epi8((char)0x80);
  __m512i zmm_coeff =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m512i zmm_add =
      _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)c->kAddY));

  __m512i zmm_bias =
      _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_coeff, zmm_80);
  zmm_add = _mm512_sub_epi32(zmm_add, zmm_bias);

  while (width >= 64) {
    __m512i zmm0 =
        _mm512_xor_si512(_mm512_loadu_si512((const __m512i*)src_argb), zmm_80);
    __m512i zmm1 = _mm512_xor_si512(
        _mm512_loadu_si512((const __m512i*)(src_argb + 64)), zmm_80);
    __m512i zmm2 = _mm512_xor_si512(
        _mm512_loadu_si512((const __m512i*)(src_argb + 128)), zmm_80);
    __m512i zmm3 = _mm512_xor_si512(
        _mm512_loadu_si512((const __m512i*)(src_argb + 192)), zmm_80);

    zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm0);
    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm1);
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm2);
    zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm3);

    zmm0 = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);  // Y 0..31
    zmm2 = _mm512_permutex2var_epi8(zmm2, zmm_perm, zmm3);  // Y 32..63

    _mm256_storeu_si256((__m256i*)dst_y, _mm512_castsi512_si256(zmm0));
    _mm256_storeu_si256((__m256i*)(dst_y + 32), _mm512_castsi512_si256(zmm2));

    src_argb += 256;
    dst_y += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i zmm0 =
        _mm512_xor_si512(_mm512_maskz_loadu_epi32(k0, src_argb), zmm_80);
    __m512i zmm1 =
        _mm512_xor_si512(k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm2 =
        _mm512_xor_si512(k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm3 =
        _mm512_xor_si512(k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                            : _mm512_setzero_si512(),
                         zmm_80);

    zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm0);
    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm1);
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm2);
    zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_coeff, zmm3);

    zmm0 = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);
    zmm2 = _mm512_permutex2var_epi8(zmm2, zmm_perm, zmm3);

    _mm256_mask_storeu_epi8(dst_y, (__mmask32)mask,
                            _mm512_castsi512_si256(zmm0));
    if (mask >> 32) {
      _mm256_mask_storeu_epi8(dst_y + 32, (__mmask32)(mask >> 32),
                              _mm512_castsi512_si256(zmm2));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOYMATRIXROW_AVX512VNNIVBMI

#ifdef HAS_ARGBTOUV444MATRIXROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBToUV444MatrixRow_AVX512VNNI(const uint8_t* src_argb,
                                     uint8_t* dst_u,
                                     uint8_t* dst_v,
                                     int width,
                                     const struct ArgbConstants* c) {
  __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m512i zmm_8000 =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddUV));
  __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);

  while (width >= 64) {
    __m512i zmm_s0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm_s1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm_s2 = _mm512_loadu_si512((const __m512i*)(src_argb + 128));
    __m512i zmm_s3 = _mm512_loadu_si512((const __m512i*)(src_argb + 192));

    __m512i zmm0 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s0, zmm_u);
    __m512i zmm1 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s1, zmm_u);
    __m512i zmm2 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s2, zmm_u);
    __m512i zmm3 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s3, zmm_u);

    zmm0 = _mm512_packs_epi32(zmm0, zmm1);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);
    zmm0 = _mm512_sub_epi16(zmm_8000, zmm0);
    zmm2 = _mm512_sub_epi16(zmm_8000, zmm2);
    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm2 = _mm512_srli_epi16(zmm2, 8);
    zmm0 = _mm512_packus_epi16(zmm0, zmm2);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);
    _mm512_storeu_si512((__m512i*)dst_u, zmm0);

    zmm0 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s0, zmm_v);
    zmm1 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s1, zmm_v);
    zmm2 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s2, zmm_v);
    zmm3 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s3, zmm_v);

    zmm0 = _mm512_packs_epi32(zmm0, zmm1);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);
    zmm0 = _mm512_sub_epi16(zmm_8000, zmm0);
    zmm2 = _mm512_sub_epi16(zmm_8000, zmm2);
    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm2 = _mm512_srli_epi16(zmm2, 8);
    zmm0 = _mm512_packus_epi16(zmm0, zmm2);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);
    _mm512_storeu_si512((__m512i*)dst_v, zmm0);

    src_argb += 256;
    dst_u += 64;
    dst_v += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i zmm_s0 = _mm512_maskz_loadu_epi32(k0, src_argb);
    __m512i zmm_s1 = k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                        : _mm512_setzero_si512();
    __m512i zmm_s2 = k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                        : _mm512_setzero_si512();
    __m512i zmm_s3 = k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                        : _mm512_setzero_si512();

    __m512i zmm0 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s0, zmm_u);
    __m512i zmm1 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s1, zmm_u);
    __m512i zmm2 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s2, zmm_u);
    __m512i zmm3 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s3, zmm_u);

    zmm0 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(zmm0, zmm1)), 8);
    zmm2 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(zmm2, zmm3)), 8);
    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_mask_storeu_epi8(dst_u, (__mmask64)mask, zmm0);

    zmm0 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s0, zmm_v);
    zmm1 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s1, zmm_v);
    zmm2 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s2, zmm_v);
    zmm3 = _mm512_dpbusd_epi32(_mm512_setzero_si512(), zmm_s3, zmm_v);

    zmm0 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(zmm0, zmm1)), 8);
    zmm2 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(zmm2, zmm3)), 8);
    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_mask_storeu_epi8(dst_v, (__mmask64)mask, zmm0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUV444MATRIXROW_AVX512VNNI

#ifdef HAS_ARGBTOUV444MATRIXROW_AVX512VNNIVBMI
// Requires AVX512VNNI and AVX512VBMI.
// U = (kAddUV - dot(kRGBToU, argb)) >> 8.  The accumulators start at
// ~kAddUV so they hold ~(kAddUV - dot) and U is the inverse of byte 1 of each
// int32.
LIBYUV_TARGET_AVX512VNNIVBMI
void ARGBToUV444MatrixRow_AVX512VNNIVBMI(const uint8_t* src_argb,
                                         uint8_t* dst_u,
                                         uint8_t* dst_v,
                                         int width,
                                         const struct ArgbConstants* c) {
  __m512i zmm_perm = _mm512_broadcast_i64x4(
      _mm256_loadu_si256((const __m256i*)kPermbDWordByte1_AVX512VNNIVBMI));
  LIBYUV_HIDE_CONSTANT(zmm_perm);
  __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  const __m512i zmm_neg1 = _mm512_set1_epi32(-1);
  __m512i zmm_add = _mm512_xor_si512(
      _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)c->kAddUV)),
      zmm_neg1);  // ~kAddUV

  while (width >= 64) {
    __m512i zmm_s0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm_s1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm_s2 = _mm512_loadu_si512((const __m512i*)(src_argb + 128));
    __m512i zmm_s3 = _mm512_loadu_si512((const __m512i*)(src_argb + 192));

    __m512i zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_s0, zmm_u);
    __m512i zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_s1, zmm_u);
    __m512i zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_s2, zmm_u);
    __m512i zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_s3, zmm_u);
    __m512i zmm4 = _mm512_dpbusd_epi32(zmm_add, zmm_s0, zmm_v);
    __m512i zmm5 = _mm512_dpbusd_epi32(zmm_add, zmm_s1, zmm_v);
    __m512i zmm6 = _mm512_dpbusd_epi32(zmm_add, zmm_s2, zmm_v);
    __m512i zmm7 = _mm512_dpbusd_epi32(zmm_add, zmm_s3, zmm_v);

    zmm0 = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);  // ~U 0..31
    zmm2 = _mm512_permutex2var_epi8(zmm2, zmm_perm, zmm3);  // ~U 32..63
    zmm4 = _mm512_permutex2var_epi8(zmm4, zmm_perm, zmm5);  // ~V 0..31
    zmm6 = _mm512_permutex2var_epi8(zmm6, zmm_perm, zmm7);  // ~V 32..63
    // Avoid _mm512_ternarylogic_epi32 (unmodeled by LLVM MSan; see GEMINI.md).
    zmm0 = _mm512_xor_si512(_mm512_shuffle_i64x2(zmm0, zmm2, 0x44),
                            zmm_neg1);  // U
    zmm4 = _mm512_xor_si512(_mm512_shuffle_i64x2(zmm4, zmm6, 0x44),
                            zmm_neg1);  // V

    _mm512_storeu_si512((__m512i*)dst_u, zmm0);
    _mm512_storeu_si512((__m512i*)dst_v, zmm4);

    src_argb += 256;
    dst_u += 64;
    dst_v += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i zmm_s0 = _mm512_maskz_loadu_epi32(k0, src_argb);
    __m512i zmm_s1 = k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                        : _mm512_setzero_si512();
    __m512i zmm_s2 = k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                        : _mm512_setzero_si512();
    __m512i zmm_s3 = k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                        : _mm512_setzero_si512();

    __m512i zmm0 = _mm512_dpbusd_epi32(zmm_add, zmm_s0, zmm_u);
    __m512i zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm_s1, zmm_u);
    __m512i zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm_s2, zmm_u);
    __m512i zmm3 = _mm512_dpbusd_epi32(zmm_add, zmm_s3, zmm_u);
    __m512i zmm4 = _mm512_dpbusd_epi32(zmm_add, zmm_s0, zmm_v);
    __m512i zmm5 = _mm512_dpbusd_epi32(zmm_add, zmm_s1, zmm_v);
    __m512i zmm6 = _mm512_dpbusd_epi32(zmm_add, zmm_s2, zmm_v);
    __m512i zmm7 = _mm512_dpbusd_epi32(zmm_add, zmm_s3, zmm_v);

    zmm0 = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);
    zmm2 = _mm512_permutex2var_epi8(zmm2, zmm_perm, zmm3);
    zmm4 = _mm512_permutex2var_epi8(zmm4, zmm_perm, zmm5);
    zmm6 = _mm512_permutex2var_epi8(zmm6, zmm_perm, zmm7);
    zmm0 = _mm512_xor_si512(
        _mm512_inserti64x4(zmm0, _mm512_castsi512_si256(zmm2), 1), zmm_neg1);
    zmm4 = _mm512_xor_si512(
        _mm512_inserti64x4(zmm4, _mm512_castsi512_si256(zmm6), 1), zmm_neg1);

    _mm512_mask_storeu_epi8(dst_u, (__mmask64)mask, zmm0);
    _mm512_mask_storeu_epi8(dst_v, (__mmask64)mask, zmm4);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUV444MATRIXROW_AVX512VNNIVBMI

#ifdef HAS_ARGBTOUVMATRIXROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBToUVMatrixRow_AVX512VNNI(const uint8_t* src_argb,
                                  int src_stride_argb,
                                  uint8_t* dst_u,
                                  uint8_t* dst_v,
                                  int width,
                                  const struct ArgbConstants* c) {
  static const uint32_t kPermdARGBToUV_AVX512BW[16] = {
      0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15};
  static const uint8_t kShuffleAARRGGBB_AVX512[16] = {
      0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15};
  __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m512i zmm_0101 = _mm512_set1_epi16(0x0101);
  __m512i zmm_8000 =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddUV));
  __m512i zmm_zero = _mm512_setzero_si512();
  __m512i zmm_shuf = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleAARRGGBB_AVX512));
  __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);
  __m512i zmm_perm_uv =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToUV_AVX512BW);

  while (width >= 32) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm2 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb));
    __m512i zmm3 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb + 64));

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_srli_epi16(zmm0, 1);
    zmm1 = _mm512_srli_epi16(zmm1, 1);
    zmm0 = _mm512_avg_epu16(zmm0, zmm_zero);
    zmm1 = _mm512_avg_epu16(zmm1, zmm_zero);

    zmm0 = _mm512_packus_epi16(zmm0, zmm1);
    zmm0 = PermutexvarEpi32(zmm_perm_uv, zmm0);

    zmm1 = _mm512_dpbusd_epi32(zmm_zero, zmm0, zmm_u);
    zmm2 = _mm512_dpbusd_epi32(zmm_zero, zmm0, zmm_v);
    zmm0 = _mm512_packs_epi32(zmm1, zmm2);
    zmm0 = _mm512_sub_epi16(zmm_8000, zmm0);
    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);

    _mm_storeu_si128((__m128i*)dst_u, _mm512_castsi512_si128(zmm0));
    _mm_storeu_si128((__m128i*)dst_v, _mm512_extracti32x4_epi32(zmm0, 1));

    src_argb += 128;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)mask;
    __mmask16 k2 = (__mmask16)(mask >> 16);
    __mmask16 k3 = ~k1;
    __mmask16 k4 = ~k2;
    __m512i zmm0 = _mm512_maskz_loadu_epi32(k1, src_argb);
    __m512i zmm1 = _mm512_maskz_loadu_epi32(k2, src_argb + 64);
    __m512i zmm2 = _mm512_maskz_loadu_epi32(k1, src_argb + src_stride_argb);
    __m512i zmm3 =
        _mm512_maskz_loadu_epi32(k2, src_argb + src_stride_argb + 64);

    zmm0 = _mm512_mask_shuffle_epi32(zmm0, k3, zmm0, (_MM_PERM_ENUM)0xa0);
    zmm1 = _mm512_mask_shuffle_epi32(zmm1, k4, zmm1, (_MM_PERM_ENUM)0xa0);
    zmm2 = _mm512_mask_shuffle_epi32(zmm2, k3, zmm2, (_MM_PERM_ENUM)0xa0);
    zmm3 = _mm512_mask_shuffle_epi32(zmm3, k4, zmm3, (_MM_PERM_ENUM)0xa0);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_srli_epi16(zmm0, 1);
    zmm1 = _mm512_srli_epi16(zmm1, 1);
    zmm0 = _mm512_avg_epu16(zmm0, zmm_zero);
    zmm1 = _mm512_avg_epu16(zmm1, zmm_zero);

    zmm0 = _mm512_packus_epi16(zmm0, zmm1);
    zmm0 = PermutexvarEpi32(zmm_perm_uv, zmm0);

    zmm1 = _mm512_dpbusd_epi32(zmm_zero, zmm0, zmm_u);
    zmm2 = _mm512_dpbusd_epi32(zmm_zero, zmm0, zmm_v);
    zmm0 = _mm512_packs_epi32(zmm1, zmm2);
    zmm0 = _mm512_sub_epi16(zmm_8000, zmm0);
    zmm0 = _mm512_srli_epi16(zmm0, 8);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm0 = PermutexvarEpi32(zmm_perm_y, zmm0);

    int halfwidth = (width + 1) >> 1;
    __mmask16 k_out = (__mmask16)((1u << halfwidth) - 1u);
    _mm_mask_storeu_epi8(dst_u, k_out, _mm512_castsi512_si128(zmm0));
    _mm_mask_storeu_epi8(dst_v, k_out, _mm512_extracti32x4_epi32(zmm0, 1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX512VNNI

#ifdef HAS_ARGBTOUVMATRIXROW_AVX512VNNIVBMI
// Requires AVX512VNNI and AVX512VBMI.
LIBYUV_TARGET_AVX512VNNIVBMI
void ARGBToUVMatrixRow_AVX512VNNIVBMI(const uint8_t* src_argb,
                                      int src_stride_argb,
                                      uint8_t* dst_u,
                                      uint8_t* dst_v,
                                      int width,
                                      const struct ArgbConstants* c) {
  static const uint32_t kPermdARGBToUV_AVX512BW[16] = {
      0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15};
  static const uint8_t kShuffleAARRGGBB_AVX512[16] = {
      0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15};
  __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m512i zmm_0101 = _mm512_set1_epi16(0x0101);
  __m512i zmm_add = _mm512_xor_si512(
      _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)c->kAddUV)),
      _mm512_set1_epi32(-1));  // ~kAddUV
  __m512i zmm_zero = _mm512_setzero_si512();
  __m512i zmm_shuf = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleAARRGGBB_AVX512));
  __m512i zmm_perm = _mm512_broadcast_i64x4(
      _mm256_loadu_si256((const __m256i*)kPermbDWordByte1_AVX512VNNIVBMI));
  LIBYUV_HIDE_CONSTANT(zmm_perm);
  __m512i zmm_perm_uv =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToUV_AVX512BW);

  while (width >= 32) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm2 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb));
    __m512i zmm3 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb + 64));

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_srli_epi16(zmm0, 1);
    zmm1 = _mm512_srli_epi16(zmm1, 1);
    zmm0 = _mm512_avg_epu16(zmm0, zmm_zero);
    zmm1 = _mm512_avg_epu16(zmm1, zmm_zero);

    zmm0 = _mm512_packus_epi16(zmm0, zmm1);
    zmm0 = PermutexvarEpi32(zmm_perm_uv, zmm0);

    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm0, zmm_u);  // ~U
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm0, zmm_v);  // ~V
    zmm0 = _mm512_permutex2var_epi8(zmm1, zmm_perm, zmm2);
    __m256i ymm_uv =
        _mm256_xor_si256(_mm512_castsi512_si256(zmm0), _mm256_set1_epi32(-1));

    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(ymm_uv));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_extracti128_si256(ymm_uv, 1));

    src_argb += 128;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)mask;
    __mmask16 k2 = (__mmask16)(mask >> 16);
    __mmask16 k3 = ~k1;
    __mmask16 k4 = ~k2;
    __m512i zmm0 = _mm512_maskz_loadu_epi32(k1, src_argb);
    __m512i zmm1 = _mm512_maskz_loadu_epi32(k2, src_argb + 64);
    __m512i zmm2 = _mm512_maskz_loadu_epi32(k1, src_argb + src_stride_argb);
    __m512i zmm3 =
        _mm512_maskz_loadu_epi32(k2, src_argb + src_stride_argb + 64);

    zmm0 = _mm512_mask_shuffle_epi32(zmm0, k3, zmm0, (_MM_PERM_ENUM)0xa0);
    zmm1 = _mm512_mask_shuffle_epi32(zmm1, k4, zmm1, (_MM_PERM_ENUM)0xa0);
    zmm2 = _mm512_mask_shuffle_epi32(zmm2, k3, zmm2, (_MM_PERM_ENUM)0xa0);
    zmm3 = _mm512_mask_shuffle_epi32(zmm3, k4, zmm3, (_MM_PERM_ENUM)0xa0);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_srli_epi16(zmm0, 1);
    zmm1 = _mm512_srli_epi16(zmm1, 1);
    zmm0 = _mm512_avg_epu16(zmm0, zmm_zero);
    zmm1 = _mm512_avg_epu16(zmm1, zmm_zero);

    zmm0 = _mm512_packus_epi16(zmm0, zmm1);
    zmm0 = PermutexvarEpi32(zmm_perm_uv, zmm0);

    zmm1 = _mm512_dpbusd_epi32(zmm_add, zmm0, zmm_u);  // ~U
    zmm2 = _mm512_dpbusd_epi32(zmm_add, zmm0, zmm_v);  // ~V
    zmm0 = _mm512_permutex2var_epi8(zmm1, zmm_perm, zmm2);
    __m256i ymm_uv =
        _mm256_xor_si256(_mm512_castsi512_si256(zmm0), _mm256_set1_epi32(-1));

    int halfwidth = (width + 1) >> 1;
    __mmask16 k_out = (__mmask16)((1u << halfwidth) - 1u);
    _mm_mask_storeu_epi8(dst_u, k_out, _mm256_castsi256_si128(ymm_uv));
    _mm_mask_storeu_epi8(dst_v, k_out, _mm256_extracti128_si256(ymm_uv, 1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX512VNNIVBMI

#ifdef HAS_ARGBTOUVMATRIXROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToUVMatrixRow_AVX2(const uint8_t* src_argb,
                            int src_stride_argb,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width,
                            const struct ArgbConstants* c) {
  __m256i ymm_u =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m256i ymm_v =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m256i ymm_0101 = _mm256_set1_epi16(0x0101);
  __m256i ymm_shuf =
      _mm256_setr_epi8(0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15, 0,
                       4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15);
  __m256i ymm_8000 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kAddUV));
  __m256i ymm_zero = _mm256_setzero_si256();

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 =
        _mm256_loadu_si256((const __m256i*)(src_argb + src_stride_argb));
    __m256i ymm3 =
        _mm256_loadu_si256((const __m256i*)(src_argb + src_stride_argb + 32));

    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf);

    ymm0 = _mm256_maddubs_epi16(ymm0, ymm_0101);
    ymm1 = _mm256_maddubs_epi16(ymm1, ymm_0101);
    ymm2 = _mm256_maddubs_epi16(ymm2, ymm_0101);
    ymm3 = _mm256_maddubs_epi16(ymm3, ymm_0101);

    ymm0 = _mm256_add_epi16(ymm0, ymm2);
    ymm1 = _mm256_add_epi16(ymm1, ymm3);

    ymm0 = _mm256_srli_epi16(ymm0, 1);
    ymm1 = _mm256_srli_epi16(ymm1, 1);
    ymm0 = _mm256_avg_epu16(ymm0, ymm_zero);
    ymm1 = _mm256_avg_epu16(ymm1, ymm_zero);

    ymm0 = _mm256_packus_epi16(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

    ymm1 = _mm256_maddubs_epi16(ymm0, ymm_v);
    ymm0 = _mm256_maddubs_epi16(ymm0, ymm_u);

    ymm0 = _mm256_hadd_epi16(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm0 = _mm256_sub_epi16(ymm_8000, ymm0);
    ymm0 = _mm256_srli_epi16(ymm0, 8);
    ymm0 = _mm256_packus_epi16(ymm0, ymm0);

    __m128i xmm_u = _mm256_castsi256_si128(ymm0);
    __m128i xmm_v = _mm256_extracti128_si256(ymm0, 1);

    _mm_storel_epi64((__m128i*)dst_u, xmm_u);
    _mm_storel_epi64((__m128i*)dst_v, xmm_v);

    src_argb += 64;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX2

#ifdef HAS_CONVERT8TO16ROW_AVX2
LIBYUV_TARGET_AVX2
void Convert8To16Row_AVX2(const uint8_t* src_y,
                          uint16_t* dst_y,
                          int bits,
                          int width) {
  const int shift = 16 - bits;
  __m128i shift128 = _mm_cvtsi32_si128(shift);
  do {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_y);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    __m256i row0 = _mm256_unpacklo_epi8(ymm0, ymm0);
    __m256i row1 = _mm256_unpackhi_epi8(ymm0, ymm0);
    row0 = _mm256_srl_epi16(row0, shift128);
    row1 = _mm256_srl_epi16(row1, shift128);
    _mm256_storeu_si256((__m256i*)dst_y, row0);
    _mm256_storeu_si256((__m256i*)(dst_y + 16), row1);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT8TO16ROW_AVX2

#ifdef HAS_MULTIPLYROW_16_AVX2
LIBYUV_TARGET_AVX2
void MultiplyRow_16_AVX2(const uint16_t* src_y,
                         uint16_t* dst_y,
                         int scale,
                         int width) {
  __m256i scale256 = _mm256_set1_epi16((short)scale);
  do {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_y + 16));
    ymm0 = _mm256_mullo_epi16(ymm0, scale256);
    ymm1 = _mm256_mullo_epi16(ymm1, scale256);
    _mm256_storeu_si256((__m256i*)dst_y, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_y + 16), ymm1);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MULTIPLYROW_16_AVX2

#ifdef HAS_MERGEUVROW_AVX2
LIBYUV_TARGET_AVX2
void MergeUVRow_AVX2(const uint8_t* src_u,
                     const uint8_t* src_v,
                     uint8_t* dst_uv,
                     int width) {
  while (width > 0) {
    __m256i ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_u));
    __m256i ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_v));

    ymm1 = _mm256_slli_epi16(ymm1, 8);
    ymm0 = _mm256_or_si256(ymm0, ymm1);

    _mm256_storeu_si256((__m256i*)dst_uv, ymm0);

    src_u += 16;
    src_v += 16;
    dst_uv += 32;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEUVROW_AVX2

#ifdef HAS_MIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
  src += width;
  while (width > 0) {
    src -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x4e);
    _mm256_storeu_si256((__m256i*)dst, ymm0);
    dst += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORROW_AVX2

#ifdef HAS_MIRRORUVROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorUVRow_AVX2(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1));
  src_uv += width * 2;
  while (width > 0) {
    src_uv -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_uv);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x4e);
    _mm256_storeu_si256((__m256i*)dst_uv, ymm0);
    dst_uv += 32;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORUVROW_AVX2

#ifdef HAS_MIRRORROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void MirrorRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  const __m512i zmm_shuf = _mm512_broadcast_i32x4(
      _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
  const __m512i zmm_rev = _mm512_set_epi16(
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  const uint8_t* s = src + width;
  while (width >= 64) {
    s -= 64;
    __m512i v = _mm512_loadu_si512((const __m512i*)s);
    v = _mm512_shuffle_epi8(v, zmm_shuf);
    v = _mm512_shuffle_i64x2(v, v, 0x1b);
    _mm512_storeu_si512((__m512i*)dst, v);
    dst += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 k = (1ULL << width) - 1ULL;
    __m512i v = _mm512_maskz_loadu_epi8(k, src);
    v = _mm512_or_si512(_mm512_srli_epi16(v, 8), _mm512_slli_epi16(v, 8));
    __m512i idx =
        _mm512_sub_epi16(zmm_rev, _mm512_set1_epi16((short)((64 - width) >> 1)));
    v = PermutexvarEpi16(idx, v);
    if (width & 1) {
      __m512i v_hi = _mm512_alignr_epi64(v, v, 2);
      v = _mm512_alignr_epi8(v_hi, v, 1);
    }
    _mm512_mask_storeu_epi8(dst, k, v);
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORROW_AVX512BW

#ifdef HAS_MIRRORSPLITUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void MirrorSplitUVRow_AVX512BW(const uint8_t* src_uv,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width) {
  const __m512i zmm_shuf = _mm512_broadcast_i32x4(
      _mm_setr_epi8(14, 12, 10, 8, 6, 4, 2, 0, 15, 13, 11, 9, 7, 5, 3, 1));
  const __m512i zmm_perm = _mm512_setr_epi64(6, 4, 2, 0, 7, 5, 3, 1);
  const __m512i zmm_rev = _mm512_set_epi16(
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  const uint8_t* s = src_uv + (ptrdiff_t)width * 2;
  while (width >= 32) {
    s -= 64;
    __m512i v = _mm512_loadu_si512((const __m512i*)s);
    v = _mm512_shuffle_epi8(v, zmm_shuf);
    v = PermutexvarEpi64(zmm_perm, v);
    _mm256_storeu_si256((__m256i*)dst_u, _mm512_castsi512_si256(v));
    _mm256_storeu_si256((__m256i*)dst_v, _mm512_extracti64x4_epi64(v, 1));
    dst_u += 32;
    dst_v += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i idx =
        _mm512_sub_epi16(zmm_rev, _mm512_set1_epi16((short)(32 - width)));
    __m512i v = _mm512_maskz_loadu_epi16(k, src_uv);
    v = PermutexvarEpi16(idx, v);
    _mm256_mask_storeu_epi8(dst_u, k, _mm512_cvtepi16_epi8(v));
    _mm256_mask_storeu_epi8(dst_v, k,
                            _mm512_cvtepi16_epi8(_mm512_srli_epi16(v, 8)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORSPLITUVROW_AVX512BW

#ifdef HAS_MIRRORUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void MirrorUVRow_AVX512BW(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  const __m512i zmm_idx = _mm512_set_epi16(
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  const uint8_t* s = src_uv + (ptrdiff_t)width * 2;
  while (width >= 32) {
    s -= 64;
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)s);
    zmm0 = PermutexvarEpi16(zmm_idx, zmm0);
    _mm512_storeu_si512((__m512i*)dst_uv, zmm0);
    dst_uv += 64;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i idx =
        _mm512_sub_epi16(zmm_idx, _mm512_set1_epi16((short)(32 - width)));
    __m512i zmm0 = _mm512_maskz_loadu_epi16(k, src_uv);
    _mm512_mask_storeu_epi16(dst_uv, k, PermutexvarEpi16(idx, zmm0));
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORUVROW_AVX512BW

#ifdef HAS_SWAPUVROW_AVX2
LIBYUV_TARGET_AVX2
void SwapUVRow_AVX2(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14));
  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_uv + 32));
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    _mm256_storeu_si256((__m256i*)dst_vu, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_vu + 32), ymm1);
    src_uv += 64;
    dst_vu += 64;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SWAPUVROW_AVX2

#ifdef HAS_MIRRORSPLITUVROW_SSSE3
LIBYUV_TARGET_SSSE3
void MirrorSplitUVRow_SSSE3(const uint8_t* src_uv,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width) {
  const __m128i xmm_shuf =
      _mm_setr_epi8(14, 12, 10, 8, 6, 4, 2, 0, 15, 13, 11, 9, 7, 5, 3, 1);
  src_uv += (ptrdiff_t)width * 2;
  while (width > 0) {
    src_uv -= 16;
    __m128i xmm0 =
        _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)src_uv), xmm_shuf);
    _mm_storel_epi64((__m128i*)dst_u, xmm0);
    _mm_storel_epi64((__m128i*)dst_v, _mm_unpackhi_epi64(xmm0, xmm0));
    dst_u += 8;
    dst_v += 8;
    width -= 8;
  }
}
#endif  // HAS_MIRRORSPLITUVROW_SSSE3

#ifdef HAS_MIRRORSPLITUVROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorSplitUVRow_AVX2(const uint8_t* src_uv,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width) {
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_setr_epi8(14, 12, 10, 8, 6, 4, 2, 0, 15, 13, 11, 9, 7, 5, 3, 1));
  src_uv += (ptrdiff_t)width * 2;
  while (width > 0) {
    src_uv -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_uv);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x72);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(ymm0));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_extracti128_si256(ymm0, 1));
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORSPLITUVROW_AVX2

#ifdef HAS_RGB24MIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void RGB24MirrorRow_AVX2(const uint8_t* src_rgb24,
                         uint8_t* dst_rgb24,
                         int width) {
  __m256i shuf0 =
      _mm256_setr_epi8(-1, 12, 13, 14, 9, 10, 11, 6, 7, 8, 3, 4, 5, 0, 1, 2, -1,
                       12, 13, 14, 9, 10, 11, 6, 7, 8, 3, 4, 5, 0, 1, 2);
  __m128i shuf1 =
      _mm_setr_epi8(13, 14, 15, 10, 11, 12, 7, 8, 9, 4, 5, 6, 1, 2, 3, -1);

  src_rgb24 += (ptrdiff_t)width * 3 - 96;
  while (width > 0) {
    __m128i v0_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 0));
    __m128i v0_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 15));
    __m256i v0 =
        _mm256_inserti128_si256(_mm256_castsi128_si256(v0_lo), v0_hi, 1);

    __m128i v1_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 30));
    __m128i v1_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 45));
    __m256i v1 =
        _mm256_inserti128_si256(_mm256_castsi128_si256(v1_lo), v1_hi, 1);

    __m128i v2_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 60));
    __m128i v2_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 75));
    __m256i v2 =
        _mm256_inserti128_si256(_mm256_castsi128_si256(v2_lo), v2_hi, 1);

    __m128i v3 = _mm_loadu_si128((const __m128i*)(src_rgb24 + 80));

    v0 = _mm256_shuffle_epi8(v0, shuf0);
    v1 = _mm256_shuffle_epi8(v1, shuf0);
    v2 = _mm256_shuffle_epi8(v2, shuf0);
    v3 = _mm_shuffle_epi8(v3, shuf1);

    _mm_storeu_si128((__m128i*)(dst_rgb24 + 80), _mm256_castsi256_si128(v0));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 65),
                     _mm256_extracti128_si256(v0, 1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 50), _mm256_castsi256_si128(v1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 35),
                     _mm256_extracti128_si256(v1, 1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 20), _mm256_castsi256_si128(v2));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 5),
                     _mm256_extracti128_si256(v2, 1));
    _mm_storel_epi64((__m128i*)(dst_rgb24 + 0), v3);

    src_rgb24 -= 96;
    dst_rgb24 += 96;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_RGB24MIRRORROW_AVX2

#ifdef HAS_INTERPOLATEROW_AVX2
LIBYUV_TARGET_AVX2
void InterpolateRow_AVX2(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int width,
                         int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  __m256i ymm_y = _mm256_set1_epi16((y1 << 8) | y0);
  __m256i ymm_8080 = _mm256_set1_epi16(0x8080);
  int i;

  if (y1 == 0) {
    for (i = 0; i < width; i += 32) {
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_loadu_si256((const __m256i*)(src_ptr + i)));
    }
  } else if (y1 == 128) {
    for (i = 0; i < width; i += 32) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      _mm256_storeu_si256((__m256i*)(dst_ptr + i), _mm256_avg_epu8(row0, row1));
    }
  } else {
    for (i = 0; i < width; i += 32) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      __m256i low = _mm256_unpacklo_epi8(row0, row1);
      __m256i high = _mm256_unpackhi_epi8(row0, row1);
      low = _mm256_sub_epi8(low, ymm_8080);
      high = _mm256_sub_epi8(high, ymm_8080);
      low = _mm256_maddubs_epi16(ymm_y, low);
      high = _mm256_maddubs_epi16(ymm_y, high);
      low = _mm256_add_epi16(low, ymm_8080);
      high = _mm256_add_epi16(high, ymm_8080);
      low = _mm256_srli_epi16(low, 8);
      high = _mm256_srli_epi16(high, 8);
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_packus_epi16(low, high));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_INTERPOLATEROW_AVX2

#ifdef HAS_INTERPOLATEROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void InterpolateRow_AVX512BW(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             int width,
                             int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  __m512i zmm_y = _mm512_set1_epi16((short)((y1 << 8) | y0));
  __m512i zmm_8080 = _mm512_set1_epi16((short)0x8080);
  int i;

  if (y1 == 0) {
    for (i = 0; i <= width - 64; i += 64) {
      _mm512_storeu_si512((__m512i*)(dst_ptr + i),
                          _mm512_loadu_si512((const __m512i*)(src_ptr + i)));
    }
    if (i < width) {
      __mmask64 mask = (__mmask64)((1ull << (width - i)) - 1ull);
      _mm512_mask_storeu_epi8(dst_ptr + i, mask,
                              _mm512_maskz_loadu_epi8(mask, src_ptr + i));
    }
  } else if (y1 == 128) {
    for (i = 0; i <= width - 64; i += 64) {
      __m512i row0 = _mm512_loadu_si512((const __m512i*)(src_ptr + i));
      __m512i row1 = _mm512_loadu_si512((const __m512i*)(src_ptr1 + i));
      _mm512_storeu_si512((__m512i*)(dst_ptr + i), _mm512_avg_epu8(row0, row1));
    }
    if (i < width) {
      __mmask64 mask = (__mmask64)((1ull << (width - i)) - 1ull);
      __m512i row0 = _mm512_maskz_loadu_epi8(mask, src_ptr + i);
      __m512i row1 = _mm512_maskz_loadu_epi8(mask, src_ptr1 + i);
      _mm512_mask_storeu_epi8(dst_ptr + i, mask, _mm512_avg_epu8(row0, row1));
    }
  } else {
    for (i = 0; i <= width - 64; i += 64) {
      __m512i row0 = _mm512_loadu_si512((const __m512i*)(src_ptr + i));
      __m512i row1 = _mm512_loadu_si512((const __m512i*)(src_ptr1 + i));
      __m512i low = _mm512_unpacklo_epi8(row0, row1);
      __m512i high = _mm512_unpackhi_epi8(row0, row1);
      low = _mm512_sub_epi8(low, zmm_8080);
      high = _mm512_sub_epi8(high, zmm_8080);
      low = _mm512_maddubs_epi16(zmm_y, low);
      high = _mm512_maddubs_epi16(zmm_y, high);
      low = _mm512_add_epi16(low, zmm_8080);
      high = _mm512_add_epi16(high, zmm_8080);
      low = _mm512_srli_epi16(low, 8);
      high = _mm512_srli_epi16(high, 8);
      _mm512_storeu_si512((__m512i*)(dst_ptr + i),
                          _mm512_packus_epi16(low, high));
    }
    if (i < width) {
      __mmask64 mask = (__mmask64)((1ull << (width - i)) - 1ull);
      __m512i row0 = _mm512_maskz_loadu_epi8(mask, src_ptr + i);
      __m512i row1 = _mm512_maskz_loadu_epi8(mask, src_ptr1 + i);
      __m512i low = _mm512_unpacklo_epi8(row0, row1);
      __m512i high = _mm512_unpackhi_epi8(row0, row1);
      low = _mm512_sub_epi8(low, zmm_8080);
      high = _mm512_sub_epi8(high, zmm_8080);
      low = _mm512_maddubs_epi16(zmm_y, low);
      high = _mm512_maddubs_epi16(zmm_y, high);
      low = _mm512_add_epi16(low, zmm_8080);
      high = _mm512_add_epi16(high, zmm_8080);
      low = _mm512_srli_epi16(low, 8);
      high = _mm512_srli_epi16(high, 8);
      _mm512_mask_storeu_epi8(dst_ptr + i, mask,
                              _mm512_packus_epi16(low, high));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_INTERPOLATEROW_AVX512BW

#ifdef HAS_INTERPOLATEROW_16_AVX2
LIBYUV_TARGET_AVX2
void InterpolateRow_16_AVX2(uint16_t* dst_ptr,
                            const uint16_t* src_ptr,
                            ptrdiff_t src_stride,
                            int width,
                            int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  __m256i ymm_y = _mm256_set1_epi32((y1 << 16) | y0);
  __m256i ymm_8000 = _mm256_set1_epi16((short)0x8000);
  __m256i ymm_round = _mm256_set1_epi32(8388736);  // 0x800000 + 128
  int i;

  if (y1 == 0) {
    for (i = 0; i < width; i += 16) {
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_loadu_si256((const __m256i*)(src_ptr + i)));
    }
  } else if (y1 == 128) {
    for (i = 0; i < width; i += 16) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_avg_epu16(row0, row1));
    }
  } else {
    for (i = 0; i < width; i += 16) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      __m256i row0l = _mm256_unpacklo_epi16(row0, row1);
      __m256i row0h = _mm256_unpackhi_epi16(row0, row1);
      row0l = _mm256_sub_epi16(row0l, ymm_8000);
      row0h = _mm256_sub_epi16(row0h, ymm_8000);
      __m256i resl = _mm256_madd_epi16(row0l, ymm_y);
      __m256i resh = _mm256_madd_epi16(row0h, ymm_y);
      resl = _mm256_add_epi32(resl, ymm_round);
      resh = _mm256_add_epi32(resh, ymm_round);
      resl = _mm256_srai_epi32(resl, 8);
      resh = _mm256_srai_epi32(resh, 8);
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_packus_epi32(resl, resh));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_INTERPOLATEROW_16_AVX2

#ifdef HAS_ARGBMIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBMirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  __m256i ymm_shuf = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
  src += width * 4;
  while (width > 0) {
    src -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src);
    ymm0 = _mm256_permutevar8x32_epi32(ymm0, ymm_shuf);
    _mm256_storeu_si256((__m256i*)dst, ymm0);
    dst += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBMIRRORROW_AVX2

#ifdef HAS_J400TOARGBROW_AVX2
static const uint8_t kShuffleMaskJ400ToARGB_0[32] = {
    0u, 0u, 0u, 128u, 1u, 1u, 1u, 128u, 2u, 2u, 2u, 128u, 3u, 3u, 3u, 128u,
    4u, 4u, 4u, 128u, 5u, 5u, 5u, 128u, 6u, 6u, 6u, 128u, 7u, 7u, 7u, 128u};
static const uint8_t kShuffleMaskJ400ToARGB_1[32] = {
    8u,   8u,   8u,  128u, 9u,   9u,   9u,  128u, 10u,  10u, 10u,
    128u, 11u,  11u, 11u,  128u, 12u,  12u, 12u,  128u, 13u, 13u,
    13u,  128u, 14u, 14u,  14u,  128u, 15u, 15u,  15u,  128u};

LIBYUV_TARGET_AVX2
void J400ToARGBRow_AVX2(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  __m256i ymm_mask0 =
      _mm256_loadu_si256((const __m256i*)kShuffleMaskJ400ToARGB_0);
  __m256i ymm_mask1 =
      _mm256_loadu_si256((const __m256i*)kShuffleMaskJ400ToARGB_1);
  __m256i ymm_alpha = _mm256_set1_epi32((int)0xff000000u);

  while (width > 0) {
    __m256i ymm0 =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)src_y));

    __m256i ymm1 = _mm256_shuffle_epi8(ymm0, ymm_mask0);
    __m256i ymm2 = _mm256_shuffle_epi8(ymm0, ymm_mask1);

    ymm1 = _mm256_or_si256(ymm1, ymm_alpha);
    ymm2 = _mm256_or_si256(ymm2, ymm_alpha);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm1);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm2);

    src_y += 16;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_J400TOARGBROW_AVX2

#ifdef HAS_RGB24TOARGBROW_AVX2
static const uint8_t kShuffleMaskRGB24ToARGB[2][16] = {
    {0u, 1u, 2u, 128u, 3u, 4u, 5u, 128u, 6u, 7u, 8u, 128u, 9u, 10u, 11u, 128u},
    {4u, 5u, 6u, 128u, 7u, 8u, 9u, 128u, 10u, 11u, 12u, 128u, 13u, 14u, 15u,
     128u}};
#endif  // HAS_RGB24TOARGBROW_AVX2

#ifdef HAS_RGB565TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void RGB565ToARGBRow_AVX2(const uint8_t* src_rgb565,
                          uint8_t* dst_argb,
                          int width) {
  __m256i ymm_scale_rb = _mm256_set1_epi32(0x01080108);
  __m256i ymm_scale_g = _mm256_set1_epi32(0x20802080);
  __m256i ymm_mask_b = _mm256_set1_epi16((short)0xf800);
  __m256i ymm_mask_g = _mm256_set1_epi16(0x07e0);
  __m256i ymm_mask_a = _mm256_set1_epi16((short)0xff00);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_rgb565);
    __m256i ymm1 = ymm0;
    __m256i ymm2 = ymm0;

    ymm1 = _mm256_and_si256(ymm1, ymm_mask_b);
    ymm2 = _mm256_slli_epi16(ymm2, 11);
    ymm1 = _mm256_mulhi_epu16(ymm1, ymm_scale_rb);
    ymm2 = _mm256_mulhi_epu16(ymm2, ymm_scale_rb);
    ymm1 = _mm256_slli_epi16(ymm1, 8);
    ymm1 = _mm256_or_si256(ymm1, ymm2);  // RB

    ymm0 = _mm256_and_si256(ymm0, ymm_mask_g);
    ymm0 = _mm256_mulhi_epu16(ymm0, ymm_scale_g);
    ymm0 = _mm256_or_si256(ymm0, ymm_mask_a);  // GA

    ymm2 = _mm256_unpacklo_epi8(ymm1, ymm0);
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm0);

    ymm0 = _mm256_permute2x128_si256(ymm2, ymm1, 0x20);
    ymm1 = _mm256_permute2x128_si256(ymm2, ymm1, 0x31);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);

    src_rgb565 += 32;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_RGB565TOARGBROW_AVX2

#ifdef HAS_ARGB1555TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void ARGB1555ToARGBRow_AVX2(const uint8_t* src_argb1555,
                            uint8_t* dst_argb,
                            int width) {
  __m256i ymm_scale_rb = _mm256_set1_epi32(0x01080108);
  __m256i ymm_scale_g = _mm256_set1_epi32(0x42004200);
  __m256i ymm_mask_b = _mm256_set1_epi16((short)0xf800);
  __m256i ymm_mask_g = _mm256_set1_epi16(0x03e0);
  __m256i ymm_mask_a = _mm256_set1_epi16((short)0xff00);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb1555);
    __m256i ymm1 = ymm0;
    __m256i ymm2 = ymm0;

    ymm1 = _mm256_slli_epi16(ymm1, 1);
    ymm2 = _mm256_slli_epi16(ymm2, 11);
    ymm1 = _mm256_and_si256(ymm1, ymm_mask_b);
    ymm2 = _mm256_mulhi_epu16(ymm2, ymm_scale_rb);
    ymm1 = _mm256_mulhi_epu16(ymm1, ymm_scale_rb);
    ymm1 = _mm256_slli_epi16(ymm1, 8);
    ymm1 = _mm256_or_si256(ymm1, ymm2);  // RB

    ymm2 = ymm0;
    ymm0 = _mm256_and_si256(ymm0, ymm_mask_g);
    ymm2 = _mm256_srai_epi16(ymm2, 8);
    ymm0 = _mm256_mulhi_epu16(ymm0, ymm_scale_g);
    ymm2 = _mm256_and_si256(ymm2, ymm_mask_a);
    ymm0 = _mm256_or_si256(ymm0, ymm2);  // GA

    ymm2 = _mm256_unpacklo_epi8(ymm1, ymm0);
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm0);

    ymm0 = _mm256_permute2x128_si256(ymm2, ymm1, 0x20);
    ymm1 = _mm256_permute2x128_si256(ymm2, ymm1, 0x31);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);

    src_argb1555 += 32;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGB1555TOARGBROW_AVX2

#ifdef HAS_ARGB4444TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void ARGB4444ToARGBRow_AVX2(const uint8_t* src_argb4444,
                            uint8_t* dst_argb,
                            int width) {
  __m256i ymm_mask = _mm256_set1_epi32(0x0f0f0f0f);
  __m256i ymm_mask2 = _mm256_slli_epi32(ymm_mask, 4);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb4444);
    __m256i ymm2 = ymm0;

    ymm0 = _mm256_and_si256(ymm0, ymm_mask);
    ymm2 = _mm256_and_si256(ymm2, ymm_mask2);

    __m256i ymm1 = ymm0;
    __m256i ymm3 = ymm2;

    ymm1 = _mm256_slli_epi16(ymm1, 4);
    ymm3 = _mm256_srli_epi16(ymm3, 4);

    ymm0 = _mm256_or_si256(ymm0, ymm1);
    ymm2 = _mm256_or_si256(ymm2, ymm3);

    ymm1 = ymm0;
    ymm0 = _mm256_unpacklo_epi8(ymm0, ymm2);
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm2);

    ymm2 = _mm256_permute2x128_si256(ymm0, ymm1, 0x20);
    ymm1 = _mm256_permute2x128_si256(ymm0, ymm1, 0x31);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm2);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);

    src_argb4444 += 32;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGB4444TOARGBROW_AVX2

#ifdef HAS_ARGBTORGB565DITHERROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToRGB565DitherRow_AVX2(const uint8_t* src_argb,
                                uint8_t* dst_rgb,
                                uint32_t dither4,
                                int width) {
  __m128i dither_shuf =
      _mm_setr_epi8(0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3);
  __m128i d128 = _mm_shuffle_epi8(_mm_cvtsi32_si128((int)dither4), dither_shuf);
  __m256i ymm_dither = _mm256_broadcastsi128_si256(d128);
  __m256i mask_b = _mm256_set1_epi32(0x0000001f);
  __m256i mask_g = _mm256_set1_epi32(0x000007e0);
  __m256i mask_r = _mm256_set1_epi32(0x0000f800);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    ymm0 = _mm256_adds_epu8(ymm0, ymm_dither);
    __m256i ymm2 = _mm256_srli_epi32(ymm0, 5);
    __m256i ymm1 = _mm256_srli_epi32(ymm0, 3);
    ymm0 = _mm256_srli_epi32(ymm0, 8);
    ymm2 = _mm256_and_si256(ymm2, mask_g);
    ymm1 = _mm256_and_si256(ymm1, mask_b);
    ymm0 = _mm256_and_si256(ymm0, mask_r);
    ymm1 = _mm256_or_si256(ymm2, ymm1);
    ymm0 = _mm256_or_si256(ymm1, ymm0);
    ymm0 = _mm256_packus_epi32(ymm0, ymm0);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    _mm_storeu_si128((__m128i*)dst_rgb, _mm256_castsi256_si128(ymm0));
    src_argb += 32;
    dst_rgb += 16;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTORGB565DITHERROW_AVX2

#ifdef HAS_ARGBTORGB565ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToRGB565Row_AVX2(const uint8_t* src_argb,
                          uint8_t* dst_rgb,
                          int width) {
  __m256i mask_b = _mm256_set1_epi32(0x0000001f);
  __m256i mask_g = _mm256_set1_epi32(0x000007e0);
  __m256i mask_r = _mm256_set1_epi32(0x0000f800);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm2 = _mm256_srli_epi32(ymm0, 5);
    __m256i ymm1 = _mm256_srli_epi32(ymm0, 3);
    ymm0 = _mm256_srli_epi32(ymm0, 8);
    ymm2 = _mm256_and_si256(ymm2, mask_g);
    ymm1 = _mm256_and_si256(ymm1, mask_b);
    ymm0 = _mm256_and_si256(ymm0, mask_r);
    ymm1 = _mm256_or_si256(ymm2, ymm1);
    ymm0 = _mm256_or_si256(ymm1, ymm0);
    ymm0 = _mm256_packus_epi32(ymm0, ymm0);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    _mm_storeu_si128((__m128i*)dst_rgb, _mm256_castsi256_si128(ymm0));
    src_argb += 32;
    dst_rgb += 16;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTORGB565ROW_AVX2

#ifdef HAS_ARGBTOARGB1555ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToARGB1555Row_AVX2(const uint8_t* src_argb,
                            uint8_t* dst_argb1555,
                            int width) {
  __m256i mask_b = _mm256_set1_epi32(0x0000001f);
  __m256i mask_g = _mm256_set1_epi32(0x000003e0);
  __m256i mask_r = _mm256_set1_epi32(0x00007c00);
  __m256i mask_a = _mm256_set1_epi32(0x00008000);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm2 = _mm256_srli_epi32(ymm0, 6);
    __m256i ymm1 = _mm256_srli_epi32(ymm0, 3);
    __m256i ymm7 = _mm256_srli_epi32(ymm0, 16);
    ymm0 = _mm256_srli_epi32(ymm0, 9);
    ymm2 = _mm256_and_si256(ymm2, mask_g);
    ymm1 = _mm256_and_si256(ymm1, mask_b);
    ymm7 = _mm256_and_si256(ymm7, mask_a);
    ymm0 = _mm256_and_si256(ymm0, mask_r);
    ymm1 = _mm256_or_si256(ymm2, ymm1);
    ymm0 = _mm256_or_si256(ymm7, ymm0);
    ymm0 = _mm256_or_si256(ymm1, ymm0);
    ymm0 = _mm256_packus_epi32(ymm0, ymm0);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    _mm_storeu_si128((__m128i*)dst_argb1555, _mm256_castsi256_si128(ymm0));
    src_argb += 32;
    dst_argb1555 += 16;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOARGB1555ROW_AVX2

#ifdef HAS_ARGBTOARGB4444ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToARGB4444Row_AVX2(const uint8_t* src_argb,
                            uint8_t* dst_argb4444,
                            int width) {
  __m256i mask_b = _mm256_set1_epi32(0x0000000f);
  __m256i mask_g = _mm256_set1_epi32(0x000000f0);
  __m256i mask_r = _mm256_set1_epi32(0x00000f00);
  __m256i mask_a = _mm256_set1_epi32(0x0000f000);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm2 = _mm256_srli_epi32(ymm0, 8);
    __m256i ymm1 = _mm256_srli_epi32(ymm0, 4);
    __m256i ymm7 = _mm256_srli_epi32(ymm0, 16);
    ymm0 = _mm256_srli_epi32(ymm0, 12);
    ymm2 = _mm256_and_si256(ymm2, mask_g);
    ymm1 = _mm256_and_si256(ymm1, mask_b);
    ymm7 = _mm256_and_si256(ymm7, mask_a);
    ymm0 = _mm256_and_si256(ymm0, mask_r);
    ymm1 = _mm256_or_si256(ymm2, ymm1);
    ymm0 = _mm256_or_si256(ymm7, ymm0);
    ymm0 = _mm256_or_si256(ymm1, ymm0);
    ymm0 = _mm256_packus_epi32(ymm0, ymm0);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    _mm_storeu_si128((__m128i*)dst_argb4444, _mm256_castsi256_si128(ymm0));
    src_argb += 32;
    dst_argb4444 += 16;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOARGB4444ROW_AVX2

#ifdef HAS_RGB24TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void RGB24ToARGBRow_AVX2(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width) {
  __m256i ymm_alpha = _mm256_set1_epi32(0xff000000);
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShuffleMaskRGB24ToARGB[0]));
  __m256i ymm_shuf2 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShuffleMaskRGB24ToARGB[1]));

  while (width > 0) {
    __m128i xmm0 = _mm_loadu_si128((const __m128i*)src_rgb24);
    __m256i ymm0 = _mm256_castsi128_si256(xmm0);
    ymm0 = _mm256_inserti128_si256(
        ymm0, _mm_loadu_si128((const __m128i*)(src_rgb24 + 12)), 1);

    __m128i xmm1 = _mm_loadu_si128((const __m128i*)(src_rgb24 + 24));
    __m256i ymm1 = _mm256_castsi128_si256(xmm1);
    ymm1 = _mm256_inserti128_si256(
        ymm1, _mm_loadu_si128((const __m128i*)(src_rgb24 + 36)), 1);

    __m128i xmm2 = _mm_loadu_si128((const __m128i*)(src_rgb24 + 48));
    __m256i ymm2 = _mm256_castsi128_si256(xmm2);
    ymm2 = _mm256_inserti128_si256(
        ymm2, _mm_loadu_si128((const __m128i*)(src_rgb24 + 60)), 1);

    __m128i xmm3 = _mm_loadu_si128((const __m128i*)(src_rgb24 + 68));
    __m256i ymm3 = _mm256_castsi128_si256(xmm3);
    ymm3 = _mm256_inserti128_si256(
        ymm3, _mm_loadu_si128((const __m128i*)(src_rgb24 + 80)), 1);

    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf2);

    ymm0 = _mm256_or_si256(ymm0, ymm_alpha);
    ymm1 = _mm256_or_si256(ymm1, ymm_alpha);
    ymm2 = _mm256_or_si256(ymm2, ymm_alpha);
    ymm3 = _mm256_or_si256(ymm3, ymm_alpha);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);
    _mm256_storeu_si256((__m256i*)(dst_argb + 64), ymm2);
    _mm256_storeu_si256((__m256i*)(dst_argb + 96), ymm3);

    src_rgb24 += 96;
    dst_argb += 128;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_RGB24TOARGBROW_AVX2

#ifdef HAS_ARGBSHUFFLEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBShuffleRow_AVX2(const uint8_t* src_argb,
                         uint8_t* dst_argb,
                         const uint8_t* shuffler,
                         int width) {
  __m256i control =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)shuffler));
  while (width >= 16) {
    __m256i row = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    row = _mm256_shuffle_epi8(row, control);
    row1 = _mm256_shuffle_epi8(row1, control);
    _mm256_storeu_si256((__m256i*)dst_argb, row);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), row1);
    src_argb += 64;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBSHUFFLEROW_AVX2

#ifdef HAS_ARGBSHUFFLEROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBShuffleRow_AVX512BW(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             const uint8_t* shuffler,
                             int width) {
  __m512i control =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)shuffler));
  while (width >= 32) {
    __m512i row = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i row1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    row = _mm512_shuffle_epi8(row, control);
    row1 = _mm512_shuffle_epi8(row1, control);
    _mm512_storeu_si512((__m512i*)dst_argb, row);
    _mm512_storeu_si512((__m512i*)(dst_argb + 64), row1);
    src_argb += 128;
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 mask0 = (__mmask16)mask;
    __mmask16 mask1 = (__mmask16)(mask >> 16);
    __m512i row = _mm512_maskz_loadu_epi32(mask0, src_argb);
    __m512i row1 = _mm512_maskz_loadu_epi32(mask1, src_argb + 64);
    row = _mm512_shuffle_epi8(row, control);
    row1 = _mm512_shuffle_epi8(row1, control);
    _mm512_mask_storeu_epi32(dst_argb, mask0, row);
    _mm512_mask_storeu_epi32(dst_argb + 64, mask1, row1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBSHUFFLEROW_AVX512BW

#ifdef HAS_I422TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void I422ToARGBRow_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  __m256i ymm_kUVToB = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  __m256i ymm_kUVToG = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  __m256i ymm_kUVToR = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  __m256i ymm_kYBiasToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb);
  __m256i ymm_128 = _mm256_set1_epi8((char)0x80);
  __m256i ymm_alpha = _mm256_set1_epi8((char)0xff);
  __m256i ymm_u_zero = _mm256_setzero_si256();

  ptrdiff_t offset = src_v - src_u;

  while (width >= 16) {
    __m128i xmm_u = _mm_loadl_epi64((const __m128i*)src_u);
    __m128i xmm_v = _mm_loadl_epi64((const __m128i*)(src_u + offset));
    src_u += 8;

    __m256i ymm3 = _mm256_insertf128_si256(ymm_u_zero, xmm_u, 0);
    __m256i ymm1 = _mm256_insertf128_si256(ymm_u_zero, xmm_v, 0);

    ymm3 = _mm256_unpacklo_epi8(ymm3, ymm1);
    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm3);

    __m128i xmm_y = _mm_loadu_si128((const __m128i*)src_y);
    src_y += 16;
    __m256i ymm4 = _mm256_insertf128_si256(ymm_u_zero, xmm_y, 0);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    ymm4 = _mm256_unpacklo_epi8(ymm4, ymm4);

    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm1 = _mm256_subs_epi16(ymm4, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm4);

    ymm0 = _mm256_srai_epi16(ymm0, 6);
    ymm1 = _mm256_srai_epi16(ymm1, 6);
    ymm2 = _mm256_srai_epi16(ymm2, 6);

    ymm0 = _mm256_packus_epi16(ymm0, ymm0);
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);

    ymm0 = _mm256_unpacklo_epi8(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm_alpha);
    ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

    __m256i argb0 = _mm256_unpacklo_epi16(ymm0, ymm2);
    __m256i argb1 = _mm256_unpackhi_epi16(ymm0, ymm2);

    _mm256_storeu_si256((__m256i*)dst_argb, argb0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), argb1);

    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TOARGBROW_AVX2

#ifdef HAS_I422TOAR30ROW_AVX2
LIBYUV_TARGET_AVX2
void I422ToAR30Row_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_ar30,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  __m256i ymm_kUVToB = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  __m256i ymm_kUVToG = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  __m256i ymm_kUVToR = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  __m256i ymm_kYBiasToRgb = _mm256_sub_epi16(
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb),
      _mm256_set1_epi16(24));
  __m256i ymm_128 = _mm256_set1_epi8((char)0x80);
  __m256i ymm_3ff0 = _mm256_set1_epi16((short)0x3ff0);
  __m256i ymm_c000 = _mm256_set1_epi16((short)0xc000);
  __m256i ymm_u_zero = _mm256_setzero_si256();

  ptrdiff_t offset = src_v - src_u;

  while (width >= 16) {
    __m128i xmm_u = _mm_loadl_epi64((const __m128i*)src_u);
    __m128i xmm_v = _mm_loadl_epi64((const __m128i*)(src_u + offset));
    src_u += 8;

    __m256i ymm3 = _mm256_insertf128_si256(ymm_u_zero, xmm_u, 0);
    __m256i ymm1 = _mm256_insertf128_si256(ymm_u_zero, xmm_v, 0);

    ymm3 = _mm256_unpacklo_epi8(ymm3, ymm1);
    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm3);

    __m128i xmm_y = _mm_loadu_si128((const __m128i*)src_y);
    src_y += 16;
    __m256i ymm4 = _mm256_insertf128_si256(ymm_u_zero, xmm_y, 0);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    ymm4 = _mm256_unpacklo_epi8(ymm4, ymm4);

    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm1 = _mm256_subs_epi16(ymm4, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm4);

    ymm0 = _mm256_min_epi16(ymm0, ymm_3ff0);
    ymm1 = _mm256_min_epi16(ymm1, ymm_3ff0);
    ymm2 = _mm256_min_epi16(ymm2, ymm_3ff0);
    ymm0 = _mm256_max_epi16(ymm0, ymm_u_zero);
    ymm1 = _mm256_max_epi16(ymm1, ymm_u_zero);
    ymm2 = _mm256_max_epi16(ymm2, ymm_u_zero);

    __m256i ymm_g_masked = _mm256_and_si256(ymm1, ymm_3ff0);
    __m256i ymm_r_masked = _mm256_and_si256(ymm2, ymm_3ff0);
    ymm0 = _mm256_srli_epi16(ymm0, 4);
    ymm3 = _mm256_slli_epi16(ymm_g_masked, 6);
    ymm0 = _mm256_or_si256(ymm0, ymm3);
    ymm1 = _mm256_srli_epi16(ymm1, 10);
    ymm2 = _mm256_or_si256(ymm_r_masked, ymm_c000);
    ymm2 = _mm256_or_si256(ymm2, ymm1);

    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

    __m256i ar30_lo = _mm256_unpacklo_epi16(ymm0, ymm2);
    __m256i ar30_hi = _mm256_unpackhi_epi16(ymm0, ymm2);

    _mm256_storeu_si256((__m256i*)dst_ar30, ar30_lo);
    _mm256_storeu_si256((__m256i*)(dst_ar30 + 32), ar30_hi);

    dst_ar30 += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TOAR30ROW_AVX2

#ifdef HAS_NV12TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void NV12ToARGBRow_AVX2(const uint8_t* src_y,
                        const uint8_t* src_uv,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  __m256i ymm_kUVToB = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  __m256i ymm_kUVToG = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  __m256i ymm_kUVToR = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  __m256i ymm_kYBiasToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb);
  __m256i ymm_128 = _mm256_set1_epi8((char)0x80);
  __m256i ymm_alpha = _mm256_set1_epi8((char)0xff);
  __m256i ymm_u_zero = _mm256_setzero_si256();

  while (width >= 16) {
    __m128i xmm_uv = _mm_loadu_si128((const __m128i*)src_uv);
    src_uv += 16;

    __m256i ymm3 = _mm256_insertf128_si256(ymm_u_zero, xmm_uv, 0);
    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm3);

    __m128i xmm_y = _mm_loadu_si128((const __m128i*)src_y);
    src_y += 16;
    __m256i ymm4 = _mm256_insertf128_si256(ymm_u_zero, xmm_y, 0);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    ymm4 = _mm256_unpacklo_epi8(ymm4, ymm4);

    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    __m256i ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm1 = _mm256_subs_epi16(ymm4, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm4);

    ymm0 = _mm256_srai_epi16(ymm0, 6);
    ymm1 = _mm256_srai_epi16(ymm1, 6);
    ymm2 = _mm256_srai_epi16(ymm2, 6);

    ymm0 = _mm256_packus_epi16(ymm0, ymm0);
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);

    ymm0 = _mm256_unpacklo_epi8(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm_alpha);
    ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

    __m256i argb0 = _mm256_unpacklo_epi16(ymm0, ymm2);
    __m256i argb1 = _mm256_unpackhi_epi16(ymm0, ymm2);

    _mm256_storeu_si256((__m256i*)dst_argb, argb0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), argb1);

    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV12TOARGBROW_AVX2

#ifdef HAS_NV21TOARGBROW_AVX2
static const uint8_t kShuffleNV21_AVX2[32] = {
    1u, 0u, 3u, 2u, 5u, 4u, 7u, 6u, 9u, 8u, 11u, 10u, 13u, 12u, 15u, 14u,
    1u, 0u, 3u, 2u, 5u, 4u, 7u, 6u, 9u, 8u, 11u, 10u, 13u, 12u, 15u, 14u};

LIBYUV_TARGET_AVX2
void NV21ToARGBRow_AVX2(const uint8_t* src_y,
                        const uint8_t* src_vu,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  __m256i ymm_kUVToB = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  __m256i ymm_kUVToG = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  __m256i ymm_kUVToR = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  __m256i ymm_kYBiasToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb);
  __m256i ymm_128 = _mm256_set1_epi8((char)0x80);
  __m256i ymm_alpha = _mm256_set1_epi8((char)0xff);
  __m256i ymm_shuf_nv21 = _mm256_loadu_si256((const __m256i*)kShuffleNV21_AVX2);
  __m256i ymm_u_zero = _mm256_setzero_si256();

  while (width >= 16) {
    __m128i xmm_vu = _mm_loadu_si128((const __m128i*)src_vu);
    src_vu += 16;

    __m256i ymm3 = _mm256_insertf128_si256(ymm_u_zero, xmm_vu, 0);
    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf_nv21);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm3);

    __m128i xmm_y = _mm_loadu_si128((const __m128i*)src_y);
    src_y += 16;
    __m256i ymm4 = _mm256_insertf128_si256(ymm_u_zero, xmm_y, 0);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    ymm4 = _mm256_unpacklo_epi8(ymm4, ymm4);

    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    __m256i ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm1 = _mm256_subs_epi16(ymm4, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm4);

    ymm0 = _mm256_srai_epi16(ymm0, 6);
    ymm1 = _mm256_srai_epi16(ymm1, 6);
    ymm2 = _mm256_srai_epi16(ymm2, 6);

    ymm0 = _mm256_packus_epi16(ymm0, ymm0);
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);

    ymm0 = _mm256_unpacklo_epi8(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm_alpha);
    ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

    __m256i argb0 = _mm256_unpacklo_epi16(ymm0, ymm2);
    __m256i argb1 = _mm256_unpackhi_epi16(ymm0, ymm2);

    _mm256_storeu_si256((__m256i*)dst_argb, argb0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), argb1);

    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV21TOARGBROW_AVX2

#if defined(HAS_I422TORGB24ROW_AVX2) ||     \
    defined(HAS_I422TORGB24ROW_AVX512BW) || \
    defined(HAS_ARGBTORGB24ROW_AVX2) ||     \
    defined(HAS_ARGBTORGB24ROW_AVX512BW)
static const uint8_t kShuffleMaskARGBToRGB24[2][16] = {
    {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u, 128u, 128u, 128u,
     128u},
    {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 128u, 128u, 128u, 128u, 10u, 12u, 13u,
     14u}};
#endif

#if defined(HAS_I422TORGB24ROW_AVX2)
LIBYUV_TARGET_AVX2
void I422ToRGB24Row_AVX2(const uint8_t* src_y,
                         const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_rgb24,
                         const struct YuvConstants* yuvconstants,
                         int width) {
  // Constants
  __m256i ymm_kUVToB = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  __m256i ymm_kUVToG = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  __m256i ymm_kUVToR = _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  __m256i ymm_kYBiasToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb);
  __m256i ymm_128 = _mm256_set1_epi8((char)0x80);

  __m256i ymm_shuf0 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShuffleMaskARGBToRGB24[1]));
  __m256i ymm_shuf1 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShuffleMaskARGBToRGB24[0]));
  __m256i ymm_u_zero = _mm256_setzero_si256();

  ptrdiff_t offset = src_v - src_u;

  while (width >= 16) {
    // READYUV422_AVX2
    __m128i xmm_u = _mm_loadl_epi64((const __m128i*)src_u);
    __m128i xmm_v = _mm_loadl_epi64((const __m128i*)(src_u + offset));
    src_u += 8;

    __m256i ymm3 = _mm256_insertf128_si256(ymm_u_zero, xmm_u, 0);
    __m256i ymm1 = _mm256_insertf128_si256(ymm_u_zero, xmm_v, 0);

    ymm3 = _mm256_unpacklo_epi8(ymm3, ymm1);
    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm3);

    __m128i xmm_y = _mm_loadu_si128((const __m128i*)src_y);
    src_y += 16;
    __m256i ymm4 = _mm256_insertf128_si256(ymm_u_zero, xmm_y, 0);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    ymm4 = _mm256_unpacklo_epi8(ymm4, ymm4);

    // YUVTORGB_AVX2
    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm1 = _mm256_subs_epi16(ymm4, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm4);

    ymm0 = _mm256_srai_epi16(ymm0, 6);
    ymm1 = _mm256_srai_epi16(ymm1, 6);
    ymm2 = _mm256_srai_epi16(ymm2, 6);

    ymm0 = _mm256_packus_epi16(ymm0, ymm0);
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);

    // STORERGB24_AVX2
    __m256i ymm0_packed = _mm256_unpacklo_epi8(ymm0, ymm1);
    __m256i ymm2_packed = _mm256_unpacklo_epi8(ymm2, ymm2);
    __m256i ymm1_packed = ymm0_packed;

    ymm0_packed = _mm256_unpacklo_epi16(ymm0_packed, ymm2_packed);
    ymm1_packed = _mm256_unpackhi_epi16(ymm1_packed, ymm2_packed);

    ymm0_packed = _mm256_shuffle_epi8(ymm0_packed, ymm_shuf0);
    ymm1_packed = _mm256_shuffle_epi8(ymm1_packed, ymm_shuf1);

    ymm1_packed = _mm256_alignr_epi8(ymm1_packed, ymm0_packed, 0xc);

    __m128i xmm0_store = _mm256_castsi256_si128(ymm0_packed);
    __m128i xmm1_store = _mm256_castsi256_si128(ymm1_packed);
    __m128i xmm2_store = _mm256_extractf128_si256(ymm0_packed, 1);
    __m128i xmm3_store = _mm256_extractf128_si256(ymm1_packed, 1);

    xmm0_store = _mm_unpacklo_epi64(xmm0_store, xmm1_store);
    xmm2_store = _mm_alignr_epi8(xmm2_store, xmm1_store, 8);

    _mm_storeu_si128((__m128i*)dst_rgb24, xmm0_store);
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 16), xmm2_store);
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 32), xmm3_store);

    dst_rgb24 += 48;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_I422TORGB24ROW_AVX2) ||
        // defined(HAS_I422TORGB24ROW_AVX512BW)

#if defined(HAS_I422TORGB24ROW_AVX512VBMI) || \
    defined(HAS_I422TORGB24ROW_AVX512BW)
#define READYUV422_AVX512BW                                          \
  __m128i xmm_u = _mm_loadu_si128((const __m128i*)src_u);            \
  __m128i xmm_v = _mm_loadu_si128((const __m128i*)(src_u + offset)); \
  src_u += 16;                                                       \
  __m512i zmm_u_val = _mm512_castsi128_si512(xmm_u);                 \
  __m512i zmm_v_val = _mm512_castsi128_si512(xmm_v);                 \
  zmm_u_val = PermutexvarEpi64(zmm_split, zmm_u_val);                \
  zmm_v_val = PermutexvarEpi64(zmm_split, zmm_v_val);                \
  __m512i zmm3 = _mm512_unpacklo_epi8(zmm_u_val, zmm_v_val);         \
  zmm3 = _mm512_permutex_epi64(zmm3, 0xd8);                          \
  zmm3 = _mm512_unpacklo_epi16(zmm3, zmm3);                          \
  __m256i ymm_y = _mm256_loadu_si256((const __m256i*)src_y);         \
  src_y += 32;                                                       \
  __m512i zmm4 = _mm512_castsi256_si512(ymm_y);                      \
  zmm4 = PermutexvarEpi64(zmm_split_y, zmm4);                        \
  zmm4 = _mm512_permutex_epi64(zmm4, 0xd8);                          \
  zmm4 = _mm512_unpacklo_epi8(zmm4, zmm4);

#define READYUV422_MASK_AVX512BW                                    \
  __mmask16 mask_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u); \
  __mmask32 mask_y = (__mmask32)((1u << width) - 1u);               \
  __m128i xmm_u = _mm_maskz_loadu_epi8(mask_uv, src_u);             \
  __m128i xmm_v = _mm_maskz_loadu_epi8(mask_uv, src_u + offset);    \
  __m512i zmm_u_val = _mm512_castsi128_si512(xmm_u);                \
  __m512i zmm_v_val = _mm512_castsi128_si512(xmm_v);                \
  zmm_u_val = PermutexvarEpi64(zmm_split, zmm_u_val);               \
  zmm_v_val = PermutexvarEpi64(zmm_split, zmm_v_val);               \
  __m512i zmm3 = _mm512_unpacklo_epi8(zmm_u_val, zmm_v_val);        \
  zmm3 = _mm512_permutex_epi64(zmm3, 0xd8);                         \
  zmm3 = _mm512_unpacklo_epi16(zmm3, zmm3);                         \
  __m256i ymm_y = _mm256_maskz_loadu_epi8(mask_y, src_y);           \
  __m512i zmm4 = _mm512_castsi256_si512(ymm_y);                     \
  zmm4 = PermutexvarEpi64(zmm_split_y, zmm4);                       \
  zmm4 = _mm512_permutex_epi64(zmm4, 0xd8);                         \
  zmm4 = _mm512_unpacklo_epi8(zmm4, zmm4);

#define YUVTORGB_AVX512BW                                \
  zmm3 = _mm512_sub_epi8(zmm3, zmm_128);                 \
  zmm4 = _mm512_mulhi_epu16(zmm4, zmm_kYToRgb);          \
  __m512i zmm0 = _mm512_maddubs_epi16(zmm_kUVToB, zmm3); \
  __m512i zmm1 = _mm512_maddubs_epi16(zmm_kUVToG, zmm3); \
  __m512i zmm2 = _mm512_maddubs_epi16(zmm_kUVToR, zmm3); \
  zmm4 = _mm512_add_epi16(zmm4, zmm_kYBiasToRgb);        \
  zmm0 = _mm512_adds_epi16(zmm0, zmm4);                  \
  zmm1 = _mm512_subs_epi16(zmm4, zmm1);                  \
  zmm2 = _mm512_adds_epi16(zmm2, zmm4);                  \
  zmm0 = _mm512_srai_epi16(zmm0, 6);                     \
  zmm1 = _mm512_srai_epi16(zmm1, 6);                     \
  zmm2 = _mm512_srai_epi16(zmm2, 6);                     \
  zmm0 = _mm512_packus_epi16(zmm0, zmm0);                \
  zmm1 = _mm512_packus_epi16(zmm1, zmm1);                \
  zmm2 = _mm512_packus_epi16(zmm2, zmm2);

#define STORERGB24_AVX512VBMI                                               \
  __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);       \
  __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2); \
  __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2); \
  _mm512_storeu_si512((__m512i*)dst_rgb24, zmm_dst0);                       \
  _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64),                           \
                      _mm512_castsi512_si256(zmm_dst1));                    \
  dst_rgb24 += 96;

#define STORERGB24_MASK_AVX512VBMI                                            \
  int dst_bytes = width * 3;                                                  \
  __mmask64 mask_dst0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull; \
  __mmask32 mask_dst1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;  \
  __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);         \
  __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);   \
  __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);   \
  _mm512_mask_storeu_epi8(dst_rgb24, mask_dst0, zmm_dst0);                    \
  _mm256_mask_storeu_epi8(dst_rgb24 + 64, mask_dst1,                          \
                          _mm512_castsi512_si256(zmm_dst1));

#define STORERGB24_AVX512BW                                                  \
  __m512i zmm_bg = _mm512_unpacklo_epi8(zmm0, zmm1);                         \
  __m512i zmm_rr = _mm512_unpacklo_epi8(zmm2, zmm2);                         \
  __m512i zmm_lo = _mm512_unpacklo_epi16(zmm_bg, zmm_rr);                    \
  __m512i zmm_hi = _mm512_unpackhi_epi16(zmm_bg, zmm_rr);                    \
  zmm_lo = _mm512_shuffle_epi8(zmm_lo, zmm_shuf0);                           \
  zmm_hi = _mm512_shuffle_epi8(zmm_hi, zmm_shuf1);                           \
  zmm_hi = _mm512_alignr_epi8(zmm_hi, zmm_lo, 12);                           \
  __m512i zmm_dst0 = _mm512_permutex2var_epi64(zmm_lo, zmm_stitch0, zmm_hi); \
  __m512i zmm_dst1 = _mm512_permutex2var_epi64(zmm_lo, zmm_stitch1, zmm_hi); \
  _mm512_storeu_si512((__m512i*)dst_rgb24, zmm_dst0);                        \
  _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64),                            \
                      _mm512_castsi512_si256(zmm_dst1));                     \
  dst_rgb24 += 96;

#define STORERGB24_MASK_AVX512BW                                              \
  int dst_bytes = width * 3;                                                  \
  __mmask64 mask_dst0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull; \
  __mmask32 mask_dst1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;  \
  __m512i zmm_bg = _mm512_unpacklo_epi8(zmm0, zmm1);                          \
  __m512i zmm_rr = _mm512_unpacklo_epi8(zmm2, zmm2);                          \
  __m512i zmm_lo = _mm512_unpacklo_epi16(zmm_bg, zmm_rr);                     \
  __m512i zmm_hi = _mm512_unpackhi_epi16(zmm_bg, zmm_rr);                     \
  zmm_lo = _mm512_shuffle_epi8(zmm_lo, zmm_shuf0);                            \
  zmm_hi = _mm512_shuffle_epi8(zmm_hi, zmm_shuf1);                            \
  zmm_hi = _mm512_alignr_epi8(zmm_hi, zmm_lo, 12);                            \
  __m512i zmm_dst0 = _mm512_permutex2var_epi64(zmm_lo, zmm_stitch0, zmm_hi);  \
  __m512i zmm_dst1 = _mm512_permutex2var_epi64(zmm_lo, zmm_stitch1, zmm_hi);  \
  _mm512_mask_storeu_epi8(dst_rgb24, mask_dst0, zmm_dst0);                    \
  _mm256_mask_storeu_epi8(dst_rgb24 + 64, mask_dst1,                          \
                          _mm512_castsi512_si256(zmm_dst1));
#endif  // defined(HAS_I422TORGB24ROW_AVX512VBMI) ||
        // defined(HAS_I422TORGB24ROW_AVX512BW)

#ifdef HAS_I422TORGB24ROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void I422ToRGB24Row_AVX512VBMI(const uint8_t* src_y,
                               const uint8_t* src_u,
                               const uint8_t* src_v,
                               uint8_t* dst_rgb24,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  // Masks
  static const uint8_t kMaskBG[64] = {
      0x00, 0x40, 0x01, 0x41, 0x02, 0x42, 0x03, 0x43, 0x04, 0x44, 0x05,
      0x45, 0x06, 0x46, 0x07, 0x47, 0x10, 0x50, 0x11, 0x51, 0x12, 0x52,
      0x13, 0x53, 0x14, 0x54, 0x15, 0x55, 0x16, 0x56, 0x17, 0x57, 0x20,
      0x60, 0x21, 0x61, 0x22, 0x62, 0x23, 0x63, 0x24, 0x64, 0x25, 0x65,
      0x26, 0x66, 0x27, 0x67, 0x30, 0x70, 0x31, 0x71, 0x32, 0x72, 0x33,
      0x73, 0x34, 0x74, 0x35, 0x75, 0x36, 0x76, 0x37, 0x77};
  static const uint8_t kMaskDST0[64] = {
      0x00, 0x01, 0x40, 0x02, 0x03, 0x41, 0x04, 0x05, 0x42, 0x06, 0x07,
      0x43, 0x08, 0x09, 0x44, 0x0a, 0x0b, 0x45, 0x0c, 0x0d, 0x46, 0x0e,
      0x0f, 0x47, 0x10, 0x11, 0x50, 0x12, 0x13, 0x51, 0x14, 0x15, 0x52,
      0x16, 0x17, 0x53, 0x18, 0x19, 0x54, 0x1a, 0x1b, 0x55, 0x1c, 0x1d,
      0x56, 0x1e, 0x1f, 0x57, 0x20, 0x21, 0x60, 0x22, 0x23, 0x61, 0x24,
      0x25, 0x62, 0x26, 0x27, 0x63, 0x28, 0x29, 0x64, 0x2a};
  static const uint8_t kMaskDST1[64] = {
      0x2b, 0x65, 0x2c, 0x2d, 0x66, 0x2e, 0x2f, 0x67, 0x30, 0x31, 0x70,
      0x32, 0x33, 0x71, 0x34, 0x35, 0x72, 0x36, 0x37, 0x73, 0x38, 0x39,
      0x74, 0x3a, 0x3b, 0x75, 0x3c, 0x3d, 0x76, 0x3e, 0x3f, 0x77, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  static const uint64_t kSplitQuadWords[8] = {0, 2, 2, 2, 1, 2, 2, 2};
  static const uint64_t kSplitDoubleQuadWords[8] = {0, 1, 4, 4, 2, 3, 4, 4};

  // Constants
  __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  __m512i zmm_128 = _mm512_set1_epi8((char)0x80);

  __m512i zmm_mask_BG = _mm512_loadu_si512((const __m512i*)kMaskBG);
  __m512i zmm_mask_DST0 = _mm512_loadu_si512((const __m512i*)kMaskDST0);
  __m512i zmm_mask_DST1 = _mm512_loadu_si512((const __m512i*)kMaskDST1);
  __m512i zmm_split = _mm512_loadu_si512((const __m512i*)kSplitQuadWords);
  __m512i zmm_split_y =
      _mm512_loadu_si512((const __m512i*)kSplitDoubleQuadWords);

  ptrdiff_t offset = src_v - src_u;

  while (width >= 32) {
    READYUV422_AVX512BW
    YUVTORGB_AVX512BW
    STORERGB24_AVX512VBMI
    width -= 32;
  }
  if (width > 0) {
    READYUV422_MASK_AVX512BW
    YUVTORGB_AVX512BW
    STORERGB24_MASK_AVX512VBMI
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TORGB24ROW_AVX512VBMI

#ifdef HAS_I422TORGB24ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I422ToRGB24Row_AVX512BW(const uint8_t* src_y,
                             const uint8_t* src_u,
                             const uint8_t* src_v,
                             uint8_t* dst_rgb24,
                             const struct YuvConstants* yuvconstants,
                             int width) {
  static const uint64_t kStitchRGB24_0[8] = {0, 8, 9, 2, 10, 11, 4, 12};
  static const uint64_t kStitchRGB24_1[8] = {13, 6, 14, 15, 0, 0, 0, 0};
  static const uint64_t kSplitQuadWords[8] = {0, 2, 2, 2, 1, 2, 2, 2};
  static const uint64_t kSplitDoubleQuadWords[8] = {0, 1, 4, 4, 2, 3, 4, 4};

  // Constants
  __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  __m512i zmm_128 = _mm512_set1_epi8((char)0x80);

  __m512i zmm_shuf0 = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleMaskARGBToRGB24[1]));
  __m512i zmm_shuf1 = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleMaskARGBToRGB24[0]));
  __m512i zmm_stitch0 = _mm512_loadu_si512((const __m512i*)kStitchRGB24_0);
  __m512i zmm_stitch1 = _mm512_loadu_si512((const __m512i*)kStitchRGB24_1);
  __m512i zmm_split = _mm512_loadu_si512((const __m512i*)kSplitQuadWords);
  __m512i zmm_split_y =
      _mm512_loadu_si512((const __m512i*)kSplitDoubleQuadWords);

  ptrdiff_t offset = src_v - src_u;

  while (width >= 32) {
    READYUV422_AVX512BW
    YUVTORGB_AVX512BW
    STORERGB24_AVX512BW
    width -= 32;
  }
  if (width > 0) {
    READYUV422_MASK_AVX512BW
    YUVTORGB_AVX512BW
    STORERGB24_MASK_AVX512BW
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TORGB24ROW_AVX512BW

#if defined(HAS_I422TORGB24ROW_AVX512VBMI) || \
    defined(HAS_I422TORGB24ROW_AVX512BW)
#undef READYUV422_AVX512BW
#undef READYUV422_MASK_AVX512BW
#undef YUVTORGB_AVX512BW
#undef STORERGB24_AVX512VBMI
#undef STORERGB24_MASK_AVX512VBMI
#undef STORERGB24_AVX512BW
#undef STORERGB24_MASK_AVX512BW
#endif

#ifdef HAS_I422TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I422ToAR30Row_AVX512BW(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  static const uint64_t kSplitQuadWords[8] = {0, 2, 2, 2, 1, 2, 2, 2};
  static const uint64_t kSplitDoubleQuadWords[8] = {0, 1, 4, 4, 2, 3, 4, 4};
  static const uint16_t kPermAR30_0[32] = {
      0, 32, 1, 33, 2,  34, 3,  35, 4,  36, 5,  37, 6,  38, 7,  39,
      8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47};
  static const uint16_t kPermAR30_1[32] = {
      16, 48, 17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55,
      24, 56, 25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63};

  // Constants
  __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  __m512i zmm_kYBiasToRgb = _mm512_sub_epi16(
      _mm512_broadcast_i32x4(
          _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb)),
      _mm512_set1_epi16(24));
  __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  __m512i zmm_3ff0 = _mm512_set1_epi16((short)0x3ff0);
  __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  __m512i zmm_zero = _mm512_setzero_si512();

  __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0);
  __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1);
  __m512i zmm_split = _mm512_loadu_si512((const __m512i*)kSplitQuadWords);
  __m512i zmm_split_y =
      _mm512_loadu_si512((const __m512i*)kSplitDoubleQuadWords);

  ptrdiff_t offset = src_v - src_u;

  while (width >= 32) {
    // READYUV422_AVX512BW
    __m128i xmm_u = _mm_loadu_si128((const __m128i*)src_u);
    __m128i xmm_v = _mm_loadu_si128((const __m128i*)(src_u + offset));
    src_u += 16;

    __m512i zmm_u_val = _mm512_castsi128_si512(xmm_u);
    __m512i zmm_v_val = _mm512_castsi128_si512(xmm_v);

    zmm_u_val = PermutexvarEpi64(zmm_split, zmm_u_val);
    zmm_v_val = PermutexvarEpi64(zmm_split, zmm_v_val);

    __m512i zmm3 = _mm512_unpacklo_epi8(zmm_u_val, zmm_v_val);
    zmm3 = _mm512_permutex_epi64(zmm3, 0xd8);
    zmm3 = _mm512_unpacklo_epi16(zmm3, zmm3);

    __m256i ymm_y = _mm256_loadu_si256((const __m256i*)src_y);
    src_y += 32;
    __m512i zmm4 = _mm512_castsi256_si512(ymm_y);
    zmm4 = PermutexvarEpi64(zmm_split_y, zmm4);
    zmm4 = _mm512_permutex_epi64(zmm4, 0xd8);
    zmm4 = _mm512_unpacklo_epi8(zmm4, zmm4);

    // YUVTORGB_AVX512BW
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_mulhi_epu16(zmm4, zmm_kYToRgb);

    __m512i zmm0 = _mm512_maddubs_epi16(zmm_kUVToB, zmm3);
    __m512i zmm1 = _mm512_maddubs_epi16(zmm_kUVToG, zmm3);
    __m512i zmm2 = _mm512_maddubs_epi16(zmm_kUVToR, zmm3);

    zmm4 = _mm512_add_epi16(zmm4, zmm_kYBiasToRgb);

    zmm0 = _mm512_adds_epi16(zmm0, zmm4);
    zmm1 = _mm512_subs_epi16(zmm4, zmm1);
    zmm2 = _mm512_adds_epi16(zmm2, zmm4);

    // Clamp to [0, 0x3ff0] (1023 * 16)
    zmm0 = _mm512_min_epi16(zmm0, zmm_3ff0);
    zmm1 = _mm512_min_epi16(zmm1, zmm_3ff0);
    zmm2 = _mm512_min_epi16(zmm2, zmm_3ff0);
    zmm0 = _mm512_max_epi16(zmm0, zmm_zero);
    zmm1 = _mm512_max_epi16(zmm1, zmm_zero);
    zmm2 = _mm512_max_epi16(zmm2, zmm_zero);

    // STOREAR30_AVX512BW:
    zmm0 = _mm512_srli_epi16(zmm0, 4);
    zmm3 = _mm512_srli_epi16(zmm1, 10);
    zmm1 = _mm512_slli_epi16(zmm1, 6);
    zmm0 = _mm512_or_si512(zmm0, _mm512_and_si512(zmm1, zmm_fc00));
    zmm2 = _mm512_or_si512(_mm512_and_si512(zmm2, zmm_3ff0),
                           _mm512_andnot_si512(zmm_3ff0, zmm3));
    zmm2 = _mm512_or_si512(zmm2, zmm_c000);

    __m512i zmm_dst0 = _mm512_permutex2var_epi16(zmm0, zmm_perm0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi16(zmm0, zmm_perm1, zmm2);

    _mm512_storeu_si512((__m512i*)dst_ar30, zmm_dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), zmm_dst1);

    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    __mmask16 mask_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __mmask32 mask_y = (__mmask32)((1u << width) - 1u);
    __m128i xmm_u = _mm_maskz_loadu_epi8(mask_uv, src_u);
    __m128i xmm_v = _mm_maskz_loadu_epi8(mask_uv, src_u + offset);

    __m512i zmm_u_val = _mm512_castsi128_si512(xmm_u);
    __m512i zmm_v_val = _mm512_castsi128_si512(xmm_v);

    zmm_u_val = PermutexvarEpi64(zmm_split, zmm_u_val);
    zmm_v_val = PermutexvarEpi64(zmm_split, zmm_v_val);

    __m512i zmm3 = _mm512_unpacklo_epi8(zmm_u_val, zmm_v_val);
    zmm3 = _mm512_permutex_epi64(zmm3, 0xd8);
    zmm3 = _mm512_unpacklo_epi16(zmm3, zmm3);

    __m256i ymm_y = _mm256_maskz_loadu_epi8(mask_y, src_y);
    __m512i zmm4 = _mm512_castsi256_si512(ymm_y);
    zmm4 = PermutexvarEpi64(zmm_split_y, zmm4);
    zmm4 = _mm512_permutex_epi64(zmm4, 0xd8);
    zmm4 = _mm512_unpacklo_epi8(zmm4, zmm4);

    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_mulhi_epu16(zmm4, zmm_kYToRgb);

    __m512i zmm0 = _mm512_maddubs_epi16(zmm_kUVToB, zmm3);
    __m512i zmm1 = _mm512_maddubs_epi16(zmm_kUVToG, zmm3);
    __m512i zmm2 = _mm512_maddubs_epi16(zmm_kUVToR, zmm3);

    zmm4 = _mm512_add_epi16(zmm4, zmm_kYBiasToRgb);

    zmm0 = _mm512_adds_epi16(zmm0, zmm4);
    zmm1 = _mm512_subs_epi16(zmm4, zmm1);
    zmm2 = _mm512_adds_epi16(zmm2, zmm4);

    zmm0 = _mm512_min_epi16(zmm0, zmm_3ff0);
    zmm1 = _mm512_min_epi16(zmm1, zmm_3ff0);
    zmm2 = _mm512_min_epi16(zmm2, zmm_3ff0);
    zmm0 = _mm512_max_epi16(zmm0, zmm_zero);
    zmm1 = _mm512_max_epi16(zmm1, zmm_zero);
    zmm2 = _mm512_max_epi16(zmm2, zmm_zero);

    zmm0 = _mm512_srli_epi16(zmm0, 4);
    zmm3 = _mm512_srli_epi16(zmm1, 10);
    zmm1 = _mm512_slli_epi16(zmm1, 6);
    zmm0 = _mm512_or_si512(zmm0, _mm512_and_si512(zmm1, zmm_fc00));
    zmm2 = _mm512_or_si512(_mm512_and_si512(zmm2, zmm_3ff0),
                           _mm512_andnot_si512(zmm_3ff0, zmm3));
    zmm2 = _mm512_or_si512(zmm2, zmm_c000);

    __m512i zmm_dst0 = _mm512_permutex2var_epi16(zmm0, zmm_perm0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi16(zmm0, zmm_perm1, zmm2);

    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)mask_y, zmm_dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(mask_y >> 16),
                             zmm_dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TOAR30ROW_AVX512BW

#ifdef HAS_ARGBBLENDROW_AVX2
static const uint8_t kShuffleAlpha[16] = {3u,  0x80, 3u,  0x80, 7u,  0x80,
                                          7u,  0x80, 11u, 0x80, 11u, 0x80,
                                          15u, 0x80, 15u, 0x80};

LIBYUV_TARGET_AVX2
void ARGBBlendRow_AVX2(const uint8_t* src_argb0,
                       const uint8_t* src_argb1,
                       uint8_t* dst_argb,
                       int width) {
  __m256i shuff =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)kShuffleAlpha));
  __m256i ymm_1 = _mm256_set1_epi16(1);
  __m256i mask_lo = _mm256_set1_epi16(0x00ff);
  __m256i mask_hi = _mm256_set1_epi16((short)0xff00);
  __m256i mask_alpha = _mm256_set1_epi32((int)0xff000000);

  while (width >= 8) {
    __m256i ymm_s0 = _mm256_loadu_si256((const __m256i*)src_argb0);
    __m256i ymm_s1 = _mm256_loadu_si256((const __m256i*)src_argb1);
    src_argb0 += 32;
    src_argb1 += 32;

    __m256i ymm_a = _mm256_xor_si256(ymm_s0, mask_alpha);
    ymm_a = _mm256_shuffle_epi8(ymm_a, shuff);
    ymm_a = _mm256_add_epi16(ymm_a, ymm_1);

    __m256i ymm_s1_lo = _mm256_and_si256(ymm_s1, mask_lo);
    __m256i ymm_s1_hi = _mm256_srli_epi16(ymm_s1, 8);

    ymm_s1_lo = _mm256_mullo_epi16(ymm_s1_lo, ymm_a);
    ymm_s1_hi = _mm256_mullo_epi16(ymm_s1_hi, ymm_a);

    ymm_s1_lo = _mm256_srli_epi16(ymm_s1_lo, 8);
    ymm_s1_hi = _mm256_and_si256(ymm_s1_hi, mask_hi);

    __m256i res = _mm256_or_si256(ymm_s0, mask_alpha);
    res = _mm256_adds_epu8(res, ymm_s1_lo);
    res = _mm256_adds_epu8(res, ymm_s1_hi);

    _mm256_storeu_si256((__m256i*)dst_argb, res);
    dst_argb += 32;
    width -= 8;
  }
  _mm256_zeroupper();

  while (width > 0) {
    uint32_t s0 = *(const uint32_t*)src_argb0;
    uint32_t s1 = *(const uint32_t*)src_argb1;
    src_argb0 += 4;
    src_argb1 += 4;

    uint32_t a = 256 - (s0 >> 24);
    uint32_t b = (((s1 & 0x000000ff) * a) >> 8) + (s0 & 0x000000ff);
    uint32_t g = (((s1 & 0x0000ff00) * a) >> 8) + (s0 & 0x0000ff00);
    uint32_t r = (((s1 & 0x00ff0000) >> 8) * a) + (s0 & 0x00ff0000);

    b = (b > 255) ? 255 : b;
    g = (g > 0xff00) ? 0xff00 : (g & 0xff00);
    r = (r > 0x00ff0000) ? 0x00ff0000 : (r & 0x00ff0000);

    *(uint32_t*)dst_argb = 0xff000000 | r | g | b;
    dst_argb += 4;
    --width;
  }
}
#endif  // HAS_ARGBBLENDROW_AVX2

#ifdef HAS_BLENDPLANEROW_SSSE3
void BlendPlaneRow_SSSE3(const uint8_t* src0,
                         const uint8_t* src1,
                         const uint8_t* alpha,
                         uint8_t* dst,
                         int width) {
  __m128i xmm_ff00 = _mm_set1_epi16((short)0xff00);
  __m128i xmm_80 = _mm_set1_epi8((char)0x80);
  __m128i xmm_round = _mm_set1_epi16((short)0x807f);

  while (width > 0) {
    __m128i xmm_a = _mm_loadl_epi64((const __m128i*)alpha);
    __m128i xmm_s0 = _mm_loadl_epi64((const __m128i*)src0);
    __m128i xmm_s1 = _mm_loadl_epi64((const __m128i*)src1);
    alpha += 8;
    src0 += 8;
    src1 += 8;

    xmm_a = _mm_unpacklo_epi8(xmm_a, xmm_a);
    xmm_a = _mm_xor_si128(xmm_a, xmm_ff00);

    __m128i xmm_s = _mm_unpacklo_epi8(xmm_s0, xmm_s1);
    xmm_s = _mm_sub_epi8(xmm_s, xmm_80);

    __m128i xmm_p = _mm_maddubs_epi16(xmm_a, xmm_s);
    xmm_p = _mm_add_epi16(xmm_p, xmm_round);
    xmm_p = _mm_srli_epi16(xmm_p, 8);

    __m128i xmm_d = _mm_packus_epi16(xmm_p, xmm_p);
    _mm_storel_epi64((__m128i*)dst, xmm_d);
    dst += 8;
    width -= 8;
  }
}
#endif  // HAS_BLENDPLANEROW_SSSE3

#ifdef HAS_BLENDPLANEROW_AVX2
LIBYUV_TARGET_AVX2
void BlendPlaneRow_AVX2(const uint8_t* src0,
                        const uint8_t* src1,
                        const uint8_t* alpha,
                        uint8_t* dst,
                        int width) {
  __m256i ymm_ff00 = _mm256_set1_epi16((short)0xff00);
  __m256i ymm_80 = _mm256_set1_epi8((char)0x80);
  __m256i ymm_round = _mm256_set1_epi16((short)0x807f);

  while (width > 0) {
    __m256i ymm_a = _mm256_loadu_si256((const __m256i*)alpha);
    __m256i ymm_s0 = _mm256_loadu_si256((const __m256i*)src0);
    __m256i ymm_s1 = _mm256_loadu_si256((const __m256i*)src1);
    alpha += 32;
    src0 += 32;
    src1 += 32;

    __m256i ymm_a_lo = _mm256_unpacklo_epi8(ymm_a, ymm_a);
    __m256i ymm_a_hi = _mm256_unpackhi_epi8(ymm_a, ymm_a);
    ymm_a_lo = _mm256_xor_si256(ymm_a_lo, ymm_ff00);
    ymm_a_hi = _mm256_xor_si256(ymm_a_hi, ymm_ff00);

    __m256i ymm_s_lo = _mm256_unpacklo_epi8(ymm_s0, ymm_s1);
    __m256i ymm_s_hi = _mm256_unpackhi_epi8(ymm_s0, ymm_s1);
    ymm_s_lo = _mm256_sub_epi8(ymm_s_lo, ymm_80);
    ymm_s_hi = _mm256_sub_epi8(ymm_s_hi, ymm_80);

    __m256i ymm_p_lo = _mm256_maddubs_epi16(ymm_a_lo, ymm_s_lo);
    __m256i ymm_p_hi = _mm256_maddubs_epi16(ymm_a_hi, ymm_s_hi);

    ymm_p_lo = _mm256_add_epi16(ymm_p_lo, ymm_round);
    ymm_p_hi = _mm256_add_epi16(ymm_p_hi, ymm_round);

    ymm_p_lo = _mm256_srli_epi16(ymm_p_lo, 8);
    ymm_p_hi = _mm256_srli_epi16(ymm_p_hi, 8);

    __m256i ymm_d = _mm256_packus_epi16(ymm_p_lo, ymm_p_hi);
    _mm256_storeu_si256((__m256i*)dst, ymm_d);
    dst += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_BLENDPLANEROW_AVX2

#ifdef HAS_BLENDPLANEROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void BlendPlaneRow_AVX512BW(const uint8_t* src0,
                            const uint8_t* src1,
                            const uint8_t* alpha,
                            uint8_t* dst,
                            int width) {
  __m512i zmm_ff00 = _mm512_set1_epi16((short)0xff00);
  __m512i zmm_80 = _mm512_set1_epi8((char)0x80);
  __m512i zmm_round = _mm512_set1_epi16((short)0x807f);

  while (width >= 64) {
    __m512i zmm_a = _mm512_loadu_si512((const __m512i*)alpha);
    __m512i zmm_s0 = _mm512_loadu_si512((const __m512i*)src0);
    __m512i zmm_s1 = _mm512_loadu_si512((const __m512i*)src1);
    alpha += 64;
    src0 += 64;
    src1 += 64;

    __m512i zmm_a_lo = _mm512_unpacklo_epi8(zmm_a, zmm_a);
    __m512i zmm_a_hi = _mm512_unpackhi_epi8(zmm_a, zmm_a);
    zmm_a_lo = _mm512_xor_si512(zmm_a_lo, zmm_ff00);
    zmm_a_hi = _mm512_xor_si512(zmm_a_hi, zmm_ff00);

    __m512i zmm_s_lo = _mm512_unpacklo_epi8(zmm_s0, zmm_s1);
    __m512i zmm_s_hi = _mm512_unpackhi_epi8(zmm_s0, zmm_s1);
    zmm_s_lo = _mm512_sub_epi8(zmm_s_lo, zmm_80);
    zmm_s_hi = _mm512_sub_epi8(zmm_s_hi, zmm_80);

    __m512i zmm_p_lo = _mm512_maddubs_epi16(zmm_a_lo, zmm_s_lo);
    __m512i zmm_p_hi = _mm512_maddubs_epi16(zmm_a_hi, zmm_s_hi);

    zmm_p_lo = _mm512_add_epi16(zmm_p_lo, zmm_round);
    zmm_p_hi = _mm512_add_epi16(zmm_p_hi, zmm_round);

    zmm_p_lo = _mm512_srli_epi16(zmm_p_lo, 8);
    zmm_p_hi = _mm512_srli_epi16(zmm_p_hi, 8);

    __m512i zmm_d = _mm512_packus_epi16(zmm_p_lo, zmm_p_hi);
    _mm512_storeu_si512((__m512i*)dst, zmm_d);
    dst += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 mask = (1ULL << width) - 1ULL;
    __m512i zmm_a = _mm512_maskz_loadu_epi8(mask, alpha);
    __m512i zmm_s0 = _mm512_maskz_loadu_epi8(mask, src0);
    __m512i zmm_s1 = _mm512_maskz_loadu_epi8(mask, src1);

    __m512i zmm_a_lo = _mm512_unpacklo_epi8(zmm_a, zmm_a);
    __m512i zmm_a_hi = _mm512_unpackhi_epi8(zmm_a, zmm_a);
    zmm_a_lo = _mm512_xor_si512(zmm_a_lo, zmm_ff00);
    zmm_a_hi = _mm512_xor_si512(zmm_a_hi, zmm_ff00);

    __m512i zmm_s_lo = _mm512_unpacklo_epi8(zmm_s0, zmm_s1);
    __m512i zmm_s_hi = _mm512_unpackhi_epi8(zmm_s0, zmm_s1);
    zmm_s_lo = _mm512_sub_epi8(zmm_s_lo, zmm_80);
    zmm_s_hi = _mm512_sub_epi8(zmm_s_hi, zmm_80);

    __m512i zmm_p_lo = _mm512_maddubs_epi16(zmm_a_lo, zmm_s_lo);
    __m512i zmm_p_hi = _mm512_maddubs_epi16(zmm_a_hi, zmm_s_hi);

    zmm_p_lo = _mm512_add_epi16(zmm_p_lo, zmm_round);
    zmm_p_hi = _mm512_add_epi16(zmm_p_hi, zmm_round);

    zmm_p_lo = _mm512_srli_epi16(zmm_p_lo, 8);
    zmm_p_hi = _mm512_srli_epi16(zmm_p_hi, 8);

    __m512i zmm_d = _mm512_packus_epi16(zmm_p_lo, zmm_p_hi);
    _mm512_mask_storeu_epi8(dst, mask, zmm_d);
  }
  _mm256_zeroupper();
}
#endif  // HAS_BLENDPLANEROW_AVX512BW

#ifdef HAS_ARGBEXTRACTALPHAROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBExtractAlphaRow_AVX512BW(const uint8_t* src_argb,
                                  uint8_t* dst_a,
                                  int width) {
  while (width >= 32) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i z1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m128i a0 = _mm512_cvtepi32_epi8(_mm512_srli_epi32(z0, 24));
    __m128i a1 = _mm512_cvtepi32_epi8(_mm512_srli_epi32(z1, 24));
    _mm_storeu_si128((__m128i*)dst_a, a0);
    _mm_storeu_si128((__m128i*)(dst_a + 16), a1);
    src_argb += 128;
    dst_a += 32;
    width -= 32;
  }
  if (width >= 16) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m128i a0 = _mm512_cvtepi32_epi8(_mm512_srli_epi32(z0, 24));
    _mm_storeu_si128((__m128i*)dst_a, a0);
    src_argb += 64;
    dst_a += 16;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 mask = (__mmask16)((1u << width) - 1u);
    __m512i z0 = _mm512_maskz_loadu_epi32(mask, src_argb);
    __m128i a0 = _mm512_cvtepi32_epi8(_mm512_srli_epi32(z0, 24));
    // Use _mm_mask_storeu_epi8 instead of _mm512_mask_cvtepi32_storeu_epi8
    // because LLVM MSan does not instrument @llvm.x86.avx512.mask.pmov.db.mem
    // to unpoison destination shadow memory.
    _mm_mask_storeu_epi8(dst_a, mask, a0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBEXTRACTALPHAROW_AVX512BW

#ifdef HAS_ARGBCOPYYTOALPHAROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBCopyYToAlphaRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  // Store only the alpha byte (every 4th byte) using a byte mask instead of
  // loading dst and blending with _mm512_ternarylogic_epi32. This avoids
  // reading uninitialized destination bytes and avoids
  // @llvm.x86.avx512.pternlog which LLVM MSan does not model bitwise.
  const __mmask64 alpha_mask = 0x8888888888888888ull;
  while (width >= 32) {
    __m512i a0 = _mm512_slli_epi32(
        _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)src)), 24);
    __m512i a1 = _mm512_slli_epi32(
        _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)(src + 16))), 24);
    _mm512_mask_storeu_epi8(dst, alpha_mask, a0);
    _mm512_mask_storeu_epi8(dst + 64, alpha_mask, a1);
    src += 32;
    dst += 128;
    width -= 32;
  }
  if (width >= 16) {
    __m512i a0 = _mm512_slli_epi32(
        _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)src)), 24);
    _mm512_mask_storeu_epi8(dst, alpha_mask, a0);
    src += 16;
    dst += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 mask = (__mmask16)((1u << width) - 1u);
    __mmask64 tail_alpha_mask = alpha_mask >> (64 - width * 4);
    __m512i a0 = _mm512_slli_epi32(
        _mm512_maskz_cvtepu8_epi32(mask, _mm_maskz_loadu_epi8(mask, src)), 24);
    _mm512_mask_storeu_epi8(dst, tail_alpha_mask, a0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBCOPYYTOALPHAROW_AVX512BW

#ifdef HAS_ARGBMIRRORROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBMirrorRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  const __m512i rev =
      _mm512_set_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  const uint8_t* s = src + (ptrdiff_t)width * 4;
  while (width >= 16) {
    s -= 64;
    _mm512_storeu_si512(
        (__m512i*)dst,
        PermutexvarEpi32(rev, _mm512_loadu_si512((const __m512i*)s)));
    dst += 64;
    width -= 16;
  }
  if (width > 0) {
    // Remaining 1 to 15 pixels are at the start of src.
    __mmask16 mask = (__mmask16)((1u << width) - 1u);
    __m512i idx = _mm512_sub_epi32(rev, _mm512_set1_epi32(16 - width));
    __m512i v = _mm512_maskz_loadu_epi32(mask, src);
    _mm512_mask_storeu_epi32(dst, mask, PermutexvarEpi32(idx, v));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBMIRRORROW_AVX512BW

#ifdef HAS_RGB24MIRRORROW_AVX512VBMI
static const uint8_t kPermMirrorRGB24_0[64] = {
    125u, 126u, 127u, 122u, 123u, 124u, 119u, 120u, 121u, 116u, 117u,
    118u, 113u, 114u, 115u, 110u, 111u, 112u, 107u, 108u, 109u, 104u,
    105u, 106u, 101u, 102u, 103u, 98u,  99u,  100u, 95u,  96u,  97u,
    92u,  93u,  94u,  89u,  90u,  91u,  86u,  87u,  88u,  83u,  84u,
    85u,  80u,  81u,  82u,  77u,  78u,  79u,  74u,  75u,  76u,  71u,
    72u,  73u,  68u,  69u,  70u,  65u,  66u,  67u,  62u};
static const uint8_t kPermMirrorRGB24_1[64] = {
    127u, 128u, 123u, 124u, 125u, 120u, 121u, 122u, 117u, 118u, 119u,
    114u, 115u, 116u, 111u, 112u, 113u, 108u, 109u, 110u, 105u, 106u,
    107u, 102u, 103u, 104u, 99u,  100u, 101u, 96u,  97u,  98u,  93u,
    94u,  95u,  90u,  91u,  92u,  87u,  88u,  89u,  84u,  85u,  86u,
    81u,  82u,  83u,  78u,  79u,  80u,  75u,  76u,  77u,  72u,  73u,
    74u,  69u,  70u,  71u,  66u,  67u,  68u,  63u,  64u};
static const uint8_t kPermMirrorRGB24_2[64] = {
    65u, 60u, 61u, 62u, 57u, 58u, 59u, 54u, 55u, 56u, 51u, 52u, 53u,
    48u, 49u, 50u, 45u, 46u, 47u, 42u, 43u, 44u, 39u, 40u, 41u, 36u,
    37u, 38u, 33u, 34u, 35u, 30u, 31u, 32u, 27u, 28u, 29u, 24u, 25u,
    26u, 21u, 22u, 23u, 18u, 19u, 20u, 15u, 16u, 17u, 12u, 13u, 14u,
    9u,  10u, 11u, 6u,  7u,  8u,  3u,  4u,  5u,  0u,  1u,  2u};

// Byte mask with the low n bits set, clamped to [0, 64].
static inline __mmask64 RGB24MirrorMask(int n) {
  return n <= 0 ? 0ull : n >= 64 ? ~0ull : (1ull << n) - 1ull;
}

// Mirror 64 RGB24 pixels (3 full zmm vectors) per loop.  The remaining 1 to
// 63 pixels use masked loads and stores that access only valid bytes.
LIBYUV_TARGET_AVX512VBMI
void RGB24MirrorRow_AVX512VBMI(const uint8_t* src_rgb24,
                               uint8_t* dst_rgb24,
                               int width) {
  const __m512i idx0 = _mm512_loadu_si512((const __m512i*)kPermMirrorRGB24_0);
  const __m512i idx1 = _mm512_loadu_si512((const __m512i*)kPermMirrorRGB24_1);
  const __m512i idx2 = _mm512_loadu_si512((const __m512i*)kPermMirrorRGB24_2);
  int r = width & 63;
  int w = width - r;
  if (w > 0) {
    const uint8_t* src = src_rgb24 + (ptrdiff_t)width * 3 - 192;
    do {
      __m512i a = _mm512_loadu_si512((const __m512i*)src);
      __m512i b = _mm512_loadu_si512((const __m512i*)(src + 64));
      __m512i c = _mm512_loadu_si512((const __m512i*)(src + 128));
      __m512i d0 = _mm512_permutex2var_epi8(b, idx0, c);
      __m512i d1 = _mm512_permutex2var_epi8(a, idx1, b);
      __m512i d2 = _mm512_permutex2var_epi8(a, idx2, b);
      d1 = _mm512_mask_broadcastb_epi8(d1, (__mmask64)2,
                                       _mm512_castsi512_si128(c));
      _mm512_storeu_si512((__m512i*)dst_rgb24, d0);
      _mm512_storeu_si512((__m512i*)(dst_rgb24 + 64), d1);
      _mm512_storeu_si512((__m512i*)(dst_rgb24 + 128), d2);
      src -= 192;
      dst_rgb24 += 192;
      w -= 64;
    } while (w > 0);
  }
  if (r) {
    int nb = r * 3;
    int off = 192 - nb;
    __mmask64 m0 = RGB24MirrorMask(nb);
    __mmask64 m1 = RGB24MirrorMask(nb - 64);
    __mmask64 m2 = RGB24MirrorMask(nb - 128);
    __m512i a = _mm512_maskz_loadu_epi8(m0, src_rgb24);
    __m512i b = _mm512_maskz_loadu_epi8(m1, src_rgb24 + 64);
    __m512i c = _mm512_maskz_loadu_epi8(m2, src_rgb24 + 128);
    __m512i t0 = _mm512_sub_epi8(idx0, _mm512_set1_epi8((char)(off - 64)));
    __m512i t1 = _mm512_sub_epi8(idx1, _mm512_set1_epi8((char)off));
    __m512i t2 = _mm512_sub_epi8(idx2, _mm512_set1_epi8((char)off));
    __m512i d0 = MaskPermutexvarEpi8(_mm512_permutex2var_epi8(a, t0, b),
                                     _mm512_movepi8_mask(t0), t0, c);
    __m512i d1 = MaskPermutexvarEpi8(_mm512_permutex2var_epi8(a, t1, b),
                                     _mm512_movepi8_mask(t1), t1, c);
    __m512i d2 = MaskPermutexvarEpi8(_mm512_permutex2var_epi8(a, t2, b),
                                     _mm512_movepi8_mask(t2), t2, c);
    _mm512_mask_storeu_epi8(dst_rgb24, m0, d0);
    _mm512_mask_storeu_epi8(dst_rgb24 + 64, m1, d1);
    _mm512_mask_storeu_epi8(dst_rgb24 + 128, m2, d2);
  }
  _mm256_zeroupper();
}
#endif  // HAS_RGB24MIRRORROW_AVX512VBMI

// ===========================================================================
// Module 1: AYUV to Planar Y / UV / VU (AVX2 / AVX512VBMI)
// ===========================================================================

#ifdef HAS_AYUVTOYROW_AVX2
LIBYUV_TARGET_AVX2
void AYUVToYRow_AVX2(const uint8_t* src_ayuv, uint8_t* dst_y, int width) {
  const __m256i ymm_mask = _mm256_set1_epi32(0x000000ff);
  while (width >= 16) {
    __m256i ymm0 = _mm256_and_si256(
        _mm256_srli_epi32(_mm256_loadu_si256((const __m256i*)src_ayuv), 16),
        ymm_mask);
    __m256i ymm1 = _mm256_and_si256(
        _mm256_srli_epi32(_mm256_loadu_si256((const __m256i*)(src_ayuv + 32)),
                          16),
        ymm_mask);
    src_ayuv += 64;
    ymm0 = _mm256_permute4x64_epi64(_mm256_packus_epi32(ymm0, ymm1), 0xd8);
    __m128i out = _mm_packus_epi16(_mm256_castsi256_si128(ymm0),
                                   _mm256_extracti128_si256(ymm0, 1));
    _mm_storeu_si128((__m128i*)dst_y, out);
    dst_y += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_AYUVTOYROW_AVX2

#if defined(HAS_AYUVTOUVROW_AVX2) || defined(HAS_AYUVTOVUROW_AVX2)
static const uint8_t kShufAYUVToUV16_Win[16] = {
    1, 5, 0, 4, 9, 13, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

static const uint8_t kShufAYUVToVU16_Win[16] = {
    0, 4, 1, 5, 8, 12, 9, 13, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

LIBYUV_TARGET_AVX2
static inline void AYUVToUVOrVURow_Win_AVX2(const uint8_t* src_ayuv,
                                            int src_stride_ayuv,
                                            uint8_t* dst_uv,
                                            const uint8_t* shuf_table,
                                            int width) {
  const __m256i ymm_shuf =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)shuf_table));
  const __m256i ymm_0101 = _mm256_set1_epi16(0x0101);
  const __m256i ymm_zero = _mm256_setzero_si256();
  while (width >= 16) {
    __m256i s00 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)src_ayuv), ymm_shuf);
    __m256i s01 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_ayuv + 32)), ymm_shuf);
    __m256i s10 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_ayuv + src_stride_ayuv)),
        ymm_shuf);
    __m256i s11 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_ayuv + src_stride_ayuv + 32)),
        ymm_shuf);
    src_ayuv += 64;
    __m256i p0 =
        _mm256_maddubs_epi16(_mm256_unpacklo_epi64(s00, s01), ymm_0101);
    __m256i p1 =
        _mm256_maddubs_epi16(_mm256_unpacklo_epi64(s10, s11), ymm_0101);
    p0 = _mm256_avg_epu16(_mm256_srli_epi16(_mm256_add_epi16(p0, p1), 1),
                          ymm_zero);
    p0 = _mm256_permute4x64_epi64(p0, 0xd8);
    __m128i out = _mm_packus_epi16(_mm256_castsi256_si128(p0),
                                   _mm256_extracti128_si256(p0, 1));
    _mm_storeu_si128((__m128i*)dst_uv, out);
    dst_uv += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif

#ifdef HAS_AYUVTOUVROW_AVX2
LIBYUV_TARGET_AVX2
void AYUVToUVRow_AVX2(const uint8_t* src_ayuv,
                      int src_stride_ayuv,
                      uint8_t* dst_uv,
                      int width) {
  AYUVToUVOrVURow_Win_AVX2(src_ayuv, src_stride_ayuv, dst_uv,
                           kShufAYUVToUV16_Win, width);
}
#endif  // HAS_AYUVTOUVROW_AVX2

#ifdef HAS_AYUVTOVUROW_AVX2
LIBYUV_TARGET_AVX2
void AYUVToVURow_AVX2(const uint8_t* src_ayuv,
                      int src_stride_ayuv,
                      uint8_t* dst_vu,
                      int width) {
  AYUVToUVOrVURow_Win_AVX2(src_ayuv, src_stride_ayuv, dst_vu,
                           kShufAYUVToVU16_Win, width);
}
#endif  // HAS_AYUVTOVUROW_AVX2

#ifdef HAS_AYUVTOYROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void AYUVToYRow_AVX512VBMI(const uint8_t* src_ayuv, uint8_t* dst_y, int width) {
  static const uint8_t kPermAYUVToY32[64] = {
      2,  6,  10, 14, 18, 22, 26, 30, 34, 38,  42,  46,  50,  54,  58,  62,
      66, 70, 74, 78, 82, 86, 90, 94, 98, 102, 106, 110, 114, 118, 122, 126,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0};
  const __m512i zmm_perm = _mm512_loadu_si512((const __m512i*)kPermAYUVToY32);
  while (width >= 32) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_ayuv);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_ayuv + 64));
    src_ayuv += 128;
    __m512i out = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);
    _mm256_storeu_si256((__m256i*)dst_y, _mm512_castsi512_si256(out));
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)m32;
    __mmask16 k2 = (__mmask16)(m32 >> 16);
    __m512i zmm0 = _mm512_maskz_loadu_epi32(k1, src_ayuv);
    __m512i zmm1 = _mm512_maskz_loadu_epi32(k2, src_ayuv + 64);
    __m512i out = _mm512_permutex2var_epi8(zmm0, zmm_perm, zmm1);
    _mm256_mask_storeu_epi8(dst_y, (__mmask32)m32, _mm512_castsi512_si256(out));
  }
  _mm256_zeroupper();
}
#endif  // HAS_AYUVTOYROW_AVX512VBMI

#if defined(HAS_AYUVTOUVROW_AVX512VBMI) || defined(HAS_AYUVTOVUROW_AVX512VBMI)
static const uint8_t kPermAYUVToUV_Win[64] = {
    1,   5,   0,   4,   9,   13,  8,   12,  17,  21,  16,  20, 25,
    29,  24,  28,  33,  37,  32,  36,  41,  45,  40,  44,  49, 53,
    48,  52,  57,  61,  56,  60,  65,  69,  64,  68,  73,  77, 72,
    76,  81,  85,  80,  84,  89,  93,  88,  92,  97,  101, 96, 100,
    105, 109, 104, 108, 113, 117, 112, 116, 121, 125, 120, 124};

static const uint8_t kPermAYUVToVU_Win[64] = {
    0,   4,   1,   5,   8,   12,  9,   13,  16,  20,  17,  21, 24,
    28,  25,  29,  32,  36,  33,  37,  40,  44,  41,  45,  48, 52,
    49,  53,  56,  60,  57,  61,  64,  68,  65,  69,  72,  76, 73,
    77,  80,  84,  81,  85,  88,  92,  89,  93,  96,  100, 97, 101,
    104, 108, 105, 109, 112, 116, 113, 117, 120, 124, 121, 125};

LIBYUV_TARGET_AVX512VBMI
static inline void AYUVToUVOrVURow_Win_AVX512VBMI(const uint8_t* src_ayuv,
                                                  int src_stride_ayuv,
                                                  uint8_t* dst_uv,
                                                  const uint8_t* perm_table,
                                                  int width) {
  const __m512i zmm_perm = _mm512_loadu_si512((const __m512i*)perm_table);
  const __m512i zmm_0101 = _mm512_set1_epi16(0x0101);
  const __m512i zmm_zero = _mm512_setzero_si512();
  while (width >= 32) {
    __m512i s00 = _mm512_loadu_si512((const __m512i*)src_ayuv);
    __m512i s01 = _mm512_loadu_si512((const __m512i*)(src_ayuv + 64));
    __m512i s10 =
        _mm512_loadu_si512((const __m512i*)(src_ayuv + src_stride_ayuv));
    __m512i s11 =
        _mm512_loadu_si512((const __m512i*)(src_ayuv + src_stride_ayuv + 64));
    src_ayuv += 128;
    __m512i p0 = _mm512_permutex2var_epi8(s00, zmm_perm, s01);
    __m512i p1 = _mm512_permutex2var_epi8(s10, zmm_perm, s11);
    p0 = _mm512_maddubs_epi16(p0, zmm_0101);
    p1 = _mm512_maddubs_epi16(p1, zmm_0101);
    p0 = _mm512_avg_epu16(_mm512_srli_epi16(_mm512_add_epi16(p0, p1), 1),
                          zmm_zero);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm512_cvtepi16_epi8(p0));
    dst_uv += 32;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)m32;
    __mmask16 k2 = (__mmask16)(m32 >> 16);
    __m512i s00 = _mm512_maskz_loadu_epi32(k1, src_ayuv);
    __m512i s01 = _mm512_maskz_loadu_epi32(k2, src_ayuv + 64);
    __m512i s10 = _mm512_maskz_loadu_epi32(k1, src_ayuv + src_stride_ayuv);
    __m512i s11 = _mm512_maskz_loadu_epi32(k2, src_ayuv + src_stride_ayuv + 64);
    __m512i p0 = _mm512_permutex2var_epi8(s00, zmm_perm, s01);
    __m512i p1 = _mm512_permutex2var_epi8(s10, zmm_perm, s11);
    __mmask16 k_odd = (__mmask16)((width & 1) << (width >> 1));
    p0 = _mm512_mask_or_epi32(p0, k_odd, p0, _mm512_slli_epi32(p0, 8));
    p1 = _mm512_mask_or_epi32(p1, k_odd, p1, _mm512_slli_epi32(p1, 8));
    p0 = _mm512_maddubs_epi16(p0, zmm_0101);
    p1 = _mm512_maddubs_epi16(p1, zmm_0101);
    p0 = _mm512_avg_epu16(_mm512_srli_epi16(_mm512_add_epi16(p0, p1), 1),
                          zmm_zero);
    uint32_t out_bytes = (uint32_t)((width + 1) & ~1);
    __mmask32 k_out =
        out_bytes >= 32 ? ~0u : (__mmask32)((1u << out_bytes) - 1u);
    _mm256_mask_storeu_epi8(dst_uv, k_out, _mm512_cvtepi16_epi8(p0));
  }
  _mm256_zeroupper();
}
#endif

#ifdef HAS_AYUVTOUVROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void AYUVToUVRow_AVX512VBMI(const uint8_t* src_ayuv,
                            int src_stride_ayuv,
                            uint8_t* dst_uv,
                            int width) {
  AYUVToUVOrVURow_Win_AVX512VBMI(src_ayuv, src_stride_ayuv, dst_uv,
                                 kPermAYUVToUV_Win, width);
}
#endif  // HAS_AYUVTOUVROW_AVX512VBMI

#ifdef HAS_AYUVTOVUROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void AYUVToVURow_AVX512VBMI(const uint8_t* src_ayuv,
                            int src_stride_ayuv,
                            uint8_t* dst_vu,
                            int width) {
  AYUVToUVOrVURow_Win_AVX512VBMI(src_ayuv, src_stride_ayuv, dst_vu,
                                 kPermAYUVToVU_Win, width);
}
#endif  // HAS_AYUVTOVUROW_AVX512VBMI

// ===========================================================================
// Module 2: 10-bit / 30-bit AR30 & XR30 Pack / Unpack (AVX2 / AVX512BW)
// ===========================================================================

#ifdef HAS_AR30TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void AR30ToARGBRow_AVX2(const uint8_t* src_ar30,
                        uint8_t* dst_argb,
                        int width) {
  const __m256i m0 = _mm256_set1_epi32((int)0xc0000000u);
  const __m256i m1 = _mm256_set1_epi32(0x300000ff);
  const __m256i m2 = _mm256_set1_epi32(0x0c00ff00);
  const __m256i m3 = _mm256_set1_epi32(0x03ff0000);
  while (width >= 8) {
    __m256i v = _mm256_loadu_si256((const __m256i*)src_ar30);
    src_ar30 += 32;
    __m256i out = _mm256_or_si256(
        _mm256_or_si256(_mm256_and_si256(v, m0),
                        _mm256_and_si256(_mm256_srli_epi32(v, 2), m1)),
        _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(v, 4), m2),
                        _mm256_and_si256(_mm256_srli_epi32(v, 6), m3)));
    _mm256_storeu_si256((__m256i*)dst_argb, out);
    dst_argb += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOARGBROW_AVX2

#ifdef HAS_AR30TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void AR30ToARGBRow_AVX512BW(const uint8_t* src_ar30,
                            uint8_t* dst_argb,
                            int width) {
  const __m512i m0 = _mm512_set1_epi32((int)0xc0000000u);
  const __m512i m1 = _mm512_set1_epi32(0x300000ff);
  const __m512i m2 = _mm512_set1_epi32(0x0c00ff00);
  const __m512i m3 = _mm512_set1_epi32(0x03ff0000);
  while (width >= 16) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_ar30);
    src_ar30 += 64;
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_and_si512(v, m0),
                        _mm512_and_si512(_mm512_srli_epi32(v, 2), m1)),
        _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(v, 4), m2),
                        _mm512_and_si512(_mm512_srli_epi32(v, 6), m3)));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi32(k, src_ar30);
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_and_si512(v, m0),
                        _mm512_and_si512(_mm512_srli_epi32(v, 2), m1)),
        _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(v, 4), m2),
                        _mm512_and_si512(_mm512_srli_epi32(v, 6), m3)));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOARGBROW_AVX512BW

#ifdef HAS_AR30TOABGRROW_AVX2
LIBYUV_TARGET_AVX2
void AR30ToABGRRow_AVX2(const uint8_t* src_ar30,
                        uint8_t* dst_abgr,
                        int width) {
  static const uint8_t kShufSwapRB16[16] = {2,  1, 0, 3,  6,  5,  4,  7,
                                            10, 9, 8, 11, 14, 13, 12, 15};
  const __m256i m0 = _mm256_set1_epi32((int)0xc0000000u);
  const __m256i m1 = _mm256_set1_epi32(0x300000ff);
  const __m256i m2 = _mm256_set1_epi32(0x0c00ff00);
  const __m256i m3 = _mm256_set1_epi32(0x03ff0000);
  const __m256i shuf =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)kShufSwapRB16));
  while (width >= 8) {
    __m256i v = _mm256_loadu_si256((const __m256i*)src_ar30);
    src_ar30 += 32;
    __m256i out = _mm256_or_si256(
        _mm256_or_si256(_mm256_and_si256(v, m0),
                        _mm256_and_si256(_mm256_srli_epi32(v, 2), m1)),
        _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(v, 4), m2),
                        _mm256_and_si256(_mm256_srli_epi32(v, 6), m3)));
    out = _mm256_shuffle_epi8(out, shuf);
    _mm256_storeu_si256((__m256i*)dst_abgr, out);
    dst_abgr += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOABGRROW_AVX2

#ifdef HAS_AR30TOABGRROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void AR30ToABGRRow_AVX512BW(const uint8_t* src_ar30,
                            uint8_t* dst_abgr,
                            int width) {
  static const uint8_t kShufSwapRB16[16] = {2,  1, 0, 3,  6,  5,  4,  7,
                                            10, 9, 8, 11, 14, 13, 12, 15};
  const __m512i m0 = _mm512_set1_epi32((int)0xc0000000u);
  const __m512i m1 = _mm512_set1_epi32(0x300000ff);
  const __m512i m2 = _mm512_set1_epi32(0x0c00ff00);
  const __m512i m3 = _mm512_set1_epi32(0x03ff0000);
  const __m512i shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kShufSwapRB16));
  while (width >= 16) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_ar30);
    src_ar30 += 64;
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_and_si512(v, m0),
                        _mm512_and_si512(_mm512_srli_epi32(v, 2), m1)),
        _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(v, 4), m2),
                        _mm512_and_si512(_mm512_srli_epi32(v, 6), m3)));
    out = _mm512_shuffle_epi8(out, shuf);
    _mm512_storeu_si512((__m512i*)dst_abgr, out);
    dst_abgr += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi32(k, src_ar30);
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_and_si512(v, m0),
                        _mm512_and_si512(_mm512_srli_epi32(v, 2), m1)),
        _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(v, 4), m2),
                        _mm512_and_si512(_mm512_srli_epi32(v, 6), m3)));
    out = _mm512_shuffle_epi8(out, shuf);
    _mm512_mask_storeu_epi32(dst_abgr, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOABGRROW_AVX512BW

#ifdef HAS_AR30TOAB30ROW_AVX2
LIBYUV_TARGET_AVX2
void AR30ToAB30Row_AVX2(const uint8_t* src_ar30,
                        uint8_t* dst_ab30,
                        int width) {
  const __m256i m0 = _mm256_set1_epi32((int)0xc00ffc00u);
  const __m256i m1 = _mm256_set1_epi32(0x3ff00000);
  const __m256i m2 = _mm256_set1_epi32(0x000003ff);
  while (width >= 8) {
    __m256i v = _mm256_loadu_si256((const __m256i*)src_ar30);
    src_ar30 += 32;
    __m256i out = _mm256_or_si256(
        _mm256_and_si256(v, m0),
        _mm256_or_si256(_mm256_and_si256(_mm256_slli_epi32(v, 20), m1),
                        _mm256_and_si256(_mm256_srli_epi32(v, 20), m2)));
    _mm256_storeu_si256((__m256i*)dst_ab30, out);
    dst_ab30 += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOAB30ROW_AVX2

#ifdef HAS_AR30TOAB30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void AR30ToAB30Row_AVX512BW(const uint8_t* src_ar30,
                            uint8_t* dst_ab30,
                            int width) {
  const __m512i m0 = _mm512_set1_epi32((int)0xc00ffc00u);
  const __m512i m1 = _mm512_set1_epi32(0x3ff00000);
  const __m512i m2 = _mm512_set1_epi32(0x000003ff);
  while (width >= 16) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_ar30);
    src_ar30 += 64;
    __m512i out = _mm512_or_si512(
        _mm512_and_si512(v, m0),
        _mm512_or_si512(_mm512_and_si512(_mm512_slli_epi32(v, 20), m1),
                        _mm512_and_si512(_mm512_srli_epi32(v, 20), m2)));
    _mm512_storeu_si512((__m512i*)dst_ab30, out);
    dst_ab30 += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi32(k, src_ar30);
    __m512i out = _mm512_or_si512(
        _mm512_and_si512(v, m0),
        _mm512_or_si512(_mm512_and_si512(_mm512_slli_epi32(v, 20), m1),
                        _mm512_and_si512(_mm512_srli_epi32(v, 20), m2)));
    _mm512_mask_storeu_epi32(dst_ab30, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_AR30TOAB30ROW_AVX512BW

#ifdef HAS_MERGEXR30ROW_10_AVX512BW
LIBYUV_TARGET_AVX512BW
void MergeXR30Row_10_AVX512BW(const uint16_t* src_r,
                              const uint16_t* src_g,
                              const uint16_t* src_b,
                              uint8_t* dst_ar30,
                              int depth,
                              int width) {
  (void)depth;
  const __m512i max10 = _mm512_set1_epi32(1023);
  const __m512i alpha = _mm512_set1_epi32((int)0xc0000000u);
  while (width >= 16) {
    __m512i r = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src_r)),
        max10);
    __m512i g = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src_g)),
        max10);
    __m512i b = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src_b)),
        max10);
    src_r += 16;
    src_g += 16;
    src_b += 16;
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_slli_epi32(r, 20), _mm512_slli_epi32(g, 10)),
        _mm512_or_si512(b, alpha));
    _mm512_storeu_si512((__m512i*)dst_ar30, out);
    dst_ar30 += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i r = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src_r)), max10);
    __m512i g = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src_g)), max10);
    __m512i b = _mm512_min_epu32(
        _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src_b)), max10);
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(_mm512_slli_epi32(r, 20), _mm512_slli_epi32(g, 10)),
        _mm512_or_si512(b, alpha));
    _mm512_mask_storeu_epi32(dst_ar30, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEXR30ROW_10_AVX512BW

#if defined(HAS_ARGBTOAR30ROW_AVX512BW) || defined(HAS_ABGRTOAR30ROW_AVX512BW)
static const uint8_t kShuffleRB30_Win[16] = {128u, 0u,  128u, 2u, 128u, 4u,
                                             128u, 6u,  128u, 8u, 128u, 10u,
                                             128u, 12u, 128u, 14u};

static const uint8_t kShuffleBR30_Win[16] = {128u, 2u,  128u, 0u,  128u, 6u,
                                             128u, 4u,  128u, 10u, 128u, 8u,
                                             128u, 14u, 128u, 12u};

LIBYUV_TARGET_AVX512BW
static inline void ARGBOrABGRToAR30Row_Win_AVX512BW(const uint8_t* src,
                                                    uint8_t* dst,
                                                    const uint8_t* shuf_rb,
                                                    int width) {
  const __m512i zmm_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)shuf_rb));
  const __m512i zmm_mul_rb = _mm512_set1_epi32(1028 * 16 * 65536 + 1028);
  const __m512i zmm_mask_rb = _mm512_set1_epi32(0x3ff003ff);
  const __m512i zmm_mask_ag = _mm512_set1_epi32((int)0xc000ff00u);
  const __m512i zmm_mul_ag = _mm512_set1_epi32(64 * 65536 + 1028);
  while (width >= 16) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src);
    src += 64;
    __m512i rb = _mm512_and_si512(
        _mm512_mulhi_epu16(_mm512_shuffle_epi8(v, zmm_shuf), zmm_mul_rb),
        zmm_mask_rb);
    __m512i ag = _mm512_slli_epi32(
        _mm512_mulhi_epu16(_mm512_and_si512(v, zmm_mask_ag), zmm_mul_ag), 10);
    _mm512_storeu_si512((__m512i*)dst, _mm512_or_si512(ag, rb));
    dst += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi32(k, src);
    __m512i rb = _mm512_and_si512(
        _mm512_mulhi_epu16(_mm512_shuffle_epi8(v, zmm_shuf), zmm_mul_rb),
        zmm_mask_rb);
    __m512i ag = _mm512_slli_epi32(
        _mm512_mulhi_epu16(_mm512_and_si512(v, zmm_mask_ag), zmm_mul_ag), 10);
    _mm512_mask_storeu_epi32(dst, k, _mm512_or_si512(ag, rb));
  }
  _mm256_zeroupper();
}
#endif

#ifdef HAS_ARGBTOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToAR30Row_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ARGBOrABGRToAR30Row_Win_AVX512BW(src, dst, kShuffleRB30_Win, width);
}
#endif  // HAS_ARGBTOAR30ROW_AVX512BW

#ifdef HAS_ABGRTOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ABGRToAR30Row_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ARGBOrABGRToAR30Row_Win_AVX512BW(src, dst, kShuffleBR30_Win, width);
}
#endif  // HAS_ABGRTOAR30ROW_AVX512BW

// ===========================================================================
// Module 3: Planar / Channel Pack & Mirror / Interpolate_16 (AVX512VBMI / BW)
// ===========================================================================

#ifdef HAS_MERGERGBROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void MergeRGBRow_AVX512VBMI(const uint8_t* src_r,
                            const uint8_t* src_g,
                            const uint8_t* src_b,
                            uint8_t* dst_rgb,
                            int width) {
  static const uint8_t kPermMergeRGB64_0[64] = {
      0x00, 0x20, 0x40, 0x01, 0x21, 0x41, 0x02, 0x22, 0x42, 0x03, 0x23,
      0x43, 0x04, 0x24, 0x44, 0x05, 0x25, 0x45, 0x06, 0x26, 0x46, 0x07,
      0x27, 0x47, 0x08, 0x28, 0x48, 0x09, 0x29, 0x49, 0x0a, 0x2a, 0x4a,
      0x0b, 0x2b, 0x4b, 0x0c, 0x2c, 0x4c, 0x0d, 0x2d, 0x4d, 0x0e, 0x2e,
      0x4e, 0x0f, 0x2f, 0x4f, 0x10, 0x30, 0x50, 0x11, 0x31, 0x51, 0x12,
      0x32, 0x52, 0x13, 0x33, 0x53, 0x14, 0x34, 0x54, 0x15};
  static const uint8_t kPermMergeRGB64_1[64] = {
      0x35, 0x55, 0x16, 0x36, 0x56, 0x17, 0x37, 0x57, 0x18, 0x38, 0x58,
      0x19, 0x39, 0x59, 0x1a, 0x3a, 0x5a, 0x1b, 0x3b, 0x5b, 0x1c, 0x3c,
      0x5c, 0x1d, 0x3d, 0x5d, 0x1e, 0x3e, 0x5e, 0x1f, 0x3f, 0x5f, 0x60,
      0x00, 0x20, 0x61, 0x01, 0x21, 0x62, 0x02, 0x22, 0x63, 0x03, 0x23,
      0x64, 0x04, 0x24, 0x65, 0x05, 0x25, 0x66, 0x06, 0x26, 0x67, 0x07,
      0x27, 0x68, 0x08, 0x28, 0x69, 0x09, 0x29, 0x6a, 0x0a};
  static const uint8_t kPermMergeRGB64_2[64] = {
      0x6a, 0x2b, 0x4b, 0x6b, 0x2c, 0x4c, 0x6c, 0x2d, 0x4d, 0x6d, 0x2e,
      0x4e, 0x6e, 0x2f, 0x4f, 0x6f, 0x30, 0x50, 0x70, 0x31, 0x51, 0x71,
      0x32, 0x52, 0x72, 0x33, 0x53, 0x73, 0x34, 0x54, 0x74, 0x35, 0x55,
      0x75, 0x36, 0x56, 0x76, 0x37, 0x57, 0x77, 0x38, 0x58, 0x78, 0x39,
      0x59, 0x79, 0x3a, 0x5a, 0x7a, 0x3b, 0x5b, 0x7b, 0x3c, 0x5c, 0x7c,
      0x3d, 0x5d, 0x7d, 0x3e, 0x5e, 0x7e, 0x3f, 0x5f, 0x7f};
  const __m512i m_rgb0 = _mm512_loadu_si512((const __m512i*)kPermMergeRGB64_0);
  const __m512i m_rgb1 = _mm512_loadu_si512((const __m512i*)kPermMergeRGB64_1);
  const __m512i m_rgb2 = _mm512_loadu_si512((const __m512i*)kPermMergeRGB64_2);
  while (width >= 64) {
    __m512i rg0 = _mm512_inserti64x4(
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)src_r)),
        _mm256_loadu_si256((const __m256i*)src_g), 1);
    __m512i br = _mm512_inserti64x4(
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)src_b)),
        _mm256_loadu_si256((const __m256i*)(src_r + 32)), 1);
    __m512i gb1 = _mm512_inserti64x4(
        _mm512_castsi256_si512(
            _mm256_loadu_si256((const __m256i*)(src_g + 32))),
        _mm256_loadu_si256((const __m256i*)(src_b + 32)), 1);
    src_r += 64;
    src_g += 64;
    src_b += 64;
    __m512i d0 = _mm512_permutex2var_epi8(rg0, m_rgb0, br);
    __m512i d1 = MaskPermutexvarEpi8(_mm512_permutex2var_epi8(rg0, m_rgb1, br),
                                     0xb6db6db600000000ull, m_rgb1, gb1);
    __m512i d2 = _mm512_permutex2var_epi8(br, m_rgb2, gb1);
    _mm512_storeu_si512((__m512i*)dst_rgb, d0);
    _mm512_storeu_si512((__m512i*)(dst_rgb + 64), d1);
    _mm512_storeu_si512((__m512i*)(dst_rgb + 128), d2);
    dst_rgb += 192;
    width -= 64;
  }
  if (width >= 32) {
    __m512i rg = _mm512_inserti64x4(
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)src_r)),
        _mm256_loadu_si256((const __m256i*)src_g), 1);
    __m512i b =
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)src_b));
    src_r += 32;
    src_g += 32;
    src_b += 32;
    __m512i d0 = _mm512_permutex2var_epi8(rg, m_rgb0, b);
    __m512i d1 = _mm512_permutex2var_epi8(rg, m_rgb1, b);
    _mm512_storeu_si512((__m512i*)dst_rgb, d0);
    _mm256_storeu_si256((__m256i*)(dst_rgb + 64), _mm512_castsi512_si256(d1));
    dst_rgb += 96;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k_in = (__mmask32)((1u << width) - 1u);
    __m512i rg = _mm512_inserti64x4(
        _mm512_castsi256_si512(_mm256_maskz_loadu_epi8(k_in, src_r)),
        _mm256_maskz_loadu_epi8(k_in, src_g), 1);
    __m512i b = _mm512_castsi256_si512(_mm256_maskz_loadu_epi8(k_in, src_b));
    __m512i d0 = _mm512_permutex2var_epi8(rg, m_rgb0, b);
    __m512i d1 = _mm512_permutex2var_epi8(rg, m_rgb1, b);
    int dst_bytes = width * 3;
    __mmask64 k0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull;
    __mmask32 k1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;
    _mm512_mask_storeu_epi8(dst_rgb, k0, d0);
    _mm256_mask_storeu_epi8(dst_rgb + 64, k1, _mm512_castsi512_si256(d1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGERGBROW_AVX512VBMI

#ifdef HAS_RAWTORGBAROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void RAWToRGBARow_AVX512VBMI(const uint8_t* src_raw,
                             uint8_t* dst_rgba,
                             int width) {
  static const uint8_t kPermRAWToRGBA0[64] = {
      0, 2,  1,  0,  0, 5,  4,  3,  0, 8,  7,  6,  0, 11, 10, 9,
      0, 14, 13, 12, 0, 17, 16, 15, 0, 20, 19, 18, 0, 23, 22, 21,
      0, 26, 25, 24, 0, 29, 28, 27, 0, 32, 31, 30, 0, 35, 34, 33,
      0, 38, 37, 36, 0, 41, 40, 39, 0, 44, 43, 42, 0, 47, 46, 45};
  static const uint8_t kPermRAWToRGBA1[64] = {
      0, 50, 49, 48, 0, 53, 52, 51, 0, 56, 55, 54, 0, 59, 58, 57,
      0, 62, 61, 60, 0, 65, 64, 63, 0, 68, 67, 66, 0, 71, 70, 69,
      0, 74, 73, 72, 0, 77, 76, 75, 0, 80, 79, 78, 0, 83, 82, 81,
      0, 86, 85, 84, 0, 89, 88, 87, 0, 92, 91, 90, 0, 95, 94, 93};
  const __m512i p0 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGBA0);
  const __m512i p1 = _mm512_loadu_si512((const __m512i*)kPermRAWToRGBA1);
  const __m512i alpha = _mm512_set1_epi32(0x000000ff);
  while (width >= 32) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_raw);
    __m512i s1 = _mm512_castsi256_si512(
        _mm256_loadu_si256((const __m256i*)(src_raw + 64)));
    src_raw += 96;
    __m512i d0 = _mm512_or_si512(PermutexvarEpi8(p0, s0), alpha);
    __m512i d1 = _mm512_or_si512(_mm512_permutex2var_epi8(s0, p1, s1), alpha);
    _mm512_storeu_si512((__m512i*)dst_rgba, d0);
    _mm512_storeu_si512((__m512i*)(dst_rgba + 64), d1);
    dst_rgba += 128;
    width -= 32;
  }
  if (width > 0) {
    int src_bytes = width * 3;
    __mmask64 ks0 = src_bytes >= 64 ? ~0ull : (1ull << src_bytes) - 1ull;
    __mmask32 ks1 = src_bytes > 64 ? (1u << (src_bytes - 64)) - 1u : 0u;
    __m512i s0 = _mm512_maskz_loadu_epi8(ks0, src_raw);
    __m512i s1 =
        _mm512_castsi256_si512(_mm256_maskz_loadu_epi8(ks1, src_raw + 64));
    __m512i d0 = _mm512_or_si512(PermutexvarEpi8(p0, s0), alpha);
    __m512i d1 = _mm512_or_si512(_mm512_permutex2var_epi8(s0, p1, s1), alpha);
    uint32_t m32 = (1u << width) - 1u;
    _mm512_mask_storeu_epi32(dst_rgba, (__mmask16)m32, d0);
    _mm512_mask_storeu_epi32(dst_rgba + 64, (__mmask16)(m32 >> 16), d1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_RAWTORGBAROW_AVX512VBMI

#ifdef HAS_SWAPUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void SwapUVRow_AVX512BW(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  static const uint8_t kSwapUV16[16] = {1, 0, 3,  2,  5,  4,  7,  6,
                                        9, 8, 11, 10, 13, 12, 15, 14};
  const __m512i swap =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kSwapUV16));
  while (width >= 32) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_uv);
    src_uv += 64;
    _mm512_storeu_si512((__m512i*)dst_vu, _mm512_shuffle_epi8(v, swap));
    dst_vu += 64;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi16(k, src_uv);
    _mm512_mask_storeu_epi16(dst_vu, k, _mm512_shuffle_epi8(v, swap));
  }
  _mm256_zeroupper();
}
#endif  // HAS_SWAPUVROW_AVX512BW

#ifdef HAS_MIRRORROW_16_AVX512BW
LIBYUV_TARGET_AVX512BW
void MirrorRow_16_AVX512BW(const uint16_t* src, uint16_t* dst, int width) {
  static const uint16_t kMirror16Idx[32] = {
      31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
      15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
  const __m512i rev = _mm512_loadu_si512((const __m512i*)kMirror16Idx);
  src += width;
  while (width >= 32) {
    src -= 32;
    __m512i v = _mm512_loadu_si512((const __m512i*)src);
    _mm512_storeu_si512((__m512i*)dst, PermutexvarEpi16(rev, v));
    dst += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i idx = _mm512_sub_epi16(rev, _mm512_set1_epi16((short)(32 - width)));
    __m512i v = _mm512_maskz_loadu_epi16(k, src - width);
    _mm512_mask_storeu_epi16(dst, k, PermutexvarEpi16(idx, v));
  }
  _mm256_zeroupper();
}
#endif  // HAS_MIRRORROW_16_AVX512BW

#ifdef HAS_HALFMERGEUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void HalfMergeUVRow_AVX512BW(const uint8_t* src_u,
                             int src_stride_u,
                             const uint8_t* src_v,
                             int src_stride_v,
                             uint8_t* dst_uv,
                             int width) {
  const __m512i c0101 = _mm512_set1_epi16(0x0101);
  const __m512i czero = _mm512_setzero_si512();
  const __m512i c00ff = _mm512_set1_epi16(0x00ff);
  while (width >= 64) {
    __m512i u0 = _mm512_loadu_si512((const __m512i*)src_u);
    __m512i u1 = _mm512_loadu_si512((const __m512i*)(src_u + src_stride_u));
    __m512i v0 = _mm512_loadu_si512((const __m512i*)src_v);
    __m512i v1 = _mm512_loadu_si512((const __m512i*)(src_v + src_stride_v));
    src_u += 64;
    src_v += 64;
    u0 = _mm512_avg_epu16(
        _mm512_srli_epi16(_mm512_add_epi16(_mm512_maddubs_epi16(u0, c0101),
                                           _mm512_maddubs_epi16(u1, c0101)),
                          1),
        czero);
    v0 = _mm512_avg_epu16(
        _mm512_srli_epi16(_mm512_add_epi16(_mm512_maddubs_epi16(v0, c0101),
                                           _mm512_maddubs_epi16(v1, c0101)),
                          1),
        czero);
    _mm512_storeu_si512((__m512i*)dst_uv,
                        _mm512_or_si512(u0, _mm512_slli_epi16(v0, 8)));
    dst_uv += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 k = width >= 64 ? ~0ull : (1ull << width) - 1ull;
    __m512i u0 = _mm512_maskz_loadu_epi8(k, src_u);
    __m512i u1 = _mm512_maskz_loadu_epi8(k, src_u + src_stride_u);
    __m512i v0 = _mm512_maskz_loadu_epi8(k, src_v);
    __m512i v1 = _mm512_maskz_loadu_epi8(k, src_v + src_stride_v);
    __mmask32 k_odd = (__mmask32)((uint32_t)(width & 1) << (width >> 1));
    u0 = _mm512_mask_mullo_epi16(u0, k_odd, _mm512_and_si512(u0, c00ff), c0101);
    u1 = _mm512_mask_mullo_epi16(u1, k_odd, _mm512_and_si512(u1, c00ff), c0101);
    v0 = _mm512_mask_mullo_epi16(v0, k_odd, _mm512_and_si512(v0, c00ff), c0101);
    v1 = _mm512_mask_mullo_epi16(v1, k_odd, _mm512_and_si512(v1, c00ff), c0101);
    u0 = _mm512_avg_epu16(
        _mm512_srli_epi16(_mm512_add_epi16(_mm512_maddubs_epi16(u0, c0101),
                                           _mm512_maddubs_epi16(u1, c0101)),
                          1),
        czero);
    v0 = _mm512_avg_epu16(
        _mm512_srli_epi16(_mm512_add_epi16(_mm512_maddubs_epi16(v0, c0101),
                                           _mm512_maddubs_epi16(v1, c0101)),
                          1),
        czero);
    int out_words = (width + 1) >> 1;
    __mmask32 k_out =
        out_words >= 32 ? ~0u : (__mmask32)((1u << out_words) - 1u);
    _mm512_mask_storeu_epi16(dst_uv, k_out,
                             _mm512_or_si512(u0, _mm512_slli_epi16(v0, 8)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_HALFMERGEUVROW_AVX512BW

#ifdef HAS_INTERPOLATEROW_16_AVX512BW
LIBYUV_TARGET_AVX512BW
void InterpolateRow_16_AVX512BW(uint16_t* dst_ptr,
                                const uint16_t* src_ptr,
                                ptrdiff_t src_stride,
                                int width,
                                int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  __m512i zmm_y = _mm512_set1_epi32((y1 << 16) | y0);
  __m512i zmm_8000 = _mm512_set1_epi16((short)0x8000);
  __m512i zmm_round = _mm512_set1_epi32(8388736);  // 0x800000 + 128
  int i;

  if (y1 == 0) {
    for (i = 0; i <= width - 32; i += 32) {
      _mm512_storeu_si512((__m512i*)(dst_ptr + i),
                          _mm512_loadu_si512((const __m512i*)(src_ptr + i)));
    }
    if (i < width) {
      __mmask32 mask = (__mmask32)((1u << (width - i)) - 1u);
      _mm512_mask_storeu_epi16(dst_ptr + i, mask,
                               _mm512_maskz_loadu_epi16(mask, src_ptr + i));
    }
  } else if (y1 == 128) {
    for (i = 0; i <= width - 32; i += 32) {
      __m512i row0 = _mm512_loadu_si512((const __m512i*)(src_ptr + i));
      __m512i row1 = _mm512_loadu_si512((const __m512i*)(src_ptr1 + i));
      _mm512_storeu_si512((__m512i*)(dst_ptr + i),
                          _mm512_avg_epu16(row0, row1));
    }
    if (i < width) {
      __mmask32 mask = (__mmask32)((1u << (width - i)) - 1u);
      __m512i row0 = _mm512_maskz_loadu_epi16(mask, src_ptr + i);
      __m512i row1 = _mm512_maskz_loadu_epi16(mask, src_ptr1 + i);
      _mm512_mask_storeu_epi16(dst_ptr + i, mask, _mm512_avg_epu16(row0, row1));
    }
  } else {
    for (i = 0; i <= width - 32; i += 32) {
      __m512i row0 = _mm512_loadu_si512((const __m512i*)(src_ptr + i));
      __m512i row1 = _mm512_loadu_si512((const __m512i*)(src_ptr1 + i));
      __m512i row0l = _mm512_unpacklo_epi16(row0, row1);
      __m512i row0h = _mm512_unpackhi_epi16(row0, row1);
      row0l = _mm512_sub_epi16(row0l, zmm_8000);
      row0h = _mm512_sub_epi16(row0h, zmm_8000);
      __m512i resl = _mm512_madd_epi16(row0l, zmm_y);
      __m512i resh = _mm512_madd_epi16(row0h, zmm_y);
      resl = _mm512_add_epi32(resl, zmm_round);
      resh = _mm512_add_epi32(resh, zmm_round);
      resl = _mm512_srai_epi32(resl, 8);
      resh = _mm512_srai_epi32(resh, 8);
      _mm512_storeu_si512((__m512i*)(dst_ptr + i),
                          _mm512_packus_epi32(resl, resh));
    }
    if (i < width) {
      __mmask32 mask = (__mmask32)((1u << (width - i)) - 1u);
      __m512i row0 = _mm512_maskz_loadu_epi16(mask, src_ptr + i);
      __m512i row1 = _mm512_maskz_loadu_epi16(mask, src_ptr1 + i);
      __m512i row0l = _mm512_unpacklo_epi16(row0, row1);
      __m512i row0h = _mm512_unpackhi_epi16(row0, row1);
      row0l = _mm512_sub_epi16(row0l, zmm_8000);
      row0h = _mm512_sub_epi16(row0h, zmm_8000);
      __m512i resl = _mm512_madd_epi16(row0l, zmm_y);
      __m512i resh = _mm512_madd_epi16(row0h, zmm_y);
      resl = _mm512_add_epi32(resl, zmm_round);
      resh = _mm512_add_epi32(resh, zmm_round);
      resl = _mm512_srai_epi32(resl, 8);
      resh = _mm512_srai_epi32(resh, 8);
      _mm512_mask_storeu_epi16(dst_ptr + i, mask,
                               _mm512_packus_epi32(resl, resh));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_INTERPOLATEROW_16_AVX512BW

// ===========================================================================
// Module 4: NV12 / NV21 & I422Alpha to ARGB / RGB24 (AVX512BW / AVX512VBMI)
// ===========================================================================

#if defined(HAS_NV12TOARGBROW_AVX512BW) ||    \
    defined(HAS_NV21TOARGBROW_AVX512BW) ||    \
    defined(HAS_NV12TORGB24ROW_AVX512VBMI) || \
    defined(HAS_NV21TORGB24ROW_AVX512VBMI) || \
    defined(HAS_I422ALPHATOARGBROW_AVX512BW)
static const uint16_t kSplitQuadWords_Win[32] = {
    0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,
    8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15};
static const uint64_t kUnpermuteAVX512_Win[8] = {0, 4, 1, 5, 2, 6, 3, 7};
#endif

#ifdef HAS_NV12TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void NV12ToARGBRow_AVX512BW(const uint8_t* y_buf,
                            const uint8_t* uv_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_Win);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_Win);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)uv_buf)));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    uv_buf += 32;
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_storeu_si512((__m512i*)dst_argb, _mm512_unpacklo_epi16(bg, ra));
    _mm512_storeu_si512((__m512i*)(dst_argb + 64),
                        _mm512_unpackhi_epi16(bg, ra));
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_maskz_loadu_epi16(k_uv, uv_buf)));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_mask_storeu_epi32(dst_argb, (__mmask16)m32,
                             _mm512_unpacklo_epi16(bg, ra));
    _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(m32 >> 16),
                             _mm512_unpackhi_epi16(bg, ra));
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV12TOARGBROW_AVX512BW

#ifdef HAS_NV21TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void NV21ToARGBRow_AVX512BW(const uint8_t* y_buf,
                            const uint8_t* vu_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_Win);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_Win);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_or_si256(
            _mm256_slli_epi16(_mm256_loadu_si256((const __m256i*)vu_buf), 8),
            _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)vu_buf), 8))));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    vu_buf += 32;
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_storeu_si512((__m512i*)dst_argb, _mm512_unpacklo_epi16(bg, ra));
    _mm512_storeu_si512((__m512i*)(dst_argb + 64),
                        _mm512_unpackhi_epi16(bg, ra));
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_or_si256(
            _mm256_slli_epi16(_mm256_maskz_loadu_epi16(k_uv, vu_buf), 8),
            _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, vu_buf), 8))));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_mask_storeu_epi32(dst_argb, (__mmask16)m32,
                             _mm512_unpacklo_epi16(bg, ra));
    _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(m32 >> 16),
                             _mm512_unpackhi_epi16(bg, ra));
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV21TOARGBROW_AVX512BW

#if defined(HAS_NV12TORGB24ROW_AVX512VBMI) || \
    defined(HAS_NV21TORGB24ROW_AVX512VBMI)
static const uint8_t kMaskBG_NV[64] = {
    0x00, 0x40, 0x01, 0x41, 0x02, 0x42, 0x03, 0x43, 0x04, 0x44, 0x05,
    0x45, 0x06, 0x46, 0x07, 0x47, 0x10, 0x50, 0x11, 0x51, 0x12, 0x52,
    0x13, 0x53, 0x14, 0x54, 0x15, 0x55, 0x16, 0x56, 0x17, 0x57, 0x20,
    0x60, 0x21, 0x61, 0x22, 0x62, 0x23, 0x63, 0x24, 0x64, 0x25, 0x65,
    0x26, 0x66, 0x27, 0x67, 0x30, 0x70, 0x31, 0x71, 0x32, 0x72, 0x33,
    0x73, 0x34, 0x74, 0x35, 0x75, 0x36, 0x76, 0x37, 0x77};
static const uint8_t kMaskDST0_NV[64] = {
    0x00, 0x01, 0x40, 0x02, 0x03, 0x41, 0x04, 0x05, 0x42, 0x06, 0x07,
    0x43, 0x08, 0x09, 0x44, 0x0a, 0x0b, 0x45, 0x0c, 0x0d, 0x46, 0x0e,
    0x0f, 0x47, 0x10, 0x11, 0x50, 0x12, 0x13, 0x51, 0x14, 0x15, 0x52,
    0x16, 0x17, 0x53, 0x18, 0x19, 0x54, 0x1a, 0x1b, 0x55, 0x1c, 0x1d,
    0x56, 0x1e, 0x1f, 0x57, 0x20, 0x21, 0x60, 0x22, 0x23, 0x61, 0x24,
    0x25, 0x62, 0x26, 0x27, 0x63, 0x28, 0x29, 0x64, 0x2a};
static const uint8_t kMaskDST1_NV[64] = {
    0x2b, 0x65, 0x2c, 0x2d, 0x66, 0x2e, 0x2f, 0x67, 0x30, 0x31, 0x70,
    0x32, 0x33, 0x71, 0x34, 0x35, 0x72, 0x36, 0x37, 0x73, 0x38, 0x39,
    0x74, 0x3a, 0x3b, 0x75, 0x3c, 0x3d, 0x76, 0x3e, 0x3f, 0x77, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

#ifdef HAS_NV12TORGB24ROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void NV12ToRGB24Row_AVX512VBMI(const uint8_t* y_buf,
                               const uint8_t* uv_buf,
                               uint8_t* dst_rgb24,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_Win);
  const __m512i zmm_mask_BG = _mm512_loadu_si512((const __m512i*)kMaskBG_NV);
  const __m512i zmm_mask_DST0 =
      _mm512_loadu_si512((const __m512i*)kMaskDST0_NV);
  const __m512i zmm_mask_DST1 =
      _mm512_loadu_si512((const __m512i*)kMaskDST1_NV);
  while (width >= 32) {
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_loadu_si256((const __m256i*)uv_buf)));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    uv_buf += 32;
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6);
    __m512i zmm1 = _mm512_srai_epi16(
        _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6);
    __m512i zmm2 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    _mm512_storeu_si512((__m512i*)dst_rgb24, zmm_dst0);
    _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64),
                        _mm512_castsi512_si256(zmm_dst1));
    dst_rgb24 += 96;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_maskz_loadu_epi16(k_uv, uv_buf)));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6);
    __m512i zmm1 = _mm512_srai_epi16(
        _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6);
    __m512i zmm2 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    int dst_bytes = width * 3;
    __mmask64 k0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull;
    __mmask32 k1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;
    _mm512_mask_storeu_epi8(dst_rgb24, k0, zmm_dst0);
    _mm256_mask_storeu_epi8(dst_rgb24 + 64, k1,
                            _mm512_castsi512_si256(zmm_dst1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV12TORGB24ROW_AVX512VBMI

#ifdef HAS_NV21TORGB24ROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void NV21ToRGB24Row_AVX512VBMI(const uint8_t* y_buf,
                               const uint8_t* vu_buf,
                               uint8_t* dst_rgb24,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_Win);
  const __m512i zmm_mask_BG = _mm512_loadu_si512((const __m512i*)kMaskBG_NV);
  const __m512i zmm_mask_DST0 =
      _mm512_loadu_si512((const __m512i*)kMaskDST0_NV);
  const __m512i zmm_mask_DST1 =
      _mm512_loadu_si512((const __m512i*)kMaskDST1_NV);
  while (width >= 32) {
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_or_si256(
            _mm256_slli_epi16(_mm256_loadu_si256((const __m256i*)vu_buf), 8),
            _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)vu_buf), 8))));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    vu_buf += 32;
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6);
    __m512i zmm1 = _mm512_srai_epi16(
        _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6);
    __m512i zmm2 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    _mm512_storeu_si512((__m512i*)dst_rgb24, zmm_dst0);
    _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64),
                        _mm512_castsi512_si256(zmm_dst1));
    dst_rgb24 += 96;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm3 = PermutexvarEpi16(
        zmm_quad,
        _mm512_castsi256_si512(_mm256_or_si256(
            _mm256_slli_epi16(_mm256_maskz_loadu_epi16(k_uv, vu_buf), 8),
            _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, vu_buf), 8))));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6);
    __m512i zmm1 = _mm512_srai_epi16(
        _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6);
    __m512i zmm2 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    int dst_bytes = width * 3;
    __mmask64 k0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull;
    __mmask32 k1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;
    _mm512_mask_storeu_epi8(dst_rgb24, k0, zmm_dst0);
    _mm256_mask_storeu_epi8(dst_rgb24 + 64, k1,
                            _mm512_castsi512_si256(zmm_dst1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV21TORGB24ROW_AVX512VBMI

#ifdef HAS_I422ALPHATOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I422AlphaToARGBRow_AVX512BW(const uint8_t* y_buf,
                                 const uint8_t* u_buf,
                                 const uint8_t* v_buf,
                                 const uint8_t* a_buf,
                                 uint8_t* dst_argb,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToB));
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToG));
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kUVToR));
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYToRgb));
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)yuvconstants->kYBiasToRgb));
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_Win);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_Win);
  ptrdiff_t offset = v_buf - u_buf;
  while (width >= 32) {
    __m128i xu = _mm_loadu_si128((const __m128i*)u_buf);
    __m128i xv = _mm_loadu_si128((const __m128i*)(u_buf + offset));
    u_buf += 16;
    __m256i yuv = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_castsi256_si512(yuv));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    __m512i zmm5 = _mm512_packus_epi16(
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)a_buf)),
        _mm512_setzero_si512());
    y_buf += 32;
    a_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_storeu_si512((__m512i*)dst_argb, _mm512_unpacklo_epi16(bg, ra));
    _mm512_storeu_si512((__m512i*)(dst_argb + 64),
                        _mm512_unpackhi_epi16(bg, ra));
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m128i xu = _mm_maskz_loadu_epi8(k_uv, u_buf);
    __m128i xv = _mm_maskz_loadu_epi8(k_uv, u_buf + offset);
    __m256i yuv = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_castsi256_si512(yuv));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    __m512i zmm5 = _mm512_packus_epi16(
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, a_buf)),
        _mm512_setzero_si512());
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    zmm3 = _mm512_sub_epi8(zmm3, zmm_128);
    zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),
                            zmm_kYBiasToRgb);
    __m512i zmm0 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4), 6),
        zmm5);
    __m512i zmm1 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)), 6),
        zmm5);
    __m512i zmm2 = _mm512_packus_epi16(
        _mm512_srai_epi16(
            _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4), 6),
        zmm5);
    __m512i bg = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));
    __m512i ra = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));
    _mm512_mask_storeu_epi32(dst_argb, (__mmask16)m32,
                             _mm512_unpacklo_epi16(bg, ra));
    _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(m32 >> 16),
                             _mm512_unpackhi_epi16(bg, ra));
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422ALPHATOARGBROW_AVX512BW

// ===========================================================================
// Module 5: I400ToARGB & ARGB Shaders (Attenuate, Multiply, Gray, Blend)
// ===========================================================================

#ifdef HAS_I400TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I400ToARGBRow_AVX512BW(const uint8_t* y_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  static const uint64_t kLinearizePackQ[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  static const uint8_t kBcastGrayByte[16] = {0, 0, 0, 0x80, 4,  4,  4,  0x80,
                                             8, 8, 8, 0x80, 12, 12, 12, 0x80};
  const __m512i zmm_yg =
      _mm512_set1_epi16(*(const int16_t*)yuvconstants->kYToRgb);
  const __m512i zmm_ygb =
      _mm512_set1_epi16(*(const int16_t*)yuvconstants->kYBiasToRgb);
  const __m512i zmm_lin = _mm512_loadu_si512((const __m512i*)kLinearizePackQ);
  const __m512i zmm_bcast =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kBcastGrayByte));
  const __m512i zmm_alpha = _mm512_set1_epi32((int)0xff000000u);
  while (width >= 32) {
    __m512i y16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    y_buf += 32;
    y16 = _mm512_or_si512(y16, _mm512_slli_epi16(y16, 8));
    y16 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_mulhi_epu16(y16, zmm_yg), zmm_ygb), 6);
    __m512i g = PermutexvarEpi64(zmm_lin, _mm512_packus_epi16(y16, y16));
    __m256i g256 = _mm512_castsi512_si256(g);
    __m512i d0 = _mm512_or_si512(
        _mm512_shuffle_epi8(_mm512_cvtepu8_epi32(_mm256_castsi256_si128(g256)),
                            zmm_bcast),
        zmm_alpha);
    __m512i d1 = _mm512_or_si512(
        _mm512_shuffle_epi8(
            _mm512_cvtepu8_epi32(_mm256_extracti128_si256(g256, 1)), zmm_bcast),
        zmm_alpha);
    _mm512_storeu_si512((__m512i*)dst_argb, d0);
    _mm512_storeu_si512((__m512i*)(dst_argb + 64), d1);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __m512i y16 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    y16 = _mm512_or_si512(y16, _mm512_slli_epi16(y16, 8));
    y16 = _mm512_srai_epi16(
        _mm512_adds_epi16(_mm512_mulhi_epu16(y16, zmm_yg), zmm_ygb), 6);
    __m512i g = PermutexvarEpi64(zmm_lin, _mm512_packus_epi16(y16, y16));
    __m256i g256 = _mm512_castsi512_si256(g);
    __m512i d0 = _mm512_or_si512(
        _mm512_shuffle_epi8(_mm512_cvtepu8_epi32(_mm256_castsi256_si128(g256)),
                            zmm_bcast),
        zmm_alpha);
    __m512i d1 = _mm512_or_si512(
        _mm512_shuffle_epi8(
            _mm512_cvtepu8_epi32(_mm256_extracti128_si256(g256, 1)), zmm_bcast),
        zmm_alpha);
    _mm512_mask_storeu_epi32(dst_argb, (__mmask16)m32, d0);
    _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(m32 >> 16), d1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I400TOARGBROW_AVX512BW

#ifdef HAS_ARGBATTENUATEROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBAttenuateRow_AVX512BW(const uint8_t* src_argb,
                               uint8_t* dst_argb,
                               int width) {
  static const int8_t kAttenShuf16[16] = {6,    -128, 6,    -128, 6,  -128,
                                          -128, -128, 14,   -128, 14, -128,
                                          14,   -128, -128, -128};
  const __m512i zmm_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kAttenShuf16));
  const __m512i zmm_alpha = _mm512_set1_epi32((int)0xff000000u);
  const __m512i zmm_bias = _mm512_set1_epi16(0x00ff);
  while (width >= 16) {
    __m512i s = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    __m512i lo = _mm512_unpacklo_epi8(s, zmm_alpha);
    __m512i hi = _mm512_unpackhi_epi8(s, zmm_alpha);
    lo = _mm512_srli_epi16(
        _mm512_add_epi16(
            _mm512_mullo_epi16(lo, _mm512_shuffle_epi8(lo, zmm_shuf)),
            zmm_bias),
        8);
    hi = _mm512_srli_epi16(
        _mm512_add_epi16(
            _mm512_mullo_epi16(hi, _mm512_shuffle_epi8(hi, zmm_shuf)),
            zmm_bias),
        8);
    __m512i out = _mm512_or_si512(_mm512_packus_epi16(lo, hi),
                                  _mm512_and_si512(s, zmm_alpha));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i lo = _mm512_unpacklo_epi8(s, zmm_alpha);
    __m512i hi = _mm512_unpackhi_epi8(s, zmm_alpha);
    lo = _mm512_srli_epi16(
        _mm512_add_epi16(
            _mm512_mullo_epi16(lo, _mm512_shuffle_epi8(lo, zmm_shuf)),
            zmm_bias),
        8);
    hi = _mm512_srli_epi16(
        _mm512_add_epi16(
            _mm512_mullo_epi16(hi, _mm512_shuffle_epi8(hi, zmm_shuf)),
            zmm_bias),
        8);
    __m512i out = _mm512_or_si512(_mm512_packus_epi16(lo, hi),
                                  _mm512_and_si512(s, zmm_alpha));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBATTENUATEROW_AVX512BW

#ifdef HAS_ARGBMULTIPLYROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBMultiplyRow_AVX512BW(const uint8_t* src_argb,
                              const uint8_t* src_argb1,
                              uint8_t* dst_argb,
                              int width) {
  const __m512i zero = _mm512_setzero_si512();
  while (width >= 16) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)src_argb1);
    src_argb += 64;
    src_argb1 += 64;
    __m512i lo = _mm512_mulhrs_epi16(
        _mm512_srli_epi16(_mm512_unpacklo_epi8(zero, s0), 1),
        _mm512_unpacklo_epi8(s1, zero));
    __m512i hi = _mm512_mulhrs_epi16(
        _mm512_srli_epi16(_mm512_unpackhi_epi8(zero, s0), 1),
        _mm512_unpackhi_epi8(s1, zero));
    _mm512_storeu_si512((__m512i*)dst_argb, _mm512_packus_epi16(lo, hi));
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s0 = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i s1 = _mm512_maskz_loadu_epi32(k, src_argb1);
    __m512i lo = _mm512_mulhrs_epi16(
        _mm512_srli_epi16(_mm512_unpacklo_epi8(zero, s0), 1),
        _mm512_unpacklo_epi8(s1, zero));
    __m512i hi = _mm512_mulhrs_epi16(
        _mm512_srli_epi16(_mm512_unpackhi_epi8(zero, s0), 1),
        _mm512_unpackhi_epi8(s1, zero));
    _mm512_mask_storeu_epi32(dst_argb, k, _mm512_packus_epi16(lo, hi));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBMULTIPLYROW_AVX512BW

#ifdef HAS_ARGBGRAYROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBGrayRow_AVX512BW(const uint8_t* src_argb,
                          uint8_t* dst_argb,
                          int width) {
  static const uint8_t kShufGrayDword[16] = {0, 0, 0, 0x80, 4,  4,  4,  0x80,
                                             8, 8, 8, 0x80, 12, 12, 12, 0x80};
  const __m512i c_coeff = _mm512_set1_epi32(0x004d961d);
  const __m512i c_sub128 = _mm512_set1_epi8((char)0x80);
  const __m512i c_one16 = _mm512_set1_epi16(1);
  const __m512i c_bias = _mm512_set1_epi32(32896);
  const __m512i c_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kShufGrayDword));
  const __m512i c_alpha = _mm512_set1_epi32((int)0xff000000u);
  while (width >= 16) {
    __m512i s = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    __m512i y16 = _mm512_maddubs_epi16(c_coeff, _mm512_sub_epi8(s, c_sub128));
    __m512i y32 = _mm512_srli_epi32(
        _mm512_add_epi32(_mm512_madd_epi16(y16, c_one16), c_bias), 8);
    __m512i out = _mm512_or_si512(_mm512_and_si512(s, c_alpha),
                                  _mm512_shuffle_epi8(y32, c_shuf));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i y16 = _mm512_maddubs_epi16(c_coeff, _mm512_sub_epi8(s, c_sub128));
    __m512i y32 = _mm512_srli_epi32(
        _mm512_add_epi32(_mm512_madd_epi16(y16, c_one16), c_bias), 8);
    __m512i out = _mm512_or_si512(_mm512_and_si512(s, c_alpha),
                                  _mm512_shuffle_epi8(y32, c_shuf));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBGRAYROW_AVX512BW

#ifdef HAS_ARGBGRAYROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBGrayRow_AVX512VNNI(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            int width) {
  static const uint8_t kShufGrayDword[16] = {0, 0, 0, 0x80, 4,  4,  4,  0x80,
                                             8, 8, 8, 0x80, 12, 12, 12, 0x80};
  const __m512i c_coeff = _mm512_set1_epi32(0x004d961d);
  const __m512i c_sub128 = _mm512_set1_epi8((char)0x80);
  const __m512i c_bias = _mm512_set1_epi32(32896);
  const __m512i c_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kShufGrayDword));
  const __m512i c_alpha = _mm512_set1_epi32((int)0xff000000u);
  // Avoid _mm512_ternarylogic_epi32 (unmodeled by LLVM MSan on RGBX inputs;
  // see GEMINI.md).
  while (width >= 16) {
    __m512i s = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    __m512i y32 = _mm512_srli_epi32(
        _mm512_dpbusd_epi32(c_bias, c_coeff, _mm512_xor_si512(s, c_sub128)), 8);
    __m512i out = _mm512_or_si512(_mm512_shuffle_epi8(y32, c_shuf),
                                  _mm512_and_si512(s, c_alpha));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i y32 = _mm512_srli_epi32(
        _mm512_dpbusd_epi32(c_bias, c_coeff, _mm512_xor_si512(s, c_sub128)), 8);
    __m512i out = _mm512_or_si512(_mm512_shuffle_epi8(y32, c_shuf),
                                  _mm512_and_si512(s, c_alpha));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBGRAYROW_AVX512VNNI

#ifdef HAS_ARGBBLENDROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBBlendRow_AVX512BW(const uint8_t* src_argb,
                           const uint8_t* src_argb1,
                           uint8_t* dst_argb,
                           int width) {
  static const uint8_t kShuffleAlpha16[16] = {3u,  0x80, 3u,  0x80, 7u,  0x80,
                                              7u,  0x80, 11u, 0x80, 11u, 0x80,
                                              15u, 0x80, 15u, 0x80};
  const __m512i zmm_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)kShuffleAlpha16));
  const __m512i zmm_alpha = _mm512_set1_epi32((int)0xff000000u);
  const __m512i zmm_ff00 = _mm512_set1_epi16((short)0xff00u);
  const __m512i zmm_00ff = _mm512_set1_epi16(0x00ff);
  const __m512i zmm_0001 = _mm512_set1_epi16(0x0001);
  while (width >= 16) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)src_argb1);
    src_argb += 64;
    src_argb1 += 64;
    __m512i inv_a = _mm512_add_epi16(
        _mm512_shuffle_epi8(_mm512_xor_si512(s0, zmm_alpha), zmm_shuf),
        zmm_0001);
    __m512i even = _mm512_srli_epi16(
        _mm512_mullo_epi16(_mm512_and_si512(s1, zmm_00ff), inv_a), 8);
    __m512i odd = _mm512_and_si512(
        _mm512_mullo_epi16(_mm512_srli_epi16(s1, 8), inv_a), zmm_ff00);
    __m512i out = _mm512_adds_epu8(
        _mm512_adds_epu8(_mm512_or_si512(s0, zmm_alpha), even), odd);
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s0 = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i s1 = _mm512_maskz_loadu_epi32(k, src_argb1);
    __m512i inv_a = _mm512_add_epi16(
        _mm512_shuffle_epi8(_mm512_xor_si512(s0, zmm_alpha), zmm_shuf),
        zmm_0001);
    __m512i even = _mm512_srli_epi16(
        _mm512_mullo_epi16(_mm512_and_si512(s1, zmm_00ff), inv_a), 8);
    __m512i odd = _mm512_and_si512(
        _mm512_mullo_epi16(_mm512_srli_epi16(s1, 8), inv_a), zmm_ff00);
    __m512i out = _mm512_adds_epu8(
        _mm512_adds_epu8(_mm512_or_si512(s0, zmm_alpha), even), odd);
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBBLENDROW_AVX512BW

#ifdef HAS_ARGBORROW_SSE2
void ARGBOrRow_SSE2(const uint8_t* src_argb,
                    uint8_t* dst_argb,
                    uint32_t value,
                    int width) {
  const __m128i val = _mm_set1_epi32((int)value);
  while (width >= 4) {
    __m128i s = _mm_loadu_si128((const __m128i*)src_argb);
    src_argb += 16;
    _mm_storeu_si128((__m128i*)dst_argb, _mm_or_si128(s, val));
    dst_argb += 16;
    width -= 4;
  }
}
#endif  // HAS_ARGBORROW_SSE2

#ifdef HAS_ARGBORROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBOrRow_AVX2(const uint8_t* src_argb,
                    uint8_t* dst_argb,
                    uint32_t value,
                    int width) {
  const __m256i val = _mm256_set1_epi32((int)value);
  while (width >= 8) {
    __m256i s = _mm256_loadu_si256((const __m256i*)src_argb);
    src_argb += 32;
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_or_si256(s, val));
    dst_argb += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBORROW_AVX2

#ifdef HAS_ARGBORROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBOrRow_AVX512BW(const uint8_t* src_argb,
                        uint8_t* dst_argb,
                        uint32_t value,
                        int width) {
  const __m512i val = _mm512_set1_epi32((int)value);
  while (width >= 16) {
    __m512i s = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    _mm512_storeu_si512((__m512i*)dst_argb, _mm512_or_si512(s, val));
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s = _mm512_maskz_loadu_epi32(k, src_argb);
    _mm512_mask_storeu_epi32(dst_argb, k, _mm512_or_si512(s, val));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBORROW_AVX512BW

#ifdef HAS_ARGBALPHABLENDROW_SSSE3
LIBYUV_TARGET_SSSE3
void ARGBAlphaBlendRow_SSSE3(const uint8_t* src_argb0,
                             const uint8_t* src_argb1,
                             uint8_t* dst_argb,
                             int width) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i k0001 = _mm_set1_epi16(0x0001);
  const __m128i k0101 = _mm_set1_epi16(0x0101);
  const __m128i k00ff = _mm_set1_epi16(0x00ff);
  const __m128i k8080 = _mm_set1_epi8((char)0x80);
  const __m128i k7f81 = _mm_set1_epi16(0x7f81);
  const __m128i k00ffffff = _mm_set1_epi32(0x00ffffff);
  while (width >= 4) {
    __m128i s = _mm_loadu_si128((const __m128i*)src_argb0);
    __m128i d = _mm_loadu_si128((const __m128i*)src_argb1);
    src_argb0 += 16;
    src_argb1 += 16;
    __m128i sa = _mm_srli_epi32(s, 24);
    __m128i da = _mm_srli_epi32(d, 24);
    __m128i da_sa = _mm_mulhi_epu16(
        _mm_add_epi16(_mm_mullo_epi16(da, sa), k0001), k0101);
    __m128i da2 = _mm_add_epi32(da, _mm_sub_epi32(sa, da_sa));
    __m128i t = _mm_srli_epi32(_mm_cmpeq_epi32(da, zero), 24);
    __m128i sa_eff = _mm_or_si128(sa, t);
    __m128i den_eff = _mm_or_si128(da2, t);
    __m128i num = _mm_sub_epi32(_mm_slli_epi32(sa_eff, 8), sa_eff);
    __m128i ratio = _mm_cvttps_epi32(
        _mm_div_ps(_mm_cvtepi32_ps(num), _mm_cvtepi32_ps(den_eff)));
    __m128i r_rep = _mm_shufflelo_epi16(_mm_shufflehi_epi16(ratio, 0xa0), 0xa0);
    __m128i w =
        _mm_or_si128(r_rep, _mm_slli_epi16(_mm_sub_epi16(k00ff, r_rep), 8));
    __m128i w_lo = _mm_unpacklo_epi16(w, w);
    __m128i w_hi = _mm_unpackhi_epi16(w, w);
    __m128i sb = _mm_sub_epi8(s, k8080);
    __m128i db = _mm_sub_epi8(d, k8080);
    __m128i lo = _mm_mulhi_epu16(
        _mm_add_epi16(_mm_maddubs_epi16(w_lo, _mm_unpacklo_epi8(sb, db)),
                      k7f81),
        k0101);
    __m128i hi = _mm_mulhi_epu16(
        _mm_add_epi16(_mm_maddubs_epi16(w_hi, _mm_unpackhi_epi8(sb, db)),
                      k7f81),
        k0101);
    __m128i rgb = _mm_packus_epi16(lo, hi);
    __m128i out =
        _mm_or_si128(_mm_and_si128(rgb, k00ffffff), _mm_slli_epi32(da2, 24));
    _mm_storeu_si128((__m128i*)dst_argb, out);
    dst_argb += 16;
    width -= 4;
  }
}
#endif  // HAS_ARGBALPHABLENDROW_SSSE3

#ifdef HAS_ARGBALPHABLENDROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBAlphaBlendRow_AVX2(const uint8_t* src_argb0,
                            const uint8_t* src_argb1,
                            uint8_t* dst_argb,
                            int width) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i k0001 = _mm256_set1_epi16(0x0001);
  const __m256i k0101 = _mm256_set1_epi16(0x0101);
  const __m256i k00ff = _mm256_set1_epi16(0x00ff);
  const __m256i k8080 = _mm256_set1_epi8((char)0x80);
  const __m256i k7f81 = _mm256_set1_epi16(0x7f81);
  const __m256i k00ffffff = _mm256_set1_epi32(0x00ffffff);
  while (width >= 8) {
    __m256i s = _mm256_loadu_si256((const __m256i*)src_argb0);
    __m256i d = _mm256_loadu_si256((const __m256i*)src_argb1);
    src_argb0 += 32;
    src_argb1 += 32;
    __m256i sa = _mm256_srli_epi32(s, 24);
    __m256i da = _mm256_srli_epi32(d, 24);
    __m256i da_sa = _mm256_mulhi_epu16(
        _mm256_add_epi16(_mm256_mullo_epi16(da, sa), k0001), k0101);
    __m256i da2 = _mm256_add_epi32(da, _mm256_sub_epi32(sa, da_sa));
    __m256i t = _mm256_srli_epi32(_mm256_cmpeq_epi32(da, zero), 24);
    __m256i sa_eff = _mm256_or_si256(sa, t);
    __m256i den_eff = _mm256_or_si256(da2, t);
    __m256i num = _mm256_sub_epi32(_mm256_slli_epi32(sa_eff, 8), sa_eff);
    __m256i ratio = _mm256_cvttps_epi32(
        _mm256_div_ps(_mm256_cvtepi32_ps(num), _mm256_cvtepi32_ps(den_eff)));
    __m256i r_rep =
        _mm256_shufflelo_epi16(_mm256_shufflehi_epi16(ratio, 0xa0), 0xa0);
    __m256i w = _mm256_or_si256(
        r_rep, _mm256_slli_epi16(_mm256_sub_epi16(k00ff, r_rep), 8));
    __m256i w_lo = _mm256_unpacklo_epi16(w, w);
    __m256i w_hi = _mm256_unpackhi_epi16(w, w);
    __m256i sb = _mm256_sub_epi8(s, k8080);
    __m256i db = _mm256_sub_epi8(d, k8080);
    __m256i lo = _mm256_mulhi_epu16(
        _mm256_add_epi16(
            _mm256_maddubs_epi16(w_lo, _mm256_unpacklo_epi8(sb, db)), k7f81),
        k0101);
    __m256i hi = _mm256_mulhi_epu16(
        _mm256_add_epi16(
            _mm256_maddubs_epi16(w_hi, _mm256_unpackhi_epi8(sb, db)), k7f81),
        k0101);
    __m256i rgb = _mm256_packus_epi16(lo, hi);
    __m256i out = _mm256_or_si256(_mm256_and_si256(rgb, k00ffffff),
                                  _mm256_slli_epi32(da2, 24));
    _mm256_storeu_si256((__m256i*)dst_argb, out);
    dst_argb += 32;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBALPHABLENDROW_AVX2

#ifdef HAS_ARGBALPHABLENDROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBAlphaBlendRow_AVX512BW(const uint8_t* src_argb0,
                                const uint8_t* src_argb1,
                                uint8_t* dst_argb,
                                int width) {
  const __m512i zero = _mm512_setzero_si512();
  const __m512i k0001 = _mm512_set1_epi16(0x0001);
  const __m512i k0101 = _mm512_set1_epi16(0x0101);
  const __m512i k00ff = _mm512_set1_epi16(0x00ff);
  const __m512i k8080 = _mm512_set1_epi8((char)0x80);
  const __m512i k7f81 = _mm512_set1_epi16(0x7f81);
  const __m512i k00ffffff = _mm512_set1_epi32(0x00ffffff);
  while (width >= 16) {
    __m512i s = _mm512_loadu_si512((const __m512i*)src_argb0);
    __m512i d = _mm512_loadu_si512((const __m512i*)src_argb1);
    src_argb0 += 64;
    src_argb1 += 64;
    __m512i sa = _mm512_srli_epi32(s, 24);
    __m512i da = _mm512_srli_epi32(d, 24);
    __m512i da_sa = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_mullo_epi16(da, sa), k0001), k0101);
    __m512i da2 = _mm512_add_epi32(da, _mm512_sub_epi32(sa, da_sa));
    __mmask16 k_da0 = _mm512_cmpeq_epi32_mask(da, zero);
    __m512i t = _mm512_maskz_set1_epi32(k_da0, 255);
    __m512i sa_eff = _mm512_or_si512(sa, t);
    __m512i den_eff = _mm512_or_si512(da2, t);
    __m512i num = _mm512_sub_epi32(_mm512_slli_epi32(sa_eff, 8), sa_eff);
    __m512i ratio = CvttpsEpi32(
        _mm512_div_ps(_mm512_cvtepi32_ps(num), _mm512_cvtepi32_ps(den_eff)));
    __m512i r_rep =
        _mm512_shufflelo_epi16(_mm512_shufflehi_epi16(ratio, 0xa0), 0xa0);
    __m512i w = _mm512_or_si512(
        r_rep, _mm512_slli_epi16(_mm512_sub_epi16(k00ff, r_rep), 8));
    __m512i w_lo = _mm512_unpacklo_epi16(w, w);
    __m512i w_hi = _mm512_unpackhi_epi16(w, w);
    __m512i sb = _mm512_sub_epi8(s, k8080);
    __m512i db = _mm512_sub_epi8(d, k8080);
    __m512i lo = _mm512_mulhi_epu16(
        _mm512_add_epi16(
            _mm512_maddubs_epi16(w_lo, _mm512_unpacklo_epi8(sb, db)), k7f81),
        k0101);
    __m512i hi = _mm512_mulhi_epu16(
        _mm512_add_epi16(
            _mm512_maddubs_epi16(w_hi, _mm512_unpackhi_epi8(sb, db)), k7f81),
        k0101);
    __m512i rgb = _mm512_packus_epi16(lo, hi);
    __m512i out = _mm512_or_si512(_mm512_and_si512(rgb, k00ffffff),
                                  _mm512_slli_epi32(da2, 24));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s = _mm512_maskz_loadu_epi32(k, src_argb0);
    __m512i d = _mm512_maskz_loadu_epi32(k, src_argb1);
    __m512i sa = _mm512_srli_epi32(s, 24);
    __m512i da = _mm512_srli_epi32(d, 24);
    __m512i da_sa = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_mullo_epi16(da, sa), k0001), k0101);
    __m512i da2 = _mm512_add_epi32(da, _mm512_sub_epi32(sa, da_sa));
    __mmask16 k_da0 = _mm512_cmpeq_epi32_mask(da, zero);
    __m512i t = _mm512_maskz_set1_epi32(k_da0, 255);
    __m512i sa_eff = _mm512_or_si512(sa, t);
    __m512i den_eff = _mm512_or_si512(da2, t);
    __m512i num = _mm512_sub_epi32(_mm512_slli_epi32(sa_eff, 8), sa_eff);
    __m512i ratio = CvttpsEpi32(
        _mm512_div_ps(_mm512_cvtepi32_ps(num), _mm512_cvtepi32_ps(den_eff)));
    __m512i r_rep =
        _mm512_shufflelo_epi16(_mm512_shufflehi_epi16(ratio, 0xa0), 0xa0);
    __m512i w = _mm512_or_si512(
        r_rep, _mm512_slli_epi16(_mm512_sub_epi16(k00ff, r_rep), 8));
    __m512i w_lo = _mm512_unpacklo_epi16(w, w);
    __m512i w_hi = _mm512_unpackhi_epi16(w, w);
    __m512i sb = _mm512_sub_epi8(s, k8080);
    __m512i db = _mm512_sub_epi8(d, k8080);
    __m512i lo = _mm512_mulhi_epu16(
        _mm512_add_epi16(
            _mm512_maddubs_epi16(w_lo, _mm512_unpacklo_epi8(sb, db)), k7f81),
        k0101);
    __m512i hi = _mm512_mulhi_epu16(
        _mm512_add_epi16(
            _mm512_maddubs_epi16(w_hi, _mm512_unpackhi_epi8(sb, db)), k7f81),
        k0101);
    __m512i rgb = _mm512_packus_epi16(lo, hi);
    __m512i out = _mm512_or_si512(_mm512_and_si512(rgb, k00ffffff),
                                  _mm512_slli_epi32(da2, 24));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBALPHABLENDROW_AVX512BW

// ===========================================================================
// Module 6: AVX2 Row Functions (Minimum AVX2 Coverage)
// ===========================================================================

#ifdef HAS_ARGBSEPIAROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBSepiaRow_AVX2(uint8_t* dst_argb, int width) {
  static const int8_t kSepiaB[16] = {17, 68, 35, 0, 17, 68, 35, 0,
                                     17, 68, 35, 0, 17, 68, 35, 0};
  static const int8_t kSepiaG[16] = {22, 88, 45, 0, 22, 88, 45, 0,
                                     22, 88, 45, 0, 22, 88, 45, 0};
  static const int8_t kSepiaR[16] = {24, 98, 50, 0, 24, 98, 50, 0,
                                     24, 98, 50, 0, 24, 98, 50, 0};
  if (width >= 8) {
    const __m256i vb =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)kSepiaB));
    const __m256i vg =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)kSepiaG));
    const __m256i vr =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)kSepiaR));
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)dst_argb);
      __m256i b = _mm256_maddubs_epi16(v0, vb);
      __m256i g = _mm256_maddubs_epi16(v0, vg);
      __m256i r = _mm256_maddubs_epi16(v0, vr);
      __m256i bg = _mm256_srli_epi16(_mm256_hadd_epi16(b, g), 7);
      __m256i rr = _mm256_srli_epi16(_mm256_hadd_epi16(r, r), 7);
      bg = _mm256_packus_epi16(bg, bg);
      rr = _mm256_packus_epi16(rr, rr);
      bg = _mm256_unpacklo_epi8(bg, _mm256_shuffle_epi32(bg, 0x01));
      __m256i a = _mm256_srli_epi32(v0, 24);
      a = _mm256_packus_epi16(a, a);
      a = _mm256_packus_epi16(a, a);
      __m256i ra = _mm256_unpacklo_epi8(rr, a);
      __m256i out = _mm256_unpacklo_epi16(bg, ra);
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBSepiaRow_C(dst_argb, width);
  }
}
#endif  // HAS_ARGBSEPIAROW_AVX2

#ifdef HAS_ARGBCOLORMATRIXROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBColorMatrixRow_AVX2(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             const int8_t* matrix_argb,
                             int width) {
  static const int8_t kShufTranspose4x4[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                               2, 6, 10, 14, 3, 7, 11, 15};
  if (width >= 8) {
    const __m256i m = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i*)matrix_argb));
    const __m256i shuf = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i*)kShufTranspose4x4));
    const __m256i m0 = _mm256_shuffle_epi32(m, 0x00);
    const __m256i m1 = _mm256_shuffle_epi32(m, 0x55);
    const __m256i m2 = _mm256_shuffle_epi32(m, 0xaa);
    const __m256i m3 = _mm256_shuffle_epi32(m, 0xff);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)src_argb);
      src_argb += 32;
      __m256i p01 = _mm256_hadds_epi16(_mm256_maddubs_epi16(v0, m0),
                                       _mm256_maddubs_epi16(v0, m1));
      __m256i p23 = _mm256_hadds_epi16(_mm256_maddubs_epi16(v0, m2),
                                       _mm256_maddubs_epi16(v0, m3));
      p01 = _mm256_srai_epi16(p01, 6);
      p23 = _mm256_srai_epi16(p23, 6);
      __m256i out = _mm256_shuffle_epi8(_mm256_packus_epi16(p01, p23), shuf);
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBColorMatrixRow_C(src_argb, dst_argb, matrix_argb, width);
  }
}
#endif  // HAS_ARGBCOLORMATRIXROW_AVX2

#ifdef HAS_ARGBQUANTIZEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBQuantizeRow_AVX2(uint8_t* dst_argb,
                          int scale,
                          int interval_size,
                          int interval_offset,
                          int width) {
  if (width >= 8) {
    const __m256i v_scale = _mm256_set1_epi16((short)scale);
    const __m256i v_size = _mm256_set1_epi16((short)interval_size);
    const __m256i v_off = _mm256_set1_epi16((short)interval_offset);
    const __m256i zero = _mm256_setzero_si256();
    const __m256i alpha_mask = _mm256_set1_epi32((int)0xff000000u);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)dst_argb);
      __m256i a = _mm256_and_si256(v0, alpha_mask);
      __m256i lo = _mm256_unpacklo_epi8(v0, zero);
      __m256i hi = _mm256_unpackhi_epi8(v0, zero);
      lo = _mm256_add_epi16(
          _mm256_mullo_epi16(_mm256_mulhi_epu16(lo, v_scale), v_size), v_off);
      hi = _mm256_add_epi16(
          _mm256_mullo_epi16(_mm256_mulhi_epu16(hi, v_scale), v_size), v_off);
      __m256i out = _mm256_or_si256(
          _mm256_andnot_si256(alpha_mask, _mm256_packus_epi16(lo, hi)), a);
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBQuantizeRow_C(dst_argb, scale, interval_size, interval_offset, width);
  }
}
#endif  // HAS_ARGBQUANTIZEROW_AVX2

#ifdef HAS_ARGBSHADEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBShadeRow_AVX2(const uint8_t* src_argb,
                       uint8_t* dst_argb,
                       int width,
                       uint32_t value) {
  if (width >= 8) {
    __m256i v_val = _mm256_set1_epi32((int)value);
    v_val = _mm256_unpacklo_epi8(v_val, v_val);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)src_argb);
      src_argb += 32;
      __m256i lo = _mm256_unpacklo_epi8(v0, v0);
      __m256i hi = _mm256_unpackhi_epi8(v0, v0);
      lo = _mm256_srli_epi16(_mm256_mulhi_epu16(lo, v_val), 8);
      hi = _mm256_srli_epi16(_mm256_mulhi_epu16(hi, v_val), 8);
      _mm256_storeu_si256((__m256i*)dst_argb, _mm256_packus_epi16(lo, hi));
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBShadeRow_C(src_argb, dst_argb, width, value);
  }
}
#endif  // HAS_ARGBSHADEROW_AVX2

#ifdef HAS_SOBELXROW_AVX2
LIBYUV_TARGET_AVX2
void SobelXRow_AVX2(const uint8_t* src_y0,
                    const uint8_t* src_y1,
                    const uint8_t* src_y2,
                    uint8_t* dst_sobelx,
                    int width) {
  while (width > 0) {
    __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_y0));
    __m256i b0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y0 + 2)));
    __m256i a1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_y1));
    __m256i b1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y1 + 2)));
    __m256i a2 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_y2));
    __m256i b2 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y2 + 2)));
    __m256i d0 = _mm256_sub_epi16(a0, b0);
    __m256i d1 = _mm256_sub_epi16(a1, b1);
    __m256i d2 = _mm256_sub_epi16(a2, b2);
    __m256i sum =
        _mm256_add_epi16(_mm256_add_epi16(d0, d2), _mm256_add_epi16(d1, d1));
    sum = _mm256_abs_epi16(sum);
    sum = _mm256_permute4x64_epi64(_mm256_packus_epi16(sum, sum), 0xd8);
    _mm_storeu_si128((__m128i*)dst_sobelx, _mm256_castsi256_si128(sum));
    src_y0 += 16;
    src_y1 += 16;
    src_y2 += 16;
    dst_sobelx += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SOBELXROW_AVX2

#ifdef HAS_SOBELYROW_AVX2
LIBYUV_TARGET_AVX2
void SobelYRow_AVX2(const uint8_t* src_y0,
                    const uint8_t* src_y1,
                    uint8_t* dst_sobely,
                    int width) {
  while (width > 0) {
    __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_y0));
    __m256i b0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_y1));
    __m256i a1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y0 + 1)));
    __m256i b1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y1 + 1)));
    __m256i a2 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y0 + 2)));
    __m256i b2 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_y1 + 2)));
    __m256i d0 = _mm256_sub_epi16(a0, b0);
    __m256i d1 = _mm256_sub_epi16(a1, b1);
    __m256i d2 = _mm256_sub_epi16(a2, b2);
    __m256i sum =
        _mm256_add_epi16(_mm256_add_epi16(d0, d2), _mm256_add_epi16(d1, d1));
    sum = _mm256_abs_epi16(sum);
    sum = _mm256_permute4x64_epi64(_mm256_packus_epi16(sum, sum), 0xd8);
    _mm_storeu_si128((__m128i*)dst_sobely, _mm256_castsi256_si128(sum));
    src_y0 += 16;
    src_y1 += 16;
    dst_sobely += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SOBELYROW_AVX2

#ifdef HAS_SOBELROW_AVX2
LIBYUV_TARGET_AVX2
void SobelRow_AVX2(const uint8_t* src_sobelx,
                   const uint8_t* src_sobely,
                   uint8_t* dst_argb,
                   int width) {
  const __m256i v_alpha = _mm256_set1_epi32((int)0xff000000u);
  while (width > 0) {
    __m128i vx = _mm_loadu_si128((const __m128i*)src_sobelx);
    __m128i vy = _mm_loadu_si128((const __m128i*)src_sobely);
    __m256i s16 = _mm256_cvtepu8_epi16(_mm_adds_epu8(vx, vy));
    s16 = _mm256_or_si256(s16, _mm256_slli_epi16(s16, 8));
    s16 = _mm256_permute4x64_epi64(s16, 0xd8);
    __m256i p0 = _mm256_or_si256(_mm256_unpacklo_epi16(s16, s16), v_alpha);
    __m256i p1 = _mm256_or_si256(_mm256_unpackhi_epi16(s16, s16), v_alpha);
    _mm256_storeu_si256((__m256i*)dst_argb, p0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), p1);
    src_sobelx += 16;
    src_sobely += 16;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SOBELROW_AVX2

#ifdef HAS_SOBELTOPLANEROW_AVX2
LIBYUV_TARGET_AVX2
void SobelToPlaneRow_AVX2(const uint8_t* src_sobelx,
                          const uint8_t* src_sobely,
                          uint8_t* dst_y,
                          int width) {
  while (width > 0) {
    __m256i vx = _mm256_loadu_si256((const __m256i*)src_sobelx);
    __m256i vy = _mm256_loadu_si256((const __m256i*)src_sobely);
    _mm256_storeu_si256((__m256i*)dst_y, _mm256_adds_epu8(vx, vy));
    src_sobelx += 32;
    src_sobely += 32;
    dst_y += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SOBELTOPLANEROW_AVX2

#ifdef HAS_SOBELXYROW_AVX2
LIBYUV_TARGET_AVX2
void SobelXYRow_AVX2(const uint8_t* src_sobelx,
                     const uint8_t* src_sobely,
                     uint8_t* dst_argb,
                     int width) {
  const __m256i v_ff00 = _mm256_set1_epi16((short)0xff00u);
  while (width > 0) {
    __m128i vx = _mm_loadu_si128((const __m128i*)src_sobelx);
    __m128i vy = _mm_loadu_si128((const __m128i*)src_sobely);
    __m128i vs = _mm_adds_epu8(vx, vy);
    __m256i bg =
        _mm256_or_si256(_mm256_cvtepu8_epi16(vy),
                        _mm256_slli_epi16(_mm256_cvtepu8_epi16(vs), 8));
    __m256i ra = _mm256_or_si256(_mm256_cvtepu8_epi16(vx), v_ff00);
    bg = _mm256_permute4x64_epi64(bg, 0xd8);
    ra = _mm256_permute4x64_epi64(ra, 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_unpacklo_epi16(bg, ra));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_unpackhi_epi16(bg, ra));
    src_sobelx += 16;
    src_sobely += 16;
    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_SOBELXYROW_AVX2

#ifdef HAS_ARGBAFFINEROW_AVX2
LIBYUV_API
LIBYUV_TARGET_AVX2
void ARGBAffineRow_AVX2(const uint8_t* src_argb,
                        int src_argb_stride,
                        uint8_t* dst_argb,
                        const float* src_dudv,
                        int width) {
  static const float kRamp[8] = {0.0f, 1.0f, 2.0f, 3.0f,
                                 4.0f, 5.0f, 6.0f, 7.0f};
  float uv_tail[4] = {src_dudv[0], src_dudv[1], src_dudv[2], src_dudv[3]};
  if (width >= 8) {
    const __m256 ramp = _mm256_loadu_ps(kRamp);
    const __m256 step8 = _mm256_set1_ps(8.0f);
    __m256 du = _mm256_set1_ps(uv_tail[2]);
    __m256 dv = _mm256_set1_ps(uv_tail[3]);
    __m256 u =
        _mm256_add_ps(_mm256_set1_ps(uv_tail[0]), _mm256_mul_ps(ramp, du));
    __m256 v =
        _mm256_add_ps(_mm256_set1_ps(uv_tail[1]), _mm256_mul_ps(ramp, dv));
    du = _mm256_mul_ps(du, step8);
    dv = _mm256_mul_ps(dv, step8);
    const __m256i v_stride = _mm256_set1_epi32(src_argb_stride);
    while (width >= 8) {
      __m256i iu = _mm256_cvttps_epi32(u);
      __m256i iv = _mm256_cvttps_epi32(v);
      __m256i idx = _mm256_add_epi32(_mm256_slli_epi32(iu, 2),
                                     _mm256_mullo_epi32(iv, v_stride));
      __m256i pix = _mm256_i32gather_epi32((const int*)src_argb, idx, 1);
      _mm256_storeu_si256((__m256i*)dst_argb, pix);
      u = _mm256_add_ps(u, du);
      v = _mm256_add_ps(v, dv);
      dst_argb += 32;
      width -= 8;
    }
    uv_tail[0] = _mm256_cvtss_f32(u);
    uv_tail[1] = _mm256_cvtss_f32(v);
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBAffineRow_C(src_argb, src_argb_stride, dst_argb, uv_tail, width);
  }
}
#endif  // HAS_ARGBAFFINEROW_AVX2

#ifdef HAS_ARGBCOLORTABLEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBColorTableRow_AVX2(uint8_t* dst_argb,
                            const uint8_t* table_argb,
                            int width) {
  if (width >= 8) {
    const __m256i m0 = _mm256_set1_epi32(0x000000ff);
    const __m256i m1 = _mm256_set1_epi32(0x0000ff00);
    const __m256i m2 = _mm256_set1_epi32(0x00ff0000);
    const __m256i m3 = _mm256_set1_epi32((int)0xff000000u);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)dst_argb);
      __m256i c0 =
          _mm256_and_si256(_mm256_i32gather_epi32((const int*)table_argb,
                                                  _mm256_and_si256(v0, m0), 4),
                           m0);
      __m256i c1 = _mm256_and_si256(
          _mm256_i32gather_epi32((const int*)table_argb,
                                 _mm256_and_si256(_mm256_srli_epi32(v0, 8), m0),
                                 4),
          m1);
      __m256i c2 = _mm256_and_si256(
          _mm256_i32gather_epi32(
              (const int*)table_argb,
              _mm256_and_si256(_mm256_srli_epi32(v0, 16), m0), 4),
          m2);
      __m256i c3 =
          _mm256_and_si256(_mm256_i32gather_epi32((const int*)table_argb,
                                                  _mm256_srli_epi32(v0, 24), 4),
                           m3);
      __m256i out =
          _mm256_or_si256(_mm256_or_si256(c0, c1), _mm256_or_si256(c2, c3));
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBColorTableRow_C(dst_argb, table_argb, width);
  }
}
#endif  // HAS_ARGBCOLORTABLEROW_AVX2

#ifdef HAS_RGBCOLORTABLEROW_AVX2
LIBYUV_TARGET_AVX2
void RGBColorTableRow_AVX2(uint8_t* dst_argb,
                           const uint8_t* table_argb,
                           int width) {
  if (width >= 8) {
    const __m256i m0 = _mm256_set1_epi32(0x000000ff);
    const __m256i m1 = _mm256_set1_epi32(0x0000ff00);
    const __m256i m2 = _mm256_set1_epi32(0x00ff0000);
    const __m256i m3 = _mm256_set1_epi32((int)0xff000000u);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)dst_argb);
      __m256i c0 =
          _mm256_and_si256(_mm256_i32gather_epi32((const int*)table_argb,
                                                  _mm256_and_si256(v0, m0), 4),
                           m0);
      __m256i c1 = _mm256_and_si256(
          _mm256_i32gather_epi32((const int*)table_argb,
                                 _mm256_and_si256(_mm256_srli_epi32(v0, 8), m0),
                                 4),
          m1);
      __m256i c2 = _mm256_and_si256(
          _mm256_i32gather_epi32(
              (const int*)table_argb,
              _mm256_and_si256(_mm256_srli_epi32(v0, 16), m0), 4),
          m2);
      __m256i c3 = _mm256_and_si256(v0, m3);
      __m256i out =
          _mm256_or_si256(_mm256_or_si256(c0, c1), _mm256_or_si256(c2, c3));
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    RGBColorTableRow_C(dst_argb, table_argb, width);
  }
}
#endif  // HAS_RGBCOLORTABLEROW_AVX2

#ifdef HAS_ARGBLUMACOLORTABLEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBLumaColorTableRow_AVX2(const uint8_t* src_argb,
                                uint8_t* dst_argb,
                                int width,
                                const uint8_t* luma,
                                uint32_t lumacoeff) {
  if (width >= 8) {
    const __m256i c_br = _mm256_set1_epi32((int)(lumacoeff & 0x00ff00ffu));
    const __m256i c_ga =
        _mm256_set1_epi32((int)((lumacoeff >> 8) & 0x000000ffu));
    const __m256i m_00ff = _mm256_set1_epi16(0x00ff);
    const __m256i m_ff = _mm256_set1_epi32(0x000000ff);
    const __m256i m_7f00 = _mm256_set1_epi32(0x00007f00);
    const __m256i m_18 = _mm256_set1_epi32(0x00000018);
    const __m256i m_alpha = _mm256_set1_epi32((int)0xff000000u);
    while (width >= 8) {
      __m256i v0 = _mm256_loadu_si256((const __m256i*)src_argb);
      src_argb += 32;
      __m256i y = _mm256_add_epi32(
          _mm256_madd_epi16(_mm256_and_si256(v0, m_00ff), c_br),
          _mm256_madd_epi16(_mm256_srli_epi16(v0, 8), c_ga));
      y = _mm256_and_si256(y, m_7f00);

      __m256i b_idx =
          _mm256_srli_epi32(_mm256_or_si256(_mm256_and_si256(v0, m_ff), y), 2);
      __m256i b_val = _mm256_i32gather_epi32((const int*)luma, b_idx, 4);
      __m256i b_sh = _mm256_and_si256(_mm256_slli_epi32(v0, 3), m_18);
      b_val = _mm256_and_si256(_mm256_srlv_epi32(b_val, b_sh), m_ff);

      __m256i g_idx = _mm256_srli_epi32(
          _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(v0, 8), m_ff), y),
          2);
      __m256i g_val = _mm256_i32gather_epi32((const int*)luma, g_idx, 4);
      __m256i g_sh = _mm256_and_si256(_mm256_srli_epi32(v0, 5), m_18);
      g_val = _mm256_slli_epi32(
          _mm256_and_si256(_mm256_srlv_epi32(g_val, g_sh), m_ff), 8);

      __m256i r_idx = _mm256_srli_epi32(
          _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(v0, 16), m_ff), y),
          2);
      __m256i r_val = _mm256_i32gather_epi32((const int*)luma, r_idx, 4);
      __m256i r_sh = _mm256_and_si256(_mm256_srli_epi32(v0, 13), m_18);
      r_val = _mm256_slli_epi32(
          _mm256_and_si256(_mm256_srlv_epi32(r_val, r_sh), m_ff), 16);

      __m256i a_val = _mm256_and_si256(v0, m_alpha);
      __m256i out = _mm256_or_si256(_mm256_or_si256(b_val, g_val),
                                    _mm256_or_si256(r_val, a_val));
      _mm256_storeu_si256((__m256i*)dst_argb, out);
      dst_argb += 32;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBLumaColorTableRow_C(src_argb, dst_argb, width, luma, lumacoeff);
  }
}
#endif  // HAS_ARGBLUMACOLORTABLEROW_AVX2

#ifdef HAS_BYTETOFLOATROW_AVX2
LIBYUV_TARGET_AVX2
void ByteToFloatRow_AVX2(const uint8_t* src,
                         float* dst,
                         float scale,
                         int width) {
  const __m256 v_scale = _mm256_set1_ps(scale);
  while (width > 0) {
    __m256i i0 = _mm256_cvtepu8_epi32(_mm_loadl_epi64((const __m128i*)src));
    __m256i i1 =
        _mm256_cvtepu8_epi32(_mm_loadl_epi64((const __m128i*)(src + 8)));
    _mm256_storeu_ps(dst, _mm256_mul_ps(_mm256_cvtepi32_ps(i0), v_scale));
    _mm256_storeu_ps(dst + 8, _mm256_mul_ps(_mm256_cvtepi32_ps(i1), v_scale));
    src += 16;
    dst += 16;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_BYTETOFLOATROW_AVX2

#ifdef HAS_GAUSSROW_AVX2
LIBYUV_TARGET_AVX2
void GaussRow_AVX2(const uint32_t* src, uint16_t* dst, int width) {
  static const int8_t kShuf32To16[16] = {0,  1,  4,  5,  8,  9,  12, 13,
                                         -1, -1, -1, -1, -1, -1, -1, -1};
  if (width >= 8) {
    const __m256i v_128 = _mm256_set1_epi32(128);
    const __m256i v_shuf = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i*)kShuf32To16));
    while (width >= 8) {
      __m256i s0 = _mm256_loadu_si256((const __m256i*)src);
      __m256i s1 = _mm256_loadu_si256((const __m256i*)(src + 1));
      __m256i s2 = _mm256_loadu_si256((const __m256i*)(src + 2));
      __m256i s3 = _mm256_loadu_si256((const __m256i*)(src + 3));
      __m256i s4 = _mm256_loadu_si256((const __m256i*)(src + 4));
      src += 8;
      __m256i sum04 = _mm256_add_epi32(s0, s4);
      __m256i sum13 = _mm256_slli_epi32(_mm256_add_epi32(s1, s3), 2);
      __m256i sum2 =
          _mm256_add_epi32(_mm256_slli_epi32(s2, 2), _mm256_slli_epi32(s2, 1));
      __m256i acc =
          _mm256_srli_epi32(_mm256_add_epi32(_mm256_add_epi32(sum04, sum13),
                                             _mm256_add_epi32(sum2, v_128)),
                            8);
      acc = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(acc, v_shuf), 0xd8);
      _mm_storeu_si128((__m128i*)dst, _mm256_castsi256_si128(acc));
      dst += 8;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    GaussRow_C(src, dst, width);
  }
}
#endif  // HAS_GAUSSROW_AVX2

#ifdef HAS_GAUSSROW_F32_AVX2
LIBYUV_TARGET_AVX2
void GaussRow_F32_AVX2(const float* src, float* dst, int width) {
  if (width >= 8) {
    const __m256 v4 = _mm256_set1_ps(4.0f);
    const __m256 v6 = _mm256_set1_ps(6.0f);
    const __m256 v_scale = _mm256_set1_ps(1.0f / 256.0f);
    while (width >= 8) {
      __m256 acc = _mm256_loadu_ps(src);
      acc = _mm256_add_ps(acc, _mm256_mul_ps(_mm256_loadu_ps(src + 1), v4));
      acc = _mm256_add_ps(acc, _mm256_mul_ps(_mm256_loadu_ps(src + 2), v6));
      acc = _mm256_add_ps(acc, _mm256_mul_ps(_mm256_loadu_ps(src + 3), v4));
      acc = _mm256_add_ps(acc, _mm256_loadu_ps(src + 4));
      acc = _mm256_mul_ps(acc, v_scale);
      _mm256_storeu_ps(dst, acc);
      src += 8;
      dst += 8;
      width -= 8;
    }
    _mm256_zeroupper();
  }
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    GaussRow_F32_C(src, dst, width);
  }
}
#endif  // HAS_GAUSSROW_F32_AVX2

#ifdef HAS_SETROW_AVX2
LIBYUV_TARGET_AVX2
void SetRow_AVX2(uint8_t* dst, uint8_t v8, int width) {
  const __m256i v = _mm256_set1_epi8((char)v8);
  while (width >= 64) {
    _mm256_storeu_si256((__m256i*)dst, v);
    _mm256_storeu_si256((__m256i*)(dst + 32), v);
    dst += 64;
    width -= 64;
  }
  if (width > 0) {
    _mm256_storeu_si256((__m256i*)dst, v);
  }
  _mm256_zeroupper();
}
#endif  // HAS_SETROW_AVX2

#ifdef HAS_ARGBSETROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBSetRow_AVX2(uint8_t* dst_argb, uint32_t v32, int width) {
  const __m256i v = _mm256_set1_epi32((int)v32);
  while (width >= 16) {
    _mm256_storeu_si256((__m256i*)dst_argb, v);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), v);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    _mm256_storeu_si256((__m256i*)dst_argb, v);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBSETROW_AVX2

#ifdef HAS_COPYROW_16_AVX2
LIBYUV_TARGET_AVX2
void CopyRow_16_AVX2(const uint16_t* src, uint16_t* dst, int width) {
  while (width > 0) {
    __m256i v0 = _mm256_loadu_si256((const __m256i*)src);
    __m256i v1 = _mm256_loadu_si256((const __m256i*)(src + 16));
    _mm256_storeu_si256((__m256i*)dst, v0);
    _mm256_storeu_si256((__m256i*)(dst + 16), v1);
    src += 32;
    dst += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_COPYROW_16_AVX2

#ifdef HAS_DETILEROW_AVX2
LIBYUV_TARGET_AVX2
void DetileRow_AVX2(const uint8_t* src,
                    ptrdiff_t src_tile_stride,
                    uint8_t* dst,
                    int width) {
  while (width >= 32) {
    __m128i t0 = _mm_loadu_si128((const __m128i*)src);
    __m128i t1 = _mm_loadu_si128((const __m128i*)(src + src_tile_stride));
    src += 2 * src_tile_stride;
    _mm256_storeu_si256((__m256i*)dst, _mm256_set_m128i(t1, t0));
    dst += 32;
    width -= 32;
  }
  if (width > 0) {
    _mm_storeu_si128((__m128i*)dst, _mm_loadu_si128((const __m128i*)src));
  }
  _mm256_zeroupper();
}
#endif  // HAS_DETILEROW_AVX2

#ifdef HAS_DETILEROW_16_AVX2
LIBYUV_TARGET_AVX2
void DetileRow_16_AVX2(const uint16_t* src,
                       ptrdiff_t src_tile_stride,
                       uint16_t* dst,
                       int width) {
  while (width >= 32) {
    __m256i t0 = _mm256_loadu_si256((const __m256i*)src);
    __m256i t1 = _mm256_loadu_si256((const __m256i*)(src + src_tile_stride));
    src += 2 * src_tile_stride;
    _mm256_storeu_si256((__m256i*)dst, t0);
    _mm256_storeu_si256((__m256i*)(dst + 16), t1);
    dst += 32;
    width -= 32;
  }
  if (width > 0) {
    _mm256_storeu_si256((__m256i*)dst, _mm256_loadu_si256((const __m256i*)src));
  }
  _mm256_zeroupper();
}
#endif  // HAS_DETILEROW_16_AVX2

#ifdef HAS_DETILESPLITUVROW_AVX2
LIBYUV_TARGET_AVX2
void DetileSplitUVRow_AVX2(const uint8_t* src_uv,
                           ptrdiff_t src_tile_stride,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width) {
  static const uint8_t kDeinterlaceUV_Win[16] = {0, 2, 4, 6, 8, 10, 12, 14,
                                                 1, 3, 5, 7, 9, 11, 13, 15};
  const __m128i shuf128 = _mm_loadu_si128((const __m128i*)kDeinterlaceUV_Win);
  const __m256i shuf256 = _mm256_broadcastsi128_si256(shuf128);
  while (width >= 32) {
    __m128i t0 = _mm_loadu_si128((const __m128i*)src_uv);
    __m128i t1 = _mm_loadu_si128((const __m128i*)(src_uv + src_tile_stride));
    src_uv += 2 * src_tile_stride;
    __m256i v = _mm256_permute4x64_epi64(
        _mm256_shuffle_epi8(_mm256_set_m128i(t1, t0), shuf256), 0xd8);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(v));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_extracti128_si256(v, 1));
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    __m128i v =
        _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)src_uv), shuf128);
    _mm_storel_epi64((__m128i*)dst_u, v);
    _mm_storel_epi64((__m128i*)dst_v, _mm_unpackhi_epi64(v, v));
  }
  _mm256_zeroupper();
}
#endif  // HAS_DETILESPLITUVROW_AVX2

#if defined(__x86_64__) || defined(_M_X64)

#ifdef HAS_ARGB1555TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGB1555ToARGBRow_AVX512BW(const uint8_t* src_argb1555,
                                uint8_t* dst_argb,
                                int width) {
  const __m512i zmm_f800 = _mm512_set1_epi16((short)0xf800);
  const __m512i zmm_03e0 = _mm512_set1_epi16(0x03e0);
  const __m512i zmm_ff00 = _mm512_set1_epi16((short)0xff00);
  const __m512i zmm_0108 = _mm512_set1_epi16(0x0108);
  const __m512i zmm_4200 = _mm512_set1_epi16(0x4200);
  while (width >= 32) {
    __m512i v0 = _mm512_loadu_si512((const __m512i*)src_argb1555);
    src_argb1555 += 64;
    __m512i r = _mm512_mulhi_epu16(
        _mm512_and_si512(_mm512_slli_epi16(v0, 1), zmm_f800), zmm_0108);
    __m512i b = _mm512_mulhi_epu16(_mm512_slli_epi16(v0, 11), zmm_0108);
    __m512i rb = _mm512_or_si512(_mm512_slli_epi16(r, 8), b);
    __m512i g = _mm512_mulhi_epu16(_mm512_and_si512(v0, zmm_03e0), zmm_4200);
    __m512i a = _mm512_and_si512(_mm512_srai_epi16(v0, 8), zmm_ff00);
    __m512i ga = _mm512_or_si512(g, a);
    __m512i lo = _mm512_unpacklo_epi8(rb, ga);
    __m512i hi = _mm512_unpackhi_epi8(rb, ga);
    __m512i out0 = _mm512_shuffle_i64x2(lo, hi, 0x44);
    __m512i out1 = _mm512_shuffle_i64x2(lo, hi, 0xee);
    out0 = _mm512_shuffle_i64x2(out0, out0, 0xd8);
    out1 = _mm512_shuffle_i64x2(out1, out1, 0xd8);
    _mm512_storeu_si512((__m512i*)dst_argb, out0);
    _mm512_storeu_si512((__m512i*)(dst_argb + 64), out1);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1ULL << width) - 1ULL);
    __m512i v0 = _mm512_maskz_loadu_epi16(k, src_argb1555);
    __m512i r = _mm512_mulhi_epu16(
        _mm512_and_si512(_mm512_slli_epi16(v0, 1), zmm_f800), zmm_0108);
    __m512i b = _mm512_mulhi_epu16(_mm512_slli_epi16(v0, 11), zmm_0108);
    __m512i rb = _mm512_or_si512(_mm512_slli_epi16(r, 8), b);
    __m512i g = _mm512_mulhi_epu16(_mm512_and_si512(v0, zmm_03e0), zmm_4200);
    __m512i a = _mm512_and_si512(_mm512_srai_epi16(v0, 8), zmm_ff00);
    __m512i ga = _mm512_or_si512(g, a);
    __m512i lo = _mm512_unpacklo_epi8(rb, ga);
    __m512i hi = _mm512_unpackhi_epi8(rb, ga);
    __m512i out0 = _mm512_shuffle_i64x2(lo, hi, 0x44);
    __m512i out1 = _mm512_shuffle_i64x2(lo, hi, 0xee);
    out0 = _mm512_shuffle_i64x2(out0, out0, 0xd8);
    out1 = _mm512_shuffle_i64x2(out1, out1, 0xd8);
    if (width > 16) {
      _mm512_storeu_si512((__m512i*)dst_argb, out0);
      _mm512_mask_storeu_epi32(dst_argb + 64, (__mmask16)(k >> 16), out1);
    } else {
      _mm512_mask_storeu_epi32(dst_argb, (__mmask16)k, out0);
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGB1555TOARGBROW_AVX512BW

#ifdef HAS_ARGBTORGB565DITHERROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToRGB565DitherRow_AVX512BW(const uint8_t* src_argb,
                                    uint8_t* dst_rgb,
                                    uint32_t dither4,
                                    int width) {
  __m128i d128 = _mm_cvtsi32_si128((int)dither4);
  d128 = _mm_unpacklo_epi8(d128, d128);
  d128 = _mm_unpacklo_epi16(d128, d128);
  const __m512i zmm_dither = _mm512_broadcast_i32x4(d128);
  const __m512i zmm_001f = _mm512_set1_epi32(0x0000001f);
  const __m512i zmm_07e0 = _mm512_set1_epi32(0x000007e0);
  const __m512i zmm_f800 = _mm512_set1_epi32(0x0000f800);
  while (width >= 16) {
    __m512i v0 = _mm512_adds_epu8(_mm512_loadu_si512((const __m512i*)src_argb),
                                  zmm_dither);
    src_argb += 64;
    __m512i b = _mm512_and_si512(_mm512_srli_epi32(v0, 3), zmm_001f);
    __m512i g = _mm512_and_si512(_mm512_srli_epi32(v0, 5), zmm_07e0);
    __m512i r = _mm512_and_si512(_mm512_srli_epi32(v0, 8), zmm_f800);
    __m256i rgb16 =
        _mm512_cvtepi32_epi16(_mm512_or_si512(_mm512_or_si512(b, g), r));
    _mm256_storeu_si256((__m256i*)dst_rgb, rgb16);
    dst_rgb += 32;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v0 =
        _mm512_adds_epu8(_mm512_maskz_loadu_epi32(k, src_argb), zmm_dither);
    __m512i b = _mm512_and_si512(_mm512_srli_epi32(v0, 3), zmm_001f);
    __m512i g = _mm512_and_si512(_mm512_srli_epi32(v0, 5), zmm_07e0);
    __m512i r = _mm512_and_si512(_mm512_srli_epi32(v0, 8), zmm_f800);
    __m256i rgb16 =
        _mm512_cvtepi32_epi16(_mm512_or_si512(_mm512_or_si512(b, g), r));
    _mm256_mask_storeu_epi16(dst_rgb, k, rgb16);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTORGB565DITHERROW_AVX512BW

#ifdef HAS_ARGBTORGB565ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToRGB565Row_AVX512BW(const uint8_t* src_argb,
                              uint8_t* dst_rgb,
                              int width) {
  const __m512i zmm_001f = _mm512_set1_epi32(0x0000001f);
  const __m512i zmm_07e0 = _mm512_set1_epi32(0x000007e0);
  const __m512i zmm_f800 = _mm512_set1_epi32(0x0000f800);
  while (width >= 16) {
    __m512i v0 = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    __m512i b = _mm512_and_si512(_mm512_srli_epi32(v0, 3), zmm_001f);
    __m512i g = _mm512_and_si512(_mm512_srli_epi32(v0, 5), zmm_07e0);
    __m512i r = _mm512_and_si512(_mm512_srli_epi32(v0, 8), zmm_f800);
    __m256i rgb16 =
        _mm512_cvtepi32_epi16(_mm512_or_si512(_mm512_or_si512(b, g), r));
    _mm256_storeu_si256((__m256i*)dst_rgb, rgb16);
    dst_rgb += 32;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v0 = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i b = _mm512_and_si512(_mm512_srli_epi32(v0, 3), zmm_001f);
    __m512i g = _mm512_and_si512(_mm512_srli_epi32(v0, 5), zmm_07e0);
    __m512i r = _mm512_and_si512(_mm512_srli_epi32(v0, 8), zmm_f800);
    __m256i rgb16 =
        _mm512_cvtepi32_epi16(_mm512_or_si512(_mm512_or_si512(b, g), r));
    _mm256_mask_storeu_epi16(dst_rgb, k, rgb16);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTORGB565ROW_AVX512BW

#ifdef HAS_MERGEUVROW_16_AVX512BW
LIBYUV_TARGET_AVX512BW
void MergeUVRow_16_AVX512BW(const uint16_t* src_u,
                            const uint16_t* src_v,
                            uint16_t* dst_uv,
                            int depth,
                            int width) {
  const __m128i sh_u = _mm_cvtsi32_si128(16 - depth);
  const __m128i sh_v = _mm_cvtsi32_si128(32 - depth);
  while (width >= 16) {
    __m512i u32 =
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src_u));
    __m512i v32 =
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src_v));
    src_u += 16;
    src_v += 16;
    u32 = _mm512_sll_epi16(u32, sh_u);
    v32 = _mm512_sll_epi32(v32, sh_v);
    _mm512_storeu_si512((__m512i*)dst_uv, _mm512_or_si512(u32, v32));
    dst_uv += 32;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i u32 = _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src_u));
    __m512i v32 = _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src_v));
    u32 = _mm512_sll_epi16(u32, sh_u);
    v32 = _mm512_sll_epi32(v32, sh_v);
    _mm512_mask_storeu_epi32(dst_uv, k, _mm512_or_si512(u32, v32));
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEUVROW_16_AVX512BW

#ifdef HAS_DIVIDEROW_16_AVX512BW
LIBYUV_TARGET_AVX512BW
void DivideRow_16_AVX512BW(const uint16_t* src_y,
                           uint16_t* dst_y,
                           int scale,
                           int width) {
  if (scale == 65536) {  // Divide by 1, so do memcpy.
    if (src_y != dst_y) {
      memcpy(dst_y, src_y, (size_t)width * 2);
    }
    return;
  }
  const __m512i zmm_scale = _mm512_set1_epi16((short)scale);
  while (width >= 32) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_y);
    src_y += 32;
    _mm512_storeu_si512((__m512i*)dst_y, _mm512_mulhi_epu16(v, zmm_scale));
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi16(k, src_y);
    _mm512_mask_storeu_epi16(dst_y, k, _mm512_mulhi_epu16(v, zmm_scale));
  }
  _mm256_zeroupper();
}
#endif  // HAS_DIVIDEROW_16_AVX512BW

#ifdef HAS_YUY2TOYROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void YUY2ToYRow_AVX512BW(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  while (width >= 32) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_yuy2);
    src_yuy2 += 64;
    _mm256_storeu_si256((__m256i*)dst_y, _mm512_cvtepi16_epi8(v));
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi16(k, src_yuy2);
    _mm256_mask_storeu_epi8(dst_y, k, _mm512_cvtepi16_epi8(v));
  }
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOYROW_AVX512BW

#ifdef HAS_YUY2TOUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void YUY2ToUVRow_AVX512BW(const uint8_t* src_yuy2,
                          int stride_yuy2,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width) {
  const ptrdiff_t stride = (ptrdiff_t)stride_yuy2;
  while (width >= 32) {
    __m512i r0 = _mm512_loadu_si512((const __m512i*)src_yuy2);
    __m512i r1 = _mm512_loadu_si512((const __m512i*)(src_yuy2 + stride));
    src_yuy2 += 64;
    __m512i avg = _mm512_avg_epu8(r0, r1);
    _mm_storeu_si128((__m128i*)dst_u,
                     _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 8)));
    _mm_storeu_si128((__m128i*)dst_v,
                     _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 24)));
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i r0 = _mm512_maskz_loadu_epi32(k, src_yuy2);
    __m512i r1 = _mm512_maskz_loadu_epi32(k, src_yuy2 + stride);
    __m512i avg = _mm512_avg_epu8(r0, r1);
    _mm_mask_storeu_epi8(dst_u, k,
                         _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 8)));
    _mm_mask_storeu_epi8(dst_v, k,
                         _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 24)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOUVROW_AVX512BW

#ifdef HAS_UYVYTOYROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void UYVYToYRow_AVX512BW(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  while (width >= 32) {
    __m512i v = _mm512_loadu_si512((const __m512i*)src_uyvy);
    src_uyvy += 64;
    _mm256_storeu_si256((__m256i*)dst_y,
                        _mm512_cvtepi16_epi8(_mm512_srli_epi16(v, 8)));
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i v = _mm512_maskz_loadu_epi16(k, src_uyvy);
    _mm256_mask_storeu_epi8(dst_y, k,
                            _mm512_cvtepi16_epi8(_mm512_srli_epi16(v, 8)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_UYVYTOYROW_AVX512BW

#ifdef HAS_UYVYTOUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void UYVYToUVRow_AVX512BW(const uint8_t* src_uyvy,
                          int stride_uyvy,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width) {
  const ptrdiff_t stride = (ptrdiff_t)stride_uyvy;
  while (width >= 32) {
    __m512i r0 = _mm512_loadu_si512((const __m512i*)src_uyvy);
    __m512i r1 = _mm512_loadu_si512((const __m512i*)(src_uyvy + stride));
    src_uyvy += 64;
    __m512i avg = _mm512_avg_epu8(r0, r1);
    _mm_storeu_si128((__m128i*)dst_u, _mm512_cvtepi32_epi8(avg));
    _mm_storeu_si128((__m128i*)dst_v,
                     _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 16)));
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i r0 = _mm512_maskz_loadu_epi32(k, src_uyvy);
    __m512i r1 = _mm512_maskz_loadu_epi32(k, src_uyvy + stride);
    __m512i avg = _mm512_avg_epu8(r0, r1);
    _mm_mask_storeu_epi8(dst_u, k, _mm512_cvtepi32_epi8(avg));
    _mm_mask_storeu_epi8(dst_v, k,
                         _mm512_cvtepi32_epi8(_mm512_srli_epi32(avg, 16)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_UYVYTOUVROW_AVX512BW

#ifdef HAS_ARGBSEPIAROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBSepiaRow_AVX512VNNI(uint8_t* dst_argb, int width) {
  const __m512i zero = _mm512_setzero_si512();
  const __m512i c_b = _mm512_set1_epi32(0x00234411);
  const __m512i c_g = _mm512_set1_epi32(0x002d5816);
  const __m512i c_r = _mm512_set1_epi32(0x00326218);
  const __m512i max255 = _mm512_set1_epi32(255);
  const __m512i a_mask = _mm512_set1_epi32((int)0xff000000u);
  while (width >= 16) {
    __m512i v0 = _mm512_loadu_si512((const __m512i*)dst_argb);
    __m512i b = _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_b), 7);
    __m512i g = _mm512_min_epu32(
        _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_g), 7), max255);
    __m512i r = _mm512_min_epu32(
        _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_r), 7), max255);
    __m512i a = _mm512_and_si512(v0, a_mask);
    __m512i out = _mm512_or_si512(_mm512_or_si512(b, _mm512_slli_epi32(g, 8)),
                                  _mm512_or_si512(_mm512_slli_epi32(r, 16), a));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v0 = _mm512_maskz_loadu_epi32(k, dst_argb);
    __m512i b = _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_b), 7);
    __m512i g = _mm512_min_epu32(
        _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_g), 7), max255);
    __m512i r = _mm512_min_epu32(
        _mm512_srli_epi32(_mm512_dpbusd_epi32(zero, v0, c_r), 7), max255);
    __m512i a = _mm512_and_si512(v0, a_mask);
    __m512i out = _mm512_or_si512(_mm512_or_si512(b, _mm512_slli_epi32(g, 8)),
                                  _mm512_or_si512(_mm512_slli_epi32(r, 16), a));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBSEPIAROW_AVX512VNNI

#ifdef HAS_ARGBCOLORMATRIXROW_AVX512VNNI
LIBYUV_TARGET_AVX512VNNI
void ARGBColorMatrixRow_AVX512VNNI(const uint8_t* src_argb,
                                   uint8_t* dst_argb,
                                   const int8_t* matrix_argb,
                                   int width) {
  const __m512i zero = _mm512_setzero_si512();
  const __m512i max255 = _mm512_set1_epi32(255);
  const __m128i m128 = _mm_loadu_si128((const __m128i*)matrix_argb);
  const __m512i m0 = _mm512_set1_epi32(_mm_cvtsi128_si32(m128));
  const __m512i m1 =
      _mm512_set1_epi32(_mm_cvtsi128_si32(_mm_srli_si128(m128, 4)));
  const __m512i m2 =
      _mm512_set1_epi32(_mm_cvtsi128_si32(_mm_srli_si128(m128, 8)));
  const __m512i m3 =
      _mm512_set1_epi32(_mm_cvtsi128_si32(_mm_srli_si128(m128, 12)));
  while (width >= 16) {
    __m512i v0 = _mm512_loadu_si512((const __m512i*)src_argb);
    src_argb += 64;
    __m512i b = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m0), 6), zero),
        max255);
    __m512i g = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m1), 6), zero),
        max255);
    __m512i r = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m2), 6), zero),
        max255);
    __m512i a = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m3), 6), zero),
        max255);
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(b, _mm512_slli_epi32(g, 8)),
        _mm512_or_si512(_mm512_slli_epi32(r, 16), _mm512_slli_epi32(a, 24)));
    _mm512_storeu_si512((__m512i*)dst_argb, out);
    dst_argb += 64;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i v0 = _mm512_maskz_loadu_epi32(k, src_argb);
    __m512i b = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m0), 6), zero),
        max255);
    __m512i g = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m1), 6), zero),
        max255);
    __m512i r = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m2), 6), zero),
        max255);
    __m512i a = _mm512_min_epu32(
        _mm512_max_epi32(
            _mm512_srai_epi32(_mm512_dpbusd_epi32(zero, v0, m3), 6), zero),
        max255);
    __m512i out = _mm512_or_si512(
        _mm512_or_si512(b, _mm512_slli_epi32(g, 8)),
        _mm512_or_si512(_mm512_slli_epi32(r, 16), _mm512_slli_epi32(a, 24)));
    _mm512_mask_storeu_epi32(dst_argb, k, out);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBCOLORMATRIXROW_AVX512VNNI

#ifdef HAS_HALFFLOATROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void HalfFloatRow_AVX512BW(const uint16_t* src,
                           uint16_t* dst,
                           float scale,
                           int width) {
  const __m512 vscale = _mm512_set1_ps(scale);
  while (width >= 16) {
    __m512i s32 =
        _mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i*)src));
    src += 16;
    __m512 f = _mm512_mul_ps(_mm512_cvtepi32_ps(s32), vscale);
    __m256i h16 = _mm512_cvtps_ph(f, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    _mm256_storeu_si256((__m256i*)dst, h16);
    dst += 16;
    width -= 16;
  }
  if (width > 0) {
    __mmask16 k = (__mmask16)((1u << width) - 1u);
    __m512i s32 = _mm512_cvtepu16_epi32(_mm256_maskz_loadu_epi16(k, src));
    __m512 f = _mm512_mul_ps(_mm512_cvtepi32_ps(s32), vscale);
    __m256i h16 = _mm512_cvtps_ph(f, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    _mm256_mask_storeu_epi16(dst, k, h16);
  }
  _mm256_zeroupper();
}
#endif  // HAS_HALFFLOATROW_AVX512BW

#ifdef HAS_CONVERT8TO8ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void Convert8To8Row_AVX512BW(const uint8_t* src_y,
                             uint8_t* dst_y,
                             int scale,
                             int bias,
                             int width) {
  const __m512i zmm_scale =
      _mm512_slli_epi16(_mm512_set1_epi16((short)scale), 8);
  const __m512i zmm_bias = _mm512_set1_epi8((char)bias);
  const __m512i zmm_zero = _mm512_setzero_si512();
  while (width >= 64) {
    __m512i y8 = _mm512_loadu_si512((const __m512i*)src_y);
    src_y += 64;
    __m512i y_lo =
        _mm512_mulhi_epu16(_mm512_unpacklo_epi8(y8, zmm_zero), zmm_scale);
    __m512i y_hi =
        _mm512_mulhi_epu16(_mm512_unpackhi_epi8(y8, zmm_zero), zmm_scale);
    __m512i out8 = _mm512_add_epi8(_mm512_packus_epi16(y_lo, y_hi), zmm_bias);
    _mm512_storeu_si512((__m512i*)dst_y, out8);
    dst_y += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 k = (__mmask64)((1ULL << width) - 1ULL);
    __m512i y8 = _mm512_maskz_loadu_epi8(k, src_y);
    __m512i y_lo =
        _mm512_mulhi_epu16(_mm512_unpacklo_epi8(y8, zmm_zero), zmm_scale);
    __m512i y_hi =
        _mm512_mulhi_epu16(_mm512_unpackhi_epi8(y8, zmm_zero), zmm_scale);
    __m512i out8 = _mm512_add_epi8(_mm512_packus_epi16(y_lo, y_hi), zmm_bias);
    _mm512_mask_storeu_epi8(dst_y, k, out8);
  }
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT8TO8ROW_AVX512BW

// ===========================================================================
// Extended YUV (I422, I444, YUY2, UYVY, I210, I212, I410, P210, P410)
// to ARGB / RGBA / RGB24 / AR30 (AVX512BW / AVX512VBMI)
// ===========================================================================

static const uint16_t kSplitQuadWords_ExtWin[32] = {
    0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,
    8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15};
static const uint64_t kUnpermuteAVX512_ExtWin[8] = {0, 4, 1, 5, 2, 6, 3, 7};
static const uint16_t kPermAR30_0_Win[32] = {
    0, 32, 1, 33, 2,  34, 3,  35, 4,  36, 5,  37, 6,  38, 7,  39,
    8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47};
static const uint16_t kPermAR30_1_Win[32] = {
    16, 48, 17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55,
    24, 56, 25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63};
static const uint8_t kShuffleYUY2Y_Win[16] = {0, 0, 2,  2,  4,  4,  6,  6,
                                              8, 8, 10, 10, 12, 12, 14, 14};
static const uint8_t kShuffleYUY2UV_Win[16] = {1, 3,  1, 3,  5,  7,  5,  7,
                                               9, 11, 9, 11, 13, 15, 13, 15};
static const uint8_t kShuffleUYVYY_Win[16] = {1, 1, 3,  3,  5,  5,  7,  7,
                                              9, 9, 11, 11, 13, 13, 15, 15};
static const uint8_t kShuffleUYVYUV_Win[16] = {0, 2,  0, 2,  4,  6,  4,  6,
                                               8, 10, 8, 10, 12, 14, 12, 14};

#define YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants)                    \
  const __m512i zmm_kUVToB = _mm512_broadcast_i32x4(                 \
      _mm_loadu_si128((const __m128i*)(yuvconstants)->kUVToB));      \
  const __m512i zmm_kUVToG = _mm512_broadcast_i32x4(                 \
      _mm_loadu_si128((const __m128i*)(yuvconstants)->kUVToG));      \
  const __m512i zmm_kUVToR = _mm512_broadcast_i32x4(                 \
      _mm_loadu_si128((const __m128i*)(yuvconstants)->kUVToR));      \
  const __m512i zmm_kYToRgb = _mm512_broadcast_i32x4(                \
      _mm_loadu_si128((const __m128i*)(yuvconstants)->kYToRgb));     \
  const __m512i zmm_kYBiasToRgb = _mm512_broadcast_i32x4(            \
      _mm_loadu_si128((const __m128i*)(yuvconstants)->kYBiasToRgb)); \
  const __m512i zmm_128 = _mm512_set1_epi8((char)0x80)

#define YUVTORGB16_WIN_AVX512BW                                        \
  zmm3 = _mm512_sub_epi8(zmm3, zmm_128);                               \
  zmm4 = _mm512_add_epi16(_mm512_mulhi_epu16(zmm4, zmm_kYToRgb),       \
                          zmm_kYBiasToRgb);                            \
  __m512i b16 =                                                        \
      _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToB, zmm3), zmm4); \
  __m512i g16 =                                                        \
      _mm512_subs_epi16(zmm4, _mm512_maddubs_epi16(zmm_kUVToG, zmm3)); \
  __m512i r16 = _mm512_adds_epi16(_mm512_maddubs_epi16(zmm_kUVToR, zmm3), zmm4)

#define YUVTORGB8_WIN_AVX512BW                                         \
  YUVTORGB16_WIN_AVX512BW;                                             \
  __m512i zmm0 = _mm512_packus_epi16(_mm512_srai_epi16(b16, 6), zmm5); \
  __m512i zmm1 = _mm512_packus_epi16(_mm512_srai_epi16(g16, 6), zmm5); \
  __m512i zmm2 = _mm512_packus_epi16(_mm512_srai_epi16(r16, 6), zmm5)

#define STOREARGB_WIN_AVX512BW(dst_ptr)                                      \
  do {                                                                       \
    __m512i bg =                                                             \
        PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1));      \
    __m512i ra =                                                             \
        PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5));      \
    _mm512_storeu_si512((__m512i*)(dst_ptr), _mm512_unpacklo_epi16(bg, ra)); \
    _mm512_storeu_si512((__m512i*)((dst_ptr) + 64),                          \
                        _mm512_unpackhi_epi16(bg, ra));                      \
  } while (0)

#define STOREARGB_MASK_WIN_AVX512BW(dst_ptr, m32)                       \
  do {                                                                  \
    __m512i bg =                                                        \
        PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm0, zmm1)); \
    __m512i ra =                                                        \
        PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm2, zmm5)); \
    _mm512_mask_storeu_epi32((dst_ptr), (__mmask16)(m32),               \
                             _mm512_unpacklo_epi16(bg, ra));            \
    _mm512_mask_storeu_epi32((dst_ptr) + 64, (__mmask16)((m32) >> 16),  \
                             _mm512_unpackhi_epi16(bg, ra));            \
  } while (0)

#define PACKAR30_WIN_AVX512BW(dst0, dst1)                                     \
  b16 = _mm512_max_epi16(_mm512_min_epi16(b16, zmm_3ff0), zmm_zero);          \
  g16 = _mm512_max_epi16(_mm512_min_epi16(g16, zmm_3ff0), zmm_zero);          \
  r16 = _mm512_max_epi16(_mm512_min_epi16(r16, zmm_3ff0), zmm_zero);          \
  __m512i lo16 =                                                              \
      _mm512_or_si512(_mm512_srli_epi16(b16, 4),                              \
                      _mm512_and_si512(_mm512_slli_epi16(g16, 6), zmm_fc00)); \
  __m512i hi16 =                                                              \
      _mm512_or_si512(_mm512_or_si512(_mm512_and_si512(r16, zmm_3ff0),        \
                                      _mm512_srli_epi16(g16, 10)),            \
                      zmm_c000);                                              \
  __m512i dst0 = _mm512_permutex2var_epi16(lo16, zmm_perm0, hi16);            \
  __m512i dst1 = _mm512_permutex2var_epi16(lo16, zmm_perm1, hi16)

#ifdef HAS_I422TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I422ToARGBRow_AVX512BW(const uint8_t* y_buf,
                            const uint8_t* u_buf,
                            const uint8_t* v_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m128i xu = _mm_loadu_si128((const __m128i*)u_buf);
    __m128i xv = _mm_loadu_si128((const __m128i*)v_buf);
    u_buf += 16;
    v_buf += 16;
    __m256i uv256 = _mm256_inserti128_si256(
        _mm256_zextsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m128i xu = _mm_maskz_loadu_epi8(k_uv, u_buf);
    __m128i xv = _mm_maskz_loadu_epi8(k_uv, v_buf);
    __m256i uv256 = _mm256_inserti128_si256(
        _mm256_zextsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TOARGBROW_AVX512BW

#ifdef HAS_I422TORGBAROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I422ToRGBARow_AVX512BW(const uint8_t* y_buf,
                            const uint8_t* u_buf,
                            const uint8_t* v_buf,
                            uint8_t* dst_rgba,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m128i xu = _mm_loadu_si128((const __m128i*)u_buf);
    __m128i xv = _mm_loadu_si128((const __m128i*)v_buf);
    u_buf += 16;
    v_buf += 16;
    __m256i uv256 = _mm256_inserti128_si256(
        _mm256_zextsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    y_buf += 32;
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    __m512i ab = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm5, zmm0));
    __m512i gr = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm1, zmm2));
    _mm512_storeu_si512((__m512i*)dst_rgba, _mm512_unpacklo_epi16(ab, gr));
    _mm512_storeu_si512((__m512i*)(dst_rgba + 64),
                        _mm512_unpackhi_epi16(ab, gr));
    dst_rgba += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m128i xu = _mm_maskz_loadu_epi8(k_uv, u_buf);
    __m128i xv = _mm_maskz_loadu_epi8(k_uv, v_buf);
    __m256i uv256 = _mm256_inserti128_si256(
        _mm256_zextsi128_si256(_mm_unpacklo_epi8(xu, xv)),
        _mm_unpackhi_epi8(xu, xv), 1);
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8((__mmask32)m32, y_buf));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    __m512i ab = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm5, zmm0));
    __m512i gr = PermutexvarEpi64(zmm_unperm, _mm512_unpacklo_epi8(zmm1, zmm2));
    _mm512_mask_storeu_epi32(dst_rgba, (__mmask16)m32,
                             _mm512_unpacklo_epi16(ab, gr));
    _mm512_mask_storeu_epi32(dst_rgba + 64, (__mmask16)(m32 >> 16),
                             _mm512_unpackhi_epi16(ab, gr));
  }
  _mm256_zeroupper();
}
#endif  // HAS_I422TORGBAROW_AVX512BW

#ifdef HAS_I444TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I444ToARGBRow_AVX512BW(const uint8_t* y_buf,
                            const uint8_t* u_buf,
                            const uint8_t* v_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i u16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)u_buf));
    __m512i v16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)v_buf));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask32 k = (__mmask32)m32;
    __m512i u16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, u_buf));
    __m512i v16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, v_buf));
    __m512i zmm4 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, y_buf));
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I444TOARGBROW_AVX512BW

#ifdef HAS_I444ALPHATOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I444AlphaToARGBRow_AVX512BW(const uint8_t* y_buf,
                                 const uint8_t* u_buf,
                                 const uint8_t* v_buf,
                                 const uint8_t* a_buf,
                                 uint8_t* dst_argb,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  while (width >= 32) {
    __m512i u16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)u_buf));
    __m512i v16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)v_buf));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    __m512i a16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)a_buf));
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    a_buf += 32;
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask32 k = (__mmask32)m32;
    __m512i u16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, u_buf));
    __m512i v16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, v_buf));
    __m512i zmm4 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, y_buf));
    __m512i a16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, a_buf));
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I444ALPHATOARGBROW_AVX512BW

#ifdef HAS_I444TORGB24ROW_AVX512VBMI
static const uint8_t kMaskBG_I444[64] = {
    0x00, 0x40, 0x01, 0x41, 0x02, 0x42, 0x03, 0x43, 0x04, 0x44, 0x05,
    0x45, 0x06, 0x46, 0x07, 0x47, 0x10, 0x50, 0x11, 0x51, 0x12, 0x52,
    0x13, 0x53, 0x14, 0x54, 0x15, 0x55, 0x16, 0x56, 0x17, 0x57, 0x20,
    0x60, 0x21, 0x61, 0x22, 0x62, 0x23, 0x63, 0x24, 0x64, 0x25, 0x65,
    0x26, 0x66, 0x27, 0x67, 0x30, 0x70, 0x31, 0x71, 0x32, 0x72, 0x33,
    0x73, 0x34, 0x74, 0x35, 0x75, 0x36, 0x76, 0x37, 0x77};
static const uint8_t kMaskDST0_I444[64] = {
    0x00, 0x01, 0x40, 0x02, 0x03, 0x41, 0x04, 0x05, 0x42, 0x06, 0x07,
    0x43, 0x08, 0x09, 0x44, 0x0a, 0x0b, 0x45, 0x0c, 0x0d, 0x46, 0x0e,
    0x0f, 0x47, 0x10, 0x11, 0x50, 0x12, 0x13, 0x51, 0x14, 0x15, 0x52,
    0x16, 0x17, 0x53, 0x18, 0x19, 0x54, 0x1a, 0x1b, 0x55, 0x1c, 0x1d,
    0x56, 0x1e, 0x1f, 0x57, 0x20, 0x21, 0x60, 0x22, 0x23, 0x61, 0x24,
    0x25, 0x62, 0x26, 0x27, 0x63, 0x28, 0x29, 0x64, 0x2a};
static const uint8_t kMaskDST1_I444[64] = {
    0x2b, 0x65, 0x2c, 0x2d, 0x66, 0x2e, 0x2f, 0x67, 0x30, 0x31, 0x70,
    0x32, 0x33, 0x71, 0x34, 0x35, 0x72, 0x36, 0x37, 0x73, 0x38, 0x39,
    0x74, 0x3a, 0x3b, 0x75, 0x3c, 0x3d, 0x76, 0x3e, 0x3f, 0x77, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

LIBYUV_TARGET_AVX512VBMI
void I444ToRGB24Row_AVX512VBMI(const uint8_t* y_buf,
                               const uint8_t* u_buf,
                               const uint8_t* v_buf,
                               uint8_t* dst_rgb24,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_mask_BG = _mm512_loadu_si512((const __m512i*)kMaskBG_I444);
  const __m512i zmm_mask_DST0 =
      _mm512_loadu_si512((const __m512i*)kMaskDST0_I444);
  const __m512i zmm_mask_DST1 =
      _mm512_loadu_si512((const __m512i*)kMaskDST1_I444);
  while (width >= 32) {
    __m512i u16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)u_buf));
    __m512i v16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)v_buf));
    __m512i zmm4 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)y_buf));
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB16_WIN_AVX512BW;
    __m512i zmm0 = _mm512_srai_epi16(b16, 6);
    __m512i zmm1 = _mm512_srai_epi16(g16, 6);
    __m512i zmm2 = _mm512_srai_epi16(r16, 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    _mm512_storeu_si512((__m512i*)dst_rgb24, zmm_dst0);
    _mm256_storeu_si256((__m256i*)(dst_rgb24 + 64),
                        _mm512_castsi512_si256(zmm_dst1));
    dst_rgb24 += 96;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i u16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, u_buf));
    __m512i v16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, v_buf));
    __m512i zmm4 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, y_buf));
    __m512i zmm3 = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    zmm4 = _mm512_or_si512(zmm4, _mm512_slli_epi16(zmm4, 8));
    YUVTORGB16_WIN_AVX512BW;
    __m512i zmm0 = _mm512_srai_epi16(b16, 6);
    __m512i zmm1 = _mm512_srai_epi16(g16, 6);
    __m512i zmm2 = _mm512_srai_epi16(r16, 6);
    zmm0 = _mm512_packus_epi16(zmm0, zmm0);
    zmm1 = _mm512_packus_epi16(zmm1, zmm1);
    zmm2 = _mm512_packus_epi16(zmm2, zmm2);
    __m512i zmm_BG = _mm512_permutex2var_epi8(zmm0, zmm_mask_BG, zmm1);
    __m512i zmm_dst0 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST0, zmm2);
    __m512i zmm_dst1 = _mm512_permutex2var_epi8(zmm_BG, zmm_mask_DST1, zmm2);
    int dst_bytes = width * 3;
    __mmask64 k0 = dst_bytes >= 64 ? ~0ull : (1ull << dst_bytes) - 1ull;
    __mmask32 k1 = dst_bytes > 64 ? (1u << (dst_bytes - 64)) - 1u : 0u;
    _mm512_mask_storeu_epi8(dst_rgb24, k0, zmm_dst0);
    _mm256_mask_storeu_epi8(dst_rgb24 + 64, k1,
                            _mm512_castsi512_si256(zmm_dst1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_I444TORGB24ROW_AVX512VBMI

#ifdef HAS_YUY2TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void YUY2ToARGBRow_AVX512BW(const uint8_t* yuy2_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm_shuf_y = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleYUY2Y_Win));
  const __m512i zmm_shuf_uv = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleYUY2UV_Win));
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i in = _mm512_loadu_si512((const __m512i*)yuy2_buf);
    yuy2_buf += 64;
    __m512i zmm4 = _mm512_shuffle_epi8(in, zmm_shuf_y);
    __m512i zmm3 = _mm512_shuffle_epi8(in, zmm_shuf_uv);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k2 = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i in = _mm512_maskz_loadu_epi32(k2, yuy2_buf);
    __m512i zmm4 = _mm512_shuffle_epi8(in, zmm_shuf_y);
    __m512i zmm3 = _mm512_shuffle_epi8(in, zmm_shuf_uv);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOARGBROW_AVX512BW

#ifdef HAS_UYVYTOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void UYVYToARGBRow_AVX512BW(const uint8_t* uyvy_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm_shuf_y = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleUYVYY_Win));
  const __m512i zmm_shuf_uv = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleUYVYUV_Win));
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i in = _mm512_loadu_si512((const __m512i*)uyvy_buf);
    uyvy_buf += 64;
    __m512i zmm4 = _mm512_shuffle_epi8(in, zmm_shuf_y);
    __m512i zmm3 = _mm512_shuffle_epi8(in, zmm_shuf_uv);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k2 = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i in = _mm512_maskz_loadu_epi32(k2, uyvy_buf);
    __m512i zmm4 = _mm512_shuffle_epi8(in, zmm_shuf_y);
    __m512i zmm3 = _mm512_shuffle_epi8(in, zmm_shuf_uv);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_UYVYTOARGBROW_AVX512BW

#ifdef HAS_I210TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I210ToARGBRow_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m256i u_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)u_buf), 2);
    __m256i v_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 16;
    v_buf += 16;
    y_buf += 32;
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m256i u_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, u_buf), 2);
    __m256i v_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I210TOARGBROW_AVX512BW

#ifdef HAS_I210TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I210ToAR30Row_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0_Win);
  const __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1_Win);
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_3ff0 = _mm512_set1_epi16(0x3ff0);
  const __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  const __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  while (width >= 32) {
    __m256i u_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)u_buf), 2);
    __m256i v_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 16;
    v_buf += 16;
    y_buf += 32;
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_storeu_si512((__m512i*)dst_ar30, dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), dst1);
    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m256i u_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, u_buf), 2);
    __m256i v_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)m32, dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(m32 >> 16), dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I210TOAR30ROW_AVX512BW

#ifdef HAS_I210ALPHATOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I210AlphaToARGBRow_AVX512BW(const uint16_t* y_buf,
                                 const uint16_t* u_buf,
                                 const uint16_t* v_buf,
                                 const uint16_t* a_buf,
                                 uint8_t* dst_argb,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  while (width >= 32) {
    __m256i u_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)u_buf), 2);
    __m256i v_s =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i a16 =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)a_buf), 2);
    u_buf += 16;
    v_buf += 16;
    y_buf += 32;
    a_buf += 32;
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m256i u_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, u_buf), 2);
    __m256i v_s = _mm256_srli_epi16(_mm256_maskz_loadu_epi16(k_uv, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m512i a16 =
        _mm512_srli_epi16(_mm512_maskz_loadu_epi16((__mmask32)m32, a_buf), 2);
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I210ALPHATOARGBROW_AVX512BW

#ifdef HAS_I212TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I212ToARGBRow_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m256i u_s =
        _mm256_srai_epi16(_mm256_loadu_si256((const __m256i*)u_buf), 4);
    __m256i v_s =
        _mm256_srai_epi16(_mm256_loadu_si256((const __m256i*)v_buf), 4);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 16;
    v_buf += 16;
    y_buf += 32;
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 4), _mm512_srli_epi16(y16, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m256i u_s = _mm256_srai_epi16(_mm256_maskz_loadu_epi16(k_uv, u_buf), 4);
    __m256i v_s = _mm256_srai_epi16(_mm256_maskz_loadu_epi16(k_uv, v_buf), 4);
    __m512i y16 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 4), _mm512_srli_epi16(y16, 8));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I212TOARGBROW_AVX512BW

#ifdef HAS_I212TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I212ToAR30Row_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_quad =
      _mm512_loadu_si512((const __m512i*)kSplitQuadWords_ExtWin);
  const __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0_Win);
  const __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1_Win);
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_3ff0 = _mm512_set1_epi16(0x3ff0);
  const __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  const __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  while (width >= 32) {
    __m256i u_s =
        _mm256_srai_epi16(_mm256_loadu_si256((const __m256i*)u_buf), 4);
    __m256i v_s =
        _mm256_srai_epi16(_mm256_loadu_si256((const __m256i*)v_buf), 4);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 16;
    v_buf += 16;
    y_buf += 32;
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 4), _mm512_srli_epi16(y16, 8));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_storeu_si512((__m512i*)dst_ar30, dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), dst1);
    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m256i u_s = _mm256_srai_epi16(_mm256_maskz_loadu_epi16(k_uv, u_buf), 4);
    __m256i v_s = _mm256_srai_epi16(_mm256_maskz_loadu_epi16(k_uv, v_buf), 4);
    __m512i y16 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m256i uv256 = _mm256_unpacklo_epi8(_mm256_packus_epi16(u_s, u_s),
                                         _mm256_packus_epi16(v_s, v_s));
    __m512i zmm3 = PermutexvarEpi16(zmm_quad, _mm512_zextsi256_si512(uv256));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 4), _mm512_srli_epi16(y16, 8));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)m32, dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(m32 >> 16), dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I212TOAR30ROW_AVX512BW

#ifdef HAS_I410TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I410ToARGBRow_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i u_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)u_buf), 2);
    __m512i v_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask32 k = (__mmask32)m32;
    __m512i u_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, u_buf), 2);
    __m512i v_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16(k, y_buf);
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I410TOARGBROW_AVX512BW

#ifdef HAS_I410TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I410ToAR30Row_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* u_buf,
                            const uint16_t* v_buf,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0_Win);
  const __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1_Win);
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_3ff0 = _mm512_set1_epi16(0x3ff0);
  const __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  const __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  while (width >= 32) {
    __m512i u_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)u_buf), 2);
    __m512i v_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_storeu_si512((__m512i*)dst_ar30, dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), dst1);
    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask32 k = (__mmask32)m32;
    __m512i u_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, u_buf), 2);
    __m512i v_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16(k, y_buf);
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)m32, dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(m32 >> 16), dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I410TOAR30ROW_AVX512BW

#ifdef HAS_I410ALPHATOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void I410AlphaToARGBRow_AVX512BW(const uint16_t* y_buf,
                                 const uint16_t* u_buf,
                                 const uint16_t* v_buf,
                                 const uint16_t* a_buf,
                                 uint8_t* dst_argb,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  while (width >= 32) {
    __m512i u_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)u_buf), 2);
    __m512i v_s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)v_buf), 2);
    __m512i y16 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i a16 =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)a_buf), 2);
    u_buf += 32;
    v_buf += 32;
    y_buf += 32;
    a_buf += 32;
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask32 k = (__mmask32)m32;
    __m512i u_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, u_buf), 2);
    __m512i v_s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, v_buf), 2);
    __m512i y16 = _mm512_maskz_loadu_epi16(k, y_buf);
    __m512i a16 = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, a_buf), 2);
    __m512i zmm3 = _mm512_unpacklo_epi8(_mm512_packus_epi16(u_s, u_s),
                                        _mm512_packus_epi16(v_s, v_s));
    __m512i zmm4 =
        _mm512_or_si512(_mm512_slli_epi16(y16, 6), _mm512_srli_epi16(y16, 4));
    __m512i zmm5 = _mm512_packus_epi16(a16, a16);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_I410ALPHATOARGBROW_AVX512BW

#ifdef HAS_P210TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void P210ToARGBRow_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* uv_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm_shuf_uv = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleYUY2UV_Win));
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i zmm4 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i zmm3 = _mm512_shuffle_epi8(
        _mm512_loadu_si512((const __m512i*)uv_buf), zmm_shuf_uv);
    y_buf += 32;
    uv_buf += 32;
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm4 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m512i zmm3 = _mm512_shuffle_epi8(_mm512_maskz_loadu_epi32(k_uv, uv_buf),
                                       zmm_shuf_uv);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_P210TOARGBROW_AVX512BW

#ifdef HAS_P210TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void P210ToAR30Row_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* uv_buf,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0_Win);
  const __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1_Win);
  const __m512i zmm_shuf_uv = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleYUY2UV_Win));
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_3ff0 = _mm512_set1_epi16(0x3ff0);
  const __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  const __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  while (width >= 32) {
    __m512i zmm4 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i zmm3 = _mm512_shuffle_epi8(
        _mm512_loadu_si512((const __m512i*)uv_buf), zmm_shuf_uv);
    y_buf += 32;
    uv_buf += 32;
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_storeu_si512((__m512i*)dst_ar30, dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), dst1);
    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __mmask16 k_uv = (__mmask16)((1u << ((width + 1) >> 1)) - 1u);
    __m512i zmm4 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m512i zmm3 = _mm512_shuffle_epi8(_mm512_maskz_loadu_epi32(k_uv, uv_buf),
                                       zmm_shuf_uv);
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)m32, dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(m32 >> 16), dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_P210TOAR30ROW_AVX512BW

#ifdef HAS_P410TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void P410ToARGBRow_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* uv_buf,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_unperm =
      _mm512_loadu_si512((const __m512i*)kUnpermuteAVX512_ExtWin);
  const __m512i zmm5 = _mm512_set1_epi32(-1);
  while (width >= 32) {
    __m512i zmm4 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i uv0 = _mm512_loadu_si512((const __m512i*)uv_buf);
    __m512i uv1 = _mm512_loadu_si512((const __m512i*)(uv_buf + 32));
    y_buf += 32;
    uv_buf += 64;
    __m256i lo = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8));
    __m256i hi = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8));
    __m512i zmm3 = _mm512_inserti64x4(_mm512_zextsi256_si512(lo), hi, 1);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_WIN_AVX512BW(dst_argb);
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __m512i zmm4 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m512i uv0 = _mm512_maskz_loadu_epi32((__mmask16)m32, uv_buf);
    __m512i uv1 = _mm512_maskz_loadu_epi32((__mmask16)(m32 >> 16), uv_buf + 32);
    __m256i lo = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8));
    __m256i hi = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8));
    __m512i zmm3 = _mm512_inserti64x4(_mm512_zextsi256_si512(lo), hi, 1);
    YUVTORGB8_WIN_AVX512BW;
    STOREARGB_MASK_WIN_AVX512BW(dst_argb, m32);
  }
  _mm256_zeroupper();
}
#endif  // HAS_P410TOARGBROW_AVX512BW

#ifdef HAS_P410TOAR30ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void P410ToAR30Row_AVX512BW(const uint16_t* y_buf,
                            const uint16_t* uv_buf,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  YUVTORGB_SETUP_WIN_AVX512BW(yuvconstants);
  const __m512i zmm_perm0 = _mm512_loadu_si512((const __m512i*)kPermAR30_0_Win);
  const __m512i zmm_perm1 = _mm512_loadu_si512((const __m512i*)kPermAR30_1_Win);
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_3ff0 = _mm512_set1_epi16(0x3ff0);
  const __m512i zmm_fc00 = _mm512_set1_epi16((short)0xfc00);
  const __m512i zmm_c000 = _mm512_set1_epi16((short)0xc000);
  while (width >= 32) {
    __m512i zmm4 = _mm512_loadu_si512((const __m512i*)y_buf);
    __m512i uv0 = _mm512_loadu_si512((const __m512i*)uv_buf);
    __m512i uv1 = _mm512_loadu_si512((const __m512i*)(uv_buf + 32));
    y_buf += 32;
    uv_buf += 64;
    __m256i lo = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8));
    __m256i hi = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8));
    __m512i zmm3 = _mm512_inserti64x4(_mm512_zextsi256_si512(lo), hi, 1);
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_storeu_si512((__m512i*)dst_ar30, dst0);
    _mm512_storeu_si512((__m512i*)(dst_ar30 + 64), dst1);
    dst_ar30 += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t m32 = (1u << width) - 1u;
    __m512i zmm4 = _mm512_maskz_loadu_epi16((__mmask32)m32, y_buf);
    __m512i uv0 = _mm512_maskz_loadu_epi32((__mmask16)m32, uv_buf);
    __m512i uv1 = _mm512_maskz_loadu_epi32((__mmask16)(m32 >> 16), uv_buf + 32);
    __m256i lo = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8));
    __m256i hi = _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8));
    __m512i zmm3 = _mm512_inserti64x4(_mm512_zextsi256_si512(lo), hi, 1);
    YUVTORGB16_WIN_AVX512BW;
    PACKAR30_WIN_AVX512BW(dst0, dst1);
    _mm512_mask_storeu_epi32(dst_ar30, (__mmask16)m32, dst0);
    _mm512_mask_storeu_epi32(dst_ar30 + 64, (__mmask16)(m32 >> 16), dst1);
  }
  _mm256_zeroupper();
}
#endif  // HAS_P410TOAR30ROW_AVX512BW

#if defined(HAS_ARGBTORGB24ROW_AVX512BW) || defined(HAS_ARGBTORAWROW_AVX512BW)
LIBYUV_TARGET_AVX512BW
static inline void ARGBToRGBRow_AVX512BW(const uint8_t* src,
                                         uint8_t* dst,
                                         int width,
                                         const uint8_t* shuffler) {
  const __m512i zmm_shuf =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)shuffler));
  const __m512i zmm_p0 =
      _mm512_loadu_si512((const __m512i*)kPermdRGB24_0_AVX512BW);
  const __m512i zmm_p1 =
      _mm512_loadu_si512((const __m512i*)kPermdRGB24_1_AVX512BW);
  const __m512i zmm_p2 =
      _mm512_loadu_si512((const __m512i*)kPermdRGB24_2_AVX512BW);

  while (width >= 64) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src + 64));
    __m512i zmm2 = _mm512_loadu_si512((const __m512i*)(src + 128));
    __m512i zmm3 = _mm512_loadu_si512((const __m512i*)(src + 192));
    src += 256;

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    __m512i out0 = _mm512_permutex2var_epi32(zmm0, zmm_p0, zmm1);
    __m512i out1 = _mm512_permutex2var_epi32(zmm1, zmm_p1, zmm2);
    __m512i out2 = _mm512_permutex2var_epi32(zmm2, zmm_p2, zmm3);

    _mm512_storeu_si512((__m512i*)dst, out0);
    _mm512_storeu_si512((__m512i*)(dst + 64), out1);
    _mm512_storeu_si512((__m512i*)(dst + 128), out2);
    dst += 192;
    width -= 64;
  }

  if (width > 0) {
    uint64_t m_in = (1ULL << width) - 1ULL;
    __m512i zmm0 = _mm512_maskz_loadu_epi32((__mmask16)m_in, src);
    __m512i zmm1 = _mm512_maskz_loadu_epi32((__mmask16)(m_in >> 16), src + 64);
    __m512i zmm2 = _mm512_maskz_loadu_epi32((__mmask16)(m_in >> 32), src + 128);
    __m512i zmm3 = _mm512_maskz_loadu_epi32((__mmask16)(m_in >> 48), src + 192);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    __m512i out0 = _mm512_permutex2var_epi32(zmm0, zmm_p0, zmm1);
    __m512i out1 = _mm512_permutex2var_epi32(zmm1, zmm_p1, zmm2);
    __m512i out2 = _mm512_permutex2var_epi32(zmm2, zmm_p2, zmm3);

    int bytes = width * 3;
    __mmask64 k_out0 =
        (bytes >= 64) ? ~(__mmask64)0 : (__mmask64)((1ULL << bytes) - 1ULL);
    __mmask64 k_out1 =
        (bytes <= 64)
            ? 0
            : ((bytes >= 128) ? ~(__mmask64)0
                              : (__mmask64)((1ULL << (bytes - 64)) - 1ULL));
    __mmask64 k_out2 =
        (bytes <= 128) ? 0 : (__mmask64)((1ULL << (bytes - 128)) - 1ULL);

    _mm512_mask_storeu_epi8(dst, k_out0, out0);
    _mm512_mask_storeu_epi8(dst + 64, k_out1, out1);
    _mm512_mask_storeu_epi8(dst + 128, k_out2, out2);
  }
  _mm256_zeroupper();
}
#endif  // defined(HAS_ARGBTORGB24ROW_AVX512BW) ||
        // defined(HAS_ARGBTORAWROW_AVX512BW)

#ifdef HAS_ARGBTORGB24ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToRGB24Row_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ARGBToRGBRow_AVX512BW(src, dst, width, kShuffleMaskARGBToRGB24[0]);
}
#endif  // HAS_ARGBTORGB24ROW_AVX512BW

#ifdef HAS_ARGBTORAWROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToRAWRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  static const uint8_t kShuffleMaskARGBToRAW_AVX512[16] = {
      2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 8u, 14u, 13u, 12u, 128u, 128u, 128u,
      128u};
  ARGBToRGBRow_AVX512BW(src, dst, width, kShuffleMaskARGBToRAW_AVX512);
}
#endif  // HAS_ARGBTORAWROW_AVX512BW

#ifdef HAS_CONVERT16TO8ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void Convert16To8Row_AVX512BW(const uint16_t* src_y,
                              uint8_t* dst_y,
                              int scale,
                              int width) {
  const __m512i zmm_scale = _mm512_set1_epi16((short)scale);
  const __m512i zmm_255 = _mm512_set1_epi16(255);
  while (width >= 64) {
    __m512i y0 = _mm512_loadu_si512((const __m512i*)src_y);
    __m512i y1 = _mm512_loadu_si512((const __m512i*)(src_y + 32));
    y0 = _mm512_min_epu16(_mm512_mulhi_epu16(y0, zmm_scale), zmm_255);
    y1 = _mm512_min_epu16(_mm512_mulhi_epu16(y1, zmm_scale), zmm_255);
    _mm256_storeu_si256((__m256i*)dst_y, _mm512_cvtepi16_epi8(y0));
    _mm256_storeu_si256((__m256i*)(dst_y + 32), _mm512_cvtepi16_epi8(y1));
    src_y += 64;
    dst_y += 64;
    width -= 64;
  }
  if (width >= 32) {
    __m512i y0 = _mm512_loadu_si512((const __m512i*)src_y);
    y0 = _mm512_min_epu16(_mm512_mulhi_epu16(y0, zmm_scale), zmm_255);
    _mm256_storeu_si256((__m256i*)dst_y, _mm512_cvtepi16_epi8(y0));
    src_y += 32;
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i y0 = _mm512_maskz_loadu_epi16(k, src_y);
    y0 = _mm512_min_epu16(_mm512_mulhi_epu16(y0, zmm_scale), zmm_255);
    _mm256_mask_storeu_epi8(dst_y, k, _mm512_cvtepi16_epi8(y0));
  }
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT16TO8ROW_AVX512BW

#ifdef HAS_COPYROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void CopyRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  while (width >= 128) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src);
    __m512i z1 = _mm512_loadu_si512((const __m512i*)(src + 64));
    _mm512_storeu_si512((__m512i*)dst, z0);
    _mm512_storeu_si512((__m512i*)(dst + 64), z1);
    src += 128;
    dst += 128;
    width -= 128;
  }
  if (width >= 64) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src);
    _mm512_storeu_si512((__m512i*)dst, z0);
    src += 64;
    dst += 64;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 k = (__mmask64)((1ULL << width) - 1ULL);
    __m512i z0 = _mm512_maskz_loadu_epi8(k, src);
    _mm512_mask_storeu_epi8(dst, k, z0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_COPYROW_AVX512BW

#ifdef HAS_HALFROW_16TO8_AVX512BW
LIBYUV_TARGET_AVX512BW
void HalfRow_16To8_AVX512BW(const uint16_t* src_uv,
                            ptrdiff_t src_uv_stride,
                            uint8_t* dst_uv,
                            int scale,
                            int width) {
  const __m512i zmm_scale = _mm512_set1_epi16((short)scale);
  const __m512i zmm_255 = _mm512_set1_epi16(255);
  while (width >= 64) {
    __m512i r0 = _mm512_loadu_si512((const __m512i*)src_uv);
    __m512i r1 = _mm512_loadu_si512((const __m512i*)(src_uv + 32));
    __m512i s0 = _mm512_loadu_si512((const __m512i*)(src_uv + src_uv_stride));
    __m512i s1 =
        _mm512_loadu_si512((const __m512i*)(src_uv + src_uv_stride + 32));
    r0 = _mm512_avg_epu16(r0, s0);
    r1 = _mm512_avg_epu16(r1, s1);
    r0 = _mm512_min_epu16(_mm512_mulhi_epu16(r0, zmm_scale), zmm_255);
    r1 = _mm512_min_epu16(_mm512_mulhi_epu16(r1, zmm_scale), zmm_255);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm512_cvtepi16_epi8(r0));
    _mm256_storeu_si256((__m256i*)(dst_uv + 32), _mm512_cvtepi16_epi8(r1));
    src_uv += 64;
    dst_uv += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask32 k1 = (__mmask32)mask;
    __mmask32 k2 = (__mmask32)(mask >> 32);
    __m512i r0 = _mm512_maskz_loadu_epi16(k1, src_uv);
    __m512i r1 = _mm512_maskz_loadu_epi16(k2, src_uv + 32);
    __m512i s0 = _mm512_maskz_loadu_epi16(k1, src_uv + src_uv_stride);
    __m512i s1 = _mm512_maskz_loadu_epi16(k2, src_uv + src_uv_stride + 32);
    r0 = _mm512_avg_epu16(r0, s0);
    r1 = _mm512_avg_epu16(r1, s1);
    r0 = _mm512_min_epu16(_mm512_mulhi_epu16(r0, zmm_scale), zmm_255);
    r1 = _mm512_min_epu16(_mm512_mulhi_epu16(r1, zmm_scale), zmm_255);
    _mm256_mask_storeu_epi8(dst_uv, k1, _mm512_cvtepi16_epi8(r0));
    _mm256_mask_storeu_epi8(dst_uv + 32, k2, _mm512_cvtepi16_epi8(r1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_HALFROW_16TO8_AVX512BW

#ifdef HAS_HALFWIDTHROW_16TO8_AVX512BW
LIBYUV_TARGET_AVX512BW
void HalfWidthRow_16To8_AVX512BW(const uint16_t* src_uv,
                                 ptrdiff_t src_uv_stride,
                                 uint8_t* dst_uv,
                                 int scale,
                                 int width) {
  const __m512i zmm_mask16 = _mm512_set1_epi32(0x0000ffff);
  const __m512i zmm_2 = _mm512_set1_epi32(2);
  const __m512i zmm_scale = _mm512_set1_epi16((short)scale);
  const __m512i zmm_255 = _mm512_set1_epi16(255);
  while (width >= 32) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_uv);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)(src_uv + 32));
    __m512i t0 = _mm512_loadu_si512((const __m512i*)(src_uv + src_uv_stride));
    __m512i t1 =
        _mm512_loadu_si512((const __m512i*)(src_uv + src_uv_stride + 32));
    __m512i sum0 = _mm512_add_epi32(_mm512_and_si512(s0, zmm_mask16),
                                    _mm512_srli_epi32(s0, 16));
    sum0 = _mm512_add_epi32(sum0, _mm512_and_si512(t0, zmm_mask16));
    sum0 = _mm512_add_epi32(sum0, _mm512_srli_epi32(t0, 16));
    sum0 = _mm512_srli_epi32(_mm512_add_epi32(sum0, zmm_2), 2);

    __m512i sum1 = _mm512_add_epi32(_mm512_and_si512(s1, zmm_mask16),
                                    _mm512_srli_epi32(s1, 16));
    sum1 = _mm512_add_epi32(sum1, _mm512_and_si512(t1, zmm_mask16));
    sum1 = _mm512_add_epi32(sum1, _mm512_srli_epi32(t1, 16));
    sum1 = _mm512_srli_epi32(_mm512_add_epi32(sum1, zmm_2), 2);

    __m512i avg16 =
        _mm512_inserti64x4(_mm512_zextsi256_si512(_mm512_cvtepi32_epi16(sum0)),
                           _mm512_cvtepi32_epi16(sum1), 1);
    avg16 = _mm512_min_epu16(_mm512_mulhi_epu16(avg16, zmm_scale), zmm_255);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm512_cvtepi16_epi8(avg16));
    src_uv += 64;
    dst_uv += 32;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)mask;
    __mmask16 k2 = (__mmask16)(mask >> 16);
    __mmask32 k3 = (__mmask32)mask;
    __m512i s0 = _mm512_maskz_loadu_epi32(k1, src_uv);
    __m512i s1 = _mm512_maskz_loadu_epi32(k2, src_uv + 32);
    __m512i t0 = _mm512_maskz_loadu_epi32(k1, src_uv + src_uv_stride);
    __m512i t1 = _mm512_maskz_loadu_epi32(k2, src_uv + src_uv_stride + 32);
    __m512i sum0 = _mm512_add_epi32(_mm512_and_si512(s0, zmm_mask16),
                                    _mm512_srli_epi32(s0, 16));
    sum0 = _mm512_add_epi32(sum0, _mm512_and_si512(t0, zmm_mask16));
    sum0 = _mm512_add_epi32(sum0, _mm512_srli_epi32(t0, 16));
    sum0 = _mm512_srli_epi32(_mm512_add_epi32(sum0, zmm_2), 2);

    __m512i sum1 = _mm512_add_epi32(_mm512_and_si512(s1, zmm_mask16),
                                    _mm512_srli_epi32(s1, 16));
    sum1 = _mm512_add_epi32(sum1, _mm512_and_si512(t1, zmm_mask16));
    sum1 = _mm512_add_epi32(sum1, _mm512_srli_epi32(t1, 16));
    sum1 = _mm512_srli_epi32(_mm512_add_epi32(sum1, zmm_2), 2);

    __m512i avg16 =
        _mm512_inserti64x4(_mm512_zextsi256_si512(_mm512_cvtepi32_epi16(sum0)),
                           _mm512_cvtepi32_epi16(sum1), 1);
    avg16 = _mm512_min_epu16(_mm512_mulhi_epu16(avg16, zmm_scale), zmm_255);
    _mm256_mask_storeu_epi8(dst_uv, k3, _mm512_cvtepi16_epi8(avg16));
  }
  _mm256_zeroupper();
}
#endif  // HAS_HALFWIDTHROW_16TO8_AVX512BW

#ifdef HAS_SPLITUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void SplitUVRow_AVX512BW(const uint8_t* src_uv,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  while (width >= 64) {
    __m512i uv0 = _mm512_loadu_si512((const __m512i*)src_uv);
    __m512i uv1 = _mm512_loadu_si512((const __m512i*)(src_uv + 64));
    _mm256_storeu_si256((__m256i*)dst_u, _mm512_cvtepi16_epi8(uv0));
    _mm256_storeu_si256((__m256i*)(dst_u + 32), _mm512_cvtepi16_epi8(uv1));
    _mm256_storeu_si256((__m256i*)dst_v,
                        _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8)));
    _mm256_storeu_si256((__m256i*)(dst_v + 32),
                        _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8)));
    src_uv += 128;
    dst_u += 64;
    dst_v += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask32 k0 = (__mmask32)mask;
    __mmask32 k1 = (__mmask32)(mask >> 32);
    __m512i uv0 = _mm512_maskz_loadu_epi16(k0, src_uv);
    _mm256_mask_storeu_epi8(dst_u, k0, _mm512_cvtepi16_epi8(uv0));
    _mm256_mask_storeu_epi8(dst_v, k0,
                            _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv0, 8)));
    if (k1) {
      __m512i uv1 = _mm512_maskz_loadu_epi16(k1, src_uv + 64);
      _mm256_mask_storeu_epi8(dst_u + 32, k1, _mm512_cvtepi16_epi8(uv1));
      _mm256_mask_storeu_epi8(dst_v + 32, k1,
                              _mm512_cvtepi16_epi8(_mm512_srli_epi16(uv1, 8)));
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_SPLITUVROW_AVX512BW

#ifdef HAS_CONVERT8TO16ROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void Convert8To16Row_AVX512BW(const uint8_t* src_y,
                              uint16_t* dst_y,
                              int bits,
                              int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(16 - bits);
  while (width >= 64) {
    __m256i y0_8 = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i y1_8 = _mm256_loadu_si256((const __m256i*)(src_y + 32));
    __m512i y0 = _mm512_cvtepu8_epi16(y0_8);
    __m512i y1 = _mm512_cvtepu8_epi16(y1_8);
    y0 = _mm512_srl_epi16(_mm512_or_si512(y0, _mm512_slli_epi16(y0, 8)),
                          shift128);
    y1 = _mm512_srl_epi16(_mm512_or_si512(y1, _mm512_slli_epi16(y1, 8)),
                          shift128);
    _mm512_storeu_si512((__m512i*)dst_y, y0);
    _mm512_storeu_si512((__m512i*)(dst_y + 32), y1);
    src_y += 64;
    dst_y += 64;
    width -= 64;
  }
  if (width >= 32) {
    __m256i y0_8 = _mm256_loadu_si256((const __m256i*)src_y);
    __m512i y0 = _mm512_cvtepu8_epi16(y0_8);
    y0 = _mm512_srl_epi16(_mm512_or_si512(y0, _mm512_slli_epi16(y0, 8)),
                          shift128);
    _mm512_storeu_si512((__m512i*)dst_y, y0);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i y0 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, src_y));
    y0 = _mm512_srl_epi16(_mm512_or_si512(y0, _mm512_slli_epi16(y0, 8)),
                          shift128);
    _mm512_mask_storeu_epi16(dst_y, k, y0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT8TO16ROW_AVX512BW

#ifdef HAS_J400TOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void J400ToARGBRow_AVX512BW(const uint8_t* src_y,
                            uint8_t* dst_argb,
                            int width) {
  static const uint8_t kShuffleMaskJ400ToARGB_AVX512[64] = {
      0u,  0u,  0u,   128u, 1u,  1u,  1u,   128u, 2u,  2u,  2u,   128u, 3u,
      3u,  3u,  128u, 4u,   4u,  4u,  128u, 5u,   5u,  5u,  128u, 6u,   6u,
      6u,  128u, 7u,  7u,   7u,  128u, 8u,  8u,   8u,  128u, 9u,  9u,   9u,
      128u, 10u, 10u, 10u,  128u, 11u, 11u, 11u,  128u, 12u, 12u, 12u,  128u,
      13u, 13u, 13u,  128u, 14u, 14u, 14u,  128u, 15u, 15u, 15u,  128u};
  const __m512i zmm_mask =
      _mm512_loadu_si512((const __m512i*)kShuffleMaskJ400ToARGB_AVX512);
  const __m512i zmm_alpha = _mm512_set1_epi32((int)0xff000000u);
  while (width >= 32) {
    __m512i z0 =
        _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)src_y));
    __m512i z1 =
        _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)(src_y + 16)));
    z0 = _mm512_or_si512(_mm512_shuffle_epi8(z0, zmm_mask), zmm_alpha);
    z1 = _mm512_or_si512(_mm512_shuffle_epi8(z1, zmm_mask), zmm_alpha);
    _mm512_storeu_si512((__m512i*)dst_argb, z0);
    _mm512_storeu_si512((__m512i*)(dst_argb + 64), z1);
    src_y += 32;
    dst_argb += 128;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __m512i z0 = _mm512_broadcast_i32x4(_mm_maskz_loadu_epi8(k0, src_y));
    z0 = _mm512_or_si512(_mm512_shuffle_epi8(z0, zmm_mask), zmm_alpha);
    _mm512_mask_storeu_epi32(dst_argb, k0, z0);
    if (k1) {
      __m512i z1 = _mm512_broadcast_i32x4(_mm_maskz_loadu_epi8(k1, src_y + 16));
      z1 = _mm512_or_si512(_mm512_shuffle_epi8(z1, zmm_mask), zmm_alpha);
      _mm512_mask_storeu_epi32(dst_argb + 64, k1, z1);
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_J400TOARGBROW_AVX512BW

#ifdef HAS_MERGEUVROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void MergeUVRow_AVX512BW(const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_uv,
                         int width) {
  while (width >= 32) {
    __m512i u16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)src_u));
    __m512i v16 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)src_v));
    __m512i uv = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    _mm512_storeu_si512((__m512i*)dst_uv, uv);
    src_u += 32;
    src_v += 32;
    dst_uv += 64;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i u16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, src_u));
    __m512i v16 = _mm512_cvtepu8_epi16(_mm256_maskz_loadu_epi8(k, src_v));
    __m512i uv = _mm512_or_si512(u16, _mm512_slli_epi16(v16, 8));
    _mm512_mask_storeu_epi16(dst_uv, k, uv);
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEUVROW_AVX512BW

#ifdef HAS_MULTIPLYROW_16_AVX512BW
LIBYUV_TARGET_AVX512BW
void MultiplyRow_16_AVX512BW(const uint16_t* src_y,
                             uint16_t* dst_y,
                             int scale,
                             int width) {
  const __m512i zmm_scale = _mm512_set1_epi16((short)scale);
  while (width >= 64) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src_y);
    __m512i z1 = _mm512_loadu_si512((const __m512i*)(src_y + 32));
    z0 = _mm512_mullo_epi16(z0, zmm_scale);
    z1 = _mm512_mullo_epi16(z1, zmm_scale);
    _mm512_storeu_si512((__m512i*)dst_y, z0);
    _mm512_storeu_si512((__m512i*)(dst_y + 32), z1);
    src_y += 64;
    dst_y += 64;
    width -= 64;
  }
  if (width >= 32) {
    __m512i z0 = _mm512_loadu_si512((const __m512i*)src_y);
    z0 = _mm512_mullo_epi16(z0, zmm_scale);
    _mm512_storeu_si512((__m512i*)dst_y, z0);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  }
  if (width > 0) {
    __mmask32 k = (__mmask32)((1u << width) - 1u);
    __m512i z0 = _mm512_maskz_loadu_epi16(k, src_y);
    z0 = _mm512_mullo_epi16(z0, zmm_scale);
    _mm512_mask_storeu_epi16(dst_y, k, z0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_MULTIPLYROW_16_AVX512BW

#ifdef HAS_ARGBTOYMATRIXROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToYMatrixRow_AVX512BW(const uint8_t* src_argb,
                               uint8_t* dst_y,
                               int width,
                               const struct ArgbConstants* c) {
  const __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);
  const __m512i zmm_80 = _mm512_set1_epi8((char)0x80);
  const __m512i zmm_one = _mm512_set1_epi16(1);
  const __m512i zmm_coeff =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m512i zmm_add =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddY));

  __m512i zmm_bias = _mm512_maddubs_epi16(zmm_coeff, zmm_80);
  zmm_bias = _mm512_madd_epi16(zmm_bias, zmm_one);
  zmm_bias = _mm512_packs_epi32(zmm_bias, zmm_bias);
  zmm_add = _mm512_sub_epi16(zmm_add, zmm_bias);

  while (width >= 64) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm2 = _mm512_loadu_si512((const __m512i*)(src_argb + 128));
    __m512i zmm3 = _mm512_loadu_si512((const __m512i*)(src_argb + 192));

    zmm0 = _mm512_xor_si512(zmm0, zmm_80);
    zmm1 = _mm512_xor_si512(zmm1, zmm_80);
    zmm2 = _mm512_xor_si512(zmm2, zmm_80);
    zmm3 = _mm512_xor_si512(zmm3, zmm_80);

    zmm0 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm0), zmm_one);
    zmm1 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm1), zmm_one);
    zmm0 = _mm512_packs_epi32(zmm0, zmm1);

    zmm2 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm2), zmm_one);
    zmm3 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm3), zmm_one);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);

    zmm0 = _mm512_srli_epi16(_mm512_add_epi16(zmm0, zmm_add), 8);
    zmm2 = _mm512_srli_epi16(_mm512_add_epi16(zmm2, zmm_add), 8);

    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_storeu_si512((__m512i*)dst_y, zmm0);

    src_argb += 256;
    dst_y += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i zmm0 =
        _mm512_xor_si512(_mm512_maskz_loadu_epi32(k0, src_argb), zmm_80);
    __m512i zmm1 =
        _mm512_xor_si512(k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm2 =
        _mm512_xor_si512(k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                            : _mm512_setzero_si512(),
                         zmm_80);
    __m512i zmm3 =
        _mm512_xor_si512(k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                            : _mm512_setzero_si512(),
                         zmm_80);

    zmm0 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm0), zmm_one);
    zmm1 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm1), zmm_one);
    zmm0 = _mm512_packs_epi32(zmm0, zmm1);

    zmm2 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm2), zmm_one);
    zmm3 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm_coeff, zmm3), zmm_one);
    zmm2 = _mm512_packs_epi32(zmm2, zmm3);

    zmm0 = _mm512_srli_epi16(_mm512_add_epi16(zmm0, zmm_add), 8);
    zmm2 = _mm512_srli_epi16(_mm512_add_epi16(zmm2, zmm_add), 8);

    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm2));
    _mm512_mask_storeu_epi8(dst_y, (__mmask64)mask, zmm0);
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOYMATRIXROW_AVX512BW

#ifdef HAS_ARGBTOUV444MATRIXROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToUV444MatrixRow_AVX512BW(const uint8_t* src_argb,
                                   uint8_t* dst_u,
                                   uint8_t* dst_v,
                                   int width,
                                   const struct ArgbConstants* c) {
  const __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  const __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  const __m512i zmm_one = _mm512_set1_epi16(1);
  const __m512i zmm_8000 =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddUV));
  const __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);

  while (width >= 64) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i s2 = _mm512_loadu_si512((const __m512i*)(src_argb + 128));
    __m512i s3 = _mm512_loadu_si512((const __m512i*)(src_argb + 192));

    __m512i u0 = _mm512_madd_epi16(_mm512_maddubs_epi16(s0, zmm_u), zmm_one);
    __m512i u1 = _mm512_madd_epi16(_mm512_maddubs_epi16(s1, zmm_u), zmm_one);
    __m512i u2 = _mm512_madd_epi16(_mm512_maddubs_epi16(s2, zmm_u), zmm_one);
    __m512i u3 = _mm512_madd_epi16(_mm512_maddubs_epi16(s3, zmm_u), zmm_one);
    __m512i u01 =
        _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(u0, u1)),
                          8);
    __m512i u23 =
        _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(u2, u3)),
                          8);
    _mm512_storeu_si512(
        (__m512i*)dst_u,
        PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(u01, u23)));

    __m512i v0 = _mm512_madd_epi16(_mm512_maddubs_epi16(s0, zmm_v), zmm_one);
    __m512i v1 = _mm512_madd_epi16(_mm512_maddubs_epi16(s1, zmm_v), zmm_one);
    __m512i v2 = _mm512_madd_epi16(_mm512_maddubs_epi16(s2, zmm_v), zmm_one);
    __m512i v3 = _mm512_madd_epi16(_mm512_maddubs_epi16(s3, zmm_v), zmm_one);
    __m512i v01 =
        _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(v0, v1)),
                          8);
    __m512i v23 =
        _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(v2, v3)),
                          8);
    _mm512_storeu_si512(
        (__m512i*)dst_v,
        PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(v01, v23)));

    src_argb += 256;
    dst_u += 64;
    dst_v += 64;
    width -= 64;
  }
  if (width > 0) {
    uint64_t mask = (1ULL << width) - 1ULL;
    __mmask16 k0 = (__mmask16)mask;
    __mmask16 k1 = (__mmask16)(mask >> 16);
    __mmask16 k2 = (__mmask16)(mask >> 32);
    __mmask16 k3 = (__mmask16)(mask >> 48);
    __m512i s0 = _mm512_maskz_loadu_epi32(k0, src_argb);
    __m512i s1 = k1 ? _mm512_maskz_loadu_epi32(k1, src_argb + 64)
                    : _mm512_setzero_si512();
    __m512i s2 = k2 ? _mm512_maskz_loadu_epi32(k2, src_argb + 128)
                    : _mm512_setzero_si512();
    __m512i s3 = k3 ? _mm512_maskz_loadu_epi32(k3, src_argb + 192)
                    : _mm512_setzero_si512();

    __m512i u0 = _mm512_madd_epi16(_mm512_maddubs_epi16(s0, zmm_u), zmm_one);
    __m512i u1 = _mm512_madd_epi16(_mm512_maddubs_epi16(s1, zmm_u), zmm_one);
    __m512i u2 = _mm512_madd_epi16(_mm512_maddubs_epi16(s2, zmm_u), zmm_one);
    __m512i u3 = _mm512_madd_epi16(_mm512_maddubs_epi16(s3, zmm_u), zmm_one);
    __m512i u01 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(u0, u1)), 8);
    __m512i u23 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(u2, u3)), 8);
    _mm512_mask_storeu_epi8(
        dst_u, (__mmask64)mask,
        PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(u01, u23)));

    __m512i v0 = _mm512_madd_epi16(_mm512_maddubs_epi16(s0, zmm_v), zmm_one);
    __m512i v1 = _mm512_madd_epi16(_mm512_maddubs_epi16(s1, zmm_v), zmm_one);
    __m512i v2 = _mm512_madd_epi16(_mm512_maddubs_epi16(s2, zmm_v), zmm_one);
    __m512i v3 = _mm512_madd_epi16(_mm512_maddubs_epi16(s3, zmm_v), zmm_one);
    __m512i v01 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(v0, v1)), 8);
    __m512i v23 = _mm512_srli_epi16(
        _mm512_sub_epi16(zmm_8000, _mm512_packs_epi32(v2, v3)), 8);
    _mm512_mask_storeu_epi8(
        dst_v, (__mmask64)mask,
        PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(v01, v23)));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUV444MATRIXROW_AVX512BW

#ifdef HAS_ARGBTOUVMATRIXROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void ARGBToUVMatrixRow_AVX512BW(const uint8_t* src_argb,
                                int src_stride_argb,
                                uint8_t* dst_u,
                                uint8_t* dst_v,
                                int width,
                                const struct ArgbConstants* c) {
  static const uint32_t kPermdARGBToUV_AVX512BW[16] = {
      0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15};
  static const uint8_t kShuffleAARRGGBB_AVX512BW[16] = {
      0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15};
  const __m512i zmm_u =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  const __m512i zmm_v =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  const __m512i zmm_0101 = _mm512_set1_epi16(0x0101);
  const __m512i zmm_one = _mm512_set1_epi16(1);
  const __m512i zmm_8000 =
      _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)c->kAddUV));
  const __m512i zmm_zero = _mm512_setzero_si512();
  const __m512i zmm_shuf = _mm512_broadcast_i32x4(
      _mm_loadu_si128((const __m128i*)kShuffleAARRGGBB_AVX512BW));
  const __m512i zmm_perm_y =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToY_AVX512BW);
  const __m512i zmm_perm_uv =
      _mm512_loadu_si512((const __m512i*)kPermdARGBToUV_AVX512BW);

  while (width >= 32) {
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)src_argb);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(src_argb + 64));
    __m512i zmm2 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb));
    __m512i zmm3 =
        _mm512_loadu_si512((const __m512i*)(src_argb + src_stride_argb + 64));

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_avg_epu16(_mm512_srli_epi16(zmm0, 1), zmm_zero);
    zmm1 = _mm512_avg_epu16(_mm512_srli_epi16(zmm1, 1), zmm_zero);

    zmm0 = PermutexvarEpi32(zmm_perm_uv, _mm512_packus_epi16(zmm0, zmm1));

    zmm1 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm0, zmm_u), zmm_one);
    zmm2 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm0, zmm_v), zmm_one);
    zmm0 = _mm512_packs_epi32(zmm1, zmm2);
    zmm0 = _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, zmm0), 8);
    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm0));

    _mm_storeu_si128((__m128i*)dst_u, _mm512_castsi512_si128(zmm0));
    _mm_storeu_si128((__m128i*)dst_v, _mm512_extracti32x4_epi32(zmm0, 1));

    src_argb += 128;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  }
  if (width > 0) {
    uint32_t mask = (1u << width) - 1u;
    __mmask16 k1 = (__mmask16)mask;
    __mmask16 k2 = (__mmask16)(mask >> 16);
    __mmask16 k3 = ~k1;
    __mmask16 k4 = ~k2;
    __m512i zmm0 = _mm512_maskz_loadu_epi32(k1, src_argb);
    __m512i zmm1 = _mm512_maskz_loadu_epi32(k2, src_argb + 64);
    __m512i zmm2 = _mm512_maskz_loadu_epi32(k1, src_argb + src_stride_argb);
    __m512i zmm3 =
        _mm512_maskz_loadu_epi32(k2, src_argb + src_stride_argb + 64);

    zmm0 = _mm512_mask_shuffle_epi32(zmm0, k3, zmm0, (_MM_PERM_ENUM)0xa0);
    zmm1 = _mm512_mask_shuffle_epi32(zmm1, k4, zmm1, (_MM_PERM_ENUM)0xa0);
    zmm2 = _mm512_mask_shuffle_epi32(zmm2, k3, zmm2, (_MM_PERM_ENUM)0xa0);
    zmm3 = _mm512_mask_shuffle_epi32(zmm3, k4, zmm3, (_MM_PERM_ENUM)0xa0);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_maddubs_epi16(zmm0, zmm_0101);
    zmm1 = _mm512_maddubs_epi16(zmm1, zmm_0101);
    zmm2 = _mm512_maddubs_epi16(zmm2, zmm_0101);
    zmm3 = _mm512_maddubs_epi16(zmm3, zmm_0101);

    zmm0 = _mm512_add_epi16(zmm0, zmm2);
    zmm1 = _mm512_add_epi16(zmm1, zmm3);

    zmm0 = _mm512_avg_epu16(_mm512_srli_epi16(zmm0, 1), zmm_zero);
    zmm1 = _mm512_avg_epu16(_mm512_srli_epi16(zmm1, 1), zmm_zero);

    zmm0 = PermutexvarEpi32(zmm_perm_uv, _mm512_packus_epi16(zmm0, zmm1));

    zmm1 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm0, zmm_u), zmm_one);
    zmm2 = _mm512_madd_epi16(_mm512_maddubs_epi16(zmm0, zmm_v), zmm_one);
    zmm0 = _mm512_packs_epi32(zmm1, zmm2);
    zmm0 = _mm512_srli_epi16(_mm512_sub_epi16(zmm_8000, zmm0), 8);
    zmm0 = PermutexvarEpi32(zmm_perm_y, _mm512_packus_epi16(zmm0, zmm0));

    int halfwidth = (width + 1) >> 1;
    __mmask16 k_out = (__mmask16)((1u << halfwidth) - 1u);
    _mm_mask_storeu_epi8(dst_u, k_out, _mm512_castsi512_si128(zmm0));
    _mm_mask_storeu_epi8(dst_v, k_out, _mm512_extracti32x4_epi32(zmm0, 1));
  }
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX512BW

#endif  // defined(__x86_64__) || defined(_M_X64)

#ifdef HAS_COPYROW_AVX
LIBYUV_TARGET_AVX
void CopyRow_AVX(const uint8_t* src, uint8_t* dst, int width) {
  do {
    __m256i y0 = _mm256_loadu_si256((const __m256i*)src);
    __m256i y1 = _mm256_loadu_si256((const __m256i*)(src + 32));
    _mm256_storeu_si256((__m256i*)dst, y0);
    _mm256_storeu_si256((__m256i*)(dst + 32), y1);
    src += 64;
    dst += 64;
    width -= 64;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_COPYROW_AVX

#ifdef HAS_DETILETOYUY2_AVX2
LIBYUV_TARGET_AVX2
void DetileToYUY2_AVX2(const uint8_t* src_y,
                       ptrdiff_t src_y_tile_stride,
                       const uint8_t* src_uv,
                       ptrdiff_t src_uv_tile_stride,
                       uint8_t* dst_yuy2,
                       int width) {
  do {
    __m128i y0 = _mm_loadu_si128((const __m128i*)src_y);
    __m128i y1 = _mm_loadu_si128((const __m128i*)(src_y + src_y_tile_stride));
    __m128i uv0 = _mm_loadu_si128((const __m128i*)src_uv);
    __m128i uv1 =
        _mm_loadu_si128((const __m128i*)(src_uv + src_uv_tile_stride));
    __m256i y = _mm256_inserti128_si256(_mm256_castsi128_si256(y0), y1, 1);
    __m256i uv = _mm256_inserti128_si256(_mm256_castsi128_si256(uv0), uv1, 1);
    __m256i lo = _mm256_unpacklo_epi8(y, uv);
    __m256i hi = _mm256_unpackhi_epi8(y, uv);
    _mm256_storeu_si256((__m256i*)dst_yuy2,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_yuy2 + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_y += 2 * src_y_tile_stride;
    src_uv += 2 * src_uv_tile_stride;
    dst_yuy2 += 64;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_DETILETOYUY2_AVX2

#ifdef HAS_UNPACKMT2T_AVX2
LIBYUV_TARGET_AVX2
void UnpackMT2T_AVX2(const uint8_t* src, uint16_t* dst, size_t size) {
  const __m256i mask_00c0 = _mm256_set1_epi16(0x00c0);
  while (size >= 80) {
    __m256i low16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src));
    __m256i l0 = _mm256_and_si256(_mm256_slli_epi16(low16, 6), mask_00c0);
    __m256i l1 = _mm256_and_si256(_mm256_slli_epi16(low16, 4), mask_00c0);
    __m256i l2 = _mm256_and_si256(_mm256_slli_epi16(low16, 2), mask_00c0);
    __m256i l3 = _mm256_and_si256(low16, mask_00c0);

    __m256i u0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 16)));
    __m256i u1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 32)));
    __m256i u2 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 48)));
    __m256i u3 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src + 64)));

    u0 = _mm256_or_si256(
        _mm256_or_si256(_mm256_slli_epi16(u0, 8), _mm256_srli_epi16(u0, 2)),
        l0);
    u1 = _mm256_or_si256(
        _mm256_or_si256(_mm256_slli_epi16(u1, 8), _mm256_srli_epi16(u1, 2)),
        l1);
    u2 = _mm256_or_si256(
        _mm256_or_si256(_mm256_slli_epi16(u2, 8), _mm256_srli_epi16(u2, 2)),
        l2);
    u3 = _mm256_or_si256(
        _mm256_or_si256(_mm256_slli_epi16(u3, 8), _mm256_srli_epi16(u3, 2)),
        l3);

    _mm256_storeu_si256((__m256i*)dst, u0);
    _mm256_storeu_si256((__m256i*)(dst + 16), u1);
    _mm256_storeu_si256((__m256i*)(dst + 32), u2);
    _mm256_storeu_si256((__m256i*)(dst + 48), u3);

    src += 80;
    dst += 64;
    size -= 80;
  }
  _mm256_zeroupper();
}
#endif  // HAS_UNPACKMT2T_AVX2

#ifdef HAS_ARGBGRAYROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBGrayRow_AVX2(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  static const uint8_t kShufGrayDword[16] = {0, 0, 0, 0x80, 4,  4,  4,  0x80,
                                             8, 8, 8, 0x80, 12, 12, 12, 0x80};
  const __m256i c_coeff = _mm256_set1_epi32(0x004d961d);
  const __m256i c_sub128 = _mm256_set1_epi8((char)0x80);
  const __m256i c_one16 = _mm256_set1_epi16(1);
  const __m256i c_bias = _mm256_set1_epi32(32896);
  const __m256i c_shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufGrayDword));
  const __m256i c_alpha = _mm256_set1_epi32((int)0xff000000u);

  while (width >= 8) {
    __m256i s = _mm256_loadu_si256((const __m256i*)src_argb);
    src_argb += 32;
    __m256i y16 = _mm256_maddubs_epi16(c_coeff, _mm256_sub_epi8(s, c_sub128));
    __m256i y32 = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_madd_epi16(y16, c_one16), c_bias), 8);
    __m256i out = _mm256_or_si256(_mm256_and_si256(s, c_alpha),
                                  _mm256_shuffle_epi8(y32, c_shuf));
    _mm256_storeu_si256((__m256i*)dst_argb, out);
    dst_argb += 32;
    width -= 8;
  }
  _mm256_zeroupper();
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ARGBGrayRow_C(src_argb, dst_argb, width);
  }
}
#endif  // HAS_ARGBGRAYROW_AVX2

#ifdef HAS_MERGERGBROW_AVX2
LIBYUV_TARGET_AVX2
void MergeRGBRow_AVX2(const uint8_t* src_r,
                      const uint8_t* src_g,
                      const uint8_t* src_b,
                      uint8_t* dst_rgb,
                      int width) {
  static const uint8_t kMergeRGBShuf[5][16] = {
      {0u, 11u, 6u, 1u, 12u, 7u, 2u, 13u, 8u, 3u, 14u, 9u, 4u, 15u, 10u, 5u},
      {5u, 0u, 11u, 6u, 1u, 12u, 7u, 2u, 13u, 8u, 3u, 14u, 9u, 4u, 15u, 10u},
      {10u, 5u, 0u, 11u, 6u, 1u, 12u, 7u, 2u, 13u, 8u, 3u, 14u, 9u, 4u, 15u},
      {0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u,
       0u},
      {0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u,
       0u}};
  const __m256i shuf_r = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kMergeRGBShuf[0]));
  const __m256i shuf_g = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kMergeRGBShuf[1]));
  const __m256i shuf_b = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kMergeRGBShuf[2]));
  const __m256i m1 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kMergeRGBShuf[3]));
  const __m256i m2 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kMergeRGBShuf[4]));

  do {
    __m256i r =
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_r), shuf_r);
    __m256i g =
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_g), shuf_g);
    __m256i b =
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_b), shuf_b);

    __m256i c0 = _mm256_blendv_epi8(_mm256_blendv_epi8(r, g, m1), b, m2);
    __m256i c1 = _mm256_blendv_epi8(_mm256_blendv_epi8(g, b, m1), r, m2);
    __m256i c2 = _mm256_blendv_epi8(_mm256_blendv_epi8(b, r, m1), g, m2);

    _mm256_storeu_si256((__m256i*)dst_rgb,
                        _mm256_permute2x128_si256(c0, c1, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_rgb + 32),
                        _mm256_permute2x128_si256(c2, c0, 0x30));
    _mm256_storeu_si256((__m256i*)(dst_rgb + 64),
                        _mm256_permute2x128_si256(c1, c2, 0x31));

    src_r += 32;
    src_g += 32;
    src_b += 32;
    dst_rgb += 96;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGERGBROW_AVX2

#ifdef HAS_RAWTORGBAROW_AVX2
LIBYUV_TARGET_AVX2
void RAWToRGBARow_AVX2(const uint8_t* src_raw, uint8_t* dst_rgba, int width) {
  static const uint8_t kShufRAWToRGBA_Lo[16] = {0x80, 2u,  1u,  0u,  0x80, 5u,
                                                4u,   3u,  0x80, 8u,  7u,  6u,
                                                0x80, 11u, 10u,  9u};
  static const uint8_t kShufRAWToRGBA_Hi[16] = {0x80, 6u,  5u,  4u,  0x80, 9u,
                                                8u,   7u,  0x80, 12u, 11u, 10u,
                                                0x80, 15u, 14u,  13u};
  const __m256i shuf_lo = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufRAWToRGBA_Lo));
  const __m256i shuf_hi = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufRAWToRGBA_Hi));
  const __m256i alpha = _mm256_set1_epi32(0x000000ff);

  do {
    __m128i s0 = _mm_loadu_si128((const __m128i*)src_raw);
    __m128i s1 = _mm_loadu_si128((const __m128i*)(src_raw + 12));
    __m128i s2 = _mm_loadu_si128((const __m128i*)(src_raw + 20));
    __m128i s3 = _mm_loadu_si128((const __m128i*)(src_raw + 32));
    __m256i y0 = _mm256_inserti128_si256(_mm256_castsi128_si256(s0), s1, 1);
    __m256i y1 = _mm256_inserti128_si256(_mm256_castsi128_si256(s2), s3, 1);
    y0 = _mm256_or_si256(_mm256_shuffle_epi8(y0, shuf_lo), alpha);
    y1 = _mm256_or_si256(_mm256_shuffle_epi8(y1, shuf_hi), alpha);
    _mm256_storeu_si256((__m256i*)dst_rgba, y0);
    _mm256_storeu_si256((__m256i*)(dst_rgba + 32), y1);
    src_raw += 48;
    dst_rgba += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_RAWTORGBAROW_AVX2

#ifdef HAS_HALFROW_AVX2
LIBYUV_TARGET_AVX2
void HalfRow_AVX2(const uint8_t* src_uv,
                  ptrdiff_t src_uv_stride,
                  uint8_t* dst_uv,
                  int width) {
  do {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride));
    _mm256_storeu_si256((__m256i*)dst_uv, _mm256_avg_epu8(r0, r1));
    src_uv += 32;
    dst_uv += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_HALFROW_AVX2

#ifdef HAS_HALFROW_16_AVX2
LIBYUV_TARGET_AVX2
void HalfRow_16_AVX2(const uint16_t* src_uv,
                     ptrdiff_t src_uv_stride,
                     uint16_t* dst_uv,
                     int width) {
  do {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride));
    _mm256_storeu_si256((__m256i*)dst_uv, _mm256_avg_epu16(r0, r1));
    src_uv += 16;
    dst_uv += 16;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_HALFROW_16_AVX2

#ifdef HAS_HALFMERGEUVROW_AVX2
LIBYUV_TARGET_AVX2
void HalfMergeUVRow_AVX2(const uint8_t* src_u,
                         int src_stride_u,
                         const uint8_t* src_v,
                         int src_stride_v,
                         uint8_t* dst_uv,
                         int width) {
  const __m256i ymm_0101 = _mm256_set1_epi16(0x0101);
  const __m256i ymm_zero = _mm256_setzero_si256();
  do {
    __m256i u0 = _mm256_loadu_si256((const __m256i*)src_u);
    __m256i v0 = _mm256_loadu_si256((const __m256i*)src_v);
    __m256i u1 = _mm256_loadu_si256((const __m256i*)(src_u + src_stride_u));
    __m256i v1 = _mm256_loadu_si256((const __m256i*)(src_v + src_stride_v));
    u0 = _mm256_add_epi16(_mm256_maddubs_epi16(u0, ymm_0101),
                          _mm256_maddubs_epi16(u1, ymm_0101));
    v0 = _mm256_add_epi16(_mm256_maddubs_epi16(v0, ymm_0101),
                          _mm256_maddubs_epi16(v1, ymm_0101));
    u0 = _mm256_avg_epu16(_mm256_srli_epi16(u0, 1), ymm_zero);
    v0 = _mm256_avg_epu16(_mm256_srli_epi16(v0, 1), ymm_zero);
    u0 = _mm256_packus_epi16(u0, u0);
    v0 = _mm256_packus_epi16(v0, v0);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm256_unpacklo_epi8(u0, v0));
    src_u += 32;
    src_v += 32;
    dst_uv += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_HALFMERGEUVROW_AVX2

#ifdef HAS_CONVERT8TO8ROW_AVX2
LIBYUV_TARGET_AVX2
void Convert8To8Row_AVX2(const uint8_t* src_y,
                         uint8_t* dst_y,
                         int scale,
                         int bias,
                         int width) {
  const __m256i ymm_scale =
      _mm256_slli_epi16(_mm256_set1_epi16((short)scale), 8);
  const __m256i ymm_bias = _mm256_set1_epi8((char)bias);
  const __m256i ymm_zero = _mm256_setzero_si256();
  do {
    __m256i y = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i lo =
        _mm256_mulhi_epu16(_mm256_unpacklo_epi8(y, ymm_zero), ymm_scale);
    __m256i hi =
        _mm256_mulhi_epu16(_mm256_unpackhi_epi8(y, ymm_zero), ymm_scale);
    __m256i out = _mm256_add_epi8(_mm256_packus_epi16(lo, hi), ymm_bias);
    _mm256_storeu_si256((__m256i*)dst_y, out);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT8TO8ROW_AVX2

#ifdef HAS_DIVIDEROW_16_AVX2
LIBYUV_TARGET_AVX2
void DivideRow_16_AVX2(const uint16_t* src_y,
                       uint16_t* dst_y,
                       int scale,
                       int width) {
  if (scale == 65536) {  // Divide by 1, so do memcpy.
    if (src_y != dst_y) {
      memcpy(dst_y, src_y, (size_t)width * 2);
    }
    return;
  }
  const __m256i ymm_scale = _mm256_set1_epi16((short)scale);
  while (width > 0) {
    __m256i v0 = _mm256_mulhi_epu16(_mm256_loadu_si256((const __m256i*)src_y),
                                    ymm_scale);
    __m256i v1 = _mm256_mulhi_epu16(
        _mm256_loadu_si256((const __m256i*)(src_y + 16)), ymm_scale);
    _mm256_storeu_si256((__m256i*)dst_y, v0);
    _mm256_storeu_si256((__m256i*)(dst_y + 16), v1);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  }
  _mm256_zeroupper();
}
#endif  // HAS_DIVIDEROW_16_AVX2

#ifdef HAS_CONVERT16TO8ROW_AVX2
LIBYUV_TARGET_AVX2
void Convert16To8Row_AVX2(const uint16_t* src_y,
                          uint8_t* dst_y,
                          int scale,
                          int width) {
  const __m256i ymm_scale = _mm256_set1_epi16((short)scale);
  const __m256i ymm_255 = _mm256_set1_epi16(255);
  do {
    __m256i y0 = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i y1 = _mm256_loadu_si256((const __m256i*)(src_y + 16));
    y0 = _mm256_min_epu16(_mm256_mulhi_epu16(y0, ymm_scale), ymm_255);
    y1 = _mm256_min_epu16(_mm256_mulhi_epu16(y1, ymm_scale), ymm_255);
    __m256i out = _mm256_permute4x64_epi64(_mm256_packus_epi16(y0, y1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_y, out);
    src_y += 32;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_CONVERT16TO8ROW_AVX2

#ifdef HAS_HALFROW_16TO8_AVX2
LIBYUV_TARGET_AVX2
void HalfRow_16To8_AVX2(const uint16_t* src_uv,
                        ptrdiff_t src_uv_stride,
                        uint8_t* dst_uv,
                        int scale,
                        int width) {
  const __m256i ymm_scale = _mm256_set1_epi16((short)scale);
  const __m256i ymm_255 = _mm256_set1_epi16(255);
  do {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_uv + 16));
    __m256i s0 = _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride));
    __m256i s1 =
        _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride + 16));
    r0 = _mm256_min_epu16(_mm256_mulhi_epu16(_mm256_avg_epu16(r0, s0), ymm_scale),
                          ymm_255);
    r1 = _mm256_min_epu16(_mm256_mulhi_epu16(_mm256_avg_epu16(r1, s1), ymm_scale),
                          ymm_255);
    __m256i out = _mm256_permute4x64_epi64(_mm256_packus_epi16(r0, r1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_uv, out);
    src_uv += 32;
    dst_uv += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_HALFROW_16TO8_AVX2

#ifdef HAS_HALFWIDTHROW_16TO8_AVX2
LIBYUV_TARGET_AVX2
void HalfWidthRow_16To8_AVX2(const uint16_t* src_uv,
                             ptrdiff_t src_uv_stride,
                             uint8_t* dst_uv,
                             int scale,
                             int width) {
  const __m256i ymm_mask16 = _mm256_set1_epi32(0x0000ffff);
  const __m256i ymm_2 = _mm256_set1_epi32(2);
  const __m256i ymm_scale = _mm256_set1_epi16((short)scale);
  const __m256i ymm_255 = _mm256_set1_epi16(255);
  do {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i s1 = _mm256_loadu_si256((const __m256i*)(src_uv + 16));
    __m256i t0 = _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride));
    __m256i t1 =
        _mm256_loadu_si256((const __m256i*)(src_uv + src_uv_stride + 16));

    __m256i sum0 = _mm256_add_epi32(_mm256_and_si256(s0, ymm_mask16),
                                    _mm256_srli_epi32(s0, 16));
    sum0 = _mm256_add_epi32(sum0, _mm256_and_si256(t0, ymm_mask16));
    sum0 = _mm256_add_epi32(sum0, _mm256_srli_epi32(t0, 16));
    sum0 = _mm256_srli_epi32(_mm256_add_epi32(sum0, ymm_2), 2);

    __m256i sum1 = _mm256_add_epi32(_mm256_and_si256(s1, ymm_mask16),
                                    _mm256_srli_epi32(s1, 16));
    sum1 = _mm256_add_epi32(sum1, _mm256_and_si256(t1, ymm_mask16));
    sum1 = _mm256_add_epi32(sum1, _mm256_srli_epi32(t1, 16));
    sum1 = _mm256_srli_epi32(_mm256_add_epi32(sum1, ymm_2), 2);

    __m256i avg16 =
        _mm256_permute4x64_epi64(_mm256_packus_epi32(sum0, sum1), 0xd8);
    avg16 = _mm256_min_epu16(_mm256_mulhi_epu16(avg16, ymm_scale), ymm_255);
    __m256i out =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(avg16, avg16), 0xd8);
    _mm_storeu_si128((__m128i*)dst_uv, _mm256_castsi256_si128(out));
    src_uv += 32;
    dst_uv += 16;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_HALFWIDTHROW_16TO8_AVX2

#ifdef HAS_HALFWIDTHROW_16TO8_ODD_AVX2
LIBYUV_TARGET_AVX2
void HalfWidthRow_16To8_Odd_AVX2(const uint16_t* src_uv,
                                 ptrdiff_t src_uv_stride,
                                 uint8_t* dst_uv,
                                 int scale,
                                 int width) {
  int n = (width - 1) & ~15;
  int r = (width - 1) & 15;
  SIMD_ALIGNED(uint16_t pair[64]);
  SIMD_ALIGNED(uint8_t out[16]);
  if (n > 0) {
    HalfWidthRow_16To8_AVX2(src_uv, src_uv_stride, dst_uv, scale, n);
    src_uv += n * 2;
    dst_uv += n;
  }
  if (r > 0) {
    HalfWidthRow_16To8_Any_AVX2(src_uv, src_uv_stride, dst_uv, scale, r);
    src_uv += r * 2;
    dst_uv += r;
  }
  memset(pair, 0, sizeof(pair));
  pair[0] = src_uv[0];
  pair[1] = src_uv[0];
  pair[32] = src_uv[src_uv_stride];
  pair[33] = src_uv[src_uv_stride];
  HalfWidthRow_16To8_AVX2(pair, 32, out, scale, 16);
  dst_uv[0] = out[0];
}
#endif  // HAS_HALFWIDTHROW_16TO8_ODD_AVX2

#ifdef HAS_GAUSSCOL_AVX2
LIBYUV_TARGET_AVX2
void GaussCol_AVX2(const uint16_t* src0,
                   const uint16_t* src1,
                   const uint16_t* src2,
                   const uint16_t* src3,
                   const uint16_t* src4,
                   uint32_t* dst,
                   int width) {
  while (width >= 8) {
    __m256i v0 = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src0));
    __m256i v1 = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src1));
    __m256i v2 = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src2));
    __m256i v3 = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src3));
    __m256i v4 = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src4));
    __m256i v04 = _mm256_add_epi32(v0, v4);
    __m256i v13 = _mm256_slli_epi32(_mm256_add_epi32(v1, v3), 2);
    __m256i v2_6 =
        _mm256_add_epi32(_mm256_slli_epi32(v2, 2), _mm256_slli_epi32(v2, 1));
    _mm256_storeu_si256((__m256i*)dst,
                        _mm256_add_epi32(_mm256_add_epi32(v04, v13), v2_6));
    src0 += 8;
    src1 += 8;
    src2 += 8;
    src3 += 8;
    src4 += 8;
    dst += 8;
    width -= 8;
  }
  _mm256_zeroupper();
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    GaussCol_C(src0, src1, src2, src3, src4, dst, width);
  }
}
#endif  // HAS_GAUSSCOL_AVX2

#ifdef HAS_GAUSSCOL_F32_AVX2
LIBYUV_TARGET_AVX2
void GaussCol_F32_AVX2(const float* src0,
                       const float* src1,
                       const float* src2,
                       const float* src3,
                       const float* src4,
                       float* dst,
                       int width) {
  const __m256 c4 = _mm256_set1_ps(4.0f);
  const __m256 c6 = _mm256_set1_ps(6.0f);
  while (width >= 8) {
    __m256 s0 = _mm256_loadu_ps(src0);
    __m256 s1 = _mm256_loadu_ps(src1);
    __m256 s2 = _mm256_loadu_ps(src2);
    __m256 s3 = _mm256_loadu_ps(src3);
    __m256 s4 = _mm256_loadu_ps(src4);
    __m256 sum = _mm256_add_ps(s0, _mm256_mul_ps(s1, c4));
    sum = _mm256_add_ps(sum, _mm256_mul_ps(s2, c6));
    sum = _mm256_add_ps(sum, _mm256_mul_ps(s3, c4));
    sum = _mm256_add_ps(sum, s4);
    _mm256_storeu_ps(dst, sum);
    src0 += 8;
    src1 += 8;
    src2 += 8;
    src3 += 8;
    src4 += 8;
    dst += 8;
    width -= 8;
  }
  _mm256_zeroupper();
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    GaussCol_F32_C(src0, src1, src2, src3, src4, dst, width);
  }
}
#endif  // HAS_GAUSSCOL_F32_AVX2

#ifdef HAS_SCALEMAXSAMPLES_AVX2
LIBYUV_TARGET_AVX2
float ScaleMaxSamples_AVX2(const float* src,
                           float* dst,
                           float scale,
                           int width) {
  __m256 vmax = _mm256_setzero_ps();
  const __m256 vscale = _mm256_set1_ps(scale);
  while (width >= 8) {
    __m256 v = _mm256_loadu_ps(src);
    src += 8;
    vmax = _mm256_max_ps(v, vmax);
    _mm256_storeu_ps(dst, _mm256_mul_ps(v, vscale));
    dst += 8;
    width -= 8;
  }
  __m128 m128 =
      _mm_max_ps(_mm256_extractf128_ps(vmax, 1), _mm256_castps256_ps128(vmax));
  m128 = _mm_max_ps(_mm_movehl_ps(m128, m128), m128);
  m128 = _mm_max_ss(_mm_shuffle_ps(m128, m128, 1), m128);
  float fmax = _mm_cvtss_f32(m128);
  _mm256_zeroupper();
  // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
  for (; width > 0; --width) {
    float v = *src++;
    float vs = v * scale;
    fmax = (v > fmax) ? v : fmax;
    *dst++ = vs;
  }
  return fmax;
}
#endif  // HAS_SCALEMAXSAMPLES_AVX2

#ifdef HAS_SCALESUMSAMPLES_AVX2
LIBYUV_TARGET_AVX2
float ScaleSumSamples_AVX2(const float* src,
                           float* dst,
                           float scale,
                           int width) {
  __m256 vsum = _mm256_setzero_ps();
  const __m256 vscale = _mm256_set1_ps(scale);
  while (width >= 8) {
    __m256 v = _mm256_loadu_ps(src);
    src += 8;
    vsum = _mm256_add_ps(vsum, _mm256_mul_ps(v, v));
    _mm256_storeu_ps(dst, _mm256_mul_ps(v, vscale));
    dst += 8;
    width -= 8;
  }
  __m128 s128 =
      _mm_add_ps(_mm256_castps256_ps128(vsum), _mm256_extractf128_ps(vsum, 1));
  s128 = _mm_add_ps(s128, _mm_movehl_ps(s128, s128));
  s128 = _mm_add_ss(s128, _mm_shuffle_ps(s128, s128, 1));
  float fsum = _mm_cvtss_f32(s128);
  _mm256_zeroupper();
  // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
  for (; width > 0; --width) {
    float v = *src++;
    fsum += v * v;
    *dst++ = v * scale;
  }
  return fsum;
}
#endif  // HAS_SCALESUMSAMPLES_AVX2

#ifdef HAS_SCALESAMPLES_AVX2
LIBYUV_TARGET_AVX2
void ScaleSamples_AVX2(const float* src, float* dst, float scale, int width) {
  const __m256 vscale = _mm256_set1_ps(scale);
  while (width >= 8) {
    __m256 v = _mm256_loadu_ps(src);
    src += 8;
    _mm256_storeu_ps(dst, _mm256_mul_ps(v, vscale));
    dst += 8;
    width -= 8;
  }
  _mm256_zeroupper();
  if (width > 0) {
    // TODO(fbarchard): Handle remainder with masking or SIMD Any function.
    ScaleSamples_C(src, dst, scale, width);
  }
}
#endif  // HAS_SCALESAMPLES_AVX2

#if defined(HAS_ARGBTORGB24ROW_AVX2) || defined(HAS_ARGBTORAWROW_AVX2)
LIBYUV_TARGET_AVX2
static inline void ARGBToRGBRow_AVX2(const uint8_t* src,
                                     uint8_t* dst,
                                     int width,
                                     const uint8_t* shuffler) {
  static const uint32_t kPermdRGB24_AVX[8] = {0, 1, 2, 4, 5, 6, 3, 7};
  const __m256i ymm_shuf =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)shuffler));
  const __m256i ymm_perm =
      _mm256_loadu_si256((const __m256i*)kPermdRGB24_AVX);
  do {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src + 64));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src + 96));
    src += 128;

    ymm0 = _mm256_permutevar8x32_epi32(_mm256_shuffle_epi8(ymm0, ymm_shuf),
                                       ymm_perm);
    ymm1 = _mm256_permutevar8x32_epi32(_mm256_shuffle_epi8(ymm1, ymm_shuf),
                                       ymm_perm);
    ymm2 = _mm256_permutevar8x32_epi32(_mm256_shuffle_epi8(ymm2, ymm_shuf),
                                       ymm_perm);
    ymm3 = _mm256_permutevar8x32_epi32(_mm256_shuffle_epi8(ymm3, ymm_shuf),
                                       ymm_perm);

    _mm256_storeu_si256(
        (__m256i*)dst,
        _mm256_or_si256(ymm0, _mm256_permute4x64_epi64(ymm1, 0x3f)));
    _mm256_storeu_si256(
        (__m256i*)(dst + 32),
        _mm256_or_si256(_mm256_permute4x64_epi64(ymm1, 0xf9),
                        _mm256_permute4x64_epi64(ymm2, 0x4f)));
    _mm256_storeu_si256(
        (__m256i*)(dst + 64),
        _mm256_or_si256(_mm256_permute4x64_epi64(ymm2, 0xfe),
                        _mm256_permute4x64_epi64(ymm3, 0x93)));
    dst += 96;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // defined(HAS_ARGBTORGB24ROW_AVX2) || defined(HAS_ARGBTORAWROW_AVX2)

#ifdef HAS_ARGBTORGB24ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToRGB24Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  ARGBToRGBRow_AVX2(src, dst, width, kShuffleMaskARGBToRGB24[0]);
}
#endif  // HAS_ARGBTORGB24ROW_AVX2

#ifdef HAS_ARGBTORAWROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToRAWRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  static const uint8_t kShuffleMaskARGBToRAW_AVX2[16] = {
      2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 8u, 14u, 13u, 12u, 128u, 128u, 128u,
      128u};
  ARGBToRGBRow_AVX2(src, dst, width, kShuffleMaskARGBToRAW_AVX2);
}
#endif  // HAS_ARGBTORAWROW_AVX2

#ifdef HAS_SPLITUVROW_AVX2
LIBYUV_TARGET_AVX2
void SplitUVRow_AVX2(const uint8_t* src_uv,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i uv0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i uv1 = _mm256_loadu_si256((const __m256i*)(src_uv + 32));
    __m256i u = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_and_si256(uv0, mask_00ff),
                            _mm256_and_si256(uv1, mask_00ff)),
        0xd8);
    __m256i v = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_srli_epi16(uv0, 8),
                            _mm256_srli_epi16(uv1, 8)),
        0xd8);
    _mm256_storeu_si256((__m256i*)dst_u, u);
    _mm256_storeu_si256((__m256i*)dst_v, v);
    src_uv += 64;
    dst_u += 32;
    dst_v += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_SPLITUVROW_AVX2

#ifdef HAS_AR64TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void AR64ToARGBRow_AVX2(const uint16_t* src_ar64,
                        uint8_t* dst_argb,
                        int width) {
  do {
    __m256i s0 = _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)src_ar64),
                                   8);
    __m256i s1 = _mm256_srli_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ar64 + 16)), 8);
    __m256i out = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, out);
    src_ar64 += 32;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_AR64TOARGBROW_AVX2

#ifdef HAS_AB64TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void AB64ToARGBRow_AVX2(const uint16_t* src_ab64,
                        uint8_t* dst_argb,
                        int width) {
  static const uint8_t kShufSwapRB[16] = {2u, 1u, 0u, 3u,  6u,  5u,  4u,  7u,
                                          10u, 9u, 8u, 11u, 14u, 13u, 12u, 15u};
  const __m256i shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufSwapRB));
  do {
    __m256i s0 = _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)src_ab64),
                                   8);
    __m256i s1 = _mm256_srli_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ab64 + 16)), 8);
    __m256i out = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8);
    out = _mm256_shuffle_epi8(out, shuf);
    _mm256_storeu_si256((__m256i*)dst_argb, out);
    src_ab64 += 32;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_AB64TOARGBROW_AVX2

#ifdef HAS_ARGBTOAR64ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToAR64Row_AVX2(const uint8_t* src_argb,
                        uint16_t* dst_ar64,
                        int width) {
  do {
    __m256i s = _mm256_permute4x64_epi64(
        _mm256_loadu_si256((const __m256i*)src_argb), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ar64, _mm256_unpacklo_epi8(s, s));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 16), _mm256_unpackhi_epi8(s, s));
    src_argb += 32;
    dst_ar64 += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOAR64ROW_AVX2

#ifdef HAS_ARGBTOAB64ROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBToAB64Row_AVX2(const uint8_t* src_argb,
                        uint16_t* dst_ab64,
                        int width) {
  static const uint8_t kShufSwapRB[16] = {2u, 1u, 0u, 3u,  6u,  5u,  4u,  7u,
                                          10u, 9u, 8u, 11u, 14u, 13u, 12u, 15u};
  const __m256i shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufSwapRB));
  do {
    __m256i s = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)src_argb), shuf);
    s = _mm256_permute4x64_epi64(s, 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ab64, _mm256_unpacklo_epi8(s, s));
    _mm256_storeu_si256((__m256i*)(dst_ab64 + 16), _mm256_unpackhi_epi8(s, s));
    src_argb += 32;
    dst_ab64 += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBTOAB64ROW_AVX2

#ifdef HAS_ARGBADDROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBAddRow_AVX2(const uint8_t* src_argb,
                     const uint8_t* src_argb1,
                     uint8_t* dst_argb,
                     int width) {
  do {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i s1 = _mm256_loadu_si256((const __m256i*)src_argb1);
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_adds_epu8(s0, s1));
    src_argb += 32;
    src_argb1 += 32;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBADDROW_AVX2

#ifdef HAS_ARGBSUBTRACTROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBSubtractRow_AVX2(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  do {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i s1 = _mm256_loadu_si256((const __m256i*)src_argb1);
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_subs_epu8(s0, s1));
    src_argb += 32;
    src_argb1 += 32;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBSUBTRACTROW_AVX2

#ifdef HAS_ARGBCOPYALPHAROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBCopyAlphaRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  const __m256i mask_alpha = _mm256_set1_epi32((int)0xff000000u);
  do {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src);
    __m256i s1 = _mm256_loadu_si256((const __m256i*)(src + 32));
    __m256i d0 = _mm256_loadu_si256((const __m256i*)dst);
    __m256i d1 = _mm256_loadu_si256((const __m256i*)(dst + 32));
    _mm256_storeu_si256((__m256i*)dst, _mm256_blendv_epi8(d0, s0, mask_alpha));
    _mm256_storeu_si256((__m256i*)(dst + 32),
                        _mm256_blendv_epi8(d1, s1, mask_alpha));
    src += 64;
    dst += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBCOPYALPHAROW_AVX2

#ifdef HAS_ARGBPOLYNOMIALROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBPolynomialRow_AVX2(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            const float* poly,
                            int width) {
  const __m256 c0 = _mm256_broadcast_ps((const __m128*)(poly + 0));
  const __m256 c1 = _mm256_broadcast_ps((const __m128*)(poly + 4));
  const __m256 c2 = _mm256_broadcast_ps((const __m128*)(poly + 8));
  const __m256 c3 = _mm256_broadcast_ps((const __m128*)(poly + 12));
  do {
    __m128i s8 = _mm_loadl_epi64((const __m128i*)src_argb);
    src_argb += 8;
    __m256 x = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(s8));
    __m256 x2 = _mm256_mul_ps(x, x);
    __m256 res = _mm256_add_ps(c0, _mm256_mul_ps(c1, x));
    res = _mm256_add_ps(res, _mm256_mul_ps(c2, x2));
    res = _mm256_add_ps(res, _mm256_mul_ps(_mm256_mul_ps(c3, x), x2));
    __m256i i32 = _mm256_cvttps_epi32(res);
    __m256i u16 = _mm256_permute4x64_epi64(_mm256_packs_epi32(i32, i32), 0xd8);
    __m128i u8 = _mm_packus_epi16(_mm256_castsi256_si128(u16),
                                  _mm256_castsi256_si128(u16));
    _mm_storel_epi64((__m128i*)dst_argb, u8);
    dst_argb += 8;
    width -= 2;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBPOLYNOMIALROW_AVX2

#ifdef HAS_ARGBUNATTENUATEROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBUnattenuateRow_AVX2(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             int width) {
  static const uint8_t kUnattenShuffleAlpha[16] = {
      0u, 1u, 0u, 1u, 0u, 1u, 6u, 7u, 8u, 9u, 8u, 9u, 8u, 9u, 14u, 15u};
  const __m256i ymm_shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kUnattenShuffleAlpha));
  do {
    __m256i ymm3 = _mm256_setr_epi32(
        (int)fixed_invtbl8[src_argb[3]], (int)fixed_invtbl8[src_argb[7]],
        (int)fixed_invtbl8[src_argb[11]], (int)fixed_invtbl8[src_argb[15]],
        (int)fixed_invtbl8[src_argb[19]], (int)fixed_invtbl8[src_argb[23]],
        (int)fixed_invtbl8[src_argb[27]], (int)fixed_invtbl8[src_argb[31]]);
    __m256i ymm6 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm0 = _mm256_unpacklo_epi8(ymm6, ymm6);
    __m256i ymm1 = _mm256_unpackhi_epi8(ymm6, ymm6);
    __m256i ymm2 =
        _mm256_shuffle_epi8(_mm256_unpacklo_epi16(ymm3, ymm3), ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(_mm256_unpackhi_epi16(ymm3, ymm3), ymm_shuf);
    ymm0 = _mm256_mulhi_epu16(ymm0, ymm2);
    ymm1 = _mm256_mulhi_epu16(ymm1, ymm3);
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_packus_epi16(ymm0, ymm1));
    src_argb += 32;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_ARGBUNATTENUATEROW_AVX2

#ifdef HAS_COMPUTECUMULATIVESUMROW_AVX2
LIBYUV_TARGET_AVX2
void ComputeCumulativeSumRow_AVX2(const uint8_t* row,
                                  int32_t* cumsum,
                                  const int32_t* previous_cumsum,
                                  int width) {
  static const uint8_t kCumSumTranspose[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                               2, 6, 10, 14, 3, 7, 11, 15};
  static const int8_t kCumSumMaddLo[32] = {
      1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
      1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0};
  static const int8_t kCumSumMaddHi[32] = {
      1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  __m256i ymm0 = _mm256_setzero_si256();
  const __m256i ymm5 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kCumSumTranspose));
  const __m256i ymm6 = _mm256_loadu_si256((const __m256i*)kCumSumMaddLo);
  const __m256i ymm7 = _mm256_loadu_si256((const __m256i*)kCumSumMaddHi);
  const __m256i ymm8 = _mm256_set1_epi16(1);

  while (width >= 4) {
    __m256i ymm2 =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)row));
    row += 16;
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm5);
    __m256i ymm3 = _mm256_madd_epi16(_mm256_maddubs_epi16(ymm2, ymm6), ymm8);
    __m256i ymm4 = _mm256_madd_epi16(_mm256_maddubs_epi16(ymm2, ymm7), ymm8);
    __m256i sum4 = _mm256_permute2x128_si256(ymm4, ymm4, 0x11);
    ymm3 = _mm256_add_epi32(
        _mm256_add_epi32(ymm3,
                         _mm256_loadu_si256((const __m256i*)previous_cumsum)),
        ymm0);
    ymm4 = _mm256_add_epi32(
        _mm256_add_epi32(
            ymm4, _mm256_loadu_si256((const __m256i*)(previous_cumsum + 8))),
        ymm0);
    previous_cumsum += 16;
    ymm0 = _mm256_add_epi32(ymm0, sum4);
    _mm256_storeu_si256((__m256i*)cumsum, ymm3);
    _mm256_storeu_si256((__m256i*)(cumsum + 8), ymm4);
    cumsum += 16;
    width -= 4;
  }

  __m128i xmm0 = _mm256_castsi256_si128(ymm0);
  while (width > 0) {
    int32_t px;
    memcpy(&px, row, sizeof(px));
    row += 4;
    __m128i xmm2 = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(px));
    xmm0 = _mm_add_epi32(xmm0, xmm2);
    xmm2 = _mm_add_epi32(xmm0,
                         _mm_loadu_si128((const __m128i*)previous_cumsum));
    previous_cumsum += 4;
    _mm_storeu_si128((__m128i*)cumsum, xmm2);
    cumsum += 4;
    --width;
  }
  _mm256_zeroupper();
}
#endif  // HAS_COMPUTECUMULATIVESUMROW_AVX2

#ifdef HAS_CUMULATIVESUMTOAVERAGEROW_AVX2
LIBYUV_TARGET_AVX2
void CumulativeSumToAverageRow_AVX2(const int32_t* topleft,
                                    const int32_t* botleft,
                                    int width,
                                    int area,
                                    uint8_t* dst,
                                    int count) {
  __m128 xmm5 = _mm_cvtsi32_ss(_mm_setzero_ps(), area);
  __m128 xmm4 = _mm_div_ss(_mm_set_ss(1.0f), xmm5);
  xmm4 = _mm_shuffle_ps(xmm4, xmm4, 0);

  if (count >= 4) {
    __m256 ymm4 = _mm256_insertf128_ps(_mm256_castps128_ps256(xmm4), xmm4, 1);
    if (area > 1 && area <= 0x80) {
      __m128 xmm6 = _mm_cvtepi32_ps(_mm_set1_epi32(0x0000ffff));
      xmm6 = _mm_mul_ss(_mm_add_ss(xmm5, xmm6), xmm4);
      __m128i i6 = _mm_cvtps_epi32(xmm6);
      i6 = _mm_packus_epi32(i6, i6);
      __m256i ymm6 = _mm256_broadcastw_epi16(i6);
      while (count >= 4) {
        __m256i ymm0 = _mm256_sub_epi32(
            _mm256_loadu_si256((const __m256i*)topleft),
            _mm256_loadu_si256((const __m256i*)(topleft + width)));
        __m256i ymm1 = _mm256_sub_epi32(
            _mm256_loadu_si256((const __m256i*)(topleft + 8)),
            _mm256_loadu_si256((const __m256i*)(topleft + width + 8)));
        topleft += 16;
        ymm0 = _mm256_sub_epi32(ymm0,
                                _mm256_loadu_si256((const __m256i*)botleft));
        ymm1 = _mm256_sub_epi32(
            ymm1, _mm256_loadu_si256((const __m256i*)(botleft + 8)));
        ymm0 = _mm256_add_epi32(
            ymm0, _mm256_loadu_si256((const __m256i*)(botleft + width)));
        ymm1 = _mm256_add_epi32(
            ymm1, _mm256_loadu_si256((const __m256i*)(botleft + width + 8)));
        botleft += 16;
        ymm0 = _mm256_mulhi_epu16(_mm256_packus_epi32(ymm0, ymm1), ymm6);
        __m128i out = _mm_packus_epi16(_mm256_castsi256_si128(ymm0),
                                       _mm256_extracti128_si256(ymm0, 1));
        _mm_storeu_si128((__m128i*)dst, _mm_shuffle_epi32(out, 0xd8));
        dst += 16;
        count -= 4;
      }
    } else {
      while (count >= 4) {
        __m256i ymm0 = _mm256_sub_epi32(
            _mm256_loadu_si256((const __m256i*)topleft),
            _mm256_loadu_si256((const __m256i*)(topleft + width)));
        __m256i ymm1 = _mm256_sub_epi32(
            _mm256_loadu_si256((const __m256i*)(topleft + 8)),
            _mm256_loadu_si256((const __m256i*)(topleft + width + 8)));
        topleft += 16;
        ymm0 = _mm256_sub_epi32(ymm0,
                                _mm256_loadu_si256((const __m256i*)botleft));
        ymm1 = _mm256_sub_epi32(
            ymm1, _mm256_loadu_si256((const __m256i*)(botleft + 8)));
        ymm0 = _mm256_add_epi32(
            ymm0, _mm256_loadu_si256((const __m256i*)(botleft + width)));
        ymm1 = _mm256_add_epi32(
            ymm1, _mm256_loadu_si256((const __m256i*)(botleft + width + 8)));
        botleft += 16;
        ymm0 =
            _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(ymm0), ymm4));
        ymm1 =
            _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(ymm1), ymm4));
        ymm0 = _mm256_packus_epi32(ymm0, ymm1);
        __m128i out = _mm_packus_epi16(_mm256_castsi256_si128(ymm0),
                                       _mm256_extracti128_si256(ymm0, 1));
        _mm_storeu_si128((__m128i*)dst, _mm_shuffle_epi32(out, 0xd8));
        dst += 16;
        count -= 4;
      }
    }
    _mm256_zeroupper();
  }

  while (count > 0) {
    __m128i d0 = _mm_sub_epi32(
        _mm_loadu_si128((const __m128i*)topleft),
        _mm_loadu_si128((const __m128i*)(topleft + width)));
    topleft += 4;
    __m128i d1 = _mm_sub_epi32(
        _mm_loadu_si128((const __m128i*)(botleft + width)),
        _mm_loadu_si128((const __m128i*)botleft));
    botleft += 4;
    d0 = _mm_add_epi32(d0, d1);
    d0 = _mm_cvttps_epi32(_mm_mul_ps(_mm_cvtepi32_ps(d0), xmm4));
    d0 = _mm_packus_epi32(d0, d0);
    d0 = _mm_packus_epi16(d0, d0);
    int32_t out32 = _mm_cvtsi128_si32(d0);
    memcpy(dst, &out32, sizeof(out32));
    dst += 4;
    --count;
  }
}
#endif  // HAS_CUMULATIVESUMTOAVERAGEROW_AVX2

#ifdef HAS_I422TOYUY2ROW_AVX2
LIBYUV_TARGET_AVX2
void I422ToYUY2Row_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_yuy2,
                        int width) {
  do {
    __m128i u = _mm_loadu_si128((const __m128i*)src_u);
    __m128i v = _mm_loadu_si128((const __m128i*)src_v);
    __m256i uv = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(u, v)),
        _mm_unpackhi_epi8(u, v), 1);
    __m256i y = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i lo = _mm256_unpacklo_epi8(y, uv);
    __m256i hi = _mm256_unpackhi_epi8(y, uv);
    _mm256_storeu_si256((__m256i*)dst_yuy2,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_yuy2 + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_y += 32;
    src_u += 16;
    src_v += 16;
    dst_yuy2 += 64;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_I422TOYUY2ROW_AVX2

#ifdef HAS_I422TOUYVYROW_AVX2
LIBYUV_TARGET_AVX2
void I422ToUYVYRow_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_uyvy,
                        int width) {
  do {
    __m128i u = _mm_loadu_si128((const __m128i*)src_u);
    __m128i v = _mm_loadu_si128((const __m128i*)src_v);
    __m256i uv = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(u, v)),
        _mm_unpackhi_epi8(u, v), 1);
    __m256i y = _mm256_loadu_si256((const __m256i*)src_y);
    __m256i lo = _mm256_unpacklo_epi8(uv, y);
    __m256i hi = _mm256_unpackhi_epi8(uv, y);
    _mm256_storeu_si256((__m256i*)dst_uyvy,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_uyvy + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_y += 32;
    src_u += 16;
    src_v += 16;
    dst_uyvy += 64;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_I422TOUYVYROW_AVX2

#ifdef HAS_MERGEARGBROW_AVX2
LIBYUV_TARGET_AVX2
void MergeARGBRow_AVX2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       const uint8_t* src_a,
                       uint8_t* dst_argb,
                       int width) {
  do {
    __m128i r = _mm_loadu_si128((const __m128i*)src_r);
    __m128i g = _mm_loadu_si128((const __m128i*)src_g);
    __m128i b = _mm_loadu_si128((const __m128i*)src_b);
    __m128i a = _mm_loadu_si128((const __m128i*)src_a);
    __m256i bg = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(b, g)),
        _mm_unpackhi_epi8(b, g), 1);
    __m256i ra = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(r, a)),
        _mm_unpackhi_epi8(r, a), 1);
    __m256i lo = _mm256_unpacklo_epi16(bg, ra);
    __m256i hi = _mm256_unpackhi_epi16(bg, ra);
    _mm256_storeu_si256((__m256i*)dst_argb,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    src_a += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEARGBROW_AVX2

#ifdef HAS_MERGEXRGBROW_AVX2
LIBYUV_TARGET_AVX2
void MergeXRGBRow_AVX2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       uint8_t* dst_argb,
                       int width) {
  const __m128i a = _mm_set1_epi8((char)0xff);
  do {
    __m128i r = _mm_loadu_si128((const __m128i*)src_r);
    __m128i g = _mm_loadu_si128((const __m128i*)src_g);
    __m128i b = _mm_loadu_si128((const __m128i*)src_b);
    __m256i bg = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(b, g)),
        _mm_unpackhi_epi8(b, g), 1);
    __m256i ra = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(r, a)),
        _mm_unpackhi_epi8(r, a), 1);
    __m256i lo = _mm256_unpacklo_epi16(bg, ra);
    __m256i hi = _mm256_unpackhi_epi16(bg, ra);
    _mm256_storeu_si256((__m256i*)dst_argb,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEXRGBROW_AVX2

#ifdef HAS_MERGEARGB16TO8ROW_AVX2
LIBYUV_TARGET_AVX2
void MergeARGB16To8Row_AVX2(const uint16_t* src_r,
                            const uint16_t* src_g,
                            const uint16_t* src_b,
                            const uint16_t* src_a,
                            uint8_t* dst_argb,
                            int depth,
                            int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(depth - 8);
  const __m256i ymm_255 = _mm256_set1_epi16(255);
  do {
    __m256i r16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_r), shift128),
        ymm_255);
    __m256i g16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_g), shift128),
        ymm_255);
    __m256i b16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_b), shift128),
        ymm_255);
    __m256i a16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_a), shift128),
        ymm_255);
    __m256i br8 =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(b16, r16), 0xd8);
    __m256i ga8 =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(g16, a16), 0xd8);
    __m128i b = _mm256_castsi256_si128(br8);
    __m128i r = _mm256_extracti128_si256(br8, 1);
    __m128i g = _mm256_castsi256_si128(ga8);
    __m128i a = _mm256_extracti128_si256(ga8, 1);
    __m256i bg = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(b, g)),
        _mm_unpackhi_epi8(b, g), 1);
    __m256i ra = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(r, a)),
        _mm_unpackhi_epi8(r, a), 1);
    __m256i lo = _mm256_unpacklo_epi16(bg, ra);
    __m256i hi = _mm256_unpackhi_epi16(bg, ra);
    _mm256_storeu_si256((__m256i*)dst_argb,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    src_a += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEARGB16TO8ROW_AVX2

#ifdef HAS_MERGEXRGB16TO8ROW_AVX2
LIBYUV_TARGET_AVX2
void MergeXRGB16To8Row_AVX2(const uint16_t* src_r,
                            const uint16_t* src_g,
                            const uint16_t* src_b,
                            uint8_t* dst_argb,
                            int depth,
                            int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(depth - 8);
  const __m256i ymm_255 = _mm256_set1_epi16(255);
  const __m128i a = _mm_set1_epi8((char)0xff);
  do {
    __m256i r16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_r), shift128),
        ymm_255);
    __m256i g16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_g), shift128),
        ymm_255);
    __m256i b16 = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_b), shift128),
        ymm_255);
    __m256i br8 =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(b16, r16), 0xd8);
    __m256i g8 =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(g16, g16), 0xd8);
    __m128i b = _mm256_castsi256_si128(br8);
    __m128i r = _mm256_extracti128_si256(br8, 1);
    __m128i g = _mm256_castsi256_si128(g8);
    __m256i bg = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(b, g)),
        _mm_unpackhi_epi8(b, g), 1);
    __m256i ra = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(r, a)),
        _mm_unpackhi_epi8(r, a), 1);
    __m256i lo = _mm256_unpacklo_epi16(bg, ra);
    __m256i hi = _mm256_unpackhi_epi16(bg, ra);
    _mm256_storeu_si256((__m256i*)dst_argb,
                        _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_permute2x128_si256(lo, hi, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEXRGB16TO8ROW_AVX2

#ifdef HAS_MERGEAR64ROW_AVX2
LIBYUV_TARGET_AVX2
void MergeAR64Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       const uint16_t* src_a,
                       uint16_t* dst_ar64,
                       int depth,
                       int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(16 - depth);
  const uint32_t max_val = (depth >= 16) ? 0xffffu : ((1u << depth) - 1u);
  const __m256i ymm_max = _mm256_set1_epi16((short)max_val);
  do {
    __m256i r = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_r), ymm_max),
        shift128);
    __m256i g = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_g), ymm_max),
        shift128);
    __m256i b = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_b), ymm_max),
        shift128);
    __m256i a = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_a), ymm_max),
        shift128);
    __m256i bg_lo = _mm256_unpacklo_epi16(b, g);
    __m256i bg_hi = _mm256_unpackhi_epi16(b, g);
    __m256i ra_lo = _mm256_unpacklo_epi16(r, a);
    __m256i ra_hi = _mm256_unpackhi_epi16(r, a);
    __m256i p0_8 = _mm256_unpacklo_epi32(bg_lo, ra_lo);
    __m256i p2_10 = _mm256_unpackhi_epi32(bg_lo, ra_lo);
    __m256i p4_12 = _mm256_unpacklo_epi32(bg_hi, ra_hi);
    __m256i p6_14 = _mm256_unpackhi_epi32(bg_hi, ra_hi);
    _mm256_storeu_si256((__m256i*)dst_ar64,
                        _mm256_permute2x128_si256(p0_8, p2_10, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 16),
                        _mm256_permute2x128_si256(p4_12, p6_14, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 32),
                        _mm256_permute2x128_si256(p0_8, p2_10, 0x31));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 48),
                        _mm256_permute2x128_si256(p4_12, p6_14, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    src_a += 16;
    dst_ar64 += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEAR64ROW_AVX2

#ifdef HAS_MERGEXR64ROW_AVX2
LIBYUV_TARGET_AVX2
void MergeXR64Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       uint16_t* dst_ar64,
                       int depth,
                       int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(16 - depth);
  const uint32_t max_val = (depth >= 16) ? 0xffffu : ((1u << depth) - 1u);
  const __m256i ymm_max = _mm256_set1_epi16((short)max_val);
  const __m256i a = _mm256_set1_epi16((short)0xffff);
  do {
    __m256i r = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_r), ymm_max),
        shift128);
    __m256i g = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_g), ymm_max),
        shift128);
    __m256i b = _mm256_sll_epi16(
        _mm256_min_epu16(_mm256_loadu_si256((const __m256i*)src_b), ymm_max),
        shift128);
    __m256i bg_lo = _mm256_unpacklo_epi16(b, g);
    __m256i bg_hi = _mm256_unpackhi_epi16(b, g);
    __m256i ra_lo = _mm256_unpacklo_epi16(r, a);
    __m256i ra_hi = _mm256_unpackhi_epi16(r, a);
    __m256i p0_8 = _mm256_unpacklo_epi32(bg_lo, ra_lo);
    __m256i p2_10 = _mm256_unpackhi_epi32(bg_lo, ra_lo);
    __m256i p4_12 = _mm256_unpacklo_epi32(bg_hi, ra_hi);
    __m256i p6_14 = _mm256_unpackhi_epi32(bg_hi, ra_hi);
    _mm256_storeu_si256((__m256i*)dst_ar64,
                        _mm256_permute2x128_si256(p0_8, p2_10, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 16),
                        _mm256_permute2x128_si256(p4_12, p6_14, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 32),
                        _mm256_permute2x128_si256(p0_8, p2_10, 0x31));
    _mm256_storeu_si256((__m256i*)(dst_ar64 + 48),
                        _mm256_permute2x128_si256(p4_12, p6_14, 0x31));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    dst_ar64 += 64;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_MERGEXR64ROW_AVX2

#ifdef HAS_NV21TOYUV24ROW_AVX2
LIBYUV_TARGET_AVX2
void NV21ToYUV24Row_AVX2(const uint8_t* src_y,
                         const uint8_t* src_vu,
                         uint8_t* dst_yuv24,
                         int width) {
  static const uint8_t kYUV24Shuffle[3][16] = {
      {8, 9, 0, 8, 9, 1, 10, 11, 2, 10, 11, 3, 12, 13, 4, 12},
      {9, 1, 10, 11, 2, 10, 11, 3, 12, 13, 4, 12, 13, 5, 14, 15},
      {2, 10, 11, 3, 12, 13, 4, 12, 13, 5, 14, 15, 6, 14, 15, 7}};
  const __m256i ymm4 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kYUV24Shuffle[0]));
  const __m256i ymm5 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kYUV24Shuffle[1]));
  const __m256i ymm6 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kYUV24Shuffle[2]));

  do {
    __m256 ymm2 = _mm256_castsi256_ps(_mm256_loadu_si256((const __m256i*)src_y));
    __m256 ymm3 =
        _mm256_castsi256_ps(_mm256_loadu_si256((const __m256i*)src_vu));
    __m256i c0 = _mm256_shuffle_epi8(
        _mm256_castps_si256(_mm256_shuffle_ps(ymm2, ymm3, 0x44)), ymm4);
    __m256i c1 = _mm256_shuffle_epi8(
        _mm256_castps_si256(_mm256_shuffle_ps(ymm2, ymm3, 0x99)), ymm5);
    __m256i c2 = _mm256_shuffle_epi8(
        _mm256_castps_si256(_mm256_shuffle_ps(ymm2, ymm3, 0xee)), ymm6);
    _mm256_storeu_si256((__m256i*)dst_yuv24,
                        _mm256_permute2x128_si256(c0, c1, 0x20));
    _mm256_storeu_si256((__m256i*)(dst_yuv24 + 32),
                        _mm256_permute2x128_si256(c2, c0, 0x30));
    _mm256_storeu_si256((__m256i*)(dst_yuv24 + 64),
                        _mm256_permute2x128_si256(c1, c2, 0x31));
    src_y += 32;
    src_vu += 32;
    dst_yuv24 += 96;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_NV21TOYUV24ROW_AVX2

#ifdef HAS_NV21TOYUV24ROW_AVX512VBMI
LIBYUV_TARGET_AVX512VBMI
void NV21ToYUV24Row_AVX512VBMI(const uint8_t* src_y,
                               const uint8_t* src_vu,
                               uint8_t* dst_yuv24,
                               int width) {
  static const uint8_t kNV21ToYUV24Perm_AVX512VBMI[3][64] = {
      {64, 65, 0,  64, 65, 1,  66, 67, 2,  66, 67, 3,  68, 69, 4,  68,
       69, 5,  70, 71, 6,  70, 71, 7,  72, 73, 8,  72, 73, 9,  74, 75,
       10, 74, 75, 11, 76, 77, 12, 76, 77, 13, 78, 79, 14, 78, 79, 15,
       80, 81, 16, 80, 81, 17, 82, 83, 18, 82, 83, 19, 84, 85, 20, 84},
      {85, 21,  86,  87, 22,  86,  87,  23,  88,  89,  24,  88,  89,
       25, 90,  91,  26, 90,  91,  27,  92,  93,  28,  92,  93,  29,
       94, 95,  30,  94, 95,  31,  96,  97,  32,  96,  97,  33,  98,
       99, 34,  98,  99, 35,  100, 101, 36,  100, 101, 37,  102, 103,
       38, 102, 103, 39, 104, 105, 40,  104, 105, 41,  106, 107},
      {42,  106, 107, 43,  108, 109, 44,  108, 109, 45,  110, 111, 46,
       110, 111, 47,  112, 113, 48,  112, 113, 49,  114, 115, 50,  114,
       115, 51,  116, 117, 52,  116, 117, 53,  118, 119, 54,  118, 119,
       55,  120, 121, 56,  120, 121, 57,  122, 123, 58,  122, 123, 59,
       124, 125, 60,  124, 125, 61,  126, 127, 62,  126, 127, 63}};
  __m512i zmm_p0 =
      _mm512_loadu_si512((const __m512i*)kNV21ToYUV24Perm_AVX512VBMI[0]);
  __m512i zmm_p1 =
      _mm512_loadu_si512((const __m512i*)kNV21ToYUV24Perm_AVX512VBMI[1]);
  __m512i zmm_p2 =
      _mm512_loadu_si512((const __m512i*)kNV21ToYUV24Perm_AVX512VBMI[2]);
  LIBYUV_HIDE_CONSTANT(zmm_p0);
  LIBYUV_HIDE_CONSTANT(zmm_p1);
  LIBYUV_HIDE_CONSTANT(zmm_p2);

  while (width >= 64) {
    __m512i zy = _mm512_loadu_si512((const __m512i*)src_y);
    __m512i zvu = _mm512_loadu_si512((const __m512i*)src_vu);
    __m512i dst0 = _mm512_permutex2var_epi8(zy, zmm_p0, zvu);
    __m512i dst1 = _mm512_permutex2var_epi8(zy, zmm_p1, zvu);
    __m512i dst2 = _mm512_permutex2var_epi8(zy, zmm_p2, zvu);
    _mm512_storeu_si512((__m512i*)dst_yuv24, dst0);
    _mm512_storeu_si512((__m512i*)(dst_yuv24 + 64), dst1);
    _mm512_storeu_si512((__m512i*)(dst_yuv24 + 128), dst2);
    src_y += 64;
    src_vu += 64;
    dst_yuv24 += 192;
    width -= 64;
  }
  if (width > 0) {
    __mmask64 ky = (__mmask64)((1ULL << width) - 1ULL);
    int vu_bytes = (width + 1) & ~1;
    __mmask64 kvu = (vu_bytes >= 64) ? ~(__mmask64)0
                                     : (__mmask64)((1ULL << vu_bytes) - 1ULL);
    __m512i zy = _mm512_maskz_loadu_epi8(ky, src_y);
    __m512i zvu = _mm512_maskz_loadu_epi8(kvu, src_vu);
    __m512i dst0 = _mm512_permutex2var_epi8(zy, zmm_p0, zvu);
    __m512i dst1 = _mm512_permutex2var_epi8(zy, zmm_p1, zvu);
    __m512i dst2 = _mm512_permutex2var_epi8(zy, zmm_p2, zvu);
    int nbytes = width * 3;
    __mmask64 k0 =
        (nbytes >= 64) ? ~(__mmask64)0 : (__mmask64)((1ULL << nbytes) - 1ULL);
    __mmask64 k1 = (nbytes <= 64) ? 0
                   : (nbytes >= 128)
                       ? ~(__mmask64)0
                       : (__mmask64)((1ULL << (nbytes - 64)) - 1ULL);
    __mmask64 k2 =
        (nbytes <= 128) ? 0 : (__mmask64)((1ULL << (nbytes - 128)) - 1ULL);
    _mm512_mask_storeu_epi8(dst_yuv24, k0, dst0);
    if (k1) {
      _mm512_mask_storeu_epi8(dst_yuv24 + 64, k1, dst1);
    }
    if (k2) {
      _mm512_mask_storeu_epi8(dst_yuv24 + 128, k2, dst2);
    }
  }
  _mm256_zeroupper();
}
#endif  // HAS_NV21TOYUV24ROW_AVX512VBMI

#ifdef HAS_SPLITARGBROW_AVX2
LIBYUV_TARGET_AVX2
void SplitARGBRow_AVX2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       uint8_t* dst_a,
                       int width) {
  static const uint8_t kShufSplitARGB[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                             2, 6, 10, 14, 3, 7, 11, 15};
  const __m256i shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufSplitARGB));
  do {
    __m256i t0 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)src_argb), shuf);
    __m256i t1 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_argb + 32)), shuf);
    __m256i p0 = _mm256_permute2x128_si256(t0, t1, 0x20);
    __m256i p1 = _mm256_permute2x128_si256(t0, t1, 0x31);
    __m256i bg = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(p0, p1), 0xd8);
    __m256i ra = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(p0, p1), 0xd8);
    _mm_storeu_si128((__m128i*)dst_b, _mm256_castsi256_si128(bg));
    _mm_storeu_si128((__m128i*)dst_g, _mm256_extracti128_si256(bg, 1));
    _mm_storeu_si128((__m128i*)dst_r, _mm256_castsi256_si128(ra));
    _mm_storeu_si128((__m128i*)dst_a, _mm256_extracti128_si256(ra, 1));
    src_argb += 64;
    dst_r += 16;
    dst_g += 16;
    dst_b += 16;
    dst_a += 16;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_SPLITARGBROW_AVX2

#ifdef HAS_SPLITXRGBROW_AVX2
LIBYUV_TARGET_AVX2
void SplitXRGBRow_AVX2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       int width) {
  static const uint8_t kShufSplitARGB[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                             2, 6, 10, 14, 3, 7, 11, 15};
  const __m256i shuf = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kShufSplitARGB));
  do {
    __m256i t0 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)src_argb), shuf);
    __m256i t1 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_argb + 32)), shuf);
    __m256i p0 = _mm256_permute2x128_si256(t0, t1, 0x20);
    __m256i p1 = _mm256_permute2x128_si256(t0, t1, 0x31);
    __m256i bg = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(p0, p1), 0xd8);
    __m256i ra = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(p0, p1), 0xd8);
    _mm_storeu_si128((__m128i*)dst_b, _mm256_castsi256_si128(bg));
    _mm_storeu_si128((__m128i*)dst_g, _mm256_extracti128_si256(bg, 1));
    _mm_storeu_si128((__m128i*)dst_r, _mm256_castsi256_si128(ra));
    src_argb += 64;
    dst_r += 16;
    dst_g += 16;
    dst_b += 16;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_SPLITXRGBROW_AVX2

#ifdef HAS_SPLITRGBROW_AVX2
LIBYUV_TARGET_AVX2
void SplitRGBRow_AVX2(const uint8_t* src_rgb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      int width) {
  static const uint8_t kSplitRGBShuffle[5][16] = {
      {0u, 3u, 6u, 9u, 12u, 15u, 2u, 5u, 8u, 11u, 14u, 1u, 4u, 7u, 10u, 13u},
      {1u, 4u, 7u, 10u, 13u, 0u, 3u, 6u, 9u, 12u, 15u, 2u, 5u, 8u, 11u, 14u},
      {2u, 5u, 8u, 11u, 14u, 1u, 4u, 7u, 10u, 13u, 0u, 3u, 6u, 9u, 12u, 15u},
      {0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u,
       0u},
      {0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u,
       0u}};
  const __m256i s0 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kSplitRGBShuffle[0]));
  const __m256i s1 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kSplitRGBShuffle[1]));
  const __m256i s2 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kSplitRGBShuffle[2]));
  const __m256i m0 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kSplitRGBShuffle[3]));
  const __m256i m1 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)kSplitRGBShuffle[4]));

  do {
    __m256i ymm4 = _mm256_loadu_si256((const __m256i*)src_rgb);
    __m256i ymm5 = _mm256_loadu_si256((const __m256i*)(src_rgb + 32));
    __m256i ymm6 = _mm256_loadu_si256((const __m256i*)(src_rgb + 64));
    src_rgb += 96;
    __m256i ymm1 = _mm256_blend_epi32(ymm4, ymm5, 0xf0);
    __m256i ymm2 = _mm256_permute2x128_si256(ymm4, ymm6, 0x21);
    __m256i ymm3 = _mm256_blend_epi32(ymm5, ymm6, 0xf0);
    ymm4 = _mm256_blendv_epi8(ymm1, ymm3, m0);
    ymm5 = _mm256_blendv_epi8(ymm2, ymm1, m0);
    ymm6 = _mm256_blendv_epi8(ymm3, ymm2, m0);
    ymm1 = _mm256_shuffle_epi8(_mm256_blendv_epi8(ymm4, ymm5, m1), s0);
    ymm2 = _mm256_shuffle_epi8(_mm256_blendv_epi8(ymm5, ymm6, m1), s1);
    ymm3 = _mm256_shuffle_epi8(_mm256_blendv_epi8(ymm6, ymm4, m1), s2);
    _mm256_storeu_si256((__m256i*)dst_r, ymm1);
    _mm256_storeu_si256((__m256i*)dst_g, ymm2);
    _mm256_storeu_si256((__m256i*)dst_b, ymm3);
    dst_r += 32;
    dst_g += 32;
    dst_b += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_SPLITRGBROW_AVX2

#ifdef HAS_SPLITUVROW_16_AVX2
LIBYUV_TARGET_AVX2
void SplitUVRow_16_AVX2(const uint16_t* src_uv,
                        uint16_t* dst_u,
                        uint16_t* dst_v,
                        int depth,
                        int width) {
  const __m128i shift128 = _mm_cvtsi32_si128(16 - depth);
  const __m256i mask16 = _mm256_set1_epi32(0x0000ffff);
  do {
    __m256i uv0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i uv1 = _mm256_loadu_si256((const __m256i*)(src_uv + 16));
    __m256i u = _mm256_permute4x64_epi64(
        _mm256_packus_epi32(_mm256_and_si256(uv0, mask16),
                            _mm256_and_si256(uv1, mask16)),
        0xd8);
    __m256i v = _mm256_permute4x64_epi64(
        _mm256_packus_epi32(_mm256_srli_epi32(uv0, 16),
                            _mm256_srli_epi32(uv1, 16)),
        0xd8);
    _mm256_storeu_si256((__m256i*)dst_u, _mm256_srl_epi16(u, shift128));
    _mm256_storeu_si256((__m256i*)dst_v, _mm256_srl_epi16(v, shift128));
    src_uv += 32;
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_SPLITUVROW_16_AVX2

#ifdef HAS_YUY2TOYROW_AVX2
LIBYUV_TARGET_AVX2
void YUY2ToYRow_AVX2(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i s0 =
        _mm256_and_si256(_mm256_loadu_si256((const __m256i*)src_yuy2),
                         mask_00ff);
    __m256i s1 =
        _mm256_and_si256(_mm256_loadu_si256((const __m256i*)(src_yuy2 + 32)),
                         mask_00ff);
    _mm256_storeu_si256(
        (__m256i*)dst_y,
        _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8));
    src_yuy2 += 64;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOYROW_AVX2

#ifdef HAS_UYVYTOYROW_AVX2
LIBYUV_TARGET_AVX2
void UYVYToYRow_AVX2(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  do {
    __m256i s0 =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)src_uyvy), 8);
    __m256i s1 =
        _mm256_srli_epi16(_mm256_loadu_si256((const __m256i*)(src_uyvy + 32)),
                          8);
    _mm256_storeu_si256(
        (__m256i*)dst_y,
        _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8));
    src_uyvy += 64;
    dst_y += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_UYVYTOYROW_AVX2

#ifdef HAS_YUY2TOUVROW_AVX2
LIBYUV_TARGET_AVX2
void YUY2ToUVRow_AVX2(const uint8_t* src_yuy2,
                      int stride_yuy2,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i y0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_yuy2),
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + stride_yuy2)));
    __m256i y1 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + 32)),
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + stride_yuy2 + 32)));
    __m256i uv = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_srli_epi16(y0, 8), _mm256_srli_epi16(y1, 8)),
        0xd8);
    __m256i u = _mm256_and_si256(uv, mask_00ff);
    __m256i v = _mm256_srli_epi16(uv, 8);
    u = _mm256_permute4x64_epi64(_mm256_packus_epi16(u, u), 0xd8);
    v = _mm256_permute4x64_epi64(_mm256_packus_epi16(v, v), 0xd8);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(u));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_castsi256_si128(v));
    src_yuy2 += 64;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOUVROW_AVX2

#ifdef HAS_YUY2TOUV422ROW_AVX2
LIBYUV_TARGET_AVX2
void YUY2ToUV422Row_AVX2(const uint8_t* src_yuy2,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i y0 = _mm256_loadu_si256((const __m256i*)src_yuy2);
    __m256i y1 = _mm256_loadu_si256((const __m256i*)(src_yuy2 + 32));
    __m256i uv = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_srli_epi16(y0, 8), _mm256_srli_epi16(y1, 8)),
        0xd8);
    __m256i u = _mm256_and_si256(uv, mask_00ff);
    __m256i v = _mm256_srli_epi16(uv, 8);
    u = _mm256_permute4x64_epi64(_mm256_packus_epi16(u, u), 0xd8);
    v = _mm256_permute4x64_epi64(_mm256_packus_epi16(v, v), 0xd8);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(u));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_castsi256_si128(v));
    src_yuy2 += 64;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TOUV422ROW_AVX2

#ifdef HAS_YUY2TONVUVROW_AVX2
LIBYUV_TARGET_AVX2
void YUY2ToNVUVRow_AVX2(const uint8_t* src_yuy2,
                        int stride_yuy2,
                        uint8_t* dst_uv,
                        int width) {
  do {
    __m256i y0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_yuy2),
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + stride_yuy2)));
    __m256i y1 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + 32)),
        _mm256_loadu_si256((const __m256i*)(src_yuy2 + stride_yuy2 + 32)));
    __m256i uv = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_srli_epi16(y0, 8), _mm256_srli_epi16(y1, 8)),
        0xd8);
    _mm256_storeu_si256((__m256i*)dst_uv, uv);
    src_yuy2 += 64;
    dst_uv += 32;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // HAS_YUY2TONVUVROW_AVX2

#if defined(HAS_UYVYTOUVROW_AVX2) || defined(HAS_UYVYTOYROW_AVX2)
LIBYUV_TARGET_AVX2
void UYVYToUVRow_AVX2(const uint8_t* src_uyvy,
                      int stride_uyvy,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i y0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_uyvy),
        _mm256_loadu_si256((const __m256i*)(src_uyvy + stride_uyvy)));
    __m256i y1 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)(src_uyvy + 32)),
        _mm256_loadu_si256((const __m256i*)(src_uyvy + stride_uyvy + 32)));
    __m256i uv = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_and_si256(y0, mask_00ff),
                            _mm256_and_si256(y1, mask_00ff)),
        0xd8);
    __m256i u = _mm256_and_si256(uv, mask_00ff);
    __m256i v = _mm256_srli_epi16(uv, 8);
    u = _mm256_permute4x64_epi64(_mm256_packus_epi16(u, u), 0xd8);
    v = _mm256_permute4x64_epi64(_mm256_packus_epi16(v, v), 0xd8);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(u));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_castsi256_si128(v));
    src_uyvy += 64;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // defined(HAS_UYVYTOUVROW_AVX2) || defined(HAS_UYVYTOYROW_AVX2)

#if defined(HAS_UYVYTOUV422ROW_AVX2) || defined(HAS_UYVYTOYROW_AVX2)
LIBYUV_TARGET_AVX2
void UYVYToUV422Row_AVX2(const uint8_t* src_uyvy,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  do {
    __m256i y0 = _mm256_loadu_si256((const __m256i*)src_uyvy);
    __m256i y1 = _mm256_loadu_si256((const __m256i*)(src_uyvy + 32));
    __m256i uv = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(_mm256_and_si256(y0, mask_00ff),
                            _mm256_and_si256(y1, mask_00ff)),
        0xd8);
    __m256i u = _mm256_and_si256(uv, mask_00ff);
    __m256i v = _mm256_srli_epi16(uv, 8);
    u = _mm256_permute4x64_epi64(_mm256_packus_epi16(u, u), 0xd8);
    v = _mm256_permute4x64_epi64(_mm256_packus_epi16(v, v), 0xd8);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(u));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_castsi256_si128(v));
    src_uyvy += 64;
    dst_u += 16;
    dst_v += 16;
    width -= 32;
  } while (width > 0);
  _mm256_zeroupper();
}
#endif  // defined(HAS_UYVYTOUV422ROW_AVX2) || defined(HAS_UYVYTOYROW_AVX2)

#ifdef HAS_I444TOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void I444ToARGBRow_AVX2(const uint8_t* y_buf,
                        const uint8_t* u_buf,
                        const uint8_t* v_buf,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  const __m256i ymm_kUVToB =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToB);
  const __m256i ymm_kUVToG =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToG);
  const __m256i ymm_kUVToR =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kUVToR);
  const __m256i ymm_kYToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYToRgb);
  const __m256i ymm_kYBiasToRgb =
      _mm256_loadu_si256((const __m256i*)yuvconstants->kYBiasToRgb);
  const __m256i ymm_128 = _mm256_set1_epi8((char)0x80);
  const __m256i ymm_alpha = _mm256_set1_epi8((char)0xff);

  while (width >= 16) {
    __m128i xmm_u = _mm_loadu_si128((const __m128i*)u_buf);
    __m128i xmm_v = _mm_loadu_si128((const __m128i*)v_buf);
    __m128i xmm_y = _mm_loadu_si128((const __m128i*)y_buf);
    u_buf += 16;
    v_buf += 16;
    y_buf += 16;

    __m256i ymm3 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(xmm_u, xmm_v)),
        _mm_unpackhi_epi8(xmm_u, xmm_v), 1);
    __m256i ymm4 = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_unpacklo_epi8(xmm_y, xmm_y)),
        _mm_unpackhi_epi8(xmm_y, xmm_y), 1);

    ymm3 = _mm256_sub_epi8(ymm3, ymm_128);
    ymm4 = _mm256_mulhi_epu16(ymm4, ymm_kYToRgb);

    __m256i ymm0 = _mm256_maddubs_epi16(ymm_kUVToB, ymm3);
    __m256i ymm1 = _mm256_maddubs_epi16(ymm_kUVToG, ymm3);
    __m256i ymm2 = _mm256_maddubs_epi16(ymm_kUVToR, ymm3);

    ymm4 = _mm256_add_epi16(ymm4, ymm_kYBiasToRgb);

    ymm0 = _mm256_srai_epi16(_mm256_adds_epi16(ymm0, ymm4), 6);
    ymm1 = _mm256_srai_epi16(_mm256_subs_epi16(ymm4, ymm1), 6);
    ymm2 = _mm256_srai_epi16(_mm256_adds_epi16(ymm2, ymm4), 6);

    ymm0 = _mm256_packus_epi16(ymm0, ymm0);
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);

    ymm0 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi8(ymm0, ymm1), 0xd8);
    ymm2 =
        _mm256_permute4x64_epi64(_mm256_unpacklo_epi8(ymm2, ymm_alpha), 0xd8);

    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_unpacklo_epi16(ymm0, ymm2));
    _mm256_storeu_si256((__m256i*)(dst_argb + 32),
                        _mm256_unpackhi_epi16(ymm0, ymm2));

    dst_argb += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_I444TOARGBROW_AVX2

#ifdef HAS_MERGEXR30ROW_AVX2
LIBYUV_TARGET_AVX2
void MergeXR30Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       uint8_t* dst_ar30,
                       int depth,
                       int width) {
  int shift = depth - 10;
  __m128i v_shift = _mm_cvtsi32_si128(shift);
  __m256i v_alpha = _mm256_set1_epi16(0x0030);
  __m256i v_max = _mm256_set1_epi16(1023);
  while (width > 0) {
    __m256i r = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_r), v_shift),
        v_max);
    __m256i g = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_g), v_shift),
        v_max);
    __m256i b = _mm256_min_epu16(
        _mm256_srl_epi16(_mm256_loadu_si256((const __m256i*)src_b), v_shift),
        v_max);
    r = _mm256_slli_epi16(_mm256_permute4x64_epi64(r, 0xd8), 4);
    g = _mm256_permute4x64_epi64(g, 0xd8);
    b = _mm256_permute4x64_epi64(b, 0xd8);
    __m256i rb_lo = _mm256_unpacklo_epi16(b, r);
    __m256i rb_hi = _mm256_unpackhi_epi16(b, r);
    __m256i ag_lo = _mm256_slli_epi32(_mm256_unpacklo_epi16(g, v_alpha), 10);
    __m256i ag_hi = _mm256_slli_epi32(_mm256_unpackhi_epi16(g, v_alpha), 10);
    _mm256_storeu_si256((__m256i*)dst_ar30, _mm256_or_si256(rb_lo, ag_lo));
    _mm256_storeu_si256((__m256i*)(dst_ar30 + 32),
                        _mm256_or_si256(rb_hi, ag_hi));
    src_r += 16;
    src_g += 16;
    src_b += 16;
    dst_ar30 += 64;
    width -= 16;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEXR30ROW_AVX2

#ifdef HAS_MERGEUVROW_16_AVX2
LIBYUV_TARGET_AVX2
void MergeUVRow_16_AVX2(const uint16_t* src_u,
                        const uint16_t* src_v,
                        uint16_t* dst_uv,
                        int depth,
                        int width) {
  const __m128i sh_u = _mm_cvtsi32_si128(16 - depth);
  const __m128i sh_v = _mm_cvtsi32_si128(32 - depth);
  while (width > 0) {
    __m256i u32 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_u));
    __m256i v32 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_v));
    src_u += 8;
    src_v += 8;
    u32 = _mm256_sll_epi16(u32, sh_u);
    v32 = _mm256_sll_epi32(v32, sh_v);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm256_or_si256(u32, v32));
    dst_uv += 16;
    width -= 8;
  }
  _mm256_zeroupper();
}
#endif  // HAS_MERGEUVROW_16_AVX2

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) ||
        // defined(__i386__) || defined(_M_X64) || defined(_M_IX86)) &&
        // ((defined(_MSC_VER) && !defined(__clang__)) ||
        // defined(LIBYUV_ENABLE_ROWWIN))
