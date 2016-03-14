/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UNIT_TEST_UNIT_TEST_H_  // NOLINT
#define UNIT_TEST_UNIT_TEST_H_

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <gtest/gtest.h>

#include "libyuv/basic_types.h"

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

#define OFFBY 0

#define align_buffer_page_end(var, size)                                       \
  uint8* var;                                                                  \
  uint8* var##_mem;                                                            \
  var##_mem = reinterpret_cast<uint8*>(malloc((((size) + 4095) & ~4095) +      \
      OFFBY));                                                                 \
  var = var##_mem + (-(size) & 4095) + OFFBY;

#define free_aligned_buffer_page_end(var) \
  free(var##_mem);  \
  var = 0;

#ifdef WIN32
static inline double get_time() {
  LARGE_INTEGER t, f;
  QueryPerformanceCounter(&t);
  QueryPerformanceFrequency(&f);
  return static_cast<double>(t.QuadPart) / static_cast<double>(f.QuadPart);
}
#else
static inline double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec * 1e-6;
}
#endif

extern unsigned int fastrand_seed;
inline int fastrand() {
  fastrand_seed = fastrand_seed * 214013u + 2531011u;
  return static_cast<int>((fastrand_seed >> 16) & 0xffff);
}

static inline void MemRandomize(uint8* dst, int64 len) {
  int64 i;
  for (i = 0; i < len - 1; i += 2) {
    *reinterpret_cast<uint16*>(dst) = fastrand();
    dst += 2;
  }
  for (; i < len; ++i) {
    *dst++ = fastrand();
  }
}

class LibYUVColorTest : public ::testing::Test {
 protected:
  LibYUVColorTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

class LibYUVConvertTest : public ::testing::Test {
 protected:
  LibYUVConvertTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

class LibYUVScaleTest : public ::testing::Test {
 protected:
  LibYUVScaleTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

class LibYUVRotateTest : public ::testing::Test {
 protected:
  LibYUVRotateTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

class LibYUVPlanarTest : public ::testing::Test {
 protected:
  LibYUVPlanarTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

class LibYUVBaseTest : public ::testing::Test {
 protected:
  LibYUVBaseTest();

  int benchmark_iterations_;  // Default 1. Use 1000 for benchmarking.
  int benchmark_width_;  // Default 1280.  Use 640 for benchmarking VGA.
  int benchmark_height_;  // Default 720.  Use 360 for benchmarking VGA.
  int benchmark_pixels_div256_;  // Total pixels to benchmark / 256.
  int benchmark_pixels_div1280_;  // Total pixels to benchmark / 1280.
  int disable_cpu_flags_;  // Default 1.  Use -1 for benchmarking.
  int benchmark_cpu_info_;  // Default -1.  Use 1 to disable SIMD.
};

#endif  // UNIT_TEST_UNIT_TEST_H_  NOLINT
