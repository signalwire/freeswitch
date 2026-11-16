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

#include <float.h>

#include "libyuv/video_common.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Hakmem method for hamming distance.
uint32_t HammingDistance_C(const uint8_t* src_a,
                           const uint8_t* src_b,
                           int count) {
  uint32_t diff = 0u;

  int i;
  for (i = 0; i < count - 3; i += 4) {
    uint32_t a = (uint32_t)src_a[0] | ((uint32_t)src_a[1] << 8) |
                 ((uint32_t)src_a[2] << 16) | ((uint32_t)src_a[3] << 24);
    uint32_t b = (uint32_t)src_b[0] | ((uint32_t)src_b[1] << 8) |
                 ((uint32_t)src_b[2] << 16) | ((uint32_t)src_b[3] << 24);
    uint32_t x = a ^ b;
    uint32_t u = x - ((x >> 1) & 0x55555555);
    u = ((u >> 2) & 0x33333333) + (u & 0x33333333);
    diff += ((((u + (u >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24);
    src_a += 4;
    src_b += 4;
  }

  for (; i < count; ++i) {
    uint32_t x = *src_a ^ *src_b;
    uint32_t u = x - ((x >> 1) & 0x55);
    u = ((u >> 2) & 0x33) + (u & 0x33);
    diff += (u + (u >> 4)) & 0x0f;
    src_a += 1;
    src_b += 1;
  }

  return diff;
}

uint32_t SumSquareError_C(const uint8_t* src_a,
                          const uint8_t* src_b,
                          int count) {
  uint32_t sse = 0u;
  int i;
  for (i = 0; i < count; ++i) {
    int diff = src_a[i] - src_b[i];
    sse += (uint32_t)(diff * diff);
  }
  return sse;
}

// hash seed of 5381 recommended.
// Internal C version of HashDjb2 with int sized count for efficiency.
uint32_t HashDjb2_C(const uint8_t* src, int count, uint32_t seed) {
  uint32_t hash = seed;
  int i;
  for (i = 0; i < count; ++i) {
    hash += (hash << 5) + src[i];
  }
  return hash;
}

// Compute 4x4 tile statistics (sum_a, sum_b, sum_sq_a + sum_sq_b, sum_axb)
// across 4 rows for width pixels (width is a multiple of 4).
void Ssim4x4Row_C(const uint8_t* src_a,
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
  int x;
  for (x = 0; x < width; x += 4) {
    uint32_t sum_a = a0[0] + a0[1] + a0[2] + a0[3] + a1[0] + a1[1] + a1[2] +
                     a1[3] + a2[0] + a2[1] + a2[2] + a2[3] + a3[0] + a3[1] +
                     a3[2] + a3[3];
    uint32_t sum_b = b0[0] + b0[1] + b0[2] + b0[3] + b1[0] + b1[1] + b1[2] +
                     b1[3] + b2[0] + b2[1] + b2[2] + b2[3] + b3[0] + b3[1] +
                     b3[2] + b3[3];
    uint32_t sum_sq_ab =
        a0[0] * a0[0] + a0[1] * a0[1] + a0[2] * a0[2] + a0[3] * a0[3] +
        a1[0] * a1[0] + a1[1] * a1[1] + a1[2] * a1[2] + a1[3] * a1[3] +
        a2[0] * a2[0] + a2[1] * a2[1] + a2[2] * a2[2] + a2[3] * a2[3] +
        a3[0] * a3[0] + a3[1] * a3[1] + a3[2] * a3[2] + a3[3] * a3[3] +
        b0[0] * b0[0] + b0[1] * b0[1] + b0[2] * b0[2] + b0[3] * b0[3] +
        b1[0] * b1[0] + b1[1] * b1[1] + b1[2] * b1[2] + b1[3] * b1[3] +
        b2[0] * b2[0] + b2[1] * b2[1] + b2[2] * b2[2] + b2[3] * b2[3] +
        b3[0] * b3[0] + b3[1] * b3[1] + b3[2] * b3[2] + b3[3] * b3[3];
    uint32_t sum_axb =
        a0[0] * b0[0] + a0[1] * b0[1] + a0[2] * b0[2] + a0[3] * b0[3] +
        a1[0] * b1[0] + a1[1] * b1[1] + a1[2] * b1[2] + a1[3] * b1[3] +
        a2[0] * b2[0] + a2[1] * b2[1] + a2[2] * b2[2] + a2[3] * b2[3] +
        a3[0] * b3[0] + a3[1] * b3[1] + a3[2] * b3[2] + a3[3] * b3[3];
    dst[0] = sum_a;
    dst[1] = sum_b;
    dst[2] = sum_sq_ab;
    dst[3] = sum_axb;
    a0 += 4;
    a1 += 4;
    a2 += 4;
    a3 += 4;
    b0 += 4;
    b1 += 4;
    b2 += 4;
    b3 += 4;
    dst += 4;
  }
}

// Combine two consecutive 4x4 tile rows (row0 and row1) into 8x8 windows
// stepped on a 4-pixel horizontal grid and return the sum of SSIM values.
float SsimWindowRow_C(const uint32_t* row0,
                      const uint32_t* row1,
                      int num_windows) {
  const int32_t c1 = 26634;   // (64^2*(.01*255)^2)
  const int32_t c2 = 239708;  // (64^2*(.03*255)^2)

  float ssim_total = 0.0f;
  int32_t prev_a = (int32_t)(row0[0] + row1[0]);
  int32_t prev_b = (int32_t)(row0[1] + row1[1]);
  int32_t prev_sq = (int32_t)(row0[2] + row1[2]);
  int32_t prev_axb = (int32_t)(row0[3] + row1[3]);
  int w;
  for (w = 0; w < num_windows; ++w) {
    row0 += 4;
    row1 += 4;
    const int32_t next_a = (int32_t)(row0[0] + row1[0]);
    const int32_t next_b = (int32_t)(row0[1] + row1[1]);
    const int32_t next_sq = (int32_t)(row0[2] + row1[2]);
    const int32_t next_axb = (int32_t)(row0[3] + row1[3]);
    const int32_t sum_a = prev_a + next_a;
    const int32_t sum_b = prev_b + next_b;
    const int32_t sum_sq_ab = prev_sq + next_sq;
    const int32_t sum_axb = prev_axb + next_axb;
    prev_a = next_a;
    prev_b = next_b;
    prev_sq = next_sq;
    prev_axb = next_axb;

    const int32_t sum_a_x_sum_b_2 = 2 * sum_a * sum_b;
    const int32_t sum_a_sq_b_sq = sum_a * sum_a + sum_b * sum_b;
    const int32_t n1 = sum_a_x_sum_b_2 + c1;
    const int32_t n2 = (sum_axb << 7) - sum_a_x_sum_b_2 + c2;
    const int32_t d1 = sum_a_sq_b_sq + c1;
    const int32_t d2 = (sum_sq_ab << 6) - sum_a_sq_b_sq + c2;
    ssim_total += ((float)n1 * (float)n2) / ((float)d1 * (float)d2);
  }
  return ssim_total;
}

uint32_t ARGBDetectRow_C(const uint8_t* argb, int width) {
  int x;
  for (x = 0; x < width - 1; x += 2) {
    if (argb[0] != 255) {  // First byte is not Alpha of 255, so not ARGB.
      return FOURCC_BGRA;
    }
    if (argb[3] != 255) {  // Fourth byte is not Alpha of 255, so not BGRA.
      return FOURCC_ARGB;
    }
    if (argb[4] != 255) {  // Second pixel first byte is not Alpha of 255.
      return FOURCC_BGRA;
    }
    if (argb[7] != 255) {  // Second pixel fourth byte is not Alpha of 255.
      return FOURCC_ARGB;
    }
    argb += 8;
  }
  if (width & 1) {
    if (argb[0] != 255) {  // First byte is not Alpha of 255, so not ARGB.
      return FOURCC_BGRA;
    }
    if (argb[3] != 255) {  // 4th byte is not Alpha of 255, so not BGRA.
      return FOURCC_ARGB;
    }
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
