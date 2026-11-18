/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/compare.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "libyuv/compare_row.h"
#include "libyuv/cpu_id.h"
#include "libyuv/row.h"
#include "libyuv/video_common.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// hash seed of 5381 recommended.
LIBYUV_API
uint32_t HashDjb2(const uint8_t* src, uint64_t count, uint32_t seed) {
  const int kBlockSize = 1 << 15;  // 32768;
  int simd_size = 16;  // SIMD for multiple of 16, and C for remainder.
  int remainder;
  uint32_t (*HashDjb2_SSE)(const uint8_t* src, int count, uint32_t seed) =
      HashDjb2_C;
#if defined(HAS_HASHDJB2_SSE41)
  if (TestCpuFlag(kCpuHasSSE41)) {
    HashDjb2_SSE = HashDjb2_SSE41;
  }
#endif
#if defined(HAS_HASHDJB2_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    HashDjb2_SSE = HashDjb2_AVX2;
    simd_size = 32;
  }
#endif
#if defined(HAS_HASHDJB2_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    HashDjb2_SSE = HashDjb2_NEON;
  }
#endif
#if defined(HAS_HASHDJB2_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    HashDjb2_SSE = HashDjb2_AVX512BW;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_HASHDJB2_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    HashDjb2_SSE = HashDjb2_RVV;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_HASHDJB2_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    HashDjb2_SSE = HashDjb2_LSX;
  }
#endif
#if defined(HAS_HASHDJB2_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    HashDjb2_SSE = HashDjb2_LASX;
  }
#endif
#if defined(HAS_HASHDJB2_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    HashDjb2_SSE = HashDjb2_WASMSIMD;
  }
#endif
#if defined(HAS_HASHDJB2_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    HashDjb2_SSE = HashDjb2_HVX;
    simd_size = 1;  // Any count.
  }
#endif

  while (count >= (uint64_t)kBlockSize) {
    seed = HashDjb2_SSE(src, kBlockSize, seed);
    src += kBlockSize;
    count -= kBlockSize;
  }
  remainder = (int)count & ~(simd_size - 1);
  if (remainder) {
    seed = HashDjb2_SSE(src, remainder, seed);
    src += remainder;
    count -= remainder;
  }
  remainder = (int)count & (simd_size - 1);
  if (remainder) {
    if (HashDjb2_SSE == HashDjb2_C) {
      seed = HashDjb2_C(src, remainder, seed);
    } else {
      SIMD_ALIGNED(uint8_t buf[32]);
      uint32_t pow33 = 1;
      int i;
      memset(buf, 0, sizeof(buf));
      for (i = 0; i < remainder; ++i) {
        pow33 *= 33;
      }
      memcpy(buf + simd_size - remainder, src, remainder);
      seed = seed * pow33 + HashDjb2_SSE(buf, simd_size, 0);
    }
  }
  return seed;
}

// Scan an opaque argb image and return fourcc based on alpha offset.
// Returns FOURCC_ARGB, FOURCC_BGRA, or 0 if unknown.
LIBYUV_API
uint32_t ARGBDetect(const uint8_t* argb,
                    int stride_argb,
                    int width,
                    int height) {
  uint32_t (*ARGBDetectRow)(const uint8_t* argb, int width) = ARGBDetectRow_C;
  uint32_t fourcc = 0;
  int h;

  if (!argb || width <= 0 || height <= 0) {
    return fourcc;
  }
  // Coalesce rows.
  if (stride_argb == width * 4 && (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    stride_argb = 0;
  }
#if defined(HAS_ARGBDETECTROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBDetectRow = ARGBDetectRow_AVX2;
  }
#endif
#if defined(HAS_ARGBDETECTROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBDetectRow = ARGBDetectRow_NEON;
  }
#endif
#if defined(HAS_ARGBDETECTROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    ARGBDetectRow = ARGBDetectRow_RVV;
  }
#endif
#if defined(HAS_ARGBDETECTROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    ARGBDetectRow = ARGBDetectRow_LSX;
  }
#endif
#if defined(HAS_ARGBDETECTROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBDetectRow = ARGBDetectRow_LASX;
  }
#endif
#if defined(HAS_ARGBDETECTROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    ARGBDetectRow = ARGBDetectRow_WASMSIMD;
  }
#endif
#if defined(HAS_ARGBDETECTROW_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    ARGBDetectRow = ARGBDetectRow_HVX;
  }
#endif
  for (h = 0; h < height && fourcc == 0; ++h) {
    fourcc = ARGBDetectRow(argb, width);
    argb += stride_argb;
  }
  return fourcc;
}

// NEON version accumulates in 16 bit shorts which overflow at 65536 bytes.
// So actual maximum is 1 less loop, which is 64436 - 32 bytes.

LIBYUV_API
uint64_t ComputeHammingDistance(const uint8_t* src_a,
                                const uint8_t* src_b,
                                int count) {
  const int kBlockSize = 1 << 15;  // 32768;
  int simd_size = 64;  // SIMD for multiple of 64, and C for remainder.
  int remainder;
  uint64_t diff = 0;
  int i;
  uint32_t (*HammingDistance)(const uint8_t* src_a, const uint8_t* src_b,
                              int count) = HammingDistance_C;
#if defined(HAS_HAMMINGDISTANCE_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    HammingDistance = HammingDistance_NEON;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    HammingDistance = HammingDistance_NEON_DotProd;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    HammingDistance = HammingDistance_SSSE3;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_SSE42)
  if (TestCpuFlag(kCpuHasSSE42)) {
    HammingDistance = HammingDistance_SSE42;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    HammingDistance = HammingDistance_AVX2;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    HammingDistance = HammingDistance_AVX512BW;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_AVX512VNNI)
  if (TestCpuFlag(kCpuHasAVX512VNNI)) {
    HammingDistance = HammingDistance_AVX512VNNI;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    HammingDistance = HammingDistance_RVV;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    HammingDistance = HammingDistance_LSX;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    HammingDistance = HammingDistance_LASX;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    HammingDistance = HammingDistance_WASMSIMD;
  }
#endif
#if defined(HAS_HAMMINGDISTANCE_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    HammingDistance = HammingDistance_HVX;
    simd_size = 1;  // Any count.
  }
#endif
  remainder = count & (kBlockSize - 1) & ~(simd_size - 1);

#ifdef _OPENMP
#pragma omp parallel for reduction(+ : diff)
#endif
  for (i = 0; i < (count - (kBlockSize - 1)); i += kBlockSize) {
    diff += HammingDistance(src_a + i, src_b + i, kBlockSize);
  }
  src_a += count & ~(kBlockSize - 1);
  src_b += count & ~(kBlockSize - 1);
  if (remainder) {
    diff += HammingDistance(src_a, src_b, remainder);
    src_a += remainder;
    src_b += remainder;
  }
  remainder = count & (simd_size - 1);
  if (remainder) {
    if (HammingDistance == HammingDistance_C) {
      diff += HammingDistance_C(src_a, src_b, remainder);
    } else {
      SIMD_ALIGNED(uint8_t buf_a[64]);
      SIMD_ALIGNED(uint8_t buf_b[64]);
      memset(buf_a, 0, sizeof(buf_a));
      memset(buf_b, 0, sizeof(buf_b));
      memcpy(buf_a, src_a, remainder);
      memcpy(buf_b, src_b, remainder);
      diff += HammingDistance(buf_a, buf_b, simd_size);
    }
  }
  return diff;
}

// TODO(fbarchard): Refactor into row function.
LIBYUV_API
uint64_t ComputeSumSquareError(const uint8_t* src_a,
                               const uint8_t* src_b,
                               int count) {
  // SumSquareError returns values 0 to 65535 for each squared difference.
  // Up to 65536 of those can be summed and remain within a uint32_t.
  // After each block of 65536 pixels, accumulate into a uint64_t.
  const int kBlockSize = 65536;
  int simd_size = 32;  // SIMD for multiple of 32, and C for remainder.
  int remainder;
  uint64_t sse = 0;
  int i;
  uint32_t (*SumSquareError)(const uint8_t* src_a, const uint8_t* src_b,
                             int count) = SumSquareError_C;
#if defined(HAS_SUMSQUAREERROR_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SumSquareError = SumSquareError_NEON;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    SumSquareError = SumSquareError_NEON_DotProd;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    // Note only used for multiples of 16 so count is not checked.
    SumSquareError = SumSquareError_SSE2;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    // Note only used for multiples of 32 so count is not checked.
    SumSquareError = SumSquareError_AVX2;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    SumSquareError = SumSquareError_AVX512BW;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_SUMSQUAREERROR_AVX512VNNI)
  if (TestCpuFlag(kCpuHasAVX512VNNI)) {
    SumSquareError = SumSquareError_AVX512VNNI;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_SUMSQUAREERROR_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    SumSquareError = SumSquareError_RVV;
    simd_size = 1;  // Any count.
  }
#endif
#if defined(HAS_SUMSQUAREERROR_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    SumSquareError = SumSquareError_LSX;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    SumSquareError = SumSquareError_LASX;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    SumSquareError = SumSquareError_WASMSIMD;
  }
#endif
#if defined(HAS_SUMSQUAREERROR_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    SumSquareError = SumSquareError_HVX;
    simd_size = 1;  // Any count.
  }
#endif
  remainder = count & (kBlockSize - 1) & ~(simd_size - 1);
#ifdef _OPENMP
#pragma omp parallel for reduction(+ : sse)
#endif
  for (i = 0; i < (count - (kBlockSize - 1)); i += kBlockSize) {
    sse += SumSquareError(src_a + i, src_b + i, kBlockSize);
  }
  src_a += count & ~(kBlockSize - 1);
  src_b += count & ~(kBlockSize - 1);
  if (remainder) {
    sse += SumSquareError(src_a, src_b, remainder);
    src_a += remainder;
    src_b += remainder;
  }
  remainder = count & (simd_size - 1);
  if (remainder) {
    if (SumSquareError == SumSquareError_C) {
      sse += SumSquareError_C(src_a, src_b, remainder);
    } else {
      SIMD_ALIGNED(uint8_t buf_a[32]);
      SIMD_ALIGNED(uint8_t buf_b[32]);
      memset(buf_a, 0, sizeof(buf_a));
      memset(buf_b, 0, sizeof(buf_b));
      memcpy(buf_a, src_a, remainder);
      memcpy(buf_b, src_b, remainder);
      sse += SumSquareError(buf_a, buf_b, simd_size);
    }
  }
  return sse;
}

LIBYUV_API
uint64_t ComputeSumSquareErrorPlane(const uint8_t* src_a,
                                    int stride_a,
                                    const uint8_t* src_b,
                                    int stride_b,
                                    int width,
                                    int height) {
  uint64_t sse = 0;
  int h;
  if (!src_a || !src_b || width <= 0 || height <= 0) {
    return sse;
  }
  // Coalesce rows.
  if (stride_a == width && stride_b == width &&
      (ptrdiff_t)width * height <= INT_MAX) {
    width *= height;
    height = 1;
    stride_a = stride_b = 0;
  }
  for (h = 0; h < height; ++h) {
    sse += ComputeSumSquareError(src_a, src_b, width);
    src_a += stride_a;
    src_b += stride_b;
  }
  return sse;
}

LIBYUV_API
double SumSquareErrorToPsnr(uint64_t sse, uint64_t count) {
  double psnr;
  if (sse > 0) {
    double mse = (double)count / (double)sse;
    psnr = 10.0 * log10(255.0 * 255.0 * mse);
  } else {
    psnr = kMaxPsnr;  // Limit to prevent divide by 0
  }

  if (psnr > kMaxPsnr) {
    psnr = kMaxPsnr;
  }

  return psnr;
}

LIBYUV_API
double CalcFramePsnr(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b,
                     int width,
                     int height) {
  const uint64_t samples = (uint64_t)width * (uint64_t)height;
  const uint64_t sse = ComputeSumSquareErrorPlane(src_a, stride_a, src_b,
                                                  stride_b, width, height);
  return SumSquareErrorToPsnr(sse, samples);
}

LIBYUV_API
double I420Psnr(const uint8_t* src_y_a,
                int stride_y_a,
                const uint8_t* src_u_a,
                int stride_u_a,
                const uint8_t* src_v_a,
                int stride_v_a,
                const uint8_t* src_y_b,
                int stride_y_b,
                const uint8_t* src_u_b,
                int stride_u_b,
                const uint8_t* src_v_b,
                int stride_v_b,
                int width,
                int height) {
  const uint64_t sse_y = ComputeSumSquareErrorPlane(
      src_y_a, stride_y_a, src_y_b, stride_y_b, width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64_t sse_u = ComputeSumSquareErrorPlane(
      src_u_a, stride_u_a, src_u_b, stride_u_b, width_uv, height_uv);
  const uint64_t sse_v = ComputeSumSquareErrorPlane(
      src_v_a, stride_v_a, src_v_b, stride_v_b, width_uv, height_uv);
  const uint64_t samples = (uint64_t)width * (uint64_t)height +
                           2 * ((uint64_t)width_uv * (uint64_t)height_uv);
  const uint64_t sse = sse_y + sse_u + sse_v;
  return SumSquareErrorToPsnr(sse, samples);
}

static float Ssim8x8(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b) {
  int32_t sum_a = 0;
  int32_t sum_b = 0;
  int32_t sum_sq_ab = 0;
  int32_t sum_axb = 0;

  int i;
  for (i = 0; i < 8; ++i) {
    int j;
    for (j = 0; j < 8; ++j) {
      sum_a += src_a[j];
      sum_b += src_b[j];
      sum_sq_ab += src_a[j] * src_a[j] + src_b[j] * src_b[j];
      sum_axb += src_a[j] * src_b[j];
    }

    src_a += stride_a;
    src_b += stride_b;
  }

  {
    const int32_t c1 = 26634;   // (64^2*(.01*255)^2)
    const int32_t c2 = 239708;  // (64^2*(.03*255)^2)
    const int32_t sum_a_x_sum_b_2 = 2 * sum_a * sum_b;
    const int32_t sum_a_sq_b_sq = sum_a * sum_a + sum_b * sum_b;
    const int32_t n1 = sum_a_x_sum_b_2 + c1;
    const int32_t n2 = (sum_axb << 7) - sum_a_x_sum_b_2 + c2;
    const int32_t d1 = sum_a_sq_b_sq + c1;
    const int32_t d2 = (sum_sq_ab << 6) - sum_a_sq_b_sq + c2;
    return ((float)n1 * (float)n2) / ((float)d1 * (float)d2);
  }
}

static void Ssim4x4Row_Rem(void (*Ssim4x4Row)(const uint8_t* src_a,
                                              int stride_a,
                                              const uint8_t* src_b,
                                              int stride_b,
                                              uint32_t* dst,
                                              int width),
                           int simd_size,
                           const uint8_t* src_a,
                           int stride_a,
                           const uint8_t* src_b,
                           int stride_b,
                           uint32_t* dst,
                           int rem_width) {
  if (Ssim4x4Row == Ssim4x4Row_C) {
    Ssim4x4Row_C(src_a, stride_a, src_b, stride_b, dst, rem_width);
    return;
  }
  SIMD_ALIGNED(uint8_t buf_a[32 * 4]);
  SIMD_ALIGNED(uint8_t buf_b[32 * 4]);
  SIMD_ALIGNED(uint32_t buf_dst[32]);
  memset(buf_a, 0, sizeof(buf_a));
  memset(buf_b, 0, sizeof(buf_b));
  int r;
  for (r = 0; r < 4; ++r) {
    memcpy(buf_a + r * 32, src_a + (ptrdiff_t)r * stride_a, rem_width);
    memcpy(buf_b + r * 32, src_b + (ptrdiff_t)r * stride_b, rem_width);
  }
  Ssim4x4Row(buf_a, 32, buf_b, 32, buf_dst, simd_size);
  memcpy(dst, buf_dst, (size_t)rem_width * sizeof(uint32_t));
}

static float SsimWindowRow_Rem(float (*SsimWindowRow)(const uint32_t* row0,
                                                      const uint32_t* row1,
                                                      int num_windows),
                               int win_simd_size,
                               const uint32_t* row0,
                               const uint32_t* row1,
                               int rem_windows) {
  if (SsimWindowRow == SsimWindowRow_C) {
    return SsimWindowRow_C(row0, row1, rem_windows);
  }
  SIMD_ALIGNED(uint32_t win0[9 * 4]);
  SIMD_ALIGNED(uint32_t win1[9 * 4]);
  memset(win0, 0, sizeof(win0));
  memset(win1, 0, sizeof(win1));
  // Set odd tiles (1, 3, 5, 7) in win1 so each unused window k >= 1 has
  // sum_a = 4439, sum_b = -3, sum_sq = 307887, giving n1 = 2*4439*(-3) + c1 = 0
  // and SSIM = 0.0f (avoiding float precision loss from adding 1.0f per lane).
  int k;
  for (k = 1; k <= win_simd_size; k += 2) {
    win1[k * 4 + 0] = 4439u;
    win1[k * 4 + 1] = (uint32_t)-3;
    win1[k * 4 + 2] = 307887u;
  }
  float total = 0.0f;
  int w;
  for (w = 0; w < rem_windows; ++w) {
    const uint32_t* r0 = row0 + w * 4;
    const uint32_t* r1 = row1 + w * 4;
    win0[0] = r0[0] + r0[4];
    win0[1] = r0[1] + r0[5];
    win0[2] = r0[2] + r0[6];
    win0[3] = r0[3] + r0[7];
    win1[0] = r1[0] + r1[4] - 4439u;
    win1[1] = r1[1] + r1[5] - (uint32_t)-3;
    win1[2] = r1[2] + r1[6] - 307887u;
    win1[3] = r1[3] + r1[7];
    total += SsimWindowRow(win0, win1, win_simd_size);
  }
  return total;
}

// We are using a 8x8 moving window with starting location of each 8x8 window
// on the 4x4 pixel grid. Such arrangement allows the windows to overlap
// block boundaries to penalize blocking artifacts.
LIBYUV_API
double CalcFrameSsim(const uint8_t* src_a,
                     int stride_a,
                     const uint8_t* src_b,
                     int stride_b,
                     int width,
                     int height) {
  int samples = 0;
  double ssim_total = 0;
  // Return 0 rather than 0.0 / 0 (NaN) when the plane is too small for any
  // sample windows, so I420Ssim on small images stays finite.
  if (width <= 8 || height <= 8) {
    return 0.0;
  }

  int simd_size = 4;
  void (*Ssim4x4Row)(const uint8_t* src_a, int stride_a, const uint8_t* src_b,
                     int stride_b, uint32_t* dst, int width) = Ssim4x4Row_C;
  int win_simd_size = 1;
  float (*SsimWindowRow)(const uint32_t* row0, const uint32_t* row1,
                         int num_windows) = SsimWindowRow_C;
#if defined(HAS_SSIM4X4ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    Ssim4x4Row = Ssim4x4Row_SSE2;
    simd_size = 16;
  }
#endif
#if defined(HAS_SSIM4X4ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    Ssim4x4Row = Ssim4x4Row_AVX2;
    simd_size = 32;
  }
#endif
#if defined(HAS_SSIM4X4ROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    Ssim4x4Row = Ssim4x4Row_AVX512BW;
    simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SsimWindowRow = SsimWindowRow_SSE2;
    win_simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    SsimWindowRow = SsimWindowRow_AVX2;
    win_simd_size = 8;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_AVX512BW)
  if (TestCpuFlag(kCpuHasAVX512BW)) {
    SsimWindowRow = SsimWindowRow_AVX512BW;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    Ssim4x4Row = Ssim4x4Row_NEON;
    simd_size = 16;
  }
#endif
#if defined(HAS_SSIM4X4ROW_NEON_DOTPROD)
  if (TestCpuFlag(kCpuHasNeonDotProd)) {
    Ssim4x4Row = Ssim4x4Row_NEON_DotProd;
    simd_size = 16;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SsimWindowRow = SsimWindowRow_NEON;
    win_simd_size = 4;
  }
#endif
#if defined(HAS_SSIM4X4ROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    Ssim4x4Row = Ssim4x4Row_SVE2;
    simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_SVE2)
  if (TestCpuFlag(kCpuHasSVE2)) {
    SsimWindowRow = SsimWindowRow_SVE2;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    Ssim4x4Row = Ssim4x4Row_SME;
    simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_SME)
  if (TestCpuFlag(kCpuHasSME)) {
    SsimWindowRow = SsimWindowRow_SME;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    Ssim4x4Row = Ssim4x4Row_RVV;
    simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    SsimWindowRow = SsimWindowRow_RVV;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    Ssim4x4Row = Ssim4x4Row_LSX;
    simd_size = 16;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    SsimWindowRow = SsimWindowRow_LSX;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    Ssim4x4Row = Ssim4x4Row_LASX;
    simd_size = 32;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    SsimWindowRow = SsimWindowRow_LASX;
    win_simd_size = 1;
  }
#endif
#if defined(HAS_SSIM4X4ROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    Ssim4x4Row = Ssim4x4Row_WASMSIMD;
    simd_size = 16;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_WASMSIMD)
  if (TestCpuFlag(kCpuHasWASMSIMD)) {
    SsimWindowRow = SsimWindowRow_WASMSIMD;
    win_simd_size = 4;
  }
#endif
#if defined(HAS_SSIM4X4ROW_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    Ssim4x4Row = Ssim4x4Row_HVX;
    simd_size = 4;
  }
#endif
#if defined(HAS_SSIMWINDOWROW_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    SsimWindowRow = SsimWindowRow_HVX;
    win_simd_size = 1;
  }
#endif

  const int num_windows = (width - 8 + 3) >> 2;
  const int num_tiles = num_windows + 1;
  const int tile_width = num_tiles * 4;
  const int main_width = tile_width & ~(simd_size - 1);
  const int rem_width = tile_width & (simd_size - 1);
  const int main_windows = num_windows & ~(win_simd_size - 1);
  const int rem_windows = num_windows & (win_simd_size - 1);

#ifdef _OPENMP
#pragma omp parallel reduction(+ : ssim_total, samples)
#endif
  {
    align_buffer_64(row_buf, (size_t)tile_width * 2 * sizeof(uint32_t));
    if (row_buf) {
      uint32_t* row0 = (uint32_t*)row_buf;
      uint32_t* row1 = row0 + tile_width;
      int prev_i = -8;
      int i;
#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
      for (i = 0; i < height - 8; i += 4) {
        const uint8_t* a = src_a + (ptrdiff_t)i * stride_a;
        const uint8_t* b = src_b + (ptrdiff_t)i * stride_b;
        if (prev_i + 4 != i) {
          if (main_width > 0) {
            Ssim4x4Row(a, stride_a, b, stride_b, row0, main_width);
          }
          if (rem_width > 0) {
            Ssim4x4Row_Rem(Ssim4x4Row, simd_size, a + main_width, stride_a,
                           b + main_width, stride_b, row0 + main_width,
                           rem_width);
          }
        }
        a += (ptrdiff_t)stride_a * 4;
        b += (ptrdiff_t)stride_b * 4;
        if (main_width > 0) {
          Ssim4x4Row(a, stride_a, b, stride_b, row1, main_width);
        }
        if (rem_width > 0) {
          Ssim4x4Row_Rem(Ssim4x4Row, simd_size, a + main_width, stride_a,
                         b + main_width, stride_b, row1 + main_width,
                         rem_width);
        }
        if (main_windows > 0) {
          ssim_total += SsimWindowRow(row0, row1, main_windows);
        }
        if (rem_windows > 0) {
          ssim_total += SsimWindowRow_Rem(SsimWindowRow, win_simd_size,
                                          row0 + main_windows * 4,
                                          row1 + main_windows * 4, rem_windows);
        }
        samples += num_windows;
        uint32_t* tmp = row0;
        row0 = row1;
        row1 = tmp;
        prev_i = i;
      }
      free_aligned_buffer_64(row_buf);
    } else {
      int i;
#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
      for (i = 0; i < height - 8; i += 4) {
        const uint8_t* a = src_a + (ptrdiff_t)i * stride_a;
        const uint8_t* b = src_b + (ptrdiff_t)i * stride_b;
        int j;
        for (j = 0; j < width - 8; j += 4) {
          ssim_total += Ssim8x8(a + j, stride_a, b + j, stride_b);
          samples++;
        }
      }
    }
  }

  ssim_total /= samples;
  return ssim_total;
}

LIBYUV_API
double I420Ssim(const uint8_t* src_y_a,
                int stride_y_a,
                const uint8_t* src_u_a,
                int stride_u_a,
                const uint8_t* src_v_a,
                int stride_v_a,
                const uint8_t* src_y_b,
                int stride_y_b,
                const uint8_t* src_u_b,
                int stride_u_b,
                const uint8_t* src_v_b,
                int stride_v_b,
                int width,
                int height) {
  const double ssim_y =
      CalcFrameSsim(src_y_a, stride_y_a, src_y_b, stride_y_b, width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const double ssim_u = CalcFrameSsim(src_u_a, stride_u_a, src_u_b, stride_u_b,
                                      width_uv, height_uv);
  const double ssim_v = CalcFrameSsim(src_v_a, stride_v_a, src_v_b, stride_v_b,
                                      width_uv, height_uv);
  return ssim_y * 0.8 + 0.1 * (ssim_u + ssim_v);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
