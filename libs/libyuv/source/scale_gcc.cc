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

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for GCC x86 and x64.
#if !defined(LIBYUV_DISABLE_X86) &&               \
    (defined(__x86_64__) || defined(__i386__)) && \
    !defined(LIBYUV_ENABLE_ROWWIN)

#ifdef HAS_SCALEROWDOWN34_AVX2
// Offsets for source bytes 0 to 9
static const uvec8 kShuf0 = {0,   1,   3,   4,   5,   7,   8,   9,
                             128, 128, 128, 128, 128, 128, 128, 128};

// Offsets for source bytes 11 to 20 with 8 subtracted = 3 to 12.
static const uvec8 kShuf1 = {3,   4,   5,   7,   8,   9,   11,  12,
                             128, 128, 128, 128, 128, 128, 128, 128};

// Offsets for source bytes 21 to 31 with 16 subtracted = 5 to 31.
static const uvec8 kShuf2 = {5,   7,   8,   9,   11,  12,  13,  15,
                             128, 128, 128, 128, 128, 128, 128, 128};

// Offsets for source bytes 0 to 10
static const uvec8 kShuf01 = {0, 1, 1, 2, 2, 3, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10};

// Offsets for source bytes 10 to 21 with 8 subtracted = 3 to 13.
static const uvec8 kShuf11 = {2, 3, 4, 5,  5,  6,  6,  7,
                              8, 9, 9, 10, 10, 11, 12, 13};

// Offsets for source bytes 21 to 31 with 16 subtracted = 5 to 31.
static const uvec8 kShuf21 = {5,  6,  6,  7,  8,  9,  9,  10,
                              10, 11, 12, 13, 13, 14, 14, 15};

// Coefficients for source bytes 0 to 10
static const uvec8 kMadd01 = {3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2};

// Coefficients for source bytes 10 to 21
static const uvec8 kMadd11 = {1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1};

// Coefficients for source bytes 21 to 31
static const uvec8 kMadd21 = {2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3};

// Coefficients for source bytes 21 to 31
static const vec16 kRound34 = {2, 2, 2, 2, 2, 2, 2, 2};
#endif  // HAS_SCALEROWDOWN34_AVX2

#ifdef HAS_SCALEROWDOWN38_AVX2
static const uvec8 kShuf38a = {0,   3,   6,   8,   11,  14,  128, 128,
                               128, 128, 128, 128, 128, 128, 128, 128};

static const uvec8 kShuf38b = {128, 128, 128, 128, 128, 128, 0,   3,
                               6,   8,   11,  14,  128, 128, 128, 128};

// Arrange words 0,3,6 into 0,1,2
static const uvec8 kShufAc = {0,   1,   6,   7,   12,  13,  128, 128,
                              128, 128, 128, 128, 128, 128, 128, 128};

// Arrange words 0,3,6 into 3,4,5
static const uvec8 kShufAc3 = {128, 128, 128, 128, 128, 128, 0,   1,
                               6,   7,   12,  13,  128, 128, 128, 128};

// Scaling values for boxes of 3x3 and 2x3
static const uvec16 kScaleAc33 = {65536 / 9, 65536 / 9, 65536 / 6, 65536 / 9,
                                  65536 / 9, 65536 / 6, 0,         0};

// Arrange first value for pixels 0,1,2,3,4,5
static const uvec8 kShufAb0 = {0,  128, 3,  128, 6,   128, 8,   128,
                               11, 128, 14, 128, 128, 128, 128, 128};

// Arrange second value for pixels 0,1,2,3,4,5
static const uvec8 kShufAb1 = {1,  128, 4,  128, 7,   128, 9,   128,
                               12, 128, 15, 128, 128, 128, 128, 128};

// Arrange third value for pixels 0,1,2,3,4,5
static const uvec8 kShufAb2 = {2,  128, 5,   128, 128, 128, 10,  128,
                               13, 128, 128, 128, 128, 128, 128, 128};

// Scaling values for boxes of 3x2 and 2x2
static const uvec16 kScaleAb2 = {65536 / 3, 65536 / 3, 65536 / 2, 65536 / 3,
                                 65536 / 3, 65536 / 2, 0,         0};
#endif  // HAS_SCALEROWDOWN38_AVX2

// GCC versions of row functions are verbatim conversions from Visual C.
// Generated using gcc disassembly on Visual C object file:
// objdump -D yuvscaler.obj >yuvscaler.txt

void ScaleRowDown2_SSSE3(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  asm volatile(
      // 16 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

void ScaleRowDown2Linear_SSSE3(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  (void)src_stride;
  asm volatile(
      "pcmpeqb     %%xmm4,%%xmm4                 \n"  // 0x0101
      "pabsb       %%xmm4,%%xmm4                 \n"

      "pxor        %%xmm5,%%xmm5                 \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "pmaddubsw   %%xmm4,%%xmm0                 \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pavgw       %%xmm5,%%xmm0                 \n"
      "pavgw       %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm4", "xmm5");
}

void ScaleRowDown2Box_SSSE3(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  asm volatile(
      "pcmpeqb     %%xmm4,%%xmm4                 \n"  // 0x0101
      "pabsb       %%xmm4,%%xmm4                 \n"
      "pxor        %%xmm5,%%xmm5                 \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%3,1),%%xmm2          \n"
      "movdqu      0x10(%0,%3,1),%%xmm3          \n"
      "lea         0x20(%0),%0                   \n"
      "pmaddubsw   %%xmm4,%%xmm0                 \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm3                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"
      "psrlw       $0x1,%%xmm0                   \n"
      "psrlw       $0x1,%%xmm1                   \n"
      "pavgw       %%xmm5,%%xmm0                 \n"
      "pavgw       %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      : "r"(src_stride)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

#ifdef HAS_SCALEROWDOWN2_AVX2
void ScaleRowDown2_AVX2(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

void ScaleRowDown2Linear_AVX2(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  (void)src_stride;
  asm volatile(
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpabsb      %%ymm4,%%ymm4                 \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm4,%%ymm1,%%ymm1          \n"
      "vpavgw      %%ymm5,%%ymm0,%%ymm0          \n"
      "vpavgw      %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm4", "xmm5");
}

void ScaleRowDown2Box_AVX2(const uint8_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint8_t* dst_ptr,
                           int dst_width) {
  asm volatile(
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpabsb      %%ymm4,%%ymm4                 \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x00(%0,%3,1),%%ymm2          \n"
      "vmovdqu     0x20(%0,%3,1),%%ymm3          \n"
      "lea         0x40(%0),%0                   \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm4,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm3,%%ymm1,%%ymm1          \n"
      "vpsrlw      $0x1,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x1,%%ymm1,%%ymm1            \n"
      "vpavgw      %%ymm5,%%ymm0,%%ymm0          \n"
      "vpavgw      %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      : "r"(src_stride)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_SCALEROWDOWN2_AVX2

#ifdef HAS_SCALEROWDOWN2_16_AVX2
void ScaleRowDown2_16_AVX2(const uint16_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint16_t* dst_ptr,
                           int dst_width) {
  (void)src_stride;
  asm volatile(
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm0           \n"
      "vpsrld      $0x10,%%ymm1,%%ymm1           \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

void ScaleRowDown2Linear_16_AVX2(const uint16_t* src_ptr,
                                 ptrdiff_t src_stride,
                                 uint16_t* dst_ptr,
                                 int dst_width) {
  (void)src_stride;
  asm volatile(
      "vpcmpeqd    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $0x10,%%ymm4,%%ymm4           \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm2           \n"
      "vpsrld      $0x10,%%ymm1,%%ymm3           \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm4,%%ymm1,%%ymm1          \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm3,%%ymm2,%%ymm2          \n"
      "vpavgw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}

void ScaleRowDown2Box_16_AVX2(const uint16_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint16_t* dst_ptr,
                              int dst_width) {
  asm volatile(
      "vpcmpeqd    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrld      $0x1f,%%ymm6,%%ymm7           \n"
      "vpslld      $0x1,%%ymm7,%%ymm7            \n"
      "vpsrld      $0x10,%%ymm6,%%ymm6           \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x00(%0,%3,2),%%ymm2          \n"
      "vmovdqu     0x20(%0,%3,2),%%ymm3          \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm4           \n"
      "vpand       %%ymm6,%%ymm0,%%ymm0          \n"
      "vpaddd      %%ymm4,%%ymm0,%%ymm0          \n"
      "vpsrld      $0x10,%%ymm2,%%ymm4           \n"
      "vpand       %%ymm6,%%ymm2,%%ymm2          \n"
      "vpaddd      %%ymm4,%%ymm2,%%ymm2          \n"
      "vpaddd      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpsrld      $0x10,%%ymm1,%%ymm4           \n"
      "vpand       %%ymm6,%%ymm1,%%ymm1          \n"
      "vpaddd      %%ymm4,%%ymm1,%%ymm1          \n"
      "vpsrld      $0x10,%%ymm3,%%ymm4           \n"
      "vpand       %%ymm6,%%ymm3,%%ymm3          \n"
      "vpaddd      %%ymm4,%%ymm3,%%ymm3          \n"
      "vpaddd      %%ymm3,%%ymm1,%%ymm1          \n"
      "vpaddd      %%ymm7,%%ymm0,%%ymm0          \n"
      "vpaddd      %%ymm7,%%ymm1,%%ymm1          \n"
      "vpsrld      $0x2,%%ymm0,%%ymm0            \n"
      "vpsrld      $0x2,%%ymm1,%%ymm1            \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      : "r"(src_stride)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm6", "xmm7");
}
#endif  // HAS_SCALEROWDOWN2_16_AVX2

#ifdef HAS_SCALEROWDOWN2_AVX512BW
LIBYUV_TARGET_AVX512BW
void ScaleRowDown2_AVX512BW(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  (void)src_stride;
  uintptr_t temp;
  asm volatile(
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpmovwb     %%zmm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovd       %k3,%%k1                      \n"
      "vmovdqu16   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpmovwb     %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width),  // %2
        "=&r"(temp)       // %3
      :
      : "memory", "cc", "xmm0", "k1");
}

LIBYUV_TARGET_AVX512BW
void ScaleRowDown2Linear_AVX512BW(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  int dst_width) {
  (void)src_stride;
  uintptr_t temp;
  asm volatile(
      "vpternlogd  $0xff,%%zmm4,%%zmm4,%%zmm4    \n"
      "vpabsb      %%zmm4,%%zmm4                 \n"
      "vpxord      %%zmm5,%%zmm5,%%zmm5          \n"
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "lea         0x40(%0),%0                   \n"
      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm0          \n"
      "vpavgw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpmovwb     %%zmm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovd       %k3,%%k1                      \n"
      "vmovdqu16   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm0          \n"
      "vpavgw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpmovwb     %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width),  // %2
        "=&r"(temp)       // %3
      :
      : "memory", "cc", "xmm0", "xmm4", "xmm5", "k1");
}

LIBYUV_TARGET_AVX512BW
void ScaleRowDown2Box_AVX512BW(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  uintptr_t temp;
  asm volatile(
      "vpternlogd  $0xff,%%zmm4,%%zmm4,%%zmm4    \n"
      "vpabsb      %%zmm4,%%zmm4                 \n"
      "vpxord      %%zmm5,%%zmm5,%%zmm5          \n"
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    0x00(%0,%4,1),%%zmm2          \n"
      "lea         0x40(%0),%0                   \n"
      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm2,%%zmm0,%%zmm0          \n"
      "vpsrlw      $0x1,%%zmm0,%%zmm0            \n"
      "vpavgw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpmovwb     %%zmm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovd       %k3,%%k1                      \n"
      "vmovdqu16   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vmovdqu16   0x00(%0,%4,1),%%zmm2%{%%k1%}%{z%}\n"
      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm2,%%zmm0,%%zmm0          \n"
      "vpsrlw      $0x1,%%zmm0,%%zmm0            \n"
      "vpavgw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpmovwb     %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width),  // %2
        "=&r"(temp)       // %3
      : "r"(src_stride)   // %4
      : "memory", "cc", "xmm0", "xmm2", "xmm4", "xmm5", "k1");
}

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
  dst_ptr[0] = (src_ptr[0] + src_ptr[src_stride] + 1) >> 1;
}
#endif  // HAS_SCALEROWDOWN2_AVX512BW

void ScaleRowDown4_SSSE3(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrld       $0x18,%%xmm5                  \n"
      "pslld       $0x10,%%xmm5                  \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void ScaleRowDown4Box_SSSE3(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  ptrdiff_t stridex3;
  asm volatile(
      "pcmpeqb     %%xmm4,%%xmm4                 \n"
      "pabsw       %%xmm4,%%xmm5                 \n"
      "pabsb       %%xmm4,%%xmm4                 \n"  // 0x0101
      "psllw       $0x3,%%xmm5                   \n"  // 0x0008
      "lea         0x00(%4,%4,2),%3              \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%4,1),%%xmm2          \n"
      "movdqu      0x10(%0,%4,1),%%xmm3          \n"
      "pmaddubsw   %%xmm4,%%xmm0                 \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm3                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"
      "movdqu      0x00(%0,%4,2),%%xmm2          \n"
      "movdqu      0x10(%0,%4,2),%%xmm3          \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm3                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"
      "movdqu      0x00(%0,%3,1),%%xmm2          \n"
      "movdqu      0x10(%0,%3,1),%%xmm3          \n"
      "lea         0x20(%0),%0                   \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm3                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"
      "phaddw      %%xmm1,%%xmm0                 \n"
      "paddw       %%xmm5,%%xmm0                 \n"
      "psrlw       $0x4,%%xmm0                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width),  // %2
        "=&r"(stridex3)   // %3
      : "r"(src_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

#ifdef HAS_SCALEROWDOWN4_AVX2
void ScaleRowDown4_AVX2(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrld      $0x18,%%ymm5,%%ymm5           \n"
      "vpslld      $0x10,%%ymm5,%%ymm5           \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void ScaleRowDown4Box_AVX2(const uint8_t* src_ptr,
                           ptrdiff_t src_stride,
                           uint8_t* dst_ptr,
                           int dst_width) {
  asm volatile(
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpabsw      %%ymm4,%%ymm5                 \n"
      "vpabsb      %%ymm4,%%ymm4                 \n"  // 0x0101
      "vpsllw      $0x3,%%ymm5,%%ymm5            \n"  // 0x0008

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x00(%0,%3,1),%%ymm2          \n"
      "vmovdqu     0x20(%0,%3,1),%%ymm3          \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm4,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm3,%%ymm1,%%ymm1          \n"
      "vmovdqu     0x00(%0,%3,2),%%ymm2          \n"
      "vmovdqu     0x20(%0,%3,2),%%ymm3          \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm3,%%ymm1,%%ymm1          \n"
      "vmovdqu     0x00(%0,%4,1),%%ymm2          \n"
      "vmovdqu     0x20(%0,%4,1),%%ymm3          \n"
      "lea         0x40(%0),%0                   \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm3,%%ymm1,%%ymm1          \n"
      "vphaddw     %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpaddw      %%ymm5,%%ymm0,%%ymm0          \n"
      "vpsrlw      $0x4,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),       // %0
        "+r"(dst_ptr),       // %1
        "+r"(dst_width)      // %2
      : "r"(src_stride),     // %3
        "r"(src_stride * 3)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_SCALEROWDOWN4_AVX2

#ifdef HAS_SCALEROWDOWN34_AVX2
void ScaleRowDown34_AVX2(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  asm volatile(
      "vbroadcasti128 %[kShuf0],%%ymm3           \n"
      "vbroadcasti128 %[kShuf1],%%ymm4           \n"
      "vbroadcasti128 %[kShuf2],%%ymm5           \n"
      "sub         $0x30,%[dst_width]            \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x20(%[src_ptr]),%%ymm1       \n"
      "lea         0x40(%[src_ptr]),%[src_ptr]   \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm2    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm0,%%ymm6    \n"
      "vpalignr    $0x8,%%ymm2,%%ymm6,%%ymm1     \n"
      "vpshufb     %%ymm3,%%ymm2,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm5,%%ymm6,%%ymm2          \n"
      "vpunpcklqdq %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%xmm0,(%[dst_ptr])           \n"
      "vmovq       %%xmm2,0x10(%[dst_ptr])       \n"
      "vextracti128 $0x1,%%ymm0,%%xmm0           \n"
      "vextracti128 $0x1,%%ymm2,%%xmm2           \n"
      "vmovdqu     %%xmm0,0x18(%[dst_ptr])       \n"
      "vmovq       %%xmm2,0x28(%[dst_ptr])       \n"
      "lea         0x30(%[dst_ptr]),%[dst_ptr]   \n"
      "sub         $0x30,%[dst_width]            \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x30,%[dst_width]            \n"
      "jle         99f                           \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vextracti128 $0x1,%%ymm0,%%xmm2           \n"
      "vpalignr    $0x8,%%xmm0,%%xmm2,%%xmm1     \n"
      "vpshufb     %%xmm3,%%xmm0,%%xmm0          \n"
      "vpshufb     %%xmm4,%%xmm1,%%xmm1          \n"
      "vpshufb     %%xmm5,%%xmm2,%%xmm2          \n"
      "vpunpcklqdq %%xmm1,%%xmm0,%%xmm0          \n"
      "vmovdqu     %%xmm0,(%[dst_ptr])           \n"
      "vmovq       %%xmm2,0x10(%[dst_ptr])       \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr), [dst_ptr] "+r"(dst_ptr),
        [dst_width] "+r"(dst_width)
      : [kShuf0] "m"(kShuf0), [kShuf1] "m"(kShuf1), [kShuf2] "m"(kShuf2)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

void ScaleRowDown34_1_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  asm volatile(
      "vbroadcasti128 %[kRound34],%%ymm5         \n"
      "sub         $0x30,%[dst_width]            \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm2 \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vmovdqu     0x20(%[src_ptr]),%%ymm1       \n"
      "vmovdqu     0x20(%[src_ptr],%[src_stride],1),%%ymm3 \n"
      "lea         0x40(%[src_ptr]),%[src_ptr]   \n"
      "vpavgb      %%ymm3,%%ymm1,%%ymm1          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm2    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpalignr    $0x8,%%ymm2,%%ymm3,%%ymm4     \n"

      "vbroadcasti128 %[kShuf01],%%ymm6          \n"
      "vbroadcasti128 %[kMadd01],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm2          \n"
      "vpaddsw     %%ymm5,%%ymm2,%%ymm2          \n"
      "vpsrlw      $0x2,%%ymm2,%%ymm2            \n"
      "vpackuswb   %%ymm2,%%ymm2,%%ymm2          \n"

      "vbroadcasti128 %[kShuf11],%%ymm6          \n"
      "vbroadcasti128 %[kMadd11],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm4,%%ymm4          \n"
      "vpmaddubsw  %%ymm7,%%ymm4,%%ymm4          \n"
      "vpaddsw     %%ymm5,%%ymm4,%%ymm4          \n"
      "vpsrlw      $0x2,%%ymm4,%%ymm4            \n"
      "vpackuswb   %%ymm4,%%ymm4,%%ymm4          \n"

      "vbroadcasti128 %[kShuf21],%%ymm6          \n"
      "vbroadcasti128 %[kMadd21],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm3,%%ymm3          \n"
      "vpmaddubsw  %%ymm7,%%ymm3,%%ymm3          \n"
      "vpaddsw     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpsrlw      $0x2,%%ymm3,%%ymm3            \n"
      "vpackuswb   %%ymm3,%%ymm3,%%ymm3          \n"

      "vpunpcklqdq %%ymm4,%%ymm2,%%ymm2          \n"
      "vmovdqu     %%xmm2,(%[dst_ptr])           \n"
      "vmovq       %%xmm3,0x10(%[dst_ptr])       \n"
      "vextracti128 $0x1,%%ymm2,%%xmm0           \n"
      "vextracti128 $0x1,%%ymm3,%%xmm1           \n"
      "vmovdqu     %%xmm0,0x18(%[dst_ptr])       \n"
      "vmovq       %%xmm1,0x28(%[dst_ptr])       \n"
      "lea         0x30(%[dst_ptr]),%[dst_ptr]   \n"
      "sub         $0x30,%[dst_width]            \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x30,%[dst_width]            \n"
      "jle         99f                           \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm1 \n"
      "vpavgb      %%ymm1,%%ymm0,%%ymm0          \n"
      "vextracti128 $0x1,%%ymm0,%%xmm3           \n"
      "vpalignr    $0x8,%%xmm0,%%xmm3,%%xmm4     \n"

      "vpshufb     %[kShuf01],%%xmm0,%%xmm0      \n"
      "vpmaddubsw  %[kMadd01],%%xmm0,%%xmm0      \n"
      "vpaddsw     %%xmm5,%%xmm0,%%xmm0          \n"
      "vpsrlw      $0x2,%%xmm0,%%xmm0            \n"
      "vpackuswb   %%xmm0,%%xmm0,%%xmm0          \n"

      "vpshufb     %[kShuf11],%%xmm4,%%xmm4      \n"
      "vpmaddubsw  %[kMadd11],%%xmm4,%%xmm4      \n"
      "vpaddsw     %%xmm5,%%xmm4,%%xmm4          \n"
      "vpsrlw      $0x2,%%xmm4,%%xmm4            \n"
      "vpackuswb   %%xmm4,%%xmm4,%%xmm4          \n"

      "vpshufb     %[kShuf21],%%xmm3,%%xmm3      \n"
      "vpmaddubsw  %[kMadd21],%%xmm3,%%xmm3      \n"
      "vpaddsw     %%xmm5,%%xmm3,%%xmm3          \n"
      "vpsrlw      $0x2,%%xmm3,%%xmm3            \n"
      "vpackuswb   %%xmm3,%%xmm3,%%xmm3          \n"

      "vpunpcklqdq %%xmm4,%%xmm0,%%xmm0          \n"
      "vmovdqu     %%xmm0,(%[dst_ptr])           \n"
      "vmovq       %%xmm3,0x10(%[dst_ptr])       \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr), [dst_ptr] "+r"(dst_ptr),
        [dst_width] "+rm"(dst_width)
      : [src_stride] "r"(src_stride), [kShuf01] "m"(kShuf01),
        [kShuf11] "m"(kShuf11), [kShuf21] "m"(kShuf21), [kMadd01] "m"(kMadd01),
        [kMadd11] "m"(kMadd11), [kRound34] "m"(kRound34), [kMadd21] "m"(kMadd21)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

void ScaleRowDown34_0_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  asm volatile(
      "vbroadcasti128 %[kRound34],%%ymm5         \n"
      "sub         $0x30,%[dst_width]            \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm2 \n"
      "vpavgb      %%ymm0,%%ymm2,%%ymm2          \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vmovdqu     0x20(%[src_ptr]),%%ymm1       \n"
      "vmovdqu     0x20(%[src_ptr],%[src_stride],1),%%ymm3 \n"
      "lea         0x40(%[src_ptr]),%[src_ptr]   \n"
      "vpavgb      %%ymm1,%%ymm3,%%ymm3          \n"
      "vpavgb      %%ymm3,%%ymm1,%%ymm1          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm2    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpalignr    $0x8,%%ymm2,%%ymm3,%%ymm4     \n"

      "vbroadcasti128 %[kShuf01],%%ymm6          \n"
      "vbroadcasti128 %[kMadd01],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm2          \n"
      "vpaddsw     %%ymm5,%%ymm2,%%ymm2          \n"
      "vpsrlw      $0x2,%%ymm2,%%ymm2            \n"
      "vpackuswb   %%ymm2,%%ymm2,%%ymm2          \n"

      "vbroadcasti128 %[kShuf11],%%ymm6          \n"
      "vbroadcasti128 %[kMadd11],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm4,%%ymm4          \n"
      "vpmaddubsw  %%ymm7,%%ymm4,%%ymm4          \n"
      "vpaddsw     %%ymm5,%%ymm4,%%ymm4          \n"
      "vpsrlw      $0x2,%%ymm4,%%ymm4            \n"
      "vpackuswb   %%ymm4,%%ymm4,%%ymm4          \n"

      "vbroadcasti128 %[kShuf21],%%ymm6          \n"
      "vbroadcasti128 %[kMadd21],%%ymm7          \n"
      "vpshufb     %%ymm6,%%ymm3,%%ymm3          \n"
      "vpmaddubsw  %%ymm7,%%ymm3,%%ymm3          \n"
      "vpaddsw     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpsrlw      $0x2,%%ymm3,%%ymm3            \n"
      "vpackuswb   %%ymm3,%%ymm3,%%ymm3          \n"

      "vpunpcklqdq %%ymm4,%%ymm2,%%ymm2          \n"
      "vmovdqu     %%xmm2,(%[dst_ptr])           \n"
      "vmovq       %%xmm3,0x10(%[dst_ptr])       \n"
      "vextracti128 $0x1,%%ymm2,%%xmm0           \n"
      "vextracti128 $0x1,%%ymm3,%%xmm1           \n"
      "vmovdqu     %%xmm0,0x18(%[dst_ptr])       \n"
      "vmovq       %%xmm1,0x28(%[dst_ptr])       \n"
      "lea         0x30(%[dst_ptr]),%[dst_ptr]   \n"
      "sub         $0x30,%[dst_width]            \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x30,%[dst_width]            \n"
      "jle         99f                           \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm1 \n"
      "vpavgb      %%ymm0,%%ymm1,%%ymm1          \n"
      "vpavgb      %%ymm1,%%ymm0,%%ymm0          \n"
      "vextracti128 $0x1,%%ymm0,%%xmm3           \n"
      "vpalignr    $0x8,%%xmm0,%%xmm3,%%xmm4     \n"

      "vpshufb     %[kShuf01],%%xmm0,%%xmm0      \n"
      "vpmaddubsw  %[kMadd01],%%xmm0,%%xmm0      \n"
      "vpaddsw     %%xmm5,%%xmm0,%%xmm0          \n"
      "vpsrlw      $0x2,%%xmm0,%%xmm0            \n"
      "vpackuswb   %%xmm0,%%xmm0,%%xmm0          \n"

      "vpshufb     %[kShuf11],%%xmm4,%%xmm4      \n"
      "vpmaddubsw  %[kMadd11],%%xmm4,%%xmm4      \n"
      "vpaddsw     %%xmm5,%%xmm4,%%xmm4          \n"
      "vpsrlw      $0x2,%%xmm4,%%xmm4            \n"
      "vpackuswb   %%xmm4,%%xmm4,%%xmm4          \n"

      "vpshufb     %[kShuf21],%%xmm3,%%xmm3      \n"
      "vpmaddubsw  %[kMadd21],%%xmm3,%%xmm3      \n"
      "vpaddsw     %%xmm5,%%xmm3,%%xmm3          \n"
      "vpsrlw      $0x2,%%xmm3,%%xmm3            \n"
      "vpackuswb   %%xmm3,%%xmm3,%%xmm3          \n"

      "vpunpcklqdq %%xmm4,%%xmm0,%%xmm0          \n"
      "vmovdqu     %%xmm0,(%[dst_ptr])           \n"
      "vmovq       %%xmm3,0x10(%[dst_ptr])       \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr), [dst_ptr] "+r"(dst_ptr),
        [dst_width] "+rm"(dst_width)
      : [src_stride] "r"(src_stride), [kShuf01] "m"(kShuf01),
        [kShuf11] "m"(kShuf11), [kShuf21] "m"(kShuf21), [kMadd01] "m"(kMadd01),
        [kMadd11] "m"(kMadd11), [kRound34] "m"(kRound34), [kMadd21] "m"(kMadd21)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_SCALEROWDOWN34_AVX2

#ifdef HAS_SCALEROWDOWN38_AVX2
void ScaleRowDown38_AVX2(const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         uint8_t* dst_ptr,
                         int dst_width) {
  (void)src_stride;
  asm volatile(
      "vmovdqu     %3,%%xmm4                     \n"
      "vinserti128 $0x1,%4,%%ymm4,%%ymm4         \n"
      "sub         $0x18,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vextracti128 $0x1,%%ymm0,%%xmm2           \n"
      "vextracti128 $0x1,%%ymm1,%%xmm3           \n"
      "vpor        %%xmm2,%%xmm0,%%xmm0          \n"
      "vpor        %%xmm3,%%xmm1,%%xmm1          \n"
      "vmovq       %%xmm0,(%1)                   \n"
      "vpsrldq     $0x8,%%xmm0,%%xmm0            \n"
      "vmovd       %%xmm0,0x8(%1)                \n"
      "vmovq       %%xmm1,0xc(%1)                \n"
      "vpsrldq     $0x8,%%xmm1,%%xmm1            \n"
      "vmovd       %%xmm1,0x14(%1)               \n"
      "lea         0x18(%1),%1                   \n"
      "sub         $0x18,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x18,%2                      \n"
      "jle         99f                           \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vextracti128 $0x1,%%ymm0,%%xmm2           \n"
      "vpor        %%xmm2,%%xmm0,%%xmm0          \n"
      "vmovq       %%xmm0,(%1)                   \n"
      "vpsrldq     $0x8,%%xmm0,%%xmm0            \n"
      "vmovd       %%xmm0,0x8(%1)                \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      : "m"(kShuf38a),   // %3
        "m"(kShuf38b)    // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}

void ScaleRowDown38_2_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  asm volatile(
      "vbroadcasti128 %[kShufAb0],%%ymm2         \n"
      "vbroadcasti128 %[kShufAb1],%%ymm3         \n"
      "vbroadcasti128 %[kShufAb2],%%ymm4         \n"
      "vbroadcasti128 %[kScaleAb2],%%ymm5        \n"
      "vpxor       %%ymm7,%%ymm7,%%ymm7          \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm1 \n"
      "lea         0x20(%[src_ptr]),%[src_ptr]   \n"
      "vpavgb      %%ymm1,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm2,%%ymm0,%%ymm1          \n"
      "vpshufb     %%ymm3,%%ymm0,%%ymm6          \n"
      "vpaddusw    %%ymm6,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpaddusw    %%ymm0,%%ymm1,%%ymm1          \n"
      "vpmulhuw    %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm7,%%ymm1,%%ymm1          \n"
      "vextracti128 $0x1,%%ymm1,%%xmm0           \n"
      "vpslldq     $0x6,%%xmm0,%%xmm0            \n"
      "vpor        %%xmm0,%%xmm1,%%xmm1          \n"
      "vmovq       %%xmm1,(%[dst_ptr])           \n"
      "vpsrldq     $0x8,%%xmm1,%%xmm1            \n"
      "vmovd       %%xmm1,0x8(%[dst_ptr])        \n"
      "lea         0xc(%[dst_ptr]),%[dst_ptr]    \n"
      "sub         $0xc,%[dst_width]             \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr), [dst_ptr] "+r"(dst_ptr),
        [dst_width] "+rm"(dst_width)
      : [src_stride] "r"(src_stride), [kShufAb0] "m"(kShufAb0),
        [kShufAb1] "m"(kShufAb1), [kShufAb2] "m"(kShufAb2),
        [kScaleAb2] "m"(kScaleAb2)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

void ScaleRowDown38_3_Box_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  asm volatile(
      "vbroadcasti128 %[kShufAc],%%ymm2          \n"
      "vbroadcasti128 %[kShufAc3],%%ymm3         \n"
      "vbroadcasti128 %[kScaleAc33],%%ymm4       \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_ptr]),%%ymm0           \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],1),%%ymm6 \n"
      "vpunpckhbw  %%ymm5,%%ymm0,%%ymm1          \n"
      "vpunpcklbw  %%ymm5,%%ymm0,%%ymm0          \n"
      "vpunpckhbw  %%ymm5,%%ymm6,%%ymm7          \n"
      "vpunpcklbw  %%ymm5,%%ymm6,%%ymm6          \n"
      "vpaddusw    %%ymm6,%%ymm0,%%ymm0          \n"
      "vpaddusw    %%ymm7,%%ymm1,%%ymm1          \n"
      "vmovdqu     0x00(%[src_ptr],%[src_stride],2),%%ymm6 \n"
      "lea         0x20(%[src_ptr]),%[src_ptr]   \n"
      "vpunpckhbw  %%ymm5,%%ymm6,%%ymm7          \n"
      "vpunpcklbw  %%ymm5,%%ymm6,%%ymm6          \n"
      "vpaddusw    %%ymm6,%%ymm0,%%ymm0          \n"
      "vpaddusw    %%ymm7,%%ymm1,%%ymm1          \n"
      "vpsrldq     $0x2,%%ymm0,%%ymm6            \n"
      "vpaddusw    %%ymm0,%%ymm6,%%ymm6          \n"
      "vpsrldq     $0x4,%%ymm0,%%ymm0            \n"
      "vpaddusw    %%ymm0,%%ymm6,%%ymm6          \n"
      "vpshufb     %%ymm2,%%ymm6,%%ymm6          \n"
      "vpsrldq     $0x2,%%ymm1,%%ymm7            \n"
      "vpaddusw    %%ymm1,%%ymm7,%%ymm7          \n"
      "vpsrldq     $0x4,%%ymm1,%%ymm1            \n"
      "vpaddusw    %%ymm1,%%ymm7,%%ymm7          \n"
      "vpshufb     %%ymm3,%%ymm7,%%ymm7          \n"
      "vpaddusw    %%ymm7,%%ymm6,%%ymm6          \n"
      "vpmulhuw    %%ymm4,%%ymm6,%%ymm6          \n"
      "vpackuswb   %%ymm5,%%ymm6,%%ymm6          \n"
      "vextracti128 $0x1,%%ymm6,%%xmm0           \n"
      "vpslldq     $0x6,%%xmm0,%%xmm0            \n"
      "vpor        %%xmm0,%%xmm6,%%xmm6          \n"
      "vmovq       %%xmm6,(%[dst_ptr])           \n"
      "vpsrldq     $0x8,%%xmm6,%%xmm6            \n"
      "vmovd       %%xmm6,0x8(%[dst_ptr])        \n"
      "lea         0xc(%[dst_ptr]),%[dst_ptr]    \n"
      "sub         $0xc,%[dst_width]             \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr), [dst_ptr] "+r"(dst_ptr),
        [dst_width] "+rm"(dst_width)
      : [src_stride] "r"(src_stride), [kShufAc] "m"(kShufAc),
        [kShufAc3] "m"(kShufAc3), [kScaleAc33] "m"(kScaleAc33)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_SCALEROWDOWN38_AVX2

static const uvec8 kLinearShuffleFar = {2,  3,  0, 1, 6,  7,  4,  5,
                                        10, 11, 8, 9, 14, 15, 12, 13};

static const uvec8 kLinearMadd31 = {3, 1, 1, 3, 3, 1, 1, 3,
                                    3, 1, 1, 3, 3, 1, 1, 3};

#ifdef HAS_SCALEROWUP2_LINEAR_SSE2
void ScaleRowUp2_Linear_SSE2(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int dst_width) {
  asm volatile(
      "pxor        %%xmm0,%%xmm0                 \n"  // 0
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $1,%%xmm6                     \n"  // all 2

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm1                   \n"  // 01234567
      "movq        1(%0),%%xmm2                  \n"  // 12345678
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm1,%%xmm1                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788
      "movdqa      %%xmm1,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm4                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 11223344 (16)
      "paddw       %%xmm5,%%xmm4                 \n"
      "movdqa      %%xmm3,%%xmm5                 \n"
      "paddw       %%xmm6,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 01122334 (16)
      "paddw       %%xmm5,%%xmm5                 \n"
      "paddw       %%xmm4,%%xmm5                 \n"  // 3*near+far+2 (lo)
      "psrlw       $2,%%xmm5                     \n"  // 3/4*near+1/4*far (lo)

      "punpckhbw   %%xmm0,%%xmm1                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm2,%%xmm1                 \n"
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm6,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm3                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm1                     \n"  // 3/4*near+1/4*far (hi)

      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_SSE2
void ScaleRowUp2_Bilinear_SSE2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width) {
  asm volatile(
      "1:          \n"
      "pxor        %%xmm0,%%xmm0                 \n"  // 0
      // above line
      "movq        (%0),%%xmm1                   \n"  // 01234567
      "movq        1(%0),%%xmm2                  \n"  // 12345678
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm1,%%xmm1                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788

      "movdqa      %%xmm1,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm4                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 11223344 (16)
      "paddw       %%xmm5,%%xmm4                 \n"  // near+far
      "movdqa      %%xmm3,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 01122334 (16)
      "paddw       %%xmm5,%%xmm5                 \n"  // 2*near
      "paddw       %%xmm5,%%xmm4                 \n"  // 3*near+far (1, lo)

      "punpckhbw   %%xmm0,%%xmm1                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm2,%%xmm1                 \n"
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm3,%%xmm3                 \n"  // 2*near
      "paddw       %%xmm3,%%xmm1                 \n"  // 3*near+far (1, hi)

      // below line
      "movq        (%0,%3),%%xmm6                \n"  // 01234567
      "movq        1(%0,%3),%%xmm2               \n"  // 12345678
      "movdqa      %%xmm6,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm6,%%xmm6                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788

      "movdqa      %%xmm6,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm7                 \n"
      "punpcklbw   %%xmm0,%%xmm7                 \n"  // 11223344 (16)
      "paddw       %%xmm7,%%xmm5                 \n"  // near+far
      "movdqa      %%xmm3,%%xmm7                 \n"
      "punpcklbw   %%xmm0,%%xmm7                 \n"  // 01122334 (16)
      "paddw       %%xmm7,%%xmm7                 \n"  // 2*near
      "paddw       %%xmm7,%%xmm5                 \n"  // 3*near+far (2, lo)

      "punpckhbw   %%xmm0,%%xmm6                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm6,%%xmm2                 \n"  // near+far
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm3,%%xmm3                 \n"  // 2*near
      "paddw       %%xmm3,%%xmm2                 \n"  // 3*near+far (2, hi)

      // xmm4 xmm1
      // xmm5 xmm2
      "pcmpeqw     %%xmm0,%%xmm0                 \n"
      "psrlw       $15,%%xmm0                    \n"
      "psllw       $3,%%xmm0                     \n"  // all 8

      "movdqa      %%xmm4,%%xmm3                 \n"
      "movdqa      %%xmm5,%%xmm6                 \n"
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm0,%%xmm6                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm4,%%xmm3                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm6,%%xmm3                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm3                     \n"  // ^ div by 16

      "movdqa      %%xmm1,%%xmm7                 \n"
      "movdqa      %%xmm2,%%xmm6                 \n"
      "paddw       %%xmm7,%%xmm7                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm0,%%xmm6                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm1,%%xmm7                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm6,%%xmm7                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm7                     \n"  // ^ div by 16

      "packuswb    %%xmm7,%%xmm3                 \n"
      "movdqu      %%xmm3,(%1)                   \n"  // save above line

      "movdqa      %%xmm5,%%xmm3                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm3,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm4,%%xmm5                 \n"  // 9 3 3 1 + 8 (lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16

      "movdqa      %%xmm2,%%xmm3                 \n"
      "paddw       %%xmm0,%%xmm1                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm3,%%xmm2                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm1,%%xmm2                 \n"  // 9 3 3 1 + 8 (hi)
      "psrlw       $4,%%xmm2                     \n"  // ^ div by 16

      "packuswb    %%xmm2,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // save below line

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width)   // %2
      : "r"(src_stride),  // %3
        "r"(dst_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_12_SSSE3
void ScaleRowUp2_Linear_12_SSSE3(const uint16_t* src_ptr,
                                 uint16_t* dst_ptr,
                                 int dst_width) {
  asm volatile(
      "movdqa      %3,%%xmm5                     \n"
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $15,%%xmm4                    \n"
      "psllw       $1,%%xmm4                     \n"  // all 2

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // 01234567 (16)
      "movdqu      2(%0),%%xmm1                  \n"  // 12345678 (16)

      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhwd   %%xmm1,%%xmm2                 \n"  // 45566778 (16)
      "punpcklwd   %%xmm1,%%xmm0                 \n"  // 01122334 (16)

      "movdqa      %%xmm2,%%xmm3                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pshufb      %%xmm5,%%xmm3                 \n"  // 54657687 (far)
      "pshufb      %%xmm5,%%xmm1                 \n"  // 10213243 (far)

      "paddw       %%xmm4,%%xmm1                 \n"  // far+2
      "paddw       %%xmm4,%%xmm3                 \n"  // far+2
      "paddw       %%xmm0,%%xmm1                 \n"  // near+far+2
      "paddw       %%xmm2,%%xmm3                 \n"  // near+far+2
      "paddw       %%xmm0,%%xmm0                 \n"  // 2*near
      "paddw       %%xmm2,%%xmm2                 \n"  // 2*near
      "paddw       %%xmm1,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddw       %%xmm3,%%xmm2                 \n"  // 3*near+far+2 (hi)

      "psrlw       $2,%%xmm0                     \n"  // 3/4*near+1/4*far
      "psrlw       $2,%%xmm2                     \n"  // 3/4*near+1/4*far
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm2,16(%1)                 \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),          // %0
        "+r"(dst_ptr),          // %1
        "+r"(dst_width)         // %2
      : "m"(kLinearShuffleFar)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_12_SSSE3
void ScaleRowUp2_Bilinear_12_SSSE3(const uint16_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint16_t* dst_ptr,
                                   ptrdiff_t dst_stride,
                                   int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm7,%%xmm7                 \n"
      "psrlw       $15,%%xmm7                    \n"
      "psllw       $3,%%xmm7                     \n"  // all 8
      "movdqa      %5,%%xmm6                     \n"

      LABELALIGN
      "1:          \n"
      // above line
      "movdqu      (%0),%%xmm0                   \n"  // 01234567 (16)
      "movdqu      2(%0),%%xmm1                  \n"  // 12345678 (16)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhwd   %%xmm1,%%xmm2                 \n"  // 45566778 (16)
      "punpcklwd   %%xmm1,%%xmm0                 \n"  // 01122334 (16)
      "movdqa      %%xmm2,%%xmm3                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pshufb      %%xmm6,%%xmm3                 \n"  // 54657687 (far)
      "pshufb      %%xmm6,%%xmm1                 \n"  // 10213243 (far)
      "paddw       %%xmm0,%%xmm1                 \n"  // near+far
      "paddw       %%xmm2,%%xmm3                 \n"  // near+far
      "paddw       %%xmm0,%%xmm0                 \n"  // 2*near
      "paddw       %%xmm2,%%xmm2                 \n"  // 2*near
      "paddw       %%xmm1,%%xmm0                 \n"  // 3*near+far (1, lo)
      "paddw       %%xmm3,%%xmm2                 \n"  // 3*near+far (1, hi)

      // below line
      "movdqu      (%0,%3,2),%%xmm1              \n"  // 01234567 (16)
      "movdqu      2(%0,%3,2),%%xmm4             \n"  // 12345678 (16)
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpckhwd   %%xmm4,%%xmm3                 \n"  // 45566778 (16)
      "punpcklwd   %%xmm4,%%xmm1                 \n"  // 01122334 (16)
      "movdqa      %%xmm3,%%xmm5                 \n"
      "movdqa      %%xmm1,%%xmm4                 \n"
      "pshufb      %%xmm6,%%xmm5                 \n"  // 54657687 (far)
      "pshufb      %%xmm6,%%xmm4                 \n"  // 10213243 (far)
      "paddw       %%xmm1,%%xmm4                 \n"  // near+far
      "paddw       %%xmm3,%%xmm5                 \n"  // near+far
      "paddw       %%xmm1,%%xmm1                 \n"  // 2*near
      "paddw       %%xmm3,%%xmm3                 \n"  // 2*near
      "paddw       %%xmm4,%%xmm1                 \n"  // 3*near+far (2, lo)
      "paddw       %%xmm5,%%xmm3                 \n"  // 3*near+far (2, hi)

      // xmm0 xmm2
      // xmm1 xmm3

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm4,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm7,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16
      "movdqu      %%xmm4,(%1)                   \n"

      "movdqa      %%xmm2,%%xmm4                 \n"
      "movdqa      %%xmm3,%%xmm5                 \n"
      "paddw       %%xmm4,%%xmm4                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm7,%%xmm5                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm2,%%xmm4                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16
      "movdqu      %%xmm4,0x10(%1)               \n"

      "movdqa      %%xmm1,%%xmm4                 \n"
      "paddw       %%xmm7,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm4,%%xmm4                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm4,%%xmm1                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm0,%%xmm1                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrlw       $4,%%xmm1                     \n"  // ^ div by 16
      "movdqu      %%xmm1,(%1,%4,2)              \n"

      "movdqa      %%xmm3,%%xmm4                 \n"
      "paddw       %%xmm7,%%xmm2                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm4,%%xmm4                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm4,%%xmm3                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm2,%%xmm3                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrlw       $4,%%xmm3                     \n"  // ^ div by 16
      "movdqu      %%xmm3,0x10(%1,%4,2)          \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),          // %0
        "+r"(dst_ptr),          // %1
        "+r"(dst_width)         // %2
      : "r"(src_stride),        // %3
        "r"(dst_stride),        // %4
        "m"(kLinearShuffleFar)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_16_SSE2
void ScaleRowUp2_Linear_16_SSE2(const uint16_t* src_ptr,
                                uint16_t* dst_ptr,
                                int dst_width) {
  asm volatile(
      "pxor        %%xmm5,%%xmm5                 \n"
      "pcmpeqd     %%xmm4,%%xmm4                 \n"
      "psrld       $31,%%xmm4                    \n"
      "pslld       $1,%%xmm4                     \n"  // all 2

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 0123 (16b)
      "movq        2(%0),%%xmm1                  \n"  // 1234 (16b)

      "punpcklwd   %%xmm5,%%xmm0                 \n"  // 0123 (32b)
      "punpcklwd   %%xmm5,%%xmm1                 \n"  // 1234 (32b)

      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"

      "pshufd      $0b10110001,%%xmm2,%%xmm2     \n"  // 1032 (even, far)
      "pshufd      $0b10110001,%%xmm3,%%xmm3     \n"  // 2143 (odd, far)

      "paddd       %%xmm4,%%xmm2                 \n"  // far+2 (lo)
      "paddd       %%xmm4,%%xmm3                 \n"  // far+2 (hi)
      "paddd       %%xmm0,%%xmm2                 \n"  // near+far+2 (lo)
      "paddd       %%xmm1,%%xmm3                 \n"  // near+far+2 (hi)
      "paddd       %%xmm0,%%xmm0                 \n"  // 2*near (lo)
      "paddd       %%xmm1,%%xmm1                 \n"  // 2*near (hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddd       %%xmm3,%%xmm1                 \n"  // 3*near+far+2 (hi)

      "psrld       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrld       $2,%%xmm1                     \n"  // 3/4*near+1/4*far (hi)
      "pshuflw     $0x88,%%xmm0,%%xmm0           \n"
      "pshufhw     $0x88,%%xmm0,%%xmm0           \n"
      "pshufd      $0x88,%%xmm0,%%xmm0           \n"
      "pshuflw     $0x88,%%xmm1,%%xmm1           \n"
      "pshufhw     $0x88,%%xmm1,%%xmm1           \n"
      "pshufd      $0x88,%%xmm1,%%xmm1           \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 pixel to 8 pixel
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_16_SSE2
void ScaleRowUp2_Bilinear_16_SSE2(const uint16_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint16_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  asm volatile(
      "pxor        %%xmm7,%%xmm7                 \n"
      "pcmpeqd     %%xmm6,%%xmm6                 \n"
      "psrld       $31,%%xmm6                    \n"
      "pslld       $3,%%xmm6                     \n"  // all 8

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 0123 (16b)
      "movq        2(%0),%%xmm1                  \n"  // 1234 (16b)
      "punpcklwd   %%xmm7,%%xmm0                 \n"  // 0123 (32b)
      "punpcklwd   %%xmm7,%%xmm1                 \n"  // 1234 (32b)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "pshufd      $0b10110001,%%xmm2,%%xmm2     \n"  // 1032 (even, far)
      "pshufd      $0b10110001,%%xmm3,%%xmm3     \n"  // 2143 (odd, far)
      "paddd       %%xmm0,%%xmm2                 \n"  // near+far (lo)
      "paddd       %%xmm1,%%xmm3                 \n"  // near+far (hi)
      "paddd       %%xmm0,%%xmm0                 \n"  // 2*near (lo)
      "paddd       %%xmm1,%%xmm1                 \n"  // 2*near (hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 3*near+far (1, lo)
      "paddd       %%xmm3,%%xmm1                 \n"  // 3*near+far (1, hi)

      "movq        (%0,%3,2),%%xmm2              \n"
      "movq        2(%0,%3,2),%%xmm3             \n"
      "punpcklwd   %%xmm7,%%xmm2                 \n"  // 0123 (32b)
      "punpcklwd   %%xmm7,%%xmm3                 \n"  // 1234 (32b)
      "movdqa      %%xmm2,%%xmm4                 \n"
      "movdqa      %%xmm3,%%xmm5                 \n"
      "pshufd      $0b10110001,%%xmm4,%%xmm4     \n"  // 1032 (even, far)
      "pshufd      $0b10110001,%%xmm5,%%xmm5     \n"  // 2143 (odd, far)
      "paddd       %%xmm2,%%xmm4                 \n"  // near+far (lo)
      "paddd       %%xmm3,%%xmm5                 \n"  // near+far (hi)
      "paddd       %%xmm2,%%xmm2                 \n"  // 2*near (lo)
      "paddd       %%xmm3,%%xmm3                 \n"  // 2*near (hi)
      "paddd       %%xmm4,%%xmm2                 \n"  // 3*near+far (2, lo)
      "paddd       %%xmm5,%%xmm3                 \n"  // 3*near+far (2, hi)

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm2,%%xmm5                 \n"
      "paddd       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddd       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddd       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddd       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrld       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm2,%%xmm5                 \n"
      "paddd       %%xmm2,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddd       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddd       %%xmm2,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddd       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrld       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm2                 \n"
      "paddd       %%xmm1,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddd       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (2, hi)
      "paddd       %%xmm1,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrld       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm2                 \n"
      "paddd       %%xmm3,%%xmm2                 \n"  // 6*near+2*far (2, hi)
      "paddd       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (1, hi)
      "paddd       %%xmm3,%%xmm2                 \n"  // 9*near+3*far (2, hi)
      "paddd       %%xmm1,%%xmm2                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrld       $4,%%xmm2                     \n"  // ^ div by 16 (2, hi)

      "pshuflw     $0x88,%%xmm4,%%xmm4           \n"
      "pshufhw     $0x88,%%xmm4,%%xmm4           \n"
      "pshufd      $0x88,%%xmm4,%%xmm4           \n"
      "pshuflw     $0x88,%%xmm0,%%xmm0           \n"
      "pshufhw     $0x88,%%xmm0,%%xmm0           \n"
      "pshufd      $0x88,%%xmm0,%%xmm0           \n"
      "punpckldq   %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "pshuflw     $0x88,%%xmm5,%%xmm5           \n"
      "pshufhw     $0x88,%%xmm5,%%xmm5           \n"
      "pshufd      $0x88,%%xmm5,%%xmm5           \n"
      "pshuflw     $0x88,%%xmm2,%%xmm2           \n"
      "pshufhw     $0x88,%%xmm2,%%xmm2           \n"
      "pshufd      $0x88,%%xmm2,%%xmm2           \n"
      "punpckldq   %%xmm2,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4,2)              \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 pixel to 8 pixel
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width)   // %2
      : "r"(src_stride),  // %3
        "r"(dst_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_SSSE3
void ScaleRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $15,%%xmm4                    \n"
      "psllw       $1,%%xmm4                     \n"  // all 2
      "movdqa      %3,%%xmm3                     \n"

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 01234567
      "movq        1(%0),%%xmm1                  \n"  // 12345678
      "punpcklwd   %%xmm0,%%xmm0                 \n"  // 0101232345456767
      "punpcklwd   %%xmm1,%%xmm1                 \n"  // 1212343456567878
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 4545565667677878
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 0101121223233434
      "pmaddubsw   %%xmm3,%%xmm2                 \n"  // 3*near+far (hi)
      "pmaddubsw   %%xmm3,%%xmm0                 \n"  // 3*near+far (lo)
      "paddw       %%xmm4,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddw       %%xmm4,%%xmm2                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrlw       $2,%%xmm2                     \n"  // 3/4*near+1/4*far (hi)
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),      // %0
        "+r"(dst_ptr),      // %1
        "+r"(dst_width)     // %2
      : "m"(kLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_SSSE3
void ScaleRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint8_t* dst_ptr,
                                ptrdiff_t dst_stride,
                                int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $3,%%xmm6                     \n"  // all 8
      "movdqa      %5,%%xmm7                     \n"

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 01234567
      "movq        1(%0),%%xmm1                  \n"  // 12345678
      "punpcklwd   %%xmm0,%%xmm0                 \n"  // 0101232345456767
      "punpcklwd   %%xmm1,%%xmm1                 \n"  // 1212343456567878
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 4545565667677878
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 0101121223233434
      "pmaddubsw   %%xmm7,%%xmm2                 \n"  // 3*near+far (1, hi)
      "pmaddubsw   %%xmm7,%%xmm0                 \n"  // 3*near+far (1, lo)

      "movq        (%0,%3),%%xmm1                \n"
      "movq        1(%0,%3),%%xmm4               \n"
      "punpcklwd   %%xmm1,%%xmm1                 \n"
      "punpcklwd   %%xmm4,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpckhdq   %%xmm4,%%xmm3                 \n"
      "punpckldq   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm7,%%xmm3                 \n"  // 3*near+far (2, hi)
      "pmaddubsw   %%xmm7,%%xmm1                 \n"  // 3*near+far (2, lo)

      // xmm0 xmm2
      // xmm1 xmm3

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm1,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm1,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm2,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm2,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm1,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm1                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm2,%%xmm1                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrlw       $4,%%xmm1                     \n"  // ^ div by 16 (2, hi)

      "packuswb    %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),      // %0
        "+r"(dst_ptr),      // %1
        "+r"(dst_width)     // %2
      : "r"(src_stride),    // %3
        "r"(dst_stride),    // %4
        "m"(kLinearMadd31)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_AVX2
void ScaleRowUp2_Linear_AVX2(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int dst_width) {
  asm volatile(
      "vpcmpeqw    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrlw      $15,%%ymm4,%%ymm4             \n"
      "vpsllw      $1,%%ymm4,%%ymm4              \n"  // all 2
      "vbroadcastf128 %3,%%ymm3                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // 0123456789ABCDEF
      "vmovdqu     1(%0),%%xmm1                  \n"  // 123456789ABCDEF0
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"
      "vpunpcklwd  %%ymm0,%%ymm0,%%ymm0          \n"
      "vpunpcklwd  %%ymm1,%%ymm1,%%ymm1          \n"
      "vpunpckhdq  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpckldq  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm3,%%ymm2,%%ymm1          \n"  // 3*near+far (hi)
      "vpmaddubsw  %%ymm3,%%ymm0,%%ymm0          \n"  // 3*near+far (lo)
      "vpaddw      %%ymm4,%%ymm0,%%ymm0          \n"  // 3*near+far+2 (lo)
      "vpaddw      %%ymm4,%%ymm1,%%ymm1          \n"  // 3*near+far+2 (hi)
      "vpsrlw      $2,%%ymm0,%%ymm0              \n"  // 3/4*near+1/4*far (lo)
      "vpsrlw      $2,%%ymm1,%%ymm1              \n"  // 3/4*near+1/4*far (hi)
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 16 sample to 32 sample
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),      // %0
        "+r"(dst_ptr),      // %1
        "+r"(dst_width)     // %2
      : "m"(kLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_AVX2
void ScaleRowUp2_Bilinear_AVX2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width) {
  asm volatile(
      "vpcmpeqw    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrlw      $15,%%ymm6,%%ymm6             \n"
      "vpsllw      $3,%%ymm6,%%ymm6              \n"  // all 8
      "vbroadcastf128 %5,%%ymm7                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // 0123456789ABCDEF
      "vmovdqu     1(%0),%%xmm1                  \n"  // 123456789ABCDEF0
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"
      "vpunpcklwd  %%ymm0,%%ymm0,%%ymm0          \n"
      "vpunpcklwd  %%ymm1,%%ymm1,%%ymm1          \n"
      "vpunpckhdq  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpckldq  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm1          \n"  // 3*near+far (1, hi)
      "vpmaddubsw  %%ymm7,%%ymm0,%%ymm0          \n"  // 3*near+far (1, lo)

      "vmovdqu     (%0,%3),%%xmm2                \n"  // 0123456789ABCDEF
      "vmovdqu     1(%0,%3),%%xmm3               \n"  // 123456789ABCDEF0
      "vpermq      $0b11011000,%%ymm2,%%ymm2     \n"
      "vpermq      $0b11011000,%%ymm3,%%ymm3     \n"
      "vpunpcklwd  %%ymm2,%%ymm2,%%ymm2          \n"
      "vpunpcklwd  %%ymm3,%%ymm3,%%ymm3          \n"
      "vpunpckhdq  %%ymm3,%%ymm2,%%ymm4          \n"
      "vpunpckldq  %%ymm3,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm7,%%ymm4,%%ymm3          \n"  // 3*near+far (2, hi)
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm2          \n"  // 3*near+far (2, lo)

      // ymm0 ymm1
      // ymm2 ymm3

      "vpaddw      %%ymm0,%%ymm0,%%ymm4          \n"  // 6*near+2*far (1, lo)
      "vpaddw      %%ymm6,%%ymm2,%%ymm5          \n"  // 3*near+far+8 (2, lo)
      "vpaddw      %%ymm4,%%ymm0,%%ymm4          \n"  // 9*near+3*far (1, lo)
      "vpaddw      %%ymm4,%%ymm5,%%ymm4          \n"  // 9 3 3 1 + 8 (1, lo)
      "vpsrlw      $4,%%ymm4,%%ymm4              \n"  // ^ div by 16 (1, lo)

      "vpaddw      %%ymm2,%%ymm2,%%ymm5          \n"  // 6*near+2*far (2, lo)
      "vpaddw      %%ymm6,%%ymm0,%%ymm0          \n"  // 3*near+far+8 (1, lo)
      "vpaddw      %%ymm5,%%ymm2,%%ymm5          \n"  // 9*near+3*far (2, lo)
      "vpaddw      %%ymm5,%%ymm0,%%ymm5          \n"  // 9 3 3 1 + 8 (2, lo)
      "vpsrlw      $4,%%ymm5,%%ymm5              \n"  // ^ div by 16 (2, lo)

      "vpaddw      %%ymm1,%%ymm1,%%ymm0          \n"  // 6*near+2*far (1, hi)
      "vpaddw      %%ymm6,%%ymm3,%%ymm2          \n"  // 3*near+far+8 (2, hi)
      "vpaddw      %%ymm0,%%ymm1,%%ymm0          \n"  // 9*near+3*far (1, hi)
      "vpaddw      %%ymm0,%%ymm2,%%ymm0          \n"  // 9 3 3 1 + 8 (1, hi)
      "vpsrlw      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16 (1, hi)

      "vpaddw      %%ymm3,%%ymm3,%%ymm2          \n"  // 6*near+2*far (2, hi)
      "vpaddw      %%ymm6,%%ymm1,%%ymm1          \n"  // 3*near+far+8 (1, hi)
      "vpaddw      %%ymm2,%%ymm3,%%ymm2          \n"  // 9*near+3*far (2, hi)
      "vpaddw      %%ymm2,%%ymm1,%%ymm2          \n"  // 9 3 3 1 + 8 (2, hi)
      "vpsrlw      $4,%%ymm2,%%ymm2              \n"  // ^ div by 16 (2, hi)

      "vpackuswb   %%ymm0,%%ymm4,%%ymm4          \n"
      "vmovdqu     %%ymm4,(%1)                   \n"  // store above
      "vpackuswb   %%ymm2,%%ymm5,%%ymm5          \n"
      "vmovdqu     %%ymm5,(%1,%4)                \n"  // store below

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 16 sample to 32 sample
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),      // %0
        "+r"(dst_ptr),      // %1
        "+r"(dst_width)     // %2
      : "r"(src_stride),    // %3
        "r"(dst_stride),    // %4
        "m"(kLinearMadd31)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_12_AVX2
void ScaleRowUp2_Linear_12_AVX2(const uint16_t* src_ptr,
                                uint16_t* dst_ptr,
                                int dst_width) {
  asm volatile(
      "vbroadcastf128 %3,%%ymm5                  \n"
      "vpcmpeqw    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrlw      $15,%%ymm4,%%ymm4             \n"
      "vpsllw      $1,%%ymm4,%%ymm4              \n"  // all 2

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // 0123456789ABCDEF (16b)
      "vmovdqu     2(%0),%%ymm1                  \n"  // 123456789ABCDEF0 (16b)

      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"  // 012389AB4567CDEF
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"  // 12349ABC5678DEF0

      "vpunpckhwd  %%ymm1,%%ymm0,%%ymm2          \n"  // 899AABBCCDDEEFF0 (near)
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"  // 0112233445566778 (near)
      "vpshufb     %%ymm5,%%ymm2,%%ymm3          \n"  // 98A9BACBDCEDFE0F (far)
      "vpshufb     %%ymm5,%%ymm0,%%ymm1          \n"  // 1021324354657687 (far)

      "vpaddw      %%ymm4,%%ymm1,%%ymm1          \n"  // far+2
      "vpaddw      %%ymm4,%%ymm3,%%ymm3          \n"  // far+2
      "vpaddw      %%ymm0,%%ymm1,%%ymm1          \n"  // near+far+2
      "vpaddw      %%ymm2,%%ymm3,%%ymm3          \n"  // near+far+2
      "vpaddw      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near
      "vpaddw      %%ymm2,%%ymm2,%%ymm2          \n"  // 2*near
      "vpaddw      %%ymm0,%%ymm1,%%ymm0          \n"  // 3*near+far+2
      "vpaddw      %%ymm2,%%ymm3,%%ymm2          \n"  // 3*near+far+2

      "vpsrlw      $2,%%ymm0,%%ymm0              \n"  // 3/4*near+1/4*far
      "vpsrlw      $2,%%ymm2,%%ymm2              \n"  // 3/4*near+1/4*far
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm2,32(%1)                 \n"

      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%1),%1                   \n"  // 16 sample to 32 sample
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),          // %0
        "+r"(dst_ptr),          // %1
        "+r"(dst_width)         // %2
      : "m"(kLinearShuffleFar)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_12_AVX2
void ScaleRowUp2_Bilinear_12_AVX2(const uint16_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint16_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  asm volatile(
      "vbroadcastf128 %5,%%ymm5                  \n"
      "vpcmpeqw    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrlw      $15,%%ymm4,%%ymm4             \n"
      "vpsllw      $3,%%ymm4,%%ymm4              \n"  // all 8

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0),%%xmm0                   \n"  // 01234567 (16b)
      "vmovdqu     2(%0),%%xmm1                  \n"  // 12345678 (16b)
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"  // 0123000045670000
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"  // 1234000056780000
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"  // 0112233445566778 (near)
      "vpshufb     %%ymm5,%%ymm0,%%ymm1          \n"  // 1021324354657687 (far)
      "vpaddw      %%ymm0,%%ymm1,%%ymm1          \n"  // near+far
      "vpaddw      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near
      "vpaddw      %%ymm0,%%ymm1,%%ymm2          \n"  // 3*near+far (1)

      "vmovdqu     (%0,%3,2),%%xmm0              \n"  // 01234567 (16b)
      "vmovdqu     2(%0,%3,2),%%xmm1             \n"  // 12345678 (16b)
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"  // 0123000045670000
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"  // 1234000056780000
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"  // 0112233445566778 (near)
      "vpshufb     %%ymm5,%%ymm0,%%ymm1          \n"  // 1021324354657687 (far)
      "vpaddw      %%ymm0,%%ymm1,%%ymm1          \n"  // near+far
      "vpaddw      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near
      "vpaddw      %%ymm0,%%ymm1,%%ymm3          \n"  // 3*near+far (2)

      "vpaddw      %%ymm2,%%ymm2,%%ymm0          \n"  // 6*near+2*far (1)
      "vpaddw      %%ymm4,%%ymm3,%%ymm1          \n"  // 3*near+far+8 (2)
      "vpaddw      %%ymm0,%%ymm2,%%ymm0          \n"  // 9*near+3*far (1)
      "vpaddw      %%ymm0,%%ymm1,%%ymm0          \n"  // 9 3 3 1 + 8 (1)
      "vpsrlw      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16
      "vmovdqu     %%ymm0,(%1)                   \n"  // store above

      "vpaddw      %%ymm3,%%ymm3,%%ymm0          \n"  // 6*near+2*far (2)
      "vpaddw      %%ymm4,%%ymm2,%%ymm1          \n"  // 3*near+far+8 (1)
      "vpaddw      %%ymm0,%%ymm3,%%ymm0          \n"  // 9*near+3*far (2)
      "vpaddw      %%ymm0,%%ymm1,%%ymm0          \n"  // 9 3 3 1 + 8 (2)
      "vpsrlw      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16
      "vmovdqu     %%ymm0,(%1,%4,2)              \n"  // store below

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),          // %0
        "+r"(dst_ptr),          // %1
        "+r"(dst_width)         // %2
      : "r"(src_stride),        // %3
        "r"(dst_stride),        // %4
        "m"(kLinearShuffleFar)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_SCALEROWUP2_LINEAR_16_AVX2
void ScaleRowUp2_Linear_16_AVX2(const uint16_t* src_ptr,
                                uint16_t* dst_ptr,
                                int dst_width) {
  asm volatile(
      "vpcmpeqd    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $31,%%ymm4,%%ymm4             \n"
      "vpslld      $1,%%ymm4,%%ymm4              \n"  // all 2

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // 01234567 (16b, 1u1v)
      "vmovdqu     2(%0),%%xmm1                  \n"  // 12345678 (16b, 1u1v)

      "vpmovzxwd   %%xmm0,%%ymm0                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm1,%%ymm1                 \n"  // 12345678 (32b, 1u1v)

      "vpshufd     $0b10110001,%%ymm0,%%ymm2     \n"  // 10325476 (lo, far)
      "vpshufd     $0b10110001,%%ymm1,%%ymm3     \n"  // 21436587 (hi, far)

      "vpaddd      %%ymm4,%%ymm2,%%ymm2          \n"  // far+2 (lo)
      "vpaddd      %%ymm4,%%ymm3,%%ymm3          \n"  // far+2 (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"  // near+far+2 (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm3          \n"  // near+far+2 (hi)
      "vpaddd      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near (lo)
      "vpaddd      %%ymm1,%%ymm1,%%ymm1          \n"  // 2*near (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 3*near+far+2 (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm1          \n"  // 3*near+far+2 (hi)

      "vpsrld      $2,%%ymm0,%%ymm0              \n"  // 3/4*near+1/4*far (lo)
      "vpsrld      $2,%%ymm1,%%ymm1              \n"  // 3/4*near+1/4*far (hi)
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpshufd     $0b11011000,%%ymm0,%%ymm0     \n"
      "vmovdqu     %%ymm0,(%1)                   \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 pixel to 16 pixel
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_16_AVX2
void ScaleRowUp2_Bilinear_16_AVX2(const uint16_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint16_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  asm volatile(
      "vpcmpeqd    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrld      $31,%%ymm6,%%ymm6             \n"
      "vpslld      $3,%%ymm6,%%ymm6              \n"  // all 8

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0),%%xmm0                   \n"  // 01234567 (16b, 1u1v)
      "vmovdqu     2(%0),%%xmm1                  \n"  // 12345678 (16b, 1u1v)
      "vpmovzxwd   %%xmm0,%%ymm0                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm1,%%ymm1                 \n"  // 12345678 (32b, 1u1v)
      "vpshufd     $0b10110001,%%ymm0,%%ymm2     \n"  // 10325476 (lo, far)
      "vpshufd     $0b10110001,%%ymm1,%%ymm3     \n"  // 21436587 (hi, far)
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"  // near+far (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm3          \n"  // near+far (hi)
      "vpaddd      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near (lo)
      "vpaddd      %%ymm1,%%ymm1,%%ymm1          \n"  // 2*near (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 3*near+far (1, lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm1          \n"  // 3*near+far (1, hi)

      "vmovdqu     (%0,%3,2),%%xmm2              \n"  // 01234567 (16b, 1u1v)
      "vmovdqu     2(%0,%3,2),%%xmm3             \n"  // 12345678 (16b, 1u1v)
      "vpmovzxwd   %%xmm2,%%ymm2                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm3,%%ymm3                 \n"  // 12345678 (32b, 1u1v)
      "vpshufd     $0b10110001,%%ymm2,%%ymm4     \n"  // 10325476 (lo, far)
      "vpshufd     $0b10110001,%%ymm3,%%ymm5     \n"  // 21436587 (hi, far)
      "vpaddd      %%ymm2,%%ymm4,%%ymm4          \n"  // near+far (lo)
      "vpaddd      %%ymm3,%%ymm5,%%ymm5          \n"  // near+far (hi)
      "vpaddd      %%ymm2,%%ymm2,%%ymm2          \n"  // 2*near (lo)
      "vpaddd      %%ymm3,%%ymm3,%%ymm3          \n"  // 2*near (hi)
      "vpaddd      %%ymm2,%%ymm4,%%ymm2          \n"  // 3*near+far (2, lo)
      "vpaddd      %%ymm3,%%ymm5,%%ymm3          \n"  // 3*near+far (2, hi)

      "vpaddd      %%ymm0,%%ymm0,%%ymm4          \n"  // 6*near+2*far (1, lo)
      "vpaddd      %%ymm6,%%ymm2,%%ymm5          \n"  // 3*near+far+8 (2, lo)
      "vpaddd      %%ymm4,%%ymm0,%%ymm4          \n"  // 9*near+3*far (1, lo)
      "vpaddd      %%ymm4,%%ymm5,%%ymm4          \n"  // 9 3 3 1 + 8 (1, lo)
      "vpsrld      $4,%%ymm4,%%ymm4              \n"  // ^ div by 16 (1, lo)

      "vpaddd      %%ymm2,%%ymm2,%%ymm5          \n"  // 6*near+2*far (2, lo)
      "vpaddd      %%ymm6,%%ymm0,%%ymm0          \n"  // 3*near+far+8 (1, lo)
      "vpaddd      %%ymm5,%%ymm2,%%ymm5          \n"  // 9*near+3*far (2, lo)
      "vpaddd      %%ymm5,%%ymm0,%%ymm5          \n"  // 9 3 3 1 + 8 (2, lo)
      "vpsrld      $4,%%ymm5,%%ymm5              \n"  // ^ div by 16 (2, lo)

      "vpaddd      %%ymm1,%%ymm1,%%ymm0          \n"  // 6*near+2*far (1, hi)
      "vpaddd      %%ymm6,%%ymm3,%%ymm2          \n"  // 3*near+far+8 (2, hi)
      "vpaddd      %%ymm0,%%ymm1,%%ymm0          \n"  // 9*near+3*far (1, hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 9 3 3 1 + 8 (1, hi)
      "vpsrld      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16 (1, hi)

      "vpaddd      %%ymm3,%%ymm3,%%ymm2          \n"  // 6*near+2*far (2, hi)
      "vpaddd      %%ymm6,%%ymm1,%%ymm1          \n"  // 3*near+far+8 (1, hi)
      "vpaddd      %%ymm2,%%ymm3,%%ymm2          \n"  // 9*near+3*far (2, hi)
      "vpaddd      %%ymm2,%%ymm1,%%ymm2          \n"  // 9 3 3 1 + 8 (2, hi)
      "vpsrld      $4,%%ymm2,%%ymm2              \n"  // ^ div by 16 (2, hi)

      "vpackusdw   %%ymm0,%%ymm4,%%ymm4          \n"
      "vpshufd     $0b11011000,%%ymm4,%%ymm4     \n"
      "vmovdqu     %%ymm4,(%1)                   \n"  // store above
      "vpackusdw   %%ymm2,%%ymm5,%%ymm5          \n"
      "vpshufd     $0b11011000,%%ymm5,%%ymm5     \n"
      "vmovdqu     %%ymm5,(%1,%4,2)              \n"  // store below

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 pixel to 16 pixel
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width)   // %2
      : "r"(src_stride),  // %3
        "r"(dst_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif

// Reads 16xN bytes and produces 16 shorts at a time.
void ScaleAddRow_SSE2(const uint8_t* src_ptr,
                      uint16_t* dst_ptr,
                      int src_width) {
  asm volatile("pxor        %%xmm5,%%xmm5                 \n"

               // 16 pixel loop.
               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm3                   \n"
               "lea         0x10(%0),%0                   \n"  // src_ptr += 16
               "movdqu      (%1),%%xmm0                   \n"
               "movdqu      0x10(%1),%%xmm1               \n"
               "movdqa      %%xmm3,%%xmm2                 \n"
               "punpcklbw   %%xmm5,%%xmm2                 \n"
               "punpckhbw   %%xmm5,%%xmm3                 \n"
               "paddusw     %%xmm2,%%xmm0                 \n"
               "paddusw     %%xmm3,%%xmm1                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src_ptr),   // %0
                 "+r"(dst_ptr),   // %1
                 "+r"(src_width)  // %2
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}

#ifdef HAS_SCALEADDROW_AVX2
// Reads 32 bytes and accumulates to 32 shorts at a time.
void ScaleAddRow_AVX2(const uint8_t* src_ptr,
                      uint16_t* dst_ptr,
                      int src_width) {
  asm volatile("vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm3                   \n"
               "lea         0x20(%0),%0                   \n"  // src_ptr += 32
               "vpermq      $0xd8,%%ymm3,%%ymm3           \n"
               "vpunpcklbw  %%ymm5,%%ymm3,%%ymm2          \n"
               "vpunpckhbw  %%ymm5,%%ymm3,%%ymm3          \n"
               "vpaddusw    (%1),%%ymm2,%%ymm0            \n"
               "vpaddusw    0x20(%1),%%ymm3,%%ymm1        \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "vmovdqu     %%ymm1,0x20(%1)               \n"
               "lea         0x40(%1),%1                   \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_ptr),   // %0
                 "+r"(dst_ptr),   // %1
                 "+r"(src_width)  // %2
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SCALEADDROW_AVX2

#ifdef HAS_SCALEADDROW_AVX512BW
// Reads 32xN bytes and accumulates to 32xN shorts at a time, with masked tail.
LIBYUV_TARGET_AVX512BW
void ScaleAddRow_AVX512BW(const uint8_t* src_ptr,
                          uint16_t* dst_ptr,
                          int src_width) {
  uintptr_t temp;
  asm volatile(
      "sub         $0x40,%2                      \n"
      "jl          2f                            \n"

      // 64 pixel loop.
      LABELALIGN
      "1:          \n"
      "vpmovzxbw   (%0),%%zmm0                   \n"
      "vpmovzxbw   0x20(%0),%%zmm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpaddusw    (%1),%%zmm0,%%zmm0            \n"
      "vpaddusw    0x40(%1),%%zmm1,%%zmm1        \n"
      "vmovdqu16   %%zmm0,(%1)                   \n"
      "vmovdqu16   %%zmm1,0x40(%1)               \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jl          3f                            \n"
      "vpmovzxbw   (%0),%%zmm0                   \n"
      "lea         0x20(%0),%0                   \n"
      "vpaddusw    (%1),%%zmm0,%%zmm0            \n"
      "vmovdqu16   %%zmm0,(%1)                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x20,%2                      \n"

      "3:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovd       %k3,%%k1                      \n"
      "vpmovzxbw   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vpaddusw    (%1),%%zmm0,%%zmm0%{%%k1%}%{z%}\n"
      "vmovdqu16   %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(src_width),  // %2
        "=&r"(temp)       // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "k1");
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
  intptr_t temp, mask;
  asm volatile(
      "mov         $0x0000ffff,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm6              \n"
      "vpbroadcastd %k[scaleval],%%zmm7          \n"
      "cmp         $4,%k[bw]                     \n"
      "jg          20f                           \n"

      // Path 1: 1 <= boxwidth <= 4 (16 output pixels per loop).
      "vpmovzxbd   %[kSeq],%%zmm3                \n"
      "vpslld      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpbroadcastw %k[bw],%%zmm2                \n"
      "vpmullw     %%zmm2,%%zmm3,%%zmm3          \n"
      "mov         $0x00010000,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm0              \n"
      "mov         $0x00030002,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm1              \n"
      "vpaddw      %%zmm0,%%zmm3,%%zmm4          \n"
      "vpaddw      %%zmm1,%%zmm3,%%zmm5          \n"
      "vpcmpuw     $6,%%zmm0,%%zmm2,%%k2         \n"
      "vpcmpuw     $6,%%zmm1,%%zmm2,%%k3         \n"
      "mov         %k[bw],%k[temp]               \n"
      "shl         $4,%k[temp]                   \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k4                 \n"
      "shl         $1,%q[temp]                   \n"
      "sub         $0x10,%[dst_width]            \n"
      "jl          12f                           \n"

      LABELALIGN
      "11:         \n"
      "vmovdqu16   (%[src_ptr]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr]),%%zmm1%{%%k4%}%{z%} \n"
      "add         %q[temp],%[src_ptr]           \n"
      "vmovdqa64   %%zmm4,%%zmm2                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm2%{%%k2%}%{z%}    \n"
      "vmovdqa64   %%zmm5,%%zmm3                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm3%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm2,%%zmm0             \n"
      "vpandd      %%zmm6,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm3,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpaddd      %%zmm1,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpmulld     %%zmm7,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpmovdb     %%zmm2,(%[dst_ptr])           \n"
      "lea         0x10(%[dst_ptr]),%[dst_ptr]   \n"
      "sub         $0x10,%[dst_width]            \n"
      "jge         11b                           \n"

      "12:         \n"
      "add         $0x10,%[dst_width]            \n"
      "jle         99f                           \n"
      "mov         %k[dst_width],%k[temp]        \n"
      "imul        %k[bw],%k[temp]               \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k4                 \n"
      "vmovdqu16   (%[src_ptr]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr]),%%zmm1%{%%k4%}%{z%} \n"
      "vmovdqa64   %%zmm4,%%zmm2                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm2%{%%k2%}%{z%}    \n"
      "vmovdqa64   %%zmm5,%%zmm3                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm3%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm2,%%zmm0             \n"
      "vpandd      %%zmm6,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm3,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpaddd      %%zmm1,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpmulld     %%zmm7,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[dst_width],%q[mask],%q[mask]\n"
      "kmovw       %k[mask],%%k1                 \n"
      "vpmovdb     %%zmm2,(%[dst_ptr])%{%%k1%}   \n"
      "jmp         99f                           \n"

      // Path 2: 5 <= boxwidth <= 8 (8 output pixels per loop).
      "20:         \n"
      "cmp         $8,%k[bw]                     \n"
      "jg          30f                           \n"
      "vpmovzxbq   %[kSeq],%%zmm3                \n"
      "vpsllq      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpsllq      $32,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpbroadcastw %k[bw],%%zmm2                \n"
      "vpmullw     %%zmm2,%%zmm3,%%zmm3          \n"
      "movabs      $0x0003000200010000,%q[temp]  \n"
      "vpbroadcastq %q[temp],%%zmm0              \n"
      "movabs      $0x0007000600050004,%q[temp]  \n"
      "vpbroadcastq %q[temp],%%zmm1              \n"
      "vpaddw      %%zmm0,%%zmm3,%%zmm4          \n"
      "vpaddw      %%zmm1,%%zmm3,%%zmm5          \n"
      "vpcmpuw     $6,%%zmm1,%%zmm2,%%k3         \n"
      "mov         %k[bw],%k[temp]               \n"
      "shl         $3,%k[temp]                   \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k4                 \n"
      "shl         $1,%q[temp]                   \n"
      "sub         $0x8,%[dst_width]             \n"
      "jl          22f                           \n"

      LABELALIGN
      "21:         \n"
      "vmovdqu16   (%[src_ptr]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr]),%%zmm1%{%%k4%}%{z%} \n"
      "add         %q[temp],%[src_ptr]           \n"
      "vmovdqa64   %%zmm4,%%zmm2                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm2          \n"
      "vmovdqa64   %%zmm5,%%zmm3                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm3%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm2,%%zmm0             \n"
      "vpandd      %%zmm6,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm3,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpaddd      %%zmm1,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpsrlq      $32,%%zmm2,%%zmm0             \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpmulld     %%zmm7,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpmovqb     %%zmm2,(%[dst_ptr])           \n"
      "lea         0x8(%[dst_ptr]),%[dst_ptr]    \n"
      "sub         $0x8,%[dst_width]             \n"
      "jge         21b                           \n"

      "22:         \n"
      "add         $0x8,%[dst_width]             \n"
      "jle         99f                           \n"
      "mov         %k[dst_width],%k[temp]        \n"
      "imul        %k[bw],%k[temp]               \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k4                 \n"
      "vmovdqu16   (%[src_ptr]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr]),%%zmm1%{%%k4%}%{z%} \n"
      "vmovdqa64   %%zmm4,%%zmm2                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm2          \n"
      "vmovdqa64   %%zmm5,%%zmm3                 \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm3%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm2,%%zmm0             \n"
      "vpandd      %%zmm6,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm3,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpaddd      %%zmm1,%%zmm3,%%zmm3          \n"
      "vpaddd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpsrlq      $32,%%zmm2,%%zmm0             \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "vpmulld     %%zmm7,%%zmm2,%%zmm2          \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[dst_width],%q[mask],%q[mask]\n"
      "kmovw       %k[mask],%%k1                 \n"
      "vpmovqb     %%zmm2,(%[dst_ptr])%{%%k1%}   \n"
      "jmp         99f                           \n"

      // Path 3: boxwidth >= 9 (ZMM 32-word reduction per output pixel).
      "30:         \n"
      "mov         %k[bw],%k[temp]               \n"
      "and         $0x1f,%k[temp]                \n"
      "mov         $-1,%k[mask]                  \n"
      "bzhi        %k[temp],%k[mask],%k[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"

      LABELALIGN
      "31:         \n"
      "vpxord      %%zmm2,%%zmm2,%%zmm2          \n"
      "mov         %[src_ptr],%q[mask]           \n"
      "lea         (%[src_ptr],%q[bw],2),%[src_ptr] \n"
      "mov         %q[bw],%q[temp]               \n"
      "sub         $0x20,%q[temp]                \n"
      "jl          33f                           \n"
      "32:         \n"
      "vmovdqu16   (%q[mask]),%%zmm0             \n"
      "lea         0x40(%q[mask]),%q[mask]       \n"
      "vpsrld      $16,%%zmm0,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm1,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "sub         $0x20,%q[temp]                \n"
      "jge         32b                           \n"
      "33:         \n"
      "add         $0x20,%q[temp]                \n"
      "jle         34f                           \n"
      "vmovdqu16   (%q[mask]),%%zmm0%{%%k1%}%{z%}\n"
      "vpsrld      $16,%%zmm0,%%zmm1             \n"
      "vpandd      %%zmm6,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm1,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "34:         \n"
      "vextracti32x8 $1,%%zmm2,%%ymm0            \n"
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"
      "vextracti128 $1,%%ymm2,%%xmm0             \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpshufd     $0xee,%%xmm2,%%xmm0           \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpshufd     $0x55,%%xmm2,%%xmm0           \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpmulld     %%xmm7,%%xmm2,%%xmm2          \n"
      "vpsrld      $16,%%xmm2,%%xmm2             \n"
      "vmovd       %%xmm2,%k[temp]               \n"
      "mov         %b[temp],(%[dst_ptr])         \n"
      "inc         %[dst_ptr]                    \n"
      "dec         %[dst_width]                  \n"
      "jg          31b                           \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr),       // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),       // %[dst_ptr]
        [dst_width] "+r"(dst_width),   // %[dst_width]
        [temp] "=&r"(temp),            // %[temp]
        [mask] "=&r"(mask)             // %[mask]
      : [bw] "r"((intptr_t)boxwidth),  // %[bw]
        [scaleval] "r"(scaleval),      // %[scaleval]
        [kSeq] "m"(kSequence0_15)      // %[kSeq]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7", "k1", "k2", "k3", "k4");
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
  int scaletbl[2];
  scaletbl[0] = 65536 / ((minboxwidth < 1 ? 1 : minboxwidth) * boxheight);
  scaletbl[1] =
      65536 / (((minboxwidth + 1) < 1 ? 1 : (minboxwidth + 1)) * boxheight);
  intptr_t base_ix, temp, mask;
  asm volatile(
      "mov         $0x0000ffff,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm20             \n"
      "vpbroadcastd %[scale0],%%zmm22            \n"
      "vpbroadcastd %[scale1],%%zmm23            \n"
      "cmpl        $1,%[minbw]                   \n"
      "jl          30f                           \n"
      "cmpl        $3,%[minbw]                   \n"
      "jg          20f                           \n"

      // Path 1: 1 <= minboxwidth <= 3 (16 output pixels per loop).
      "vpmovzxbd   %[kSeq],%%zmm16               \n"
      "vpbroadcastd %k[dx],%%zmm17               \n"
      "vpmulld     %%zmm17,%%zmm16,%%zmm16       \n"
      "vpaddd      %%zmm17,%%zmm16,%%zmm17       \n"
      "mov         $0x00010000,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm18             \n"
      "mov         $0x00030002,%k[temp]          \n"
      "vpbroadcastd %k[temp],%%zmm19             \n"
      "vpbroadcastd %[minbw],%%zmm21             \n"
      "sub         $0x10,%[dst_width]            \n"
      "jl          12f                           \n"

      LABELALIGN
      "11:         \n"
      "movslq      %k[x],%q[base_ix]             \n"
      "movzwl      %w[x],%k[temp]                \n"
      "vpbroadcastd %k[temp],%%zmm2              \n"
      "sar         $16,%q[base_ix]               \n"
      "lea         (%k[x],%k[dx],8),%k[temp]     \n"
      "lea         (%k[temp],%k[dx],8),%k[x]     \n"
      "movslq      %k[x],%q[temp]                \n"
      "sar         $16,%q[temp]                  \n"
      "sub         %q[base_ix],%q[temp]          \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k2                 \n"
      "vmovdqu16   (%[src_ptr],%q[base_ix],2),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr],%q[base_ix],2),%%zmm1%{%%k2%}%{z%} \n"

      "vpaddd      %%zmm16,%%zmm2,%%zmm3         \n"
      "vpaddd      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrld      $16,%%zmm3,%%zmm3             \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpsubd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpcmpd      $6,%%zmm21,%%zmm2,%%k4        \n"

      "vpslld      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpslld      $16,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm18,%%zmm3,%%zmm4         \n"
      "vpaddw      %%zmm19,%%zmm3,%%zmm5         \n"
      "vpcmpuw     $6,%%zmm18,%%zmm2,%%k2        \n"
      "vpcmpuw     $6,%%zmm19,%%zmm2,%%k3        \n"

      "vpermi2w    %%zmm1,%%zmm0,%%zmm4%{%%k2%}%{z%}    \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm5%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm4,%%zmm0             \n"
      "vpandd      %%zmm20,%%zmm4,%%zmm4         \n"
      "vpsrld      $16,%%zmm5,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm5,%%zmm5         \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"
      "vpaddd      %%zmm1,%%zmm5,%%zmm5          \n"
      "vpaddd      %%zmm5,%%zmm4,%%zmm4          \n"

      "vpblendmd   %%zmm23,%%zmm22,%%zmm2%{%%k4%}\n"
      "vpmulld     %%zmm2,%%zmm4,%%zmm4          \n"
      "vpsrld      $16,%%zmm4,%%zmm4             \n"
      "vpmovdb     %%zmm4,(%[dst_ptr])           \n"
      "lea         0x10(%[dst_ptr]),%[dst_ptr]   \n"
      "sub         $0x10,%[dst_width]            \n"
      "jge         11b                           \n"

      "12:         \n"
      "add         $0x10,%[dst_width]            \n"
      "jle         99f                           \n"
      "movslq      %k[x],%q[base_ix]             \n"
      "movzwl      %w[x],%k[temp]                \n"
      "vpbroadcastd %k[temp],%%zmm2              \n"
      "sar         $16,%q[base_ix]               \n"
      "mov         %k[dst_width],%k[temp]        \n"
      "imul        %k[dx],%k[temp]               \n"
      "add         %k[x],%k[temp]                \n"
      "sar         $16,%k[temp]                  \n"
      "movslq      %k[temp],%q[temp]             \n"
      "sub         %q[base_ix],%q[temp]          \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k2                 \n"
      "vmovdqu16   (%[src_ptr],%q[base_ix],2),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr],%q[base_ix],2),%%zmm1%{%%k2%}%{z%} \n"

      "vpaddd      %%zmm16,%%zmm2,%%zmm3         \n"
      "vpaddd      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrld      $16,%%zmm3,%%zmm3             \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpsubd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpcmpd      $6,%%zmm21,%%zmm2,%%k4        \n"

      "vpslld      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpslld      $16,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm18,%%zmm3,%%zmm4         \n"
      "vpaddw      %%zmm19,%%zmm3,%%zmm5         \n"
      "vpcmpuw     $6,%%zmm18,%%zmm2,%%k2        \n"
      "vpcmpuw     $6,%%zmm19,%%zmm2,%%k3        \n"

      "vpermi2w    %%zmm1,%%zmm0,%%zmm4%{%%k2%}%{z%}    \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm5%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm4,%%zmm0             \n"
      "vpandd      %%zmm20,%%zmm4,%%zmm4         \n"
      "vpsrld      $16,%%zmm5,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm5,%%zmm5         \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"
      "vpaddd      %%zmm1,%%zmm5,%%zmm5          \n"
      "vpaddd      %%zmm5,%%zmm4,%%zmm4          \n"

      "vpblendmd   %%zmm23,%%zmm22,%%zmm2%{%%k4%}\n"
      "vpmulld     %%zmm2,%%zmm4,%%zmm4          \n"
      "vpsrld      $16,%%zmm4,%%zmm4             \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[dst_width],%q[mask],%q[mask]\n"
      "kmovw       %k[mask],%%k1                 \n"
      "vpmovdb     %%zmm4,(%[dst_ptr])%{%%k1%}   \n"
      "jmp         99f                           \n"

      // Path 2: 4 <= minboxwidth <= 7 (8 output pixels per loop).
      "20:         \n"
      "cmpl        $7,%[minbw]                   \n"
      "jg          30f                           \n"
      "vpmovzxbq   %[kSeq],%%zmm16               \n"
      "movslq      %k[dx],%q[temp]               \n"
      "vpbroadcastq %q[temp],%%zmm17             \n"
      "vpmulld     %%zmm17,%%zmm16,%%zmm16       \n"
      "vpaddq      %%zmm17,%%zmm16,%%zmm17       \n"
      "movabs      $0x0003000200010000,%q[temp]  \n"
      "vpbroadcastq %q[temp],%%zmm18             \n"
      "movabs      $0x0007000600050004,%q[temp]  \n"
      "vpbroadcastq %q[temp],%%zmm19             \n"
      "movslq      %[minbw],%q[temp]             \n"
      "vpbroadcastq %q[temp],%%zmm21             \n"
      "sub         $0x8,%[dst_width]             \n"
      "jl          22f                           \n"

      LABELALIGN
      "21:         \n"
      "movslq      %k[x],%q[base_ix]             \n"
      "movzwl      %w[x],%k[temp]                \n"
      "vpbroadcastq %q[temp],%%zmm2              \n"
      "sar         $16,%q[base_ix]               \n"
      "lea         (%k[x],%k[dx],8),%k[x]        \n"
      "movslq      %k[x],%q[temp]                \n"
      "sar         $16,%q[temp]                  \n"
      "sub         %q[base_ix],%q[temp]          \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k2                 \n"
      "vmovdqu16   (%[src_ptr],%q[base_ix],2),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr],%q[base_ix],2),%%zmm1%{%%k2%}%{z%} \n"

      "vpaddd      %%zmm16,%%zmm2,%%zmm3         \n"
      "vpaddd      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrld      $16,%%zmm3,%%zmm3             \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpsubd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpcmpq      $6,%%zmm21,%%zmm2,%%k4        \n"

      "vpsllq      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpsllq      $32,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpsllq      $16,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpsllq      $32,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm18,%%zmm3,%%zmm4         \n"
      "vpaddw      %%zmm19,%%zmm3,%%zmm5         \n"
      "vpcmpuw     $6,%%zmm19,%%zmm2,%%k3        \n"

      "vpermi2w    %%zmm1,%%zmm0,%%zmm4          \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm5%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm4,%%zmm0             \n"
      "vpandd      %%zmm20,%%zmm4,%%zmm4         \n"
      "vpsrld      $16,%%zmm5,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm5,%%zmm5         \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"
      "vpaddd      %%zmm1,%%zmm5,%%zmm5          \n"
      "vpaddd      %%zmm5,%%zmm4,%%zmm4          \n"
      "vpsrlq      $32,%%zmm4,%%zmm0             \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"

      "vpblendmq   %%zmm23,%%zmm22,%%zmm2%{%%k4%}\n"
      "vpmulld     %%zmm2,%%zmm4,%%zmm4          \n"
      "vpsrld      $16,%%zmm4,%%zmm4             \n"
      "vpmovqb     %%zmm4,(%[dst_ptr])           \n"
      "lea         0x8(%[dst_ptr]),%[dst_ptr]    \n"
      "sub         $0x8,%[dst_width]             \n"
      "jge         21b                           \n"

      "22:         \n"
      "add         $0x8,%[dst_width]             \n"
      "jle         99f                           \n"
      "movslq      %k[x],%q[base_ix]             \n"
      "movzwl      %w[x],%k[temp]                \n"
      "vpbroadcastq %q[temp],%%zmm2              \n"
      "sar         $16,%q[base_ix]               \n"
      "mov         %k[dst_width],%k[temp]        \n"
      "imul        %k[dx],%k[temp]               \n"
      "add         %k[x],%k[temp]                \n"
      "sar         $16,%k[temp]                  \n"
      "movslq      %k[temp],%q[temp]             \n"
      "sub         %q[base_ix],%q[temp]          \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "shr         $32,%q[mask]                  \n"
      "kmovd       %k[mask],%%k2                 \n"
      "vmovdqu16   (%[src_ptr],%q[base_ix],2),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu16   0x40(%[src_ptr],%q[base_ix],2),%%zmm1%{%%k2%}%{z%} \n"

      "vpaddd      %%zmm16,%%zmm2,%%zmm3         \n"
      "vpaddd      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrld      $16,%%zmm3,%%zmm3             \n"
      "vpsrld      $16,%%zmm2,%%zmm2             \n"
      "vpsubd      %%zmm3,%%zmm2,%%zmm2          \n"
      "vpcmpq      $6,%%zmm21,%%zmm2,%%k4        \n"

      "vpsllq      $16,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpsllq      $32,%%zmm3,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm3,%%zmm3          \n"
      "vpsllq      $16,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpsllq      $32,%%zmm2,%%zmm4             \n"
      "vpord       %%zmm4,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm18,%%zmm3,%%zmm4         \n"
      "vpaddw      %%zmm19,%%zmm3,%%zmm5         \n"
      "vpcmpuw     $6,%%zmm19,%%zmm2,%%k3        \n"

      "vpermi2w    %%zmm1,%%zmm0,%%zmm4          \n"
      "vpermi2w    %%zmm1,%%zmm0,%%zmm5%{%%k3%}%{z%}    \n"
      "vpsrld      $16,%%zmm4,%%zmm0             \n"
      "vpandd      %%zmm20,%%zmm4,%%zmm4         \n"
      "vpsrld      $16,%%zmm5,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm5,%%zmm5         \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"
      "vpaddd      %%zmm1,%%zmm5,%%zmm5          \n"
      "vpaddd      %%zmm5,%%zmm4,%%zmm4          \n"
      "vpsrlq      $32,%%zmm4,%%zmm0             \n"
      "vpaddd      %%zmm0,%%zmm4,%%zmm4          \n"

      "vpblendmq   %%zmm23,%%zmm22,%%zmm2%{%%k4%}\n"
      "vpmulld     %%zmm2,%%zmm4,%%zmm4          \n"
      "vpsrld      $16,%%zmm4,%%zmm4             \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[dst_width],%q[mask],%q[mask]\n"
      "kmovw       %k[mask],%%k1                 \n"
      "vpmovqb     %%zmm4,(%[dst_ptr])%{%%k1%}   \n"
      "jmp         99f                           \n"

      // Path 3: minboxwidth >= 8 or minboxwidth < 1.
      LABELALIGN
      "30:         \n"
      "movslq      %k[x],%q[base_ix]             \n"
      "sar         $16,%q[base_ix]               \n"
      "add         %k[dx],%k[x]                  \n"
      "movslq      %k[x],%q[temp]                \n"
      "sar         $16,%q[temp]                  \n"
      "sub         %q[base_ix],%q[temp]          \n"
      "cmp         $1,%q[temp]                   \n"
      "jge         31f                           \n"
      "mov         $1,%q[temp]                   \n"
      "31:         \n"
      "vmovdqa32   %%xmm22,%%xmm3                \n"
      "cmp         %k[temp],%[minbw]             \n"
      "jge         32f                           \n"
      "vmovdqa32   %%xmm23,%%xmm3                \n"
      "32:         \n"
      "vpxord      %%zmm2,%%zmm2,%%zmm2          \n"
      "lea         (%[src_ptr],%q[base_ix],2),%q[base_ix] \n"
      "sub         $0x20,%q[temp]                \n"
      "jl          34f                           \n"
      "33:         \n"
      "vmovdqu16   (%q[base_ix]),%%zmm0          \n"
      "lea         0x40(%q[base_ix]),%q[base_ix] \n"
      "vpsrld      $16,%%zmm0,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm0,%%zmm0         \n"
      "vpaddd      %%zmm1,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "sub         $0x20,%q[temp]                \n"
      "jge         33b                           \n"
      "34:         \n"
      "add         $0x20,%q[temp]                \n"
      "jle         35f                           \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovd       %k[mask],%%k1                 \n"
      "vmovdqu16   (%q[base_ix]),%%zmm0%{%%k1%}%{z%} \n"
      "vpsrld      $16,%%zmm0,%%zmm1             \n"
      "vpandd      %%zmm20,%%zmm0,%%zmm0         \n"
      "vpaddd      %%zmm1,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm0,%%zmm2,%%zmm2          \n"
      "35:         \n"
      "vextracti32x8 $1,%%zmm2,%%ymm0            \n"
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"
      "vextracti128 $1,%%ymm2,%%xmm0             \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpshufd     $0xee,%%xmm2,%%xmm0           \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpshufd     $0x55,%%xmm2,%%xmm0           \n"
      "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
      "vpmulld     %%xmm3,%%xmm2,%%xmm2          \n"
      "vpsrld      $16,%%xmm2,%%xmm2             \n"
      "vmovd       %%xmm2,%k[temp]               \n"
      "mov         %b[temp],(%[dst_ptr])         \n"
      "inc         %[dst_ptr]                    \n"
      "dec         %[dst_width]                  \n"
      "jg          30b                           \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_ptr] "+r"(src_ptr),      // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),      // %[dst_ptr]
        [dst_width] "+r"(dst_width),  // %[dst_width]
        [x] "+r"(x),                  // %[x]
        [base_ix] "=&r"(base_ix),     // %[base_ix]
        [temp] "=&r"(temp),           // %[temp]
        [mask] "=&r"(mask)            // %[mask]
      : [dx] "r"(dx),                 // %[dx]
        [minbw] "m"(minboxwidth),     // %[minbw]
        [scale0] "m"(scaletbl[0]),    // %[scale0]
        [scale1] "m"(scaletbl[1]),    // %[scale1]
        [kSeq] "m"(kSequence0_15)     // %[kSeq]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23", "k1",
        "k2", "k3", "k4");
}
#endif  // HAS_SCALEADDCOLS2_AVX512BW

#ifdef HAS_SCALEFILTERCOLS_AVX2
static const int32_t kStep0_7_Filter[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const uvec8 kShufFracCols = {0,   0,   4,   4,   8,   8,   12,  12,
                                    128, 128, 128, 128, 128, 128, 128, 128};

// Bilinear column filtering. AVX2 version.
void ScaleFilterCols_AVX2(uint8_t* dst_ptr,
                          const uint8_t* src_ptr,
                          int dst_width,
                          int x,
                          int dx) {
  int n = dst_width & ~7;
  int r = dst_width & 7;
  if (n > 0) {
    SIMD_ALIGNED(int32_t xi_buf[8]);
    int32_t* xi_ptr = xi_buf;
    uintptr_t tmp;
    int loop_n = n;
    asm volatile(
        "vpbroadcastd %[x],%%ymm2                  \n"
        "vpbroadcastd %[dx],%%ymm3                 \n"
        "vpmulld     %[kStep],%%ymm3,%%ymm0        \n"
        "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"
        "vpslld      $0x3,%%ymm3,%%ymm3            \n"
        "vbroadcasti128 %[kShufFrac],%%ymm5        \n"
        "vpcmpeqb    %%xmm6,%%xmm6,%%xmm6          \n"
        "vpsrlw      $0x8,%%xmm6,%%xmm7            \n"  // 0x00ff
        "vpsrlw      $0x9,%%xmm6,%%xmm6            \n"  // 0x007f

        LABELALIGN
        "1:          \n"
        "vpsrad      $0x10,%%ymm2,%%ymm0           \n"
        "vmovdqu     %%ymm0,(%[xi])                \n"
        "vpsrlw      $0x9,%%ymm2,%%ymm1            \n"
        "vpaddd      %%ymm3,%%ymm2,%%ymm2          \n"
        "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
        "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
        "vpxor       %%xmm6,%%xmm1,%%xmm1          \n"
#if defined(__x86_64__)
        "movslq      0x00(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x0,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x04(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x1,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x08(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x2,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x0c(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x3,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x10(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x4,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x14(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x5,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x18(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x6,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
        "movslq      0x1c(%[xi]),%q[tmp]           \n"
        "vpinsrw     $0x7,0x00(%[src],%q[tmp],1),%%xmm0,%%xmm0 \n"
#else
        "mov         0x00(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x0,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x04(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x1,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x08(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x2,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x0c(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x3,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x10(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x4,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x14(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x5,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x18(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x6,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
        "mov         0x1c(%[xi]),%[tmp]            \n"
        "vpinsrw     $0x7,0x00(%[src],%[tmp],1),%%xmm0,%%xmm0 \n"
#endif
        "vpand       %%xmm7,%%xmm0,%%xmm4          \n"
        "vpmaddubsw  %%xmm1,%%xmm0,%%xmm0          \n"
        "vpaddw      %%xmm4,%%xmm0,%%xmm0          \n"
        "vpsubw      %%xmm6,%%xmm7,%%xmm4          \n"
        "vpsrlw      $0x1,%%xmm4,%%xmm4            \n"  // 0x0040
        "vpaddw      %%xmm4,%%xmm0,%%xmm0          \n"
        "vpsrlw      $0x7,%%xmm0,%%xmm0            \n"
        "vpackuswb   %%xmm0,%%xmm0,%%xmm0          \n"
        "vmovq       %%xmm0,(%[dst])               \n"
        "lea         0x8(%[dst]),%[dst]            \n"
        "subl        $0x8,%[n]                     \n"
        "jg          1b                            \n"
        "vzeroupper  \n"
        : [dst] "+r"(dst_ptr), [n] "+rm"(loop_n), [tmp] "=&r"(tmp)
        : [src] "r"(src_ptr), [xi] "r"(xi_ptr), [x] "m"(x), [dx] "m"(dx),
          [kStep] "m"(kStep0_7_Filter), [kShufFrac] "m"(kShufFracCols)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7");
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
// Reads 16 pixels, duplicates them and writes 32 pixels (plus 16/8 tails).
void ScaleColsUp2_AVX2(uint8_t* dst_ptr,
                       const uint8_t* src_ptr,
                       int dst_width,
                       int x,
                       int dx) {
  (void)x;
  (void)dx;
  asm volatile(
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%1),%%xmm0                   \n"
      "lea         0x10(%1),%1                   \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpunpcklbw  %%ymm0,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%0)                   \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "test        $0x10,%2                      \n"
      "je          3f                            \n"
      "vmovq       (%1),%%xmm0                   \n"
      "lea         0x8(%1),%1                    \n"
      "vpunpcklbw  %%xmm0,%%xmm0,%%xmm0          \n"
      "vmovdqu     %%xmm0,(%0)                   \n"
      "lea         0x10(%0),%0                   \n"

      "3:          \n"
      "test        $0x8,%2                       \n"
      "je          99f                           \n"
      "vmovd       (%1),%%xmm0                   \n"
      "vpunpcklbw  %%xmm0,%%xmm0,%%xmm0          \n"
      "vmovq       %%xmm0,(%0)                   \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(dst_ptr),   // %0
        "+r"(src_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0");
}
#endif  // HAS_SCALECOLSUP2_AVX2

#ifdef HAS_SCALEARGBROWDOWN2_AVX2
void ScaleARGBRowDown2_AVX2(const uint8_t* src_argb,
                            ptrdiff_t src_stride,
                            uint8_t* dst_argb,
                            int dst_width) {
  (void)src_stride;
  asm volatile(
      LABELALIGN
      "1:          \n"
      "vmovups     (%0),%%ymm0                   \n"
      "vmovups     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vshufps     $0xdd,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovups     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

void ScaleARGBRowDown2Linear_AVX2(const uint8_t* src_argb,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_argb,
                                  int dst_width) {
  (void)src_stride;
  asm volatile(
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vshufps     $0x88,%%ymm1,%%ymm0,%%ymm2    \n"
      "vshufps     $0xdd,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpavgb      %%ymm0,%%ymm2,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}

void ScaleARGBRowDown2Box_AVX2(const uint8_t* src_argb,
                               ptrdiff_t src_stride,
                               uint8_t* dst_argb,
                               int dst_width) {
  asm volatile(
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x00(%0,%3,1),%%ymm2          \n"
      "vmovdqu     0x20(%0,%3,1),%%ymm3          \n"
      "lea         0x40(%0),%0                   \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpavgb      %%ymm3,%%ymm1,%%ymm1          \n"
      "vshufps     $0x88,%%ymm1,%%ymm0,%%ymm2    \n"
      "vshufps     $0xdd,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpavgb      %%ymm0,%%ymm2,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(dst_width)  // %2
      : "r"(src_stride)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_SCALEARGBROWDOWN2_AVX2

#ifdef HAS_SCALEARGBROWDOWNEVEN_AVX2
static const int32_t kStep0_7_Even[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// Reads 8 pixels at a time.
void ScaleARGBRowDownEven_AVX2(const uint8_t* src_argb,
                               ptrdiff_t src_stride,
                               int src_stepx,
                               uint8_t* dst_argb,
                               int dst_width) {
  ptrdiff_t src_stepx_x32 = (ptrdiff_t)src_stepx * 32;
  (void)src_stride;
  asm volatile(
      "vpbroadcastd %[stepx],%%ymm2              \n"
      "vpmulld     %[kStep],%%ymm2,%%ymm2        \n"

      LABELALIGN
      "1:          \n"
      "vpcmpeqd    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpgatherdd  %%ymm3,(%[src],%%ymm2,4),%%ymm0 \n"
      "add         %[step32],%[src]              \n"
      "vmovdqu     %%ymm0,(%[dst])               \n"
      "lea         0x20(%[dst]),%[dst]           \n"
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src] "+r"(src_argb), [dst] "+r"(dst_argb), [width] "+rm"(dst_width)
      : [step32] "r"(src_stepx_x32), [stepx] "m"(src_stepx),
        [kStep] "m"(kStep0_7_Even)
      : "memory", "cc", "xmm0", "xmm2", "xmm3");
}

// Blends eight 2x2 to 8x1.
void ScaleARGBRowDownEvenBox_AVX2(const uint8_t* src_argb,
                                  ptrdiff_t src_stride,
                                  int src_stepx,
                                  uint8_t* dst_argb,
                                  int dst_width) {
  const uint8_t* row1 = src_argb + src_stride;
  ptrdiff_t step4_bytes = (ptrdiff_t)src_stepx * 16;
  asm volatile(
      "vmovd       %[stepx],%%xmm4               \n"
      "vpshufd     $0x0,%%xmm4,%%xmm4            \n"
      "vpmulld     %[kStep],%%xmm4,%%xmm4        \n"

      LABELALIGN
      "1:          \n"
      "vpcmpeqd    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpgatherdq  %%ymm5,(%[row0],%%xmm4,4),%%ymm0 \n"
      "vpcmpeqd    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpgatherdq  %%ymm5,(%[row1],%%xmm4,4),%%ymm2 \n"
      "add         %[step4],%[row0]              \n"
      "add         %[step4],%[row1]              \n"
      "vpcmpeqd    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpgatherdq  %%ymm5,(%[row0],%%xmm4,4),%%ymm1 \n"
      "vpcmpeqd    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpgatherdq  %%ymm5,(%[row1],%%xmm4,4),%%ymm3 \n"
      "add         %[step4],%[row0]              \n"
      "add         %[step4],%[row1]              \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpavgb      %%ymm3,%%ymm1,%%ymm1          \n"
      "vshufps     $0x88,%%ymm1,%%ymm0,%%ymm2    \n"
      "vshufps     $0xdd,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpavgb      %%ymm0,%%ymm2,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%[dst])               \n"
      "lea         0x20(%[dst]),%[dst]           \n"
      "subl        $0x8,%[width]                 \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [row0] "+r"(src_argb), [row1] "+r"(row1), [dst] "+r"(dst_argb),
        [width] "+rm"(dst_width)
      : [step4] "r"(step4_bytes), [stepx] "m"(src_stepx),
        [kStep] "m"(kStep0_7_Even)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_SCALEARGBROWDOWNEVEN_AVX2

#ifdef HAS_SCALEARGBCOLS_AVX2
static const int32_t kStep0_7_Cols[8] = {0, 1, 2, 3, 4, 5, 6, 7};

void ScaleARGBCols_AVX2(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  int n = dst_width & ~7;
  int r = dst_width & 7;
  if (n > 0) {
    int loop_n = n;
    asm volatile(
        "vpbroadcastd %[x],%%ymm2                  \n"
        "vpbroadcastd %[dx],%%ymm3                 \n"
        "vpmulld     %[kStep],%%ymm3,%%ymm0        \n"
        "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"
        "vpslld      $0x3,%%ymm3,%%ymm3            \n"

        LABELALIGN
        "1:          \n"
        "vpsrad      $0x10,%%ymm2,%%ymm0           \n"
        "vpaddd      %%ymm3,%%ymm2,%%ymm2          \n"
        "vpcmpeqd    %%ymm4,%%ymm4,%%ymm4          \n"
        "vpgatherdd  %%ymm4,(%[src],%%ymm0,4),%%ymm1 \n"
        "vmovdqu     %%ymm1,(%[dst])               \n"
        "lea         0x20(%[dst]),%[dst]           \n"
        "subl        $0x8,%[n]                     \n"
        "jg          1b                            \n"
        "vzeroupper  \n"
        : [dst] "+r"(dst_argb), [n] "+rm"(loop_n)
        : [src] "r"(src_argb), [x] "m"(x), [dx] "m"(dx),
          [kStep] "m"(kStep0_7_Cols)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
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
// Reads 4 pixels, duplicates them and writes 8 pixels.
void ScaleARGBColsUp2_AVX2(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx) {
  (void)x;
  (void)dx;
  asm volatile(
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%1),%%xmm0                   \n"
      "lea         0x10(%1),%1                   \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpunpckldq  %%ymm0,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%0)                   \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(dst_argb),  // %0
        "+r"(src_argb),  // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0");
}
#endif  // HAS_SCALEARGBCOLSUP2_AVX2

#ifdef HAS_SCALEARGBFILTERCOLS_AVX2
static const int32_t kStep0_3_ARGB[4] = {0, 1, 2, 3};

// Shuffle table for arranging 2 pixels into pairs for pmaddubsw
static const uvec8 kShuffleColARGB = {
    0u, 4u,  1u, 5u,  2u,  6u,  3u,  7u,  // bbggrraa 1st pixel
    8u, 12u, 9u, 13u, 10u, 14u, 11u, 15u  // bbggrraa 2nd pixel
};

// Shuffle table for duplicating 4 fractions across 2x128-bit lanes
static const uint8_t kShuffleFractionsAVX2[32] = {
    0u, 0u, 0u, 0u, 0u,  0u,  0u,  0u,  4u,  4u,  4u,  4u,  4u,  4u,  4u,  4u,
    8u, 8u, 8u, 8u, 8u,  8u,  8u,  8u,  12u, 12u, 12u, 12u, 12u, 12u, 12u, 12u,
};

// Bilinear row filtering combines 4x2 -> 4x1. AVX2 version.
void ScaleARGBFilterCols_AVX2(uint8_t* dst_argb,
                              const uint8_t* src_argb,
                              int dst_width,
                              int x,
                              int dx) {
  int n = dst_width & ~3;
  int r = dst_width & 3;
  if (n > 0) {
    int loop_n = n;
    asm volatile(
        "vbroadcasti128 %[kShufARGB],%%ymm4        \n"
        "vmovdqu     %[kShufFrac],%%ymm5           \n"
        "vmovd       %[x],%%xmm2                   \n"
        "vpshufd     $0x0,%%xmm2,%%xmm2            \n"
        "vmovd       %[dx],%%xmm3                  \n"
        "vpshufd     $0x0,%%xmm3,%%xmm3            \n"
        "vpmulld     %[kStep],%%xmm3,%%xmm0        \n"
        "vpaddd      %%xmm0,%%xmm2,%%xmm2          \n"
        "vpslld      $0x2,%%xmm3,%%xmm3            \n"
        "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"
        "vpsrlw      $0x9,%%ymm6,%%ymm6            \n"

        LABELALIGN
        "1:          \n"
        "vpsrad      $0x10,%%xmm2,%%xmm1           \n"
        "vpcmpeqd    %%ymm7,%%ymm7,%%ymm7          \n"
        "vpgatherdq  %%ymm7,(%[src],%%xmm1,4),%%ymm0 \n"
        "vpsrlw      $0x9,%%xmm2,%%xmm1            \n"
        "vpaddd      %%xmm3,%%xmm2,%%xmm2          \n"
        "vinserti128 $0x1,%%xmm1,%%ymm1,%%ymm1     \n"
        "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
        "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
        "vpxor       %%ymm6,%%ymm1,%%ymm1          \n"
        "vpmaddubsw  %%ymm1,%%ymm0,%%ymm0          \n"
        "vpsrlw      $0x7,%%ymm0,%%ymm0            \n"
        "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
        "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
        "vmovdqu     %%xmm0,(%[dst])               \n"
        "lea         0x10(%[dst]),%[dst]           \n"
        "subl        $0x4,%[n]                     \n"
        "jg          1b                            \n"
        "vzeroupper  \n"
        : [dst] "+r"(dst_argb), [n] "+rm"(loop_n)
        : [src] "r"(src_argb), [x] "m"(x), [dx] "m"(dx),
          [kStep] "m"(kStep0_3_ARGB), [kShufARGB] "m"(kShuffleColARGB),
          [kShufFrac] "m"(kShuffleFractionsAVX2)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm6", "xmm7");
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

// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_X86(int num, int div) {
  asm volatile(
      "cdq         \n"
      "shld        $0x10,%%eax,%%edx             \n"
      "shl         $0x10,%%eax                   \n"
      "idiv        %1                            \n"
      "mov         %0, %%eax                     \n"
      : "+a"(num)  // %0
      : "c"(div)   // %1
      : "memory", "cc", "edx");
  return num;
}

// Divide num - 1 by div - 1 and return as 16.16 fixed point result.
int FixedDiv1_X86(int num, int div) {
  asm volatile(
      "cdq         \n"
      "shld        $0x10,%%eax,%%edx             \n"
      "shl         $0x10,%%eax                   \n"
      "sub         $0x10001,%%eax                \n"
      "sbb         $0x0,%%edx                    \n"
      "sub         $0x1,%1                       \n"
      "idiv        %1                            \n"
      "mov         %0, %%eax                     \n"
      : "+a"(num),  // %0
        "+c"(div)   // %1
      :
      : "memory", "cc", "edx");
  return num;
}

#ifdef HAS_SCALEUVROWDOWN2_AVX2
void ScaleUVRowDown2_AVX2(const uint8_t* src_uv,
                          ptrdiff_t src_stride,
                          uint8_t* dst_uv,
                          int dst_width) {
  (void)src_stride;
  asm volatile(
      "sub         $0x10,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm0           \n"
      "vpsrld      $0x10,%%ymm1,%%ymm1           \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x10,%2                      \n"
      "jle         99f                           \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm0           \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_uv),    // %0
        "+r"(dst_uv),    // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_SCALEUVROWDOWN2_AVX2

#ifdef HAS_SCALEUVROWDOWN2LINEAR_AVX2
void ScaleUVRowDown2Linear_AVX2(const uint8_t* src_uv,
                                ptrdiff_t src_stride,
                                uint8_t* dst_uv,
                                int dst_width) {
  (void)src_stride;
  asm volatile(
      "vpcmpeqd    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $0x10,%%ymm4,%%ymm4           \n"
      "sub         $0x10,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm2           \n"
      "vpsrld      $0x10,%%ymm1,%%ymm3           \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpavgb      %%ymm3,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm4,%%ymm1,%%ymm1          \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x10,%2                      \n"
      "jle         99f                           \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsrld      $0x10,%%ymm0,%%ymm2           \n"
      "vpavgb      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_uv),    // %0
        "+r"(dst_uv),    // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_SCALEUVROWDOWN2LINEAR_AVX2

#ifdef HAS_SCALEUVROWDOWN2BOX_AVX2
// Shuffle table for splitting UV into upper and lower part of register.
static const uvec8 kShuffleSplitUV = {0u, 2u, 4u, 6u, 8u, 10u, 12u, 14u,
                                      1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u};
static const uvec8 kShuffleMergeUV = {0u,   8u,   2u,   10u,  4u,   12u,
                                      6u,   14u,  0x80, 0x80, 0x80, 0x80,
                                      0x80, 0x80, 0x80, 0x80};

void ScaleUVRowDown2Box_AVX2(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst_ptr,
                             int dst_width) {
  asm volatile(
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"  // 01010101
      "vpabsb      %%ymm4,%%ymm4                 \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"  // zero
      "vbroadcastf128 %4,%%ymm1                  \n"  // split shuffler
      "vbroadcastf128 %5,%%ymm3                  \n"  // merge shuffler

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // 16 UV row 0
      "vmovdqu     0x00(%0,%3,1),%%ymm2          \n"  // 16 UV row 1
      "lea         0x20(%0),%0                   \n"
      "vpshufb     %%ymm1,%%ymm0,%%ymm0          \n"  // uuuuvvvv
      "vpshufb     %%ymm1,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"  // horizontal add
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"  // vertical add
      "vpsrlw      $0x1,%%ymm0,%%ymm0            \n"  // round
      "vpavgw      %%ymm5,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm3,%%ymm0,%%ymm0          \n"  // merge uv
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"  // combine qwords
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"  // 8 UV
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),         // %0
        "+r"(dst_ptr),         // %1
        "+r"(dst_width)        // %2
      : "r"(src_stride),       // %3
        "m"(kShuffleSplitUV),  // %4
        "m"(kShuffleMergeUV)   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_SCALEUVROWDOWN2BOX_AVX2

#ifdef HAS_SCALEUVROWDOWNEVEN_AVX2
void ScaleUVRowDownEven_AVX2(const uint8_t* src_uv,
                             ptrdiff_t src_stride,
                             int src_stepx,
                             uint8_t* dst_uv,
                             int dst_width) {
  ptrdiff_t step = (ptrdiff_t)src_stepx * 2;
  ptrdiff_t step3 = step * 3;
  (void)src_stride;
  asm volatile(
      "vpxor       %%xmm0,%%xmm0,%%xmm0          \n"

      LABELALIGN
      "1:          \n"
      "vpinsrw     $0x0,(%[src]),%%xmm0,%%xmm0   \n"
      "vpinsrw     $0x1,0x00(%[src],%[step],1),%%xmm0,%%xmm0 \n"
      "vpinsrw     $0x2,0x00(%[src],%[step],2),%%xmm0,%%xmm0 \n"
      "vpinsrw     $0x3,0x00(%[src],%[step3],1),%%xmm0,%%xmm0 \n"
      "lea         0x00(%[src],%[step],4),%[src] \n"
      "vpinsrw     $0x4,(%[src]),%%xmm0,%%xmm0   \n"
      "vpinsrw     $0x5,0x00(%[src],%[step],1),%%xmm0,%%xmm0 \n"
      "vpinsrw     $0x6,0x00(%[src],%[step],2),%%xmm0,%%xmm0 \n"
      "vpinsrw     $0x7,0x00(%[src],%[step3],1),%%xmm0,%%xmm0 \n"
      "lea         0x00(%[src],%[step],4),%[src] \n"
      "vmovdqu     %%xmm0,(%[dst])               \n"
      "lea         0x10(%[dst]),%[dst]           \n"
      "subl        $0x8,%[width]                 \n"
      "jg          1b                            \n"
      : [src] "+r"(src_uv), [dst] "+r"(dst_uv), [width] "+rm"(dst_width)
      : [step] "r"(step), [step3] "r"(step3)
      : "memory", "cc", "xmm0");
}
#endif  // HAS_SCALEUVROWDOWNEVEN_AVX2

static const uvec8 kUVLinearMadd31 = {3, 1, 3, 1, 1, 3, 1, 3,
                                      3, 1, 3, 1, 1, 3, 1, 3};

#ifdef HAS_SCALEUVROWUP2_LINEAR_SSSE3
void ScaleUVRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                                uint8_t* dst_ptr,
                                int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $15,%%xmm4                    \n"
      "psllw       $1,%%xmm4                     \n"  // all 2
      "movdqa      %3,%%xmm3                     \n"

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 00112233 (1u1v)
      "movq        2(%0),%%xmm1                  \n"  // 11223344 (1u1v)
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // 0101121223233434 (2u2v)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm0,%%xmm2                 \n"  // 2323232334343434 (2u2v)
      "punpckldq   %%xmm0,%%xmm0                 \n"  // 0101010112121212 (2u2v)
      "pmaddubsw   %%xmm3,%%xmm2                 \n"  // 3*near+far (1u1v16, hi)
      "pmaddubsw   %%xmm3,%%xmm0                 \n"  // 3*near+far (1u1v16, lo)
      "paddw       %%xmm4,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddw       %%xmm4,%%xmm2                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrlw       $2,%%xmm2                     \n"  // 3/4*near+1/4*far (hi)
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),        // %0
        "+r"(dst_ptr),        // %1
        "+r"(dst_width)       // %2
      : "m"(kUVLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif

#ifdef HAS_SCALEUVROWUP2_BILINEAR_SSSE3
void ScaleUVRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $3,%%xmm6                     \n"  // all 8
      "movdqa      %5,%%xmm7                     \n"

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 00112233 (1u1v)
      "movq        2(%0),%%xmm1                  \n"  // 11223344 (1u1v)
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // 0101121223233434 (2u2v)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm0,%%xmm2                 \n"  // 2323232334343434 (2u2v)
      "punpckldq   %%xmm0,%%xmm0                 \n"  // 0101010112121212 (2u2v)
      "pmaddubsw   %%xmm7,%%xmm2                 \n"  // 3*near+far (1u1v16, hi)
      "pmaddubsw   %%xmm7,%%xmm0                 \n"  // 3*near+far (1u1v16, lo)

      "movq        (%0,%3),%%xmm1                \n"
      "movq        2(%0,%3),%%xmm4               \n"
      "punpcklbw   %%xmm4,%%xmm1                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpckhdq   %%xmm1,%%xmm3                 \n"
      "punpckldq   %%xmm1,%%xmm1                 \n"
      "pmaddubsw   %%xmm7,%%xmm3                 \n"  // 3*near+far (2, hi)
      "pmaddubsw   %%xmm7,%%xmm1                 \n"  // 3*near+far (2, lo)

      // xmm0 xmm2
      // xmm1 xmm3

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm1,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm1,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm2,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm2,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm1,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm1                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm2,%%xmm1                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrlw       $4,%%xmm1                     \n"  // ^ div by 16 (2, hi)

      "packuswb    %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),        // %0
        "+r"(dst_ptr),        // %1
        "+r"(dst_width)       // %2
      : "r"(src_stride),      // %3
        "r"(dst_stride),      // %4
        "m"(kUVLinearMadd31)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEUVROWUP2_LINEAR_AVX2

void ScaleUVRowUp2_Linear_AVX2(const uint8_t* src_ptr,
                               uint8_t* dst_ptr,
                               int dst_width) {
  asm volatile(
      "vpcmpeqw    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrlw      $15,%%ymm4,%%ymm4             \n"
      "vpsllw      $1,%%ymm4,%%ymm4              \n"  // all 2
      "vbroadcastf128 %3,%%ymm3                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"
      "vmovdqu     2(%0),%%xmm1                  \n"
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"
      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpunpckhdq  %%ymm0,%%ymm0,%%ymm2          \n"
      "vpunpckldq  %%ymm0,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm3,%%ymm2,%%ymm1          \n"  // 3*near+far (hi)
      "vpmaddubsw  %%ymm3,%%ymm0,%%ymm0          \n"  // 3*near+far (lo)
      "vpaddw      %%ymm4,%%ymm0,%%ymm0          \n"  // 3*near+far+2 (lo)
      "vpaddw      %%ymm4,%%ymm1,%%ymm1          \n"  // 3*near+far+2 (hi)
      "vpsrlw      $2,%%ymm0,%%ymm0              \n"  // 3/4*near+1/4*far (lo)
      "vpsrlw      $2,%%ymm1,%%ymm1              \n"  // 3/4*near+1/4*far (hi)
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 uv to 16 uv
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),        // %0
        "+r"(dst_ptr),        // %1
        "+r"(dst_width)       // %2
      : "m"(kUVLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif

#ifdef HAS_SCALEUVROWUP2_BILINEAR_AVX2
void ScaleUVRowUp2_Bilinear_AVX2(const uint8_t* src_ptr,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_ptr,
                                 ptrdiff_t dst_stride,
                                 int dst_width) {
  asm volatile(
      "vpcmpeqw    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrlw      $15,%%ymm6,%%ymm6             \n"
      "vpsllw      $3,%%ymm6,%%ymm6              \n"  // all 8
      "vbroadcastf128 %5,%%ymm7                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"
      "vmovdqu     2(%0),%%xmm1                  \n"
      "vpermq      $0b11011000,%%ymm0,%%ymm0     \n"
      "vpermq      $0b11011000,%%ymm1,%%ymm1     \n"
      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpunpckhdq  %%ymm0,%%ymm0,%%ymm2          \n"
      "vpunpckldq  %%ymm0,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm1          \n"  // 3*near+far (1, hi)
      "vpmaddubsw  %%ymm7,%%ymm0,%%ymm0          \n"  // 3*near+far (1, lo)

      "vmovdqu     (%0,%3),%%xmm2                \n"  // 0123456789ABCDEF
      "vmovdqu     2(%0,%3),%%xmm3               \n"  // 123456789ABCDEF0
      "vpermq      $0b11011000,%%ymm2,%%ymm2     \n"
      "vpermq      $0b11011000,%%ymm3,%%ymm3     \n"
      "vpunpcklbw  %%ymm3,%%ymm2,%%ymm2          \n"
      "vpunpckhdq  %%ymm2,%%ymm2,%%ymm4          \n"
      "vpunpckldq  %%ymm2,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm7,%%ymm4,%%ymm3          \n"  // 3*near+far (2, hi)
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm2          \n"  // 3*near+far (2, lo)

      // ymm0 ymm1
      // ymm2 ymm3

      "vpaddw      %%ymm0,%%ymm0,%%ymm4          \n"  // 6*near+2*far (1, lo)
      "vpaddw      %%ymm6,%%ymm2,%%ymm5          \n"  // 3*near+far+8 (2, lo)
      "vpaddw      %%ymm4,%%ymm0,%%ymm4          \n"  // 9*near+3*far (1, lo)
      "vpaddw      %%ymm4,%%ymm5,%%ymm4          \n"  // 9 3 3 1 + 8 (1, lo)
      "vpsrlw      $4,%%ymm4,%%ymm4              \n"  // ^ div by 16 (1, lo)

      "vpaddw      %%ymm2,%%ymm2,%%ymm5          \n"  // 6*near+2*far (2, lo)
      "vpaddw      %%ymm6,%%ymm0,%%ymm0          \n"  // 3*near+far+8 (1, lo)
      "vpaddw      %%ymm5,%%ymm2,%%ymm5          \n"  // 9*near+3*far (2, lo)
      "vpaddw      %%ymm5,%%ymm0,%%ymm5          \n"  // 9 3 3 1 + 8 (2, lo)
      "vpsrlw      $4,%%ymm5,%%ymm5              \n"  // ^ div by 16 (2, lo)

      "vpaddw      %%ymm1,%%ymm1,%%ymm0          \n"  // 6*near+2*far (1, hi)
      "vpaddw      %%ymm6,%%ymm3,%%ymm2          \n"  // 3*near+far+8 (2, hi)
      "vpaddw      %%ymm0,%%ymm1,%%ymm0          \n"  // 9*near+3*far (1, hi)
      "vpaddw      %%ymm0,%%ymm2,%%ymm0          \n"  // 9 3 3 1 + 8 (1, hi)
      "vpsrlw      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16 (1, hi)

      "vpaddw      %%ymm3,%%ymm3,%%ymm2          \n"  // 6*near+2*far (2, hi)
      "vpaddw      %%ymm6,%%ymm1,%%ymm1          \n"  // 3*near+far+8 (1, hi)
      "vpaddw      %%ymm2,%%ymm3,%%ymm2          \n"  // 9*near+3*far (2, hi)
      "vpaddw      %%ymm2,%%ymm1,%%ymm2          \n"  // 9 3 3 1 + 8 (2, hi)
      "vpsrlw      $4,%%ymm2,%%ymm2              \n"  // ^ div by 16 (2, hi)

      "vpackuswb   %%ymm0,%%ymm4,%%ymm4          \n"
      "vmovdqu     %%ymm4,(%1)                   \n"  // store above
      "vpackuswb   %%ymm2,%%ymm5,%%ymm5          \n"
      "vmovdqu     %%ymm5,(%1,%4)                \n"  // store below

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 8 uv to 16 uv
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),        // %0
        "+r"(dst_ptr),        // %1
        "+r"(dst_width)       // %2
      : "r"(src_stride),      // %3
        "r"(dst_stride),      // %4
        "m"(kUVLinearMadd31)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEUVROWUP2_LINEAR_16_SSE41
void ScaleUVRowUp2_Linear_16_SSE41(const uint16_t* src_ptr,
                                   uint16_t* dst_ptr,
                                   int dst_width) {
  asm volatile(
      "pxor        %%xmm5,%%xmm5                 \n"
      "pcmpeqd     %%xmm4,%%xmm4                 \n"
      "psrld       $31,%%xmm4                    \n"
      "pslld       $1,%%xmm4                     \n"  // all 2

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 0011 (16b, 1u1v)
      "movq        4(%0),%%xmm1                  \n"  // 1122 (16b, 1u1v)

      "punpcklwd   %%xmm5,%%xmm0                 \n"  // 0011 (32b, 1u1v)
      "punpcklwd   %%xmm5,%%xmm1                 \n"  // 1122 (32b, 1u1v)

      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"

      "pshufd      $0b01001110,%%xmm2,%%xmm2     \n"  // 1100 (lo, far)
      "pshufd      $0b01001110,%%xmm3,%%xmm3     \n"  // 2211 (hi, far)

      "paddd       %%xmm4,%%xmm2                 \n"  // far+2 (lo)
      "paddd       %%xmm4,%%xmm3                 \n"  // far+2 (hi)
      "paddd       %%xmm0,%%xmm2                 \n"  // near+far+2 (lo)
      "paddd       %%xmm1,%%xmm3                 \n"  // near+far+2 (hi)
      "paddd       %%xmm0,%%xmm0                 \n"  // 2*near (lo)
      "paddd       %%xmm1,%%xmm1                 \n"  // 2*near (hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddd       %%xmm3,%%xmm1                 \n"  // 3*near+far+2 (hi)

      "psrld       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrld       $2,%%xmm1                     \n"  // 3/4*near+1/4*far (hi)
      "packusdw    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 2 uv to 4 uv
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_SCALEUVROWUP2_BILINEAR_16_SSE41
void ScaleUVRowUp2_Bilinear_16_SSE41(const uint16_t* src_ptr,
                                     ptrdiff_t src_stride,
                                     uint16_t* dst_ptr,
                                     ptrdiff_t dst_stride,
                                     int dst_width) {
  asm volatile(
      "pxor        %%xmm7,%%xmm7                 \n"
      "pcmpeqd     %%xmm6,%%xmm6                 \n"
      "psrld       $31,%%xmm6                    \n"
      "pslld       $3,%%xmm6                     \n"  // all 8

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"  // 0011 (16b, 1u1v)
      "movq        4(%0),%%xmm1                  \n"  // 1122 (16b, 1u1v)
      "punpcklwd   %%xmm7,%%xmm0                 \n"  // 0011 (near) (32b, 1u1v)
      "punpcklwd   %%xmm7,%%xmm1                 \n"  // 1122 (near) (32b, 1u1v)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "pshufd      $0b01001110,%%xmm2,%%xmm2     \n"  // 1100 (far) (1, lo)
      "pshufd      $0b01001110,%%xmm3,%%xmm3     \n"  // 2211 (far) (1, hi)
      "paddd       %%xmm0,%%xmm2                 \n"  // near+far (1, lo)
      "paddd       %%xmm1,%%xmm3                 \n"  // near+far (1, hi)
      "paddd       %%xmm0,%%xmm0                 \n"  // 2*near (1, lo)
      "paddd       %%xmm1,%%xmm1                 \n"  // 2*near (1, hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 3*near+far (1, lo)
      "paddd       %%xmm3,%%xmm1                 \n"  // 3*near+far (1, hi)

      "movq        (%0,%3,2),%%xmm2              \n"
      "movq        4(%0,%3,2),%%xmm3             \n"
      "punpcklwd   %%xmm7,%%xmm2                 \n"
      "punpcklwd   %%xmm7,%%xmm3                 \n"
      "movdqa      %%xmm2,%%xmm4                 \n"
      "movdqa      %%xmm3,%%xmm5                 \n"
      "pshufd      $0b01001110,%%xmm4,%%xmm4     \n"  // 1100 (far) (2, lo)
      "pshufd      $0b01001110,%%xmm5,%%xmm5     \n"  // 2211 (far) (2, hi)
      "paddd       %%xmm2,%%xmm4                 \n"  // near+far (2, lo)
      "paddd       %%xmm3,%%xmm5                 \n"  // near+far (2, hi)
      "paddd       %%xmm2,%%xmm2                 \n"  // 2*near (2, lo)
      "paddd       %%xmm3,%%xmm3                 \n"  // 2*near (2, hi)
      "paddd       %%xmm4,%%xmm2                 \n"  // 3*near+far (2, lo)
      "paddd       %%xmm5,%%xmm3                 \n"  // 3*near+far (2, hi)

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm2,%%xmm5                 \n"
      "paddd       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddd       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddd       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddd       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrld       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm2,%%xmm5                 \n"
      "paddd       %%xmm2,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddd       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddd       %%xmm2,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddd       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrld       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm2                 \n"
      "paddd       %%xmm1,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddd       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (2, hi)
      "paddd       %%xmm1,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddd       %%xmm2,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrld       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm2                 \n"
      "paddd       %%xmm3,%%xmm2                 \n"  // 6*near+2*far (2, hi)
      "paddd       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (1, hi)
      "paddd       %%xmm3,%%xmm2                 \n"  // 9*near+3*far (2, hi)
      "paddd       %%xmm1,%%xmm2                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrld       $4,%%xmm2                     \n"  // ^ div by 16 (2, hi)

      "packusdw    %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "packusdw    %%xmm2,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4,2)              \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 2 uv to 4 uv
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width)   // %2
      : "r"(src_stride),  // %3
        "r"(dst_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif

#ifdef HAS_SCALEUVROWUP2_LINEAR_16_AVX2
void ScaleUVRowUp2_Linear_16_AVX2(const uint16_t* src_ptr,
                                  uint16_t* dst_ptr,
                                  int dst_width) {
  asm volatile(
      "vpcmpeqd    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $31,%%ymm4,%%ymm4             \n"
      "vpslld      $1,%%ymm4,%%ymm4              \n"  // all 2

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // 00112233 (16b, 1u1v)
      "vmovdqu     4(%0),%%xmm1                  \n"  // 11223344 (16b, 1u1v)

      "vpmovzxwd   %%xmm0,%%ymm0                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm1,%%ymm1                 \n"  // 12345678 (32b, 1u1v)

      "vpshufd     $0b01001110,%%ymm0,%%ymm2     \n"  // 11003322 (lo, far)
      "vpshufd     $0b01001110,%%ymm1,%%ymm3     \n"  // 22114433 (hi, far)

      "vpaddd      %%ymm4,%%ymm2,%%ymm2          \n"  // far+2 (lo)
      "vpaddd      %%ymm4,%%ymm3,%%ymm3          \n"  // far+2 (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"  // near+far+2 (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm3          \n"  // near+far+2 (hi)
      "vpaddd      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near (lo)
      "vpaddd      %%ymm1,%%ymm1,%%ymm1          \n"  // 2*near (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 3*near+far+2 (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm1          \n"  // 3*near+far+2 (hi)

      "vpsrld      $2,%%ymm0,%%ymm0              \n"  // 3/4*near+1/4*far (lo)
      "vpsrld      $2,%%ymm1,%%ymm1              \n"  // 3/4*near+1/4*far (hi)
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif

#ifdef HAS_SCALEUVROWUP2_BILINEAR_16_AVX2
void ScaleUVRowUp2_Bilinear_16_AVX2(const uint16_t* src_ptr,
                                    ptrdiff_t src_stride,
                                    uint16_t* dst_ptr,
                                    ptrdiff_t dst_stride,
                                    int dst_width) {
  asm volatile(
      "vpcmpeqd    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrld      $31,%%ymm6,%%ymm6             \n"
      "vpslld      $3,%%ymm6,%%ymm6              \n"  // all 8

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0),%%xmm0                   \n"  // 00112233 (16b, 1u1v)
      "vmovdqu     4(%0),%%xmm1                  \n"  // 11223344 (16b, 1u1v)
      "vpmovzxwd   %%xmm0,%%ymm0                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm1,%%ymm1                 \n"  // 12345678 (32b, 1u1v)
      "vpshufd     $0b01001110,%%ymm0,%%ymm2     \n"  // 11003322 (lo, far)
      "vpshufd     $0b01001110,%%ymm1,%%ymm3     \n"  // 22114433 (hi, far)
      "vpaddd      %%ymm0,%%ymm2,%%ymm2          \n"  // near+far (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm3          \n"  // near+far (hi)
      "vpaddd      %%ymm0,%%ymm0,%%ymm0          \n"  // 2*near (lo)
      "vpaddd      %%ymm1,%%ymm1,%%ymm1          \n"  // 2*near (hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 3*near+far (lo)
      "vpaddd      %%ymm1,%%ymm3,%%ymm1          \n"  // 3*near+far (hi)

      "vmovdqu     (%0,%3,2),%%xmm2              \n"  // 00112233 (16b, 1u1v)
      "vmovdqu     4(%0,%3,2),%%xmm3             \n"  // 11223344 (16b, 1u1v)
      "vpmovzxwd   %%xmm2,%%ymm2                 \n"  // 01234567 (32b, 1u1v)
      "vpmovzxwd   %%xmm3,%%ymm3                 \n"  // 12345678 (32b, 1u1v)
      "vpshufd     $0b01001110,%%ymm2,%%ymm4     \n"  // 11003322 (lo, far)
      "vpshufd     $0b01001110,%%ymm3,%%ymm5     \n"  // 22114433 (hi, far)
      "vpaddd      %%ymm2,%%ymm4,%%ymm4          \n"  // near+far (lo)
      "vpaddd      %%ymm3,%%ymm5,%%ymm5          \n"  // near+far (hi)
      "vpaddd      %%ymm2,%%ymm2,%%ymm2          \n"  // 2*near (lo)
      "vpaddd      %%ymm3,%%ymm3,%%ymm3          \n"  // 2*near (hi)
      "vpaddd      %%ymm2,%%ymm4,%%ymm2          \n"  // 3*near+far (lo)
      "vpaddd      %%ymm3,%%ymm5,%%ymm3          \n"  // 3*near+far (hi)

      "vpaddd      %%ymm0,%%ymm0,%%ymm4          \n"  // 6*near+2*far (1, lo)
      "vpaddd      %%ymm6,%%ymm2,%%ymm5          \n"  // 3*near+far+8 (2, lo)
      "vpaddd      %%ymm4,%%ymm0,%%ymm4          \n"  // 9*near+3*far (1, lo)
      "vpaddd      %%ymm4,%%ymm5,%%ymm4          \n"  // 9 3 3 1 + 8 (1, lo)
      "vpsrld      $4,%%ymm4,%%ymm4              \n"  // ^ div by 16 (1, lo)

      "vpaddd      %%ymm2,%%ymm2,%%ymm5          \n"  // 6*near+2*far (2, lo)
      "vpaddd      %%ymm6,%%ymm0,%%ymm0          \n"  // 3*near+far+8 (1, lo)
      "vpaddd      %%ymm5,%%ymm2,%%ymm5          \n"  // 9*near+3*far (2, lo)
      "vpaddd      %%ymm5,%%ymm0,%%ymm5          \n"  // 9 3 3 1 + 8 (2, lo)
      "vpsrld      $4,%%ymm5,%%ymm5              \n"  // ^ div by 16 (2, lo)

      "vpaddd      %%ymm1,%%ymm1,%%ymm0          \n"  // 6*near+2*far (1, hi)
      "vpaddd      %%ymm6,%%ymm3,%%ymm2          \n"  // 3*near+far+8 (2, hi)
      "vpaddd      %%ymm0,%%ymm1,%%ymm0          \n"  // 9*near+3*far (1, hi)
      "vpaddd      %%ymm0,%%ymm2,%%ymm0          \n"  // 9 3 3 1 + 8 (1, hi)
      "vpsrld      $4,%%ymm0,%%ymm0              \n"  // ^ div by 16 (1, hi)

      "vpaddd      %%ymm3,%%ymm3,%%ymm2          \n"  // 6*near+2*far (2, hi)
      "vpaddd      %%ymm6,%%ymm1,%%ymm1          \n"  // 3*near+far+8 (1, hi)
      "vpaddd      %%ymm2,%%ymm3,%%ymm2          \n"  // 9*near+3*far (2, hi)
      "vpaddd      %%ymm2,%%ymm1,%%ymm2          \n"  // 9 3 3 1 + 8 (2, hi)
      "vpsrld      $4,%%ymm2,%%ymm2              \n"  // ^ div by 16 (2, hi)

      "vpackusdw   %%ymm0,%%ymm4,%%ymm4          \n"
      "vmovdqu     %%ymm4,(%1)                   \n"  // store above
      "vpackusdw   %%ymm2,%%ymm5,%%ymm5          \n"
      "vmovdqu     %%ymm5,(%1,%4,2)              \n"  // store below

      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ptr),    // %0
        "+r"(dst_ptr),    // %1
        "+r"(dst_width)   // %2
      : "r"(src_stride),  // %3
        "r"(dst_stride)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif

#ifdef HAS_SCALEUVADDCOLS3_AVX2
static const ulvec32 kPermUV3_0 = {0, 3, 6, 1, 4, 7, 2, 5};
static const ulvec32 kPermUV3_1 = {3, 4, 5, 6, 7, 0, 1, 2};
static const ulvec32 kPermUV3_2 = {6, 7, 0, 1, 2, 3, 4, 5};

// Scale UV columns by 3 box filter.
// Reads 96 bytes (24 UV pixels) and writes 16 bytes (8 UV pixels) per block.
void ScaleUVAddCols3_AVX2(int dst_width,
                          int scaleval,
                          const uint16_t* src_uv,
                          uint8_t* dst_uv) {
#if defined(__x86_64__)
  asm volatile(
      "vmovd       %k[scaleval],%%xmm7           \n"
      "vpbroadcastw %%xmm7,%%ymm7                \n"
      "vmovdqa     %[kPerm0],%%ymm4              \n"
      "vmovdqa     %[kPerm1],%%ymm5              \n"
      "vmovdqa     %[kPerm2],%%ymm6              \n"
      "sub         $0x10,%[dst_width]            \n"
      "jl          2f                            \n"

      // Process 16 output UV pixels (192 input bytes -> 32 output bytes).
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_uv]),%%ymm0            \n"
      "vmovdqu     0x20(%[src_uv]),%%ymm1        \n"
      "vpermd      %%ymm0,%%ymm4,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm0,%%ymm2    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm0,%%ymm0    \n"
      "vmovdqu     0x40(%[src_uv]),%%ymm1        \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm3,%%ymm3    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm2,%%ymm2    \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpermd      %%ymm0,%%ymm6,%%ymm0          \n"
      "vpaddw      %%ymm2,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm0,%%ymm3,%%ymm8          \n"
      "vpmulhuw    %%ymm7,%%ymm8,%%ymm8          \n"

      "vmovdqu     0x60(%[src_uv]),%%ymm0        \n"
      "vmovdqu     0x80(%[src_uv]),%%ymm1        \n"
      "lea         0xc0(%[src_uv]),%[src_uv]     \n"
      "vpermd      %%ymm0,%%ymm4,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm0,%%ymm2    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm0,%%ymm0    \n"
      "vmovdqu     -0x20(%[src_uv]),%%ymm1       \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm3,%%ymm3    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm2,%%ymm2    \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpermd      %%ymm0,%%ymm6,%%ymm0          \n"
      "vpaddw      %%ymm2,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm0,%%ymm3,%%ymm3          \n"
      "vpmulhuw    %%ymm7,%%ymm3,%%ymm3          \n"

      "vpackuswb   %%ymm3,%%ymm8,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%[dst_uv])            \n"
      "lea         0x20(%[dst_uv]),%[dst_uv]     \n"
      "sub         $0x10,%[dst_width]            \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x10,%[dst_width]            \n"
      "jle         99f                           \n"

      // 8-pixel remainder.
      "vmovdqu     (%[src_uv]),%%ymm0            \n"
      "vmovdqu     0x20(%[src_uv]),%%ymm1        \n"
      "vpermd      %%ymm0,%%ymm4,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm0,%%ymm2    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm0,%%ymm0    \n"
      "vmovdqu     0x40(%[src_uv]),%%ymm1        \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm3,%%ymm3    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm2,%%ymm2    \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpermd      %%ymm0,%%ymm6,%%ymm0          \n"
      "vpaddw      %%ymm2,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm0,%%ymm3,%%ymm3          \n"
      "vpmulhuw    %%ymm7,%%ymm3,%%ymm3          \n"
      "vextracti128 $1,%%ymm3,%%xmm2             \n"
      "vpackuswb   %%xmm2,%%xmm3,%%xmm3          \n"
      "vmovdqu     %%xmm3,(%[dst_uv])            \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_uv] "+r"(src_uv),       // %[src_uv]
        [dst_uv] "+r"(dst_uv),       // %[dst_uv]
        [dst_width] "+r"(dst_width)  // %[dst_width]
      : [scaleval] "r"(scaleval),    // %[scaleval]
        [kPerm0] "m"(kPermUV3_0),    // %[kPerm0]
        [kPerm1] "m"(kPermUV3_1),    // %[kPerm1]
        [kPerm2] "m"(kPermUV3_2)     // %[kPerm2]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7", "xmm8");
#else   // defined(__x86_64__)
  asm volatile(
      "vmovd       %[scaleval],%%xmm7            \n"
      "vpbroadcastw %%xmm7,%%ymm7                \n"
      "vmovdqa     %[kPerm0],%%ymm4              \n"
      "vmovdqa     %[kPerm1],%%ymm5              \n"
      "vmovdqa     %[kPerm2],%%ymm6              \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%[src_uv]),%%ymm0            \n"
      "vmovdqu     0x20(%[src_uv]),%%ymm1        \n"
      "vpermd      %%ymm0,%%ymm4,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm3    \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm0,%%ymm2    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm0,%%ymm0    \n"
      "vmovdqu     0x40(%[src_uv]),%%ymm1        \n"
      "lea         0x60(%[src_uv]),%[src_uv]     \n"
      "vpermd      %%ymm1,%%ymm4,%%ymm1          \n"
      "vpblendd    $0xc0,%%ymm1,%%ymm3,%%ymm3    \n"
      "vpblendd    $0x07,%%ymm1,%%ymm2,%%ymm2    \n"
      "vpblendd    $0x38,%%ymm1,%%ymm0,%%ymm0    \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpermd      %%ymm0,%%ymm6,%%ymm0          \n"
      "vpaddw      %%ymm2,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm0,%%ymm3,%%ymm3          \n"
      "vpmulhuw    %%ymm7,%%ymm3,%%ymm3          \n"
      "vextracti128 $1,%%ymm3,%%xmm2             \n"
      "vpackuswb   %%xmm2,%%xmm3,%%xmm3          \n"
      "vmovdqu     %%xmm3,(%[dst_uv])            \n"
      "lea         0x10(%[dst_uv]),%[dst_uv]     \n"
      "sub         $0x8,%[dst_width]             \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [src_uv] "+r"(src_uv),       // %[src_uv]
        [dst_uv] "+r"(dst_uv),       // %[dst_uv]
        [dst_width] "+r"(dst_width)  // %[dst_width]
      : [scaleval] "rm"(scaleval),   // %[scaleval]
        [kPerm0] "m"(kPermUV3_0),    // %[kPerm0]
        [kPerm1] "m"(kPermUV3_1),    // %[kPerm1]
        [kPerm2] "m"(kPermUV3_2)     // %[kPerm2]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
#endif  // defined(__x86_64__)
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

// Scale UV columns by 3 box filter using AVX512VBMI.
// Processes 32 UV pixels (384 input bytes -> 64 output bytes) per main loop
// iteration, followed by a 16-pixel block and a k-masked tail (1..15 pixels).
LIBYUV_TARGET_AVX512VBMI
void ScaleUVAddCols3_AVX512VBMI(int dst_width,
                                int scaleval,
                                const uint16_t* src_uv,
                                uint8_t* dst_uv) {
  intptr_t temp, mask;
  asm volatile(
      "vpbroadcastw %k[scaleval],%%zmm20         \n"
      "vpmovzxbd   %[kCol0],%%zmm16              \n"
      "vpmovzxbd   %[kCol1],%%zmm17              \n"
      "vpmovzxbd   %[kCol2],%%zmm18              \n"
      "vmovdqu8    %[kPack],%%zmm19              \n"
      "mov         $0xf800,%k[temp]              \n"
      "kmovw       %k[temp],%%k5                 \n"
      "mov         $0xfc00,%k[temp]              \n"
      "kmovw       %k[temp],%%k6                 \n"

      "sub         $0x20,%[dst_width]            \n"
      "jl          2f                            \n"

      // Main loop: 32 output UV pixels (64 output bytes) per iteration.
      LABELALIGN
      "1:          \n"
      "vmovdqu32   (%[src_uv]),%%zmm0            \n"
      "vmovdqu32   0x40(%[src_uv]),%%zmm1        \n"
      "vmovdqu32   0x80(%[src_uv]),%%zmm2        \n"
      "vmovdqa32   %%zmm16,%%zmm3                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm3          \n"
      "vmovdqa32   %%zmm17,%%zmm4                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm4          \n"
      "vmovdqa32   %%zmm18,%%zmm5                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm5          \n"
      "vpermd      %%zmm2,%%zmm16,%%zmm3%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm17,%%zmm4%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm18,%%zmm5%{%%k6%} \n"
      "vpaddw      %%zmm4,%%zmm3,%%zmm3          \n"
      "vpaddw      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmulhuw    %%zmm20,%%zmm3,%%zmm0         \n"

      "vmovdqu32   0xc0(%[src_uv]),%%zmm6        \n"
      "vmovdqu32   0x100(%[src_uv]),%%zmm1       \n"
      "vmovdqu32   0x140(%[src_uv]),%%zmm2       \n"
      "lea         0x180(%[src_uv]),%[src_uv]    \n"
      "vmovdqa32   %%zmm16,%%zmm3                \n"
      "vpermi2d    %%zmm1,%%zmm6,%%zmm3          \n"
      "vmovdqa32   %%zmm17,%%zmm4                \n"
      "vpermi2d    %%zmm1,%%zmm6,%%zmm4          \n"
      "vmovdqa32   %%zmm18,%%zmm5                \n"
      "vpermi2d    %%zmm1,%%zmm6,%%zmm5          \n"
      "vpermd      %%zmm2,%%zmm16,%%zmm3%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm17,%%zmm4%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm18,%%zmm5%{%%k6%} \n"
      "vpaddw      %%zmm4,%%zmm3,%%zmm3          \n"
      "vpaddw      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmulhuw    %%zmm20,%%zmm3,%%zmm3         \n"

      "vmovdqa64   %%zmm19,%%zmm2                \n"
      "vpermi2b    %%zmm3,%%zmm0,%%zmm2          \n"
      "vmovdqu8    %%zmm2,(%[dst_uv])            \n"
      "lea         0x40(%[dst_uv]),%[dst_uv]     \n"
      "sub         $0x20,%[dst_width]            \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%[dst_width]            \n"
      "jle         99f                           \n"
      "cmp         $0x10,%[dst_width]            \n"
      "jl          3f                            \n"

      // 16-pixel half-block (32 output bytes).
      "vmovdqu32   (%[src_uv]),%%zmm0            \n"
      "vmovdqu32   0x40(%[src_uv]),%%zmm1        \n"
      "vmovdqu32   0x80(%[src_uv]),%%zmm2        \n"
      "lea         0xc0(%[src_uv]),%[src_uv]     \n"
      "vmovdqa32   %%zmm16,%%zmm3                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm3          \n"
      "vmovdqa32   %%zmm17,%%zmm4                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm4          \n"
      "vmovdqa32   %%zmm18,%%zmm5                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm5          \n"
      "vpermd      %%zmm2,%%zmm16,%%zmm3%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm17,%%zmm4%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm18,%%zmm5%{%%k6%} \n"
      "vpaddw      %%zmm4,%%zmm3,%%zmm3          \n"
      "vpaddw      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmulhuw    %%zmm20,%%zmm3,%%zmm0         \n"
      "vpmovwb     %%zmm0,(%[dst_uv])            \n"
      "lea         0x20(%[dst_uv]),%[dst_uv]     \n"
      "sub         $0x10,%[dst_width]            \n"
      "jle         99f                           \n"

      // Tail: 1..15 output UV pixels (3..45 input dwords, 2..30 output bytes).
      "3:          \n"
      "lea         (%q[dst_width],%q[dst_width],2),%q[temp] \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[temp],%q[mask],%q[mask]    \n"
      "kmovw       %k[mask],%%k1                 \n"
      "shr         $16,%q[mask]                  \n"
      "kmovw       %k[mask],%%k2                 \n"
      "shr         $16,%q[mask]                  \n"
      "kmovw       %k[mask],%%k3                 \n"
      "mov         $-1,%q[mask]                  \n"
      "bzhi        %q[dst_width],%q[mask],%q[mask] \n"
      "kmovw       %k[mask],%%k4                 \n"

      "vmovdqu32   (%[src_uv]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu32   0x40(%[src_uv]),%%zmm1%{%%k2%}%{z%} \n"
      "vmovdqu32   0x80(%[src_uv]),%%zmm2%{%%k3%}%{z%} \n"
      "vmovdqa32   %%zmm16,%%zmm3                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm3          \n"
      "vmovdqa32   %%zmm17,%%zmm4                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm4          \n"
      "vmovdqa32   %%zmm18,%%zmm5                \n"
      "vpermi2d    %%zmm1,%%zmm0,%%zmm5          \n"
      "vpermd      %%zmm2,%%zmm16,%%zmm3%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm17,%%zmm4%{%%k5%} \n"
      "vpermd      %%zmm2,%%zmm18,%%zmm5%{%%k6%} \n"
      "vpaddw      %%zmm4,%%zmm3,%%zmm3          \n"
      "vpaddw      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmulhuw    %%zmm20,%%zmm3,%%zmm0         \n"
      "vpmovwb     %%zmm0,%%ymm0                 \n"
      "vmovdqu16   %%ymm0,(%[dst_uv])%{%%k4%}    \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src_uv] "+r"(src_uv),        // %[src_uv]
        [dst_uv] "+r"(dst_uv),        // %[dst_uv]
        [dst_width] "+r"(dst_width),  // %[dst_width]
        [temp] "=&r"(temp),           // %[temp]
        [mask] "=&r"(mask)            // %[mask]
      : [scaleval] "r"(scaleval),     // %[scaleval]
        [kCol0] "m"(kPerm512_Col0),   // %[kCol0]
        [kCol1] "m"(kPerm512_Col1),   // %[kCol1]
        [kCol2] "m"(kPerm512_Col2),   // %[kCol2]
        [kPack] "m"(kPackEvenBytes)   // %[kPack]
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm16", "xmm17", "xmm18", "xmm19", "xmm20", "k1", "k2", "k3", "k4",
        "k5", "k6");
}
#endif  // HAS_SCALEUVADDCOLS3_AVX512VBMI

#endif  // defined(__x86_64__) || defined(__i386__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
