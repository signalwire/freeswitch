/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"
#include "libyuv/scale_row.h"

#if !defined(LIBYUV_DISABLE_X86) &&                                 \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
     defined(_M_IX86)) &&                                           \
    ((defined(_MSC_VER) && !defined(__clang__)) ||                  \
     defined(LIBYUV_ENABLE_ROWWIN))
#include <emmintrin.h>
#include <immintrin.h>
#include <string.h>
#include <tmmintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// This module is for 32 bit Visual C x86
#if !defined(LIBYUV_DISABLE_X86) && defined(_MSC_VER) && defined(_M_IX86) && \
    !defined(__clang__) && !defined(LIBYUV_ENABLE_ROWWIN)

#ifdef HAS_SCALEROWDOWN2_SSSE3
// Reads 32 pixels, throws half away and writes 16 pixels.
__declspec(naked) void ScaleRowDown2_SSSE3(const uint8_t* src_ptr,
                                           ptrdiff_t src_stride,
                                           uint8_t* dst_ptr,
                                           int dst_width) {
  __asm {
    mov        eax, [esp + 4]  // src_ptr
    // src_stride ignored
    mov        edx, [esp + 12]  // dst_ptr
    mov        ecx, [esp + 16]  // dst_width

  wloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8       // isolate odd pixels.
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    jg         wloop

    ret
  }
}

// Blends 32x1 rectangle to 16x1.
__declspec(naked) void ScaleRowDown2Linear_SSSE3(const uint8_t* src_ptr,
                                                 ptrdiff_t src_stride,
                                                 uint8_t* dst_ptr,
                                                 int dst_width) {
  __asm {
    mov        eax, [esp + 4]  // src_ptr
    // src_stride
    mov        edx, [esp + 12]  // dst_ptr
    mov        ecx, [esp + 16]  // dst_width

    pcmpeqb    xmm4, xmm4  // constant 0x0101
    psrlw      xmm4, 15
    packuswb   xmm4, xmm4
    pxor       xmm5, xmm5  // constant 0

  wloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pmaddubsw  xmm0, xmm4  // horizontal add
    pmaddubsw  xmm1, xmm4
    pavgw      xmm0, xmm5    // (x + 1) / 2
    pavgw      xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    jg         wloop

    ret
  }
}

// Blends 32x2 rectangle to 16x1.
__declspec(naked) void ScaleRowDown2Box_SSSE3(const uint8_t* src_ptr,
                                              ptrdiff_t src_stride,
                                              uint8_t* dst_ptr,
                                              int dst_width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]  // src_ptr
    mov        esi, [esp + 4 + 8]  // src_stride
    mov        edx, [esp + 4 + 12]  // dst_ptr
    mov        ecx, [esp + 4 + 16]  // dst_width

    pcmpeqb    xmm4, xmm4  // constant 0x0101
    psrlw      xmm4, 15
    packuswb   xmm4, xmm4
    pxor       xmm5, xmm5  // constant 0

  wloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + esi]
    movdqu     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pmaddubsw  xmm0, xmm4  // horizontal add
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    paddw      xmm0, xmm2  // vertical add
    paddw      xmm1, xmm3
    psrlw      xmm0, 1
    psrlw      xmm1, 1
    pavgw      xmm0, xmm5  // (x + 1) / 2
    pavgw      xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    jg         wloop

    pop        esi
    ret
  }
}
#endif  // HAS_SCALEROWDOWN2_SSSE3

#ifdef HAS_SCALEROWDOWN4_SSSE3
// Point samples 32 pixels to 8 pixels.
__declspec(naked) void ScaleRowDown4_SSSE3(const uint8_t* src_ptr,
                                           ptrdiff_t src_stride,
                                           uint8_t* dst_ptr,
                                           int dst_width) {
  __asm {
    mov        eax, [esp + 4]  // src_ptr
    // src_stride ignored
    mov        edx, [esp + 12]  // dst_ptr
    mov        ecx, [esp + 16]  // dst_width
    pcmpeqb    xmm5, xmm5     // generate mask 0x00ff0000
    psrld      xmm5, 24
    pslld      xmm5, 16

  wloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    psrlw      xmm0, 8
    packuswb   xmm0, xmm0
    movq       qword ptr [edx], xmm0
    lea        edx, [edx + 8]
    sub        ecx, 8
    jg         wloop

    ret
  }
}

// Blends 32x4 rectangle to 8x1.
__declspec(naked) void ScaleRowDown4Box_SSSE3(const uint8_t* src_ptr,
                                              ptrdiff_t src_stride,
                                              uint8_t* dst_ptr,
                                              int dst_width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]  // src_ptr
    mov        esi, [esp + 8 + 8]  // src_stride
    mov        edx, [esp + 8 + 12]  // dst_ptr
    mov        ecx, [esp + 8 + 16]  // dst_width
    lea        edi, [esi + esi * 2]  // src_stride * 3
    pcmpeqb    xmm4, xmm4  // constant 0x0101
    psrlw      xmm4, 15
    movdqa     xmm5, xmm4
    packuswb   xmm4, xmm4
    psllw      xmm5, 3  // constant 0x0008

  wloop:
    movdqu     xmm0, [eax]  // average rows
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + esi]
    movdqu     xmm3, [eax + esi + 16]
    pmaddubsw  xmm0, xmm4  // horizontal add
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    paddw      xmm0, xmm2  // vertical add rows 0, 1
    paddw      xmm1, xmm3
    movdqu     xmm2, [eax + esi * 2]
    movdqu     xmm3, [eax + esi * 2 + 16]
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    paddw      xmm0, xmm2  // add row 2
    paddw      xmm1, xmm3
    movdqu     xmm2, [eax + edi]
    movdqu     xmm3, [eax + edi + 16]
    lea        eax, [eax + 32]
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    paddw      xmm0, xmm2  // add row 3
    paddw      xmm1, xmm3
    phaddw     xmm0, xmm1
    paddw      xmm0, xmm5  // + 8 for round
    psrlw      xmm0, 4  // /16 for average of 4 * 4
    packuswb   xmm0, xmm0
    movq       qword ptr [edx], xmm0
    lea        edx, [edx + 8]
    sub        ecx, 8
    jg         wloop

    pop        edi
    pop        esi
    ret
  }
}
#endif  // HAS_SCALEROWDOWN4_SSSE3

#ifdef HAS_FIXEDDIV_X86
// Divide num by div and return as 16.16 fixed point result.
__declspec(naked) int FixedDiv_X86(int num, int div) {
  __asm {
    mov        eax, [esp + 4]  // num
    cdq  // extend num to 64 bits
    shld       edx, eax, 16  // 32.16
    shl        eax, 16
    idiv       dword ptr [esp + 8]
    ret
  }
}

// Divide num by div and return as 16.16 fixed point result.
__declspec(naked) int FixedDiv1_X86(int num, int div) {
  __asm {
    mov        eax, [esp + 4]  // num
    mov        ecx, [esp + 8]  // denom
    cdq  // extend num to 64 bits
    shld       edx, eax, 16  // 32.16
    shl        eax, 16
    sub        eax, 0x00010001
    sbb        edx, 0
    sub        ecx, 1
    idiv       ecx
    ret
  }
}
#endif  // HAS_FIXEDDIV_X86
#endif  // !defined(LIBYUV_DISABLE_X86) && defined(_MSC_VER) && defined(_M_IX86)

// The following are available for Visual C 32/64 bit, and for clang with
// LIBYUV_ENABLE_ROWWIN:
#if !defined(LIBYUV_DISABLE_X86) &&                                 \
    (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || \
     defined(_M_IX86)) &&                                           \
    ((defined(_MSC_VER) && !defined(__clang__)) ||                  \
     defined(LIBYUV_ENABLE_ROWWIN))

#if defined(__clang__) || defined(__GNUC__)
#define LIBYUV_TARGET_AVX2 __attribute__((target("avx2")))
#define LIBYUV_TARGET_AVX512BW \
  __attribute__((target("avx512bw,avx512vl,avx512f")))
#define LIBYUV_TARGET_AVX512VBMI \
  __attribute__((target("avx512vbmi,avx512bw,avx512vl,avx512f")))
#else
#define LIBYUV_TARGET_AVX2
#define LIBYUV_TARGET_AVX512BW
#define LIBYUV_TARGET_AVX512VBMI
#endif  // defined(__clang__) || defined(__GNUC__)

static inline uint16_t LoadU16(const void* p) {
  uint16_t v;
  memcpy(&v, p, sizeof(v));
  return v;
}

static inline uint32_t LoadU32(const void* p) {
  uint32_t v;
  memcpy(&v, p, sizeof(v));
  return v;
}

static inline uint64_t LoadU64(const void* p) {
  uint64_t v;
  memcpy(&v, p, sizeof(v));
  return v;
}

static inline void StoreU32(void* p, uint32_t v) {
  memcpy(p, &v, sizeof(v));
}

#if defined(HAS_FIXEDDIV_X86) &&                                      \
    !(defined(_MSC_VER) && defined(_M_IX86) && !defined(__clang__) && \
      !defined(LIBYUV_ENABLE_ROWWIN))
int FixedDiv_X86(int num, int div) {
  return (int)(((int64_t)num * 65536) / div);
}
#endif

#if defined(HAS_FIXEDDIV1_X86) &&                                     \
    !(defined(_MSC_VER) && defined(_M_IX86) && !defined(__clang__) && \
      !defined(LIBYUV_ENABLE_ROWWIN))
int FixedDiv1_X86(int num, int div) {
  return (int)((((int64_t)num * 65536) - 0x00010001) / (div - 1));
}
#endif

#ifdef HAS_SCALEROWDOWN2_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowDown2_AVX2(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    __m256i s0 = _mm256_srli_epi16(
        _mm256_loadu_si256((const __m256i*)src_ptr), 8);
    __m256i s1 = _mm256_srli_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), 8);
    src_ptr += 64;
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 32;
    dst_width -= 32;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown2Linear_AVX2(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  (void)src_stride;
  const __m256i ones = _mm256_set1_epi8(1);
  const __m256i zero = _mm256_setzero_si256();
  while (dst_width > 0) {
    __m256i s0 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)src_ptr), ones);
    __m256i s1 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), ones);
    src_ptr += 64;
    s0 = _mm256_avg_epu16(s0, zero);
    s1 = _mm256_avg_epu16(s1, zero);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 32;
    dst_width -= 32;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown2Box_AVX2(const uint8_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint8_t* dst_ptr,
                           int dst_width) {
  const __m256i ones = _mm256_set1_epi8(1);
  const __m256i zero = _mm256_setzero_si256();
  while (dst_width > 0) {
    __m256i s0 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)src_ptr), ones);
    __m256i s1 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), ones);
    __m256i s2 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)), ones);
    __m256i s3 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride + 32)), ones);
    src_ptr += 64;
    s0 = _mm256_avg_epu16(_mm256_srli_epi16(_mm256_add_epi16(s0, s2), 1), zero);
    s1 = _mm256_avg_epu16(_mm256_srli_epi16(_mm256_add_epi16(s1, s3), 1), zero);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 32;
    dst_width -= 32;
  }
}
#endif  // HAS_SCALEROWDOWN2_AVX2

#ifdef HAS_SCALEROWDOWN2_16_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowDown2_16_AVX2(const uint16_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint16_t* dst_ptr,
                           int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    __m256i s0 = _mm256_srli_epi32(
        _mm256_loadu_si256((const __m256i*)src_ptr), 16);
    __m256i s1 = _mm256_srli_epi32(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 16)), 16);
    src_ptr += 32;
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 16;
    dst_width -= 16;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown2Linear_16_AVX2(const uint16_t* src_ptr,
                                 ptrdiff_t src_stride,
                                 uint16_t* dst_ptr,
                                 int dst_width) {
  (void)src_stride;
  const __m256i mask = _mm256_set1_epi32(0x0000ffff);
  while (dst_width > 0) {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_ptr + 16));
    src_ptr += 32;
    __m256i e = _mm256_packus_epi32(_mm256_and_si256(r0, mask),
                                    _mm256_and_si256(r1, mask));
    __m256i o = _mm256_packus_epi32(_mm256_srli_epi32(r0, 16),
                                    _mm256_srli_epi32(r1, 16));
    __m256i d = _mm256_permute4x64_epi64(_mm256_avg_epu16(e, o), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 16;
    dst_width -= 16;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown2Box_16_AVX2(const uint16_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint16_t* dst_ptr,
                              int dst_width) {
  const __m256i mask = _mm256_set1_epi32(0x0000ffff);
  const __m256i round2 = _mm256_set1_epi32(2);
  while (dst_width > 0) {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_ptr + 16));
    __m256i r2 = _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride));
    __m256i r3 =
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride + 16));
    src_ptr += 32;
    __m256i s0 = _mm256_add_epi32(
        _mm256_add_epi32(_mm256_and_si256(r0, mask), _mm256_srli_epi32(r0, 16)),
        _mm256_add_epi32(_mm256_and_si256(r2, mask),
                         _mm256_srli_epi32(r2, 16)));
    __m256i s1 = _mm256_add_epi32(
        _mm256_add_epi32(_mm256_and_si256(r1, mask), _mm256_srli_epi32(r1, 16)),
        _mm256_add_epi32(_mm256_and_si256(r3, mask),
                         _mm256_srli_epi32(r3, 16)));
    s0 = _mm256_srli_epi32(_mm256_add_epi32(s0, round2), 2);
    s1 = _mm256_srli_epi32(_mm256_add_epi32(s1, round2), 2);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWDOWN2_16_AVX2

#ifdef HAS_SCALEROWDOWN2_AVX512BW
LIBYUV_TARGET_AVX512BW
void ScaleRowDown2_AVX512BW(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  (void)src_stride;
  while (dst_width >= 32) {
    __m512i s =
        _mm512_srli_epi16(_mm512_loadu_si512((const __m512i*)src_ptr), 8);
    src_ptr += 64;
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm512_cvtepi16_epi8(s));
    dst_ptr += 32;
    dst_width -= 32;
  }
  if (dst_width > 0) {
    __mmask32 k = (__mmask32)((1ULL << dst_width) - 1);
    __m512i s = _mm512_srli_epi16(_mm512_maskz_loadu_epi16(k, src_ptr), 8);
    _mm256_mask_storeu_epi8(dst_ptr, k, _mm512_cvtepi16_epi8(s));
  }
}

LIBYUV_TARGET_AVX512BW
void ScaleRowDown2Linear_AVX512BW(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  int dst_width) {
  (void)src_stride;
  const __m512i ones = _mm512_set1_epi8(1);
  const __m512i zero = _mm512_setzero_si512();
  while (dst_width >= 32) {
    __m512i s = _mm512_avg_epu16(
        _mm512_maddubs_epi16(_mm512_loadu_si512((const __m512i*)src_ptr), ones),
        zero);
    src_ptr += 64;
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm512_cvtepi16_epi8(s));
    dst_ptr += 32;
    dst_width -= 32;
  }
  if (dst_width > 0) {
    __mmask32 k = (__mmask32)((1ULL << dst_width) - 1);
    __m512i s = _mm512_avg_epu16(
        _mm512_maddubs_epi16(_mm512_maskz_loadu_epi16(k, src_ptr), ones),
        zero);
    _mm256_mask_storeu_epi8(dst_ptr, k, _mm512_cvtepi16_epi8(s));
  }
}

LIBYUV_TARGET_AVX512BW
void ScaleRowDown2Box_AVX512BW(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m512i ones = _mm512_set1_epi8(1);
  const __m512i zero = _mm512_setzero_si512();
  while (dst_width >= 32) {
    __m512i s0 = _mm512_maddubs_epi16(
        _mm512_loadu_si512((const __m512i*)src_ptr), ones);
    __m512i s1 = _mm512_maddubs_epi16(
        _mm512_loadu_si512((const __m512i*)(src_ptr + src_stride)), ones);
    src_ptr += 64;
    __m512i s =
        _mm512_avg_epu16(_mm512_srli_epi16(_mm512_add_epi16(s0, s1), 1), zero);
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm512_cvtepi16_epi8(s));
    dst_ptr += 32;
    dst_width -= 32;
  }
  if (dst_width > 0) {
    __mmask32 k = (__mmask32)((1ULL << dst_width) - 1);
    __m512i s0 =
        _mm512_maddubs_epi16(_mm512_maskz_loadu_epi16(k, src_ptr), ones);
    __m512i s1 = _mm512_maddubs_epi16(
        _mm512_maskz_loadu_epi16(k, src_ptr + src_stride), ones);
    __m512i s =
        _mm512_avg_epu16(_mm512_srli_epi16(_mm512_add_epi16(s0, s1), 1), zero);
    _mm256_mask_storeu_epi8(dst_ptr, k, _mm512_cvtepi16_epi8(s));
  }
}

LIBYUV_TARGET_AVX512BW
void ScaleRowDown2Box_Odd_AVX512BW(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   int dst_width) {
  dst_width -= 1;
  if (dst_width > 0) {
    ScaleRowDown2Box_AVX512BW(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += dst_width * 2;
    dst_ptr += dst_width;
  }
  dst_ptr[0] = (uint8_t)((src_ptr[0] + src_ptr[src_stride] + 1) >> 1);
}
#endif  // HAS_SCALEROWDOWN2_AVX512BW

#ifdef HAS_SCALEROWDOWN4_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowDown4_AVX2(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  const __m256i mask = _mm256_set1_epi32(0x00ff0000);
  while (dst_width > 0) {
    __m256i s0 =
        _mm256_and_si256(_mm256_loadu_si256((const __m256i*)src_ptr), mask);
    __m256i s1 = _mm256_and_si256(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), mask);
    src_ptr += 64;
    __m256i t = _mm256_srli_epi16(
        _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0xd8), 8);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi16(t, t), 0xd8);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d));
    dst_ptr += 16;
    dst_width -= 16;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown4Box_AVX2(const uint8_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint8_t* dst_ptr,
                           int dst_width) {
  const __m256i ones = _mm256_set1_epi8(1);
  const __m256i round8 = _mm256_set1_epi16(8);
  const ptrdiff_t stridex3 = src_stride * 3;
  while (dst_width > 0) {
    __m256i s0 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)src_ptr), ones);
    __m256i s1 = _mm256_maddubs_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), ones);
    s0 = _mm256_add_epi16(
        s0, _mm256_maddubs_epi16(
                _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)),
                ones));
    s1 = _mm256_add_epi16(
        s1, _mm256_maddubs_epi16(
                _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride + 32)),
                ones));
    s0 = _mm256_add_epi16(
        s0, _mm256_maddubs_epi16(
                _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride * 2)),
                ones));
    s1 = _mm256_add_epi16(
        s1,
        _mm256_maddubs_epi16(
            _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride * 2 + 32)),
            ones));
    s0 = _mm256_add_epi16(
        s0,
        _mm256_maddubs_epi16(
            _mm256_loadu_si256((const __m256i*)(src_ptr + stridex3)), ones));
    s1 = _mm256_add_epi16(
        s1, _mm256_maddubs_epi16(
                _mm256_loadu_si256((const __m256i*)(src_ptr + stridex3 + 32)),
                ones));
    src_ptr += 64;
    __m256i t = _mm256_permute4x64_epi64(_mm256_hadd_epi16(s0, s1), 0xd8);
    t = _mm256_srli_epi16(_mm256_add_epi16(t, round8), 4);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi16(t, t), 0xd8);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d));
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWDOWN4_AVX2

#ifdef HAS_SCALEROWDOWN34_AVX2
static const uvec8 kShuf0 = {0,   1,   3,   4,   5,   7,   8,   9,
                             128, 128, 128, 128, 128, 128, 128, 128};
static const uvec8 kShuf1 = {3,   4,   5,   7,   8,   9,   11,  12,
                             128, 128, 128, 128, 128, 128, 128, 128};
static const uvec8 kShuf2 = {5,   7,   8,   9,   11,  12,  13,  15,
                             128, 128, 128, 128, 128, 128, 128, 128};
static const uvec8 kShuf01 = {0, 1, 1, 2, 2, 3, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10};
static const uvec8 kShuf11 = {2, 3, 4, 5,  5,  6,  6,  7,
                              8, 9, 9, 10, 10, 11, 12, 13};
static const uvec8 kShuf21 = {5,  6,  6,  7,  8,  9,  9,  10,
                              10, 11, 12, 13, 13, 14, 14, 15};
static const uvec8 kMadd01 = {3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2};
static const uvec8 kMadd11 = {1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1};
static const uvec8 kMadd21 = {2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3};

LIBYUV_TARGET_AVX2
void ScaleRowDown34_AVX2(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  const __m256i shuf0 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf0));
  const __m256i shuf1 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf1));
  const __m256i shuf2 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf2));
  while (dst_width >= 48) {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i s1 = _mm256_loadu_si256((const __m256i*)(src_ptr + 32));
    src_ptr += 64;
    __m256i lo = _mm256_permute2x128_si256(s0, s1, 0x20);
    __m256i hi = _mm256_permute2x128_si256(s0, s1, 0x31);
    __m256i mid = _mm256_alignr_epi8(hi, lo, 8);
    __m256i d0 = _mm256_shuffle_epi8(lo, shuf0);
    __m256i d1 = _mm256_shuffle_epi8(mid, shuf1);
    __m256i d2 = _mm256_shuffle_epi8(hi, shuf2);
    __m256i d01 = _mm256_unpacklo_epi64(d0, d1);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d01));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), _mm256_castsi256_si128(d2));
    _mm_storeu_si128((__m128i*)(dst_ptr + 24), _mm256_extracti128_si256(d01, 1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 40), _mm256_extracti128_si256(d2, 1));
    dst_ptr += 48;
    dst_width -= 48;
  }
  if (dst_width > 0) {
    __m256i s0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m128i lo = _mm256_castsi256_si128(s0);
    __m128i hi = _mm256_extracti128_si256(s0, 1);
    __m128i mid = _mm_alignr_epi8(hi, lo, 8);
    __m128i d0 = _mm_shuffle_epi8(lo, _mm256_castsi256_si128(shuf0));
    __m128i d1 = _mm_shuffle_epi8(mid, _mm256_castsi256_si128(shuf1));
    __m128i d2 = _mm_shuffle_epi8(hi, _mm256_castsi256_si128(shuf2));
    _mm_storeu_si128((__m128i*)dst_ptr, _mm_unpacklo_epi64(d0, d1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), d2);
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown34_1_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m256i shuf01 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf01));
  const __m256i shuf11 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf11));
  const __m256i shuf21 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf21));
  const __m256i madd01 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd01));
  const __m256i madd11 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd11));
  const __m256i madd21 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd21));
  const __m256i round2 = _mm256_set1_epi16(2);
  while (dst_width >= 48) {
    __m256i s0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_ptr),
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)));
    __m256i s1 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)),
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride + 32)));
    src_ptr += 64;
    __m256i lo = _mm256_permute2x128_si256(s0, s1, 0x20);
    __m256i hi = _mm256_permute2x128_si256(s0, s1, 0x31);
    __m256i mid = _mm256_alignr_epi8(hi, lo, 8);
    __m256i p0 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(lo, shuf01), madd01),
            round2),
        2);
    __m256i p1 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(mid, shuf11), madd11),
            round2),
        2);
    __m256i p2 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(hi, shuf21), madd21),
            round2),
        2);
    p0 = _mm256_packus_epi16(p0, p0);
    p1 = _mm256_packus_epi16(p1, p1);
    p2 = _mm256_packus_epi16(p2, p2);
    __m256i d01 = _mm256_unpacklo_epi64(p0, p1);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d01));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), _mm256_castsi256_si128(p2));
    _mm_storeu_si128((__m128i*)(dst_ptr + 24), _mm256_extracti128_si256(d01, 1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 40), _mm256_extracti128_si256(p2, 1));
    dst_ptr += 48;
    dst_width -= 48;
  }
  if (dst_width > 0) {
    __m256i s0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_ptr),
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)));
    __m128i lo = _mm256_castsi256_si128(s0);
    __m128i hi = _mm256_extracti128_si256(s0, 1);
    __m128i mid = _mm_alignr_epi8(hi, lo, 8);
    __m128i r2 = _mm256_castsi256_si128(round2);
    __m128i p0 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(_mm_shuffle_epi8(lo, _mm256_castsi256_si128(shuf01)),
                              _mm256_castsi256_si128(madd01)),
            r2),
        2);
    __m128i p1 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(
                _mm_shuffle_epi8(mid, _mm256_castsi256_si128(shuf11)),
                _mm256_castsi256_si128(madd11)),
            r2),
        2);
    __m128i p2 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(_mm_shuffle_epi8(hi, _mm256_castsi256_si128(shuf21)),
                              _mm256_castsi256_si128(madd21)),
            r2),
        2);
    p0 = _mm_packus_epi16(p0, p0);
    p1 = _mm_packus_epi16(p1, p1);
    p2 = _mm_packus_epi16(p2, p2);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm_unpacklo_epi64(p0, p1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), p2);
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown34_0_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m256i shuf01 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf01));
  const __m256i shuf11 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf11));
  const __m256i shuf21 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf21));
  const __m256i madd01 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd01));
  const __m256i madd11 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd11));
  const __m256i madd21 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kMadd21));
  const __m256i round2 = _mm256_set1_epi16(2);
  while (dst_width >= 48) {
    __m256i a0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i b0 = _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride));
    __m256i s0 = _mm256_avg_epu8(a0, _mm256_avg_epu8(a0, b0));
    __m256i a1 = _mm256_loadu_si256((const __m256i*)(src_ptr + 32));
    __m256i b1 = _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride + 32));
    __m256i s1 = _mm256_avg_epu8(a1, _mm256_avg_epu8(a1, b1));
    src_ptr += 64;
    __m256i lo = _mm256_permute2x128_si256(s0, s1, 0x20);
    __m256i hi = _mm256_permute2x128_si256(s0, s1, 0x31);
    __m256i mid = _mm256_alignr_epi8(hi, lo, 8);
    __m256i p0 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(lo, shuf01), madd01),
            round2),
        2);
    __m256i p1 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(mid, shuf11), madd11),
            round2),
        2);
    __m256i p2 = _mm256_srli_epi16(
        _mm256_adds_epi16(
            _mm256_maddubs_epi16(_mm256_shuffle_epi8(hi, shuf21), madd21),
            round2),
        2);
    p0 = _mm256_packus_epi16(p0, p0);
    p1 = _mm256_packus_epi16(p1, p1);
    p2 = _mm256_packus_epi16(p2, p2);
    __m256i d01 = _mm256_unpacklo_epi64(p0, p1);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d01));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), _mm256_castsi256_si128(p2));
    _mm_storeu_si128((__m128i*)(dst_ptr + 24), _mm256_extracti128_si256(d01, 1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 40), _mm256_extracti128_si256(p2, 1));
    dst_ptr += 48;
    dst_width -= 48;
  }
  if (dst_width > 0) {
    __m256i a0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i b0 = _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride));
    __m256i s0 = _mm256_avg_epu8(a0, _mm256_avg_epu8(a0, b0));
    __m128i lo = _mm256_castsi256_si128(s0);
    __m128i hi = _mm256_extracti128_si256(s0, 1);
    __m128i mid = _mm_alignr_epi8(hi, lo, 8);
    __m128i r2 = _mm256_castsi256_si128(round2);
    __m128i p0 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(_mm_shuffle_epi8(lo, _mm256_castsi256_si128(shuf01)),
                              _mm256_castsi256_si128(madd01)),
            r2),
        2);
    __m128i p1 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(
                _mm_shuffle_epi8(mid, _mm256_castsi256_si128(shuf11)),
                _mm256_castsi256_si128(madd11)),
            r2),
        2);
    __m128i p2 = _mm_srli_epi16(
        _mm_adds_epi16(
            _mm_maddubs_epi16(_mm_shuffle_epi8(hi, _mm256_castsi256_si128(shuf21)),
                              _mm256_castsi256_si128(madd21)),
            r2),
        2);
    p0 = _mm_packus_epi16(p0, p0);
    p1 = _mm_packus_epi16(p1, p1);
    p2 = _mm_packus_epi16(p2, p2);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm_unpacklo_epi64(p0, p1));
    _mm_storel_epi64((__m128i*)(dst_ptr + 16), p2);
  }
}
#endif  // HAS_SCALEROWDOWN34_AVX2

#ifdef HAS_SCALEROWDOWN38_AVX2
static const uvec8 kShuf38a = {0,   3,   6,   8,   11,  14,  128, 128,
                               128, 128, 128, 128, 128, 128, 128, 128};
static const uvec8 kShuf38b = {128, 128, 128, 128, 128, 128, 0,   3,
                               6,   8,   11,  14,  128, 128, 128, 128};
static const uvec8 kShufAc = {0,   1,   6,   7,   12,  13,  128, 128,
                              128, 128, 128, 128, 128, 128, 128, 128};
static const uvec8 kShufAc3 = {128, 128, 128, 128, 128, 128, 0,   1,
                               6,   7,   12,  13,  128, 128, 128, 128};
static const uvec16 kScaleAc33 = {65536 / 9, 65536 / 9, 65536 / 6, 65536 / 9,
                                  65536 / 9, 65536 / 6, 0,         0};
static const uvec8 kShufAb0 = {0,  128, 3,  128, 6,   128, 8,   128,
                               11, 128, 14, 128, 128, 128, 128, 128};
static const uvec8 kShufAb1 = {1,  128, 4,  128, 7,   128, 9,   128,
                               12, 128, 15, 128, 128, 128, 128, 128};
static const uvec8 kShufAb2 = {2,  128, 5,   128, 128, 128, 10,  128,
                               13, 128, 128, 128, 128, 128, 128, 128};
static const uvec16 kScaleAb2 = {65536 / 3, 65536 / 3, 65536 / 2, 65536 / 3,
                                 65536 / 3, 65536 / 2, 0,         0};

LIBYUV_TARGET_AVX2
void ScaleRowDown38_AVX2(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  const __m256i shuf38 = _mm256_inserti128_si256(
      _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)&kShuf38a)),
      _mm_loadu_si128((const __m128i*)&kShuf38b), 1);
  while (dst_width >= 24) {
    __m256i s0 =
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_ptr), shuf38);
    __m256i s1 = _mm256_shuffle_epi8(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)), shuf38);
    src_ptr += 64;
    __m128i d0 =
        _mm_or_si128(_mm256_castsi256_si128(s0), _mm256_extracti128_si256(s0, 1));
    __m128i d1 =
        _mm_or_si128(_mm256_castsi256_si128(s1), _mm256_extracti128_si256(s1, 1));
    _mm_storel_epi64((__m128i*)dst_ptr, d0);
    StoreU32(dst_ptr + 8, (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(d0, 8)));
    _mm_storel_epi64((__m128i*)(dst_ptr + 12), d1);
    StoreU32(dst_ptr + 20, (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(d1, 8)));
    dst_ptr += 24;
    dst_width -= 24;
  }
  if (dst_width > 0) {
    __m256i s0 =
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_ptr), shuf38);
    __m128i d0 =
        _mm_or_si128(_mm256_castsi256_si128(s0), _mm256_extracti128_si256(s0, 1));
    _mm_storel_epi64((__m128i*)dst_ptr, d0);
    StoreU32(dst_ptr + 8, (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(d0, 8)));
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown38_2_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m256i shuf_ab0 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShufAb0));
  const __m256i shuf_ab1 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShufAb1));
  const __m256i shuf_ab2 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShufAb2));
  const __m256i scale_ab2 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kScaleAb2));
  const __m256i zero = _mm256_setzero_si256();
  while (dst_width > 0) {
    __m256i r = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_ptr),
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)));
    src_ptr += 32;
    __m256i t = _mm256_adds_epu16(
        _mm256_adds_epu16(_mm256_shuffle_epi8(r, shuf_ab0),
                          _mm256_shuffle_epi8(r, shuf_ab1)),
        _mm256_shuffle_epi8(r, shuf_ab2));
    t = _mm256_packus_epi16(_mm256_mulhi_epu16(t, scale_ab2), zero);
    __m128i d = _mm_or_si128(_mm256_castsi256_si128(t),
                             _mm_slli_si128(_mm256_extracti128_si256(t, 1), 6));
    _mm_storel_epi64((__m128i*)dst_ptr, d);
    StoreU32(dst_ptr + 8, (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(d, 8)));
    dst_ptr += 12;
    dst_width -= 12;
  }
}

LIBYUV_TARGET_AVX2
void ScaleRowDown38_3_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m256i shuf_ac =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShufAc));
  const __m256i shuf_ac3 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kShufAc3));
  const __m256i scale_ac33 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)&kScaleAc33));
  const __m256i zero = _mm256_setzero_si256();
  while (dst_width > 0) {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_ptr);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride));
    __m256i r2 =
        _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride * 2));
    src_ptr += 32;
    __m256i s0 =
        _mm256_adds_epu16(_mm256_adds_epu16(_mm256_unpacklo_epi8(r0, zero),
                                            _mm256_unpacklo_epi8(r1, zero)),
                          _mm256_unpacklo_epi8(r2, zero));
    __m256i s1 =
        _mm256_adds_epu16(_mm256_adds_epu16(_mm256_unpackhi_epi8(r0, zero),
                                            _mm256_unpackhi_epi8(r1, zero)),
                          _mm256_unpackhi_epi8(r2, zero));
    __m256i t0 = _mm256_shuffle_epi8(
        _mm256_adds_epu16(_mm256_adds_epu16(s0, _mm256_srli_si256(s0, 2)),
                          _mm256_srli_si256(s0, 4)),
        shuf_ac);
    __m256i t1 = _mm256_shuffle_epi8(
        _mm256_adds_epu16(_mm256_adds_epu16(s1, _mm256_srli_si256(s1, 2)),
                          _mm256_srli_si256(s1, 4)),
        shuf_ac3);
    __m256i t = _mm256_packus_epi16(
        _mm256_mulhi_epu16(_mm256_adds_epu16(t0, t1), scale_ac33), zero);
    __m128i d = _mm_or_si128(_mm256_castsi256_si128(t),
                             _mm_slli_si128(_mm256_extracti128_si256(t, 1), 6));
    _mm_storel_epi64((__m128i*)dst_ptr, d);
    StoreU32(dst_ptr + 8, (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(d, 8)));
    dst_ptr += 12;
    dst_width -= 12;
  }
}
#endif  // HAS_SCALEROWDOWN38_AVX2

#ifdef HAS_SCALEADDROW_SSE2
// Reads 16 bytes and accumulates to 16 shorts at a time.
void ScaleAddRow_SSE2(const uint8_t* src_ptr,
                      uint16_t* dst_ptr,
                      int src_width) {
  const __m128i zero = _mm_setzero_si128();
  while (src_width > 0) {
    __m128i s = _mm_loadu_si128((const __m128i*)src_ptr);
    src_ptr += 16;
    __m128i d0 = _mm_loadu_si128((const __m128i*)dst_ptr);
    __m128i d1 = _mm_loadu_si128((const __m128i*)(dst_ptr + 8));
    d0 = _mm_adds_epu16(d0, _mm_unpacklo_epi8(s, zero));
    d1 = _mm_adds_epu16(d1, _mm_unpackhi_epi8(s, zero));
    _mm_storeu_si128((__m128i*)dst_ptr, d0);
    _mm_storeu_si128((__m128i*)(dst_ptr + 8), d1);
    dst_ptr += 16;
    src_width -= 16;
  }
}
#endif  // HAS_SCALEADDROW_SSE2

#ifdef HAS_SCALEADDROW_AVX2
// Reads 32 bytes and accumulates to 32 shorts at a time.
LIBYUV_TARGET_AVX2
void ScaleAddRow_AVX2(const uint8_t* src_ptr,
                      uint16_t* dst_ptr,
                      int src_width) {
  while (src_width > 0) {
    __m256i s0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_ptr));
    __m256i s1 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)(src_ptr + 16)));
    src_ptr += 32;
    __m256i d0 = _mm256_loadu_si256((const __m256i*)dst_ptr);
    __m256i d1 = _mm256_loadu_si256((const __m256i*)(dst_ptr + 16));
    d0 = _mm256_adds_epu16(d0, s0);
    d1 = _mm256_adds_epu16(d1, s1);
    _mm256_storeu_si256((__m256i*)dst_ptr, d0);
    _mm256_storeu_si256((__m256i*)(dst_ptr + 16), d1);
    dst_ptr += 32;
    src_width -= 32;
  }
}
#endif  // HAS_SCALEADDROW_AVX2

#ifdef HAS_SCALEADDROW_AVX512BW
// Reads 64 bytes and accumulates to 64 shorts at a time, with masked tail.
LIBYUV_TARGET_AVX512BW
void ScaleAddRow_AVX512BW(const uint8_t* src_ptr,
                          uint16_t* dst_ptr,
                          int src_width) {
  while (src_width >= 64) {
    __m512i s0 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)src_ptr));
    __m512i s1 = _mm512_cvtepu8_epi16(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 32)));
    src_ptr += 64;
    __m512i d0 = _mm512_loadu_si512((const __m512i*)dst_ptr);
    __m512i d1 = _mm512_loadu_si512((const __m512i*)(dst_ptr + 32));
    d0 = _mm512_adds_epu16(d0, s0);
    d1 = _mm512_adds_epu16(d1, s1);
    _mm512_storeu_si512((__m512i*)dst_ptr, d0);
    _mm512_storeu_si512((__m512i*)(dst_ptr + 32), d1);
    dst_ptr += 64;
    src_width -= 64;
  }
  if (src_width >= 32) {
    __m512i s0 =
        _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i*)src_ptr));
    src_ptr += 32;
    __m512i d0 = _mm512_loadu_si512((const __m512i*)dst_ptr);
    d0 = _mm512_adds_epu16(d0, s0);
    _mm512_storeu_si512((__m512i*)dst_ptr, d0);
    dst_ptr += 32;
    src_width -= 32;
  }
  if (src_width > 0) {
    __mmask32 k = (__mmask32)((1ULL << src_width) - 1);
    __m512i s0 = _mm512_maskz_cvtepu8_epi16(
        k, _mm256_maskz_loadu_epi8(k, (const void*)src_ptr));
    __m512i d0 = _mm512_maskz_loadu_epi16(k, (const void*)dst_ptr);
    d0 = _mm512_maskz_adds_epu16(k, d0, s0);
    _mm512_mask_storeu_epi16((void*)dst_ptr, k, d0);
  }
}
#endif  // HAS_SCALEADDROW_AVX512BW

#if defined(HAS_SCALEADDCOLS1_AVX512BW) || defined(HAS_SCALEADDCOLS2_AVX512BW)
static const uint8_t kSequence0_15[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                          8, 9, 10, 11, 12, 13, 14, 15};
#endif

#ifdef HAS_SCALEADDCOLS1_AVX512BW
LIBYUV_TARGET_AVX512BW
void ScaleAddCols1_AVX512BW(int dst_width,
                            int boxheight,
                            int x,
                            int dx,
                            const uint16_t* src_ptr,
                            uint8_t* dst_ptr) {
  int boxwidth = dx >> 16;
  if (boxwidth < 1) {
    boxwidth = 1;
  }
  int scaleval = 65536 / (boxwidth * boxheight);
  src_ptr += (x >> 16);
  const __m512i mask_ffff = _mm512_set1_epi32(0x0000ffff);
  const __m512i vscale = _mm512_set1_epi32(scaleval);
  if (boxwidth <= 4) {
    __m512i seq =
        _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)kSequence0_15));
    seq = _mm512_or_si512(seq, _mm512_slli_epi32(seq, 16));
    __m512i vbw = _mm512_set1_epi16((int16_t)boxwidth);
    __m512i base = _mm512_mullo_epi16(seq, vbw);
    __m512i c01 = _mm512_set1_epi32(0x00010000);
    __m512i c23 = _mm512_set1_epi32(0x00030002);
    __m512i idx01 = _mm512_add_epi16(base, c01);
    __m512i idx23 = _mm512_add_epi16(base, c23);
    __mmask32 k2 = _mm512_cmpgt_epu16_mask(vbw, c01);
    __mmask32 k3 = _mm512_cmpgt_epu16_mask(vbw, c23);
    int span = boxwidth * 16;
    uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
    __mmask32 k1 = (__mmask32)m64;
    __mmask32 k4 = (__mmask32)(m64 >> 32);
    while (dst_width >= 16) {
      __m512i s0 = _mm512_maskz_loadu_epi16(k1, src_ptr);
      __m512i s1 = _mm512_maskz_loadu_epi16(k4, src_ptr + 32);
      src_ptr += span;
      __m512i p01 = _mm512_maskz_permutex2var_epi16(k2, s0, idx01, s1);
      __m512i p23 = _mm512_maskz_permutex2var_epi16(k3, s0, idx23, s1);
      __m512i sum01 = _mm512_add_epi32(_mm512_and_si512(p01, mask_ffff),
                                       _mm512_srli_epi32(p01, 16));
      __m512i sum23 = _mm512_add_epi32(_mm512_and_si512(p23, mask_ffff),
                                       _mm512_srli_epi32(p23, 16));
      __m512i sum = _mm512_add_epi32(sum01, sum23);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vscale), 16);
      _mm_storeu_si128((__m128i*)dst_ptr, _mm512_cvtepi32_epi8(sum));
      dst_ptr += 16;
      dst_width -= 16;
    }
    if (dst_width > 0) {
      int rem_span = dst_width * boxwidth;
      uint64_t rm64 = (rem_span >= 64) ? ~0ULL : ((1ULL << rem_span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)rm64, src_ptr);
      __m512i s1 =
          _mm512_maskz_loadu_epi16((__mmask32)(rm64 >> 32), src_ptr + 32);
      __m512i p01 = _mm512_maskz_permutex2var_epi16(k2, s0, idx01, s1);
      __m512i p23 = _mm512_maskz_permutex2var_epi16(k3, s0, idx23, s1);
      __m512i sum01 = _mm512_add_epi32(_mm512_and_si512(p01, mask_ffff),
                                       _mm512_srli_epi32(p01, 16));
      __m512i sum23 = _mm512_add_epi32(_mm512_and_si512(p23, mask_ffff),
                                       _mm512_srli_epi32(p23, 16));
      __m512i sum = _mm512_add_epi32(sum01, sum23);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vscale), 16);
      __mmask16 km = (__mmask16)((1U << dst_width) - 1);
      // Use _mm_mask_storeu_epi8 instead of _mm512_mask_cvtepi32_storeu_epi8
      // because LLVM MSan does not instrument @llvm.x86.avx512.mask.pmov.db.mem
      // to unpoison destination shadow memory.
      _mm_mask_storeu_epi8(dst_ptr, km, _mm512_cvtepi32_epi8(sum));
    }
  } else if (boxwidth <= 8) {
    __m512i seq =
        _mm512_cvtepu8_epi64(_mm_loadu_si128((const __m128i*)kSequence0_15));
    seq = _mm512_or_si512(seq, _mm512_slli_epi64(seq, 16));
    seq = _mm512_or_si512(seq, _mm512_slli_epi64(seq, 32));
    __m512i vbw = _mm512_set1_epi16((int16_t)boxwidth);
    __m512i base = _mm512_mullo_epi16(seq, vbw);
    __m512i c03 = _mm512_set1_epi64(0x0003000200010000ULL);
    __m512i c47 = _mm512_set1_epi64(0x0007000600050004ULL);
    __m512i idx03 = _mm512_add_epi16(base, c03);
    __m512i idx47 = _mm512_add_epi16(base, c47);
    __mmask32 k3 = _mm512_cmpgt_epu16_mask(vbw, c47);
    int span = boxwidth * 8;
    uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
    __mmask32 k1 = (__mmask32)m64;
    __mmask32 k4 = (__mmask32)(m64 >> 32);
    while (dst_width >= 8) {
      __m512i s0 = _mm512_maskz_loadu_epi16(k1, src_ptr);
      __m512i s1 = _mm512_maskz_loadu_epi16(k4, src_ptr + 32);
      src_ptr += span;
      __m512i p03 = _mm512_permutex2var_epi16(s0, idx03, s1);
      __m512i p47 = _mm512_maskz_permutex2var_epi16(k3, s0, idx47, s1);
      __m512i sum03 = _mm512_add_epi32(_mm512_and_si512(p03, mask_ffff),
                                       _mm512_srli_epi32(p03, 16));
      __m512i sum47 = _mm512_add_epi32(_mm512_and_si512(p47, mask_ffff),
                                       _mm512_srli_epi32(p47, 16));
      __m512i sum = _mm512_add_epi32(sum03, sum47);
      sum = _mm512_add_epi32(sum, _mm512_srli_epi64(sum, 32));
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vscale), 16);
      _mm_storel_epi64((__m128i*)dst_ptr, _mm512_cvtepi64_epi8(sum));
      dst_ptr += 8;
      dst_width -= 8;
    }
    if (dst_width > 0) {
      int rem_span = dst_width * boxwidth;
      uint64_t rm64 = (rem_span >= 64) ? ~0ULL : ((1ULL << rem_span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)rm64, src_ptr);
      __m512i s1 =
          _mm512_maskz_loadu_epi16((__mmask32)(rm64 >> 32), src_ptr + 32);
      __m512i p03 = _mm512_permutex2var_epi16(s0, idx03, s1);
      __m512i p47 = _mm512_maskz_permutex2var_epi16(k3, s0, idx47, s1);
      __m512i sum03 = _mm512_add_epi32(_mm512_and_si512(p03, mask_ffff),
                                       _mm512_srli_epi32(p03, 16));
      __m512i sum47 = _mm512_add_epi32(_mm512_and_si512(p47, mask_ffff),
                                       _mm512_srli_epi32(p47, 16));
      __m512i sum = _mm512_add_epi32(sum03, sum47);
      sum = _mm512_add_epi32(sum, _mm512_srli_epi64(sum, 32));
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vscale), 16);
      __mmask8 km = (__mmask8)((1U << dst_width) - 1);
      // Use _mm_mask_storeu_epi8 instead of _mm512_mask_cvtepi64_storeu_epi8
      // because LLVM MSan does not instrument @llvm.x86.avx512.mask.pmov.qb.mem
      // to unpoison destination shadow memory.
      _mm_mask_storeu_epi8(dst_ptr, km, _mm512_cvtepi64_epi8(sum));
    }
  } else {
    int rem = boxwidth & 31;
    __mmask32 k1 = (__mmask32)((1ULL << rem) - 1);
    while (dst_width > 0) {
      __m512i acc = _mm512_setzero_si512();
      const uint16_t* p = src_ptr;
      src_ptr += boxwidth;
      int n = boxwidth;
      while (n >= 32) {
        __m512i s = _mm512_loadu_si512((const __m512i*)p);
        p += 32;
        acc = _mm512_add_epi32(acc,
                               _mm512_add_epi32(_mm512_and_si512(s, mask_ffff),
                                                _mm512_srli_epi32(s, 16)));
        n -= 32;
      }
      if (n > 0) {
        __m512i s = _mm512_maskz_loadu_epi16(k1, p);
        acc = _mm512_add_epi32(acc,
                               _mm512_add_epi32(_mm512_and_si512(s, mask_ffff),
                                                _mm512_srli_epi32(s, 16)));
      }
      uint32_t total = (uint32_t)_mm512_reduce_add_epi32(acc);
      *dst_ptr++ = (uint8_t)((total * (uint32_t)scaleval) >> 16);
      --dst_width;
    }
  }
}
#endif  // HAS_SCALEADDCOLS1_AVX512BW

#ifdef HAS_SCALEADDCOLS2_AVX512BW
LIBYUV_TARGET_AVX512BW
void ScaleAddCols2_AVX512BW(int dst_width,
                            int boxheight,
                            int x,
                            int dx,
                            const uint16_t* src_ptr,
                            uint8_t* dst_ptr) {
  int minboxwidth = dx >> 16;
  int scale0 = 65536 / ((minboxwidth < 1 ? 1 : minboxwidth) * boxheight);
  int scale1 =
      65536 / (((minboxwidth + 1) < 1 ? 1 : (minboxwidth + 1)) * boxheight);
  const __m512i mask_ffff = _mm512_set1_epi32(0x0000ffff);
  const __m512i vscale0 = _mm512_set1_epi32(scale0);
  const __m512i vscale1 = _mm512_set1_epi32(scale1);
  if (minboxwidth >= 1 && minboxwidth <= 3) {
    __m512i seq =
        _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)kSequence0_15));
    __m512i vdx = _mm512_set1_epi32(dx);
    __m512i step0 = _mm512_mullo_epi32(seq, vdx);
    __m512i step1 = _mm512_add_epi32(step0, vdx);
    __m512i c01 = _mm512_set1_epi32(0x00010000);
    __m512i c23 = _mm512_set1_epi32(0x00030002);
    __m512i vminbw = _mm512_set1_epi32(minboxwidth);
    while (dst_width >= 16) {
      int base_ix = x >> 16;
      __m512i xfrac = _mm512_set1_epi32(x & 0xffff);
      x += dx * 16;
      int span = (x >> 16) - base_ix;
      uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)m64, src_ptr + base_ix);
      __m512i s1 = _mm512_maskz_loadu_epi16((__mmask32)(m64 >> 32),
                                            src_ptr + base_ix + 32);
      __m512i ix0 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step0), 16);
      __m512i ix1 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step1), 16);
      __m512i bw = _mm512_sub_epi32(ix1, ix0);
      __mmask16 k4 = _mm512_cmpgt_epi32_mask(bw, vminbw);
      __m512i ix0_16 = _mm512_or_si512(ix0, _mm512_slli_epi32(ix0, 16));
      __m512i bw_16 = _mm512_or_si512(bw, _mm512_slli_epi32(bw, 16));
      __m512i idx01 = _mm512_add_epi16(ix0_16, c01);
      __m512i idx23 = _mm512_add_epi16(ix0_16, c23);
      __mmask32 k2 = _mm512_cmpgt_epu16_mask(bw_16, c01);
      __mmask32 k3 = _mm512_cmpgt_epu16_mask(bw_16, c23);
      __m512i p01 = _mm512_maskz_permutex2var_epi16(k2, s0, idx01, s1);
      __m512i p23 = _mm512_maskz_permutex2var_epi16(k3, s0, idx23, s1);
      __m512i sum01 = _mm512_add_epi32(_mm512_and_si512(p01, mask_ffff),
                                       _mm512_srli_epi32(p01, 16));
      __m512i sum23 = _mm512_add_epi32(_mm512_and_si512(p23, mask_ffff),
                                       _mm512_srli_epi32(p23, 16));
      __m512i sum = _mm512_add_epi32(sum01, sum23);
      __m512i vsc = _mm512_mask_blend_epi32(k4, vscale0, vscale1);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vsc), 16);
      _mm_storeu_si128((__m128i*)dst_ptr, _mm512_cvtepi32_epi8(sum));
      dst_ptr += 16;
      dst_width -= 16;
    }
    if (dst_width > 0) {
      int base_ix = x >> 16;
      __m512i xfrac = _mm512_set1_epi32(x & 0xffff);
      int end_x = x + dx * dst_width;
      int span = (end_x >> 16) - base_ix;
      uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)m64, src_ptr + base_ix);
      __m512i s1 = _mm512_maskz_loadu_epi16((__mmask32)(m64 >> 32),
                                            src_ptr + base_ix + 32);
      __m512i ix0 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step0), 16);
      __m512i ix1 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step1), 16);
      __m512i bw = _mm512_sub_epi32(ix1, ix0);
      __mmask16 k4 = _mm512_cmpgt_epi32_mask(bw, vminbw);
      __m512i ix0_16 = _mm512_or_si512(ix0, _mm512_slli_epi32(ix0, 16));
      __m512i bw_16 = _mm512_or_si512(bw, _mm512_slli_epi32(bw, 16));
      __m512i idx01 = _mm512_add_epi16(ix0_16, c01);
      __m512i idx23 = _mm512_add_epi16(ix0_16, c23);
      __mmask32 k2 = _mm512_cmpgt_epu16_mask(bw_16, c01);
      __mmask32 k3 = _mm512_cmpgt_epu16_mask(bw_16, c23);
      __m512i p01 = _mm512_maskz_permutex2var_epi16(k2, s0, idx01, s1);
      __m512i p23 = _mm512_maskz_permutex2var_epi16(k3, s0, idx23, s1);
      __m512i sum01 = _mm512_add_epi32(_mm512_and_si512(p01, mask_ffff),
                                       _mm512_srli_epi32(p01, 16));
      __m512i sum23 = _mm512_add_epi32(_mm512_and_si512(p23, mask_ffff),
                                       _mm512_srli_epi32(p23, 16));
      __m512i sum = _mm512_add_epi32(sum01, sum23);
      __m512i vsc = _mm512_mask_blend_epi32(k4, vscale0, vscale1);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vsc), 16);
      __mmask16 km = (__mmask16)((1U << dst_width) - 1);
      // Use _mm_mask_storeu_epi8 instead of _mm512_mask_cvtepi32_storeu_epi8
      // because LLVM MSan does not instrument @llvm.x86.avx512.mask.pmov.db.mem
      // to unpoison destination shadow memory.
      _mm_mask_storeu_epi8(dst_ptr, km, _mm512_cvtepi32_epi8(sum));
    }
  } else if (minboxwidth >= 4 && minboxwidth <= 7) {
    __m512i seq =
        _mm512_cvtepu8_epi64(_mm_loadu_si128((const __m128i*)kSequence0_15));
    __m512i vdx = _mm512_set1_epi64(dx);
    __m512i step0 = _mm512_mullo_epi32(seq, vdx);
    __m512i step1 = _mm512_add_epi64(step0, vdx);
    __m512i c03 = _mm512_set1_epi64(0x0003000200010000ULL);
    __m512i c47 = _mm512_set1_epi64(0x0007000600050004ULL);
    __m512i vminbw = _mm512_set1_epi64(minboxwidth);
    while (dst_width >= 8) {
      int base_ix = x >> 16;
      __m512i xfrac = _mm512_set1_epi64(x & 0xffff);
      x += dx * 8;
      int span = (x >> 16) - base_ix;
      uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)m64, src_ptr + base_ix);
      __m512i s1 = _mm512_maskz_loadu_epi16((__mmask32)(m64 >> 32),
                                            src_ptr + base_ix + 32);
      __m512i ix0 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step0), 16);
      __m512i ix1 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step1), 16);
      __m512i bw = _mm512_sub_epi32(ix1, ix0);
      __mmask8 k4 = _mm512_cmpgt_epi64_mask(bw, vminbw);
      __m512i ix0_16 = _mm512_or_si512(ix0, _mm512_slli_epi64(ix0, 16));
      ix0_16 = _mm512_or_si512(ix0_16, _mm512_slli_epi64(ix0_16, 32));
      __m512i bw_16 = _mm512_or_si512(bw, _mm512_slli_epi64(bw, 16));
      bw_16 = _mm512_or_si512(bw_16, _mm512_slli_epi64(bw_16, 32));
      __m512i idx03 = _mm512_add_epi16(ix0_16, c03);
      __m512i idx47 = _mm512_add_epi16(ix0_16, c47);
      __mmask32 k3 = _mm512_cmpgt_epu16_mask(bw_16, c47);
      __m512i p03 = _mm512_permutex2var_epi16(s0, idx03, s1);
      __m512i p47 = _mm512_maskz_permutex2var_epi16(k3, s0, idx47, s1);
      __m512i sum03 = _mm512_add_epi32(_mm512_and_si512(p03, mask_ffff),
                                       _mm512_srli_epi32(p03, 16));
      __m512i sum47 = _mm512_add_epi32(_mm512_and_si512(p47, mask_ffff),
                                       _mm512_srli_epi32(p47, 16));
      __m512i sum = _mm512_add_epi32(sum03, sum47);
      sum = _mm512_add_epi32(sum, _mm512_srli_epi64(sum, 32));
      __m512i vsc = _mm512_mask_blend_epi64(k4, vscale0, vscale1);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vsc), 16);
      _mm_storel_epi64((__m128i*)dst_ptr, _mm512_cvtepi64_epi8(sum));
      dst_ptr += 8;
      dst_width -= 8;
    }
    if (dst_width > 0) {
      int base_ix = x >> 16;
      __m512i xfrac = _mm512_set1_epi64(x & 0xffff);
      int end_x = x + dx * dst_width;
      int span = (end_x >> 16) - base_ix;
      uint64_t m64 = (span >= 64) ? ~0ULL : ((1ULL << span) - 1);
      __m512i s0 = _mm512_maskz_loadu_epi16((__mmask32)m64, src_ptr + base_ix);
      __m512i s1 = _mm512_maskz_loadu_epi16((__mmask32)(m64 >> 32),
                                            src_ptr + base_ix + 32);
      __m512i ix0 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step0), 16);
      __m512i ix1 = _mm512_srli_epi32(_mm512_add_epi32(xfrac, step1), 16);
      __m512i bw = _mm512_sub_epi32(ix1, ix0);
      __mmask8 k4 = _mm512_cmpgt_epi64_mask(bw, vminbw);
      __m512i ix0_16 = _mm512_or_si512(ix0, _mm512_slli_epi64(ix0, 16));
      ix0_16 = _mm512_or_si512(ix0_16, _mm512_slli_epi64(ix0_16, 32));
      __m512i bw_16 = _mm512_or_si512(bw, _mm512_slli_epi64(bw, 16));
      bw_16 = _mm512_or_si512(bw_16, _mm512_slli_epi64(bw_16, 32));
      __m512i idx03 = _mm512_add_epi16(ix0_16, c03);
      __m512i idx47 = _mm512_add_epi16(ix0_16, c47);
      __mmask32 k3 = _mm512_cmpgt_epu16_mask(bw_16, c47);
      __m512i p03 = _mm512_permutex2var_epi16(s0, idx03, s1);
      __m512i p47 = _mm512_maskz_permutex2var_epi16(k3, s0, idx47, s1);
      __m512i sum03 = _mm512_add_epi32(_mm512_and_si512(p03, mask_ffff),
                                       _mm512_srli_epi32(p03, 16));
      __m512i sum47 = _mm512_add_epi32(_mm512_and_si512(p47, mask_ffff),
                                       _mm512_srli_epi32(p47, 16));
      __m512i sum = _mm512_add_epi32(sum03, sum47);
      sum = _mm512_add_epi32(sum, _mm512_srli_epi64(sum, 32));
      __m512i vsc = _mm512_mask_blend_epi64(k4, vscale0, vscale1);
      sum = _mm512_srli_epi32(_mm512_mullo_epi32(sum, vsc), 16);
      __mmask8 km = (__mmask8)((1U << dst_width) - 1);
      // Use _mm_mask_storeu_epi8 instead of _mm512_mask_cvtepi64_storeu_epi8
      // because LLVM MSan does not instrument @llvm.x86.avx512.mask.pmov.qb.mem
      // to unpoison destination shadow memory.
      _mm_mask_storeu_epi8(dst_ptr, km, _mm512_cvtepi64_epi8(sum));
    }
  } else {
    while (dst_width > 0) {
      int ix = x >> 16;
      x += dx;
      int bw = (x >> 16) - ix;
      if (bw < 1) {
        bw = 1;
      }
      int sc = (bw > minboxwidth) ? scale1 : scale0;
      __m512i acc = _mm512_setzero_si512();
      const uint16_t* p = src_ptr + ix;
      int n = bw;
      while (n >= 32) {
        __m512i s = _mm512_loadu_si512((const __m512i*)p);
        p += 32;
        acc = _mm512_add_epi32(acc,
                               _mm512_add_epi32(_mm512_and_si512(s, mask_ffff),
                                                _mm512_srli_epi32(s, 16)));
        n -= 32;
      }
      if (n > 0) {
        __mmask32 k1 = (__mmask32)((1ULL << n) - 1);
        __m512i s = _mm512_maskz_loadu_epi16(k1, p);
        acc = _mm512_add_epi32(acc,
                               _mm512_add_epi32(_mm512_and_si512(s, mask_ffff),
                                                _mm512_srli_epi32(s, 16)));
      }
      uint32_t total = (uint32_t)_mm512_reduce_add_epi32(acc);
      *dst_ptr++ = (uint8_t)((total * (uint32_t)sc) >> 16);
      --dst_width;
    }
  }
}
#endif  // HAS_SCALEADDCOLS2_AVX512BW

#ifdef HAS_SCALEUVADDCOLS3_AVX2
static const ulvec32 kPermUV3_0 = {0, 3, 6, 1, 4, 7, 2, 5};
static const ulvec32 kPermUV3_1 = {3, 4, 5, 6, 7, 0, 1, 2};
static const ulvec32 kPermUV3_2 = {6, 7, 0, 1, 2, 3, 4, 5};

// Scale UV columns by 3 box filter.
// Reads 96 bytes (24 UV pixels) and writes 16 bytes (8 UV pixels) per block.
LIBYUV_TARGET_AVX2
void ScaleUVAddCols3_AVX2(int dst_width,
                          int scaleval,
                          const uint16_t* src_uv,
                          uint8_t* dst_uv) {
  const __m256i vscale = _mm256_set1_epi16((int16_t)scaleval);
  const __m256i perm0 = _mm256_loadu_si256((const __m256i*)&kPermUV3_0);
  const __m256i perm1 = _mm256_loadu_si256((const __m256i*)&kPermUV3_1);
  const __m256i perm2 = _mm256_loadu_si256((const __m256i*)&kPermUV3_2);
  while (dst_width >= 16) {
    __m256i a0 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)src_uv), perm0);
    __m256i a1 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 16)), perm0);
    __m256i a2 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 32)), perm0);
    __m256i c0 = _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0x38), a2, 0xc0);
    __m256i c1 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0xc0), a2, 0x07), perm1);
    __m256i c2 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0x07), a2, 0x38), perm2);
    __m256i sum0 = _mm256_mulhi_epu16(
        _mm256_add_epi16(_mm256_add_epi16(c0, c1), c2), vscale);

    __m256i b0 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 48)), perm0);
    __m256i b1 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 64)), perm0);
    __m256i b2 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 80)), perm0);
    src_uv += 96;
    __m256i d0 = _mm256_blend_epi32(_mm256_blend_epi32(b0, b1, 0x38), b2, 0xc0);
    __m256i d1 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(b0, b1, 0xc0), b2, 0x07), perm1);
    __m256i d2 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(b0, b1, 0x07), b2, 0x38), perm2);
    __m256i sum1 = _mm256_mulhi_epu16(
        _mm256_add_epi16(_mm256_add_epi16(d0, d1), d2), vscale);

    __m256i packed =
        _mm256_permute4x64_epi64(_mm256_packus_epi16(sum0, sum1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_uv, packed);
    dst_uv += 32;
    dst_width -= 16;
  }
  while (dst_width > 0) {
    __m256i a0 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)src_uv), perm0);
    __m256i a1 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 16)), perm0);
    __m256i a2 = _mm256_permutevar8x32_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 32)), perm0);
    src_uv += 48;
    __m256i c0 = _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0x38), a2, 0xc0);
    __m256i c1 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0xc0), a2, 0x07), perm1);
    __m256i c2 = _mm256_permutevar8x32_epi32(
        _mm256_blend_epi32(_mm256_blend_epi32(a0, a1, 0x07), a2, 0x38), perm2);
    __m256i sum = _mm256_mulhi_epu16(
        _mm256_add_epi16(_mm256_add_epi16(c0, c1), c2), vscale);
    __m128i lo = _mm256_castsi256_si128(sum);
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    _mm_storeu_si128((__m128i*)dst_uv, _mm_packus_epi16(lo, hi));
    dst_uv += 16;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEUVADDCOLS3_AVX2

#ifdef HAS_SCALEUVADDCOLS3_AVX512VBMI
static const uint8_t kPerm512_Col0[16] = {0,  3,  6,  9, 12, 15, 18, 21,
                                          24, 27, 30, 1, 4,  7,  10, 13};
static const uint8_t kPerm512_Col1[16] = {1,  4,  7,  10, 13, 16, 19, 22,
                                          25, 28, 31, 2,  5,  8,  11, 14};
static const uint8_t kPerm512_Col2[16] = {2,  5,  8, 11, 14, 17, 20, 23,
                                          26, 29, 0, 3,  6,  9,  12, 15};
static const uint8_t kPackEvenBytes[64] = {
    0,   2,   4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,
    26,  28,  30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,
    52,  54,  56,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
    78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98,  100, 102,
    104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126};

// Under MemorySanitizer, use a portable lane-by-lane C loop so shadow
// propagation relies only on standard C memory/array semantics rather than
// target-specific LLVM intrinsic modeling. In non-MSan builds, call the native
// AVX-512 intrinsic directly.
LIBYUV_TARGET_AVX512VBMI
static inline __m512i MaskPermutexvarEpi32(__m512i src,
                                           __mmask16 k,
                                           __m512i idx,
                                           __m512i v) {
#if defined(LIBYUV_MSAN)
  uint32_t src_arr[16];
  uint32_t idx_arr[16];
  uint32_t v_arr[16];
  uint32_t out_arr[16];
  _mm512_storeu_si512((__m512i*)src_arr, src);
  _mm512_storeu_si512((__m512i*)idx_arr, idx);
  _mm512_storeu_si512((__m512i*)v_arr, v);
  for (int i = 0; i < 16; ++i) {
    out_arr[i] = ((k >> i) & 1) ? v_arr[idx_arr[i] & 15] : src_arr[i];
  }
  return _mm512_loadu_si512((const __m512i*)out_arr);
#else
  return _mm512_mask_permutexvar_epi32(src, k, idx, v);
#endif
}

// Scale UV columns by 3 box filter using AVX512VBMI.
// Processes 32 UV pixels (384 input bytes -> 64 output bytes) per main loop
// iteration, followed by a 16-pixel block and a k-masked tail (1..15 pixels).
LIBYUV_TARGET_AVX512VBMI
void ScaleUVAddCols3_AVX512VBMI(int dst_width,
                                int scaleval,
                                const uint16_t* src_uv,
                                uint8_t* dst_uv) {
  const __m512i vscale = _mm512_set1_epi16((int16_t)scaleval);
  const __m512i col0 =
      _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)kPerm512_Col0));
  const __m512i col1 =
      _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)kPerm512_Col1));
  const __m512i col2 =
      _mm512_cvtepu8_epi32(_mm_loadu_si128((const __m128i*)kPerm512_Col2));
  const __m512i pack_even = _mm512_loadu_si512((const __m512i*)kPackEvenBytes);
  const __mmask16 k5 = 0xf800;
  const __mmask16 k6 = 0xfc00;

  while (dst_width >= 32) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_uv);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)(src_uv + 32));
    __m512i s2 = _mm512_loadu_si512((const __m512i*)(src_uv + 64));
    __m512i a0 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col0, s1),
                                      k5, col0, s2);
    __m512i a1 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col1, s1),
                                      k5, col1, s2);
    __m512i a2 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col2, s1),
                                      k6, col2, s2);
    __m512i sum0 = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_add_epi16(a0, a1), a2), vscale);

    __m512i s3 = _mm512_loadu_si512((const __m512i*)(src_uv + 96));
    __m512i s4 = _mm512_loadu_si512((const __m512i*)(src_uv + 128));
    __m512i s5 = _mm512_loadu_si512((const __m512i*)(src_uv + 160));
    src_uv += 192;
    __m512i b0 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s3, col0, s4),
                                      k5, col0, s5);
    __m512i b1 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s3, col1, s4),
                                      k5, col1, s5);
    __m512i b2 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s3, col2, s4),
                                      k6, col2, s5);
    __m512i sum1 = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_add_epi16(b0, b1), b2), vscale);

    __m512i out = _mm512_permutex2var_epi8(sum0, pack_even, sum1);
    _mm512_storeu_si512((__m512i*)dst_uv, out);
    dst_uv += 64;
    dst_width -= 32;
  }
  if (dst_width >= 16) {
    __m512i s0 = _mm512_loadu_si512((const __m512i*)src_uv);
    __m512i s1 = _mm512_loadu_si512((const __m512i*)(src_uv + 32));
    __m512i s2 = _mm512_loadu_si512((const __m512i*)(src_uv + 64));
    src_uv += 96;
    __m512i a0 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col0, s1),
                                      k5, col0, s2);
    __m512i a1 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col1, s1),
                                      k5, col1, s2);
    __m512i a2 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col2, s1),
                                      k6, col2, s2);
    __m512i sum0 = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_add_epi16(a0, a1), a2), vscale);
    _mm256_storeu_si256((__m256i*)dst_uv, _mm512_cvtepi16_epi8(sum0));
    dst_uv += 32;
    dst_width -= 16;
  }
  if (dst_width > 0) {
    uint64_t m64 = (1ULL << (dst_width * 3)) - 1;
    __mmask16 k1 = (__mmask16)m64;
    __mmask16 k2 = (__mmask16)(m64 >> 16);
    __mmask16 k3 = (__mmask16)(m64 >> 32);
    __mmask16 k4 = (__mmask16)((1U << dst_width) - 1);
    __m512i s0 = _mm512_maskz_loadu_epi32(k1, src_uv);
    __m512i s1 = _mm512_maskz_loadu_epi32(k2, src_uv + 32);
    __m512i s2 = _mm512_maskz_loadu_epi32(k3, src_uv + 64);
    __m512i a0 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col0, s1),
                                      k5, col0, s2);
    __m512i a1 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col1, s1),
                                      k5, col1, s2);
    __m512i a2 = MaskPermutexvarEpi32(_mm512_permutex2var_epi32(s0, col2, s1),
                                      k6, col2, s2);
    __m512i sum0 = _mm512_mulhi_epu16(
        _mm512_add_epi16(_mm512_add_epi16(a0, a1), a2), vscale);
    _mm256_mask_storeu_epi16(dst_uv, k4, _mm512_cvtepi16_epi8(sum0));
  }
}
#endif  // HAS_SCALEUVADDCOLS3_AVX512VBMI

#if defined(HAS_SCALEROWUP2_LINEAR_AVX2) || \
    defined(HAS_SCALEROWUP2_BILINEAR_AVX2)
static const uvec8 kLinearMadd31 = {3, 1, 1, 3, 3, 1, 1, 3,
                                    3, 1, 1, 3, 3, 1, 1, 3};
#endif

#if defined(HAS_SCALEROWUP2_LINEAR_12_AVX2) || \
    defined(HAS_SCALEROWUP2_BILINEAR_12_AVX2)
static const uvec8 kLinearShuffleFar = {2,  3,  0, 1, 6,  7,  4,  5,
                                        10, 11, 8, 9, 14, 15, 12, 13};
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Linear_AVX2(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int dst_width) {
  const __m256i madd31 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kLinearMadd31));
  const __m256i round2 = _mm256_set1_epi16(2);
  while (dst_width > 0) {
    __m256i s0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    __m256i s1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)(src_ptr + 1))),
        0xd8);
    src_ptr += 16;
    s0 = _mm256_unpacklo_epi16(s0, s0);
    s1 = _mm256_unpacklo_epi16(s1, s1);
    __m256i lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(s0, s1), madd31);
    __m256i hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(s0, s1), madd31);
    lo = _mm256_srli_epi16(_mm256_add_epi16(lo, round2), 2);
    hi = _mm256_srli_epi16(_mm256_add_epi16(hi, round2), 2);
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi16(lo, hi));
    dst_ptr += 32;
    dst_width -= 32;
  }
}
#endif  // HAS_SCALEROWUP2_LINEAR_AVX2

#ifdef HAS_SCALEROWUP2_BILINEAR_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Bilinear_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width) {
  const __m256i madd31 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kLinearMadd31));
  const __m256i round8 = _mm256_set1_epi16(8);
  while (dst_width > 0) {
    __m256i a0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    __m256i a1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)(src_ptr + 1))),
        0xd8);
    a0 = _mm256_unpacklo_epi16(a0, a0);
    a1 = _mm256_unpacklo_epi16(a1, a1);
    __m256i r0_lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(a0, a1), madd31);
    __m256i r0_hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(a0, a1), madd31);

    __m256i b0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride))),
        0xd8);
    __m256i b1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride + 1))),
        0xd8);
    src_ptr += 16;
    b0 = _mm256_unpacklo_epi16(b0, b0);
    b1 = _mm256_unpacklo_epi16(b1, b1);
    __m256i r1_lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(b0, b1), madd31);
    __m256i r1_hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(b0, b1), madd31);

    __m256i d0_lo = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r0_lo, r0_lo), r0_lo),
                         _mm256_add_epi16(r1_lo, round8)),
        4);
    __m256i d1_lo = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r1_lo, r1_lo), r1_lo),
                         _mm256_add_epi16(r0_lo, round8)),
        4);
    __m256i d0_hi = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r0_hi, r0_hi), r0_hi),
                         _mm256_add_epi16(r1_hi, round8)),
        4);
    __m256i d1_hi = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r1_hi, r1_hi), r1_hi),
                         _mm256_add_epi16(r0_hi, round8)),
        4);

    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi16(d0_lo, d0_hi));
    _mm256_storeu_si256((__m256i*)(dst_ptr + dst_stride),
                        _mm256_packus_epi16(d1_lo, d1_hi));
    dst_ptr += 32;
    dst_width -= 32;
  }
}
#endif  // HAS_SCALEROWUP2_BILINEAR_AVX2

#ifdef HAS_SCALEROWUP2_LINEAR_12_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Linear_12_AVX2(const uint16_t* src_ptr,
                                uint16_t* dst_ptr,
                                int dst_width) {
  const __m256i shuf_far = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kLinearShuffleFar));
  const __m256i round2 = _mm256_set1_epi16(2);
  while (dst_width > 0) {
    __m256i s0 =
        _mm256_permute4x64_epi64(_mm256_loadu_si256((const __m256i*)src_ptr),
                                 0xd8);
    __m256i s1 = _mm256_permute4x64_epi64(
        _mm256_loadu_si256((const __m256i*)(src_ptr + 1)), 0xd8);
    src_ptr += 16;
    __m256i near_lo = _mm256_unpacklo_epi16(s0, s1);
    __m256i near_hi = _mm256_unpackhi_epi16(s0, s1);
    __m256i far_lo = _mm256_shuffle_epi8(near_lo, shuf_far);
    __m256i far_hi = _mm256_shuffle_epi8(near_hi, shuf_far);
    __m256i d_lo = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(near_lo, near_lo),
                         _mm256_add_epi16(near_lo, _mm256_add_epi16(far_lo, round2))),
        2);
    __m256i d_hi = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(near_hi, near_hi),
                         _mm256_add_epi16(near_hi, _mm256_add_epi16(far_hi, round2))),
        2);
    _mm256_storeu_si256((__m256i*)dst_ptr, d_lo);
    _mm256_storeu_si256((__m256i*)(dst_ptr + 16), d_hi);
    dst_ptr += 32;
    dst_width -= 32;
  }
}
#endif  // HAS_SCALEROWUP2_LINEAR_12_AVX2

#ifdef HAS_SCALEROWUP2_BILINEAR_12_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Bilinear_12_AVX2(const uint16_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint16_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  const __m256i shuf_far = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kLinearShuffleFar));
  const __m256i round8 = _mm256_set1_epi16(8);
  while (dst_width > 0) {
    __m256i a0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    __m256i a1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)(src_ptr + 1))),
        0xd8);
    __m256i near0 = _mm256_unpacklo_epi16(a0, a1);
    __m256i far0 = _mm256_shuffle_epi8(near0, shuf_far);
    __m256i r0 = _mm256_add_epi16(_mm256_add_epi16(near0, near0),
                                  _mm256_add_epi16(near0, far0));

    __m256i b0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride))),
        0xd8);
    __m256i b1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride + 1))),
        0xd8);
    src_ptr += 8;
    __m256i near1 = _mm256_unpacklo_epi16(b0, b1);
    __m256i far1 = _mm256_shuffle_epi8(near1, shuf_far);
    __m256i r1 = _mm256_add_epi16(_mm256_add_epi16(near1, near1),
                                  _mm256_add_epi16(near1, far1));

    __m256i d0 = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r0, r0), r0),
                         _mm256_add_epi16(r1, round8)),
        4);
    __m256i d1 = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r1, r1), r1),
                         _mm256_add_epi16(r0, round8)),
        4);
    _mm256_storeu_si256((__m256i*)dst_ptr, d0);
    _mm256_storeu_si256((__m256i*)(dst_ptr + dst_stride), d1);
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWUP2_BILINEAR_12_AVX2

#ifdef HAS_SCALEROWUP2_LINEAR_16_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Linear_16_AVX2(const uint16_t* src_ptr,
                                uint16_t* dst_ptr,
                                int dst_width) {
  const __m256i round2 = _mm256_set1_epi32(2);
  while (dst_width > 0) {
    __m256i near_lo =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_ptr));
    __m256i near_hi =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)(src_ptr + 1)));
    src_ptr += 8;
    __m256i far_lo = _mm256_shuffle_epi32(near_lo, 0xb1);
    __m256i far_hi = _mm256_shuffle_epi32(near_hi, 0xb1);
    __m256i d_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(near_lo, near_lo),
                         _mm256_add_epi32(near_lo, _mm256_add_epi32(far_lo, round2))),
        2);
    __m256i d_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(near_hi, near_hi),
                         _mm256_add_epi32(near_hi, _mm256_add_epi32(far_hi, round2))),
        2);
    __m256i d = _mm256_shuffle_epi32(_mm256_packus_epi32(d_lo, d_hi), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_ptr, d);
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWUP2_LINEAR_16_AVX2

#ifdef HAS_SCALEROWUP2_BILINEAR_16_AVX2
LIBYUV_TARGET_AVX2
void ScaleRowUp2_Bilinear_16_AVX2(const uint16_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint16_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  const __m256i round8 = _mm256_set1_epi32(8);
  while (dst_width > 0) {
    __m256i a0 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_ptr));
    __m256i a1 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)(src_ptr + 1)));
    __m256i r0_lo = _mm256_add_epi32(
        _mm256_add_epi32(a0, a0),
        _mm256_add_epi32(a0, _mm256_shuffle_epi32(a0, 0xb1)));
    __m256i r0_hi = _mm256_add_epi32(
        _mm256_add_epi32(a1, a1),
        _mm256_add_epi32(a1, _mm256_shuffle_epi32(a1, 0xb1)));

    __m256i b0 = _mm256_cvtepu16_epi32(
        _mm_loadu_si128((const __m128i*)(src_ptr + src_stride)));
    __m256i b1 = _mm256_cvtepu16_epi32(
        _mm_loadu_si128((const __m128i*)(src_ptr + src_stride + 1)));
    src_ptr += 8;
    __m256i r1_lo = _mm256_add_epi32(
        _mm256_add_epi32(b0, b0),
        _mm256_add_epi32(b0, _mm256_shuffle_epi32(b0, 0xb1)));
    __m256i r1_hi = _mm256_add_epi32(
        _mm256_add_epi32(b1, b1),
        _mm256_add_epi32(b1, _mm256_shuffle_epi32(b1, 0xb1)));

    __m256i d0_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r0_lo, r0_lo), r0_lo),
                         _mm256_add_epi32(r1_lo, round8)),
        4);
    __m256i d1_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r1_lo, r1_lo), r1_lo),
                         _mm256_add_epi32(r0_lo, round8)),
        4);
    __m256i d0_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r0_hi, r0_hi), r0_hi),
                         _mm256_add_epi32(r1_hi, round8)),
        4);
    __m256i d1_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r1_hi, r1_hi), r1_hi),
                         _mm256_add_epi32(r0_hi, round8)),
        4);

    _mm256_storeu_si256(
        (__m256i*)dst_ptr,
        _mm256_shuffle_epi32(_mm256_packus_epi32(d0_lo, d0_hi), 0xd8));
    _mm256_storeu_si256(
        (__m256i*)(dst_ptr + dst_stride),
        _mm256_shuffle_epi32(_mm256_packus_epi32(d1_lo, d1_hi), 0xd8));
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWUP2_BILINEAR_16_AVX2

#ifdef HAS_SCALEFILTERCOLS_AVX2
static const int32_t kStep0_7_Filter[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const uvec8 kShufFracCols = {0,   0,   4,   4,   8,   8,   12,  12,
                                    128, 128, 128, 128, 128, 128, 128, 128};

LIBYUV_TARGET_AVX2
void ScaleFilterCols_AVX2(uint8_t* dst_ptr,
                          const uint8_t* src_ptr,
                          int dst_width,
                          int x,
                          int dx) {
  int n = dst_width & ~7;
  int r = dst_width & 7;
  if (n > 0) {
    SIMD_ALIGNED(int32_t xi_buf[8]);
    SIMD_ALIGNED(uint16_t pix_buf[8]);
    __m256i vx = _mm256_add_epi32(
        _mm256_set1_epi32(x),
        _mm256_mullo_epi32(
            _mm256_set1_epi32(dx),
            _mm256_loadu_si256((const __m256i*)kStep0_7_Filter)));
    const __m256i vdx8 = _mm256_slli_epi32(_mm256_set1_epi32(dx), 3);
    const __m256i shuf_frac = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i*)&kShufFracCols));
    const __m128i mask_7f = _mm_set1_epi16(0x007f);
    const __m128i mask_ff = _mm_set1_epi16(0x00ff);
    const __m128i round_40 = _mm_set1_epi16(0x0040);
    for (int i = 0; i < n; i += 8) {
      _mm256_store_si256((__m256i*)xi_buf, _mm256_srai_epi32(vx, 16));
      __m256i f256 = _mm256_permute4x64_epi64(
          _mm256_shuffle_epi8(_mm256_srli_epi16(vx, 9), shuf_frac), 0xd8);
      vx = _mm256_add_epi32(vx, vdx8);
      __m128i weights = _mm_xor_si128(_mm256_castsi256_si128(f256), mask_7f);
      pix_buf[0] = LoadU16(src_ptr + xi_buf[0]);
      pix_buf[1] = LoadU16(src_ptr + xi_buf[1]);
      pix_buf[2] = LoadU16(src_ptr + xi_buf[2]);
      pix_buf[3] = LoadU16(src_ptr + xi_buf[3]);
      pix_buf[4] = LoadU16(src_ptr + xi_buf[4]);
      pix_buf[5] = LoadU16(src_ptr + xi_buf[5]);
      pix_buf[6] = LoadU16(src_ptr + xi_buf[6]);
      pix_buf[7] = LoadU16(src_ptr + xi_buf[7]);
      __m128i pix = _mm_load_si128((const __m128i*)pix_buf);
      __m128i a = _mm_and_si128(pix, mask_ff);
      __m128i res = _mm_srli_epi16(
          _mm_add_epi16(_mm_add_epi16(_mm_maddubs_epi16(pix, weights), a),
                        round_40),
          7);
      _mm_storel_epi64((__m128i*)dst_ptr, _mm_packus_epi16(res, res));
      dst_ptr += 8;
    }
    x = (int)((uint32_t)x + (uint32_t)n * (uint32_t)dx);
  }
  for (int j = 0; j < r; ++j) {
    int xi = x >> 16;
    int a = src_ptr[xi];
    int b = src_ptr[xi + 1];
    dst_ptr[j] = (uint8_t)(a + ((((x & 0xffff) >> 9) * (b - a) + 0x40) >> 7));
    x = (int)((uint32_t)x + (uint32_t)dx);
  }
}
#endif  // HAS_SCALEFILTERCOLS_AVX2

#ifdef HAS_SCALECOLSUP2_AVX2
LIBYUV_TARGET_AVX2
void ScaleColsUp2_AVX2(uint8_t* dst_ptr,
                       const uint8_t* src_ptr,
                       int dst_width,
                       int x,
                       int dx) {
  (void)x;
  (void)dx;
  while (dst_width >= 32) {
    __m256i s = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    src_ptr += 16;
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_unpacklo_epi8(s, s));
    dst_ptr += 32;
    dst_width -= 32;
  }
  if (dst_width & 16) {
    __m128i s = _mm_loadl_epi64((const __m128i*)src_ptr);
    src_ptr += 8;
    _mm_storeu_si128((__m128i*)dst_ptr, _mm_unpacklo_epi8(s, s));
    dst_ptr += 16;
  }
  if (dst_width & 8) {
    __m128i s = _mm_cvtsi32_si128((int)LoadU32(src_ptr));
    _mm_storel_epi64((__m128i*)dst_ptr, _mm_unpacklo_epi8(s, s));
  }
}
#endif  // HAS_SCALECOLSUP2_AVX2

#ifdef HAS_SCALEARGBROWDOWN2_AVX2
LIBYUV_TARGET_AVX2
void ScaleARGBRowDown2_AVX2(const uint8_t* src_argb,
                            ptrdiff_t src_stride,
                            uint8_t* dst_argb,
                            int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    __m256 s0 = _mm256_loadu_ps((const float*)src_argb);
    __m256 s1 = _mm256_loadu_ps((const float*)(src_argb + 32));
    src_argb += 64;
    __m256i d = _mm256_permute4x64_epi64(
        _mm256_castps_si256(_mm256_shuffle_ps(s0, s1, 0xdd)), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, d);
    dst_argb += 32;
    dst_width -= 8;
  }
}

LIBYUV_TARGET_AVX2
void ScaleARGBRowDown2Linear_AVX2(const uint8_t* src_argb,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_argb,
                                  int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    __m256 s0 = _mm256_loadu_ps((const float*)src_argb);
    __m256 s1 = _mm256_loadu_ps((const float*)(src_argb + 32));
    src_argb += 64;
    __m256i e = _mm256_castps_si256(_mm256_shuffle_ps(s0, s1, 0x88));
    __m256i o = _mm256_castps_si256(_mm256_shuffle_ps(s0, s1, 0xdd));
    __m256i d = _mm256_permute4x64_epi64(_mm256_avg_epu8(e, o), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, d);
    dst_argb += 32;
    dst_width -= 8;
  }
}

LIBYUV_TARGET_AVX2
void ScaleARGBRowDown2Box_AVX2(const uint8_t* src_argb,
                               ptrdiff_t src_stride,
                               uint8_t* dst_argb,
                               int dst_width) {
  while (dst_width > 0) {
    __m256i r0 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)src_argb),
        _mm256_loadu_si256((const __m256i*)(src_argb + src_stride)));
    __m256i r1 = _mm256_avg_epu8(
        _mm256_loadu_si256((const __m256i*)(src_argb + 32)),
        _mm256_loadu_si256((const __m256i*)(src_argb + src_stride + 32)));
    src_argb += 64;
    __m256i e = _mm256_castps_si256(
        _mm256_shuffle_ps(_mm256_castsi256_ps(r0), _mm256_castsi256_ps(r1),
                          0x88));
    __m256i o = _mm256_castps_si256(
        _mm256_shuffle_ps(_mm256_castsi256_ps(r0), _mm256_castsi256_ps(r1),
                          0xdd));
    __m256i d = _mm256_permute4x64_epi64(_mm256_avg_epu8(e, o), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, d);
    dst_argb += 32;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEARGBROWDOWN2_AVX2

#ifdef HAS_SCALEARGBROWDOWNEVEN_AVX2
LIBYUV_TARGET_AVX2
void ScaleARGBRowDownEven_AVX2(const uint8_t* src_argb,
                               ptrdiff_t src_stride,
                               int src_stepx,
                               uint8_t* dst_argb,
                               int dst_width) {
  (void)src_stride;
  const ptrdiff_t step = (ptrdiff_t)src_stepx * 4;
  SIMD_ALIGNED(uint32_t pix_buf[8]);
  while (dst_width > 0) {
    pix_buf[0] = LoadU32(src_argb);
    pix_buf[1] = LoadU32(src_argb + step);
    pix_buf[2] = LoadU32(src_argb + step * 2);
    pix_buf[3] = LoadU32(src_argb + step * 3);
    pix_buf[4] = LoadU32(src_argb + step * 4);
    pix_buf[5] = LoadU32(src_argb + step * 5);
    pix_buf[6] = LoadU32(src_argb + step * 6);
    pix_buf[7] = LoadU32(src_argb + step * 7);
    src_argb += step * 8;
    _mm256_storeu_si256((__m256i*)dst_argb,
                        _mm256_load_si256((const __m256i*)pix_buf));
    dst_argb += 32;
    dst_width -= 8;
  }
}

LIBYUV_TARGET_AVX2
void ScaleARGBRowDownEvenBox_AVX2(const uint8_t* src_argb,
                                  ptrdiff_t src_stride,
                                  int src_stepx,
                                  uint8_t* dst_argb,
                                  int dst_width) {
  const uint8_t* row1 = src_argb + src_stride;
  const ptrdiff_t step = (ptrdiff_t)src_stepx * 4;
  SIMD_ALIGNED(uint64_t r0_lo[4]);
  SIMD_ALIGNED(uint64_t r0_hi[4]);
  SIMD_ALIGNED(uint64_t r1_lo[4]);
  SIMD_ALIGNED(uint64_t r1_hi[4]);
  while (dst_width > 0) {
    r0_lo[0] = LoadU64(src_argb);
    r0_lo[1] = LoadU64(src_argb + step);
    r0_lo[2] = LoadU64(src_argb + step * 2);
    r0_lo[3] = LoadU64(src_argb + step * 3);
    r0_hi[0] = LoadU64(src_argb + step * 4);
    r0_hi[1] = LoadU64(src_argb + step * 5);
    r0_hi[2] = LoadU64(src_argb + step * 6);
    r0_hi[3] = LoadU64(src_argb + step * 7);
    r1_lo[0] = LoadU64(row1);
    r1_lo[1] = LoadU64(row1 + step);
    r1_lo[2] = LoadU64(row1 + step * 2);
    r1_lo[3] = LoadU64(row1 + step * 3);
    r1_hi[0] = LoadU64(row1 + step * 4);
    r1_hi[1] = LoadU64(row1 + step * 5);
    r1_hi[2] = LoadU64(row1 + step * 6);
    r1_hi[3] = LoadU64(row1 + step * 7);
    src_argb += step * 8;
    row1 += step * 8;
    __m256i a0 = _mm256_avg_epu8(_mm256_load_si256((const __m256i*)r0_lo),
                                 _mm256_load_si256((const __m256i*)r1_lo));
    __m256i a1 = _mm256_avg_epu8(_mm256_load_si256((const __m256i*)r0_hi),
                                 _mm256_load_si256((const __m256i*)r1_hi));
    __m256i e = _mm256_castps_si256(
        _mm256_shuffle_ps(_mm256_castsi256_ps(a0), _mm256_castsi256_ps(a1),
                          0x88));
    __m256i o = _mm256_castps_si256(
        _mm256_shuffle_ps(_mm256_castsi256_ps(a0), _mm256_castsi256_ps(a1),
                          0xdd));
    __m256i d = _mm256_permute4x64_epi64(_mm256_avg_epu8(e, o), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_argb, d);
    dst_argb += 32;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEARGBROWDOWNEVEN_AVX2

#ifdef HAS_SCALEARGBCOLS_AVX2
static const int32_t kStep0_7_Cols[8] = {0, 1, 2, 3, 4, 5, 6, 7};

LIBYUV_TARGET_AVX2
void ScaleARGBCols_AVX2(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  int n = dst_width & ~7;
  int r = dst_width & 7;
  if (n > 0) {
    SIMD_ALIGNED(int32_t xi_buf[8]);
    SIMD_ALIGNED(uint32_t pix_buf[8]);
    __m256i vx = _mm256_add_epi32(
        _mm256_set1_epi32(x),
        _mm256_mullo_epi32(
            _mm256_set1_epi32(dx),
            _mm256_loadu_si256((const __m256i*)kStep0_7_Cols)));
    const __m256i vdx8 = _mm256_slli_epi32(_mm256_set1_epi32(dx), 3);
    for (int i = 0; i < n; i += 8) {
      _mm256_store_si256((__m256i*)xi_buf, _mm256_srai_epi32(vx, 16));
      vx = _mm256_add_epi32(vx, vdx8);
      pix_buf[0] = LoadU32(src_argb + (ptrdiff_t)xi_buf[0] * 4);
      pix_buf[1] = LoadU32(src_argb + (ptrdiff_t)xi_buf[1] * 4);
      pix_buf[2] = LoadU32(src_argb + (ptrdiff_t)xi_buf[2] * 4);
      pix_buf[3] = LoadU32(src_argb + (ptrdiff_t)xi_buf[3] * 4);
      pix_buf[4] = LoadU32(src_argb + (ptrdiff_t)xi_buf[4] * 4);
      pix_buf[5] = LoadU32(src_argb + (ptrdiff_t)xi_buf[5] * 4);
      pix_buf[6] = LoadU32(src_argb + (ptrdiff_t)xi_buf[6] * 4);
      pix_buf[7] = LoadU32(src_argb + (ptrdiff_t)xi_buf[7] * 4);
      _mm256_storeu_si256((__m256i*)dst_argb,
                          _mm256_load_si256((const __m256i*)pix_buf));
      dst_argb += 32;
    }
    x = (int)((uint32_t)x + (uint32_t)n * (uint32_t)dx);
  }
  for (int j = 0; j < r; ++j) {
    const uint8_t* src = src_argb + (ptrdiff_t)(x >> 16) * 4;
    dst_argb[0] = src[0];
    dst_argb[1] = src[1];
    dst_argb[2] = src[2];
    dst_argb[3] = src[3];
    dst_argb += 4;
    x = (int)((uint32_t)x + (uint32_t)dx);
  }
}
#endif  // HAS_SCALEARGBCOLS_AVX2

#ifdef HAS_SCALEARGBCOLSUP2_AVX2
LIBYUV_TARGET_AVX2
void ScaleARGBColsUp2_AVX2(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx) {
  (void)x;
  (void)dx;
  while (dst_width > 0) {
    __m256i s = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_argb)),
        0xd8);
    src_argb += 16;
    _mm256_storeu_si256((__m256i*)dst_argb, _mm256_unpacklo_epi32(s, s));
    dst_argb += 32;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEARGBCOLSUP2_AVX2

#ifdef HAS_SCALEARGBFILTERCOLS_AVX2
static const int32_t kStep0_3_ARGB[4] = {0, 1, 2, 3};
static const uvec8 kShuffleColARGB = {
    0u, 4u,  1u, 5u,  2u,  6u,  3u,  7u,
    8u, 12u, 9u, 13u, 10u, 14u, 11u, 15u};
static const uint8_t kShuffleFractionsAVX2[32] = {
    0u, 0u, 0u, 0u, 0u,  0u,  0u,  0u,  4u,  4u,  4u,  4u,  4u,  4u,  4u,  4u,
    8u, 8u, 8u, 8u, 8u,  8u,  8u,  8u,  12u, 12u, 12u, 12u, 12u, 12u, 12u, 12u,
};

LIBYUV_TARGET_AVX2
void ScaleARGBFilterCols_AVX2(uint8_t* dst_argb,
                              const uint8_t* src_argb,
                              int dst_width,
                              int x,
                              int dx) {
  int n = dst_width & ~3;
  int r = dst_width & 3;
  if (n > 0) {
    SIMD_ALIGNED(int32_t xi_buf[4]);
    SIMD_ALIGNED(uint64_t pix_buf[4]);
    const __m256i shuf_argb = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i*)&kShuffleColARGB));
    const __m256i shuf_frac =
        _mm256_loadu_si256((const __m256i*)kShuffleFractionsAVX2);
    const __m256i mask_7f = _mm256_set1_epi16(0x007f);
    __m128i vx = _mm_add_epi32(
        _mm_set1_epi32(x),
        _mm_mullo_epi32(_mm_set1_epi32(dx),
                        _mm_loadu_si128((const __m128i*)kStep0_3_ARGB)));
    const __m128i vdx4 = _mm_slli_epi32(_mm_set1_epi32(dx), 2);
    for (int i = 0; i < n; i += 4) {
      _mm_store_si128((__m128i*)xi_buf, _mm_srai_epi32(vx, 16));
      __m128i f128 = _mm_srli_epi16(vx, 9);
      vx = _mm_add_epi32(vx, vdx4);
      pix_buf[0] = LoadU64(src_argb + (ptrdiff_t)xi_buf[0] * 4);
      pix_buf[1] = LoadU64(src_argb + (ptrdiff_t)xi_buf[1] * 4);
      pix_buf[2] = LoadU64(src_argb + (ptrdiff_t)xi_buf[2] * 4);
      pix_buf[3] = LoadU64(src_argb + (ptrdiff_t)xi_buf[3] * 4);
      __m256i pix = _mm256_shuffle_epi8(
          _mm256_load_si256((const __m256i*)pix_buf), shuf_argb);
      __m256i weights = _mm256_xor_si256(
          _mm256_shuffle_epi8(
              _mm256_inserti128_si256(_mm256_zextsi128_si256(f128), f128, 1),
              shuf_frac),
          mask_7f);
      __m256i res = _mm256_srli_epi16(_mm256_maddubs_epi16(pix, weights), 7);
      res = _mm256_permute4x64_epi64(_mm256_packus_epi16(res, res), 0xd8);
      _mm_storeu_si128((__m128i*)dst_argb, _mm256_castsi256_si128(res));
      dst_argb += 16;
    }
    x = (int)((uint32_t)x + (uint32_t)n * (uint32_t)dx);
  }
  for (int j = 0; j < r; ++j) {
    ptrdiff_t xi = (ptrdiff_t)(x >> 16) * 4;
    int xf = (x >> 9) & 0x7f;
    const uint8_t* a = src_argb + xi;
    const uint8_t* b = a + 4;
    dst_argb[0] = (uint8_t)((a[0] * (0x7f ^ xf) + b[0] * xf) >> 7);
    dst_argb[1] = (uint8_t)((a[1] * (0x7f ^ xf) + b[1] * xf) >> 7);
    dst_argb[2] = (uint8_t)((a[2] * (0x7f ^ xf) + b[2] * xf) >> 7);
    dst_argb[3] = (uint8_t)((a[3] * (0x7f ^ xf) + b[3] * xf) >> 7);
    dst_argb += 4;
    x = (int)((uint32_t)x + (uint32_t)dx);
  }
}
#endif  // HAS_SCALEARGBFILTERCOLS_AVX2

#ifdef HAS_SCALEUVROWDOWN2_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowDown2_AVX2(const uint8_t* src_uv,
                          ptrdiff_t src_stride,
                          uint8_t* dst_uv,
                          int dst_width) {
  (void)src_stride;
  while (dst_width >= 16) {
    __m256i s0 =
        _mm256_srli_epi32(_mm256_loadu_si256((const __m256i*)src_uv), 16);
    __m256i s1 = _mm256_srli_epi32(
        _mm256_loadu_si256((const __m256i*)(src_uv + 32)), 16);
    src_uv += 64;
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_uv, d);
    dst_uv += 32;
    dst_width -= 16;
  }
  if (dst_width > 0) {
    __m256i s0 =
        _mm256_srli_epi32(_mm256_loadu_si256((const __m256i*)src_uv), 16);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s0), 0xd8);
    _mm_storeu_si128((__m128i*)dst_uv, _mm256_castsi256_si128(d));
  }
}
#endif  // HAS_SCALEUVROWDOWN2_AVX2

#ifdef HAS_SCALEUVROWDOWN2LINEAR_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowDown2Linear_AVX2(const uint8_t* src_uv,
                                ptrdiff_t src_stride,
                                uint8_t* dst_uv,
                                int dst_width) {
  (void)src_stride;
  const __m256i mask = _mm256_set1_epi32(0x0000ffff);
  while (dst_width >= 16) {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(src_uv + 32));
    src_uv += 64;
    __m256i s0 =
        _mm256_and_si256(_mm256_avg_epu8(r0, _mm256_srli_epi32(r0, 16)), mask);
    __m256i s1 =
        _mm256_and_si256(_mm256_avg_epu8(r1, _mm256_srli_epi32(r1, 16)), mask);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s1), 0xd8);
    _mm256_storeu_si256((__m256i*)dst_uv, d);
    dst_uv += 32;
    dst_width -= 16;
  }
  if (dst_width > 0) {
    __m256i r0 = _mm256_loadu_si256((const __m256i*)src_uv);
    __m256i s0 =
        _mm256_and_si256(_mm256_avg_epu8(r0, _mm256_srli_epi32(r0, 16)), mask);
    __m256i d = _mm256_permute4x64_epi64(_mm256_packus_epi32(s0, s0), 0xd8);
    _mm_storeu_si128((__m128i*)dst_uv, _mm256_castsi256_si128(d));
  }
}
#endif  // HAS_SCALEUVROWDOWN2LINEAR_AVX2

#ifdef HAS_SCALEUVROWDOWN2BOX_AVX2
static const uvec8 kShuffleSplitUV = {0u, 2u, 4u, 6u, 8u, 10u, 12u, 14u,
                                      1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u};
static const uvec8 kShuffleMergeUV = {0u,   8u,   2u,   10u,  4u,   12u,
                                      6u,   14u,  0x80, 0x80, 0x80, 0x80,
                                      0x80, 0x80, 0x80, 0x80};

LIBYUV_TARGET_AVX2
void ScaleUVRowDown2Box_AVX2(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst_ptr,
                             int dst_width) {
  const __m256i split = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kShuffleSplitUV));
  const __m256i merge = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kShuffleMergeUV));
  const __m256i ones = _mm256_set1_epi8(1);
  const __m256i zero = _mm256_setzero_si256();
  while (dst_width > 0) {
    __m256i r0 = _mm256_maddubs_epi16(
        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)src_ptr), split),
        ones);
    __m256i r1 = _mm256_maddubs_epi16(
        _mm256_shuffle_epi8(
            _mm256_loadu_si256((const __m256i*)(src_ptr + src_stride)), split),
        ones);
    src_ptr += 32;
    __m256i s =
        _mm256_avg_epu16(_mm256_srli_epi16(_mm256_add_epi16(r0, r1), 1), zero);
    __m256i d = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(s, merge), 0xd8);
    _mm_storeu_si128((__m128i*)dst_ptr, _mm256_castsi256_si128(d));
    dst_ptr += 16;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEUVROWDOWN2BOX_AVX2

#ifdef HAS_SCALEUVROWDOWNEVEN_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowDownEven_AVX2(const uint8_t* src_uv,
                             ptrdiff_t src_stride,
                             int src_stepx,
                             uint8_t* dst_uv,
                             int dst_width) {
  (void)src_stride;
  const ptrdiff_t step = (ptrdiff_t)src_stepx * 2;
  SIMD_ALIGNED(uint16_t uv_buf[8]);
  while (dst_width > 0) {
    uv_buf[0] = LoadU16(src_uv);
    uv_buf[1] = LoadU16(src_uv + step);
    uv_buf[2] = LoadU16(src_uv + step * 2);
    uv_buf[3] = LoadU16(src_uv + step * 3);
    uv_buf[4] = LoadU16(src_uv + step * 4);
    uv_buf[5] = LoadU16(src_uv + step * 5);
    uv_buf[6] = LoadU16(src_uv + step * 6);
    uv_buf[7] = LoadU16(src_uv + step * 7);
    src_uv += step * 8;
    _mm_storeu_si128((__m128i*)dst_uv, _mm_load_si128((const __m128i*)uv_buf));
    dst_uv += 16;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEUVROWDOWNEVEN_AVX2

#if defined(HAS_SCALEUVROWUP2_LINEAR_AVX2) || \
    defined(HAS_SCALEUVROWUP2_BILINEAR_AVX2)
static const uvec8 kUVLinearMadd31 = {3, 1, 3, 1, 1, 3, 1, 3,
                                      3, 1, 3, 1, 1, 3, 1, 3};
#endif

#ifdef HAS_SCALEUVROWUP2_LINEAR_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowUp2_Linear_AVX2(const uint8_t* src_ptr,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const __m256i madd31 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kUVLinearMadd31));
  const __m256i round2 = _mm256_set1_epi16(2);
  while (dst_width > 0) {
    __m256i s0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    __m256i s1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)(src_ptr + 2))),
        0xd8);
    src_ptr += 16;
    __m256i p = _mm256_unpacklo_epi8(s0, s1);
    __m256i lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(p, p), madd31);
    __m256i hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(p, p), madd31);
    lo = _mm256_srli_epi16(_mm256_add_epi16(lo, round2), 2);
    hi = _mm256_srli_epi16(_mm256_add_epi16(hi, round2), 2);
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi16(lo, hi));
    dst_ptr += 32;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEUVROWUP2_LINEAR_AVX2

#ifdef HAS_SCALEUVROWUP2_BILINEAR_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowUp2_Bilinear_AVX2(const uint8_t* src_ptr,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_ptr,
                                 ptrdiff_t dst_stride,
                                 int dst_width) {
  const __m256i madd31 = _mm256_broadcastsi128_si256(
      _mm_loadu_si128((const __m128i*)&kUVLinearMadd31));
  const __m256i round8 = _mm256_set1_epi16(8);
  while (dst_width > 0) {
    __m256i a0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)src_ptr)), 0xd8);
    __m256i a1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)(src_ptr + 2))),
        0xd8);
    __m256i pa = _mm256_unpacklo_epi8(a0, a1);
    __m256i r0_lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(pa, pa), madd31);
    __m256i r0_hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(pa, pa), madd31);

    __m256i b0 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride))),
        0xd8);
    __m256i b1 = _mm256_permute4x64_epi64(
        _mm256_zextsi128_si256(
            _mm_loadu_si128((const __m128i*)(src_ptr + src_stride + 2))),
        0xd8);
    src_ptr += 16;
    __m256i pb = _mm256_unpacklo_epi8(b0, b1);
    __m256i r1_lo = _mm256_maddubs_epi16(_mm256_unpacklo_epi32(pb, pb), madd31);
    __m256i r1_hi = _mm256_maddubs_epi16(_mm256_unpackhi_epi32(pb, pb), madd31);

    __m256i d0_lo = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r0_lo, r0_lo), r0_lo),
                         _mm256_add_epi16(r1_lo, round8)),
        4);
    __m256i d1_lo = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r1_lo, r1_lo), r1_lo),
                         _mm256_add_epi16(r0_lo, round8)),
        4);
    __m256i d0_hi = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r0_hi, r0_hi), r0_hi),
                         _mm256_add_epi16(r1_hi, round8)),
        4);
    __m256i d1_hi = _mm256_srli_epi16(
        _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(r1_hi, r1_hi), r1_hi),
                         _mm256_add_epi16(r0_hi, round8)),
        4);

    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi16(d0_lo, d0_hi));
    _mm256_storeu_si256((__m256i*)(dst_ptr + dst_stride),
                        _mm256_packus_epi16(d1_lo, d1_hi));
    dst_ptr += 32;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEUVROWUP2_BILINEAR_AVX2

#ifdef HAS_SCALEUVROWUP2_LINEAR_16_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowUp2_Linear_16_AVX2(const uint16_t* src_ptr,
                                  uint16_t* dst_ptr,
                                  int dst_width) {
  const __m256i round2 = _mm256_set1_epi32(2);
  while (dst_width > 0) {
    __m256i near_lo =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_ptr));
    __m256i near_hi =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)(src_ptr + 2)));
    src_ptr += 8;
    __m256i far_lo = _mm256_shuffle_epi32(near_lo, 0x4e);
    __m256i far_hi = _mm256_shuffle_epi32(near_hi, 0x4e);
    __m256i d_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(near_lo, near_lo),
                         _mm256_add_epi32(near_lo, _mm256_add_epi32(far_lo, round2))),
        2);
    __m256i d_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(near_hi, near_hi),
                         _mm256_add_epi32(near_hi, _mm256_add_epi32(far_hi, round2))),
        2);
    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi32(d_lo, d_hi));
    dst_ptr += 16;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEUVROWUP2_LINEAR_16_AVX2

#ifdef HAS_SCALEUVROWUP2_BILINEAR_16_AVX2
LIBYUV_TARGET_AVX2
void ScaleUVRowUp2_Bilinear_16_AVX2(const uint16_t* src_ptr,
                                    ptrdiff_t src_stride,
                                    uint16_t* dst_ptr,
                                    ptrdiff_t dst_stride,
                                    int dst_width) {
  const __m256i round8 = _mm256_set1_epi32(8);
  while (dst_width > 0) {
    __m256i a0 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)src_ptr));
    __m256i a1 =
        _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i*)(src_ptr + 2)));
    __m256i r0_lo = _mm256_add_epi32(
        _mm256_add_epi32(a0, a0),
        _mm256_add_epi32(a0, _mm256_shuffle_epi32(a0, 0x4e)));
    __m256i r0_hi = _mm256_add_epi32(
        _mm256_add_epi32(a1, a1),
        _mm256_add_epi32(a1, _mm256_shuffle_epi32(a1, 0x4e)));

    __m256i b0 = _mm256_cvtepu16_epi32(
        _mm_loadu_si128((const __m128i*)(src_ptr + src_stride)));
    __m256i b1 = _mm256_cvtepu16_epi32(
        _mm_loadu_si128((const __m128i*)(src_ptr + src_stride + 2)));
    src_ptr += 8;
    __m256i r1_lo = _mm256_add_epi32(
        _mm256_add_epi32(b0, b0),
        _mm256_add_epi32(b0, _mm256_shuffle_epi32(b0, 0x4e)));
    __m256i r1_hi = _mm256_add_epi32(
        _mm256_add_epi32(b1, b1),
        _mm256_add_epi32(b1, _mm256_shuffle_epi32(b1, 0x4e)));

    __m256i d0_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r0_lo, r0_lo), r0_lo),
                         _mm256_add_epi32(r1_lo, round8)),
        4);
    __m256i d1_lo = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r1_lo, r1_lo), r1_lo),
                         _mm256_add_epi32(r0_lo, round8)),
        4);
    __m256i d0_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r0_hi, r0_hi), r0_hi),
                         _mm256_add_epi32(r1_hi, round8)),
        4);
    __m256i d1_hi = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(r1_hi, r1_hi), r1_hi),
                         _mm256_add_epi32(r0_hi, round8)),
        4);

    _mm256_storeu_si256((__m256i*)dst_ptr, _mm256_packus_epi32(d0_lo, d0_hi));
    _mm256_storeu_si256((__m256i*)(dst_ptr + dst_stride),
                        _mm256_packus_epi32(d1_lo, d1_hi));
    dst_ptr += 16;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEUVROWUP2_BILINEAR_16_AVX2

#undef LIBYUV_TARGET_AVX2
#undef LIBYUV_TARGET_AVX512BW
#undef LIBYUV_TARGET_AVX512VBMI

#endif  // !defined(LIBYUV_DISABLE_X86) && ...

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
