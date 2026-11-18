/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#if !defined(LIBYUV_DISABLE_HVX) && defined(__HVX__)
#include <hexagon_protos.h>
#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>
#include <string.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

static inline HVX_Vector LoadAligned_HVX(const void* ptr) {
  HVX_Vector v;
  memcpy(&v, (const HVX_Vector*)ptr, sizeof(v));
  return v;
}

static inline HVX_Vector LoadU_HVX(const void* ptr) {
  HVX_UVector v;
  memcpy(&v, ptr, sizeof(v));
  return v;
}

static inline void StoreU_HVX(void* ptr, HVX_Vector v) {
  memcpy(ptr, &v, sizeof(HVX_UVector));
}

static inline HVX_Vector LoadPartial_HVX(const void* ptr, int n_bytes) {
  if (n_bytes >= 128) {
    return LoadU_HVX(ptr);
  }
  const uint8_t* p = (const uint8_t*)ptr;
  const uintptr_t addr = (uintptr_t)p;
  const uintptr_t offset = addr & 127u;
  HVX_Vector v0 = LoadAligned_HVX(p);
  HVX_Vector v1 =
      (offset + (uintptr_t)n_bytes > 128u) ? LoadAligned_HVX(p + 128) : v0;
  return Q6_V_valign_VVR(v1, v0, addr);
}

static inline void StorePartial_HVX(void* ptr, HVX_Vector v, int n_bytes) {
  if (n_bytes >= 128) {
    StoreU_HVX(ptr, v);
    return;
  }
  uint8_t* p = (uint8_t*)ptr;
  const uintptr_t addr = (uintptr_t)p;
  const uintptr_t offset = addr & 127u;
  HVX_Vector value = Q6_V_vlalign_VVR(v, v, addr);
  HVX_VectorPred ql_not = Q6_Q_vsetq_R(addr);
  HVX_VectorPred qr = Q6_Q_vsetq2_R(offset + (uintptr_t)n_bytes);
  if (offset + (uintptr_t)n_bytes > 128u) {
    Q6_vmem_QRIV(qr, p + 128, value);
    qr = Q6_Q_vcmp_eq_VbVb(value, value);
  }
  ql_not = Q6_Q_or_QQn(ql_not, qr);
  Q6_vmem_QnRIV(ql_not, p, value);
}

static inline void Interleave4_HVX(HVX_Vector* x0,
                                   HVX_Vector* x1,
                                   HVX_Vector* x2,
                                   HVX_Vector* x3,
                                   int stride) {
  HVX_VectorPair p01 = Q6_W_vshuff_VVR(*x1, *x0, -stride);
  HVX_VectorPair p23 = Q6_W_vshuff_VVR(*x3, *x2, -stride);
  HVX_VectorPair q01 =
      Q6_W_vshuff_VVR(Q6_V_lo_W(p23), Q6_V_lo_W(p01), -2 * stride);
  HVX_VectorPair q23 =
      Q6_W_vshuff_VVR(Q6_V_hi_W(p23), Q6_V_hi_W(p01), -2 * stride);
  *x0 = Q6_V_lo_W(q01);
  *x1 = Q6_V_hi_W(q01);
  *x2 = Q6_V_lo_W(q23);
  *x3 = Q6_V_hi_W(q23);
}

static inline void Interleave32_HVX(HVX_Vector* x,
                                    HVX_Vector* out,
                                    int stride) {
  int g, q, j;
  for (g = 0; g < 32; g += 4) {
    Interleave4_HVX(&x[g], &x[g + 1], &x[g + 2], &x[g + 3], stride);
  }
  for (q = 0; q < 4; ++q) {
    Interleave4_HVX(&x[q], &x[4 + q], &x[8 + q], &x[12 + q], 4 * stride);
    Interleave4_HVX(&x[16 + q], &x[20 + q], &x[24 + q], &x[28 + q], 4 * stride);
    for (j = 0; j < 4; ++j) {
      HVX_VectorPair p =
          Q6_W_vshuff_VVR(x[16 + 4 * j + q], x[4 * j + q], -16 * stride);
      out[8 * q + 2 * j] = Q6_V_lo_W(p);
      out[8 * q + 2 * j + 1] = Q6_V_hi_W(p);
    }
  }
}

#if defined(HAS_TRANSPOSEWXH_HVX) || defined(HAS_TRANSPOSEUVWXH_HVX)
static void TransposeWxH_Byte_HVX(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst_a,
                                  int dst_stride_a,
                                  uint8_t* dst_b,
                                  int dst_stride_b,
                                  int byte_width,
                                  int height) {
  const ptrdiff_t s_stride = src_stride;
  const ptrdiff_t da_stride = dst_stride_a;
  const ptrdiff_t db_stride = dst_stride_b;
  HVX_Vector buf[4][32];
  HVX_Vector v[32];
  const HVX_Vector vzero = Q6_V_vzero();

  do {
    const int block_h = height > 128 ? 128 : height;
    const uint8_t* src2 = src;
    uint8_t* dst2_a = dst_a;
    uint8_t* dst2_b = dst_b;
    int bw = byte_width;

    do {
      const int block_w = bw > 128 ? 128 : bw;
      int s, r, k;
      for (s = 0; s < 4; ++s) {
        if (s * 32 < block_h) {
          int strip_h = block_h - s * 32;
          const uint8_t* src_row = src2 + (ptrdiff_t)(s * 32) * s_stride;
          if (strip_h > 32) {
            strip_h = 32;
          }
          if (block_w == 128) {
            for (r = 0; r < strip_h; ++r) {
              v[r] = LoadU_HVX(src_row);
              src_row += s_stride;
            }
          } else {
            for (r = 0; r < strip_h; ++r) {
              v[r] = LoadPartial_HVX(src_row, block_w);
              src_row += s_stride;
            }
          }
          for (; r < 32; ++r) {
            v[r] = vzero;
          }
          Interleave32_HVX(v, buf[s], 1);
        }
      }

      if (block_h == 128) {
        for (k = 0; k < 32 && k * 4 < block_w; ++k) {
          HVX_Vector b0 = buf[0][k];
          HVX_Vector b1 = buf[1][k];
          HVX_Vector b2 = buf[2][k];
          HVX_Vector b3 = buf[3][k];
          Interleave4_HVX(&b0, &b1, &b2, &b3, 32);
          StoreU_HVX(dst2_a, b0);
          if (k * 4 + 1 < block_w) {
            StoreU_HVX(dst2_b, b1);
          }
          if (k * 4 + 2 < block_w) {
            StoreU_HVX(dst2_a + da_stride, b2);
          }
          if (k * 4 + 3 < block_w) {
            StoreU_HVX(dst2_b + db_stride, b3);
          }
          dst2_a += 2 * da_stride;
          dst2_b += 2 * db_stride;
        }
      } else {
        for (k = 0; k < 32 && k * 4 < block_w; ++k) {
          HVX_Vector b0 = buf[0][k];
          HVX_Vector b1 = (block_h > 32) ? buf[1][k] : vzero;
          HVX_Vector b2 = (block_h > 64) ? buf[2][k] : vzero;
          HVX_Vector b3 = (block_h > 96) ? buf[3][k] : vzero;
          Interleave4_HVX(&b0, &b1, &b2, &b3, 32);
          StorePartial_HVX(dst2_a, b0, block_h);
          if (k * 4 + 1 < block_w) {
            StorePartial_HVX(dst2_b, b1, block_h);
          }
          if (k * 4 + 2 < block_w) {
            StorePartial_HVX(dst2_a + da_stride, b2, block_h);
          }
          if (k * 4 + 3 < block_w) {
            StorePartial_HVX(dst2_b + db_stride, b3, block_h);
          }
          dst2_a += 2 * da_stride;
          dst2_b += 2 * db_stride;
        }
      }

      src2 += 128;
      bw -= 128;
    } while (bw > 0);

    src += 128 * s_stride;
    dst_a += 128;
    dst_b += 128;
    height -= 128;
  } while (height > 0);
}
#endif  // defined(HAS_TRANSPOSEWXH_HVX) || defined(HAS_TRANSPOSEUVWXH_HVX)

#ifdef HAS_TRANSPOSEWXH_HVX
void TransposeWxH_HVX(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst,
                      int dst_stride,
                      int width,
                      int height) {
  TransposeWxH_Byte_HVX(src, src_stride, dst, dst_stride * 2,
                        dst + (width > 1 ? dst_stride : 0), dst_stride * 2,
                        width, height);
}
#endif  // HAS_TRANSPOSEWXH_HVX

#ifdef HAS_TRANSPOSEUVWXH_HVX
void TransposeUVWxH_HVX(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst_a,
                        int dst_stride_a,
                        uint8_t* dst_b,
                        int dst_stride_b,
                        int width,
                        int height) {
  TransposeWxH_Byte_HVX(src, src_stride, dst_a, dst_stride_a, dst_b,
                        dst_stride_b, width * 2, height);
}
#endif  // HAS_TRANSPOSEUVWXH_HVX

#ifdef HAS_TRANSPOSEWXH_16_HVX
void TransposeWxH_16_HVX(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width,
                         int height) {
  const ptrdiff_t s_stride = src_stride;
  const ptrdiff_t d_stride = dst_stride;
  HVX_Vector buf[2][32];
  HVX_Vector v[32];
  const HVX_Vector vzero = Q6_V_vzero();

  do {
    const int block_h = height > 64 ? 64 : height;
    const uint16_t* src2 = src;
    uint16_t* dst2 = dst;
    int w = width;

    do {
      const int block_w = w > 64 ? 64 : w;
      int s, r, k;
      for (s = 0; s < 2; ++s) {
        if (s * 32 < block_h) {
          int strip_h = block_h - s * 32;
          const uint16_t* src_row = src2 + (ptrdiff_t)(s * 32) * s_stride;
          if (strip_h > 32) {
            strip_h = 32;
          }
          if (block_w == 64) {
            for (r = 0; r < strip_h; ++r) {
              v[r] = LoadU_HVX(src_row);
              src_row += s_stride;
            }
          } else {
            for (r = 0; r < strip_h; ++r) {
              v[r] = LoadPartial_HVX(src_row, block_w * 2);
              src_row += s_stride;
            }
          }
          for (; r < 32; ++r) {
            v[r] = vzero;
          }
          Interleave32_HVX(v, buf[s], 2);
        }
      }

      if (block_h == 64) {
        for (k = 0; k < 32 && k * 2 < block_w; ++k) {
          HVX_VectorPair c01 = Q6_W_vshuff_VVR(buf[1][k], buf[0][k], -64);
          StoreU_HVX(dst2, Q6_V_lo_W(c01));
          if (k * 2 + 1 < block_w) {
            StoreU_HVX(dst2 + d_stride, Q6_V_hi_W(c01));
          }
          dst2 += 2 * d_stride;
        }
      } else {
        for (k = 0; k < 32 && k * 2 < block_w; ++k) {
          HVX_Vector b0 = buf[0][k];
          HVX_Vector b1 = (block_h > 32) ? buf[1][k] : vzero;
          HVX_VectorPair c01 = Q6_W_vshuff_VVR(b1, b0, -64);
          StorePartial_HVX(dst2, Q6_V_lo_W(c01), block_h * 2);
          if (k * 2 + 1 < block_w) {
            StorePartial_HVX(dst2 + d_stride, Q6_V_hi_W(c01), block_h * 2);
          }
          dst2 += 2 * d_stride;
        }
      }

      src2 += 64;
      w -= 64;
    } while (w > 0);

    src += 64 * s_stride;
    dst += 64;
    height -= 64;
  } while (height > 0);
}
#endif  // HAS_TRANSPOSEWXH_16_HVX

#ifdef HAS_TRANSPOSEWXH_32_HVX
void TransposeWxH_32_HVX(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width,
                         int height) {
  const ptrdiff_t s_stride = src_stride;
  const ptrdiff_t d_stride = dst_stride;
  HVX_Vector v[32];
  HVX_Vector out[32];
  const HVX_Vector vzero = Q6_V_vzero();

  do {
    const int block_h = height > 32 ? 32 : height;
    const uint8_t* src2 = src;
    uint8_t* dst2 = dst;
    int w = width;

    do {
      const int block_w = w > 32 ? 32 : w;
      const uint8_t* src_row = src2;
      int r, c;
      if (block_w == 32) {
        for (r = 0; r < block_h; ++r) {
          v[r] = LoadU_HVX(src_row);
          src_row += s_stride;
        }
      } else {
        for (r = 0; r < block_h; ++r) {
          v[r] = LoadPartial_HVX(src_row, block_w * 4);
          src_row += s_stride;
        }
      }
      for (; r < 32; ++r) {
        v[r] = vzero;
      }
      Interleave32_HVX(v, out, 4);

      if (block_h == 32) {
        for (c = 0; c < block_w; ++c) {
          StoreU_HVX(dst2, out[c]);
          dst2 += d_stride;
        }
      } else {
        for (c = 0; c < block_w; ++c) {
          StorePartial_HVX(dst2, out[c], block_h * 4);
          dst2 += d_stride;
        }
      }

      src2 += 128;
      w -= 32;
    } while (w > 0);

    src += 32 * s_stride;
    dst += 128;
    height -= 32;
  } while (height > 0);
}
#endif  // HAS_TRANSPOSEWXH_32_HVX

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_HVX) && defined(__HVX__)
