/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/compare.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/scale.h"
#include "libyuv/scale_uv.h"

#if !defined(LIBYUV_USING_SHARED_LIBRARY) && !defined(COMPONENT_BUILD) && \
    !defined(ENABLE_ROW_TESTS)
#define ENABLE_ROW_TESTS
#endif

#ifdef ENABLE_ROW_TESTS
// row.h defines SIMD_ALIGNED, overriding unit_test.h
// TODO(fbarchard): Remove row.h from unittests.  Test public functions.
#include "libyuv/row.h" /* For ScaleSumSamples_Neon */
#endif

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define MSAN_POISON(ptr, size) __msan_allocated_memory(ptr, size)
#define MSAN_CHECK_INITIALIZED(ptr, size) \
  __msan_check_mem_is_initialized(ptr, size)
#endif
#endif
#ifndef MSAN_POISON
#define MSAN_POISON(ptr, size)
#endif
#ifndef MSAN_CHECK_INITIALIZED
#define MSAN_CHECK_INITIALIZED(ptr, size)
#endif

#define EXPECTED_UNATTENUATE_DIFF 0

namespace libyuv {

TEST_F(LibYUVPlanarTest, TestAttenuate) {
  const int kSize = 1280 * 4;
  align_buffer_page_end(orig_pixels, kSize);
  align_buffer_page_end(atten_pixels, kSize);
  align_buffer_page_end(unatten_pixels, kSize);
  align_buffer_page_end(atten2_pixels, kSize);

  // Test unattenuation clamps
  orig_pixels[0 * 4 + 0] = 200u;
  orig_pixels[0 * 4 + 1] = 129u;
  orig_pixels[0 * 4 + 2] = 127u;
  orig_pixels[0 * 4 + 3] = 128u;
  // Test unattenuation transparent and opaque are unaffected
  orig_pixels[1 * 4 + 0] = 16u;
  orig_pixels[1 * 4 + 1] = 64u;
  orig_pixels[1 * 4 + 2] = 192u;
  orig_pixels[1 * 4 + 3] = 0u;
  orig_pixels[2 * 4 + 0] = 16u;
  orig_pixels[2 * 4 + 1] = 64u;
  orig_pixels[2 * 4 + 2] = 192u;
  orig_pixels[2 * 4 + 3] = 128u;
  orig_pixels[3 * 4 + 0] = 16u;
  orig_pixels[3 * 4 + 1] = 64u;
  orig_pixels[3 * 4 + 2] = 192u;
  orig_pixels[3 * 4 + 3] = 255u;
  orig_pixels[4 * 4 + 0] = 255u;
  orig_pixels[4 * 4 + 1] = 255u;
  orig_pixels[4 * 4 + 2] = 255u;
  orig_pixels[4 * 4 + 3] = 255u;

  ARGBUnattenuate(orig_pixels, 0, unatten_pixels, 0, 5, 1);
  ASSERT_EQ(255u, unatten_pixels[0 * 4 + 0]);
  ASSERT_EQ(255u, unatten_pixels[0 * 4 + 1]);
  ASSERT_EQ(254u, unatten_pixels[0 * 4 + 2]);
  ASSERT_EQ(128u, unatten_pixels[0 * 4 + 3]);
  ASSERT_EQ(0u, unatten_pixels[1 * 4 + 0]);
  ASSERT_EQ(0u, unatten_pixels[1 * 4 + 1]);
  ASSERT_EQ(0u, unatten_pixels[1 * 4 + 2]);
  ASSERT_EQ(0u, unatten_pixels[1 * 4 + 3]);
  ASSERT_EQ(32u, unatten_pixels[2 * 4 + 0]);
  ASSERT_EQ(128u, unatten_pixels[2 * 4 + 1]);
  ASSERT_EQ(255u, unatten_pixels[2 * 4 + 2]);
  ASSERT_EQ(128u, unatten_pixels[2 * 4 + 3]);
  ASSERT_EQ(16u, unatten_pixels[3 * 4 + 0]);
  ASSERT_EQ(64u, unatten_pixels[3 * 4 + 1]);
  ASSERT_EQ(192u, unatten_pixels[3 * 4 + 2]);
  ASSERT_EQ(255u, unatten_pixels[3 * 4 + 3]);
  ASSERT_EQ(255u, unatten_pixels[4 * 4 + 0]);
  ASSERT_EQ(255u, unatten_pixels[4 * 4 + 1]);
  ASSERT_EQ(255u, unatten_pixels[4 * 4 + 2]);
  ASSERT_EQ(255u, unatten_pixels[4 * 4 + 3]);

  ARGBAttenuate(orig_pixels, 0, atten_pixels, 0, 5, 1);
  ASSERT_EQ(100u, atten_pixels[0 * 4 + 0]);
  ASSERT_EQ(65u, atten_pixels[0 * 4 + 1]);
  ASSERT_EQ(64u, atten_pixels[0 * 4 + 2]);
  ASSERT_EQ(128u, atten_pixels[0 * 4 + 3]);
  ASSERT_EQ(0u, atten_pixels[1 * 4 + 0]);
  ASSERT_EQ(0u, atten_pixels[1 * 4 + 1]);
  ASSERT_EQ(0u, atten_pixels[1 * 4 + 2]);
  ASSERT_EQ(0u, atten_pixels[1 * 4 + 3]);
  ASSERT_EQ(8u, atten_pixels[2 * 4 + 0]);
  ASSERT_EQ(32u, atten_pixels[2 * 4 + 1]);
  ASSERT_EQ(96u, atten_pixels[2 * 4 + 2]);
  ASSERT_EQ(128u, atten_pixels[2 * 4 + 3]);
  ASSERT_EQ(16u, atten_pixels[3 * 4 + 0]);
  ASSERT_EQ(64u, atten_pixels[3 * 4 + 1]);
  ASSERT_EQ(192u, atten_pixels[3 * 4 + 2]);
  ASSERT_EQ(255u, atten_pixels[3 * 4 + 3]);
  ASSERT_EQ(255u, atten_pixels[4 * 4 + 0]);
  ASSERT_EQ(255u, atten_pixels[4 * 4 + 1]);
  ASSERT_EQ(255u, atten_pixels[4 * 4 + 2]);
  ASSERT_EQ(255u, atten_pixels[4 * 4 + 3]);

  // test 255
  for (int i = 0; i < 256; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = 0;
    orig_pixels[i * 4 + 2] = 0;
    orig_pixels[i * 4 + 3] = 255;
  }
  ARGBAttenuate(orig_pixels, 0, atten_pixels, 0, 256, 1);
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(orig_pixels[i * 4 + 0], atten_pixels[i * 4 + 0]);
    ASSERT_EQ(0, atten_pixels[i * 4 + 1]);
    ASSERT_EQ(0, atten_pixels[i * 4 + 2]);
    ASSERT_EQ(255, atten_pixels[i * 4 + 3]);
  }

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = i / 2;
    orig_pixels[i * 4 + 2] = i / 3;
    orig_pixels[i * 4 + 3] = i;
  }
  ARGBAttenuate(orig_pixels, 0, atten_pixels, 0, 1280, 1);
  ARGBUnattenuate(atten_pixels, 0, unatten_pixels, 0, 1280, 1);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBAttenuate(unatten_pixels, 0, atten2_pixels, 0, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    ASSERT_NEAR(atten_pixels[i * 4 + 0], atten2_pixels[i * 4 + 0], 1);
    ASSERT_NEAR(atten_pixels[i * 4 + 1], atten2_pixels[i * 4 + 1], 1);
    ASSERT_NEAR(atten_pixels[i * 4 + 2], atten2_pixels[i * 4 + 2], 1);
    ASSERT_NEAR(atten_pixels[i * 4 + 3], atten2_pixels[i * 4 + 3], 1);
  }
  // Make sure transparent, 50% and opaque are fully accurate.
  ASSERT_EQ(0, atten_pixels[0 * 4 + 0]);
  ASSERT_EQ(0, atten_pixels[0 * 4 + 1]);
  ASSERT_EQ(0, atten_pixels[0 * 4 + 2]);
  ASSERT_EQ(0, atten_pixels[0 * 4 + 3]);
  ASSERT_EQ(64, atten_pixels[128 * 4 + 0]);
  ASSERT_EQ(32, atten_pixels[128 * 4 + 1]);
  ASSERT_EQ(21, atten_pixels[128 * 4 + 2]);
  ASSERT_EQ(128, atten_pixels[128 * 4 + 3]);
  ASSERT_EQ(255, atten_pixels[255 * 4 + 0]);
  ASSERT_EQ(127, atten_pixels[255 * 4 + 1]);
  ASSERT_EQ(85, atten_pixels[255 * 4 + 2]);
  ASSERT_EQ(255, atten_pixels[255 * 4 + 3]);

  free_aligned_buffer_page_end(atten2_pixels);
  free_aligned_buffer_page_end(unatten_pixels);
  free_aligned_buffer_page_end(atten_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

static int TestAttenuateI(int width,
                          int height,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info,
                          int invert,
                          int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBAttenuate(src_argb + off, kStride, dst_argb_c, kStride, width,
                invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAttenuate(src_argb + off, kStride, dst_argb_opt, kStride, width,
                  invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Any) {
  int max_diff = TestAttenuateI(benchmark_width_ + 1, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, +1, 0);

  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Unaligned) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Invert) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Opt) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(max_diff, 0);
}

static int TestUnattenuateI(int width,
                            int height,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info,
                            int invert,
                            int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb[i + off] = (fastrand() & 0xff);
  }
  ARGBAttenuate(src_argb + off, kStride, src_argb + off, kStride, width,
                height);
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBUnattenuate(src_argb + off, kStride, dst_argb_c, kStride, width,
                  invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBUnattenuate(src_argb + off, kStride, dst_argb_opt, kStride, width,
                    invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Any) {
  int max_diff = TestUnattenuateI(benchmark_width_ + 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Unaligned) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Invert) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Opt) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, TestARGBComputeCumulativeSum) {
  SIMD_ALIGNED(uint8_t orig_pixels[16][16][4]);
  SIMD_ALIGNED(int32_t added_pixels[16][16][4]);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      orig_pixels[y][x][0] = 1u;
      orig_pixels[y][x][1] = 2u;
      orig_pixels[y][x][2] = 3u;
      orig_pixels[y][x][3] = 255u;
    }
  }

  ARGBComputeCumulativeSum(&orig_pixels[0][0][0], 16 * 4,
                           &added_pixels[0][0][0], 16 * 4, 16, 16);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      ASSERT_EQ((x + 1) * (y + 1), added_pixels[y][x][0]);
      ASSERT_EQ((x + 1) * (y + 1) * 2, added_pixels[y][x][1]);
      ASSERT_EQ((x + 1) * (y + 1) * 3, added_pixels[y][x][2]);
      ASSERT_EQ((x + 1) * (y + 1) * 255, added_pixels[y][x][3]);
    }
  }
}

// near is for legacy platforms.
TEST_F(LibYUVPlanarTest, TestARGBGray) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  ASSERT_NEAR(29u, orig_pixels[0][0], 1);
  ASSERT_NEAR(29u, orig_pixels[0][1], 1);
  ASSERT_NEAR(29u, orig_pixels[0][2], 1);
  ASSERT_EQ(128u, orig_pixels[0][3]);
  ASSERT_EQ(149u, orig_pixels[1][0]);
  ASSERT_EQ(149u, orig_pixels[1][1]);
  ASSERT_EQ(149u, orig_pixels[1][2]);
  ASSERT_EQ(0u, orig_pixels[1][3]);
  ASSERT_NEAR(77u, orig_pixels[2][0], 1);
  ASSERT_NEAR(77u, orig_pixels[2][1], 1);
  ASSERT_NEAR(77u, orig_pixels[2][2], 1);
  ASSERT_EQ(255u, orig_pixels[2][3]);
  ASSERT_EQ(0u, orig_pixels[3][0]);
  ASSERT_EQ(0u, orig_pixels[3][1]);
  ASSERT_EQ(0u, orig_pixels[3][2]);
  ASSERT_EQ(255u, orig_pixels[3][3]);
  ASSERT_EQ(255u, orig_pixels[4][0]);
  ASSERT_EQ(255u, orig_pixels[4][1]);
  ASSERT_EQ(255u, orig_pixels[4][2]);
  ASSERT_EQ(255u, orig_pixels[4][3]);
  ASSERT_NEAR(97u, orig_pixels[5][0], 1);
  ASSERT_NEAR(97u, orig_pixels[5][1], 1);
  ASSERT_NEAR(97u, orig_pixels[5][2], 1);
  ASSERT_EQ(224u, orig_pixels[5][3]);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGray(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBGrayTo) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t gray_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 16, 1);
  ASSERT_NEAR(30u, gray_pixels[0][0], 1);
  ASSERT_NEAR(30u, gray_pixels[0][1], 1);
  ASSERT_NEAR(30u, gray_pixels[0][2], 1);
  ASSERT_NEAR(128u, gray_pixels[0][3], 1);
  ASSERT_NEAR(149u, gray_pixels[1][0], 1);
  ASSERT_NEAR(149u, gray_pixels[1][1], 1);
  ASSERT_NEAR(149u, gray_pixels[1][2], 1);
  ASSERT_NEAR(0u, gray_pixels[1][3], 1);
  ASSERT_NEAR(76u, gray_pixels[2][0], 1);
  ASSERT_NEAR(76u, gray_pixels[2][1], 1);
  ASSERT_NEAR(76u, gray_pixels[2][2], 1);
  ASSERT_NEAR(255u, gray_pixels[2][3], 1);
  ASSERT_NEAR(0u, gray_pixels[3][0], 1);
  ASSERT_NEAR(0u, gray_pixels[3][1], 1);
  ASSERT_NEAR(0u, gray_pixels[3][2], 1);
  ASSERT_NEAR(255u, gray_pixels[3][3], 1);
  ASSERT_NEAR(255u, gray_pixels[4][0], 1);
  ASSERT_NEAR(255u, gray_pixels[4][1], 1);
  ASSERT_NEAR(255u, gray_pixels[4][2], 1);
  ASSERT_NEAR(255u, gray_pixels[4][3], 1);
  ASSERT_NEAR(96u, gray_pixels[5][0], 1);
  ASSERT_NEAR(96u, gray_pixels[5][1], 1);
  ASSERT_NEAR(96u, gray_pixels[5][2], 1);
  ASSERT_NEAR(224u, gray_pixels[5][3], 1);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 1280, 1);
  }

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i;
    orig_pixels[i][2] = i;
    orig_pixels[i][3] = i;
  }
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 256, 1);
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(i, orig_pixels[i][0]);
    ASSERT_EQ(i, orig_pixels[i][1]);
    ASSERT_EQ(i, orig_pixels[i][2]);
    ASSERT_EQ(i, orig_pixels[i][3]);
  }

  // Test RGBX input with uninitialized alpha channel under MSan.
  SIMD_ALIGNED(uint8_t gray_pixels_c[37][4]);
  for (int i = 0; i < 37; ++i) {
    orig_pixels[i][0] = static_cast<uint8_t>(i * 7);
    orig_pixels[i][1] = static_cast<uint8_t>(i * 13);
    orig_pixels[i][2] = static_cast<uint8_t>(i * 23);
    orig_pixels[i][3] = 0u;
  }
  MaskCpuFlags(disable_cpu_flags_);
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels_c[0][0], 0, 37, 1);
  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < 37; ++i) {
    MSAN_POISON(&orig_pixels[i][3], 1);
  }
  MSAN_POISON(gray_pixels, sizeof(gray_pixels));
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 37, 1);
  for (int i = 0; i < 37; ++i) {
    MSAN_CHECK_INITIALIZED(&gray_pixels[i][0], 3);
    ASSERT_EQ(gray_pixels_c[i][0], gray_pixels[i][0]);
    ASSERT_EQ(gray_pixels_c[i][1], gray_pixels[i][1]);
    ASSERT_EQ(gray_pixels_c[i][2], gray_pixels[i][2]);
  }
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 37, 1);
  for (int i = 0; i < 37; ++i) {
    MSAN_CHECK_INITIALIZED(&orig_pixels[i][0], 3);
    ASSERT_EQ(gray_pixels_c[i][0], orig_pixels[i][0]);
    ASSERT_EQ(gray_pixels_c[i][1], orig_pixels[i][1]);
    ASSERT_EQ(gray_pixels_c[i][2], orig_pixels[i][2]);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBSepia) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  ASSERT_EQ(33u, orig_pixels[0][0]);
  ASSERT_EQ(43u, orig_pixels[0][1]);
  ASSERT_EQ(47u, orig_pixels[0][2]);
  ASSERT_EQ(128u, orig_pixels[0][3]);
  ASSERT_EQ(135u, orig_pixels[1][0]);
  ASSERT_EQ(175u, orig_pixels[1][1]);
  ASSERT_EQ(195u, orig_pixels[1][2]);
  ASSERT_EQ(0u, orig_pixels[1][3]);
  ASSERT_EQ(69u, orig_pixels[2][0]);
  ASSERT_EQ(89u, orig_pixels[2][1]);
  ASSERT_EQ(99u, orig_pixels[2][2]);
  ASSERT_EQ(255u, orig_pixels[2][3]);
  ASSERT_EQ(0u, orig_pixels[3][0]);
  ASSERT_EQ(0u, orig_pixels[3][1]);
  ASSERT_EQ(0u, orig_pixels[3][2]);
  ASSERT_EQ(255u, orig_pixels[3][3]);
  ASSERT_EQ(239u, orig_pixels[4][0]);
  ASSERT_EQ(255u, orig_pixels[4][1]);
  ASSERT_EQ(255u, orig_pixels[4][2]);
  ASSERT_EQ(255u, orig_pixels[4][3]);
  ASSERT_EQ(88u, orig_pixels[5][0]);
  ASSERT_EQ(114u, orig_pixels[5][1]);
  ASSERT_EQ(127u, orig_pixels[5][2]);
  ASSERT_EQ(224u, orig_pixels[5][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBColorMatrix) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8_t kRGBToSepia[]) = {
      17 / 2, 68 / 2, 35 / 2, 0, 22 / 2, 88 / 2, 45 / 2, 0,
      24 / 2, 98 / 2, 50 / 2, 0, 0,      0,      0,      64,  // Copy alpha.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                  &kRGBToSepia[0], 16, 1);
  ASSERT_EQ(31u, dst_pixels_opt[0][0]);
  ASSERT_EQ(43u, dst_pixels_opt[0][1]);
  ASSERT_EQ(47u, dst_pixels_opt[0][2]);
  ASSERT_EQ(128u, dst_pixels_opt[0][3]);
  ASSERT_EQ(135u, dst_pixels_opt[1][0]);
  ASSERT_EQ(175u, dst_pixels_opt[1][1]);
  ASSERT_EQ(195u, dst_pixels_opt[1][2]);
  ASSERT_EQ(0u, dst_pixels_opt[1][3]);
  ASSERT_EQ(67u, dst_pixels_opt[2][0]);
  ASSERT_EQ(87u, dst_pixels_opt[2][1]);
  ASSERT_EQ(99u, dst_pixels_opt[2][2]);
  ASSERT_EQ(255u, dst_pixels_opt[2][3]);
  ASSERT_EQ(87u, dst_pixels_opt[3][0]);
  ASSERT_EQ(112u, dst_pixels_opt[3][1]);
  ASSERT_EQ(127u, dst_pixels_opt[3][2]);
  ASSERT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  MaskCpuFlags(disable_cpu_flags_);
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                  &kRGBToSepia[0], 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                    &kRGBToSepia[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    ASSERT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    ASSERT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    ASSERT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

TEST_F(LibYUVPlanarTest, TestRGBColorMatrix) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8_t kRGBToSepia[]) = {
      17, 68, 35, 0, 22, 88, 45, 0,
      24, 98, 50, 0, 0,  0,  0,  0,  // Unused but makes matrix 16 bytes.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 16, 1);
  ASSERT_EQ(31u, orig_pixels[0][0]);
  ASSERT_EQ(43u, orig_pixels[0][1]);
  ASSERT_EQ(47u, orig_pixels[0][2]);
  ASSERT_EQ(128u, orig_pixels[0][3]);
  ASSERT_EQ(135u, orig_pixels[1][0]);
  ASSERT_EQ(175u, orig_pixels[1][1]);
  ASSERT_EQ(195u, orig_pixels[1][2]);
  ASSERT_EQ(0u, orig_pixels[1][3]);
  ASSERT_EQ(67u, orig_pixels[2][0]);
  ASSERT_EQ(87u, orig_pixels[2][1]);
  ASSERT_EQ(99u, orig_pixels[2][2]);
  ASSERT_EQ(255u, orig_pixels[2][3]);
  ASSERT_EQ(87u, orig_pixels[3][0]);
  ASSERT_EQ(112u, orig_pixels[3][1]);
  ASSERT_EQ(127u, orig_pixels[3][2]);
  ASSERT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBColorTable) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8_t kARGBTable[256 * 4] = {
      1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  ASSERT_EQ(1u, orig_pixels[0][0]);
  ASSERT_EQ(2u, orig_pixels[0][1]);
  ASSERT_EQ(3u, orig_pixels[0][2]);
  ASSERT_EQ(4u, orig_pixels[0][3]);
  ASSERT_EQ(5u, orig_pixels[1][0]);
  ASSERT_EQ(6u, orig_pixels[1][1]);
  ASSERT_EQ(7u, orig_pixels[1][2]);
  ASSERT_EQ(8u, orig_pixels[1][3]);
  ASSERT_EQ(9u, orig_pixels[2][0]);
  ASSERT_EQ(10u, orig_pixels[2][1]);
  ASSERT_EQ(11u, orig_pixels[2][2]);
  ASSERT_EQ(12u, orig_pixels[2][3]);
  ASSERT_EQ(1u, orig_pixels[3][0]);
  ASSERT_EQ(6u, orig_pixels[3][1]);
  ASSERT_EQ(11u, orig_pixels[3][2]);
  ASSERT_EQ(16u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

// Same as TestARGBColorTable except alpha does not change.
TEST_F(LibYUVPlanarTest, TestRGBColorTable) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8_t kARGBTable[256 * 4] = {
      1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  ASSERT_EQ(1u, orig_pixels[0][0]);
  ASSERT_EQ(2u, orig_pixels[0][1]);
  ASSERT_EQ(3u, orig_pixels[0][2]);
  ASSERT_EQ(0u, orig_pixels[0][3]);  // Alpha unchanged.
  ASSERT_EQ(5u, orig_pixels[1][0]);
  ASSERT_EQ(6u, orig_pixels[1][1]);
  ASSERT_EQ(7u, orig_pixels[1][2]);
  ASSERT_EQ(1u, orig_pixels[1][3]);  // Alpha unchanged.
  ASSERT_EQ(9u, orig_pixels[2][0]);
  ASSERT_EQ(10u, orig_pixels[2][1]);
  ASSERT_EQ(11u, orig_pixels[2][2]);
  ASSERT_EQ(2u, orig_pixels[2][3]);  // Alpha unchanged.
  ASSERT_EQ(1u, orig_pixels[3][0]);
  ASSERT_EQ(6u, orig_pixels[3][1]);
  ASSERT_EQ(11u, orig_pixels[3][2]);
  ASSERT_EQ(3u, orig_pixels[3][3]);  // Alpha unchanged.

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBQuantize) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  ARGBQuantize(&orig_pixels[0][0], 0, (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0,
               1280, 1);

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ((i / 8 * 8 + 8 / 2) & 255, orig_pixels[i][0]);
    ASSERT_EQ((i / 2 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][1]);
    ASSERT_EQ((i / 3 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][2]);
    ASSERT_EQ(i & 255, orig_pixels[i][3]);
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBQuantize(&orig_pixels[0][0], 0, (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0,
                 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, ARGBMirror_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_pixels_opt,
                        benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_ * 4);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_ * 4);
  MaskCpuFlags(disable_cpu_flags_);
  ARGBMirror(src_pixels, benchmark_width_ * 4, dst_pixels_c,
             benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBMirror(src_pixels, benchmark_width_ * 4, dst_pixels_opt,
               benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 4; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MirrorPlane_16_Opt) {
  align_buffer_page_end(src_pixels_8, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_opt_8,
                        benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_c_8,
                        benchmark_width_ * benchmark_height_ * 2);
  uint16_t* src_pixels = reinterpret_cast<uint16_t*>(src_pixels_8);
  uint16_t* dst_pixels_opt = reinterpret_cast<uint16_t*>(dst_pixels_opt_8);
  uint16_t* dst_pixels_c = reinterpret_cast<uint16_t*>(dst_pixels_c_8);

  MemRandomize(src_pixels_8, benchmark_width_ * benchmark_height_ * 2);
  MaskCpuFlags(disable_cpu_flags_);
  MirrorPlane_16(src_pixels, benchmark_width_, dst_pixels_c, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MirrorPlane_16(src_pixels, benchmark_width_, dst_pixels_opt,
                   benchmark_width_, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels_8);
  free_aligned_buffer_page_end(dst_pixels_opt_8);
  free_aligned_buffer_page_end(dst_pixels_c_8);
}

TEST_F(LibYUVPlanarTest, MirrorPlane_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_pixels_opt, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_);
  MaskCpuFlags(disable_cpu_flags_);
  MirrorPlane(src_pixels, benchmark_width_, dst_pixels_c, benchmark_width_,
              benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MirrorPlane(src_pixels, benchmark_width_, dst_pixels_opt, benchmark_width_,
                benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MirrorUVPlane_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_opt,
                        benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_ * 2);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_ * 2);
  MaskCpuFlags(disable_cpu_flags_);
  MirrorUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_c,
                benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MirrorUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_opt,
                  benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 2; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, TestShade) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t shade_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  orig_pixels[0][0] = 10u;
  orig_pixels[0][1] = 20u;
  orig_pixels[0][2] = 40u;
  orig_pixels[0][3] = 80u;
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 0u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 255u;
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 0u;
  orig_pixels[2][3] = 0u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 0u;
  // Do 8 pixels to allow opt version to be used.
  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80ffffff);
  ASSERT_EQ(10u, shade_pixels[0][0]);
  ASSERT_EQ(20u, shade_pixels[0][1]);
  ASSERT_EQ(40u, shade_pixels[0][2]);
  ASSERT_EQ(40u, shade_pixels[0][3]);
  ASSERT_EQ(0u, shade_pixels[1][0]);
  ASSERT_EQ(0u, shade_pixels[1][1]);
  ASSERT_EQ(0u, shade_pixels[1][2]);
  ASSERT_EQ(128u, shade_pixels[1][3]);
  ASSERT_EQ(0u, shade_pixels[2][0]);
  ASSERT_EQ(0u, shade_pixels[2][1]);
  ASSERT_EQ(0u, shade_pixels[2][2]);
  ASSERT_EQ(0u, shade_pixels[2][3]);
  ASSERT_EQ(0u, shade_pixels[3][0]);
  ASSERT_EQ(0u, shade_pixels[3][1]);
  ASSERT_EQ(0u, shade_pixels[3][2]);
  ASSERT_EQ(0u, shade_pixels[3][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80808080);
  ASSERT_EQ(5u, shade_pixels[0][0]);
  ASSERT_EQ(10u, shade_pixels[0][1]);
  ASSERT_EQ(20u, shade_pixels[0][2]);
  ASSERT_EQ(40u, shade_pixels[0][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x10204080);
  ASSERT_EQ(5u, shade_pixels[0][0]);
  ASSERT_EQ(5u, shade_pixels[0][1]);
  ASSERT_EQ(5u, shade_pixels[0][2]);
  ASSERT_EQ(5u, shade_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 1280, 1,
              0x80808080);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBInterpolate) {
  SIMD_ALIGNED(uint8_t orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8_t orig_pixels_1[1280][4]);
  SIMD_ALIGNED(uint8_t interpolate_pixels[1280][4]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0][0] = 16u;
  orig_pixels_0[0][1] = 32u;
  orig_pixels_0[0][2] = 64u;
  orig_pixels_0[0][3] = 128u;
  orig_pixels_0[1][0] = 0u;
  orig_pixels_0[1][1] = 0u;
  orig_pixels_0[1][2] = 0u;
  orig_pixels_0[1][3] = 255u;
  orig_pixels_0[2][0] = 0u;
  orig_pixels_0[2][1] = 0u;
  orig_pixels_0[2][2] = 0u;
  orig_pixels_0[2][3] = 0u;
  orig_pixels_0[3][0] = 0u;
  orig_pixels_0[3][1] = 0u;
  orig_pixels_0[3][2] = 0u;
  orig_pixels_0[3][3] = 0u;

  orig_pixels_1[0][0] = 0u;
  orig_pixels_1[0][1] = 0u;
  orig_pixels_1[0][2] = 0u;
  orig_pixels_1[0][3] = 0u;
  orig_pixels_1[1][0] = 0u;
  orig_pixels_1[1][1] = 0u;
  orig_pixels_1[1][2] = 0u;
  orig_pixels_1[1][3] = 0u;
  orig_pixels_1[2][0] = 0u;
  orig_pixels_1[2][1] = 0u;
  orig_pixels_1[2][2] = 0u;
  orig_pixels_1[2][3] = 0u;
  orig_pixels_1[3][0] = 255u;
  orig_pixels_1[3][1] = 255u;
  orig_pixels_1[3][2] = 255u;
  orig_pixels_1[3][3] = 255u;

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 128);
  ASSERT_EQ(8u, interpolate_pixels[0][0]);
  ASSERT_EQ(16u, interpolate_pixels[0][1]);
  ASSERT_EQ(32u, interpolate_pixels[0][2]);
  ASSERT_EQ(64u, interpolate_pixels[0][3]);
  ASSERT_EQ(0u, interpolate_pixels[1][0]);
  ASSERT_EQ(0u, interpolate_pixels[1][1]);
  ASSERT_EQ(0u, interpolate_pixels[1][2]);
  ASSERT_EQ(128u, interpolate_pixels[1][3]);
  ASSERT_EQ(0u, interpolate_pixels[2][0]);
  ASSERT_EQ(0u, interpolate_pixels[2][1]);
  ASSERT_EQ(0u, interpolate_pixels[2][2]);
  ASSERT_EQ(0u, interpolate_pixels[2][3]);
  ASSERT_EQ(128u, interpolate_pixels[3][0]);
  ASSERT_EQ(128u, interpolate_pixels[3][1]);
  ASSERT_EQ(128u, interpolate_pixels[3][2]);
  ASSERT_EQ(128u, interpolate_pixels[3][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 0);
  ASSERT_EQ(16u, interpolate_pixels[0][0]);
  ASSERT_EQ(32u, interpolate_pixels[0][1]);
  ASSERT_EQ(64u, interpolate_pixels[0][2]);
  ASSERT_EQ(128u, interpolate_pixels[0][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 192);

  ASSERT_EQ(4u, interpolate_pixels[0][0]);
  ASSERT_EQ(8u, interpolate_pixels[0][1]);
  ASSERT_EQ(16u, interpolate_pixels[0][2]);
  ASSERT_EQ(32u, interpolate_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                    &interpolate_pixels[0][0], 0, 1280, 1, 128);
  }
}

TEST_F(LibYUVPlanarTest, TestInterpolatePlane) {
  SIMD_ALIGNED(uint8_t orig_pixels_0[1280]);
  SIMD_ALIGNED(uint8_t orig_pixels_1[1280]);
  SIMD_ALIGNED(uint8_t interpolate_pixels[1280]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0] = 16u;
  orig_pixels_0[1] = 32u;
  orig_pixels_0[2] = 64u;
  orig_pixels_0[3] = 128u;
  orig_pixels_0[4] = 0u;
  orig_pixels_0[5] = 0u;
  orig_pixels_0[6] = 0u;
  orig_pixels_0[7] = 255u;
  orig_pixels_0[8] = 0u;
  orig_pixels_0[9] = 0u;
  orig_pixels_0[10] = 0u;
  orig_pixels_0[11] = 0u;
  orig_pixels_0[12] = 0u;
  orig_pixels_0[13] = 0u;
  orig_pixels_0[14] = 0u;
  orig_pixels_0[15] = 0u;

  orig_pixels_1[0] = 0u;
  orig_pixels_1[1] = 0u;
  orig_pixels_1[2] = 0u;
  orig_pixels_1[3] = 0u;
  orig_pixels_1[4] = 0u;
  orig_pixels_1[5] = 0u;
  orig_pixels_1[6] = 0u;
  orig_pixels_1[7] = 0u;
  orig_pixels_1[8] = 0u;
  orig_pixels_1[9] = 0u;
  orig_pixels_1[10] = 0u;
  orig_pixels_1[11] = 0u;
  orig_pixels_1[12] = 255u;
  orig_pixels_1[13] = 255u;
  orig_pixels_1[14] = 255u;
  orig_pixels_1[15] = 255u;

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 128);
  ASSERT_EQ(8u, interpolate_pixels[0]);
  ASSERT_EQ(16u, interpolate_pixels[1]);
  ASSERT_EQ(32u, interpolate_pixels[2]);
  ASSERT_EQ(64u, interpolate_pixels[3]);
  ASSERT_EQ(0u, interpolate_pixels[4]);
  ASSERT_EQ(0u, interpolate_pixels[5]);
  ASSERT_EQ(0u, interpolate_pixels[6]);
  ASSERT_EQ(128u, interpolate_pixels[7]);
  ASSERT_EQ(0u, interpolate_pixels[8]);
  ASSERT_EQ(0u, interpolate_pixels[9]);
  ASSERT_EQ(0u, interpolate_pixels[10]);
  ASSERT_EQ(0u, interpolate_pixels[11]);
  ASSERT_EQ(128u, interpolate_pixels[12]);
  ASSERT_EQ(128u, interpolate_pixels[13]);
  ASSERT_EQ(128u, interpolate_pixels[14]);
  ASSERT_EQ(128u, interpolate_pixels[15]);

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 0);
  ASSERT_EQ(16u, interpolate_pixels[0]);
  ASSERT_EQ(32u, interpolate_pixels[1]);
  ASSERT_EQ(64u, interpolate_pixels[2]);
  ASSERT_EQ(128u, interpolate_pixels[3]);

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 192);

  ASSERT_EQ(4u, interpolate_pixels[0]);
  ASSERT_EQ(8u, interpolate_pixels[1]);
  ASSERT_EQ(16u, interpolate_pixels[2]);
  ASSERT_EQ(32u, interpolate_pixels[3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                     &interpolate_pixels[0], 0, 1280, 1, 123);
  }
}

TEST_F(LibYUVPlanarTest, TestInterpolatePlane_16) {
  SIMD_ALIGNED(uint16_t orig_pixels_0[1280]);
  SIMD_ALIGNED(uint16_t orig_pixels_1[1280]);
  SIMD_ALIGNED(uint16_t interpolate_pixels[1280]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0] = 16u;
  orig_pixels_0[1] = 32u;
  orig_pixels_0[2] = 64u;
  orig_pixels_0[3] = 128u;
  orig_pixels_0[4] = 0u;
  orig_pixels_0[5] = 0u;
  orig_pixels_0[6] = 0u;
  orig_pixels_0[7] = 255u;
  orig_pixels_0[8] = 0u;
  orig_pixels_0[9] = 0u;
  orig_pixels_0[10] = 0u;
  orig_pixels_0[11] = 0u;
  orig_pixels_0[12] = 0u;
  orig_pixels_0[13] = 0u;
  orig_pixels_0[14] = 0u;
  orig_pixels_0[15] = 0u;

  orig_pixels_1[0] = 0u;
  orig_pixels_1[1] = 0u;
  orig_pixels_1[2] = 0u;
  orig_pixels_1[3] = 0u;
  orig_pixels_1[4] = 0u;
  orig_pixels_1[5] = 0u;
  orig_pixels_1[6] = 0u;
  orig_pixels_1[7] = 0u;
  orig_pixels_1[8] = 0u;
  orig_pixels_1[9] = 0u;
  orig_pixels_1[10] = 0u;
  orig_pixels_1[11] = 0u;
  orig_pixels_1[12] = 255u;
  orig_pixels_1[13] = 255u;
  orig_pixels_1[14] = 255u;
  orig_pixels_1[15] = 255u;

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                      &interpolate_pixels[0], 0, 16, 1, 128);
  ASSERT_EQ(8u, interpolate_pixels[0]);
  ASSERT_EQ(16u, interpolate_pixels[1]);
  ASSERT_EQ(32u, interpolate_pixels[2]);
  ASSERT_EQ(64u, interpolate_pixels[3]);
  ASSERT_EQ(0u, interpolate_pixels[4]);
  ASSERT_EQ(0u, interpolate_pixels[5]);
  ASSERT_EQ(0u, interpolate_pixels[6]);
  ASSERT_EQ(128u, interpolate_pixels[7]);
  ASSERT_EQ(0u, interpolate_pixels[8]);
  ASSERT_EQ(0u, interpolate_pixels[9]);
  ASSERT_EQ(0u, interpolate_pixels[10]);
  ASSERT_EQ(0u, interpolate_pixels[11]);
  ASSERT_EQ(128u, interpolate_pixels[12]);
  ASSERT_EQ(128u, interpolate_pixels[13]);
  ASSERT_EQ(128u, interpolate_pixels[14]);
  ASSERT_EQ(128u, interpolate_pixels[15]);

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                      &interpolate_pixels[0], 0, 16, 1, 0);
  ASSERT_EQ(16u, interpolate_pixels[0]);
  ASSERT_EQ(32u, interpolate_pixels[1]);
  ASSERT_EQ(64u, interpolate_pixels[2]);
  ASSERT_EQ(128u, interpolate_pixels[3]);

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                      &interpolate_pixels[0], 0, 16, 1, 192);

  ASSERT_EQ(4u, interpolate_pixels[0]);
  ASSERT_EQ(8u, interpolate_pixels[1]);
  ASSERT_EQ(16u, interpolate_pixels[2]);
  ASSERT_EQ(32u, interpolate_pixels[3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                        &interpolate_pixels[0], 0, 1280, 1, 123);
  }
}

#define TESTTERP(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, STRIDE_B, W1280, TERP, \
                 N, NEG, OFF)                                                 \
  TEST_F(LibYUVPlanarTest, ARGBInterpolate##TERP##N) {                        \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kStrideA =                                                      \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                \
    const int kStrideB =                                                      \
        (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;                \
    align_buffer_page_end(src_argb_a, kStrideA * kHeight + OFF);              \
    align_buffer_page_end(src_argb_b, kStrideA * kHeight + OFF);              \
    align_buffer_page_end(dst_argb_c, kStrideB * kHeight);                    \
    align_buffer_page_end(dst_argb_opt, kStrideB * kHeight);                  \
    for (int i = 0; i < kStrideA * kHeight; ++i) {                            \
      src_argb_a[i + OFF] = (fastrand() & 0xff);                              \
      src_argb_b[i + OFF] = (fastrand() & 0xff);                              \
    }                                                                         \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    ARGBInterpolate(src_argb_a + OFF, kStrideA, src_argb_b + OFF, kStrideA,   \
                    dst_argb_c, kStrideB, kWidth, NEG kHeight, TERP);         \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      ARGBInterpolate(src_argb_a + OFF, kStrideA, src_argb_b + OFF, kStrideA, \
                      dst_argb_opt, kStrideB, kWidth, NEG kHeight, TERP);     \
    }                                                                         \
    for (int i = 0; i < kStrideB * kHeight; ++i) {                            \
      ASSERT_EQ(dst_argb_c[i], dst_argb_opt[i]);                              \
    }                                                                         \
    free_aligned_buffer_page_end(src_argb_a);                                 \
    free_aligned_buffer_page_end(src_argb_b);                                 \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#define TESTINTERPOLATE(TERP)                                                \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_ + 1, TERP, _Any, +, 0)   \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Unaligned, +, 1) \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Invert, -, 0)    \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Opt, +, 0)

TESTINTERPOLATE(0)
TESTINTERPOLATE(64)
TESTINTERPOLATE(128)
TESTINTERPOLATE(192)
TESTINTERPOLATE(255)

static int TestBlend(int width,
                     int height,
                     int benchmark_iterations,
                     int disable_cpu_flags,
                     int benchmark_cpu_info,
                     int invert,
                     int off,
                     int attenuate) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  MemRandomize(src_argb_a, kStride * height + off);
  MemRandomize(src_argb_b, kStride * height + off);
  if (attenuate) {
    ARGBAttenuate(src_argb_a + off, kStride, src_argb_a + off, kStride, width,
                  height);
  }
  memset(dst_argb_c, 255, kStride * height);
  memset(dst_argb_opt, 255, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBBlend(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
            kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlend(src_argb_a + off, kStride, src_argb_b + off, kStride,
              dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Any) {
  int max_diff =
      TestBlend(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Unaligned) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 1);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Invert) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 1);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Unattenuated) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 0);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Opt) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
  ASSERT_LE(max_diff, 1);
}

static void TestBlendPlane(int width,
                           int height,
                           int benchmark_iterations,
                           int disable_cpu_flags,
                           int benchmark_cpu_info,
                           int invert,
                           int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 1;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(src_argb_alpha, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height + off);
  align_buffer_page_end(dst_argb_opt, kStride * height + off);
  memset(dst_argb_c, 255, kStride * height + off);
  memset(dst_argb_opt, 255, kStride * height + off);

  // Test source is maintained exactly if alpha is 255.
  for (int i = 0; i < width; ++i) {
    src_argb_a[i + off] = i & 255;
    src_argb_b[i + off] = 255 - (i & 255);
  }
  memset(src_argb_alpha + off, 255, width);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_opt + off, width, width, 1);
  for (int i = 0; i < width; ++i) {
    ASSERT_EQ(src_argb_a[i + off], dst_argb_opt[i + off]);
  }
  // Test destination is maintained exactly if alpha is 0.
  memset(src_argb_alpha + off, 0, width);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_opt + off, width, width, 1);
  for (int i = 0; i < width; ++i) {
    ASSERT_EQ(src_argb_b[i + off], dst_argb_opt[i + off]);
  }
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
    src_argb_alpha[i + off] = (fastrand() & 0xff);
  }

  MaskCpuFlags(disable_cpu_flags);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_c + off, width, width,
             invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
               src_argb_alpha + off, width, dst_argb_opt + off, width, width,
               invert * height);
  }
  for (int i = 0; i < kStride * height; ++i) {
    ASSERT_EQ(dst_argb_c[i + off], dst_argb_opt[i + off]);
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(src_argb_alpha);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
}

TEST_F(LibYUVPlanarTest, BlendPlane_Opt) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Unaligned) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Any) {
  TestBlendPlane(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Invert) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, -1, 1);
}

#define SUBSAMPLE(v, a) ((((v) + (a) - 1)) / (a))

static void TestI420Blend(int width,
                          int height,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info,
                          int invert,
                          int off) {
  width = ((width) > 0) ? (width) : 1;
  const int kStrideUV = SUBSAMPLE(width, 2);
  const int kSizeUV = kStrideUV * SUBSAMPLE(height, 2);
  align_buffer_page_end(src_y0, width * height + off);
  align_buffer_page_end(src_u0, kSizeUV + off);
  align_buffer_page_end(src_v0, kSizeUV + off);
  align_buffer_page_end(src_y1, width * height + off);
  align_buffer_page_end(src_u1, kSizeUV + off);
  align_buffer_page_end(src_v1, kSizeUV + off);
  align_buffer_page_end(src_a, width * height + off);
  align_buffer_page_end(dst_y_c, width * height + off);
  align_buffer_page_end(dst_u_c, kSizeUV + off);
  align_buffer_page_end(dst_v_c, kSizeUV + off);
  align_buffer_page_end(dst_y_opt, width * height + off);
  align_buffer_page_end(dst_u_opt, kSizeUV + off);
  align_buffer_page_end(dst_v_opt, kSizeUV + off);

  MemRandomize(src_y0, width * height + off);
  MemRandomize(src_u0, kSizeUV + off);
  MemRandomize(src_v0, kSizeUV + off);
  MemRandomize(src_y1, width * height + off);
  MemRandomize(src_u1, kSizeUV + off);
  MemRandomize(src_v1, kSizeUV + off);
  MemRandomize(src_a, width * height + off);
  memset(dst_y_c, 255, width * height + off);
  memset(dst_u_c, 255, kSizeUV + off);
  memset(dst_v_c, 255, kSizeUV + off);
  memset(dst_y_opt, 255, width * height + off);
  memset(dst_u_opt, 255, kSizeUV + off);
  memset(dst_v_opt, 255, kSizeUV + off);

  MaskCpuFlags(disable_cpu_flags);
  I420Blend(src_y0 + off, width, src_u0 + off, kStrideUV, src_v0 + off,
            kStrideUV, src_y1 + off, width, src_u1 + off, kStrideUV,
            src_v1 + off, kStrideUV, src_a + off, width, dst_y_c + off, width,
            dst_u_c + off, kStrideUV, dst_v_c + off, kStrideUV, width,
            invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    I420Blend(src_y0 + off, width, src_u0 + off, kStrideUV, src_v0 + off,
              kStrideUV, src_y1 + off, width, src_u1 + off, kStrideUV,
              src_v1 + off, kStrideUV, src_a + off, width, dst_y_opt + off,
              width, dst_u_opt + off, kStrideUV, dst_v_opt + off, kStrideUV,
              width, invert * height);
  }
  for (int i = 0; i < width * height; ++i) {
    ASSERT_EQ(dst_y_c[i + off], dst_y_opt[i + off]);
  }
  for (int i = 0; i < kSizeUV; ++i) {
    ASSERT_EQ(dst_u_c[i + off], dst_u_opt[i + off]);
    ASSERT_EQ(dst_v_c[i + off], dst_v_opt[i + off]);
  }
  free_aligned_buffer_page_end(src_y0);
  free_aligned_buffer_page_end(src_u0);
  free_aligned_buffer_page_end(src_v0);
  free_aligned_buffer_page_end(src_y1);
  free_aligned_buffer_page_end(src_u1);
  free_aligned_buffer_page_end(src_v1);
  free_aligned_buffer_page_end(src_a);
  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_opt);
}

TEST_F(LibYUVPlanarTest, I420Blend_Opt) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, I420Blend_Unaligned) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}

// TODO(fbarchard): DISABLED because _Any uses C.  Avoid C and re-enable.
TEST_F(LibYUVPlanarTest, DISABLED_I420Blend_Any) {
  TestI420Blend(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, I420Blend_Invert) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
}

#ifdef ENABLE_ROW_TESTS
TEST_F(LibYUVPlanarTest, TestAffine) {
  SIMD_ALIGNED(uint8_t orig_pixels_0[2][1280][4]);
  SIMD_ALIGNED(uint8_t interpolate_pixels_C[1280][4]);

  for (int r = 0; r < 2; ++r) {
    for (int i = 0; i < 1280; ++i) {
      for (int j = 0; j < 4; ++j) {
        orig_pixels_0[r][i][j] = i + r * 17;
      }
    }
  }

  float uv_step[4] = {0.f, 0.f, 0.75f, 1.f / 1280.f};
  const int stride = 1280 * 4;

  ARGBAffineRow_C(&orig_pixels_0[0][0][0], stride, &interpolate_pixels_C[0][0],
                  uv_step, 1280);
  ASSERT_EQ(0u, interpolate_pixels_C[0][0]);
  ASSERT_EQ(96u, interpolate_pixels_C[128][0]);
  ASSERT_EQ(191u, interpolate_pixels_C[255][3]);

#if defined(HAS_ARGBAFFINEROW_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  if (has_avx2) {
    SIMD_ALIGNED(uint8_t interpolate_pixels_Opt[1280][4]);
    ARGBAffineRow_AVX2(&orig_pixels_0[0][0][0], stride,
                       &interpolate_pixels_Opt[0][0], uv_step, 1280);
    ASSERT_EQ(0,
              memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1280 * 4));
    ARGBAffineRow_AVX2(&orig_pixels_0[0][0][0], stride,
                       &interpolate_pixels_Opt[0][0], uv_step, 1279);
    ASSERT_EQ(0,
              memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1279 * 4));

    for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
      ARGBAffineRow_AVX2(&orig_pixels_0[0][0][0], stride,
                         &interpolate_pixels_Opt[0][0], uv_step, 1280);
    }
  }
#endif
#if defined(HAS_ARGBAFFINEROW_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
  if (has_neon) {
    SIMD_ALIGNED(uint8_t interpolate_pixels_Opt[1280][4]);
    ARGBAffineRow_NEON(&orig_pixels_0[0][0][0], stride,
                       &interpolate_pixels_Opt[0][0], uv_step, 1280);
    ASSERT_EQ(0,
              memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1280 * 4));
    ARGBAffineRow_NEON(&orig_pixels_0[0][0][0], stride,
                       &interpolate_pixels_Opt[0][0], uv_step, 1279);
    ASSERT_EQ(0,
              memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1279 * 4));

    for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
      ARGBAffineRow_NEON(&orig_pixels_0[0][0][0], stride,
                         &interpolate_pixels_Opt[0][0], uv_step, 1280);
    }
  }
#endif
}
#endif  // ENABLE_ROW_TESTS

static int TestCopyPlane(int benchmark_width,
                         int benchmark_height,
                         int benchmark_iterations,
                         int disable_cpu_flags,
                         int benchmark_cpu_info,
                         int invert,
                         int off) {
  const int y_plane_size = benchmark_width * benchmark_height;
  align_buffer_page_end(orig_y, y_plane_size + off);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(orig_y + off, y_plane_size);
  memset(dst_c, 1, y_plane_size);
  memset(dst_opt, 2, y_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  for (int i = 0; i < benchmark_iterations; i++) {
    CopyPlane(orig_y + off, benchmark_width, dst_c, benchmark_width,
              benchmark_width, benchmark_height * invert);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; i++) {
    CopyPlane(orig_y + off, benchmark_width, dst_opt, benchmark_width,
              benchmark_width, benchmark_height * invert);
  }

  int max_diff = 0;
  for (int i = 0; i < y_plane_size; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_c[i]) - static_cast<int>(dst_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);

  return max_diff;
}

TEST_F(LibYUVPlanarTest, CopyPlane_Any) {
  int max_diff = TestCopyPlane(benchmark_width_ + 1, benchmark_height_,
                               benchmark_iterations_, disable_cpu_flags_,
                               benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_Unaligned) {
  int max_diff =
      TestCopyPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                    disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_Invert) {
  int max_diff =
      TestCopyPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                    disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_Opt) {
  int max_diff =
      TestCopyPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                    disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, TestCopyPlaneZero) {
  // Test to verify copying a rect with a zero height or width does
  // not touch destination memory.
  uint8_t src = 42;
  uint8_t dst = 0;

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  CopyPlane(&src, 0, &dst, 0, 0, 0);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 1, 0);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 0, 1);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  CopyPlane(&src, 0, &dst, 0, 0, 0);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 1, 0);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 0, 1);
  ASSERT_EQ(src, 42);
  ASSERT_EQ(dst, 0);
}

static int TestCopyPlane_16(int benchmark_width,
                            int benchmark_height,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info,
                            int invert,
                            int off) {
  const int y_plane_size = benchmark_width * benchmark_height;
  align_buffer_page_end(orig_y, (y_plane_size + off) * 2);
  align_buffer_page_end(dst_c, y_plane_size * 2);
  align_buffer_page_end(dst_opt, y_plane_size * 2);
  uint16_t* orig_y_16 = reinterpret_cast<uint16_t*>(orig_y) + off;
  uint16_t* dst_c_16 = reinterpret_cast<uint16_t*>(dst_c);
  uint16_t* dst_opt_16 = reinterpret_cast<uint16_t*>(dst_opt);

  MemRandomize(orig_y, (y_plane_size + off) * 2);
  memset(dst_c, 1, y_plane_size * 2);
  memset(dst_opt, 2, y_plane_size * 2);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  for (int i = 0; i < benchmark_iterations; i++) {
    CopyPlane_16(orig_y_16, benchmark_width, dst_c_16, benchmark_width,
                 benchmark_width, benchmark_height * invert);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; i++) {
    CopyPlane_16(orig_y_16, benchmark_width, dst_opt_16, benchmark_width,
                 benchmark_width, benchmark_height * invert);
  }

  int max_diff = 0;
  for (int i = 0; i < y_plane_size; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_c_16[i]) - static_cast<int>(dst_opt_16[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);

  return max_diff;
}

TEST_F(LibYUVPlanarTest, CopyPlane_16_Any) {
  int max_diff = TestCopyPlane_16(benchmark_width_ + 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_16_Unaligned) {
  int max_diff = TestCopyPlane_16(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_16_Invert) {
  int max_diff = TestCopyPlane_16(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, CopyPlane_16_Opt) {
  int max_diff = TestCopyPlane_16(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, TestDetilePlane) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int tile_width = (benchmark_width_ + 15) & ~15;
  int tile_height = (benchmark_height_ + 15) & ~15;
  int tile_plane_size = tile_width * tile_height;
  int y_plane_size = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(tile_y, tile_plane_size);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(tile_y, tile_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 0, y_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(tile_y, tile_width, dst_c, benchmark_width_, benchmark_width_,
                benchmark_height_, 16);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(tile_y, tile_width, dst_opt, benchmark_width_, benchmark_width_,
                benchmark_height_, 16);
  }

  for (i = 0; i < y_plane_size; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]);
  }

  free_aligned_buffer_page_end(tile_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
}

TEST_F(LibYUVPlanarTest, TestDetilePlane_16) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int tile_width = (benchmark_width_ + 15) & ~15;
  int tile_height = (benchmark_height_ + 15) & ~15;
  int tile_plane_size = tile_width * tile_height * 2;
  int y_plane_size = benchmark_width_ * benchmark_height_ * 2;
  align_buffer_page_end(tile_y, tile_plane_size);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(tile_y, tile_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 0, y_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane_16((const uint16_t*)tile_y, tile_width, (uint16_t*)dst_c,
                   benchmark_width_, benchmark_width_, benchmark_height_, 16);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane_16((const uint16_t*)tile_y, tile_width, (uint16_t*)dst_opt,
                   benchmark_width_, benchmark_width_, benchmark_height_, 16);
  }

  for (i = 0; i < y_plane_size; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]);
  }

  free_aligned_buffer_page_end(tile_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
}

// Compares DetileSplitUV to 2 step Detile + SplitUV
TEST_F(LibYUVPlanarTest, TestDetileSplitUVPlane_Correctness) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int uv_width = (benchmark_width_ + 1) & ~1;
  int tile_width = (uv_width + 15) & ~15;
  int tile_height = (benchmark_height_ + 15) & ~15;
  int tile_plane_size = tile_width * tile_height;
  int uv_plane_size = (uv_width / 2) * benchmark_height_;
  align_buffer_page_end(tile_uv, tile_plane_size);
  align_buffer_page_end(detiled_uv, tile_plane_size);
  align_buffer_page_end(dst_u_two_stage, uv_plane_size);
  align_buffer_page_end(dst_u_opt, uv_plane_size);
  align_buffer_page_end(dst_v_two_stage, uv_plane_size);
  align_buffer_page_end(dst_v_opt, uv_plane_size);

  MemRandomize(tile_uv, tile_plane_size);
  memset(detiled_uv, 0, tile_plane_size);
  memset(dst_u_two_stage, 0, uv_plane_size);
  memset(dst_u_opt, 0, uv_plane_size);
  memset(dst_v_two_stage, 0, uv_plane_size);
  memset(dst_v_opt, 0, uv_plane_size);

  DetileSplitUVPlane(tile_uv, tile_width, dst_u_opt, uv_width / 2, dst_v_opt,
                     uv_width / 2, uv_width, benchmark_height_, 16);

  // Benchmark 2 step conversion for comparison.
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(tile_uv, tile_width, detiled_uv, uv_width, uv_width,
                benchmark_height_, 16);
    SplitUVPlane(detiled_uv, uv_width, dst_u_two_stage, uv_width / 2,
                 dst_v_two_stage, uv_width / 2, uv_width / 2,
                 benchmark_height_);
  }

  for (i = 0; i < uv_plane_size; ++i) {
    ASSERT_EQ(dst_u_two_stage[i], dst_u_opt[i]);
    ASSERT_EQ(dst_v_two_stage[i], dst_v_opt[i]);
  }

  free_aligned_buffer_page_end(tile_uv);
  free_aligned_buffer_page_end(detiled_uv);
  free_aligned_buffer_page_end(dst_u_two_stage);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_two_stage);
  free_aligned_buffer_page_end(dst_v_opt);
}

TEST_F(LibYUVPlanarTest, TestDetileSplitUVPlane_Benchmark) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int uv_width = (benchmark_width_ + 1) & ~1;
  int tile_width = (uv_width + 15) & ~15;
  int tile_height = (benchmark_height_ + 15) & ~15;
  int tile_plane_size = tile_width * tile_height;
  int uv_plane_size = (uv_width / 2) * benchmark_height_;
  align_buffer_page_end(tile_uv, tile_plane_size);
  align_buffer_page_end(dst_u_c, uv_plane_size);
  align_buffer_page_end(dst_u_opt, uv_plane_size);
  align_buffer_page_end(dst_v_c, uv_plane_size);
  align_buffer_page_end(dst_v_opt, uv_plane_size);

  MemRandomize(tile_uv, tile_plane_size);
  memset(dst_u_c, 0, uv_plane_size);
  memset(dst_u_opt, 0, uv_plane_size);
  memset(dst_v_c, 0, uv_plane_size);
  memset(dst_v_opt, 0, uv_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);

  DetileSplitUVPlane(tile_uv, tile_width, dst_u_c, uv_width / 2, dst_v_c,
                     uv_width / 2, uv_width, benchmark_height_, 16);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);

  for (j = 0; j < benchmark_iterations_; j++) {
    DetileSplitUVPlane(tile_uv, tile_width, dst_u_opt, uv_width / 2, dst_v_opt,
                       uv_width / 2, uv_width, benchmark_height_, 16);
  }

  for (i = 0; i < uv_plane_size; ++i) {
    ASSERT_EQ(dst_u_c[i], dst_u_opt[i]);
    ASSERT_EQ(dst_v_c[i], dst_v_opt[i]);
  }

  free_aligned_buffer_page_end(tile_uv);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_v_opt);
}

static int TestMultiply(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(src_argb_b, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBMultiply(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
               kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBMultiply(src_argb_a + off, kStride, src_argb_b + off, kStride,
                 dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Any) {
  int max_diff = TestMultiply(benchmark_width_ + 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Unaligned) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Invert) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_LE(max_diff, 0);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Opt) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 0);
}

static int TestAdd(int width,
                   int height,
                   int benchmark_iterations,
                   int disable_cpu_flags,
                   int benchmark_cpu_info,
                   int invert,
                   int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(src_argb_b, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBAdd(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
          kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAdd(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_opt,
            kStride, width, invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Any) {
  int max_diff =
      TestAdd(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Unaligned) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Invert) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Opt) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 1);
}

static int TestSubtract(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(src_argb_b, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSubtract(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
               kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSubtract(src_argb_a + off, kStride, src_argb_b + off, kStride,
                 dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Any) {
  int max_diff = TestSubtract(benchmark_width_ + 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Unaligned) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Invert) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Opt) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_LE(max_diff, 1);
}

static int TestSobel(int width,
                     int height,
                     int benchmark_iterations,
                     int disable_cpu_flags,
                     int benchmark_cpu_info,
                     int invert,
                     int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  memset(src_argb_a, 0, (size_t)kStride * height + off);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobel(src_argb_a + off, kStride, dst_argb_c, kStride, width,
            invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobel(src_argb_a + off, kStride, dst_argb_opt, kStride, width,
              invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Any) {
  int max_diff =
      TestSobel(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Unaligned) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Invert) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Opt) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestSobelToPlane(int width,
                            int height,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info,
                            int invert,
                            int off) {
  if (width < 1) {
    width = 1;
  }
  const int kSrcBpp = 4;
  const int kDstBpp = 1;
  const int kSrcStride = (width * kSrcBpp + 15) & ~15;
  const int kDstStride = (width * kDstBpp + 15) & ~15;
  align_buffer_page_end(src_argb_a, kSrcStride * height + off);
  align_buffer_page_end(dst_argb_c, kDstStride * height);
  align_buffer_page_end(dst_argb_opt, kDstStride * height);
  memset(src_argb_a, 0, kSrcStride * height + off);
  for (int i = 0; i < kSrcStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kDstStride * height);
  memset(dst_argb_opt, 0, kDstStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobelToPlane(src_argb_a + off, kSrcStride, dst_argb_c, kDstStride, width,
                   invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelToPlane(src_argb_a + off, kSrcStride, dst_argb_opt, kDstStride,
                     width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kDstStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Any) {
  int max_diff = TestSobelToPlane(benchmark_width_ + 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Unaligned) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Invert) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Opt) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestSobelXY(int width,
                       int height,
                       int benchmark_iterations,
                       int disable_cpu_flags,
                       int benchmark_cpu_info,
                       int invert,
                       int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  memset(src_argb_a, 0, (size_t)kStride * height + off);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobelXY(src_argb_a + off, kStride, dst_argb_c, kStride, width,
              invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelXY(src_argb_a + off, kStride, dst_argb_opt, kStride, width,
                invert * height);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Any) {
  int max_diff = TestSobelXY(benchmark_width_ + 1, benchmark_height_,
                             benchmark_iterations_, disable_cpu_flags_,
                             benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Unaligned) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Invert) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Opt) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestBlur(int width,
                    int height,
                    int benchmark_iterations,
                    int disable_cpu_flags,
                    int benchmark_cpu_info,
                    int invert,
                    int off,
                    int radius) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  const int kCumsumHeight = radius * 2 + 2;
  align_buffer_page_end(src_argb_a, (size_t)kStride * height + off);
  align_buffer_page_end(dst_cumsum, (size_t)width * kCumsumHeight * 16);
  align_buffer_page_end(dst_argb_c, (size_t)kStride * height);
  align_buffer_page_end(dst_argb_opt, (size_t)kStride * height);
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, (size_t)kStride * height);
  memset(dst_argb_opt, 0, (size_t)kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBBlur(src_argb_a + off, kStride, dst_argb_c, kStride,
           reinterpret_cast<int32_t*>(dst_cumsum), width * 4, width,
           invert * height, radius);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlur(src_argb_a + off, kStride, dst_argb_opt, kStride,
             reinterpret_cast<int32_t*>(dst_cumsum), width * 4, width,
             invert * height, radius);
  }
  int max_diff = 0;
  for (size_t i = 0; i < (size_t)kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_cumsum);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
#define DISABLED_ARM(name) name
#else
#define DISABLED_ARM(name) DISABLED_##name
#endif

static const int kBlurSize = 55;
TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Any)) {
  int max_diff =
      TestBlur(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Unaligned)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 1, kBlurSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Invert)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, -1, 0, kBlurSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Opt)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  ASSERT_LE(max_diff, 1);
}

static const int kBlurSmallSize = 1;
TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Any)) {
  int max_diff =
      TestBlur((benchmark_width_ + 3) / 4 + 1, (benchmark_height_ + 3) / 4,
               benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_,
               +1, 0, kBlurSmallSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Unaligned)) {
  int max_diff =
      TestBlur((benchmark_width_ + 3) / 4, (benchmark_height_ + 3) / 4,
               benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_,
               +1, 1, kBlurSmallSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Invert)) {
  int max_diff =
      TestBlur((benchmark_width_ + 3) / 4, (benchmark_height_ + 3) / 4,
               benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_,
               -1, 0, kBlurSmallSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Opt)) {
  int max_diff =
      TestBlur((benchmark_width_ + 3) / 4, (benchmark_height_ + 3) / 4,
               benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_,
               +1, 0, kBlurSmallSize);
  ASSERT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, TestARGBPolynomial) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  SIMD_ALIGNED(static const float kWarmifyPolynomial[16]) = {
      0.94230f,  -3.03300f,    -2.92500f,    0.f,  // C0
      0.584500f, 1.112000f,    1.535000f,    1.f,  // C1 x
      0.001313f, -0.002503f,   -0.004496f,   0.f,  // C2 x * x
      0.0f,      0.000006965f, 0.000008781f, 0.f,  // C3 x * x * x
  };

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test white
  orig_pixels[3][0] = 255u;
  orig_pixels[3][1] = 255u;
  orig_pixels[3][2] = 255u;
  orig_pixels[3][3] = 255u;
  // Test color
  orig_pixels[4][0] = 16u;
  orig_pixels[4][1] = 64u;
  orig_pixels[4][2] = 192u;
  orig_pixels[4][3] = 224u;
  // Do 16 to test asm version.
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                 &kWarmifyPolynomial[0], 16, 1);
  ASSERT_EQ(235u, dst_pixels_opt[0][0]);
  ASSERT_EQ(0u, dst_pixels_opt[0][1]);
  ASSERT_EQ(0u, dst_pixels_opt[0][2]);
  ASSERT_EQ(128u, dst_pixels_opt[0][3]);
  ASSERT_EQ(0u, dst_pixels_opt[1][0]);
  ASSERT_EQ(233u, dst_pixels_opt[1][1]);
  ASSERT_EQ(0u, dst_pixels_opt[1][2]);
  ASSERT_EQ(0u, dst_pixels_opt[1][3]);
  ASSERT_EQ(0u, dst_pixels_opt[2][0]);
  ASSERT_EQ(0u, dst_pixels_opt[2][1]);
  ASSERT_EQ(241u, dst_pixels_opt[2][2]);
  ASSERT_EQ(255u, dst_pixels_opt[2][3]);
  ASSERT_EQ(235u, dst_pixels_opt[3][0]);
  ASSERT_EQ(233u, dst_pixels_opt[3][1]);
  ASSERT_EQ(241u, dst_pixels_opt[3][2]);
  ASSERT_EQ(255u, dst_pixels_opt[3][3]);
  ASSERT_EQ(10u, dst_pixels_opt[4][0]);
  ASSERT_EQ(59u, dst_pixels_opt[4][1]);
  ASSERT_EQ(188u, dst_pixels_opt[4][2]);
  ASSERT_EQ(224u, dst_pixels_opt[4][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                 &kWarmifyPolynomial[0], 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                   &kWarmifyPolynomial[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    ASSERT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    ASSERT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    ASSERT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

static int TestHalfFloatPlane(int benchmark_width,
                              int benchmark_height,
                              int benchmark_iterations,
                              int disable_cpu_flags,
                              int benchmark_cpu_info,
                              float scale,
                              int mask,
                              int invert,
                              int off) {
  if (benchmark_width <= 0 || benchmark_height <= 0) {
    return 0;
  }
  const size_t y_plane_size = static_cast<size_t>(benchmark_width) *
                              static_cast<size_t>(benchmark_height) * 2;
  align_buffer_page_end(orig_y, y_plane_size + off);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(orig_y + off, y_plane_size);
  memset(dst_c, 1, y_plane_size);
  memset(dst_opt, 2, y_plane_size);

  for (size_t i = 0; i < y_plane_size / 2; ++i) {
    reinterpret_cast<uint16_t*>(orig_y + off)[i] &= mask;
  }

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  for (int j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16_t*>(orig_y + off),
                   benchmark_width * 2, reinterpret_cast<uint16_t*>(dst_c),
                   benchmark_width * 2, scale, benchmark_width,
                   benchmark_height * invert);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (int j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16_t*>(orig_y + off),
                   benchmark_width * 2, reinterpret_cast<uint16_t*>(dst_opt),
                   benchmark_width * 2, scale, benchmark_width,
                   benchmark_height * invert);
  }

  int max_diff = 0;
  for (size_t i = 0; i < y_plane_size / 2; ++i) {
    int abs_diff =
        abs(static_cast<int>(reinterpret_cast<uint16_t*>(dst_c)[i]) -
            static_cast<int>(reinterpret_cast<uint16_t*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_One) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f, 65535, +1, 0);
  ASSERT_LE(diff, 1);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_Opt) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 65535.0f, 65535, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_12bit_Opt) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 4095.0f, 4095, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_10bit_Opt) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 1023.0f, 1023, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_9bit_Opt) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 511.0f, 511, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Any) {
  int diff = TestHalfFloatPlane(
      benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 4096.0f, 4095, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Unaligned) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 4096.0f, 4095, +1, 2);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Invert) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 4096.0f, 4095, -1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Opt) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 4096.0f, 4095, +1, 0);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_denormal) {
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 65535.0f, 65535, +1, 0);
  ASSERT_EQ(0, diff);
}

#if defined(__arm__) && !defined(__SOFTFP__)
static void EnableFlushDenormalToZero(void) {
  uint32_t cw;
  asm volatile(
      "vmrs   %0, fpscr                 \n"
      "orr    %0, %0, #0x1000000        \n"
      "vmsr   fpscr, %0                 \n"
      : "=r"(cw)::"memory", "cc");  // Clobber List
}

static void DisableFlushDenormalToZero(void) {
  uint32_t cw;
  asm volatile(
      "vmrs   %0, fpscr                 \n"
      "bic    %0, %0, #0x1000000        \n"
      "vmsr   fpscr, %0                 \n"
      : "=r"(cw)::"memory", "cc");  // Clobber List
}

// 5 bit exponent with bias of 15 will underflow to a denormal if scale causes
// exponent to be less than 0.  15 - log2(65536) = -1/  This shouldnt normally
// happen since scale is 1/(1<<bits) where bits is 9, 10 or 12.

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_flush_denormal) {
  // 32 bit arm rounding on denormal case is off by 1 compared to C.
  EnableFlushDenormalToZero();
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 65535.0f, 65535, +1, 0);
  DisableFlushDenormalToZero();
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_10bit_flush_denormal) {
  EnableFlushDenormalToZero();
  int diff = TestHalfFloatPlane(
      benchmark_width_, benchmark_height_, benchmark_iterations_,
      disable_cpu_flags_, benchmark_cpu_info_, 1.0f / 1023.0f, 1023, +1, 0);
  DisableFlushDenormalToZero();
  ASSERT_EQ(0, diff);
}
#endif  // defined(__arm__) && !defined(__SOFTFP__)

static float TestByteToFloat(int benchmark_width,
                             int benchmark_height,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info,
                             float scale) {
  int i, j;
  const int y_plane_size = benchmark_width * benchmark_height;

  align_buffer_page_end(orig_y, y_plane_size);
  align_buffer_page_end(dst_opt_mem, y_plane_size * 4);
  align_buffer_page_end(dst_c_mem, y_plane_size * 4);
  float* dst_opt = reinterpret_cast<float*>(dst_opt_mem);
  float* dst_c = reinterpret_cast<float*>(dst_c_mem);

  MemRandomize(orig_y, y_plane_size);
  memset(dst_c, 0, y_plane_size * 4);
  memset(dst_opt, 1, y_plane_size * 4);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  ByteToFloat(orig_y, dst_c, scale, y_plane_size);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (j = 0; j < benchmark_iterations; j++) {
    ByteToFloat(orig_y, dst_opt, scale, y_plane_size);
  }

  float max_diff = 0;
  for (i = 0; i < y_plane_size; ++i) {
    float abs_diff = fabs(dst_c[i] - dst_opt[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_opt_mem);
  free_aligned_buffer_page_end(dst_c_mem);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestByteToFloat) {
  float diff = TestByteToFloat(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, disable_cpu_flags_,
                               benchmark_cpu_info_, 1.0f);
  ASSERT_EQ(0.f, diff);
}

TEST_F(LibYUVPlanarTest, TestARGBLumaColorTable) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  align_buffer_page_end(lumacolortable, 32768);
  int v = 0;
  for (int i = 0; i < 32768; ++i) {
    lumacolortable[i] = v;
    v += 3;
  }
  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                     &lumacolortable[0], 16, 1);
  ASSERT_EQ(253u, dst_pixels_opt[0][0]);
  ASSERT_EQ(0u, dst_pixels_opt[0][1]);
  ASSERT_EQ(0u, dst_pixels_opt[0][2]);
  ASSERT_EQ(128u, dst_pixels_opt[0][3]);
  ASSERT_EQ(0u, dst_pixels_opt[1][0]);
  ASSERT_EQ(253u, dst_pixels_opt[1][1]);
  ASSERT_EQ(0u, dst_pixels_opt[1][2]);
  ASSERT_EQ(0u, dst_pixels_opt[1][3]);
  ASSERT_EQ(0u, dst_pixels_opt[2][0]);
  ASSERT_EQ(0u, dst_pixels_opt[2][1]);
  ASSERT_EQ(253u, dst_pixels_opt[2][2]);
  ASSERT_EQ(255u, dst_pixels_opt[2][3]);
  ASSERT_EQ(48u, dst_pixels_opt[3][0]);
  ASSERT_EQ(192u, dst_pixels_opt[3][1]);
  ASSERT_EQ(64u, dst_pixels_opt[3][2]);
  ASSERT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                     lumacolortable, 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                       lumacolortable, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    ASSERT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    ASSERT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    ASSERT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }

  free_aligned_buffer_page_end(lumacolortable);
}

static int TestARGBCopyAlpha(int width,
                             int height,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info,
                             int invert,
                             int off) {
  if (width < 1) {
    width = 1;
  }
  const int kStride = width * 4;
  const int kSize = kStride * height;
  align_buffer_page_end(orig_pixels, kSize + off);
  align_buffer_page_end(dst_pixels_opt, kSize);
  align_buffer_page_end(dst_pixels_c, kSize);

  MemRandomize(orig_pixels + off, kSize);
  MemRandomize(dst_pixels_c, kSize);
  // Copy RGB bytes only, leaving alpha bytes in dst_pixels_opt uninitialized.
  MSAN_POISON(dst_pixels_opt, kSize);
  for (int i = 0; i < width * height; ++i) {
    dst_pixels_opt[i * 4 + 0] = dst_pixels_c[i * 4 + 0];
    dst_pixels_opt[i * 4 + 1] = dst_pixels_c[i * 4 + 1];
    dst_pixels_opt[i * 4 + 2] = dst_pixels_c[i * 4 + 2];
  }

  MaskCpuFlags(disable_cpu_flags);
  ARGBCopyAlpha(orig_pixels + off, kStride, dst_pixels_c, kStride, width,
                invert * height);
  MaskCpuFlags(benchmark_cpu_info);

  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBCopyAlpha(orig_pixels + off, kStride, dst_pixels_opt, kStride, width,
                  invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kSize; ++i) {
    int abs_diff = abs(static_cast<int>(dst_pixels_c[i]) -
                       static_cast<int>(dst_pixels_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBCopyAlpha_Any) {
  int max_diff = TestARGBCopyAlpha(benchmark_width_ - 1, benchmark_height_,
                                   benchmark_iterations_, disable_cpu_flags_,
                                   benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyAlpha_Unaligned) {
  int max_diff = TestARGBCopyAlpha(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, disable_cpu_flags_,
                                   benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyAlpha_Invert) {
  int max_diff = TestARGBCopyAlpha(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, disable_cpu_flags_,
                                   benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyAlpha_Opt) {
  int max_diff = TestARGBCopyAlpha(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, disable_cpu_flags_,
                                   benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestARGBExtractAlpha(int width,
                                int height,
                                int benchmark_iterations,
                                int disable_cpu_flags,
                                int benchmark_cpu_info,
                                int invert,
                                int off) {
  if (width < 1) {
    width = 1;
  }
  const int kPixels = width * height;
  align_buffer_page_end(src_pixels, kPixels * 4 + off);
  align_buffer_page_end(dst_pixels_opt, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels);

  MemRandomize(src_pixels + off, kPixels * 4);
  // Leave dst_pixels_opt uninitialized to verify MSan unpoisons all written
  // alpha bytes (including masked tail stores).
  MSAN_POISON(dst_pixels_opt, kPixels);

  MaskCpuFlags(disable_cpu_flags);
  ARGBExtractAlpha(src_pixels + off, width * 4, dst_pixels_c, width, width,
                   invert * height);

  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBExtractAlpha(src_pixels + off, width * 4, dst_pixels_opt, width, width,
                     invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kPixels; ++i) {
    int abs_diff = abs(static_cast<int>(dst_pixels_c[i]) -
                       static_cast<int>(dst_pixels_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(src_pixels);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBExtractAlpha_Any) {
  int max_diff = TestARGBExtractAlpha(benchmark_width_ - 1, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBExtractAlpha_Unaligned) {
  int max_diff = TestARGBExtractAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBExtractAlpha_Invert) {
  int max_diff = TestARGBExtractAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBExtractAlpha_Opt) {
  int max_diff = TestARGBExtractAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestARGBCopyYToAlpha(int width,
                                int height,
                                int benchmark_iterations,
                                int disable_cpu_flags,
                                int benchmark_cpu_info,
                                int invert,
                                int off) {
  if (width < 1) {
    width = 1;
  }
  const int kPixels = width * height;
  align_buffer_page_end(orig_pixels, kPixels + off);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);
  align_buffer_page_end(dst_uninit_opt, kPixels * 4);

  MemRandomize(orig_pixels + off, kPixels);
  MemRandomize(dst_pixels_c, kPixels * 4);
  // Copy RGB bytes only, leaving alpha bytes in dst_pixels_opt uninitialized.
  MSAN_POISON(dst_pixels_opt, kPixels * 4);
  for (int i = 0; i < kPixels; ++i) {
    dst_pixels_opt[i * 4 + 0] = dst_pixels_c[i * 4 + 0];
    dst_pixels_opt[i * 4 + 1] = dst_pixels_c[i * 4 + 1];
    dst_pixels_opt[i * 4 + 2] = dst_pixels_c[i * 4 + 2];
  }
  // Also test with completely uninitialized destination buffer for MSan.
  MSAN_POISON(dst_uninit_opt, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags);
  ARGBCopyYToAlpha(orig_pixels + off, width, dst_pixels_c, width * 4, width,
                   invert * height);

  MaskCpuFlags(benchmark_cpu_info);
  ARGBCopyYToAlpha(orig_pixels + off, width, dst_uninit_opt, width * 4, width,
                   invert * height);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBCopyYToAlpha(orig_pixels + off, width, dst_pixels_opt, width * 4, width,
                     invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kPixels * 4; ++i) {
    int abs_diff = abs(static_cast<int>(dst_pixels_c[i]) -
                       static_cast<int>(dst_pixels_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  for (int i = 0; i < kPixels; ++i) {
    int abs_diff = abs(static_cast<int>(dst_pixels_c[i * 4 + 3]) -
                       static_cast<int>(dst_uninit_opt[i * 4 + 3]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_uninit_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBCopyYToAlpha_Any) {
  int max_diff = TestARGBCopyYToAlpha(benchmark_width_ - 1, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyYToAlpha_Unaligned) {
  int max_diff = TestARGBCopyYToAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyYToAlpha_Invert) {
  int max_diff = TestARGBCopyYToAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, -1, 0);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBCopyYToAlpha_Opt) {
  int max_diff = TestARGBCopyYToAlpha(benchmark_width_, benchmark_height_,
                                      benchmark_iterations_, disable_cpu_flags_,
                                      benchmark_cpu_info_, +1, 0);
  ASSERT_EQ(0, max_diff);
}

static int TestARGBRect(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off,
                        int bpp) {
  if (width < 1) {
    width = 1;
  }
  const int kStride = width * bpp;
  const int kSize = kStride * height;
  const uint32_t v32 = fastrand() & (bpp == 4 ? 0xffffffff : 0xff);

  align_buffer_page_end(dst_argb_c, kSize + off);
  align_buffer_page_end(dst_argb_opt, kSize + off);

  MemRandomize(dst_argb_c + off, kSize);
  memcpy(dst_argb_opt + off, dst_argb_c + off, kSize);

  MaskCpuFlags(disable_cpu_flags);
  if (bpp == 4) {
    ARGBRect(dst_argb_c + off, kStride, 0, 0, width, invert * height, v32);
  } else {
    SetPlane(dst_argb_c + off, kStride, width, invert * height, v32);
  }

  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    if (bpp == 4) {
      ARGBRect(dst_argb_opt + off, kStride, 0, 0, width, invert * height, v32);
    } else {
      SetPlane(dst_argb_opt + off, kStride, width, invert * height, v32);
    }
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i + off]) -
                       static_cast<int>(dst_argb_opt[i + off]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBRect_Any) {
  int max_diff = TestARGBRect(benchmark_width_ + 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0, 4);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Unaligned) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 4);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Invert) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 4);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Opt) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 4);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Any) {
  int max_diff = TestARGBRect(benchmark_width_ + 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Unaligned) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Invert) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Opt) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
  ASSERT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, MergeUVPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_u, kPixels);
  align_buffer_page_end(src_pixels_v, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels_u, kPixels);
  MemRandomize(src_pixels_v, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  MergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v, benchmark_width_,
               dst_pixels_c, benchmark_width_ * 2, benchmark_width_,
               benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v, benchmark_width_,
                 dst_pixels_opt, benchmark_width_ * 2, benchmark_width_,
                 benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

// 16 bit channel split and merge
TEST_F(LibYUVPlanarTest, MergeUVPlane_16_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_u, kPixels * 2);
  align_buffer_page_end(src_pixels_v, kPixels * 2);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2 * 2);
  MemRandomize(src_pixels_u, kPixels * 2);
  MemRandomize(src_pixels_v, kPixels * 2);
  MemRandomize(dst_pixels_opt, kPixels * 2 * 2);
  MemRandomize(dst_pixels_c, kPixels * 2 * 2);

  MaskCpuFlags(disable_cpu_flags_);
  MergeUVPlane_16((const uint16_t*)src_pixels_u, benchmark_width_,
                  (const uint16_t*)src_pixels_v, benchmark_width_,
                  (uint16_t*)dst_pixels_c, benchmark_width_ * 2,
                  benchmark_width_, benchmark_height_, 12);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeUVPlane_16((const uint16_t*)src_pixels_u, benchmark_width_,
                    (const uint16_t*)src_pixels_v, benchmark_width_,
                    (uint16_t*)dst_pixels_opt, benchmark_width_ * 2,
                    benchmark_width_, benchmark_height_, 12);
  }

  for (int i = 0; i < kPixels * 2 * 2; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitUVPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(dst_pixels_u_c, kPixels);
  align_buffer_page_end(dst_pixels_v_c, kPixels);
  align_buffer_page_end(dst_pixels_u_opt, kPixels);
  align_buffer_page_end(dst_pixels_v_opt, kPixels);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(dst_pixels_u_c, kPixels);
  MemRandomize(dst_pixels_v_c, kPixels);
  MemRandomize(dst_pixels_u_opt, kPixels);
  MemRandomize(dst_pixels_v_opt, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_u_c,
               benchmark_width_, dst_pixels_v_c, benchmark_width_,
               benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_u_opt,
                 benchmark_width_, dst_pixels_v_opt, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_u_c[i], dst_pixels_u_opt[i]);
    ASSERT_EQ(dst_pixels_v_c[i], dst_pixels_v_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_u_c);
  free_aligned_buffer_page_end(dst_pixels_v_c);
  free_aligned_buffer_page_end(dst_pixels_u_opt);
  free_aligned_buffer_page_end(dst_pixels_v_opt);
}

// 16 bit channel split
TEST_F(LibYUVPlanarTest, SplitUVPlane_16_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_u_c, kPixels * 2);
  align_buffer_page_end(dst_pixels_v_c, kPixels * 2);
  align_buffer_page_end(dst_pixels_u_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_v_opt, kPixels * 2);
  MemRandomize(src_pixels, kPixels * 2 * 2);
  MemRandomize(dst_pixels_u_c, kPixels * 2);
  MemRandomize(dst_pixels_v_c, kPixels * 2);
  MemRandomize(dst_pixels_u_opt, kPixels * 2);
  MemRandomize(dst_pixels_v_opt, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane_16((const uint16_t*)src_pixels, benchmark_width_ * 2,
                  (uint16_t*)dst_pixels_u_c, benchmark_width_,
                  (uint16_t*)dst_pixels_v_c, benchmark_width_, benchmark_width_,
                  benchmark_height_, 10);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitUVPlane_16((const uint16_t*)src_pixels, benchmark_width_ * 2,
                    (uint16_t*)dst_pixels_u_opt, benchmark_width_,
                    (uint16_t*)dst_pixels_v_opt, benchmark_width_,
                    benchmark_width_, benchmark_height_, 10);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_u_c[i], dst_pixels_u_opt[i]);
    ASSERT_EQ(dst_pixels_v_c[i], dst_pixels_v_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_u_c);
  free_aligned_buffer_page_end(dst_pixels_v_c);
  free_aligned_buffer_page_end(dst_pixels_u_opt);
  free_aligned_buffer_page_end(dst_pixels_v_opt);
}

TEST_F(LibYUVPlanarTest, SwapUVPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SwapUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_c,
              benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SwapUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_opt,
                benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MergeRGBPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 3);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels * 3);
  align_buffer_page_end(dst_pixels_opt, kPixels * 3);

  MemRandomize(src_pixels, kPixels * 3);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(dst_pixels_c, kPixels * 3);
  MemRandomize(dst_pixels_opt, kPixels * 3);

  MaskCpuFlags(disable_cpu_flags_);
  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_c_r,
                benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                tmp_pixels_c_b, benchmark_width_, benchmark_width_,
                benchmark_height_);
  MergeRGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                benchmark_width_, tmp_pixels_c_b, benchmark_width_,
                dst_pixels_c, benchmark_width_ * 3, benchmark_width_,
                benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_opt_r,
                benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                tmp_pixels_opt_b, benchmark_width_, benchmark_width_,
                benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeRGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                  benchmark_width_, tmp_pixels_opt_b, benchmark_width_,
                  dst_pixels_opt, benchmark_width_ * 3, benchmark_width_,
                  benchmark_height_);
  }

  for (int i = 0; i < kPixels * 3; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
}

TEST_F(LibYUVPlanarTest, SplitRGBPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 3);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels * 3);
  align_buffer_page_end(dst_pixels_opt, kPixels * 3);

  MemRandomize(src_pixels, kPixels * 3);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(dst_pixels_c, kPixels * 3);
  MemRandomize(dst_pixels_opt, kPixels * 3);

  MaskCpuFlags(disable_cpu_flags_);
  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_c_r,
                benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                tmp_pixels_c_b, benchmark_width_, benchmark_width_,
                benchmark_height_);
  MergeRGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                benchmark_width_, tmp_pixels_c_b, benchmark_width_,
                dst_pixels_c, benchmark_width_ * 3, benchmark_width_,
                benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_opt_r,
                  benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                  tmp_pixels_opt_b, benchmark_width_, benchmark_width_,
                  benchmark_height_);
  }
  MergeRGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                benchmark_width_, tmp_pixels_opt_b, benchmark_width_,
                dst_pixels_opt, benchmark_width_ * 3, benchmark_width_,
                benchmark_height_);

  for (int i = 0; i < kPixels * 3; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
}

// Call SplitRGBPlane() with a width that is not a multiple of 32. Verify there
// is no stack buffer overflow in SplitRGBRow_Any_AVX2().
TEST_F(LibYUVPlanarTest, SplitRGBPlaneOverflow) {
  int width = 1024 + 31;  // 1055
  const int height = 2;
  align_buffer_page_end(src_rgb, width * 3 * height);
  align_buffer_page_end(dst_r, width * height);
  align_buffer_page_end(dst_g, width * height);
  align_buffer_page_end(dst_b, width * height);
  memset(src_rgb, 0x5A, width * 3 * height);
  SplitRGBPlane(src_rgb, width * 3, dst_r, width, dst_g, width, dst_b, width,
                width, height);
  free_aligned_buffer_page_end(src_rgb);
  free_aligned_buffer_page_end(dst_r);
  free_aligned_buffer_page_end(dst_g);
  free_aligned_buffer_page_end(dst_b);
}

TEST_F(LibYUVPlanarTest, MergeARGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(tmp_pixels_c_a, kPixels);
  align_buffer_page_end(tmp_pixels_opt_a, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(tmp_pixels_c_a, kPixels);
  MemRandomize(tmp_pixels_opt_a, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_c_r,
                 benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                 tmp_pixels_c_b, benchmark_width_, tmp_pixels_c_a,
                 benchmark_width_, benchmark_width_, benchmark_height_);
  MergeARGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                 benchmark_width_, tmp_pixels_c_b, benchmark_width_,
                 tmp_pixels_c_a, benchmark_width_, dst_pixels_c,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_opt_r,
                 benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                 tmp_pixels_opt_b, benchmark_width_, tmp_pixels_opt_a,
                 benchmark_width_, benchmark_width_, benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeARGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                   benchmark_width_, tmp_pixels_opt_b, benchmark_width_,
                   tmp_pixels_opt_a, benchmark_width_, dst_pixels_opt,
                   benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels * 4; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(tmp_pixels_c_a);
  free_aligned_buffer_page_end(tmp_pixels_opt_a);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitARGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(tmp_pixels_c_a, kPixels);
  align_buffer_page_end(tmp_pixels_opt_a, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(tmp_pixels_c_a, kPixels);
  MemRandomize(tmp_pixels_opt_a, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_c_r,
                 benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                 tmp_pixels_c_b, benchmark_width_, tmp_pixels_c_a,
                 benchmark_width_, benchmark_width_, benchmark_height_);
  MergeARGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                 benchmark_width_, tmp_pixels_c_b, benchmark_width_,
                 tmp_pixels_c_a, benchmark_width_, dst_pixels_c,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_opt_r,
                   benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                   tmp_pixels_opt_b, benchmark_width_, tmp_pixels_opt_a,
                   benchmark_width_, benchmark_width_, benchmark_height_);
  }

  MergeARGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                 benchmark_width_, tmp_pixels_opt_b, benchmark_width_,
                 tmp_pixels_opt_a, benchmark_width_, dst_pixels_opt,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  for (int i = 0; i < kPixels * 4; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(tmp_pixels_c_a);
  free_aligned_buffer_page_end(tmp_pixels_opt_a);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
}

// Call SplitARGBPlane() with a width that is not a multiple of 16. Verify there
// is no stack buffer overflow in SplitXRGBRow_Any_AVX2().
TEST_F(LibYUVPlanarTest, SplitARGBPlaneOverflow) {
  int width = 1024 + 15;  // 1039
  int height = 2;
  align_buffer_page_end(src_argb, width * 4 * height);
  align_buffer_page_end(dst_r, width * height);
  align_buffer_page_end(dst_g, width * height);
  align_buffer_page_end(dst_b, width * height);
  memset(src_argb, 0x5A, width * 4 * height);
  // dst_a == NULL selects SplitXRGBRow
  SplitARGBPlane(src_argb, width * 4, dst_r, width, dst_g, width, dst_b, width,
                 NULL, 0, width, height);
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_r);
  free_aligned_buffer_page_end(dst_g);
  free_aligned_buffer_page_end(dst_b);
}

TEST_F(LibYUVPlanarTest, MergeXRGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(dst_pixels_c, kPixels * 4);
  MemRandomize(dst_pixels_opt, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_c_r,
                 benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                 tmp_pixels_c_b, benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);
  MergeARGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                 benchmark_width_, tmp_pixels_c_b, benchmark_width_, NULL, 0,
                 dst_pixels_c, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_opt_r,
                 benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                 tmp_pixels_opt_b, benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeARGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                   benchmark_width_, tmp_pixels_opt_b, benchmark_width_, NULL,
                   0, dst_pixels_opt, benchmark_width_ * 4, benchmark_width_,
                   benchmark_height_);
  }

  for (int i = 0; i < kPixels * 4; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
}

TEST_F(LibYUVPlanarTest, SplitXRGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_c_r, kPixels);
  align_buffer_page_end(tmp_pixels_opt_r, kPixels);
  align_buffer_page_end(tmp_pixels_c_g, kPixels);
  align_buffer_page_end(tmp_pixels_opt_g, kPixels);
  align_buffer_page_end(tmp_pixels_c_b, kPixels);
  align_buffer_page_end(tmp_pixels_opt_b, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_c_r, kPixels);
  MemRandomize(tmp_pixels_opt_r, kPixels);
  MemRandomize(tmp_pixels_c_g, kPixels);
  MemRandomize(tmp_pixels_opt_g, kPixels);
  MemRandomize(tmp_pixels_c_b, kPixels);
  MemRandomize(tmp_pixels_opt_b, kPixels);
  MemRandomize(dst_pixels_c, kPixels * 4);
  MemRandomize(dst_pixels_opt, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_c_r,
                 benchmark_width_, tmp_pixels_c_g, benchmark_width_,
                 tmp_pixels_c_b, benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);
  MergeARGBPlane(tmp_pixels_c_r, benchmark_width_, tmp_pixels_c_g,
                 benchmark_width_, tmp_pixels_c_b, benchmark_width_, NULL, 0,
                 dst_pixels_c, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_opt_r,
                   benchmark_width_, tmp_pixels_opt_g, benchmark_width_,
                   tmp_pixels_opt_b, benchmark_width_, NULL, 0,
                   benchmark_width_, benchmark_height_);
  }

  MergeARGBPlane(tmp_pixels_opt_r, benchmark_width_, tmp_pixels_opt_g,
                 benchmark_width_, tmp_pixels_opt_b, benchmark_width_, NULL, 0,
                 dst_pixels_opt, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  for (int i = 0; i < kPixels * 4; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_c_r);
  free_aligned_buffer_page_end(tmp_pixels_opt_r);
  free_aligned_buffer_page_end(tmp_pixels_c_g);
  free_aligned_buffer_page_end(tmp_pixels_opt_g);
  free_aligned_buffer_page_end(tmp_pixels_c_b);
  free_aligned_buffer_page_end(tmp_pixels_opt_b);
  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
}

// Merge 4 channels
#define TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_##DEPTH##N) {                        \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_a, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_a, kPixels * sizeof(STYPE) + OFF);              \
    memset(dst_memory_c, 0, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_memory_opt, 0, kPixels * 4 * sizeof(DTYPE));                 \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    STYPE* src_pixels_a = reinterpret_cast<STYPE*>(src_memory_a + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, src_pixels_a, kWidth, dst_pixels_c, kWidth * 4,     \
                kWidth, NEG benchmark_height_, DEPTH);                      \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, src_pixels_a, kWidth, dst_pixels_opt, kWidth * 4, \
                  kWidth, NEG benchmark_height_, DEPTH);                    \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(src_memory_a);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

// Merge 3 channel RGB into 4 channel XRGB with opaque alpha
#define TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)     \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_Opaque_##DEPTH##N) {                 \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    memset(dst_memory_c, 0, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_memory_opt, 0, kPixels * 4 * sizeof(DTYPE));                 \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, NULL, 0, dst_pixels_c, kWidth * 4, kWidth,          \
                NEG benchmark_height_, DEPTH);                              \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, NULL, 0, dst_pixels_opt, kWidth * 4, kWidth,      \
                  NEG benchmark_height_, DEPTH);                            \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

#define TESTQPLANARTOP(FUNC, STYPE, DTYPE, DEPTH)                              \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +, 0) \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +,  \
                  2)                                                           \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0)  \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)     \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +,   \
                   0)                                                          \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +, \
                   2)                                                          \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0) \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)

TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 10)
TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 12)
TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 16)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 10)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 12)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 16)

#define TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_##DEPTH##N) {                        \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    memset(dst_pixels_c, 1, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_pixels_opt, 2, kPixels * 4 * sizeof(DTYPE));                 \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, dst_pixels_c, kWidth * 4, kWidth,                   \
                NEG benchmark_height_, DEPTH);                              \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, dst_pixels_opt, kWidth * 4, kWidth,               \
                  NEG benchmark_height_, DEPTH);                            \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

#define TESTTPLANARTOP(FUNC, STYPE, DTYPE, DEPTH)                              \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +, 0) \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +,  \
                  2)                                                           \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0)  \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)

TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 10)
TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 12)
TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 16)

// TODO(fbarchard): improve test for platforms and cpu detect
#if defined(HAS_MERGEUVROW_16_AVX512BW) || defined(HAS_MERGEUVROW_16_AVX2)
TEST_F(LibYUVPlanarTest, MergeUVRow_16_Opt) {
  // Round count up to multiple of 16
  const int kPixels = (benchmark_width_ * benchmark_height_ + 15) & ~15;

  align_buffer_page_end(src_pixels_u, kPixels * 2);
  align_buffer_page_end(src_pixels_v, kPixels * 2);
  align_buffer_page_end(dst_pixels_uv_opt, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_uv_c, kPixels * 2 * 2);

  MemRandomize(src_pixels_u, kPixels * 2);
  MemRandomize(src_pixels_v, kPixels * 2);
  memset(dst_pixels_uv_opt, 0, kPixels * 2 * 2);
  memset(dst_pixels_uv_c, 1, kPixels * 2 * 2);

  MergeUVRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_u),
                  reinterpret_cast<const uint16_t*>(src_pixels_v),
                  reinterpret_cast<uint16_t*>(dst_pixels_uv_c), 16, kPixels);

#if defined(HAS_MERGEUVROW_16_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_MERGEUVROW_16_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_MERGEUVROW_16_AVX512BW)
    if (has_avx512) {
      MergeUVRow_16_AVX512BW(reinterpret_cast<const uint16_t*>(src_pixels_u),
                             reinterpret_cast<const uint16_t*>(src_pixels_v),
                             reinterpret_cast<uint16_t*>(dst_pixels_uv_opt), 16,
                             kPixels);
    } else
#endif
#if defined(HAS_MERGEUVROW_16_AVX2)
        if (has_avx2) {
      MergeUVRow_16_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_u),
                         reinterpret_cast<const uint16_t*>(src_pixels_v),
                         reinterpret_cast<uint16_t*>(dst_pixels_uv_opt), 16,
                         kPixels);
    } else
#endif
    {
      MergeUVRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_u),
                      reinterpret_cast<const uint16_t*>(src_pixels_v),
                      reinterpret_cast<uint16_t*>(dst_pixels_uv_opt), 16,
                      kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2 * 2; ++i) {
    ASSERT_EQ(dst_pixels_uv_opt[i], dst_pixels_uv_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}
#endif

#if defined(HAS_MULTIPLYROW_16_AVX512BW) ||                                 \
    defined(HAS_MULTIPLYROW_16_AVX2) || defined(HAS_MULTIPLYROW_16_NEON) || \
    defined(HAS_MULTIPLYROW_16_SME) || defined(HAS_MULTIPLYROW_16_RVV) ||   \
    defined(HAS_MULTIPLYROW_16_HVX)
TEST_F(LibYUVPlanarTest, MultiplyRow_16_Opt) {
  // Round count up to multiple of 64
  const int kPixels = (benchmark_width_ * benchmark_height_ + 63) & ~63;

  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                   reinterpret_cast<uint16_t*>(dst_pixels_y_c), 64, kPixels);

#if defined(HAS_MULTIPLYROW_16_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_MULTIPLYROW_16_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
#if defined(HAS_MULTIPLYROW_16_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
#endif
#if defined(HAS_MULTIPLYROW_16_SME)
  int has_sme = TestCpuFlag(kCpuHasSME);
#endif
#if defined(HAS_MULTIPLYROW_16_RVV)
  int has_rvv = TestCpuFlag(kCpuHasRVV);
#endif
#if defined(HAS_MULTIPLYROW_16_HVX)
  int has_hvx = TestCpuFlag(kCpuHasHVX);
#endif

  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_MULTIPLYROW_16_AVX512BW)
    if (has_avx512) {
      MultiplyRow_16_AVX512BW(reinterpret_cast<const uint16_t*>(src_pixels_y),
                              reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                              kPixels);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_AVX2)
        if (has_avx2) {
      MultiplyRow_16_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_y),
                          reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                          kPixels);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_SME)
        if (has_sme) {
      MultiplyRow_16_SME(reinterpret_cast<const uint16_t*>(src_pixels_y),
                         reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                         kPixels);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_NEON)
        if (has_neon) {
      MultiplyRow_16_NEON(reinterpret_cast<const uint16_t*>(src_pixels_y),
                          reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                          kPixels);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_RVV)
        if (has_rvv) {
      MultiplyRow_16_RVV(reinterpret_cast<const uint16_t*>(src_pixels_y),
                         reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                         kPixels);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_HVX)
        if (has_hvx) {
      MultiplyRow_16_HVX(reinterpret_cast<const uint16_t*>(src_pixels_y),
                         reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                         kPixels);
    } else
#endif
    {
      MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                       reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                       kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

#if defined(HAS_MULTIPLYROW_16_AVX512BW) || \
    defined(HAS_MULTIPLYROW_16_AVX2) || defined(HAS_MULTIPLYROW_16_NEON)
TEST_F(LibYUVPlanarTest, MultiplyRow_16_Any) {
  const int kMaxPixels = 256;
  align_buffer_page_end(src_pixels_y, kMaxPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kMaxPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kMaxPixels * 2);

  for (int i = 0; i < kMaxPixels; ++i) {
    reinterpret_cast<uint16_t*>(src_pixels_y)[i] = fastrand() & 1023;
  }

#if defined(HAS_MULTIPLYROW_16_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_MULTIPLYROW_16_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
#if defined(HAS_MULTIPLYROW_16_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
#endif

  for (int width = 1; width <= 129; ++width) {
    memset(dst_pixels_y_opt, 0, kMaxPixels * 2);
    memset(dst_pixels_y_c, 1, kMaxPixels * 2);

    MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                     reinterpret_cast<uint16_t*>(dst_pixels_y_c), 64, width);

#if defined(HAS_MULTIPLYROW_16_AVX512BW)
    if (has_avx512) {
      MultiplyRow_16_AVX512BW(reinterpret_cast<const uint16_t*>(src_pixels_y),
                              reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                              width);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_AVX2)
        if (has_avx2) {
      MultiplyRow_16_Any_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_y),
                              reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                              width);
    } else
#endif
#if defined(HAS_MULTIPLYROW_16_NEON)
        if (has_neon) {
      MultiplyRow_16_Any_NEON(reinterpret_cast<const uint16_t*>(src_pixels_y),
                              reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                              width);
    } else
#endif
    {
      MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                       reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                       width);
    }

    for (int i = 0; i < width * 2; ++i) {
      ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
    }
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_MULTIPLYROW_16_AVX512BW || HAS_MULTIPLYROW_16_AVX2 ||
        // HAS_MULTIPLYROW_16_NEON
#endif  // HAS_MULTIPLYROW_16_...

TEST_F(LibYUVPlanarTest, Convert16To8Plane) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  Convert16To8Plane(reinterpret_cast<const uint16_t*>(src_pixels_y),
                    benchmark_width_, dst_pixels_y_c, benchmark_width_, 16384,
                    benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    Convert16To8Plane(reinterpret_cast<const uint16_t*>(src_pixels_y),
                      benchmark_width_, dst_pixels_y_opt, benchmark_width_,
                      16384, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

TEST_F(LibYUVPlanarTest, Convert8To8Plane) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  Convert8To8Plane(src_pixels_y, benchmark_width_, dst_pixels_y_c,
                   benchmark_width_, 220, 16, benchmark_width_,
                   benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    Convert8To8Plane(src_pixels_y, benchmark_width_, dst_pixels_y_opt,
                     benchmark_width_, 220, 16, benchmark_width_,
                     benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

TEST_F(LibYUVPlanarTest, YUY2ToY) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  YUY2ToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_c, benchmark_width_,
          benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    YUY2ToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_opt,
            benchmark_width_, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

TEST_F(LibYUVPlanarTest, UYVYToY) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  UYVYToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_c, benchmark_width_,
          benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    UYVYToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_opt,
            benchmark_width_, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

#ifdef ENABLE_ROW_TESTS
// TODO(fbarchard): Improve test for more platforms.
#if defined(HAS_CONVERT16TO8ROW_AVX512BW) || defined(HAS_CONVERT16TO8ROW_AVX2)
TEST_F(LibYUVPlanarTest, Convert16To8Row_Opt) {
  // AVX512 does multiple of 64, so round count up
  const int kPixels = (benchmark_width_ * benchmark_height_ + 63) & ~63;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  // clamp source range to 10 bits.
  for (int i = 0; i < kPixels; ++i) {
    reinterpret_cast<uint16_t*>(src_pixels_y)[i] &= 1023;
  }

  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  Convert16To8Row_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                    dst_pixels_y_c, 16384, kPixels);

#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_CONVERT16TO8ROW_AVX512BW)
    if (has_avx512) {
      Convert16To8Row_AVX512BW(reinterpret_cast<const uint16_t*>(src_pixels_y),
                               dst_pixels_y_opt, 16384, kPixels);
    } else
#endif
#if defined(HAS_CONVERT16TO8ROW_AVX2)
        if (has_avx2) {
      Convert16To8Row_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_y),
                           dst_pixels_y_opt, 16384, kPixels);
    } else
#endif
    {
      Convert16To8Row_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                        dst_pixels_y_opt, 16384, kPixels);
    }
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_CONVERT16TO8ROW_AVX512BW || HAS_CONVERT16TO8ROW_AVX2

#if defined(HAS_HALFWIDTHROW_16TO8_AVX2) ||     \
    defined(HAS_HALFWIDTHROW_16TO8_SSSE3) ||    \
    defined(HAS_HALFWIDTHROW_16TO8_AVX512BW) || \
    defined(HAS_HALFWIDTHROW_16TO8_NEON)
TEST_F(LibYUVPlanarTest, HalfWidthRow_16To8_Opt) {
  const int kPixels = (benchmark_width_ * benchmark_height_ + 63) & ~63;
  align_buffer_page_end_16(src_pixels_uv, kPixels * 4);
  align_buffer_page_end(dst_pixels_uv_opt, kPixels);
  align_buffer_page_end(dst_pixels_uv_c, kPixels);

  for (int i = 0; i < kPixels * 4; ++i) {
    src_pixels_uv[i] = fastrand() & 1023;
  }

  memset(dst_pixels_uv_opt, 0, kPixels);
  memset(dst_pixels_uv_c, 1, kPixels);

  HalfWidthRow_16To8_C(src_pixels_uv, kPixels * 2, dst_pixels_uv_c, 16384,
                       kPixels);

#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
#endif

  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
    if (has_avx512) {
      HalfWidthRow_16To8_AVX512BW(src_pixels_uv, kPixels * 2, dst_pixels_uv_opt,
                                  16384, kPixels);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
        if (has_avx2) {
      HalfWidthRow_16To8_AVX2(src_pixels_uv, kPixels * 2, dst_pixels_uv_opt,
                              16384, kPixels);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
        if (has_ssse3) {
      HalfWidthRow_16To8_SSSE3(src_pixels_uv, kPixels * 2, dst_pixels_uv_opt,
                               16384, kPixels);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
        if (has_neon) {
      HalfWidthRow_16To8_NEON(src_pixels_uv, kPixels * 2, dst_pixels_uv_opt,
                              16384, kPixels);
    } else
#endif
    {
      HalfWidthRow_16To8_C(src_pixels_uv, kPixels * 2, dst_pixels_uv_opt, 16384,
                           kPixels);
    }
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_uv_opt[i], dst_pixels_uv_c[i]);
  }

  free_aligned_buffer_page_end_16(src_pixels_uv);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}

TEST_F(LibYUVPlanarTest, HalfWidthRow_16To8_Any) {
  const int kMaxPixels = 256;
  align_buffer_page_end_16(src_pixels_uv, kMaxPixels * 4);
  align_buffer_page_end(dst_pixels_uv_opt, kMaxPixels);
  align_buffer_page_end(dst_pixels_uv_c, kMaxPixels);

  for (int i = 0; i < kMaxPixels * 4; ++i) {
    src_pixels_uv[i] = fastrand() & 1023;
  }

#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
#endif

  for (int width = 1; width <= 129; ++width) {
    memset(dst_pixels_uv_opt, 0, kMaxPixels);
    memset(dst_pixels_uv_c, 1, kMaxPixels);

    HalfWidthRow_16To8_C(src_pixels_uv, kMaxPixels * 2, dst_pixels_uv_c, 16384,
                         width);

#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
    if (has_avx512) {
      HalfWidthRow_16To8_AVX512BW(src_pixels_uv, kMaxPixels * 2,
                                  dst_pixels_uv_opt, 16384, width);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
        if (has_avx2) {
      HalfWidthRow_16To8_Any_AVX2(src_pixels_uv, kMaxPixels * 2,
                                  dst_pixels_uv_opt, 16384, width);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
        if (has_ssse3) {
      HalfWidthRow_16To8_Any_SSSE3(src_pixels_uv, kMaxPixels * 2,
                                   dst_pixels_uv_opt, 16384, width);
    } else
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_NEON)
        if (has_neon) {
      HalfWidthRow_16To8_Any_NEON(src_pixels_uv, kMaxPixels * 2,
                                  dst_pixels_uv_opt, 16384, width);
    } else
#endif
    {
      HalfWidthRow_16To8_C(src_pixels_uv, kMaxPixels * 2, dst_pixels_uv_opt,
                           16384, width);
    }

    for (int i = 0; i < width; ++i) {
      ASSERT_EQ(dst_pixels_uv_opt[i], dst_pixels_uv_c[i]);
    }
  }

  free_aligned_buffer_page_end_16(src_pixels_uv);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}
#endif

#if defined(HAS_UYVYTOYROW_AVX512BW) || defined(HAS_UYVYTOYROW_AVX2) || \
    defined(HAS_UYVYTOYROW_NEON)
TEST_F(LibYUVPlanarTest, UYVYToYRow_Opt) {
  // AVX2/AVX512 does multiple of 32, so round count up
  const int kPixels = (benchmark_width_ * benchmark_height_ + 31) & ~31;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  UYVYToYRow_C(src_pixels_y, dst_pixels_y_c, kPixels);

#if defined(HAS_UYVYTOYROW_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
#if defined(HAS_UYVYTOYROW_AVX2)
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
#endif
#if defined(HAS_UYVYTOYROW_NEON)
  int has_neon = TestCpuFlag(kCpuHasNEON);
#endif
  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_UYVYTOYROW_AVX512BW)
    if (has_avx512) {
      UYVYToYRow_AVX512BW(src_pixels_y, dst_pixels_y_opt, kPixels);
    } else
#endif
#if defined(HAS_UYVYTOYROW_AVX2)
        if (has_avx2) {
      UYVYToYRow_AVX2(src_pixels_y, dst_pixels_y_opt, kPixels);
    } else
#endif
#if defined(HAS_UYVYTOYROW_NEON)
        if (has_neon) {
      UYVYToYRow_NEON(src_pixels_y, dst_pixels_y_opt, kPixels);
    } else
#endif
    {
      UYVYToYRow_C(src_pixels_y, dst_pixels_y_opt, kPixels);
    }
  }

  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_UYVYTOYROW_...

#if defined(HAS_RGB565TOYROW_AVX2) && defined(HAS_ARGB1555TOYROW_AVX2) && \
    defined(HAS_ARGB4444TOYROW_AVX2)
TEST_F(LibYUVPlanarTest, RGB16ToYUVRow_Opt) {
  if (!TestCpuFlag(kCpuHasAVX2)) {
    return;
  }
  const int kMaxWidth = 128;
  SIMD_ALIGNED(uint8_t src0[kMaxWidth * 2]);
  SIMD_ALIGNED(uint8_t src1[kMaxWidth * 2]);
  SIMD_ALIGNED(uint8_t dst_y_c[kMaxWidth]);
  SIMD_ALIGNED(uint8_t dst_y_opt[kMaxWidth]);
  SIMD_ALIGNED(uint8_t dst_u_c[kMaxWidth]);
  SIMD_ALIGNED(uint8_t dst_v_c[kMaxWidth]);
  SIMD_ALIGNED(uint8_t dst_u_opt[kMaxWidth]);
  SIMD_ALIGNED(uint8_t dst_v_opt[kMaxWidth]);
  MemRandomize(src0, sizeof(src0));
  MemRandomize(src1, sizeof(src1));

  for (int width : {1, 2, 7, 15, 16, 17, 31, 32, 33, 64, 127, 128}) {
    const int halfwidth = (width + 1) / 2;
    const int stride = kMaxWidth * 2;

    RGB565ToYRow_C(src0, dst_y_c, width);
    if (IS_ALIGNED(width, 32)) {
      RGB565ToYRow_AVX2(src0, dst_y_opt, width);
    } else {
      RGB565ToYRow_Any_AVX2(src0, dst_y_opt, width);
    }
    for (int i = 0; i < width; ++i) {
      ASSERT_NEAR(dst_y_c[i], dst_y_opt[i], 1);
    }

    ARGB1555ToYRow_C(src0, dst_y_c, width);
    if (IS_ALIGNED(width, 32)) {
      ARGB1555ToYRow_AVX2(src0, dst_y_opt, width);
    } else {
      ARGB1555ToYRow_Any_AVX2(src0, dst_y_opt, width);
    }
    for (int i = 0; i < width; ++i) {
      ASSERT_NEAR(dst_y_c[i], dst_y_opt[i], 1);
    }

    ARGB4444ToYRow_C(src0, dst_y_c, width);
    if (IS_ALIGNED(width, 32)) {
      ARGB4444ToYRow_AVX2(src0, dst_y_opt, width);
    } else {
      ARGB4444ToYRow_Any_AVX2(src0, dst_y_opt, width);
    }
    for (int i = 0; i < width; ++i) {
      ASSERT_NEAR(dst_y_c[i], dst_y_opt[i], 1);
    }

    SIMD_ALIGNED(uint8_t src2rows[kMaxWidth * 2 * 2]);
    memcpy(src2rows, src0, stride);
    memcpy(src2rows + stride, src1, stride);

    RGB565ToUVRow_C(src2rows, stride, dst_u_c, dst_v_c, width);
    if (IS_ALIGNED(width, 32)) {
      RGB565ToUVRow_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    } else {
      RGB565ToUVRow_Any_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    }
    for (int i = 0; i < halfwidth; ++i) {
      ASSERT_NEAR(dst_u_c[i], dst_u_opt[i], 2);
      ASSERT_NEAR(dst_v_c[i], dst_v_opt[i], 2);
    }

    ARGB1555ToUVRow_C(src2rows, stride, dst_u_c, dst_v_c, width);
    if (IS_ALIGNED(width, 32)) {
      ARGB1555ToUVRow_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    } else {
      ARGB1555ToUVRow_Any_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    }
    for (int i = 0; i < halfwidth; ++i) {
      ASSERT_NEAR(dst_u_c[i], dst_u_opt[i], 2);
      ASSERT_NEAR(dst_v_c[i], dst_v_opt[i], 2);
    }

    ARGB4444ToUVRow_C(src2rows, stride, dst_u_c, dst_v_c, width);
    if (IS_ALIGNED(width, 32)) {
      ARGB4444ToUVRow_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    } else {
      ARGB4444ToUVRow_Any_AVX2(src2rows, stride, dst_u_opt, dst_v_opt, width);
    }
    for (int i = 0; i < halfwidth; ++i) {
      ASSERT_NEAR(dst_u_c[i], dst_u_opt[i], 2);
      ASSERT_NEAR(dst_v_c[i], dst_v_opt[i], 2);
    }
  }
}
#endif

#endif  // ENABLE_ROW_TESTS

TEST_F(LibYUVPlanarTest, Convert8To16Plane) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  Convert8To16Plane(src_pixels_y, benchmark_width_,
                    reinterpret_cast<uint16_t*>(dst_pixels_y_c),
                    benchmark_width_, 10, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    Convert8To16Plane(src_pixels_y, benchmark_width_,
                      reinterpret_cast<uint16_t*>(dst_pixels_y_opt),
                      benchmark_width_, 1024, benchmark_width_,
                      benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

#ifdef ENABLE_ROW_TESTS
// TODO(fbarchard): Improve test for more platforms.
#if defined(HAS_CONVERT8TO16ROW_AVX512BW) || defined(HAS_CONVERT8TO16ROW_AVX2)
TEST_F(LibYUVPlanarTest, Convert8To16Row_Opt) {
  const int kPixels = (benchmark_width_ * benchmark_height_ + 63) & ~63;
  align_buffer_page_end(src_pixels_y, kPixels);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  Convert8To16Row_C(src_pixels_y, reinterpret_cast<uint16_t*>(dst_pixels_y_c),
                    10, kPixels);

#if defined(HAS_CONVERT8TO16ROW_AVX512BW)
  int has_avx512 = TestCpuFlag(kCpuHasAVX512BW);
#endif
  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  for (int i = 0; i < benchmark_iterations_; ++i) {
#if defined(HAS_CONVERT8TO16ROW_AVX512BW)
    if (has_avx512) {
      Convert8To16Row_AVX512BW(src_pixels_y,
                               reinterpret_cast<uint16_t*>(dst_pixels_y_opt),
                               10, kPixels);
    } else
#endif
        if (has_avx2) {
      Convert8To16Row_AVX2(src_pixels_y,
                           reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 10,
                           kPixels);
    } else {
      Convert8To16Row_C(src_pixels_y,
                        reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 10,
                        kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    ASSERT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_CONVERT8TO16ROW_AVX512BW || HAS_CONVERT8TO16ROW_AVX2

float TestScaleMaxSamples(int benchmark_width,
                          int benchmark_height,
                          int benchmark_iterations,
                          float scale,
                          bool opt) {
  int i, j;
  float max_c, max_opt = 0.f;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3 + 48);
  uint8_t* dst_c = orig_y + kPixels * 4 + 16;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2 + 32;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  max_c = ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      max_opt = ScaleMaxSamples_NEON(reinterpret_cast<float*>(orig_y),
                                     reinterpret_cast<float*>(dst_opt), scale,
                                     kPixels);
#elif defined(HAS_SCALEMAXSAMPLES_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        max_opt = ScaleMaxSamples_AVX2(reinterpret_cast<float*>(orig_y),
                                       reinterpret_cast<float*>(dst_opt), scale,
                                       kPixels);
      } else {
        max_opt = ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                                    reinterpret_cast<float*>(dst_opt), scale,
                                    kPixels);
      }
#else
      max_opt =
          ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      max_opt =
          ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float max_diff = FAbs(max_opt - max_c);
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleMaxSamples_C) {
  float diff = TestScaleMaxSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, false);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleMaxSamples_Opt) {
  float diff = TestScaleMaxSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, true);
  ASSERT_EQ(0, diff);
}

float TestScaleSumSamples(int benchmark_width,
                          int benchmark_height,
                          int benchmark_iterations,
                          float scale,
                          bool opt) {
  int i, j;
  float sum_c, sum_opt = 0.f;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  sum_c = ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      sum_opt = ScaleSumSamples_NEON(reinterpret_cast<float*>(orig_y),
                                     reinterpret_cast<float*>(dst_opt), scale,
                                     kPixels);
#elif defined(HAS_SCALESUMSAMPLES_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        sum_opt = ScaleSumSamples_AVX2(reinterpret_cast<float*>(orig_y),
                                       reinterpret_cast<float*>(dst_opt), scale,
                                       kPixels);
      } else {
        sum_opt = ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                                    reinterpret_cast<float*>(dst_opt), scale,
                                    kPixels);
      }
#else
      sum_opt =
          ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      sum_opt =
          ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float mse_opt = sum_opt / kPixels * 4;
  float mse_c = sum_c / kPixels * 4;
  float mse_error = FAbs(mse_opt - mse_c) / mse_c;

  // If the sum of a float is more than 4 million, small adds are round down on
  // float and produce different results with vectorized sum vs scalar sum.
  // Ignore the difference if the sum is large.
  float max_diff = 0.f;
  if (mse_error > 0.0001 && sum_c < 4000000) {  // allow .01% difference of mse
    max_diff = mse_error;
  }

  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleSumSamples_C) {
  float diff = TestScaleSumSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, false);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleSumSamples_Opt) {
  float diff = TestScaleSumSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, true);
  ASSERT_EQ(0, diff);
}

float TestScaleSamples(int benchmark_width,
                       int benchmark_height,
                       int benchmark_iterations,
                       float scale,
                       bool opt) {
  int i, j;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                 reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      ScaleSamples_NEON(reinterpret_cast<float*>(orig_y),
                        reinterpret_cast<float*>(dst_opt), scale, kPixels);
#elif defined(HAS_SCALESAMPLES_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        ScaleSamples_AVX2(reinterpret_cast<float*>(orig_y),
                          reinterpret_cast<float*>(dst_opt), scale, kPixels);
      } else {
        ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                       reinterpret_cast<float*>(dst_opt), scale, kPixels);
      }
#else
      ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                     reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                     reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float max_diff = 0.f;
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleSamples_C) {
  float diff = TestScaleSamples(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, 1.2f, false);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleSamples_Opt) {
  float diff = TestScaleSamples(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, 1.2f, true);
  ASSERT_EQ(0, diff);
}

float TestCopySamples(int benchmark_width,
                      int benchmark_height,
                      int benchmark_iterations,
                      bool opt) {
  int i, j;
  // NEON does multiple of 16 floats, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 15) & ~15;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  memcpy(reinterpret_cast<void*>(dst_c), reinterpret_cast<void*>(orig_y),
         kPixels * 4);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_COPYROW_NEON
      CopyRow_NEON(orig_y, dst_opt, kPixels * 4);
#else
      CopyRow_C(orig_y, dst_opt, kPixels * 4);
#endif
    } else {
      CopyRow_C(orig_y, dst_opt, kPixels * 4);
    }
  }

  float max_diff = 0.f;
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestCopySamples_C) {
  float diff = TestCopySamples(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, false);
  ASSERT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestCopySamples_Opt) {
  float diff = TestCopySamples(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, true);
  ASSERT_EQ(0, diff);
}

extern "C" void GaussRow_NEON(const uint32_t* src, uint16_t* dst, int width);
extern "C" void GaussRow_C(const uint32_t* src, uint16_t* dst, int width);

TEST_F(LibYUVPlanarTest, TestGaussRow_Opt) {
  SIMD_ALIGNED(uint32_t orig_pixels[1280 + 8]);
  SIMD_ALIGNED(uint16_t dst_pixels_c[1280]);
  SIMD_ALIGNED(uint16_t dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 + 8; ++i) {
    orig_pixels[i] = i * 256;
  }
  GaussRow_C(&orig_pixels[0], &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if defined(HAS_GAUSSROW_AVX2)
    int has_avx2 = TestCpuFlag(kCpuHasAVX2);
    if (has_avx2) {
      GaussRow_AVX2(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#elif !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__aarch64__) || defined(__ARM_NEON__) || defined(LIBYUV_NEON))
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussRow_NEON(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#else
    GaussRow_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  ASSERT_EQ(dst_pixels_c[0],
            static_cast<uint16_t>(0 * 1 + 1 * 4 + 2 * 6 + 3 * 4 + 4 * 1));
  ASSERT_EQ(dst_pixels_c[639], static_cast<uint16_t>(10256));
}

extern "C" void GaussCol_NEON(const uint16_t* src0,
                              const uint16_t* src1,
                              const uint16_t* src2,
                              const uint16_t* src3,
                              const uint16_t* src4,
                              uint32_t* dst,
                              int width);

extern "C" void GaussCol_C(const uint16_t* src0,
                           const uint16_t* src1,
                           const uint16_t* src2,
                           const uint16_t* src3,
                           const uint16_t* src4,
                           uint32_t* dst,
                           int width);

TEST_F(LibYUVPlanarTest, TestGaussCol_Opt) {
  SIMD_ALIGNED(uint16_t orig_pixels[1280 * 5]);
  SIMD_ALIGNED(uint32_t dst_pixels_c[1280]);
  SIMD_ALIGNED(uint32_t dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 * 5; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
             &orig_pixels[1280 * 3], &orig_pixels[1280 * 4], &dst_pixels_c[0],
             1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if defined(HAS_GAUSSCOL_AVX2)
    int has_avx2 = TestCpuFlag(kCpuHasAVX2);
    if (has_avx2) {
      GaussCol_AVX2(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                    &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                    &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                 &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                 &dst_pixels_opt[0], 1280);
    }
#elif !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__aarch64__) || defined(__ARM_NEON__) || defined(LIBYUV_NEON))
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussCol_NEON(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                    &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                    &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                 &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                 &dst_pixels_opt[0], 1280);
    }
#else
    GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
               &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
               &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
}

TEST_F(LibYUVPlanarTest, TestGaussRow_F32_Opt) {
  SIMD_ALIGNED(float orig_pixels[1280 + 4]);
  SIMD_ALIGNED(float dst_pixels_c[1280]);
  SIMD_ALIGNED(float dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 + 4; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussRow_F32_C(&orig_pixels[0], &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if defined(HAS_GAUSSROW_F32_AVX2)
    int has_avx2 = TestCpuFlag(kCpuHasAVX2);
    if (has_avx2) {
      GaussRow_F32_AVX2(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_F32_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#elif !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussRow_F32_NEON(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_F32_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#else
    GaussRow_F32_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
}

TEST_F(LibYUVPlanarTest, TestGaussCol_F32_Opt) {
  SIMD_ALIGNED(float dst_pixels_c[1280]);
  SIMD_ALIGNED(float dst_pixels_opt[1280]);
  align_buffer_page_end(orig_pixels_buf, 1280 * 5 * 4);  // 5 rows
  float* orig_pixels = reinterpret_cast<float*>(orig_pixels_buf);

  memset(orig_pixels, 0, 1280 * 5 * 4);
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 * 5; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                 &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                 &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if defined(HAS_GAUSSCOL_F32_AVX2)
    int has_avx2 = TestCpuFlag(kCpuHasAVX2);
    if (has_avx2) {
      GaussCol_F32_AVX2(&orig_pixels[0], &orig_pixels[1280],
                        &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                        &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280],
                     &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                     &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    }
#elif !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussCol_F32_NEON(&orig_pixels[0], &orig_pixels[1280],
                        &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                        &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280],
                     &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                     &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    }
#else
    GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                   &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                   &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    ASSERT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(orig_pixels_buf);
}

TEST_F(LibYUVPlanarTest, SwapUVRow) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  void (*SwapUVRow)(const uint8_t* src_uv, uint8_t* dst_vu, int width) =
      SwapUVRow_C;

  align_buffer_page_end(src_pixels_vu, kPixels * 2);
  align_buffer_page_end(dst_pixels_uv, kPixels * 2);
  MemRandomize(src_pixels_vu, kPixels * 2);
  memset(dst_pixels_uv, 1, kPixels * 2);

#if defined(HAS_SWAPUVROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    SwapUVRow = SwapUVRow_Any_SSSE3;
    if (IS_ALIGNED(kPixels, 16)) {
      SwapUVRow = SwapUVRow_SSSE3;
    }
  }
#endif
#if defined(HAS_SWAPUVROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    SwapUVRow = SwapUVRow_Any_AVX2;
    if (IS_ALIGNED(kPixels, 32)) {
      SwapUVRow = SwapUVRow_AVX2;
    }
  }
#endif
#if defined(HAS_SWAPUVROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SwapUVRow = SwapUVRow_Any_NEON;
    if (IS_ALIGNED(kPixels, 16)) {
      SwapUVRow = SwapUVRow_NEON;
    }
  }
#endif
#if defined(HAS_SWAPUVROW_RVV)
  if (TestCpuFlag(kCpuHasRVV)) {
    SwapUVRow = SwapUVRow_RVV;
  }
#endif
#if defined(HAS_SWAPUVROW_HVX)
  if (TestCpuFlag(kCpuHasHVX)) {
    SwapUVRow = SwapUVRow_HVX;
  }
#endif

  for (int j = 0; j < benchmark_iterations_; j++) {
    SwapUVRow(src_pixels_vu, dst_pixels_uv, kPixels);
  }
  for (int i = 0; i < kPixels; ++i) {
    ASSERT_EQ(dst_pixels_uv[i * 2 + 0], src_pixels_vu[i * 2 + 1]);
    ASSERT_EQ(dst_pixels_uv[i * 2 + 1], src_pixels_vu[i * 2 + 0]);
  }

  free_aligned_buffer_page_end(src_pixels_vu);
  free_aligned_buffer_page_end(dst_pixels_uv);
}

TEST_F(LibYUVPlanarTest, HalfRow_Opt) {
  const int kMaxPixels = 160;
  align_buffer_page_end(src_pixels, kMaxPixels * 2);
  align_buffer_page_end(dst_pixels_opt, kMaxPixels);
  align_buffer_page_end(dst_pixels_c, kMaxPixels);
  MemRandomize(src_pixels, kMaxPixels * 2);

  for (ptrdiff_t stride : {ptrdiff_t{0}, ptrdiff_t{kMaxPixels}}) {
    for (int width = 1; width <= 129; ++width) {
      memset(dst_pixels_opt, 0, kMaxPixels);
      memset(dst_pixels_c, 1, kMaxPixels);
      HalfRow_C(src_pixels, stride, dst_pixels_c, width);
#if defined(HAS_HALFROW_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        if (IS_ALIGNED(width, 32)) {
          HalfRow_AVX2(src_pixels, stride, dst_pixels_opt, width);
        } else {
          HalfRow_Any_AVX2(src_pixels, stride, dst_pixels_opt, width);
        }
      } else
#endif
      {
        HalfRow_C(src_pixels, stride, dst_pixels_opt, width);
      }
      ASSERT_EQ(0, memcmp(dst_pixels_c, dst_pixels_opt, width))
          << "stride=" << stride << " width=" << width;
    }
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, HalfRow_16_Opt) {
  const int kMaxPixels = 160;
  align_buffer_page_end_16(src_pixels, kMaxPixels * 2);
  align_buffer_page_end_16(dst_pixels_opt, kMaxPixels);
  align_buffer_page_end_16(dst_pixels_c, kMaxPixels);
  for (int i = 0; i < kMaxPixels * 2; ++i) {
    src_pixels[i] = static_cast<uint16_t>(fastrand());
  }

  for (ptrdiff_t stride : {ptrdiff_t{0}, ptrdiff_t{kMaxPixels}}) {
    for (int width = 1; width <= 129; ++width) {
      memset(dst_pixels_opt, 0, kMaxPixels * 2);
      memset(dst_pixels_c, 1, kMaxPixels * 2);
      HalfRow_16_C(src_pixels, stride, dst_pixels_c, width);
#if defined(HAS_HALFROW_16_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        if (IS_ALIGNED(width, 16)) {
          HalfRow_16_AVX2(src_pixels, stride, dst_pixels_opt, width);
        } else {
          HalfRow_16_Any_AVX2(src_pixels, stride, dst_pixels_opt, width);
        }
      } else
#endif
      {
        HalfRow_16_C(src_pixels, stride, dst_pixels_opt, width);
      }
      ASSERT_EQ(0, memcmp(dst_pixels_c, dst_pixels_opt, width * 2))
          << "stride=" << stride << " width=" << width;
    }
  }

  free_aligned_buffer_page_end_16(src_pixels);
  free_aligned_buffer_page_end_16(dst_pixels_opt);
  free_aligned_buffer_page_end_16(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, HalfWidthRow_16To8_Odd_Opt) {
  const int kMaxPixels = 160;
  align_buffer_page_end_16(src_pixels, kMaxPixels * 4);
  align_buffer_page_end(dst_pixels_opt, kMaxPixels);
  align_buffer_page_end(dst_pixels_c, kMaxPixels);
  for (int i = 0; i < kMaxPixels * 4; ++i) {
    src_pixels[i] = fastrand() & 1023;
  }

  for (ptrdiff_t stride : {ptrdiff_t{0}, ptrdiff_t{kMaxPixels * 2}}) {
    for (int width = 1; width <= 129; ++width) {
      memset(dst_pixels_opt, 0, kMaxPixels);
      memset(dst_pixels_c, 1, kMaxPixels);
      HalfWidthRow_16To8_Odd_C(src_pixels, stride, dst_pixels_c, 16384, width);
#if defined(HAS_HALFWIDTHROW_16TO8_ODD_AVX2)
      if (TestCpuFlag(kCpuHasAVX2)) {
        HalfWidthRow_16To8_Odd_AVX2(src_pixels, stride, dst_pixels_opt, 16384,
                                    width);
      } else
#endif
      {
        HalfWidthRow_16To8_Odd_C(src_pixels, stride, dst_pixels_opt, 16384,
                                 width);
      }
      ASSERT_EQ(0, memcmp(dst_pixels_c, dst_pixels_opt, width))
          << "stride=" << stride << " width=" << width;
    }
  }

  free_aligned_buffer_page_end_16(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, ARGBChannelReorderRow_C_Test) {
  const int kPixels = 65;
  align_buffer_page_end(src_argb, kPixels * 4);
  align_buffer_page_end(dst_row, kPixels * 4);
  align_buffer_page_end(dst_api, kPixels * 4);
  MemRandomize(src_argb, kPixels * 4);

  ARGBToABGRRow_C(src_argb, dst_row, kPixels);
  ARGBToABGR(src_argb, kPixels * 4, dst_api, kPixels * 4, kPixels, 1);
  ASSERT_EQ(0, memcmp(dst_row, dst_api, kPixels * 4));

  ARGBToBGRARow_C(src_argb, dst_row, kPixels);
  ARGBToBGRA(src_argb, kPixels * 4, dst_api, kPixels * 4, kPixels, 1);
  ASSERT_EQ(0, memcmp(dst_row, dst_api, kPixels * 4));

  ARGBToRGBARow_C(src_argb, dst_row, kPixels);
  ARGBToRGBA(src_argb, kPixels * 4, dst_api, kPixels * 4, kPixels, 1);
  ASSERT_EQ(0, memcmp(dst_row, dst_api, kPixels * 4));

  RGBAToARGBRow_C(src_argb, dst_row, kPixels);
  RGBAToARGB(src_argb, kPixels * 4, dst_api, kPixels * 4, kPixels, 1);
  ASSERT_EQ(0, memcmp(dst_row, dst_api, kPixels * 4));

  align_buffer_page_end_16(src_ar64, kPixels * 4);
  align_buffer_page_end_16(dst_ab64_row, kPixels * 4);
  align_buffer_page_end_16(dst_ab64_api, kPixels * 4);
  for (int i = 0; i < kPixels * 4; ++i) {
    src_ar64[i] = static_cast<uint16_t>(fastrand());
  }
  AR64ToAB64Row_C(src_ar64, dst_ab64_row, kPixels);
  AR64ToAB64(src_ar64, kPixels * 4, dst_ab64_api, kPixels * 4, kPixels, 1);
  ASSERT_EQ(0, memcmp(dst_ab64_row, dst_ab64_api, kPixels * 8));

  free_aligned_buffer_page_end_16(src_ar64);
  free_aligned_buffer_page_end_16(dst_ab64_row);
  free_aligned_buffer_page_end_16(dst_ab64_api);
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_row);
  free_aligned_buffer_page_end(dst_api);
}
#endif  // ENABLE_ROW_TESTS

TEST_F(LibYUVPlanarTest, TestGaussPlane_F32) {
  const int kSize = benchmark_width_ * benchmark_height_ * 4;
  align_buffer_page_end(orig_pixels, kSize);
  align_buffer_page_end(dst_pixels_opt, kSize);
  align_buffer_page_end(dst_pixels_c, kSize);

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ((float*)(orig_pixels))[i] = (i & 1023) * 3.14f;
  }
  memset(dst_pixels_opt, 1, kSize);
  memset(dst_pixels_c, 2, kSize);

  MaskCpuFlags(disable_cpu_flags_);
  GaussPlane_F32((const float*)(orig_pixels), benchmark_width_,
                 (float*)(dst_pixels_c), benchmark_width_, benchmark_width_,
                 benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    GaussPlane_F32((const float*)(orig_pixels), benchmark_width_,
                   (float*)(dst_pixels_opt), benchmark_width_, benchmark_width_,
                   benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ASSERT_NEAR(((float*)(dst_pixels_c))[i], ((float*)(dst_pixels_opt))[i], 1.f)
        << i;
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVPlanarTest, HalfMergeUVPlane_Opt) {
  int dst_width = (benchmark_width_ + 1) / 2;
  int dst_height = (benchmark_height_ + 1) / 2;
  align_buffer_page_end(src_pixels_u, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_pixels_v, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_pixels_uv_opt, dst_width * 2 * dst_height);
  align_buffer_page_end(dst_pixels_uv_c, dst_width * 2 * dst_height);

  MemRandomize(src_pixels_u, benchmark_width_ * benchmark_height_);
  MemRandomize(src_pixels_v, benchmark_width_ * benchmark_height_);
  MemRandomize(dst_pixels_uv_opt, dst_width * 2 * dst_height);
  MemRandomize(dst_pixels_uv_c, dst_width * 2 * dst_height);

  MaskCpuFlags(disable_cpu_flags_);
  HalfMergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v,
                   benchmark_width_, dst_pixels_uv_c, dst_width * 2,
                   benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    HalfMergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v,
                     benchmark_width_, dst_pixels_uv_opt, dst_width * 2,
                     benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < dst_width * 2 * dst_height; ++i) {
    ASSERT_EQ(dst_pixels_uv_c[i], dst_pixels_uv_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}

TEST_F(LibYUVPlanarTest, NV12Copy) {
  const int halfwidth = (benchmark_width_ + 1) >> 1;
  const int halfheight = (benchmark_height_ + 1) >> 1;
  align_buffer_page_end(src_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_uv, halfwidth * 2 * halfheight);
  align_buffer_page_end(dst_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_uv, halfwidth * 2 * halfheight);

  MemRandomize(src_y, benchmark_width_ * benchmark_height_);
  MemRandomize(src_uv, halfwidth * 2 * halfheight);
  MemRandomize(dst_y, benchmark_width_ * benchmark_height_);
  MemRandomize(dst_uv, halfwidth * 2 * halfheight);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    NV12Copy(src_y, benchmark_width_, src_uv, halfwidth * 2, dst_y,
             benchmark_width_, dst_uv, halfwidth * 2, benchmark_width_,
             benchmark_height_);
  }

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ASSERT_EQ(src_y[i], dst_y[i]);
  }
  for (int i = 0; i < halfwidth * 2 * halfheight; ++i) {
    ASSERT_EQ(src_uv[i], dst_uv[i]);
  }

  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_uv);
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVPlanarTest, NV21Copy) {
  const int halfwidth = (benchmark_width_ + 1) >> 1;
  const int halfheight = (benchmark_height_ + 1) >> 1;
  align_buffer_page_end(src_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_vu, halfwidth * 2 * halfheight);
  align_buffer_page_end(dst_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_vu, halfwidth * 2 * halfheight);

  MemRandomize(src_y, benchmark_width_ * benchmark_height_);
  MemRandomize(src_vu, halfwidth * 2 * halfheight);
  MemRandomize(dst_y, benchmark_width_ * benchmark_height_);
  MemRandomize(dst_vu, halfwidth * 2 * halfheight);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    NV21Copy(src_y, benchmark_width_, src_vu, halfwidth * 2, dst_y,
             benchmark_width_, dst_vu, halfwidth * 2, benchmark_width_,
             benchmark_height_);
  }

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ASSERT_EQ(src_y[i], dst_y[i]);
  }
  for (int i = 0; i < halfwidth * 2 * halfheight; ++i) {
    ASSERT_EQ(src_vu[i], dst_vu[i]);
  }

  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_vu);
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_vu);
}

#if defined(ENABLE_ROW_TESTS) && !defined(LIBYUV_DISABLE_NEON) && \
    defined(__aarch64__)

TEST_F(LibYUVPlanarTest, TestConvertFP16ToFP32) {
  int i, j;
  const int y_plane_size = benchmark_width_ * benchmark_height_;

  align_buffer_page_end(orig_f, y_plane_size * 4);
  align_buffer_page_end(orig_y, y_plane_size * 2);
  align_buffer_page_end(dst_opt, y_plane_size * 4);
  align_buffer_page_end(rec_opt, y_plane_size * 2);

  for (i = 0; i < y_plane_size; ++i) {
    ((float*)orig_f)[i] = (float)(i % 10000) * 3.14f;
  }
  memset(orig_y, 1, y_plane_size * 2);
  memset(dst_opt, 2, y_plane_size * 4);
  memset(rec_opt, 3, y_plane_size * 2);

  ConvertFP32ToFP16Row_NEON((const float*)orig_f, (uint16_t*)orig_y,
                            y_plane_size);

  for (j = 0; j < benchmark_iterations_; j++) {
    ConvertFP16ToFP32Row_NEON((const uint16_t*)orig_y, (float*)dst_opt,
                              y_plane_size);
  }

  ConvertFP32ToFP16Row_NEON((const float*)dst_opt, (uint16_t*)rec_opt,
                            y_plane_size);

  for (i = 0; i < y_plane_size; ++i) {
    ASSERT_EQ(((const uint16_t*)orig_y)[i], ((const uint16_t*)rec_opt)[i]);
  }

  free_aligned_buffer_page_end(orig_f);
  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(rec_opt);
}

TEST_F(LibYUVPlanarTest, TestConvertFP16ToFP32Column) {
  int i, j;
  const int y_plane_size = benchmark_width_ * benchmark_height_;

  align_buffer_page_end(orig_f, y_plane_size * 4);
  align_buffer_page_end(orig_y, y_plane_size * 2);
  align_buffer_page_end(dst_opt, y_plane_size * 4);
  align_buffer_page_end(rec_opt, y_plane_size * 2);

  for (i = 0; i < y_plane_size; ++i) {
    ((float*)orig_f)[i] = (float)(i % 10000) * 3.14f;
  }
  memset(orig_y, 1, y_plane_size * 2);
  memset(dst_opt, 2, y_plane_size * 4);
  memset(rec_opt, 3, y_plane_size * 2);

  ConvertFP32ToFP16Row_NEON((const float*)orig_f, (uint16_t*)orig_y,
                            y_plane_size);

  for (j = 0; j < benchmark_iterations_; j++) {
    ConvertFP16ToFP32Column_NEON((const uint16_t*)orig_y, 1, (float*)dst_opt,
                                 y_plane_size);
  }

  ConvertFP32ToFP16Row_NEON((const float*)dst_opt, (uint16_t*)rec_opt,
                            y_plane_size);

  for (i = 0; i < y_plane_size; ++i) {
    ASSERT_EQ(((const uint16_t*)orig_y)[i], ((const uint16_t*)rec_opt)[i]);
  }

  free_aligned_buffer_page_end(orig_f);
  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(rec_opt);
}

#endif  // defined(ENABLE_ROW_TESTS) && defined(__aarch64__)

static int TestARGBOr(int width,
                      int height,
                      int benchmark_iterations,
                      int disable_cpu_flags,
                      int benchmark_cpu_info,
                      int invert,
                      int off,
                      uint32_t value) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb, kStride * abs(height) + off);
  align_buffer_page_end(dst_argb_c, kStride * abs(height));
  align_buffer_page_end(dst_argb_opt, kStride * abs(height));
  MemRandomize(src_argb, kStride * abs(height) + off);
  memset(dst_argb_c, 0, kStride * abs(height));
  memset(dst_argb_opt, 1, kStride * abs(height));

  MaskCpuFlags(disable_cpu_flags);
  ARGBOr(src_argb + off, kStride, dst_argb_c, kStride, width, invert * height,
         value);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBOr(src_argb + off, kStride, dst_argb_opt, kStride, width,
           invert * height, value);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * abs(height); ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBOr_Any) {
  int max_diff = TestARGBOr(benchmark_width_ + 1, benchmark_height_,
                            benchmark_iterations_, disable_cpu_flags_,
                            benchmark_cpu_info_, +1, 0, 0xff000000u);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBOr_Unaligned) {
  int max_diff = TestARGBOr(benchmark_width_, benchmark_height_,
                            benchmark_iterations_, disable_cpu_flags_,
                            benchmark_cpu_info_, +1, 1, 0xff000000u);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBOr_Invert) {
  int max_diff = TestARGBOr(benchmark_width_, benchmark_height_,
                            benchmark_iterations_, disable_cpu_flags_,
                            benchmark_cpu_info_, -1, 0, 0xff000000u);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBOr_Opt) {
  int max_diff = TestARGBOr(benchmark_width_, benchmark_height_,
                            benchmark_iterations_, disable_cpu_flags_,
                            benchmark_cpu_info_, +1, 0, 0xff000000u);
  EXPECT_EQ(0, max_diff);
}

static int TestARGBAlphaBlend(int width,
                              int height,
                              int benchmark_iterations,
                              int disable_cpu_flags,
                              int benchmark_cpu_info,
                              int invert,
                              int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb0, kStride * abs(height) + off);
  align_buffer_page_end(src_argb1, kStride * abs(height) + off);
  align_buffer_page_end(dst_argb_c, kStride * abs(height));
  align_buffer_page_end(dst_argb_opt, kStride * abs(height));
  MemRandomize(src_argb0, kStride * abs(height) + off);
  MemRandomize(src_argb1, kStride * abs(height) + off);
  memset(dst_argb_c, 0, kStride * abs(height));
  memset(dst_argb_opt, 1, kStride * abs(height));

  MaskCpuFlags(disable_cpu_flags);
  ARGBAlphaBlend(src_argb0 + off, kStride, src_argb1 + off, kStride, dst_argb_c,
                 kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAlphaBlend(src_argb0 + off, kStride, src_argb1 + off, kStride,
                   dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * abs(height); ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb0);
  free_aligned_buffer_page_end(src_argb1);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBAlphaBlend_Any) {
  int max_diff = TestARGBAlphaBlend(benchmark_width_ + 1, benchmark_height_,
                                    benchmark_iterations_, disable_cpu_flags_,
                                    benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBAlphaBlend_Unaligned) {
  int max_diff = TestARGBAlphaBlend(benchmark_width_, benchmark_height_,
                                    benchmark_iterations_, disable_cpu_flags_,
                                    benchmark_cpu_info_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBAlphaBlend_Invert) {
  int max_diff = TestARGBAlphaBlend(benchmark_width_, benchmark_height_,
                                    benchmark_iterations_, disable_cpu_flags_,
                                    benchmark_cpu_info_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBAlphaBlend_Opt) {
  int max_diff = TestARGBAlphaBlend(benchmark_width_, benchmark_height_,
                                    benchmark_iterations_, disable_cpu_flags_,
                                    benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBAlphaBlend_AllAlphas) {
  const int kWidth = 256;
  const int kHeight = 256;
  const int kStride = kWidth * 4;
  align_buffer_page_end(src_argb0, kStride * kHeight);
  align_buffer_page_end(src_argb1, kStride * kHeight);
  align_buffer_page_end(dst_argb_c, kStride * kHeight);
  align_buffer_page_end(dst_argb_opt, kStride * kHeight);
  for (int da = 0; da < 256; ++da) {
    for (int sa = 0; sa < 256; ++sa) {
      int idx = (da * 256 + sa) * 4;
      src_argb0[idx + 0] = (sa * 7 + da * 3 + 13) & 255;
      src_argb0[idx + 1] = (sa * 13 + da * 5 + 29) & 255;
      src_argb0[idx + 2] = (sa * 29 + da * 11 + 47) & 255;
      src_argb0[idx + 3] = sa;
      src_argb1[idx + 0] = (da * 17 + sa * 19 + 53) & 255;
      src_argb1[idx + 1] = (da * 23 + sa * 31 + 71) & 255;
      src_argb1[idx + 2] = (da * 37 + sa * 41 + 89) & 255;
      src_argb1[idx + 3] = da;
    }
  }
  MaskCpuFlags(disable_cpu_flags_);
  ARGBAlphaBlend(src_argb0, kStride, src_argb1, kStride, dst_argb_c, kStride,
                 kWidth, kHeight);
  MaskCpuFlags(benchmark_cpu_info_);
  ARGBAlphaBlend(src_argb0, kStride, src_argb1, kStride, dst_argb_opt, kStride,
                 kWidth, kHeight);
  for (int i = 0; i < kStride * kHeight; ++i) {
    ASSERT_EQ(dst_argb_c[i], dst_argb_opt[i]) << "Mismatch at byte " << i;
  }
  free_aligned_buffer_page_end(src_argb0);
  free_aligned_buffer_page_end(src_argb1);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
}

TEST_F(LibYUVPlanarTest, MSanUninitializedInputsAndTails) {
  static const int kWidths[] = {1, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65, 99};
  const int kMaxW = 128;
  const int kMaxSrcUV = kMaxW * 3 * 2 + 128;
  align_buffer_page_end(src0, kMaxW * 4 + 128);
  align_buffer_page_end(src1, kMaxW * 4 + 128);
  align_buffer_page_end(src2, kMaxW * 4 + 128);
  align_buffer_page_end(dst_c0, kMaxW * 4 + 128);
  align_buffer_page_end(dst_c1, kMaxW * 4 + 128);
  align_buffer_page_end(dst_c2, kMaxW * 4 + 128);
  align_buffer_page_end(dst_opt0, kMaxW * 4 + 128);
  align_buffer_page_end(dst_opt1, kMaxW * 4 + 128);
  align_buffer_page_end(dst_opt2, kMaxW * 4 + 128);
  align_buffer_page_end(uv_src, kMaxSrcUV * 3);

  for (size_t wi = 0; wi < sizeof(kWidths) / sizeof(kWidths[0]); ++wi) {
    const int w = kWidths[wi];
    const int uv_w = (w + 1) / 2;

    // 1. ARGBToI444 and ARGBToI420 with poisoned RGBX alpha and poisoned tail.
    for (int i = 0; i < w; ++i) {
      src0[i * 4 + 0] = static_cast<uint8_t>((i * 11 + 3) & 255);
      src0[i * 4 + 1] = static_cast<uint8_t>((i * 17 + 7) & 255);
      src0[i * 4 + 2] = static_cast<uint8_t>((i * 29 + 13) & 255);
      src0[i * 4 + 3] = 0u;
    }
    MaskCpuFlags(disable_cpu_flags_);
    ARGBToI444(src0, w * 4, dst_c0, w, dst_c1, w, dst_c2, w, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    for (int i = 0; i < w; ++i) {
      MSAN_POISON(&src0[i * 4 + 3], 1);
    }
    MSAN_POISON(src0 + w * 4, 128);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    MSAN_POISON(dst_opt2, kMaxW * 4);
    ARGBToI444(src0, w * 4, dst_opt0, w, dst_opt1, w, dst_opt2, w, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w);
    MSAN_CHECK_INITIALIZED(dst_opt1, w);
    MSAN_CHECK_INITIALIZED(dst_opt2, w);
    for (int i = 0; i < w; ++i) {
      EXPECT_EQ(dst_c0[i], dst_opt0[i]) << "ARGBToI444 Y w=" << w;
      EXPECT_EQ(dst_c1[i], dst_opt1[i]) << "ARGBToI444 U w=" << w;
      EXPECT_EQ(dst_c2[i], dst_opt2[i]) << "ARGBToI444 V w=" << w;
    }

    for (int i = 0; i < w; ++i) {
      src0[i * 4 + 3] = 0u;
    }
    MaskCpuFlags(disable_cpu_flags_);
    ARGBToI420(src0, w * 4, dst_c0, w, dst_c1, uv_w, dst_c2, uv_w, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    for (int i = 0; i < w; ++i) {
      MSAN_POISON(&src0[i * 4 + 3], 1);
    }
    MSAN_POISON(src0 + w * 4, 128);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    MSAN_POISON(dst_opt2, kMaxW * 4);
    ARGBToI420(src0, w * 4, dst_opt0, w, dst_opt1, uv_w, dst_opt2, uv_w, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w);
    MSAN_CHECK_INITIALIZED(dst_opt1, uv_w);
    MSAN_CHECK_INITIALIZED(dst_opt2, uv_w);
    for (int i = 0; i < w; ++i) {
      EXPECT_EQ(dst_c0[i], dst_opt0[i]) << "ARGBToI420 Y w=" << w;
    }
    for (int i = 0; i < uv_w; ++i) {
      EXPECT_EQ(dst_c1[i], dst_opt1[i]) << "ARGBToI420 U w=" << w;
      EXPECT_EQ(dst_c2[i], dst_opt2[i]) << "ARGBToI420 V w=" << w;
    }

    // 2. ARGBMirror with poisoned alpha on even pixels (verifying per-lane
    // shadow permutation) and poisoned tail.
    for (int i = 0; i < w; ++i) {
      src0[i * 4 + 0] = static_cast<uint8_t>((i * 5 + 1) & 255);
      src0[i * 4 + 1] = static_cast<uint8_t>((i * 9 + 2) & 255);
      src0[i * 4 + 2] = static_cast<uint8_t>((i * 15 + 3) & 255);
      src0[i * 4 + 3] = static_cast<uint8_t>((i * 21 + 4) & 255);
    }
    MaskCpuFlags(disable_cpu_flags_);
    ARGBMirror(src0, w * 4, dst_c0, w * 4, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    for (int i = 0; i < w; i += 2) {
      MSAN_POISON(&src0[i * 4 + 3], 1);
    }
    MSAN_POISON(src0 + w * 4, 128);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    ARGBMirror(src0, w * 4, dst_opt0, w * 4, w, 1);
    for (int i = 0; i < w; ++i) {
      const int src_i = w - 1 - i;
      MSAN_CHECK_INITIALIZED(&dst_opt0[i * 4], 3);
      EXPECT_EQ(dst_c0[i * 4 + 0], dst_opt0[i * 4 + 0]);
      EXPECT_EQ(dst_c0[i * 4 + 1], dst_opt0[i * 4 + 1]);
      EXPECT_EQ(dst_c0[i * 4 + 2], dst_opt0[i * 4 + 2]);
      if (src_i & 1) {
        MSAN_CHECK_INITIALIZED(&dst_opt0[i * 4 + 3], 1);
        EXPECT_EQ(dst_c0[i * 4 + 3], dst_opt0[i * 4 + 3]);
      }
    }

    // 3. RGB24Mirror, RAWToRGBA, RAWToRGB24, MergeRGBPlane with poisoned tails.
    for (int i = 0; i < w * 3; ++i) {
      src0[i] = static_cast<uint8_t>((i * 19 + 5) & 255);
    }
    MSAN_POISON(src0 + w * 3, 128);

    MaskCpuFlags(disable_cpu_flags_);
    RGB24Mirror(src0, w * 3, dst_c0, w * 3, w, 1);
    RAWToRGBA(src0, w * 3, dst_c1, w * 4, w, 1);
    RAWToRGB24(src0, w * 3, dst_c2, w * 3, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    MSAN_POISON(dst_opt2, kMaxW * 4);
    RGB24Mirror(src0, w * 3, dst_opt0, w * 3, w, 1);
    RAWToRGBA(src0, w * 3, dst_opt1, w * 4, w, 1);
    RAWToRGB24(src0, w * 3, dst_opt2, w * 3, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 3);
    MSAN_CHECK_INITIALIZED(dst_opt1, w * 4);
    MSAN_CHECK_INITIALIZED(dst_opt2, w * 3);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 3));
    EXPECT_EQ(0, memcmp(dst_c1, dst_opt1, w * 4));
    EXPECT_EQ(0, memcmp(dst_c2, dst_opt2, w * 3));

    for (int i = 0; i < w; ++i) {
      src0[i] = static_cast<uint8_t>((i * 3 + 11) & 255);
      src1[i] = static_cast<uint8_t>((i * 7 + 23) & 255);
      src2[i] = static_cast<uint8_t>((i * 13 + 37) & 255);
    }
    MSAN_POISON(src0 + w, 128);
    MSAN_POISON(src1 + w, 128);
    MSAN_POISON(src2 + w, 128);
    MaskCpuFlags(disable_cpu_flags_);
    MergeRGBPlane(src0, w, src1, w, src2, w, dst_c0, w * 3, w, 1);
    MirrorPlane(src0, w, dst_c1, w, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    MergeRGBPlane(src0, w, src1, w, src2, w, dst_opt0, w * 3, w, 1);
    MirrorPlane(src0, w, dst_opt1, w, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 3);
    MSAN_CHECK_INITIALIZED(dst_opt1, w);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 3));
    EXPECT_EQ(0, memcmp(dst_c1, dst_opt1, w));

    // 4. MirrorUVPlane and MirrorPlane_16 with poisoned tails.
    for (int i = 0; i < w * 2; ++i) {
      src0[i] = static_cast<uint8_t>((i * 23 + 9) & 255);
    }
    MSAN_POISON(src0 + w * 2, 128);
    MaskCpuFlags(disable_cpu_flags_);
    MirrorUVPlane(src0, w * 2, dst_c0, w * 2, w, 1);
    MirrorPlane_16(reinterpret_cast<const uint16_t*>(src0), w,
                   reinterpret_cast<uint16_t*>(dst_c1), w, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    MirrorUVPlane(src0, w * 2, dst_opt0, w * 2, w, 1);
    MirrorPlane_16(reinterpret_cast<const uint16_t*>(src0), w,
                   reinterpret_cast<uint16_t*>(dst_opt1), w, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 2);
    MSAN_CHECK_INITIALIZED(dst_opt1, w * 2);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 2));
    EXPECT_EQ(0, memcmp(dst_c1, dst_opt1, w * 2));

    // 5. I420ToAR30, NV12ToARGB, NV12ToRGB24, I444ToARGB, I210ToARGB.
    for (int i = 0; i < w; ++i) {
      src0[i] = static_cast<uint8_t>((i * 5 + 16) & 255);
      src1[i] = static_cast<uint8_t>((i * 9 + 64) & 255);
      src2[i] = static_cast<uint8_t>((i * 11 + 128) & 255);
    }
    MSAN_POISON(src0 + w, 128);
    MSAN_POISON(src1 + uv_w, 128);
    MSAN_POISON(src2 + uv_w, 128);
    MaskCpuFlags(disable_cpu_flags_);
    I420ToAR30(src0, w, src1, uv_w, src2, uv_w, dst_c0, w * 4, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    I420ToAR30(src0, w, src1, uv_w, src2, uv_w, dst_opt0, w * 4, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 4);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 4));

    for (int i = 0; i < uv_w * 2; ++i) {
      src1[i] = static_cast<uint8_t>((i * 13 + 80) & 255);
    }
    MSAN_POISON(src1 + uv_w * 2, 128);
    MaskCpuFlags(disable_cpu_flags_);
    NV12ToARGB(src0, w, src1, uv_w * 2, dst_c0, w * 4, w, 1);
    NV12ToRGB24(src0, w, src1, uv_w * 2, dst_c1, w * 3, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    MSAN_POISON(dst_opt1, kMaxW * 4);
    NV12ToARGB(src0, w, src1, uv_w * 2, dst_opt0, w * 4, w, 1);
    NV12ToRGB24(src0, w, src1, uv_w * 2, dst_opt1, w * 3, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 4);
    MSAN_CHECK_INITIALIZED(dst_opt1, w * 3);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 4));
    EXPECT_EQ(0, memcmp(dst_c1, dst_opt1, w * 3));

    for (int i = 0; i < w; ++i) {
      reinterpret_cast<uint16_t*>(src0)[i] =
          static_cast<uint16_t>((i * 17 + 64) & 1023);
    }
    for (int i = 0; i < uv_w; ++i) {
      reinterpret_cast<uint16_t*>(src1)[i] =
          static_cast<uint16_t>((i * 23 + 256) & 1023);
      reinterpret_cast<uint16_t*>(src2)[i] =
          static_cast<uint16_t>((i * 31 + 512) & 1023);
    }
    MSAN_POISON(src0 + w * 2, 128);
    MSAN_POISON(src1 + uv_w * 2, 128);
    MSAN_POISON(src2 + uv_w * 2, 128);
    MaskCpuFlags(disable_cpu_flags_);
    I210ToARGB(reinterpret_cast<const uint16_t*>(src0), w,
               reinterpret_cast<const uint16_t*>(src1), uv_w,
               reinterpret_cast<const uint16_t*>(src2), uv_w, dst_c0, w * 4, w,
               1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    I210ToARGB(reinterpret_cast<const uint16_t*>(src0), w,
               reinterpret_cast<const uint16_t*>(src1), uv_w,
               reinterpret_cast<const uint16_t*>(src2), uv_w, dst_opt0, w * 4,
               w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 4);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 4));

    // 6. ARGBAlphaBlend with poisoned tails and poisoned destination.
    for (int i = 0; i < w * 4; ++i) {
      src0[i] = static_cast<uint8_t>((i * 29 + 3) & 255);
      src1[i] = static_cast<uint8_t>((i * 37 + 11) & 255);
    }
    MSAN_POISON(src0 + w * 4, 128);
    MSAN_POISON(src1 + w * 4, 128);
    MaskCpuFlags(disable_cpu_flags_);
    ARGBAlphaBlend(src0, w * 4, src1, w * 4, dst_c0, w * 4, w, 1);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    ARGBAlphaBlend(src0, w * 4, src1, w * 4, dst_opt0, w * 4, w, 1);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 4);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 4));

    // 7. UVScale 3x box downscale (hits ScaleUVAddCols3_AVX512VBMI).
    const int src_uv_w = w * 3;
    const int src_uv_stride = src_uv_w * 2 + 64;
    MSAN_POISON(uv_src, kMaxSrcUV * 3);
    for (int row = 0; row < 3; ++row) {
      for (int i = 0; i < src_uv_w * 2; ++i) {
        uv_src[row * src_uv_stride + i] =
            static_cast<uint8_t>((row * 53 + i * 17 + 7) & 255);
      }
    }
    MaskCpuFlags(disable_cpu_flags_);
    UVScale(uv_src, src_uv_stride, src_uv_w, 3, dst_c0, w * 2, w, 1,
            kFilterBox);
    MaskCpuFlags(benchmark_cpu_info_);
    MSAN_POISON(dst_opt0, kMaxW * 4);
    UVScale(uv_src, src_uv_stride, src_uv_w, 3, dst_opt0, w * 2, w, 1,
            kFilterBox);
    MSAN_CHECK_INITIALIZED(dst_opt0, w * 2);
    EXPECT_EQ(0, memcmp(dst_c0, dst_opt0, w * 2));
  }

  free_aligned_buffer_page_end(src0);
  free_aligned_buffer_page_end(src1);
  free_aligned_buffer_page_end(src2);
  free_aligned_buffer_page_end(dst_c0);
  free_aligned_buffer_page_end(dst_c1);
  free_aligned_buffer_page_end(dst_c2);
  free_aligned_buffer_page_end(dst_opt0);
  free_aligned_buffer_page_end(dst_opt1);
  free_aligned_buffer_page_end(dst_opt2);
  free_aligned_buffer_page_end(uv_src);
}

// Regression test for RGBToUVMatrixRow_Any_SSSE3 MASK=15 (previously MASK=7
// overread 24 bytes past src_rgb24 at width=24).
TEST_F(LibYUVPlanarTest, RGB24ToI420_Width24_OOB) {
  const int kWidth = 24;
  const int kHeight = 2;
  align_buffer_page_end(src_rgb24, kWidth * 3 * kHeight);
  align_buffer_page_end(dst_y_c, kWidth * kHeight);
  align_buffer_page_end(dst_u_c, (kWidth / 2) * (kHeight / 2));
  align_buffer_page_end(dst_v_c, (kWidth / 2) * (kHeight / 2));
  align_buffer_page_end(dst_y_opt, kWidth * kHeight);
  align_buffer_page_end(dst_u_opt, (kWidth / 2) * (kHeight / 2));
  align_buffer_page_end(dst_v_opt, (kWidth / 2) * (kHeight / 2));
  MemRandomize(src_rgb24, kWidth * 3 * kHeight);

  MaskCpuFlags(disable_cpu_flags_);
  RGB24ToI420(src_rgb24, kWidth * 3, dst_y_c, kWidth, dst_u_c, kWidth / 2,
              dst_v_c, kWidth / 2, kWidth, kHeight);

  const int kNoAvx512 =
      benchmark_cpu_info_ & ~(kCpuHasAVX512BW | kCpuHasAVX512VL |
                              kCpuHasAVX512VBMI | kCpuHasAVX512VNNI);
  const int kSsse3Only = kNoAvx512 & ~kCpuHasAVX2;
  const int kModes[3] = {benchmark_cpu_info_, kNoAvx512, kSsse3Only};
  for (int m = 0; m < 3; ++m) {
    MaskCpuFlags(kModes[m]);
    memset(dst_y_opt, 0, kWidth * kHeight);
    memset(dst_u_opt, 0, (kWidth / 2) * (kHeight / 2));
    memset(dst_v_opt, 0, (kWidth / 2) * (kHeight / 2));
    RGB24ToI420(src_rgb24, kWidth * 3, dst_y_opt, kWidth, dst_u_opt, kWidth / 2,
                dst_v_opt, kWidth / 2, kWidth, kHeight);
    for (int i = 0; i < kWidth * kHeight; ++i) {
      ASSERT_EQ(dst_y_c[i], dst_y_opt[i]);
    }
    for (int i = 0; i < (kWidth / 2) * (kHeight / 2); ++i) {
      ASSERT_EQ(dst_u_c[i], dst_u_opt[i]);
      ASSERT_EQ(dst_v_c[i], dst_v_opt[i]);
    }
  }
  MaskCpuFlags(benchmark_cpu_info_);

  free_aligned_buffer_page_end(src_rgb24);
  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_opt);
}

#ifdef ENABLE_ROW_TESTS
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3) ||    \
    defined(HAS_HALFWIDTHROW_16TO8_AVX2) ||     \
    defined(HAS_HALFWIDTHROW_16TO8_AVX512BW) || \
    defined(HAS_HALFROW_16TO8_AVX512BW)
// Regression test for HalfWidthRow_16To8 (>= 0x8000 unsigned MSB samples and
// exact page-end tail bounds) and HalfRow_16To8 (non-power-of-two scale).
TEST_F(LibYUVPlanarTest, Half16To8_HighBitAndNonPow2Scale) {
  const int kWidths[] = {1, 7, 15, 24, 31, 33, 63, 65};
  for (size_t w_idx = 0; w_idx < sizeof(kWidths) / sizeof(kWidths[0]);
       ++w_idx) {
    const int w = kWidths[w_idx];
    align_buffer_page_end_16(src_uv_hw, w * 4);
    align_buffer_page_end(dst_c, w);
    align_buffer_page_end(dst_opt, w);
    for (int i = 0; i < w * 4; ++i) {
      src_uv_hw[i] = static_cast<uint16_t>(0x8000u + ((i * 257u) & 0x7fffu));
    }
    HalfWidthRow_16To8_C(src_uv_hw, w * 2, dst_c, 256, w);
#if defined(HAS_HALFWIDTHROW_16TO8_AVX512BW)
    if (TestCpuFlag(kCpuHasAVX512BW)) {
      memset(dst_opt, 0x5a, w);
      HalfWidthRow_16To8_AVX512BW(src_uv_hw, w * 2, dst_opt, 256, w);
      for (int i = 0; i < w; ++i) {
        ASSERT_EQ(dst_c[i], dst_opt[i]) << "AVX512BW w=" << w << " i=" << i;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
      memset(dst_opt, 0x5a, w);
      HalfWidthRow_16To8_Any_AVX2(src_uv_hw, w * 2, dst_opt, 256, w);
      for (int i = 0; i < w; ++i) {
        ASSERT_EQ(dst_c[i], dst_opt[i]) << "AVX2 w=" << w << " i=" << i;
      }
    }
#endif
#if defined(HAS_HALFWIDTHROW_16TO8_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
      memset(dst_opt, 0x5a, w);
      HalfWidthRow_16To8_Any_SSSE3(src_uv_hw, w * 2, dst_opt, 256, w);
      for (int i = 0; i < w; ++i) {
        ASSERT_EQ(dst_c[i], dst_opt[i]) << "SSSE3 w=" << w << " i=" << i;
      }
    }
#endif

    // HalfRow_16To8 with non-power-of-two scale (e.g. 16320).
    align_buffer_page_end_16(src_uv_hr, w * 2);
    for (int i = 0; i < w * 2; ++i) {
      src_uv_hr[i] = static_cast<uint16_t>((i * 73u) & 1023u);
    }
    HalfRow_16To8_C(src_uv_hr, w, dst_c, 16320, w);
#if defined(HAS_HALFROW_16TO8_AVX512BW)
    if (TestCpuFlag(kCpuHasAVX512BW)) {
      memset(dst_opt, 0x5a, w);
      HalfRow_16To8_AVX512BW(src_uv_hr, w, dst_opt, 16320, w);
      for (int i = 0; i < w; ++i) {
        ASSERT_EQ(dst_c[i], dst_opt[i])
            << "HalfRow_16To8_AVX512BW w=" << w << " i=" << i;
      }
    }
#endif
    free_aligned_buffer_page_end_16(src_uv_hr);
    free_aligned_buffer_page_end_16(src_uv_hw);
    free_aligned_buffer_page_end(dst_c);
    free_aligned_buffer_page_end(dst_opt);
  }
}
#endif  // HAS_HALFWIDTHROW_16TO8_* || HAS_HALFROW_16TO8_AVX512BW

#if defined(HAS_UNPACKMT2T_AVX2)
// Regression test for UnpackMT2T_C and UnpackMT2T_AVX2 when size is not a
// multiple of the 80-byte (64-pixel) MT2T block size.
TEST_F(LibYUVPlanarTest, UnpackMT2T_PartialTail) {
  if (!TestCpuFlag(kCpuHasAVX2)) {
    return;
  }
  const size_t kSrcSize = 120;  // 1 full 80-byte block + 40 trailing bytes
  const size_t kDstCount = (kSrcSize / 80) * 64;
  align_buffer_page_end(src, kSrcSize);
  align_buffer_page_end_16(dst_c, kDstCount);
  align_buffer_page_end_16(dst_opt, kDstCount);
  MemRandomize(src, kSrcSize);
  UnpackMT2T_C(src, dst_c, kSrcSize);
  UnpackMT2T_AVX2(src, dst_opt, kSrcSize);
  for (size_t i = 0; i < kDstCount; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]) << "i=" << i;
  }
  // Verify size < 80 performs zero out-of-bounds reads or writes.
  UnpackMT2T_C(src + 80, dst_c, 40);
  UnpackMT2T_AVX2(src + 80, dst_opt, 40);
  for (size_t i = 0; i < kDstCount; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]) << "i=" << i;
  }
  free_aligned_buffer_page_end(src);
  free_aligned_buffer_page_end_16(dst_c);
  free_aligned_buffer_page_end_16(dst_opt);
}
#endif  // HAS_UNPACKMT2T_AVX2
#endif  // ENABLE_ROW_TESTS

// Regression test for ConvertToLSBPlane_16 with depth=16 (scale=65536).
TEST_F(LibYUVPlanarTest, ConvertToLSBPlane_16_Depth16) {
  const int kWidth = 37;
  const int kHeight = 4;
  align_buffer_page_end_16(src_y, kWidth * kHeight);
  align_buffer_page_end_16(dst_c, kWidth * kHeight);
  align_buffer_page_end_16(dst_opt, kWidth * kHeight);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    src_y[i] = static_cast<uint16_t>(0x1234u + i * 251u);
  }
  MaskCpuFlags(disable_cpu_flags_);
  ConvertToLSBPlane_16(src_y, kWidth, dst_c, kWidth, kWidth, kHeight, 16);

  const int kNoAvx512 =
      benchmark_cpu_info_ & ~(kCpuHasAVX512BW | kCpuHasAVX512VL);
  MaskCpuFlags(benchmark_cpu_info_);
  memset(dst_opt, 0, kWidth * kHeight * 2);
  ConvertToLSBPlane_16(src_y, kWidth, dst_opt, kWidth, kWidth, kHeight, 16);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]);
  }

  MaskCpuFlags(kNoAvx512);
  memset(dst_opt, 0, kWidth * kHeight * 2);
  ConvertToLSBPlane_16(src_y, kWidth, dst_opt, kWidth, kWidth, kHeight, 16);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]);
  }
  MaskCpuFlags(benchmark_cpu_info_);

  free_aligned_buffer_page_end_16(src_y);
  free_aligned_buffer_page_end_16(dst_c);
  free_aligned_buffer_page_end_16(dst_opt);
}

// Regression test for ARGBPolynomial saturation on outputs >= 32768 / 65536,
// odd widths, and 4-byte-aligned (non-16-byte-aligned) polynomial arrays.
TEST_F(LibYUVPlanarTest, ARGBPolynomial_LargeOutputAndUnalignedPoly) {
  const int kWidth = 37;
  const int kHeight = 3;
  align_buffer_page_end(src_argb, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_c, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_opt, kWidth * 4 * kHeight);
  align_buffer_page_end(poly_buf, 17 * sizeof(float));
  // Offset by +1 float so poly is 4-byte aligned, not 16-byte aligned.
  float* poly = reinterpret_cast<float*>(poly_buf) + 1;
  for (int c = 0; c < 4; ++c) {
    poly[0 + c] = 0.0f;
    poly[4 + c] = 300.0f;  // 255 * 300 = 76500 >= 65536 -> must clamp to 255
    poly[8 + c] = 1.0f;    // 200^2 = 40000 in [32768, 65535] -> clamp to 255
    poly[12 + c] = 0.0f;
  }
  for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
    src_argb[i] = static_cast<uint8_t>(i & 255);
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBPolynomial(src_argb, kWidth * 4, dst_c, kWidth * 4, poly, kWidth,
                 kHeight);

  const int kSse2Only =
      benchmark_cpu_info_ & ~(kCpuHasAVX2 | kCpuHasFMA3 | kCpuHasAVX512BW);
  const int kModes[2] = {benchmark_cpu_info_, kSse2Only};
  for (int m = 0; m < 2; ++m) {
    MaskCpuFlags(kModes[m]);
    memset(dst_opt, 0, kWidth * 4 * kHeight);
    ARGBPolynomial(src_argb, kWidth * 4, dst_opt, kWidth * 4, poly, kWidth,
                   kHeight);
    for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
      ASSERT_EQ(dst_c[i], dst_opt[i]) << "mode=" << m << " i=" << i;
    }
  }
  MaskCpuFlags(benchmark_cpu_info_);

  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(poly_buf);
}

// Regression test for ARGBBlur / CumulativeSumToAverageRow_AVX2 when radius=0
// or radius=1 produces small window areas (area == 1 or area == 2) on 0xff.
TEST_F(LibYUVPlanarTest, ARGBBlur_SmallRadiusWhite) {
  const int kWidth = 37;
  const int kHeight = 4;
  const int kCumSumStride32 = (kWidth + 16) * 4;
  align_buffer_page_end(src_argb, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_c, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_opt, kWidth * 4 * kHeight);
  align_buffer_page_end(cumsum, kCumSumStride32 * (kHeight + 2) * 4);
  memset(src_argb, 0xff, kWidth * 4 * kHeight);

  for (int radius = 1; radius <= 2; ++radius) {
    MaskCpuFlags(disable_cpu_flags_);
    ARGBBlur(src_argb, kWidth * 4, dst_c, kWidth * 4,
             reinterpret_cast<int32_t*>(cumsum), kCumSumStride32, kWidth,
             kHeight, radius);
    MaskCpuFlags(benchmark_cpu_info_);
    memset(dst_opt, 0, kWidth * 4 * kHeight);
    ARGBBlur(src_argb, kWidth * 4, dst_opt, kWidth * 4,
             reinterpret_cast<int32_t*>(cumsum), kCumSumStride32, kWidth,
             kHeight, radius);
    for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
      ASSERT_EQ(dst_c[i], dst_opt[i]) << "radius=" << radius << " i=" << i;
    }
  }

  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(cumsum);
}

// Regression test for ARGBLumaColorTable with standard BT.601 green weight
// 150 (> 127) and bright pixels where R*77 + G*150 + B*29 > 32767.
TEST_F(LibYUVPlanarTest, ARGBLumaColorTable_BrightPixels) {
  const int kWidth = 37;
  const int kHeight = 3;
  align_buffer_page_end(src_argb, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_c, kWidth * 4 * kHeight);
  align_buffer_page_end(dst_opt, kWidth * 4 * kHeight);
  align_buffer_page_end(luma, 32768);
  for (int i = 0; i < 32768; ++i) {
    luma[i] = static_cast<uint8_t>(((i >> 8) * 3 + (i & 255)) & 255);
  }
  for (int i = 0; i < kWidth * kHeight; ++i) {
    src_argb[i * 4 + 0] = static_cast<uint8_t>(200 + (i % 56));
    src_argb[i * 4 + 1] = static_cast<uint8_t>(200 + ((i * 3) % 56));
    src_argb[i * 4 + 2] = static_cast<uint8_t>(200 + ((i * 7) % 56));
    src_argb[i * 4 + 3] = static_cast<uint8_t>(i & 255);
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBLumaColorTable(src_argb, kWidth * 4, dst_c, kWidth * 4, luma, kWidth,
                     kHeight);
  MaskCpuFlags(benchmark_cpu_info_);
  ARGBLumaColorTable(src_argb, kWidth * 4, dst_opt, kWidth * 4, luma, kWidth,
                     kHeight);
  for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
    ASSERT_EQ(dst_c[i], dst_opt[i]) << "i=" << i;
  }

  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(luma);
}

#if defined(ENABLE_ROW_TESTS) && !defined(LIBYUV_DISABLE_X86) &&   \
    (defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64) || \
     defined(__i386__))
// Regression test for 2-byte-aligned (non-16/32-byte-aligned) Intel
// YuvConstants and ArgbConstants structs and custom non-0x8000 kAddUV offsets.
TEST_F(LibYUVPlanarTest, UnalignedMatrixConstantsAndCustomKAddUV) {
  const int kWidth = 38;
  const int kHeight = 4;
  align_buffer_page_end(yuvc_buf, sizeof(YuvConstants) + 16);
  align_buffer_page_end(argbc_buf, sizeof(ArgbConstants) + 16);
  // Offset by +2 bytes (matches alignof == 2, misaligned for movdqa/vmovdqa).
  YuvConstants* yuvc = reinterpret_cast<YuvConstants*>(yuvc_buf + 2);
  ArgbConstants* argbc = reinterpret_cast<ArgbConstants*>(argbc_buf + 2);
  memcpy(yuvc, &kYuvI601Constants, sizeof(YuvConstants));
  memcpy(argbc, &kArgbI601Constants, sizeof(ArgbConstants));
  for (int i = 0; i < 16; ++i) {
    argbc->kAddUV[i] = 0x7200u;  // Non-default UV bias (!= 0x8000)
  }

  align_buffer_page_end(src_argb, kWidth * 4 * kHeight);
  align_buffer_page_end(y_c, kWidth * kHeight);
  align_buffer_page_end(u_c, kWidth * kHeight);
  align_buffer_page_end(v_c, kWidth * kHeight);
  align_buffer_page_end(y_opt, kWidth * kHeight);
  align_buffer_page_end(u_opt, kWidth * kHeight);
  align_buffer_page_end(v_opt, kWidth * kHeight);
  align_buffer_page_end(rgb_c, kWidth * 4 * kHeight);
  align_buffer_page_end(rgb_opt, kWidth * 4 * kHeight);
  MemRandomize(src_argb, kWidth * 4 * kHeight);

  MaskCpuFlags(disable_cpu_flags_);
  ARGBToI444Matrix(src_argb, kWidth * 4, y_c, kWidth, u_c, kWidth, v_c, kWidth,
                   argbc, kWidth, kHeight);
  I420ToARGBMatrix(y_c, kWidth, u_c, kWidth / 2, v_c, kWidth / 2, rgb_c,
                   kWidth * 4, yuvc, kWidth, kHeight);

  const int kNoAvx512 =
      benchmark_cpu_info_ & ~(kCpuHasAVX512BW | kCpuHasAVX512VL |
                              kCpuHasAVX512VBMI | kCpuHasAVX512VNNI);
  const int kSsse3Only = kNoAvx512 & ~kCpuHasAVX2;
  const int kModes[3] = {benchmark_cpu_info_, kNoAvx512, kSsse3Only};
  for (int m = 0; m < 3; ++m) {
    MaskCpuFlags(kModes[m]);
    ARGBToI444Matrix(src_argb, kWidth * 4, y_opt, kWidth, u_opt, kWidth, v_opt,
                     kWidth, argbc, kWidth, kHeight);
    I420ToARGBMatrix(y_c, kWidth, u_c, kWidth / 2, v_c, kWidth / 2, rgb_opt,
                     kWidth * 4, yuvc, kWidth, kHeight);
    for (int i = 0; i < kWidth * kHeight; ++i) {
      ASSERT_EQ(y_c[i], y_opt[i]);
      ASSERT_EQ(u_c[i], u_opt[i]) << "mode=" << m << " i=" << i;
      ASSERT_EQ(v_c[i], v_opt[i]) << "mode=" << m << " i=" << i;
    }
    for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
      ASSERT_EQ(rgb_c[i], rgb_opt[i]) << "mode=" << m << " i=" << i;
    }
  }

  // Also test ARGBToI420Matrix with custom kAddUV and I400ToARGBMatrix with
  // unaligned YuvConstants.
  MaskCpuFlags(disable_cpu_flags_);
  ARGBToI420Matrix(src_argb, kWidth * 4, y_c, kWidth, u_c, kWidth / 2, v_c,
                   kWidth / 2, argbc, kWidth, kHeight);
  I400ToARGBMatrix(y_c, kWidth, rgb_c, kWidth * 4, yuvc, kWidth, kHeight);
  for (int m = 0; m < 3; ++m) {
    MaskCpuFlags(kModes[m]);
    ARGBToI420Matrix(src_argb, kWidth * 4, y_opt, kWidth, u_opt, kWidth / 2,
                     v_opt, kWidth / 2, argbc, kWidth, kHeight);
    I400ToARGBMatrix(y_c, kWidth, rgb_opt, kWidth * 4, yuvc, kWidth, kHeight);
    for (int i = 0; i < (kWidth / 2) * (kHeight / 2); ++i) {
      ASSERT_NEAR(u_c[i], u_opt[i], 1) << "mode=" << m << " i=" << i;
      ASSERT_NEAR(v_c[i], v_opt[i], 1) << "mode=" << m << " i=" << i;
    }
    for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
      ASSERT_EQ(rgb_c[i], rgb_opt[i]) << "mode=" << m << " i=" << i;
    }
  }
  MaskCpuFlags(benchmark_cpu_info_);

  free_aligned_buffer_page_end(yuvc_buf);
  free_aligned_buffer_page_end(argbc_buf);
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(y_c);
  free_aligned_buffer_page_end(u_c);
  free_aligned_buffer_page_end(v_c);
  free_aligned_buffer_page_end(y_opt);
  free_aligned_buffer_page_end(u_opt);
  free_aligned_buffer_page_end(v_opt);
  free_aligned_buffer_page_end(rgb_c);
  free_aligned_buffer_page_end(rgb_opt);
}
#endif  // ENABLE_ROW_TESTS

// Regression test for MergeAR64Plane depth=16 (UBSan shift) and
// MergeARGB16To8Plane depth=8 saturation on out-of-range >255 samples.
TEST_F(LibYUVPlanarTest, MergeAR64Depth16AndMergeARGB16To8Depth8Clamp) {
  const int kWidth = 37;
  const int kHeight = 2;
  align_buffer_page_end_16(src_r, kWidth * kHeight);
  align_buffer_page_end_16(src_g, kWidth * kHeight);
  align_buffer_page_end_16(src_b, kWidth * kHeight);
  align_buffer_page_end_16(src_a, kWidth * kHeight);
  align_buffer_page_end_16(ar64_c, kWidth * 4 * kHeight);
  align_buffer_page_end_16(ar64_opt, kWidth * 4 * kHeight);
  align_buffer_page_end(argb_c, kWidth * 4 * kHeight);
  align_buffer_page_end(argb_opt, kWidth * 4 * kHeight);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    src_r[i] = static_cast<uint16_t>(200u + (i * 19u));
    src_g[i] = static_cast<uint16_t>(100u + (i * 37u));
    src_b[i] = static_cast<uint16_t>(50u + (i * 53u));
    src_a[i] = static_cast<uint16_t>(0x8000u + (i * 101u));
  }

  MaskCpuFlags(disable_cpu_flags_);
  MergeAR64Plane(src_r, kWidth, src_g, kWidth, src_b, kWidth, src_a, kWidth,
                 ar64_c, kWidth * 4, kWidth, kHeight, 16);
  MergeARGB16To8Plane(src_r, kWidth, src_g, kWidth, src_b, kWidth, src_a,
                      kWidth, argb_c, kWidth * 4, kWidth, kHeight, 8);

  MaskCpuFlags(benchmark_cpu_info_);
  MergeAR64Plane(src_r, kWidth, src_g, kWidth, src_b, kWidth, src_a, kWidth,
                 ar64_opt, kWidth * 4, kWidth, kHeight, 16);
  MergeARGB16To8Plane(src_r, kWidth, src_g, kWidth, src_b, kWidth, src_a,
                      kWidth, argb_opt, kWidth * 4, kWidth, kHeight, 8);

  for (int i = 0; i < kWidth * 4 * kHeight; ++i) {
    ASSERT_EQ(ar64_c[i], ar64_opt[i]);
    ASSERT_EQ(argb_c[i], argb_opt[i]);
  }

  free_aligned_buffer_page_end_16(src_r);
  free_aligned_buffer_page_end_16(src_g);
  free_aligned_buffer_page_end_16(src_b);
  free_aligned_buffer_page_end_16(src_a);
  free_aligned_buffer_page_end_16(ar64_c);
  free_aligned_buffer_page_end_16(ar64_opt);
  free_aligned_buffer_page_end(argb_c);
  free_aligned_buffer_page_end(argb_opt);
}

}  // namespace libyuv
