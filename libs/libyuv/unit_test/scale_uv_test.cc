/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"
#include "libyuv/scale_uv.h"

namespace libyuv {

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int UVTestFilter(int src_width,
                        int src_height,
                        int dst_width,
                        int dst_height,
                        FilterMode f,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }
  if (src_width <= 0 || src_height == 0 || dst_width <= 0 || dst_height <= 0) {
    return 0;
  }

  int i;
  int64_t src_uv_plane_size = Abs(src_width) * Abs(src_height) * 2LL;
  int src_stride_uv = Abs(src_width) * 2;
  int64_t dst_uv_plane_size = dst_width * dst_height * 2LL;
  int dst_stride_uv = dst_width * 2;

  align_buffer_page_end(src_uv, src_uv_plane_size);
  align_buffer_page_end(dst_uv_c, dst_uv_plane_size);
  align_buffer_page_end(dst_uv_opt, dst_uv_plane_size);

  if (!src_uv || !dst_uv_c || !dst_uv_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_uv, src_uv_plane_size);
  memset(dst_uv_c, 2, dst_uv_plane_size);
  memset(dst_uv_opt, 123, dst_uv_plane_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  UVScale(src_uv, src_stride_uv, src_width, src_height, dst_uv_c, dst_stride_uv,
          dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    UVScale(src_uv, src_stride_uv, src_width, src_height, dst_uv_opt,
            dst_stride_uv, dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  int max_diff = 0;
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_uv_c[i] - dst_uv_opt[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_uv_c);
  free_aligned_buffer_page_end(dst_uv_opt);
  free_aligned_buffer_page_end(src_uv);
  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
#define DX(x, nom, denom) static_cast<int>((Abs(x) / nom) * nom)
#define SX(x, nom, denom) static_cast<int>((x / nom) * denom)

#define TEST_FACTOR1(name, filter, nom, denom)                               \
  TEST_F(LibYUVScaleTest, UVScaleDownBy##name##_##filter) {                  \
    int diff = UVTestFilter(                                                 \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    ASSERT_EQ(0, diff);                                                      \
  }

#if defined(ENABLE_FULL_TESTS)
// Test a scale factor with all 4 filters.  Expect exact for SIMD vs C.
#define TEST_FACTOR(name, nom, denom)      \
  TEST_FACTOR1(name, None, nom, denom)     \
  TEST_FACTOR1(name, Linear, nom, denom)   \
  TEST_FACTOR1(name, Bilinear, nom, denom) \
  TEST_FACTOR1(name, Box, nom, denom)
#else
// Test a scale factor with Bilinear.
#define TEST_FACTOR(name, nom, denom) TEST_FACTOR1(name, Bilinear, nom, denom)
#endif

TEST_FACTOR(2, 1, 2)
TEST_FACTOR(4, 1, 4)
// TEST_FACTOR(8, 1, 8)  Disable for benchmark performance.
TEST_FACTOR(3by4, 3, 4)
TEST_FACTOR(3by8, 3, 8)
TEST_FACTOR(3, 1, 3)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(name, width, height, filter, max_diff)                \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {          \
    int diff = UVTestFilter(benchmark_width_, benchmark_height_, width,     \
                            height, kFilter##filter, benchmark_iterations_, \
                            disable_cpu_flags_, benchmark_cpu_info_);       \
    ASSERT_LE(diff, max_diff);                                              \
  }                                                                         \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {        \
    int diff = UVTestFilter(width, height, Abs(benchmark_width_),           \
                            Abs(benchmark_height_), kFilter##filter,        \
                            benchmark_iterations_, disable_cpu_flags_,      \
                            benchmark_cpu_info_);                           \
    ASSERT_LE(diff, max_diff);                                              \
  }

#if defined(ENABLE_FULL_TESTS)
/// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)       \
  TEST_SCALETO1(name, width, height, None, 0)   \
  TEST_SCALETO1(name, width, height, Linear, 3) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)
#else
#define TEST_SCALETO(name, width, height) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)
#endif

TEST_SCALETO(UVScale, 1, 1)
TEST_SCALETO(UVScale, 569, 480)
TEST_SCALETO(UVScale, 640, 360)
#ifndef DISABLE_SLOW_TESTS
TEST_SCALETO(UVScale, 256, 144) /* 128x72 * 2 */
TEST_SCALETO(UVScale, 320, 240)
TEST_SCALETO(UVScale, 1280, 720)
TEST_SCALETO(UVScale, 1920, 1080)
#endif  // DISABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(name, filter, max_diff)                              \
  TEST_F(LibYUVScaleTest, name##SwapXY_##filter) {                             \
    int diff =                                                                 \
        UVTestFilter(benchmark_width_, benchmark_height_, benchmark_height_,   \
                     benchmark_width_, kFilter##filter, benchmark_iterations_, \
                     disable_cpu_flags_, benchmark_cpu_info_);                 \
    ASSERT_LE(diff, max_diff);                                                 \
  }

#if defined(ENABLE_FULL_TESTS)
// Test scale with swapped width and height with all 3 filters.
TEST_SCALESWAPXY1(UVScale, None, 0)
TEST_SCALESWAPXY1(UVScale, Linear, 0)
TEST_SCALESWAPXY1(UVScale, Bilinear, 0)
#else
TEST_SCALESWAPXY1(UVScale, Bilinear, 0)
#endif
#undef TEST_SCALESWAPXY1

TEST_F(LibYUVScaleTest, UVTest3x) {
  const int kSrcStride = 480 * 2;
  const int kDstStride = 160 * 2;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 480 * 3; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    UVScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
            kFilterBilinear);
  }

  ASSERT_EQ(225, dest_pixels[0]);
  ASSERT_EQ(255 - 225, dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
          kFilterNone);

  ASSERT_EQ(225, dest_pixels[0]);
  ASSERT_EQ(255 - 225, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, UVTest4x) {
  const int kSrcStride = 640 * 2;
  const int kDstStride = 160 * 2;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 640 * 4; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    UVScale(orig_pixels, kSrcStride, 640, 4, dest_pixels, kDstStride, 160, 1,
            kFilterBilinear);
  }

  ASSERT_EQ(66, dest_pixels[0]);
  ASSERT_EQ(190, dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
          kFilterNone);

  ASSERT_EQ(2, dest_pixels[0]);  // expect the 3rd pixel of the 3rd row
  ASSERT_EQ(255 - 2, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

// Box filtering an interleaved UV plane has to produce the same bytes as box
// filtering the U and V planes separately, which is the path I420 takes.
// C and SIMD agreeing is not enough here: before ScaleUVBox existed both
// point sampled, so this compares against an independent implementation.
static int UVTestBoxAgainstPlanes(int src_width,
                                  int src_height,
                                  int dst_width,
                                  int dst_height,
                                  int cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }
  if (src_width <= 0 || src_height == 0 || dst_width <= 0 || dst_height <= 0) {
    return 0;
  }
  int64_t src_uv_plane_size = src_width * src_height * 2LL;
  int64_t src_plane_size = src_width * src_height * 1LL;
  int64_t dst_uv_plane_size = dst_width * dst_height * 2LL;
  int64_t dst_plane_size = dst_width * dst_height * 1LL;

  align_buffer_page_end(src_uv, src_uv_plane_size);
  align_buffer_page_end(src_u, src_plane_size);
  align_buffer_page_end(src_v, src_plane_size);
  align_buffer_page_end(dst_uv, dst_uv_plane_size);
  align_buffer_page_end(dst_u, dst_plane_size);
  align_buffer_page_end(dst_v, dst_plane_size);
  if (!src_uv || !src_u || !src_v || !dst_uv || !dst_u || !dst_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_uv, src_uv_plane_size);
  memset(dst_uv, 123, dst_uv_plane_size);
  for (int i = 0; i < src_width * src_height; ++i) {
    src_u[i] = src_uv[i * 2 + 0];
    src_v[i] = src_uv[i * 2 + 1];
  }

  MaskCpuFlags(cpu_info);
  UVScale(src_uv, src_width * 2, src_width, src_height, dst_uv, dst_width * 2,
          dst_width, dst_height, kFilterBox);
  ScalePlane(src_u, src_width, src_width, src_height, dst_u, dst_width,
             dst_width, dst_height, kFilterBox);
  ScalePlane(src_v, src_width, src_width, src_height, dst_v, dst_width,
             dst_width, dst_height, kFilterBox);

  int max_diff = 0;
  for (int i = 0; i < dst_width * dst_height; ++i) {
    int du = Abs(dst_uv[i * 2 + 0] - dst_u[i]);
    int dv = Abs(dst_uv[i * 2 + 1] - dst_v[i]);
    if (du > max_diff) {
      max_diff = du;
    }
    if (dv > max_diff) {
      max_diff = dv;
    }
  }

  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_uv);
  return max_diff;
}

// Chroma dimensions, ie half the frame sizes named in the comments.
// max_diff is 0 for everything ScaleUVBox handles. The 1/2 and 1/4 fast paths
// predate it and keep their own rounding, so 1/4 is allowed to drift by one.
#define TEST_BOXPLANE(name, swidth, sheight, dwidth, dheight, max_diff)     \
  TEST_F(LibYUVScaleTest, UVScaleBoxMatchesPlane##name) {                   \
    int diff_c = UVTestBoxAgainstPlanes(swidth, sheight, dwidth, dheight,   \
                                        disable_cpu_flags_);                \
    ASSERT_LE(diff_c, max_diff);                                            \
    int diff_opt = UVTestBoxAgainstPlanes(swidth, sheight, dwidth, dheight, \
                                          benchmark_cpu_info_);             \
    ASSERT_LE(diff_opt, max_diff);                                          \
  }

TEST_BOXPLANE(By2, 960, 540, 480, 270, 0)         /* 1080p to 960x540 */
TEST_BOXPLANE(By3, 960, 540, 320, 180, 0)         /* 1080p to 640x360 */
TEST_BOXPLANE(By3Rounded, 640, 360, 214, 120, 0)  /* 720p to 427x240 */
TEST_BOXPLANE(By4, 960, 540, 240, 135, 1)         /* ScaleUVDown4Box */
TEST_BOXPLANE(By3_75, 960, 540, 256, 144, 0)      /* 1080p to 512x288 */
TEST_BOXPLANE(By6, 640, 360, 106, 60, 0)          /* 720p to 213x120 */
TEST_BOXPLANE(By8, 640, 360, 80, 45, 0)           /* 720p to 160x90 */
TEST_BOXPLANE(OddTail, 33, 17, 7, 5, 0)           /* not a multiple of 8 wide */
TEST_BOXPLANE(By3OddTail7, 21, 15, 7, 5, 0)       /* 3x, dst_width = 7 */
TEST_BOXPLANE(By3OddTail23, 69, 15, 23, 5, 0)     /* 3x, dst_width = 23 */
TEST_BOXPLANE(By3OddTail47, 141, 15, 47, 5, 0)    /* 3x, dst_width = 47 */
TEST_BOXPLANE(By3OddTail79, 237, 15, 79, 5, 0)    /* 3x, dst_width = 79 */
TEST_BOXPLANE(Tiny, 80, 60, 3, 2, 0)              /* dst below one SIMD group */
#undef TEST_BOXPLANE

// A box filter has to read the whole box. Every 3x3 block here is 255 except
// for its top left pixel, so point sampling the block origin yields 0 while
// the box average is 8 * 255 * (65536 / 9) >> 16 = 226.
TEST_F(LibYUVScaleTest, UVScaleBoxAverages) {
  const int kSrcWidth = 48;
  const int kSrcHeight = 24;
  const int kDstWidth = kSrcWidth / 3;
  const int kDstHeight = kSrcHeight / 3;
  align_buffer_page_end(src_uv, kSrcWidth * kSrcHeight * 2);
  align_buffer_page_end(dst_uv, kDstWidth * kDstHeight * 2);

  memset(src_uv, 255, kSrcWidth * kSrcHeight * 2);
  for (int y = 0; y < kSrcHeight; y += 3) {
    for (int x = 0; x < kSrcWidth; x += 3) {
      src_uv[(y * kSrcWidth + x) * 2 + 0] = 0;
      src_uv[(y * kSrcWidth + x) * 2 + 1] = 0;
    }
  }
  memset(dst_uv, 123, kDstWidth * kDstHeight * 2);

  UVScale(src_uv, kSrcWidth * 2, kSrcWidth, kSrcHeight, dst_uv, kDstWidth * 2,
          kDstWidth, kDstHeight, kFilterBox);

  for (int i = 0; i < kDstWidth * kDstHeight * 2; ++i) {
    ASSERT_EQ(226, dst_uv[i]) << "at " << i;
  }

  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(src_uv);
}

// Test 16-bit UVScale_16 with C vs Opt and return maximum pixel difference.
static int UVTestFilter_16(int src_width,
                           int src_height,
                           int dst_width,
                           int dst_height,
                           FilterMode f,
                           int benchmark_iterations,
                           int disable_cpu_flags,
                           int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }
  if (src_width <= 0 || src_height == 0 || dst_width <= 0 || dst_height <= 0) {
    return 0;
  }

  int i;
  int64_t src_uv_samples = Abs(src_width) * Abs(src_height) * 2LL;
  int src_stride_uv = Abs(src_width) * 2;
  int64_t dst_uv_samples = dst_width * dst_height * 2LL;
  int dst_stride_uv = dst_width * 2;

  align_buffer_page_end(src_uv_buf, src_uv_samples * 2);
  align_buffer_page_end(dst_uv_c_buf, dst_uv_samples * 2);
  align_buffer_page_end(dst_uv_opt_buf, dst_uv_samples * 2);

  if (!src_uv_buf || !dst_uv_c_buf || !dst_uv_opt_buf) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* src_uv = reinterpret_cast<uint16_t*>(src_uv_buf);
  uint16_t* dst_uv_c = reinterpret_cast<uint16_t*>(dst_uv_c_buf);
  uint16_t* dst_uv_opt = reinterpret_cast<uint16_t*>(dst_uv_opt_buf);

  MemRandomize(src_uv_buf, src_uv_samples * 2);
  memset(dst_uv_c_buf, 2, dst_uv_samples * 2);
  memset(dst_uv_opt_buf, 123, dst_uv_samples * 2);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  int rc_c = UVScale_16(src_uv, src_stride_uv, src_width, src_height, dst_uv_c,
                        dst_stride_uv, dst_width, dst_height, f);
  if (rc_c != 0) {
    free_aligned_buffer_page_end(dst_uv_c_buf);
    free_aligned_buffer_page_end(dst_uv_opt_buf);
    free_aligned_buffer_page_end(src_uv_buf);
    return 65536;
  }

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  int rc_opt = 0;
  for (i = 0; i < benchmark_iterations; ++i) {
    rc_opt |= UVScale_16(src_uv, src_stride_uv, src_width, src_height,
                         dst_uv_opt, dst_stride_uv, dst_width, dst_height, f);
  }
  if (rc_opt != 0) {
    free_aligned_buffer_page_end(dst_uv_c_buf);
    free_aligned_buffer_page_end(dst_uv_opt_buf);
    free_aligned_buffer_page_end(src_uv_buf);
    return 65536;
  }

  int max_diff = 0;
  for (i = 0; i < dst_uv_samples; ++i) {
    int abs_diff = Abs(static_cast<int>(dst_uv_c[i]) -
                       static_cast<int>(dst_uv_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_uv_c_buf);
  free_aligned_buffer_page_end(dst_uv_opt_buf);
  free_aligned_buffer_page_end(src_uv_buf);
  return max_diff;
}

#define DX(x, nom, denom) static_cast<int>((Abs(x) / nom) * nom)
#define SX(x, nom, denom) static_cast<int>((x / nom) * denom)

#define TEST_FACTOR1_16(name, filter, nom, denom)                            \
  TEST_F(LibYUVScaleTest, UVScale_16_DownBy##name##_##filter) {              \
    int diff = UVTestFilter_16(                                              \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    ASSERT_EQ(0, diff);                                                      \
  }

#if defined(ENABLE_FULL_TESTS)
#define TEST_FACTOR_16(name, nom, denom)      \
  TEST_FACTOR1_16(name, None, nom, denom)     \
  TEST_FACTOR1_16(name, Linear, nom, denom)   \
  TEST_FACTOR1_16(name, Bilinear, nom, denom) \
  TEST_FACTOR1_16(name, Box, nom, denom)
#else
#define TEST_FACTOR_16(name, nom, denom) \
  TEST_FACTOR1_16(name, Bilinear, nom, denom)
#endif

TEST_FACTOR_16(2, 1, 2)
TEST_FACTOR_16(4, 1, 4)
TEST_FACTOR_16(3by4, 3, 4)
TEST_FACTOR_16(3by8, 3, 8)
TEST_FACTOR_16(3, 1, 3)
#undef TEST_FACTOR1_16
#undef TEST_FACTOR_16
#undef SX
#undef DX

#define TEST_SCALETO1_16(name, width, height, filter, max_diff)                \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {             \
    int diff = UVTestFilter_16(benchmark_width_, benchmark_height_, width,     \
                               height, kFilter##filter, benchmark_iterations_, \
                               disable_cpu_flags_, benchmark_cpu_info_);       \
    ASSERT_LE(diff, max_diff);                                                 \
  }                                                                            \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {           \
    int diff = UVTestFilter_16(width, height, Abs(benchmark_width_),           \
                               Abs(benchmark_height_), kFilter##filter,        \
                               benchmark_iterations_, disable_cpu_flags_,      \
                               benchmark_cpu_info_);                           \
    ASSERT_LE(diff, max_diff);                                                 \
  }

#if defined(ENABLE_FULL_TESTS)
#define TEST_SCALETO_16(name, width, height)       \
  TEST_SCALETO1_16(name, width, height, None, 0)   \
  TEST_SCALETO1_16(name, width, height, Linear, 0) \
  TEST_SCALETO1_16(name, width, height, Bilinear, 0)
#else
#define TEST_SCALETO_16(name, width, height) \
  TEST_SCALETO1_16(name, width, height, Bilinear, 0)
#endif

TEST_SCALETO_16(UVScale_16, 1, 1)
TEST_SCALETO_16(UVScale_16, 569, 480)
TEST_SCALETO_16(UVScale_16, 640, 360)
#undef TEST_SCALETO1_16
#undef TEST_SCALETO_16

// Compare UVScale_16 against scaling U and V planes separately with
// ScalePlane_16 across Box, Bilinear, Linear, and None filters.
static int UVTestAgainstPlanes_16(int src_width,
                                  int src_height,
                                  int dst_width,
                                  int dst_height,
                                  FilterMode filtering,
                                  int cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }
  if (src_width <= 0 || src_height == 0 || dst_width <= 0 || dst_height <= 0) {
    return 0;
  }
  const int abs_src_width = Abs(src_width);
  const int abs_src_height = Abs(src_height);
  int64_t src_uv_samples = abs_src_width * abs_src_height * 2LL;
  int64_t src_plane_samples = abs_src_width * abs_src_height * 1LL;
  int64_t dst_uv_samples = dst_width * dst_height * 2LL;
  int64_t dst_plane_samples = dst_width * dst_height * 1LL;

  align_buffer_page_end(src_uv_buf, src_uv_samples * 2);
  align_buffer_page_end(src_u_buf, src_plane_samples * 2);
  align_buffer_page_end(src_v_buf, src_plane_samples * 2);
  align_buffer_page_end(dst_uv_buf, dst_uv_samples * 2);
  align_buffer_page_end(dst_u_buf, dst_plane_samples * 2);
  align_buffer_page_end(dst_v_buf, dst_plane_samples * 2);
  if (!src_uv_buf || !src_u_buf || !src_v_buf || !dst_uv_buf || !dst_u_buf ||
      !dst_v_buf) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* src_uv = reinterpret_cast<uint16_t*>(src_uv_buf);
  uint16_t* src_u = reinterpret_cast<uint16_t*>(src_u_buf);
  uint16_t* src_v = reinterpret_cast<uint16_t*>(src_v_buf);
  uint16_t* dst_uv = reinterpret_cast<uint16_t*>(dst_uv_buf);
  uint16_t* dst_u = reinterpret_cast<uint16_t*>(dst_u_buf);
  uint16_t* dst_v = reinterpret_cast<uint16_t*>(dst_v_buf);

  MemRandomize(src_uv_buf, src_uv_samples * 2);
  memset(dst_uv_buf, 123, dst_uv_samples * 2);
  for (int i = 0; i < abs_src_width * abs_src_height; ++i) {
    src_u[i] = src_uv[i * 2 + 0];
    src_v[i] = src_uv[i * 2 + 1];
  }

  MaskCpuFlags(cpu_info);
  int rc_uv = UVScale_16(src_uv, abs_src_width * 2, src_width, src_height,
                         dst_uv, dst_width * 2, dst_width, dst_height,
                         filtering);
  int rc_u = ScalePlane_16(src_u, abs_src_width, src_width, src_height, dst_u,
                           dst_width, dst_width, dst_height, filtering);
  int rc_v = ScalePlane_16(src_v, abs_src_width, src_width, src_height, dst_v,
                           dst_width, dst_width, dst_height, filtering);
  int max_diff = (rc_uv != 0 || rc_u != 0 || rc_v != 0) ? 65536 : 0;
  for (int i = 0; i < dst_width * dst_height; ++i) {
    int du = Abs(static_cast<int>(dst_uv[i * 2 + 0]) -
                 static_cast<int>(dst_u[i]));
    int dv = Abs(static_cast<int>(dst_uv[i * 2 + 1]) -
                 static_cast<int>(dst_v[i]));
    if (du > max_diff) {
      max_diff = du;
    }
    if (dv > max_diff) {
      max_diff = dv;
    }
  }

  free_aligned_buffer_page_end(dst_v_buf);
  free_aligned_buffer_page_end(dst_u_buf);
  free_aligned_buffer_page_end(dst_uv_buf);
  free_aligned_buffer_page_end(src_v_buf);
  free_aligned_buffer_page_end(src_u_buf);
  free_aligned_buffer_page_end(src_uv_buf);
  return max_diff;
}

#define TEST_UVPLANE16(name, swidth, sheight, dwidth, dheight, filter,         \
                       max_diff)                                               \
  TEST_F(LibYUVScaleTest, UVScale_16_MatchesPlane_##name##_##filter) {         \
    int diff_c = UVTestAgainstPlanes_16(swidth, sheight, dwidth, dheight,      \
                                        kFilter##filter, disable_cpu_flags_);  \
    ASSERT_LE(diff_c, max_diff);                                               \
    int diff_opt = UVTestAgainstPlanes_16(swidth, sheight, dwidth, dheight,    \
                                          kFilter##filter,                     \
                                          benchmark_cpu_info_);                \
    ASSERT_LE(diff_opt, max_diff);                                             \
  }

TEST_UVPLANE16(By2, 480, 270, 240, 135, Box, 0)
TEST_UVPLANE16(By2, 480, 270, 240, 135, Bilinear, 0)
TEST_UVPLANE16(By2, 480, 270, 240, 135, None, 0)
TEST_UVPLANE16(By3, 480, 270, 160, 90, Box, 0)
TEST_UVPLANE16(By3Rounded, 320, 180, 107, 60, Box, 0)
TEST_UVPLANE16(By4, 480, 270, 120, 68, Box, 0)
TEST_UVPLANE16(By4Exact, 480, 272, 120, 68, Box, 1)
TEST_UVPLANE16(By6, 360, 180, 60, 30, Box, 0)
TEST_UVPLANE16(By6, 360, 180, 60, 30, None, 0)
TEST_UVPLANE16(OddTail, 33, 17, 7, 5, Box, 0)
TEST_UVPLANE16(By3OddTail, 69, 15, 23, 5, Box, 0)
TEST_UVPLANE16(DownArbitrary, 113, 71, 47, 29, Bilinear, 0)
TEST_UVPLANE16(DownArbitrary, 113, 71, 47, 29, Linear, 0)
TEST_UVPLANE16(DownArbitrary, 113, 71, 47, 29, None, 0)
TEST_UVPLANE16(Up2x, 37, 23, 74, 46, Bilinear, 0)
TEST_UVPLANE16(Up2x, 37, 23, 74, 46, Linear, 0)
TEST_UVPLANE16(Up2x, 37, 23, 74, 46, None, 0)
TEST_UVPLANE16(UpArbitrary, 37, 23, 91, 59, Bilinear, 0)
TEST_UVPLANE16(UpArbitrary, 37, 23, 91, 59, Linear, 0)
TEST_UVPLANE16(UpArbitrary, 37, 23, 91, 59, None, 0)
TEST_UVPLANE16(VerticalOnlyUp, 41, 19, 41, 53, Bilinear, 0)
TEST_UVPLANE16(VerticalOnlyDown, 41, 53, 41, 19, Bilinear, 0)
TEST_UVPLANE16(InvertCopy, 31, -17, 31, 17, None, 0)
TEST_UVPLANE16(InvertUp, 31, -17, 65, 39, Bilinear, 0)
#undef TEST_UVPLANE16

// Verify 2x kFilterLinear downscale averages pairs horizontally on odd rows
// without underflowing into the previous row.
TEST_F(LibYUVScaleTest, UVScale_16_DownBy2_LinearAveragesOddRows) {
  const int kSrcWidth = 4;
  const int kSrcHeight = 2;
  const int kDstWidth = 2;
  const int kDstHeight = 1;
  uint16_t src_uv[kSrcWidth * kSrcHeight * 2] = {
      // Row 0 (even row - must not be read):
      60000, 60000, 60000, 60000, 60000, 60000, 60000, 60000,
      // Row 1 (odd row - horizontally averaged in pairs):
      100,   1000,  300,   3000,  500,   5000,  900,   7000,
  };
  uint16_t dst_uv[kDstWidth * kDstHeight * 2] = {0};

  ASSERT_EQ(0, UVScale_16(src_uv, kSrcWidth * 2, kSrcWidth, kSrcHeight, dst_uv,
                          kDstWidth * 2, kDstWidth, kDstHeight, kFilterLinear));
  EXPECT_EQ(200, dst_uv[0]);
  EXPECT_EQ(2000, dst_uv[1]);
  EXPECT_EQ(700, dst_uv[2]);
  EXPECT_EQ(6000, dst_uv[3]);
}

}  // namespace libyuv
