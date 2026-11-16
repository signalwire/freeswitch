/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libyuv/compare.h"
#include "libyuv/version.h"

static bool ExtractResolutionFromFilename(const char* name,
                                          int* width_ptr,
                                          int* height_ptr) {
  for (int i = 0; name[i]; ++i) {
    if ((name[i] == '.' || name[i] == '_') && name[i + 1] >= '0' &&
        name[i + 1] <= '9') {
      int n = sscanf(name + i + 1, "%dx%d", width_ptr, height_ptr);  // NOLINT
      if (2 == n) {
        return true;
      }
    }
  }
  return false;
}

int main(int argc, char** argv) {
  const char* name1 = NULL;
  const char* name2 = NULL;
  int width = 0;
  int height = 0;

  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-s") && i + 2 < argc) {
      width = atoi(argv[++i]);   // NOLINT
      height = atoi(argv[++i]);  // NOLINT
    } else if (!name1) {
      name1 = argv[i];
    } else if (!name2) {
      name2 = argv[i];
    } else if (width == 0 && i + 1 < argc) {
      width = atoi(argv[i]);     // NOLINT
      height = atoi(argv[++i]);  // NOLINT
    }
  }

  if (!name1) {
    printf("libyuv compare v%d\n", LIBYUV_VERSION);
    printf("compare file1.yuv [file2.yuv [width height]]\n");
    return -1;
  }

  if (width <= 0 || height <= 0) {
    if (!ExtractResolutionFromFilename(name1, &width, &height) && name2) {
      ExtractResolutionFromFilename(name2, &width, &height);
    }
  }

  FILE* fin1 = fopen(name1, "rb");
  if (!fin1) {
    fprintf(stderr, "Cannot open %s\n", name1);
    return -1;
  }
  FILE* fin2 = name2 ? fopen(name2, "rb") : NULL;
  if (name2 && !fin2) {
    fprintf(stderr, "Cannot open %s\n", name2);
    fclose(fin1);
    return -1;
  }

  const bool has_dims = (width >= 16 && height >= 16);
  const int block_width = has_dims ? width : 256;
  const int y_size = has_dims ? width * height : 32768;
  const int uv_width = (width + 1) / 2;
  const int uv_height = (height + 1) / 2;
  const int uv_size = has_dims ? uv_width * uv_height : 0;
  const int block_size = has_dims ? (y_size + 2 * uv_size) : 32768;

  uint8_t* buf1 = static_cast<uint8_t*>(malloc(block_size));
  uint8_t* buf2 = static_cast<uint8_t*>(malloc(block_size));
  uint32_t hash1 = 5381;
  uint32_t hash2 = 5381;
  uint64_t sum_square_err = 0;
  uint64_t size_min = 0;
  double ssim_sum = 0.0;
  int ssim_count = 0;
  int amt1 = 0;
  int amt2 = 0;
  do {
    amt1 = static_cast<int>(fread(buf1, 1, block_size, fin1));
    if (amt1 > 0) {
      hash1 = libyuv::HashDjb2(buf1, amt1, hash1);
    }
    if (fin2) {
      amt2 = static_cast<int>(fread(buf2, 1, block_size, fin2));
      if (amt2 > 0) {
        hash2 = libyuv::HashDjb2(buf2, amt2, hash2);
      }
      int amt_min = (amt1 < amt2) ? amt1 : amt2;
      size_min += amt_min;
      sum_square_err += libyuv::ComputeSumSquareError(buf1, buf2, amt_min);
      if (has_dims && amt_min == block_size) {
        ssim_sum += libyuv::I420Ssim(
            buf1, width, buf1 + y_size, uv_width, buf1 + y_size + uv_size,
            uv_width, buf2, width, buf2 + y_size, uv_width,
            buf2 + y_size + uv_size, uv_width, width, height);
        ++ssim_count;
      } else if (amt_min >= block_width * 8) {
        ssim_sum += libyuv::CalcFrameSsim(buf1, block_width, buf2, block_width,
                                          block_width, amt_min / block_width);
        ++ssim_count;
      }
    }
  } while (amt1 > 0 || amt2 > 0);

  printf("hash1 %x", hash1);
  if (fin2) {
    printf(", hash2 %x", hash2);
    double mse = (size_min > 0) ? static_cast<double>(sum_square_err) /
                                      static_cast<double>(size_min)
                                : 0.0;
    printf(", mse %.2f", mse);
    double psnr = libyuv::SumSquareErrorToPsnr(sum_square_err, size_min);
    printf(", psnr %.2f", psnr);
    double ssim = (ssim_count > 0) ? (ssim_sum / ssim_count) : 0.0;
    printf(", ssim %.6f", ssim);
    fclose(fin2);
  }
  printf("\n");
  fclose(fin1);
  free(buf1);
  free(buf2);
  return 0;
}
