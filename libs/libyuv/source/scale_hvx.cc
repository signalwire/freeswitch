/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale_row.h"

#if !defined(LIBYUV_DISABLE_HVX) && defined(__HVX__)
#include <hexagon_protos.h>
#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>
#include <string.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

static __inline HVX_Vector vloada(const void* ptr) {
  HVX_Vector v;
  memcpy(&v, (const HVX_Vector*)ptr, sizeof(v));
  return v;
}

static __inline HVX_UVector vloadu(const void* ptr) {
  HVX_UVector v;
  memcpy(&v, ptr, sizeof(v));
  return v;
}

static __inline void vstoreu(void* ptr, HVX_UVector v) {
  memcpy(ptr, &v, sizeof(v));
}

static __inline void vstoreu_partial(void* dst, HVX_Vector v, int n) {
  if (n >= 128) {
    vstoreu(dst, v);
  } else if (n > 0) {
    const uintptr_t addr = (uintptr_t)dst;
    const uintptr_t offset = addr & 127;
    HVX_Vector value = Q6_V_vlalign_VVR(v, v, addr);
    HVX_VectorPred ql_not = Q6_Q_vsetq_R(addr);
    HVX_VectorPred qr = Q6_Q_vsetq2_R(offset + (uintptr_t)n);
    if (offset + (uintptr_t)n > 128) {
      Q6_vmem_QRIV(qr, (uint8_t*)dst + 128, value);
      qr = Q6_Q_vcmp_eq_VbVb(value, value);
    }
    ql_not = Q6_Q_or_QQn(ql_not, qr);
    Q6_vmem_QnRIV(ql_not, dst, value);
  }
}

static __inline HVX_Vector vloadu_partial_zero(const void* src, int n) {
  if (n >= 128) {
    return vloadu(src);
  }
  if (n <= 0) {
    return Q6_V_vzero();
  }
  const uintptr_t addr = (uintptr_t)src;
  const uintptr_t offset = addr & 127;
  HVX_Vector v0 = vloada(src);
  HVX_Vector v1 =
      (offset + (uintptr_t)n > 128) ? vloada((const uint8_t*)src + 128) : v0;
  HVX_Vector v = Q6_V_valign_VVR(v1, v0, addr);
  return Q6_V_vmux_QVV(Q6_Q_vsetq_R(n), v, Q6_V_vzero());
}

void ScaleAddRow_HVX(const uint8_t* src_ptr, uint16_t* dst_ptr, int src_width) {
  while (src_width > 0) {
    int w = src_width >= 128 ? 128 : src_width;
    HVX_Vector s = vloadu_partial_zero(src_ptr, w);
    HVX_VectorPair s16 = Q6_Wuh_vunpack_Vub(s);
    int b0 = w > 64 ? 128 : w * 2;
    HVX_Vector d0 = vloadu_partial_zero(dst_ptr, b0);
    d0 = Q6_Vh_vadd_VhVh(d0, Q6_V_lo_W(s16));
    vstoreu_partial(dst_ptr, d0, b0);
    if (w > 64) {
      int b1 = (w - 64) * 2;
      HVX_Vector d1 = vloadu_partial_zero(dst_ptr + 64, b1);
      d1 = Q6_Vh_vadd_VhVh(d1, Q6_V_hi_W(s16));
      vstoreu_partial(dst_ptr + 64, d1, b1);
    }
    src_ptr += 128;
    dst_ptr += 128;
    src_width -= 128;
  }
}

// vrdelta control vectors to compact 3 vectors of 3-column UV sums into 1
// vector of 32 UV pairs (64 uint16_t):
// kUVAddCols3P0 routes dwords 0, 3, ..., 30 -> dwords 0..10 (bytes 0..43)
static const uint8_t kUVAddCols3P0[128] __attribute__((aligned(128))) = {
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// kUVAddCols3P1 routes dwords 1, 4, ..., 31 -> dwords 11..21 (bytes 44..87)
static const uint8_t kUVAddCols3P1[128] __attribute__((aligned(128))) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x10, 0x10, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// kUVAddCols3P2 routes dwords 2, 5, ..., 29 -> dwords 22..31 (bytes 88..127)
static const uint8_t kUVAddCols3P2[128] __attribute__((aligned(128))) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08,
};

void ScaleUVAddCols3_HVX(int dst_width,
                         int scaleval,
                         const uint16_t* src_uv,
                         uint8_t* dst_uv) {
  const HVX_Vector p0 = vloada(kUVAddCols3P0);
  const HVX_Vector p1 = vloada(kUVAddCols3P1);
  const HVX_Vector p2 = vloada(kUVAddCols3P2);
  const HVX_VectorPred q44 = Q6_Q_vsetq_R(44);
  const HVX_VectorPred q88 = Q6_Q_vsetq_R(88);
  const uint32_t scale_splat =
      ((uint32_t)(uint16_t)scaleval << 16) | (uint16_t)scaleval;
  while (dst_width > 0) {
    int w = dst_width >= 32 ? 32 : dst_width;
    int src_bytes = w * 12;
    HVX_Vector v0 =
        vloadu_partial_zero(src_uv, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector v1 =
        src_bytes > 128
            ? vloadu_partial_zero((const uint8_t*)src_uv + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector v2 =
        src_bytes > 256
            ? vloadu_partial_zero((const uint8_t*)src_uv + 256, src_bytes - 256)
            : Q6_V_vzero();

    HVX_Vector s0 =
        Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(v0, Q6_V_valign_VVR(v1, v0, 4)),
                        Q6_V_valign_VVR(v1, v0, 8));
    HVX_Vector s1 =
        Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(v1, Q6_V_valign_VVR(v2, v1, 4)),
                        Q6_V_valign_VVR(v2, v1, 8));
    HVX_Vector s2 =
        Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(v2, Q6_V_valign_VVR(v2, v2, 4)),
                        Q6_V_valign_VVR(v2, v2, 8));

    HVX_Vector sum = Q6_V_vmux_QVV(
        q44, Q6_V_vrdelta_VV(s0, p0),
        Q6_V_vmux_QVV(q88, Q6_V_vrdelta_VV(s1, p1), Q6_V_vrdelta_VV(s2, p2)));

    HVX_VectorPair prod = Q6_Wuw_vmpy_VuhRuh(sum, scale_splat);
    HVX_Vector scaled = Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(prod), Q6_V_lo_W(prod));
    HVX_Vector out = Q6_Vub_vpack_VhVh_sat(scaled, scaled);
    vstoreu_partial(dst_uv, out, w * 2);
    src_uv += 192;
    dst_uv += 64;
    dst_width -= 32;
  }
}

void ScaleRowDown2_HVX(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 128 ? 128 : dst_width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -1);
    vstoreu_partial(dst, Q6_V_hi_W(d), w);
    src_ptr += 256;
    dst += 128;
    dst_width -= 128;
  }
}

void ScaleRowDown2Linear_HVX(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst,
                             int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 128 ? 128 : dst_width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_Vector avg = Q6_Vub_vavg_VubVub_rnd(Q6_V_lo_W(d), Q6_V_hi_W(d));
    vstoreu_partial(dst, avg, w);
    src_ptr += 256;
    dst += 128;
    dst_width -= 128;
  }
}

void ScaleRowDown2Box_HVX(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          int dst_width) {
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  const int32_t kOnes = 0x01010101;
  HVX_Vector vtwo = Q6_Vh_vsplat_R(2);
  while (dst_width > 0) {
    int w = dst_width >= 128 ? 128 : dst_width;
    int src_bytes = w * 2;
    HVX_Vector s0 = vloadu_partial_zero(s, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t0 = vloadu_partial_zero(t, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector sum0 = Q6_Vh_vdmpy_VubRb(s0, kOnes);
    sum0 = Q6_Vh_vdmpyacc_VhVubRb(sum0, t0, kOnes);
    sum0 = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum0, vtwo), 2);

    HVX_Vector sum1 = Q6_V_vzero();
    if (src_bytes > 128) {
      HVX_Vector s1 = vloadu_partial_zero(s + 128, src_bytes - 128);
      HVX_Vector t1 = vloadu_partial_zero(t + 128, src_bytes - 128);
      sum1 = Q6_Vh_vdmpy_VubRb(s1, kOnes);
      sum1 = Q6_Vh_vdmpyacc_VhVubRb(sum1, t1, kOnes);
      sum1 = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum1, vtwo), 2);
    }
    HVX_Vector out = Q6_Vub_vpack_VhVh_sat(sum1, sum0);
    vstoreu_partial(dst, out, w);
    s += 256;
    t += 256;
    dst += 128;
    dst_width -= 128;
  }
}

void ScaleRowDown2Box_Odd_HVX(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst,
                              int dst_width) {
  dst_width -= 1;
  if (dst_width > 0) {
    ScaleRowDown2Box_HVX(src_ptr, src_stride, dst, dst_width);
  }
  const uint8_t* s = src_ptr + dst_width * 2;
  const uint8_t* t = s + src_stride;
  dst[dst_width] = (s[0] + t[0] + 1) >> 1;
}

void ScaleRowDown2_16_HVX(const uint16_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint16_t* dst,
                          int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 64, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -2);
    vstoreu_partial(dst, Q6_V_hi_W(d), w * 2);
    src_ptr += 128;
    dst += 64;
    dst_width -= 64;
  }
}

void ScaleRowDown2Linear_16_HVX(const uint16_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint16_t* dst,
                                int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 64, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -2);
    HVX_Vector avg = Q6_Vuh_vavg_VuhVuh_rnd(Q6_V_lo_W(d), Q6_V_hi_W(d));
    vstoreu_partial(dst, avg, w * 2);
    src_ptr += 128;
    dst += 64;
    dst_width -= 64;
  }
}

void ScaleRowDown2Box_16_HVX(const uint16_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint16_t* dst,
                             int dst_width) {
  const uint16_t* s = src_ptr;
  const uint16_t* t = src_ptr + src_stride;
  HVX_Vector vtwo = Q6_V_vsplat_R(2);
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 = vloadu_partial_zero(s, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t0 = vloadu_partial_zero(t, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(s + 64, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(t + 64, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair ds = Q6_W_vdeal_VVR(s1, s0, -2);
    HVX_VectorPair dt = Q6_W_vdeal_VVR(t1, t0, -2);
    HVX_VectorPair s_even = Q6_Wuw_vunpack_Vuh(Q6_V_lo_W(ds));
    HVX_VectorPair s_odd = Q6_Wuw_vunpack_Vuh(Q6_V_hi_W(ds));
    HVX_VectorPair t_even = Q6_Wuw_vunpack_Vuh(Q6_V_lo_W(dt));
    HVX_VectorPair t_odd = Q6_Wuw_vunpack_Vuh(Q6_V_hi_W(dt));

    HVX_Vector sum_lo = Q6_Vw_vadd_VwVw(Q6_V_lo_W(s_even), Q6_V_lo_W(s_odd));
    sum_lo = Q6_Vw_vadd_VwVw(sum_lo, Q6_V_lo_W(t_even));
    sum_lo = Q6_Vw_vadd_VwVw(sum_lo, Q6_V_lo_W(t_odd));
    sum_lo = Q6_Vw_vasr_VwR(Q6_Vw_vadd_VwVw(sum_lo, vtwo), 2);

    HVX_Vector sum_hi = Q6_Vw_vadd_VwVw(Q6_V_hi_W(s_even), Q6_V_hi_W(s_odd));
    sum_hi = Q6_Vw_vadd_VwVw(sum_hi, Q6_V_hi_W(t_even));
    sum_hi = Q6_Vw_vadd_VwVw(sum_hi, Q6_V_hi_W(t_odd));
    sum_hi = Q6_Vw_vasr_VwR(Q6_Vw_vadd_VwVw(sum_hi, vtwo), 2);

    HVX_Vector out = Q6_Vuh_vpack_VwVw_sat(sum_hi, sum_lo);
    vstoreu_partial(dst, out, w * 2);
    s += 128;
    t += 128;
    dst += 64;
    dst_width -= 64;
  }
}

void ScaleRowDown4_HVX(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst_ptr,
                       int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 128 ? 128 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_ptr + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_ptr + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_ptr + 384, src_bytes - 384)
                        : Q6_V_vzero();
    HVX_VectorPair d01 = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_VectorPair d23 = Q6_W_vdeal_VVR(s3, s2, -1);
    HVX_VectorPair d_even = Q6_W_vdeal_VVR(Q6_V_lo_W(d23), Q6_V_lo_W(d01), -1);
    vstoreu_partial(dst_ptr, Q6_V_hi_W(d_even), w);
    src_ptr += 512;
    dst_ptr += 128;
    dst_width -= 128;
  }
}

void ScaleRowDown4Box_HVX(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst_ptr,
                          int dst_width) {
  const uint8_t* r0 = src_ptr;
  const uint8_t* r1 = r0 + src_stride;
  const uint8_t* r2 = r1 + src_stride;
  const uint8_t* r3 = r2 + src_stride;
  const int32_t kOnes = 0x01010101;
  HVX_Vector veight = Q6_Vh_vsplat_R(8);
  while (dst_width > 0) {
    int w = dst_width >= 128 ? 128 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector sum[4];
    for (int c = 0; c < 4; ++c) {
      int rem = src_bytes - c * 128;
      if (rem > 0) {
        int b = rem > 128 ? 128 : rem;
        HVX_Vector v0 = vloadu_partial_zero(r0 + c * 128, b);
        HVX_Vector v1 = vloadu_partial_zero(r1 + c * 128, b);
        HVX_Vector v2 = vloadu_partial_zero(r2 + c * 128, b);
        HVX_Vector v3 = vloadu_partial_zero(r3 + c * 128, b);
        HVX_Vector s = Q6_Vw_vrmpy_VubRb(v0, kOnes);
        s = Q6_Vw_vrmpyacc_VwVubRb(s, v1, kOnes);
        s = Q6_Vw_vrmpyacc_VwVubRb(s, v2, kOnes);
        s = Q6_Vw_vrmpyacc_VwVubRb(s, v3, kOnes);
        sum[c] = s;
      } else {
        sum[c] = Q6_V_vzero();
      }
    }
    HVX_Vector s01 = Q6_Vh_vpack_VwVw_sat(sum[1], sum[0]);
    HVX_Vector s23 = Q6_Vh_vpack_VwVw_sat(sum[3], sum[2]);
    s01 = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(s01, veight), 4);
    s23 = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(s23, veight), 4);
    HVX_Vector out = Q6_Vub_vpack_VhVh_sat(s23, s01);
    vstoreu_partial(dst_ptr, out, w);
    r0 += 512;
    r1 += 512;
    r2 += 512;
    r3 += 512;
    dst_ptr += 128;
    dst_width -= 128;
  }
}

void ScaleARGBRowDown2_HVX(const uint8_t* src_argb,
                           ptrdiff_t src_stride,
                           uint8_t* dst_argb,
                           int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 32 ? 32 : dst_width;
    int src_bytes = w * 8;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_argb + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -4);
    vstoreu_partial(dst_argb, Q6_V_hi_W(d), w * 4);
    src_argb += 256;
    dst_argb += 128;
    dst_width -= 32;
  }
}

void ScaleARGBRowDown2Linear_HVX(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 32 ? 32 : dst_width;
    int src_bytes = w * 8;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_argb + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -4);
    HVX_Vector avg = Q6_Vub_vavg_VubVub_rnd(Q6_V_lo_W(d), Q6_V_hi_W(d));
    vstoreu_partial(dst_argb, avg, w * 4);
    src_argb += 256;
    dst_argb += 128;
    dst_width -= 32;
  }
}

void ScaleARGBRowDown2Box_HVX(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              uint8_t* dst_argb,
                              int dst_width) {
  const uint8_t* s = src_argb;
  const uint8_t* t = src_argb + src_stride;
  HVX_Vector vtwo = Q6_Vh_vsplat_R(2);
  while (dst_width > 0) {
    int w = dst_width >= 32 ? 32 : dst_width;
    int src_bytes = w * 8;
    HVX_Vector s0 = vloadu_partial_zero(s, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t0 = vloadu_partial_zero(t, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(s + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(t + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair ds = Q6_W_vdeal_VVR(s1, s0, -4);
    HVX_VectorPair dt = Q6_W_vdeal_VVR(t1, t0, -4);
    HVX_VectorPair s_even = Q6_Wuh_vunpack_Vub(Q6_V_lo_W(ds));
    HVX_VectorPair s_odd = Q6_Wuh_vunpack_Vub(Q6_V_hi_W(ds));
    HVX_VectorPair t_even = Q6_Wuh_vunpack_Vub(Q6_V_lo_W(dt));
    HVX_VectorPair t_odd = Q6_Wuh_vunpack_Vub(Q6_V_hi_W(dt));

    HVX_Vector sum_lo = Q6_Vh_vadd_VhVh(Q6_V_lo_W(s_even), Q6_V_lo_W(s_odd));
    sum_lo = Q6_Vh_vadd_VhVh(sum_lo, Q6_V_lo_W(t_even));
    sum_lo = Q6_Vh_vadd_VhVh(sum_lo, Q6_V_lo_W(t_odd));
    sum_lo = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum_lo, vtwo), 2);

    HVX_Vector sum_hi = Q6_Vh_vadd_VhVh(Q6_V_hi_W(s_even), Q6_V_hi_W(s_odd));
    sum_hi = Q6_Vh_vadd_VhVh(sum_hi, Q6_V_hi_W(t_even));
    sum_hi = Q6_Vh_vadd_VhVh(sum_hi, Q6_V_hi_W(t_odd));
    sum_hi = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum_hi, vtwo), 2);

    HVX_Vector out = Q6_Vub_vpack_VhVh_sat(sum_hi, sum_lo);
    vstoreu_partial(dst_argb, out, w * 4);
    s += 256;
    t += 256;
    dst_argb += 128;
    dst_width -= 32;
  }
}

void ScaleUVRowDown2_HVX(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_uv,
                         int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -2);
    vstoreu_partial(dst_uv, Q6_V_hi_W(d), w * 2);
    src_ptr += 256;
    dst_uv += 128;
    dst_width -= 64;
  }
}

void ScaleUVRowDown2Linear_HVX(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_uv,
                               int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_ptr, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_ptr + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -2);
    HVX_Vector avg = Q6_Vub_vavg_VubVub_rnd(Q6_V_lo_W(d), Q6_V_hi_W(d));
    vstoreu_partial(dst_uv, avg, w * 2);
    src_ptr += 256;
    dst_uv += 128;
    dst_width -= 64;
  }
}

void ScaleUVRowDown2Box_HVX(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_uv,
                            int dst_width) {
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  HVX_Vector vtwo = Q6_Vh_vsplat_R(2);
  while (dst_width > 0) {
    int w = dst_width >= 64 ? 64 : dst_width;
    int src_bytes = w * 4;
    HVX_Vector s0 = vloadu_partial_zero(s, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t0 = vloadu_partial_zero(t, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(s + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(t + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair ds = Q6_W_vdeal_VVR(s1, s0, -2);
    HVX_VectorPair dt = Q6_W_vdeal_VVR(t1, t0, -2);
    HVX_VectorPair s_even = Q6_Wuh_vunpack_Vub(Q6_V_lo_W(ds));
    HVX_VectorPair s_odd = Q6_Wuh_vunpack_Vub(Q6_V_hi_W(ds));
    HVX_VectorPair t_even = Q6_Wuh_vunpack_Vub(Q6_V_lo_W(dt));
    HVX_VectorPair t_odd = Q6_Wuh_vunpack_Vub(Q6_V_hi_W(dt));

    HVX_Vector sum_lo = Q6_Vh_vadd_VhVh(Q6_V_lo_W(s_even), Q6_V_lo_W(s_odd));
    sum_lo = Q6_Vh_vadd_VhVh(sum_lo, Q6_V_lo_W(t_even));
    sum_lo = Q6_Vh_vadd_VhVh(sum_lo, Q6_V_lo_W(t_odd));
    sum_lo = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum_lo, vtwo), 2);

    HVX_Vector sum_hi = Q6_Vh_vadd_VhVh(Q6_V_hi_W(s_even), Q6_V_hi_W(s_odd));
    sum_hi = Q6_Vh_vadd_VhVh(sum_hi, Q6_V_hi_W(t_even));
    sum_hi = Q6_Vh_vadd_VhVh(sum_hi, Q6_V_hi_W(t_odd));
    sum_hi = Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh(sum_hi, vtwo), 2);

    HVX_Vector out = Q6_Vub_vpack_VhVh_sat(sum_hi, sum_lo);
    vstoreu_partial(dst_uv, out, w * 2);
    s += 256;
    t += 256;
    dst_uv += 128;
    dst_width -= 64;
  }
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_HVX) && defined(__HVX__)
