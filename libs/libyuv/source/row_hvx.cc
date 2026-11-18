/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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

void CopyRow_HVX(const uint8_t* src, uint8_t* dst, int count) {
  while (count >= 128) {
    vstoreu(dst, vloadu(src));
    src += 128;
    dst += 128;
    count -= 128;
  }
  if (count > 0) {
    vstoreu_partial(dst, vloadu_partial_zero(src, count), count);
  }
}

void CopyRow_16_HVX(const uint16_t* src, uint16_t* dst, int count) {
  CopyRow_HVX((const uint8_t*)src, (uint8_t*)dst, count * 2);
}

void SetRow_HVX(uint8_t* dst, uint8_t v8, int width) {
  HVX_Vector v = Q6_Vb_vsplat_R(v8);
  while (width >= 128) {
    vstoreu(dst, v);
    dst += 128;
    width -= 128;
  }
  if (width > 0) {
    vstoreu_partial(dst, v, width);
  }
}

void ARGBSetRow_HVX(uint8_t* dst_argb, uint32_t v32, int width) {
  HVX_Vector v = Q6_V_vsplat_R(v32);
  int bytes = width * 4;
  while (bytes >= 128) {
    vstoreu(dst_argb, v);
    dst_argb += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    vstoreu_partial(dst_argb, v, bytes);
  }
}

void ARGBOrRow_HVX(const uint8_t* src_argb,
                   uint8_t* dst_argb,
                   uint32_t value,
                   int width) {
  HVX_Vector v = Q6_V_vsplat_R(value);
  int bytes = width * 4;
  while (bytes >= 128) {
    HVX_Vector s = vloadu(src_argb);
    vstoreu(dst_argb, Q6_V_vor_VV(s, v));
    src_argb += 128;
    dst_argb += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    HVX_Vector s = vloadu_partial_zero(src_argb, bytes);
    vstoreu_partial(dst_argb, Q6_V_vor_VV(s, v), bytes);
  }
}

void MirrorRow_HVX(const uint8_t* src, uint8_t* dst, int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x7F);
  src += width;
  while (width >= 128) {
    src -= 128;
    HVX_Vector v = vloadu(src);
    v = Q6_V_vdelta_VV(v, ctrl);
    vstoreu(dst, v);
    dst += 128;
    width -= 128;
  }
  if (width > 0) {
    src -= width;
    HVX_Vector v = vloadu_partial_zero(src, width);
    v = Q6_V_vdelta_VV(v, ctrl);
    v = Q6_V_valign_VVR(v, v, 128 - width);
    vstoreu_partial(dst, v, width);
  }
}

void MirrorRow_16_HVX(const uint16_t* src, uint16_t* dst, int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x7E);
  const uint8_t* s = (const uint8_t*)(src + width);
  uint8_t* d = (uint8_t*)dst;
  int bytes = width * 2;
  while (bytes >= 128) {
    s -= 128;
    HVX_Vector v = vloadu(s);
    v = Q6_V_vdelta_VV(v, ctrl);
    vstoreu(d, v);
    d += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    s -= bytes;
    HVX_Vector v = vloadu_partial_zero(s, bytes);
    v = Q6_V_vdelta_VV(v, ctrl);
    v = Q6_V_valign_VVR(v, v, 128 - bytes);
    vstoreu_partial(d, v, bytes);
  }
}

void MirrorUVRow_HVX(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  MirrorRow_16_HVX((const uint16_t*)src_uv, (uint16_t*)dst_uv, width);
}

void MirrorSplitUVRow_HVX(const uint8_t* src_uv,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x7F);
  src_uv += width * 2;
  while (width >= 128) {
    src_uv -= 256;
    HVX_Vector s0 = vloadu(src_uv);
    HVX_Vector s1 = vloadu(src_uv + 128);
    HVX_VectorPair uv = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_Vector u = Q6_V_vdelta_VV(Q6_V_lo_W(uv), ctrl);
    HVX_Vector v = Q6_V_vdelta_VV(Q6_V_hi_W(uv), ctrl);
    vstoreu(dst_u, u);
    vstoreu(dst_v, v);
    dst_u += 128;
    dst_v += 128;
    width -= 128;
  }
  if (width > 0) {
    int src_bytes = width * 2;
    src_uv -= src_bytes;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uv, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uv + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair uv = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_Vector u = Q6_V_vdelta_VV(Q6_V_lo_W(uv), ctrl);
    HVX_Vector v = Q6_V_vdelta_VV(Q6_V_hi_W(uv), ctrl);
    u = Q6_V_valign_VVR(u, u, 128 - width);
    v = Q6_V_valign_VVR(v, v, 128 - width);
    vstoreu_partial(dst_u, u, width);
    vstoreu_partial(dst_v, v, width);
  }
}

void ARGBMirrorRow_HVX(const uint8_t* src, uint8_t* dst, int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x7C);
  int bytes = width * 4;
  src += bytes;
  while (bytes >= 128) {
    src -= 128;
    HVX_Vector v = vloadu(src);
    v = Q6_V_vdelta_VV(v, ctrl);
    vstoreu(dst, v);
    dst += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    src -= bytes;
    HVX_Vector v = vloadu_partial_zero(src, bytes);
    v = Q6_V_vdelta_VV(v, ctrl);
    v = Q6_V_valign_VVR(v, v, 128 - bytes);
    vstoreu_partial(dst, v, bytes);
  }
}

void SplitUVRow_HVX(const uint8_t* src_uv,
                    uint8_t* dst_u,
                    uint8_t* dst_v,
                    int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uv, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uv + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair uv = Q6_W_vdeal_VVR(s1, s0, -1);
    vstoreu_partial(dst_u, Q6_V_lo_W(uv), w);
    vstoreu_partial(dst_v, Q6_V_hi_W(uv), w);
    src_uv += 256;
    dst_u += 128;
    dst_v += 128;
    width -= 128;
  }
}

void MergeUVRow_HVX(const uint8_t* src_u,
                    const uint8_t* src_v,
                    uint8_t* dst_uv,
                    int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector u = vloadu_partial_zero(src_u, w);
    HVX_Vector v = vloadu_partial_zero(src_v, w);
    HVX_VectorPair uv = Q6_W_vshuff_VVR(v, u, -1);
    int dst_bytes = w * 2;
    vstoreu_partial(dst_uv, Q6_V_lo_W(uv), dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst_uv + 128, Q6_V_hi_W(uv), dst_bytes - 128);
    }
    src_u += 128;
    src_v += 128;
    dst_uv += 256;
    width -= 128;
  }
}

void SplitUVRow_16_HVX(const uint16_t* src_uv,
                       uint16_t* dst_u,
                       uint16_t* dst_v,
                       int depth,
                       int width) {
  int shift = 16 - depth;
  while (width > 0) {
    int w = width >= 64 ? 64 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uv, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero((const uint8_t*)src_uv + 128, src_bytes - 128)
            : Q6_V_vzero();
    HVX_VectorPair uv = Q6_W_vdeal_VVR(s1, s0, -2);
    HVX_Vector u = Q6_Vuh_vlsr_VuhR(Q6_V_lo_W(uv), shift);
    HVX_Vector v = Q6_Vuh_vlsr_VuhR(Q6_V_hi_W(uv), shift);
    vstoreu_partial(dst_u, u, w * 2);
    vstoreu_partial(dst_v, v, w * 2);
    src_uv += 128;
    dst_u += 64;
    dst_v += 64;
    width -= 64;
  }
}

void MergeUVRow_16_HVX(const uint16_t* src_u,
                       const uint16_t* src_v,
                       uint16_t* dst_uv,
                       int depth,
                       int width) {
  int shift = 16 - depth;
  while (width > 0) {
    int w = width >= 64 ? 64 : width;
    HVX_Vector u = vloadu_partial_zero(src_u, w * 2);
    HVX_Vector v = vloadu_partial_zero(src_v, w * 2);
    u = Q6_Vh_vasl_VhR(u, shift);
    v = Q6_Vh_vasl_VhR(v, shift);
    HVX_VectorPair uv = Q6_W_vshuff_VVR(v, u, -2);
    int dst_bytes = w * 4;
    vstoreu_partial(dst_uv, Q6_V_lo_W(uv), dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial((uint8_t*)dst_uv + 128, Q6_V_hi_W(uv), dst_bytes - 128);
    }
    src_u += 64;
    src_v += 64;
    dst_uv += 128;
    width -= 64;
  }
}

void SplitARGBRow_HVX(const uint8_t* src_argb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      uint8_t* dst_a,
                      int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_argb + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_argb + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_argb + 384, src_bytes - 384)
                        : Q6_V_vzero();
    HVX_VectorPair p01 = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_VectorPair p23 = Q6_W_vdeal_VVR(s3, s2, -1);
    HVX_VectorPair br = Q6_W_vdeal_VVR(Q6_V_lo_W(p23), Q6_V_lo_W(p01), -1);
    HVX_VectorPair ga = Q6_W_vdeal_VVR(Q6_V_hi_W(p23), Q6_V_hi_W(p01), -1);
    vstoreu_partial(dst_b, Q6_V_lo_W(br), w);
    vstoreu_partial(dst_g, Q6_V_lo_W(ga), w);
    vstoreu_partial(dst_r, Q6_V_hi_W(br), w);
    vstoreu_partial(dst_a, Q6_V_hi_W(ga), w);
    src_argb += 512;
    dst_b += 128;
    dst_g += 128;
    dst_r += 128;
    dst_a += 128;
    width -= 128;
  }
}

void MergeARGBRow_HVX(const uint8_t* src_r,
                      const uint8_t* src_g,
                      const uint8_t* src_b,
                      const uint8_t* src_a,
                      uint8_t* dst_argb,
                      int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector b = vloadu_partial_zero(src_b, w);
    HVX_Vector g = vloadu_partial_zero(src_g, w);
    HVX_Vector r = vloadu_partial_zero(src_r, w);
    HVX_Vector a = vloadu_partial_zero(src_a, w);
    HVX_VectorPair br = Q6_W_vshuff_VVR(r, b, -1);
    HVX_VectorPair ga = Q6_W_vshuff_VVR(a, g, -1);
    HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ga), Q6_V_lo_W(br), -1);
    HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ga), Q6_V_hi_W(br), -1);
    int dst_bytes = w * 4;
    vstoreu_partial(dst_argb, Q6_V_lo_W(p01),
                    dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst_argb + 128, Q6_V_hi_W(p01),
                      dst_bytes > 256 ? 128 : dst_bytes - 128);
    }
    if (dst_bytes > 256) {
      vstoreu_partial(dst_argb + 256, Q6_V_lo_W(p23),
                      dst_bytes > 384 ? 128 : dst_bytes - 256);
    }
    if (dst_bytes > 384) {
      vstoreu_partial(dst_argb + 384, Q6_V_hi_W(p23), dst_bytes - 384);
    }
    src_b += 128;
    src_g += 128;
    src_r += 128;
    src_a += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void SplitXRGBRow_HVX(const uint8_t* src_argb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_argb + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_argb + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_argb + 384, src_bytes - 384)
                        : Q6_V_vzero();
    HVX_VectorPair p01 = Q6_W_vdeal_VVR(s1, s0, -1);
    HVX_VectorPair p23 = Q6_W_vdeal_VVR(s3, s2, -1);
    HVX_VectorPair br = Q6_W_vdeal_VVR(Q6_V_lo_W(p23), Q6_V_lo_W(p01), -1);
    HVX_VectorPair ga = Q6_W_vdeal_VVR(Q6_V_hi_W(p23), Q6_V_hi_W(p01), -1);
    vstoreu_partial(dst_b, Q6_V_lo_W(br), w);
    vstoreu_partial(dst_g, Q6_V_lo_W(ga), w);
    vstoreu_partial(dst_r, Q6_V_hi_W(br), w);
    src_argb += 512;
    dst_b += 128;
    dst_g += 128;
    dst_r += 128;
    width -= 128;
  }
}

void MergeXRGBRow_HVX(const uint8_t* src_r,
                      const uint8_t* src_g,
                      const uint8_t* src_b,
                      uint8_t* dst_argb,
                      int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector b = vloadu_partial_zero(src_b, w);
    HVX_Vector g = vloadu_partial_zero(src_g, w);
    HVX_Vector r = vloadu_partial_zero(src_r, w);
    HVX_VectorPair br = Q6_W_vshuff_VVR(r, b, -1);
    HVX_VectorPair ga = Q6_W_vshuff_VVR(a, g, -1);
    HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ga), Q6_V_lo_W(br), -1);
    HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ga), Q6_V_hi_W(br), -1);
    int dst_bytes = w * 4;
    vstoreu_partial(dst_argb, Q6_V_lo_W(p01),
                    dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst_argb + 128, Q6_V_hi_W(p01),
                      dst_bytes > 256 ? 128 : dst_bytes - 128);
    }
    if (dst_bytes > 256) {
      vstoreu_partial(dst_argb + 256, Q6_V_lo_W(p23),
                      dst_bytes > 384 ? 128 : dst_bytes - 256);
    }
    if (dst_bytes > 384) {
      vstoreu_partial(dst_argb + 384, Q6_V_hi_W(p23), dst_bytes - 384);
    }
    src_b += 128;
    src_g += 128;
    src_r += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void SwapUVRow_HVX(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x01);
  int bytes = width * 2;
  while (bytes >= 128) {
    HVX_Vector v = vloadu(src_uv);
    vstoreu(dst_vu, Q6_V_vdelta_VV(v, ctrl));
    src_uv += 128;
    dst_vu += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    HVX_Vector v = vloadu_partial_zero(src_uv, bytes);
    vstoreu_partial(dst_vu, Q6_V_vdelta_VV(v, ctrl), bytes);
  }
}

void InterpolateRow_HVX(uint8_t* dst_ptr,
                        const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        int width,
                        int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  if (y1_fraction == 0) {
    CopyRow_HVX(src_ptr, dst_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    while (width >= 128) {
      HVX_Vector s0 = vloadu(src_ptr);
      HVX_Vector s1 = vloadu(src_ptr1);
      vstoreu(dst_ptr, Q6_Vub_vavg_VubVub_rnd(s0, s1));
      src_ptr += 128;
      src_ptr1 += 128;
      dst_ptr += 128;
      width -= 128;
    }
    if (width > 0) {
      HVX_Vector s0 = vloadu_partial_zero(src_ptr, width);
      HVX_Vector s1 = vloadu_partial_zero(src_ptr1, width);
      vstoreu_partial(dst_ptr, Q6_Vub_vavg_VubVub_rnd(s0, s1), width);
    }
    return;
  }
  uint32_t y0_32 = (uint32_t)y0_fraction * 0x01010101u;
  uint32_t y1_32 = (uint32_t)y1_fraction * 0x01010101u;
  HVX_Vector v128 = Q6_Vh_vsplat_R(128);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector s0 = vloadu_partial_zero(src_ptr, w);
    HVX_Vector s1 = vloadu_partial_zero(src_ptr1, w);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubRub(s0, y0_32);
    p = Q6_Wuh_vmpyacc_WuhVubRub(p, s1, y1_32);
    HVX_Vector lo = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(p), v128);
    HVX_Vector hi = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(p), v128);
    HVX_Vector out = Q6_Vb_vshuffo_VbVb(hi, lo);
    vstoreu_partial(dst_ptr, out, w);
    src_ptr += 128;
    src_ptr1 += 128;
    dst_ptr += 128;
    width -= 128;
  }
}

void InterpolateRow_16_HVX(uint16_t* dst_ptr,
                           const uint16_t* src_ptr,
                           ptrdiff_t src_stride,
                           int width,
                           int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  if (y1_fraction == 0) {
    CopyRow_16_HVX(src_ptr, dst_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    int bytes = width * 2;
    const uint8_t* s0_p = (const uint8_t*)src_ptr;
    const uint8_t* s1_p = (const uint8_t*)src_ptr1;
    uint8_t* d_p = (uint8_t*)dst_ptr;
    while (bytes >= 128) {
      HVX_Vector s0 = vloadu(s0_p);
      HVX_Vector s1 = vloadu(s1_p);
      vstoreu(d_p, Q6_Vuh_vavg_VuhVuh_rnd(s0, s1));
      s0_p += 128;
      s1_p += 128;
      d_p += 128;
      bytes -= 128;
    }
    if (bytes > 0) {
      HVX_Vector s0 = vloadu_partial_zero(s0_p, bytes);
      HVX_Vector s1 = vloadu_partial_zero(s1_p, bytes);
      vstoreu_partial(d_p, Q6_Vuh_vavg_VuhVuh_rnd(s0, s1), bytes);
    }
    return;
  }
  uint32_t y0_32 = ((uint32_t)y0_fraction << 16) | (uint32_t)y0_fraction;
  uint32_t y1_32 = ((uint32_t)y1_fraction << 16) | (uint32_t)y1_fraction;
  HVX_Vector v128 = Q6_V_vsplat_R(128);
  while (width > 0) {
    int w = width >= 64 ? 64 : width;
    int bytes = w * 2;
    HVX_Vector s0 = vloadu_partial_zero(src_ptr, bytes);
    HVX_Vector s1 = vloadu_partial_zero(src_ptr1, bytes);
    HVX_VectorPair p = Q6_Wuw_vmpy_VuhRuh(s0, y0_32);
    p = Q6_Wuw_vmpyacc_WuwVuhRuh(p, s1, y1_32);
    HVX_Vector lo = Q6_Vuw_vlsr_VuwR(Q6_Vw_vadd_VwVw(Q6_V_lo_W(p), v128), 8);
    HVX_Vector hi = Q6_Vuw_vlsr_VuwR(Q6_Vw_vadd_VwVw(Q6_V_hi_W(p), v128), 8);
    HVX_Vector out = Q6_Vh_vshuffe_VhVh(hi, lo);
    vstoreu_partial(dst_ptr, out, bytes);
    src_ptr += 64;
    src_ptr1 += 64;
    dst_ptr += 64;
    width -= 64;
  }
}

void BlendPlaneRow_HVX(const uint8_t* src0,
                       const uint8_t* src1,
                       const uint8_t* alpha,
                       uint8_t* dst,
                       int width) {
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector f = vloadu_partial_zero(src0, w);
    HVX_Vector b = vloadu_partial_zero(src1, w);
    HVX_Vector a = vloadu_partial_zero(alpha, w);
    HVX_Vector not_a = Q6_V_vnot_V(a);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubVub(f, a);
    p = Q6_Wuh_vmpyacc_WuhVubVub(p, b, not_a);
    HVX_Vector lo = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(p), v255);
    HVX_Vector hi = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(p), v255);
    HVX_Vector out = Q6_Vb_vshuffo_VbVb(hi, lo);
    vstoreu_partial(dst, out, w);
    src0 += 128;
    src1 += 128;
    alpha += 128;
    dst += 128;
    width -= 128;
  }
}

void ARGBBlendRow_HVX(const uint8_t* src_argb,
                      const uint8_t* src_argb1,
                      uint8_t* dst_argb,
                      int width) {
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  HVX_Vector v_alpha_mask = Q6_V_vsplat_R(0xFF000000u);
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector fg = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector bg = vloadu_partial_zero(src_argb1, b_cnt);
    HVX_Vector t = Q6_Vb_vshuffo_VbVb(fg, fg);
    HVX_Vector a_vec = Q6_Vh_vshuffo_VhVh(t, t);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubVub(bg, a_vec);
    HVX_Vector lo = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(p), v255);
    HVX_Vector hi = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(p), v255);
    HVX_Vector atten_bg = Q6_Vb_vshuffo_VbVb(hi, lo);
    HVX_Vector scaled_bg = Q6_Vub_vsub_VubVub_sat(bg, atten_bg);
    HVX_Vector out = Q6_Vub_vadd_VubVub_sat(scaled_bg, fg);
    out = Q6_V_vor_VV(out, v_alpha_mask);
    vstoreu_partial(dst_argb, out, b_cnt);
    src_argb += 128;
    src_argb1 += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBAlphaBlendRow_HVX(const uint8_t* src_argb0,
                           const uint8_t* src_argb1,
                           uint8_t* dst_argb,
                           int width) {
  HVX_Vector vzero = Q6_V_vzero();
  HVX_Vector v0001 = Q6_Vh_vsplat_R(1);
  HVX_Vector v00ff = Q6_Vh_vsplat_R(255);
  HVX_Vector vffff = Q6_Vh_vsplat_R(-1);
  HVX_Vector v_rgb_mask = Q6_V_vsplat_R(0x00FFFFFFu);
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector fg = vloadu_partial_zero(src_argb0, b_cnt);
    HVX_Vector bg = vloadu_partial_zero(src_argb1, b_cnt);

    // Broadcast sa and da to all 4 byte lanes of each 32-bit pixel.
    HVX_Vector t_fg = Q6_Vb_vshuffo_VbVb(fg, fg);
    HVX_Vector sa_vec = Q6_Vh_vshuffo_VhVh(t_fg, t_fg);
    HVX_Vector t_bg = Q6_Vb_vshuffo_VbVb(bg, bg);
    HVX_Vector da_vec = Q6_Vh_vshuffo_VhVh(t_bg, t_bg);

    // Both uint16_t halfwords of each pixel hold sa and da (0..255).
    HVX_Vector sa16 = Q6_V_vand_VV(sa_vec, v00ff);
    HVX_Vector da16 = Q6_V_vand_VV(da_vec, v00ff);

    // x = da * sa in both uint16_t halfwords of each pixel.
    HVX_Vector x = Q6_V_lo_W(Q6_Wuh_vmpy_VubVub(da_vec, sa_vec));
    // prod_div255 = (x + 1 + (x >> 8)) >> 8.
    HVX_Vector prod_div255 = Q6_Vuh_vlsr_VuhR(
        Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(x, v0001), Q6_Vuh_vlsr_VuhR(x, 8)), 8);
    // out_a = da + sa - (da * sa) / 255.
    HVX_Vector out_a16 =
        Q6_Vh_vadd_VhVh(da16, Q6_Vh_vsub_VhVh(sa16, prod_div255));

    // 8-step restoring division: ratio = (da == 0) ? 255 : (sa * 255) / out_a.
    HVX_Vector rem = Q6_Vh_vsub_VhVh(Q6_Vh_vasl_VhR(sa16, 8), sa16);
    HVX_VectorPred q_da_zero = Q6_Q_vcmp_eq_VhVh(da16, vzero);
    rem = Q6_V_vmux_QVV(q_da_zero, vffff, rem);
    HVX_Vector ratio = vzero;

    for (int bit = 7; bit >= 0; --bit) {
      HVX_Vector d_sh = Q6_Vh_vasl_VhR(out_a16, bit);
      HVX_VectorPred q_gt = Q6_Q_vcmp_gt_VuhVuh(d_sh, rem);
      rem = Q6_V_vmux_QVV(q_gt, rem, Q6_Vh_vsub_VhVh(rem, d_sh));
      ratio = Q6_V_vmux_QVV(q_gt, ratio,
                            Q6_V_vor_VV(ratio, Q6_Vb_vsplat_R(1 << bit)));
    }

    // Exact RGB lerp: (bg * (255 - ratio) + fg * ratio) / 255.
    HVX_Vector inv_ratio = Q6_V_vnot_V(ratio);
    HVX_VectorPair p_rgb = Q6_Wuh_vmpy_VubVub(fg, ratio);
    p_rgb = Q6_Wuh_vmpyacc_WuhVubVub(p_rgb, bg, inv_ratio);
    HVX_Vector lo = Q6_V_lo_W(p_rgb);
    HVX_Vector hi = Q6_V_hi_W(p_rgb);
    lo = Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(lo, v0001), Q6_Vuh_vlsr_VuhR(lo, 8));
    hi = Q6_Vh_vadd_VhVh(Q6_Vh_vadd_VhVh(hi, v0001), Q6_Vuh_vlsr_VuhR(hi, 8));
    HVX_Vector out = Q6_Vb_vshuffo_VbVb(hi, lo);
    out =
        Q6_V_vor_VV(Q6_V_vand_VV(out, v_rgb_mask), Q6_Vw_vasl_VwR(out_a16, 24));
    vstoreu_partial(dst_argb, out, b_cnt);
    src_argb0 += 128;
    src_argb1 += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBAttenuateRow_HVX(const uint8_t* src_argb,
                          uint8_t* dst_argb,
                          int width) {
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  HVX_Vector v_rgb_mask = Q6_V_vsplat_R(0x00FFFFFFu);
  HVX_Vector v_a_mask = Q6_V_vsplat_R(0xFF000000u);
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector src = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector t = Q6_Vb_vshuffo_VbVb(src, src);
    HVX_Vector a_vec = Q6_Vh_vshuffo_VhVh(t, t);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubVub(src, a_vec);
    HVX_Vector lo = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(p), v255);
    HVX_Vector hi = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(p), v255);
    HVX_Vector out = Q6_Vb_vshuffo_VbVb(hi, lo);
    out =
        Q6_V_vor_VV(Q6_V_vand_VV(out, v_rgb_mask), Q6_V_vand_VV(src, v_a_mask));
    vstoreu_partial(dst_argb, out, b_cnt);
    src_argb += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBMultiplyRow_HVX(const uint8_t* src_argb,
                         const uint8_t* src_argb1,
                         uint8_t* dst_argb,
                         int width) {
  HVX_Vector v128 = Q6_Vh_vsplat_R(128);
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector s0 = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector s1 = vloadu_partial_zero(src_argb1, b_cnt);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubVub(s0, s1);
    HVX_Vector lo = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(p), v128);
    HVX_Vector hi = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(p), v128);
    HVX_Vector out = Q6_Vb_vshuffo_VbVb(hi, lo);
    vstoreu_partial(dst_argb, out, b_cnt);
    src_argb += 128;
    src_argb1 += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBAddRow_HVX(const uint8_t* src_argb,
                    const uint8_t* src_argb1,
                    uint8_t* dst_argb,
                    int width) {
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector s0 = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector s1 = vloadu_partial_zero(src_argb1, b_cnt);
    vstoreu_partial(dst_argb, Q6_Vub_vadd_VubVub_sat(s0, s1), b_cnt);
    src_argb += 128;
    src_argb1 += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBSubtractRow_HVX(const uint8_t* src_argb,
                         const uint8_t* src_argb1,
                         uint8_t* dst_argb,
                         int width) {
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector s0 = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector s1 = vloadu_partial_zero(src_argb1, b_cnt);
    vstoreu_partial(dst_argb, Q6_Vub_vsub_VubVub_sat(s0, s1), b_cnt);
    src_argb += 128;
    src_argb1 += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBExtractAlphaRow_HVX(const uint8_t* src_argb,
                             uint8_t* dst_a,
                             int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_argb + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_argb + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_argb + 384, src_bytes - 384)
                        : Q6_V_vzero();
    HVX_Vector ga01 = Q6_V_hi_W(Q6_W_vdeal_VVR(s1, s0, -1));
    HVX_Vector ga23 = Q6_V_hi_W(Q6_W_vdeal_VVR(s3, s2, -1));
    HVX_Vector a = Q6_V_hi_W(Q6_W_vdeal_VVR(ga23, ga01, -1));
    vstoreu_partial(dst_a, a, w);
    src_argb += 512;
    dst_a += 128;
    width -= 128;
  }
}

void ARGBCopyAlphaRow_HVX(const uint8_t* src, uint8_t* dst, int width) {
  HVX_Vector v_rgb_mask = Q6_V_vsplat_R(0x00FFFFFFu);
  HVX_Vector v_a_mask = Q6_V_vsplat_R(0xFF000000u);
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector s = vloadu_partial_zero(src, b_cnt);
    HVX_Vector d = vloadu_partial_zero(dst, b_cnt);
    HVX_Vector out =
        Q6_V_vor_VV(Q6_V_vand_VV(d, v_rgb_mask), Q6_V_vand_VV(s, v_a_mask));
    vstoreu_partial(dst, out, b_cnt);
    src += 128;
    dst += 128;
    bytes -= 128;
  }
}

void ARGBCopyYToAlphaRow_HVX(const uint8_t* src, uint8_t* dst, int width) {
  HVX_Vector v_rgb_mask = Q6_V_vsplat_R(0x00FFFFFFu);
  HVX_Vector z = Q6_V_vzero();
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector y = vloadu_partial_zero(src, w);
    HVX_VectorPair ga = Q6_W_vshuff_VVR(y, z, -1);
    HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ga), z, -1);
    HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ga), z, -1);
    int dst_bytes = w * 4;
    int b0 = dst_bytes > 128 ? 128 : dst_bytes;
    HVX_Vector d0 = vloadu_partial_zero(dst, b0);
    vstoreu_partial(
        dst, Q6_V_vor_VV(Q6_V_vand_VV(d0, v_rgb_mask), Q6_V_lo_W(p01)), b0);
    if (dst_bytes > 128) {
      int b1 = dst_bytes > 256 ? 128 : dst_bytes - 128;
      HVX_Vector d1 = vloadu_partial_zero(dst + 128, b1);
      vstoreu_partial(dst + 128,
                      Q6_V_vor_VV(Q6_V_vand_VV(d1, v_rgb_mask), Q6_V_hi_W(p01)),
                      b1);
    }
    if (dst_bytes > 256) {
      int b2 = dst_bytes > 384 ? 128 : dst_bytes - 256;
      HVX_Vector d2 = vloadu_partial_zero(dst + 256, b2);
      vstoreu_partial(dst + 256,
                      Q6_V_vor_VV(Q6_V_vand_VV(d2, v_rgb_mask), Q6_V_lo_W(p23)),
                      b2);
    }
    if (dst_bytes > 384) {
      int b3 = dst_bytes - 384;
      HVX_Vector d3 = vloadu_partial_zero(dst + 384, b3);
      vstoreu_partial(dst + 384,
                      Q6_V_vor_VV(Q6_V_vand_VV(d3, v_rgb_mask), Q6_V_hi_W(p23)),
                      b3);
    }
    src += 128;
    dst += 512;
    width -= 128;
  }
}

void Convert16To8Row_HVX(const uint16_t* src_y,
                         uint8_t* dst_y,
                         int scale,
                         int width) {
  uint32_t scale32 = ((uint32_t)(uint16_t)scale << 16) | (uint16_t)scale;
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_y, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero((const uint8_t*)src_y + 128, src_bytes - 128)
            : Q6_V_vzero();
    HVX_VectorPair p0 = Q6_Wuw_vmpy_VuhRuh(s0, scale32);
    HVX_VectorPair p1 = Q6_Wuw_vmpy_VuhRuh(s1, scale32);
    HVX_Vector h0 = Q6_Vuh_vmin_VuhVuh(
        Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p0), Q6_V_lo_W(p0)), v255);
    HVX_Vector h1 = Q6_Vuh_vmin_VuhVuh(
        Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p1), Q6_V_lo_W(p1)), v255);
    HVX_Vector out = Q6_Vb_vpacke_VhVh(h1, h0);
    vstoreu_partial(dst_y, out, w);
    src_y += 128;
    dst_y += 128;
    width -= 128;
  }
}

void Convert8To16Row_HVX(const uint8_t* src_y,
                         uint16_t* dst_y,
                         int bits,
                         int width) {
  int shift = 16 - bits;
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector s = vloadu_partial_zero(src_y, w);
    HVX_VectorPair d16 = Q6_W_vshuff_VVR(s, s, -1);
    HVX_Vector d0 = Q6_Vuh_vlsr_VuhR(Q6_V_lo_W(d16), shift);
    HVX_Vector d1 = Q6_Vuh_vlsr_VuhR(Q6_V_hi_W(d16), shift);
    int dst_bytes = w * 2;
    vstoreu_partial(dst_y, d0, dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial((uint8_t*)dst_y + 128, d1, dst_bytes - 128);
    }
    src_y += 128;
    dst_y += 128;
    width -= 128;
  }
}

void Convert8To8Row_HVX(const uint8_t* src_y,
                        uint8_t* dst_y,
                        int scale,
                        int bias,
                        int width) {
  uint32_t scale32 = (uint32_t)(uint8_t)scale * 0x01010101u;
  HVX_Vector vbias = Q6_Vb_vsplat_R(bias);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector s = vloadu_partial_zero(src_y, w);
    HVX_VectorPair p = Q6_Wuh_vmpy_VubRub(s, scale32);
    HVX_Vector out =
        Q6_Vb_vadd_VbVb(Q6_Vb_vshuffo_VbVb(Q6_V_hi_W(p), Q6_V_lo_W(p)), vbias);
    vstoreu_partial(dst_y, out, w);
    src_y += 128;
    dst_y += 128;
    width -= 128;
  }
}

void MultiplyRow_16_HVX(const uint16_t* src_y,
                        uint16_t* dst_y,
                        int scale,
                        int width) {
  uint32_t scale32 = ((uint32_t)(uint16_t)scale << 16) | (uint16_t)scale;
  while (width > 0) {
    int w = width >= 64 ? 64 : width;
    int bytes = w * 2;
    HVX_Vector s = vloadu_partial_zero(src_y, bytes);
    HVX_VectorPair p = Q6_Wuw_vmpy_VuhRuh(s, scale32);
    HVX_Vector out = Q6_Vh_vshuffe_VhVh(Q6_V_hi_W(p), Q6_V_lo_W(p));
    vstoreu_partial(dst_y, out, bytes);
    src_y += 64;
    dst_y += 64;
    width -= 64;
  }
}

void DivideRow_16_HVX(const uint16_t* src_y,
                      uint16_t* dst_y,
                      int scale,
                      int width) {
  if (scale == 65536) {  // Divide by 1, so do memcpy.
    if (src_y != dst_y) {
      memcpy(dst_y, src_y, (size_t)width * 2);
    }
    return;
  }
  uint32_t scale32 = ((uint32_t)(uint16_t)scale << 16) | (uint16_t)scale;
  while (width > 0) {
    int w = width >= 64 ? 64 : width;
    int bytes = w * 2;
    HVX_Vector s = vloadu_partial_zero(src_y, bytes);
    HVX_VectorPair p = Q6_Wuw_vmpy_VuhRuh(s, scale32);
    HVX_Vector out = Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p), Q6_V_lo_W(p));
    vstoreu_partial(dst_y, out, bytes);
    src_y += 64;
    dst_y += 64;
    width -= 64;
  }
}

void HalfRow_16To8_HVX(const uint16_t* src_uv,
                       ptrdiff_t src_uv_stride,
                       uint8_t* dst_uv,
                       int scale,
                       int width) {
  uint32_t scale32 = ((uint32_t)(uint16_t)scale << 16) | (uint16_t)scale;
  HVX_Vector v255 = Q6_Vh_vsplat_R(255);
  const uint16_t* src_uv1 = src_uv + src_uv_stride;
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uv, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero((const uint8_t*)src_uv + 128, src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector t0 =
        vloadu_partial_zero(src_uv1, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero((const uint8_t*)src_uv1 + 128,
                                              src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector a0 = Q6_Vuh_vavg_VuhVuh_rnd(s0, t0);
    HVX_Vector a1 = Q6_Vuh_vavg_VuhVuh_rnd(s1, t1);
    HVX_VectorPair p0 = Q6_Wuw_vmpy_VuhRuh(a0, scale32);
    HVX_VectorPair p1 = Q6_Wuw_vmpy_VuhRuh(a1, scale32);
    HVX_Vector h0 = Q6_Vuh_vmin_VuhVuh(
        Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p0), Q6_V_lo_W(p0)), v255);
    HVX_Vector h1 = Q6_Vuh_vmin_VuhVuh(
        Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p1), Q6_V_lo_W(p1)), v255);
    HVX_Vector out = Q6_Vb_vpacke_VhVh(h1, h0);
    vstoreu_partial(dst_uv, out, w);
    src_uv += 128;
    src_uv1 += 128;
    dst_uv += 128;
    width -= 128;
  }
}

void YUY2ToYRow_HVX(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_yuy2, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_yuy2 + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -1);
    vstoreu_partial(dst_y, Q6_V_lo_W(d), w);
    src_yuy2 += 256;
    dst_y += 128;
    width -= 128;
  }
}

void YUY2ToUV422Row_HVX(const uint8_t* src_yuy2,
                        uint8_t* dst_u,
                        uint8_t* dst_v,
                        int width) {
  int uv_width = (width + 1) >> 1;
  while (uv_width > 0) {
    int w = uv_width >= 64 ? 64 : uv_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_yuy2, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_yuy2 + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector uv = Q6_V_hi_W(Q6_W_vdeal_VVR(s1, s0, -1));
    HVX_VectorPair u_v = Q6_W_vdeal_VVR(uv, uv, -1);
    vstoreu_partial(dst_u, Q6_V_lo_W(u_v), w);
    vstoreu_partial(dst_v, Q6_V_hi_W(u_v), w);
    src_yuy2 += 256;
    dst_u += 64;
    dst_v += 64;
    uv_width -= 64;
  }
}

void YUY2ToUVRow_HVX(const uint8_t* src_yuy2,
                     int src_stride_yuy2,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  const uint8_t* src_yuy2_1 = src_yuy2 + src_stride_yuy2;
  int uv_width = (width + 1) >> 1;
  while (uv_width > 0) {
    int w = uv_width >= 64 ? 64 : uv_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_yuy2, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_yuy2 + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t0 =
        vloadu_partial_zero(src_yuy2_1, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(src_yuy2_1 + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector a0 = Q6_Vub_vavg_VubVub_rnd(s0, t0);
    HVX_Vector a1 = Q6_Vub_vavg_VubVub_rnd(s1, t1);
    HVX_Vector uv = Q6_V_hi_W(Q6_W_vdeal_VVR(a1, a0, -1));
    HVX_VectorPair u_v = Q6_W_vdeal_VVR(uv, uv, -1);
    vstoreu_partial(dst_u, Q6_V_lo_W(u_v), w);
    vstoreu_partial(dst_v, Q6_V_hi_W(u_v), w);
    src_yuy2 += 256;
    src_yuy2_1 += 256;
    dst_u += 64;
    dst_v += 64;
    uv_width -= 64;
  }
}

void UYVYToYRow_HVX(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 2;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uyvy, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uyvy + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_VectorPair d = Q6_W_vdeal_VVR(s1, s0, -1);
    vstoreu_partial(dst_y, Q6_V_hi_W(d), w);
    src_uyvy += 256;
    dst_y += 128;
    width -= 128;
  }
}

void UYVYToUV422Row_HVX(const uint8_t* src_uyvy,
                        uint8_t* dst_u,
                        uint8_t* dst_v,
                        int width) {
  int uv_width = (width + 1) >> 1;
  while (uv_width > 0) {
    int w = uv_width >= 64 ? 64 : uv_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uyvy, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uyvy + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector uv = Q6_V_lo_W(Q6_W_vdeal_VVR(s1, s0, -1));
    HVX_VectorPair u_v = Q6_W_vdeal_VVR(uv, uv, -1);
    vstoreu_partial(dst_u, Q6_V_lo_W(u_v), w);
    vstoreu_partial(dst_v, Q6_V_hi_W(u_v), w);
    src_uyvy += 256;
    dst_u += 64;
    dst_v += 64;
    uv_width -= 64;
  }
}

void UYVYToUVRow_HVX(const uint8_t* src_uyvy,
                     int src_stride_uyvy,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  const uint8_t* src_uyvy_1 = src_uyvy + src_stride_uyvy;
  int uv_width = (width + 1) >> 1;
  while (uv_width > 0) {
    int w = uv_width >= 64 ? 64 : uv_width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_uyvy, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uyvy + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t0 =
        vloadu_partial_zero(src_uyvy_1, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(src_uyvy_1 + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector a0 = Q6_Vub_vavg_VubVub_rnd(s0, t0);
    HVX_Vector a1 = Q6_Vub_vavg_VubVub_rnd(s1, t1);
    HVX_Vector uv = Q6_V_lo_W(Q6_W_vdeal_VVR(a1, a0, -1));
    HVX_VectorPair u_v = Q6_W_vdeal_VVR(uv, uv, -1);
    vstoreu_partial(dst_u, Q6_V_lo_W(u_v), w);
    vstoreu_partial(dst_v, Q6_V_hi_W(u_v), w);
    src_uyvy += 256;
    src_uyvy_1 += 256;
    dst_u += 64;
    dst_v += 64;
    uv_width -= 64;
  }
}

void J400ToARGBRow_HVX(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_VectorPair yy = Q6_W_vshuff_VVR(y, y, -1);
    HVX_VectorPair ya = Q6_W_vshuff_VVR(a, y, -1);
    HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ya), Q6_V_lo_W(yy), -1);
    HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ya), Q6_V_hi_W(yy), -1);
    int dst_bytes = w * 4;
    vstoreu_partial(dst_argb, Q6_V_lo_W(p01),
                    dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst_argb + 128, Q6_V_hi_W(p01),
                      dst_bytes > 256 ? 128 : dst_bytes - 128);
    }
    if (dst_bytes > 256) {
      vstoreu_partial(dst_argb + 256, Q6_V_lo_W(p23),
                      dst_bytes > 384 ? 128 : dst_bytes - 256);
    }
    if (dst_bytes > 384) {
      vstoreu_partial(dst_argb + 384, Q6_V_hi_W(p23), dst_bytes - 384);
    }
    src_y += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void I400ToARGBRow_HVX(const uint8_t* src_y,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  uint16_t yg = (uint16_t)yuvconstants->kYToRgb[0];
  int16_t yb = yuvconstants->kYBiasToRgb[0];
  uint32_t yg32 = ((uint32_t)yg << 16) | yg;
  HVX_Vector v_yb = Q6_Vh_vsplat_R(yb);
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector y_even_0101 = Q6_Vb_vshuffe_VbVb(y, y);
    HVX_Vector y_odd_0101 = Q6_Vb_vshuffo_VbVb(y, y);
    HVX_VectorPair p_even = Q6_Wuw_vmpy_VuhRuh(y_even_0101, yg32);
    HVX_VectorPair p_odd = Q6_Wuw_vmpy_VuhRuh(y_odd_0101, yg32);
    HVX_Vector y_lo = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(
            Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p_even), Q6_V_lo_W(p_even)), v_yb),
        6);
    HVX_Vector y_hi = Q6_Vh_vasr_VhR(
        Q6_Vh_vadd_VhVh(Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p_odd), Q6_V_lo_W(p_odd)),
                        v_yb),
        6);
    HVX_Vector y_out = Q6_Vub_vsat_VhVh(y_hi, y_lo);
    HVX_VectorPair yy = Q6_W_vshuff_VVR(y_out, y_out, -1);
    HVX_VectorPair ya = Q6_W_vshuff_VVR(a, y_out, -1);
    HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ya), Q6_V_lo_W(yy), -1);
    HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ya), Q6_V_hi_W(yy), -1);
    int dst_bytes = w * 4;
    vstoreu_partial(dst_argb, Q6_V_lo_W(p01),
                    dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst_argb + 128, Q6_V_hi_W(p01),
                      dst_bytes > 256 ? 128 : dst_bytes - 128);
    }
    if (dst_bytes > 256) {
      vstoreu_partial(dst_argb + 256, Q6_V_lo_W(p23),
                      dst_bytes > 384 ? 128 : dst_bytes - 256);
    }
    if (dst_bytes > 384) {
      vstoreu_partial(dst_argb + 384, Q6_V_hi_W(p23), dst_bytes - 384);
    }
    src_y += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void ARGBToABGRRow_HVX(const uint8_t* src_argb, uint8_t* dst_abgr, int width) {
  HVX_Vector ctrl = Q6_V_vsplat_R(0x00020002);
  int bytes = width * 4;
  while (bytes >= 128) {
    vstoreu(dst_abgr, Q6_V_vdelta_VV(vloadu(src_argb), ctrl));
    src_argb += 128;
    dst_abgr += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    vstoreu_partial(dst_abgr,
                    Q6_V_vdelta_VV(vloadu_partial_zero(src_argb, bytes), ctrl),
                    bytes);
  }
}

void ARGBToBGRARow_HVX(const uint8_t* src_argb, uint8_t* dst_bgra, int width) {
  HVX_Vector ctrl = Q6_Vb_vsplat_R(0x03);
  int bytes = width * 4;
  while (bytes >= 128) {
    vstoreu(dst_bgra, Q6_V_vdelta_VV(vloadu(src_argb), ctrl));
    src_argb += 128;
    dst_bgra += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    vstoreu_partial(dst_bgra,
                    Q6_V_vdelta_VV(vloadu_partial_zero(src_argb, bytes), ctrl),
                    bytes);
  }
}

void ARGBToRGBARow_HVX(const uint8_t* src_argb, uint8_t* dst_rgba, int width) {
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector v = vloadu_partial_zero(src_argb, b_cnt);
    HVX_Vector out = Q6_V_vor_VV(Q6_Vw_vasl_VwR(v, 8), Q6_Vuw_vlsr_VuwR(v, 24));
    vstoreu_partial(dst_rgba, out, b_cnt);
    src_argb += 128;
    dst_rgba += 128;
    bytes -= 128;
  }
}

void RGBAToARGBRow_HVX(const uint8_t* src_rgba, uint8_t* dst_argb, int width) {
  int bytes = width * 4;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector v = vloadu_partial_zero(src_rgba, b_cnt);
    HVX_Vector out = Q6_V_vor_VV(Q6_Vuw_vlsr_VuwR(v, 8), Q6_Vw_vasl_VwR(v, 24));
    vstoreu_partial(dst_argb, out, b_cnt);
    src_rgba += 128;
    dst_argb += 128;
    bytes -= 128;
  }
}

void ARGBToAR64Row_HVX(const uint8_t* src_argb, uint16_t* dst_ar64, int width) {
  Convert8To16Row_HVX(src_argb, dst_ar64, 16, width * 4);
}

void ARGBToAB64Row_HVX(const uint8_t* src_argb, uint16_t* dst_ab64, int width) {
  HVX_Vector ctrl = Q6_V_vsplat_R(0x00020002);
  int bytes = width * 4;
  uint8_t* dst = (uint8_t*)dst_ab64;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    HVX_Vector v = Q6_V_vdelta_VV(vloadu_partial_zero(src_argb, b_cnt), ctrl);
    HVX_VectorPair d16 = Q6_W_vshuff_VVR(v, v, -1);
    int dst_bytes = b_cnt * 2;
    vstoreu_partial(dst, Q6_V_lo_W(d16), dst_bytes > 128 ? 128 : dst_bytes);
    if (dst_bytes > 128) {
      vstoreu_partial(dst + 128, Q6_V_hi_W(d16), dst_bytes - 128);
    }
    src_argb += 128;
    dst += 256;
    bytes -= 128;
  }
}

void AR64ToARGBRow_HVX(const uint16_t* src_ar64, uint8_t* dst_argb, int width) {
  int bytes = width * 4;
  const uint8_t* src = (const uint8_t*)src_ar64;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    int src_bytes = b_cnt * 2;
    HVX_Vector s0 = vloadu_partial_zero(src, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src + 128, src_bytes - 128)
                        : Q6_V_vzero();
    vstoreu_partial(dst_argb, Q6_Vb_vpacko_VhVh(s1, s0), b_cnt);
    src += 256;
    dst_argb += 128;
    bytes -= 128;
  }
}

void AB64ToARGBRow_HVX(const uint16_t* src_ab64, uint8_t* dst_argb, int width) {
  HVX_Vector ctrl = Q6_V_vsplat_R(0x00020002);
  int bytes = width * 4;
  const uint8_t* src = (const uint8_t*)src_ab64;
  while (bytes > 0) {
    int b_cnt = bytes >= 128 ? 128 : bytes;
    int src_bytes = b_cnt * 2;
    HVX_Vector s0 = vloadu_partial_zero(src, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector out = Q6_V_vdelta_VV(Q6_Vb_vpacko_VhVh(s1, s0), ctrl);
    vstoreu_partial(dst_argb, out, b_cnt);
    src += 256;
    dst_argb += 128;
    bytes -= 128;
  }
}

void AR64ToAB64Row_HVX(const uint16_t* src_ar64,
                       uint16_t* dst_ab64,
                       int width) {
  HVX_Vector ctrl = Q6_V_vsplat_R(0x00000404);
  int bytes = width * 8;
  const uint8_t* src = (const uint8_t*)src_ar64;
  uint8_t* dst = (uint8_t*)dst_ab64;
  while (bytes >= 128) {
    vstoreu(dst, Q6_V_vdelta_VV(vloadu(src), ctrl));
    src += 128;
    dst += 128;
    bytes -= 128;
  }
  if (bytes > 0) {
    vstoreu_partial(dst, Q6_V_vdelta_VV(vloadu_partial_zero(src, bytes), ctrl),
                    bytes);
  }
}

static __inline void YuvToRgb_HVX(HVX_Vector y,
                                  HVX_Vector u,
                                  HVX_Vector v,
                                  HVX_Vector* b_out,
                                  HVX_Vector* g_out,
                                  HVX_Vector* r_out,
                                  const struct YuvConstants* yuvconstants) {
  uint8_t ub = yuvconstants->kUVToB[0];
  uint8_t ug = yuvconstants->kUVToG[0];
  uint8_t vg = yuvconstants->kUVToG[1];
  uint8_t vr = yuvconstants->kUVToR[1];
  uint16_t yg = (uint16_t)yuvconstants->kYToRgb[0];
  int16_t yb = yuvconstants->kYBiasToRgb[0];

  uint32_t yg32 = ((uint32_t)yg << 16) | yg;
  HVX_Vector v_yb = Q6_Vh_vsplat_R(yb);
  HVX_Vector v_ub = Q6_Vb_vsplat_R(ub);
  HVX_Vector v_ug = Q6_Vb_vsplat_R(ug);
  HVX_Vector v_vg = Q6_Vb_vsplat_R(vg);
  HVX_Vector v_vr = Q6_Vb_vsplat_R(vr);
  HVX_Vector v80 = Q6_Vb_vsplat_R(0x80);

  HVX_Vector ui = Q6_Vb_vsub_VbVb(u, v80);
  HVX_Vector vi = Q6_Vb_vsub_VbVb(v, v80);

  HVX_Vector y_even_0101 = Q6_Vb_vshuffe_VbVb(y, y);
  HVX_Vector y_odd_0101 = Q6_Vb_vshuffo_VbVb(y, y);
  HVX_VectorPair p_even = Q6_Wuw_vmpy_VuhRuh(y_even_0101, yg32);
  HVX_VectorPair p_odd = Q6_Wuw_vmpy_VuhRuh(y_odd_0101, yg32);
  HVX_Vector y1_lo = Q6_Vh_vadd_VhVh(
      Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p_even), Q6_V_lo_W(p_even)), v_yb);
  HVX_Vector y1_hi = Q6_Vh_vadd_VhVh(
      Q6_Vh_vshuffo_VhVh(Q6_V_hi_W(p_odd), Q6_V_lo_W(p_odd)), v_yb);

  HVX_VectorPair b_uv = Q6_Wh_vmpy_VubVb(v_ub, ui);
  HVX_Vector b_lo =
      Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh_sat(y1_lo, Q6_V_lo_W(b_uv)), 6);
  HVX_Vector b_hi =
      Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh_sat(y1_hi, Q6_V_hi_W(b_uv)), 6);
  *b_out = Q6_Vub_vsat_VhVh(b_hi, b_lo);

  HVX_VectorPair g_uv =
      Q6_Wh_vmpyacc_WhVubVb(Q6_Wh_vmpy_VubVb(v_ug, ui), v_vg, vi);
  HVX_Vector g_lo =
      Q6_Vh_vasr_VhR(Q6_Vh_vsub_VhVh_sat(y1_lo, Q6_V_lo_W(g_uv)), 6);
  HVX_Vector g_hi =
      Q6_Vh_vasr_VhR(Q6_Vh_vsub_VhVh_sat(y1_hi, Q6_V_hi_W(g_uv)), 6);
  *g_out = Q6_Vub_vsat_VhVh(g_hi, g_lo);

  HVX_VectorPair r_uv = Q6_Wh_vmpy_VubVb(v_vr, vi);
  HVX_Vector r_lo =
      Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh_sat(y1_lo, Q6_V_lo_W(r_uv)), 6);
  HVX_Vector r_hi =
      Q6_Vh_vasr_VhR(Q6_Vh_vadd_VhVh_sat(y1_hi, Q6_V_hi_W(r_uv)), 6);
  *r_out = Q6_Vub_vsat_VhVh(r_hi, r_lo);
}

static __inline void StoreARGB_HVX(uint8_t* dst_argb,
                                   HVX_Vector b,
                                   HVX_Vector g,
                                   HVX_Vector r,
                                   HVX_Vector a,
                                   int w) {
  HVX_VectorPair br = Q6_W_vshuff_VVR(r, b, -1);
  HVX_VectorPair ga = Q6_W_vshuff_VVR(a, g, -1);
  HVX_VectorPair p01 = Q6_W_vshuff_VVR(Q6_V_lo_W(ga), Q6_V_lo_W(br), -1);
  HVX_VectorPair p23 = Q6_W_vshuff_VVR(Q6_V_hi_W(ga), Q6_V_hi_W(br), -1);
  int dst_bytes = w * 4;
  vstoreu_partial(dst_argb, Q6_V_lo_W(p01), dst_bytes > 128 ? 128 : dst_bytes);
  if (dst_bytes > 128) {
    vstoreu_partial(dst_argb + 128, Q6_V_hi_W(p01),
                    dst_bytes > 256 ? 128 : dst_bytes - 128);
  }
  if (dst_bytes > 256) {
    vstoreu_partial(dst_argb + 256, Q6_V_lo_W(p23),
                    dst_bytes > 384 ? 128 : dst_bytes - 256);
  }
  if (dst_bytes > 384) {
    vstoreu_partial(dst_argb + 384, Q6_V_hi_W(p23), dst_bytes - 384);
  }
}

void I444ToARGBRow_HVX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector u = vloadu_partial_zero(src_u, w);
    HVX_Vector v = vloadu_partial_zero(src_v, w);
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_u += 128;
    src_v += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void I444AlphaToARGBRow_HVX(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            const uint8_t* src_a,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector u = vloadu_partial_zero(src_u, w);
    HVX_Vector v = vloadu_partial_zero(src_v, w);
    HVX_Vector a = vloadu_partial_zero(src_a, w);
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_u += 128;
    src_v += 128;
    src_a += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void I422ToARGBRow_HVX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int uv_w = (w + 1) >> 1;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector u_sub = vloadu_partial_zero(src_u, uv_w);
    HVX_Vector v_sub = vloadu_partial_zero(src_v, uv_w);
    HVX_Vector u = Q6_V_lo_W(Q6_W_vshuff_VVR(u_sub, u_sub, -1));
    HVX_Vector v = Q6_V_lo_W(Q6_W_vshuff_VVR(v_sub, v_sub, -1));
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_u += 64;
    src_v += 64;
    dst_argb += 512;
    width -= 128;
  }
}

void I422AlphaToARGBRow_HVX(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            const uint8_t* src_a,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int uv_w = (w + 1) >> 1;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector u_sub = vloadu_partial_zero(src_u, uv_w);
    HVX_Vector v_sub = vloadu_partial_zero(src_v, uv_w);
    HVX_Vector a = vloadu_partial_zero(src_a, w);
    HVX_Vector u = Q6_V_lo_W(Q6_W_vshuff_VVR(u_sub, u_sub, -1));
    HVX_Vector v = Q6_V_lo_W(Q6_W_vshuff_VVR(v_sub, v_sub, -1));
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_u += 64;
    src_v += 64;
    src_a += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void I422ToRGBARow_HVX(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_rgba,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int uv_w = (w + 1) >> 1;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector u_sub = vloadu_partial_zero(src_u, uv_w);
    HVX_Vector v_sub = vloadu_partial_zero(src_v, uv_w);
    HVX_Vector u = Q6_V_lo_W(Q6_W_vshuff_VVR(u_sub, u_sub, -1));
    HVX_Vector v = Q6_V_lo_W(Q6_W_vshuff_VVR(v_sub, v_sub, -1));
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_rgba, a, b, g, r, w);
    src_y += 128;
    src_u += 64;
    src_v += 64;
    dst_rgba += 512;
    width -= 128;
  }
}

void NV12ToARGBRow_HVX(const uint8_t* src_y,
                       const uint8_t* src_uv,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int uv_bytes = ((w + 1) >> 1) * 2;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector uv = vloadu_partial_zero(src_uv, uv_bytes);
    HVX_VectorPair u_v = Q6_W_vdeal_VVR(uv, uv, -1);
    HVX_Vector u_sub = Q6_V_lo_W(u_v);
    HVX_Vector v_sub = Q6_V_hi_W(u_v);
    HVX_Vector u = Q6_V_lo_W(Q6_W_vshuff_VVR(u_sub, u_sub, -1));
    HVX_Vector v = Q6_V_lo_W(Q6_W_vshuff_VVR(v_sub, v_sub, -1));
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_uv += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void NV21ToARGBRow_HVX(const uint8_t* src_y,
                       const uint8_t* src_vu,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  HVX_Vector a = Q6_Vb_vsplat_R(0xff);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int uv_bytes = ((w + 1) >> 1) * 2;
    HVX_Vector y = vloadu_partial_zero(src_y, w);
    HVX_Vector vu = vloadu_partial_zero(src_vu, uv_bytes);
    HVX_VectorPair v_u = Q6_W_vdeal_VVR(vu, vu, -1);
    HVX_Vector v_sub = Q6_V_lo_W(v_u);
    HVX_Vector u_sub = Q6_V_hi_W(v_u);
    HVX_Vector u = Q6_V_lo_W(Q6_W_vshuff_VVR(u_sub, u_sub, -1));
    HVX_Vector v = Q6_V_lo_W(Q6_W_vshuff_VVR(v_sub, v_sub, -1));
    HVX_Vector b, g, r;
    YuvToRgb_HVX(y, u, v, &b, &g, &r, yuvconstants);
    StoreARGB_HVX(dst_argb, b, g, r, a, w);
    src_y += 128;
    src_vu += 128;
    dst_argb += 512;
    width -= 128;
  }
}

void ARGBToYMatrixRow_HVX(const uint8_t* src_argb,
                          uint8_t* dst_y,
                          int width,
                          const struct ArgbConstants* c) {
  uint32_t coeff_y;
  memcpy(&coeff_y, c->kRGBToY, sizeof(coeff_y));
  HVX_Vector v_add_y = Q6_V_vsplat_R((uint32_t)c->kAddY[0]);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_argb + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_argb + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_argb + 384, src_bytes - 384)
                        : Q6_V_vzero();
    HVX_Vector y0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vadd_VwVw(Q6_Vuw_vrmpy_VubRub(s0, coeff_y), v_add_y), 8);
    HVX_Vector y1 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vadd_VwVw(Q6_Vuw_vrmpy_VubRub(s1, coeff_y), v_add_y), 8);
    HVX_Vector y2 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vadd_VwVw(Q6_Vuw_vrmpy_VubRub(s2, coeff_y), v_add_y), 8);
    HVX_Vector y3 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vadd_VwVw(Q6_Vuw_vrmpy_VubRub(s3, coeff_y), v_add_y), 8);
    HVX_Vector y01 = Q6_Vh_vpacke_VwVw(y1, y0);
    HVX_Vector y23 = Q6_Vh_vpacke_VwVw(y3, y2);
    HVX_Vector out = Q6_Vb_vpacke_VhVh(y23, y01);
    vstoreu_partial(dst_y, out, w);
    src_argb += 512;
    dst_y += 128;
    width -= 128;
  }
}

void ARGBToUV444MatrixRow_HVX(const uint8_t* src_argb,
                              uint8_t* dst_u,
                              uint8_t* dst_v,
                              int width,
                              const struct ArgbConstants* c) {
  int32_t coeff_u;
  int32_t coeff_v;
  memcpy(&coeff_u, c->kRGBToU, sizeof(coeff_u));
  memcpy(&coeff_v, c->kRGBToV, sizeof(coeff_v));
  HVX_Vector v_add_uv = Q6_V_vsplat_R((uint32_t)c->kAddUV[0]);
  while (width > 0) {
    int w = width >= 128 ? 128 : width;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 =
        src_bytes > 128
            ? vloadu_partial_zero(src_argb + 128,
                                  src_bytes > 256 ? 128 : src_bytes - 128)
            : Q6_V_vzero();
    HVX_Vector s2 =
        src_bytes > 256
            ? vloadu_partial_zero(src_argb + 256,
                                  src_bytes > 384 ? 128 : src_bytes - 256)
            : Q6_V_vzero();
    HVX_Vector s3 = src_bytes > 384
                        ? vloadu_partial_zero(src_argb + 384, src_bytes - 384)
                        : Q6_V_vzero();

    HVX_Vector u0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s0, coeff_u)), 8);
    HVX_Vector u1 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s1, coeff_u)), 8);
    HVX_Vector u2 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s2, coeff_u)), 8);
    HVX_Vector u3 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s3, coeff_u)), 8);
    HVX_Vector u_out =
        Q6_Vb_vpacke_VhVh(Q6_Vh_vpacke_VwVw(u3, u2), Q6_Vh_vpacke_VwVw(u1, u0));
    vstoreu_partial(dst_u, u_out, w);

    HVX_Vector v0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s0, coeff_v)), 8);
    HVX_Vector v1 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s1, coeff_v)), 8);
    HVX_Vector v2 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s2, coeff_v)), 8);
    HVX_Vector v3 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(s3, coeff_v)), 8);
    HVX_Vector v_out =
        Q6_Vb_vpacke_VhVh(Q6_Vh_vpacke_VwVw(v3, v2), Q6_Vh_vpacke_VwVw(v1, v0));
    vstoreu_partial(dst_v, v_out, w);

    src_argb += 512;
    dst_u += 128;
    dst_v += 128;
    width -= 128;
  }
}

void ARGBToUVMatrixRow_HVX(const uint8_t* src_argb,
                           int src_stride_argb,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width,
                           const struct ArgbConstants* c) {
  int32_t coeff_u;
  int32_t coeff_v;
  memcpy(&coeff_u, c->kRGBToU, sizeof(coeff_u));
  memcpy(&coeff_v, c->kRGBToV, sizeof(coeff_v));
  HVX_Vector v_add_uv = Q6_V_vsplat_R((uint32_t)c->kAddUV[0]);
  HVX_Vector vtwo = Q6_Vh_vsplat_R(2);
  const uint8_t* src_argb1 = src_argb + src_stride_argb;

  int even_width = width & ~1;
  while (even_width > 0) {
    int w = even_width >= 64 ? 64 : even_width;
    int uv_w = w >> 1;
    int src_bytes = w * 4;
    HVX_Vector s0 =
        vloadu_partial_zero(src_argb, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector s1 = src_bytes > 128
                        ? vloadu_partial_zero(src_argb + 128, src_bytes - 128)
                        : Q6_V_vzero();
    HVX_Vector t0 =
        vloadu_partial_zero(src_argb1, src_bytes > 128 ? 128 : src_bytes);
    HVX_Vector t1 = src_bytes > 128
                        ? vloadu_partial_zero(src_argb1 + 128, src_bytes - 128)
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

    HVX_Vector avg = Q6_Vub_vpack_VhVh_sat(sum_hi, sum_lo);
    HVX_Vector u0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(avg, coeff_u)), 8);
    HVX_Vector v0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(avg, coeff_v)), 8);
    HVX_Vector u_out =
        Q6_Vb_vpacke_VhVh(Q6_V_vzero(), Q6_Vh_vpacke_VwVw(Q6_V_vzero(), u0));
    HVX_Vector v_out =
        Q6_Vb_vpacke_VhVh(Q6_V_vzero(), Q6_Vh_vpacke_VwVw(Q6_V_vzero(), v0));
    vstoreu_partial(dst_u, u_out, uv_w);
    vstoreu_partial(dst_v, v_out, uv_w);

    src_argb += w * 4;
    src_argb1 += w * 4;
    dst_u += uv_w;
    dst_v += uv_w;
    even_width -= w;
  }
  if (width & 1) {
    HVX_Vector s0 = vloadu_partial_zero(src_argb, 4);
    HVX_Vector t0 = vloadu_partial_zero(src_argb1, 4);
    HVX_Vector avg = Q6_Vub_vavg_VubVub_rnd(s0, t0);
    HVX_Vector u0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(avg, coeff_u)), 8);
    HVX_Vector v0 = Q6_Vuw_vlsr_VuwR(
        Q6_Vw_vsub_VwVw(v_add_uv, Q6_Vw_vrmpy_VubRb(avg, coeff_v)), 8);
    vstoreu_partial(dst_u, u0, 1);
    vstoreu_partial(dst_v, v0, 1);
  }
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_HVX) && defined(__HVX__)
