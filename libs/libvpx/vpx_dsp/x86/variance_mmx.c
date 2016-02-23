/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_dsp_rtcd.h"

#include "vpx_ports/mem.h"

DECLARE_ALIGNED(16, static const int16_t, bilinear_filters_mmx[8][8]) = {
  { 128, 128, 128, 128,   0,   0,   0,   0 },
  { 112, 112, 112, 112,  16,  16,  16,  16 },
  {  96,  96,  96,  96,  32,  32,  32,  32 },
  {  80,  80,  80,  80,  48,  48,  48,  48 },
  {  64,  64,  64,  64,  64,  64,  64,  64 },
  {  48,  48,  48,  48,  80,  80,  80,  80 },
  {  32,  32,  32,  32,  96,  96,  96,  96 },
  {  16,  16,  16,  16, 112, 112, 112, 112 }
};

extern void vpx_get4x4var_mmx(const uint8_t *a, int a_stride,
                              const uint8_t *b, int b_stride,
                              unsigned int *sse, int *sum);

extern void vpx_filter_block2d_bil4x4_var_mmx(const unsigned char *ref_ptr,
                                              int ref_pixels_per_line,
                                              const unsigned char *src_ptr,
                                              int src_pixels_per_line,
                                              const int16_t *HFilter,
                                              const int16_t *VFilter,
                                              int *sum,
                                              unsigned int *sumsquared);

extern void vpx_filter_block2d_bil_var_mmx(const unsigned char *ref_ptr,
                                           int ref_pixels_per_line,
                                           const unsigned char *src_ptr,
                                           int src_pixels_per_line,
                                           unsigned int Height,
                                           const int16_t *HFilter,
                                           const int16_t *VFilter,
                                           int *sum,
                                           unsigned int *sumsquared);


unsigned int vpx_variance4x4_mmx(const unsigned char *a, int a_stride,
                                 const unsigned char *b, int b_stride,
                                 unsigned int *sse) {
    unsigned int var;
    int avg;

    vpx_get4x4var_mmx(a, a_stride, b, b_stride, &var, &avg);
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 4));
}

unsigned int vpx_variance8x8_mmx(const unsigned char *a, int a_stride,
                                 const unsigned char *b, int b_stride,
                                 unsigned int *sse) {
    unsigned int var;
    int avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &var, &avg);
    *sse = var;

    return (var - (((unsigned int)avg * avg) >> 6));
}

unsigned int vpx_mse16x16_mmx(const unsigned char *a, int a_stride,
                              const unsigned char *b, int b_stride,
                              unsigned int *sse) {
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse2, &sum2);
    vpx_get8x8var_mmx(a + 8 * a_stride + 8, a_stride,
                      b + 8 * b_stride + 8, b_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    *sse = var;
    return var;
}

unsigned int vpx_variance16x16_mmx(const unsigned char *a, int a_stride,
                                   const unsigned char *b, int b_stride,
                                   unsigned int *sse) {
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse2, &sum2);
    vpx_get8x8var_mmx(a + 8 * a_stride + 8, a_stride,
                      b + 8 * b_stride + 8, b_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    avg = sum0 + sum1 + sum2 + sum3;
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 8));
}

unsigned int vpx_variance16x8_mmx(const unsigned char *a, int a_stride,
                                  const unsigned char *b, int b_stride,
                                  unsigned int *sse) {
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8, a_stride, b + 8, b_stride, &sse1, &sum1);

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;
    return (var - (((unsigned int)avg * avg) >> 7));
}

unsigned int vpx_variance8x16_mmx(const unsigned char *a, int a_stride,
                                  const unsigned char *b, int b_stride,
                                  unsigned int *sse) {
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vpx_get8x8var_mmx(a, a_stride, b, b_stride, &sse0, &sum0);
    vpx_get8x8var_mmx(a + 8 * a_stride, a_stride,
                      b + 8 * b_stride, b_stride, &sse1, &sum1);

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;

    return (var - (((unsigned int)avg * avg) >> 7));
}

uint32_t vpx_sub_pixel_variance4x4_mmx(const uint8_t *a, int a_stride,
                                       int xoffset, int yoffset,
                                       const uint8_t *b, int b_stride,
                                       uint32_t *sse) {
    int xsum;
    unsigned int xxsum;
    vpx_filter_block2d_bil4x4_var_mmx(a, a_stride, b, b_stride,
                                      bilinear_filters_mmx[xoffset],
                                      bilinear_filters_mmx[yoffset],
                                      &xsum, &xxsum);
    *sse = xxsum;
    return (xxsum - (((unsigned int)xsum * xsum) >> 4));
}


uint32_t vpx_sub_pixel_variance8x8_mmx(const uint8_t *a, int a_stride,
                                       int xoffset, int yoffset,
                                       const uint8_t *b, int b_stride,
                                       uint32_t *sse) {
    int xsum;
    uint32_t xxsum;
    vpx_filter_block2d_bil_var_mmx(a, a_stride, b, b_stride, 8,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum, &xxsum);
    *sse = xxsum;
    return (xxsum - (((uint32_t)xsum * xsum) >> 6));
}

uint32_t vpx_sub_pixel_variance16x16_mmx(const uint8_t *a, int a_stride,
                                         int xoffset, int yoffset,
                                         const uint8_t *b, int b_stride,
                                         uint32_t *sse) {
    int xsum0, xsum1;
    unsigned int xxsum0, xxsum1;

    vpx_filter_block2d_bil_var_mmx(a, a_stride, b, b_stride, 16,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum0, &xxsum0);

    vpx_filter_block2d_bil_var_mmx(a + 8, a_stride, b + 8, b_stride, 16,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum1, &xxsum1);

    xsum0 += xsum1;
    xxsum0 += xxsum1;

    *sse = xxsum0;
    return (xxsum0 - (((uint32_t)xsum0 * xsum0) >> 8));
}

uint32_t vpx_sub_pixel_variance16x8_mmx(const uint8_t *a, int a_stride,
                                        int xoffset, int yoffset,
                                        const uint8_t *b, int b_stride,
                                        uint32_t *sse) {
    int xsum0, xsum1;
    unsigned int xxsum0, xxsum1;

    vpx_filter_block2d_bil_var_mmx(a, a_stride, b, b_stride, 8,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum0, &xxsum0);

    vpx_filter_block2d_bil_var_mmx(a + 8, a_stride, b + 8, b_stride, 8,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum1, &xxsum1);

    xsum0 += xsum1;
    xxsum0 += xxsum1;

    *sse = xxsum0;
    return (xxsum0 - (((uint32_t)xsum0 * xsum0) >> 7));
}

uint32_t vpx_sub_pixel_variance8x16_mmx(const uint8_t *a, int a_stride,
                                        int xoffset, int yoffset,
                                        const uint8_t *b, int b_stride,
                                        uint32_t *sse) {
    int xsum;
    unsigned int xxsum;
    vpx_filter_block2d_bil_var_mmx(a, a_stride, b, b_stride, 16,
                                   bilinear_filters_mmx[xoffset],
                                   bilinear_filters_mmx[yoffset],
                                   &xsum, &xxsum);
    *sse = xxsum;
    return (xxsum - (((uint32_t)xsum * xsum) >> 7));
}

uint32_t vpx_variance_halfpixvar16x16_h_mmx(const uint8_t *a, int a_stride,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse) {
  return vpx_sub_pixel_variance16x16_mmx(a, a_stride, 4, 0, b, b_stride, sse);
}

uint32_t vpx_variance_halfpixvar16x16_v_mmx(const uint8_t *a, int a_stride,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse) {
  return vpx_sub_pixel_variance16x16_mmx(a, a_stride, 0, 4, b, b_stride, sse);
}

uint32_t vpx_variance_halfpixvar16x16_hv_mmx(const uint8_t *a, int a_stride,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  return vpx_sub_pixel_variance16x16_mmx(a, a_stride, 4, 4, b, b_stride, sse);
}
