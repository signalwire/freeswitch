/*
 * (c) 2025 Stéphane Alnet
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Contributor(s):
 * Stéphane Alnet <stephane@shimaore.net>
 *
 * switch_simd.h -- SIMD definitions
 *
 */

#ifndef SWITCH_SIMD_H
#define SWITCH_SIMD_H

#ifdef SIMD

/* The initial goal of this module is to provide noticeable speed improvements for audio muxing. It probably could be extended to video processing, but I haven't tried yet.
 * For higher speed improvemrnts you generally want your data to be aligned on the SIMD datasize.
 * (See e.g. https://www.agner.org/optimize/instruction_tables.pdf for speed measurements.)
 * Here we focus on 256 bits (8 octets) since
 * - as of 2025 it is essentially available on most x86_64 hardware via AVX and AVX2,
 * - and is an appropriate size for e.g. PCMU or PCMA at 8kHz (512 bits would be too much for 160 bytes).
 * For easy alignment, use the SWITCH_ALIGN macro. It can be used in struct/union, and for stack-allocated variables.
 * Pointers might or might not be aligned. For example, glibc malloc will return 8-octets aligned memory blocks, but an arbitrary pointer inside that structure will not necessarily be aligned!
 * Alignment results in faster loads and stores - instead of sequencing the load and store, the microcode can use a 128-bit or 256-bit lane to move the data between cache and register in a smaller number of steps.
 */

#include <stdalign.h>
#include <string.h>
#include <simde/x86/sse2.h>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>
/* SIMDE will provide substitutes for AVX512 functions on lower platforms. */
#include <simde/x86/avx512.h>

enum {
  int16_per_m256i = sizeof(simde__m256i)/sizeof(int16_t),
  mask_int16_per_m256i = int16_per_m256i-1,
  int16_per_m128i = sizeof(simde__m128i)/sizeof(int16_t),
  mask_int16_per_m128i = int16_per_m128i-1,
  int32_per_m256i = sizeof(simde__m256i)/sizeof(int32_t),
};

/* Apply the `SWITCH_ALIGN` prefix to:
 * - function variables
 * - struct/union fields
 * e.g.
 *
 *   SWITCH_ALIGN int16_t data[SWITCH_RECOMMENDED_BUFFER_SIZE/sizeof(int16_t)];
 *
 * Then `data` can be used safely as destination or source for SIMD_mux_aligned_unbound_sln, for example.
 */
#define SWITCH_ALIGN alignas(sizeof(simde__m256i))

/* SIMD-optimized int16_t saturated addition
 * - aligned: both int16_t pointers must be aligned on 256 bits boundary
 * - unbound: underlying buffer must end on m256i (256 bits / 16 int16_t) boundary.
 *            will modify data outside of the range if sample%4 != 0; might SIGSEV if the underlying buffer is too short.
 * It is safe to use with buffers defined as
 *
 *    SWITCH_ALIGN data[SWITCH_RECOMMENDED_BUFFER_SIZE];
 *
 * for example.
 */
inline static void SIMD_mux_sln_m256i_m256i_unbound(simde__m256i *dst, const simde__m256i *add, int samples)
{
  int x;
  const int blocks = samples / int16_per_m256i;
  for ( x = 0; x < blocks; x++) {
    /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_adds_epi16(
        /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
        simde_mm256_load_si256(dst+x),
        simde_mm256_load_si256(add+x)
      ));
  }
}

/* SIMD-optimized int16_t satured addition
 * - only the first parameter must be aligned
 * - unbound: underlying buffer must end on m256i (256 bits / 16 int16_t) boundary.
 */
inline static void SIMD_mux_sln_m256i_int16_unbound(simde__m256i *dst, const int16_t *add, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m256i;
  for ( x = 0; x < blocks; x++) {
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_adds_epi16(
        simde_mm256_load_si256(dst+x),
        simde_mm256_loadu_si256(add+x*int16_per_m256i)
      ));
  }
}

/* SIMD-optimized int16_t saturated addition
 * - unbound: underlying buffer must end on m256i (256 bits / 16 int16_t) boundary.
 */
inline static void SIMD_mux_sln_int16_int16_unbound(int16_t *dst, const int16_t *add, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m256i;
  for ( x = 0; x < blocks; x++) {
    simde_mm256_storeu_si256(
      dst+x*int16_per_m256i,
      simde_mm256_adds_epi16(
        simde_mm256_loadu_si256(dst+x*int16_per_m256i),
        simde_mm256_loadu_si256(add+x*int16_per_m256i)
      ));
  }
}

inline static int SIMD_is_aligned256(const void *p) {
  return (uintptr_t)p % sizeof(simde__m256i) == 0;
}

inline static int SIMD_is_aligned128(const void *p) {
  return (uintptr_t)p % sizeof(simde__m128i) == 0;
}

inline static void SIMD_mux_sln(int16_t *dst, const int16_t *add, int samples)
{
  /* Round down to the nearest 256 bits block */
  uint bound_len = samples & ~mask_int16_per_m256i;
  uint extra = samples & mask_int16_per_m256i;

  const int dst_aligned = SIMD_is_aligned256(dst);
  const int src_aligned = SIMD_is_aligned256(add);

  /* Process as much as we can from the original buffer */
  if (dst_aligned && src_aligned) {
    SIMD_mux_sln_m256i_m256i_unbound((simde__m256i *)dst, (const simde__m256i *)add, bound_len);
  } else if (dst_aligned) {
    SIMD_mux_sln_m256i_int16_unbound((simde__m256i *)dst, add, bound_len);
  } else {
    SIMD_mux_sln_int16_int16_unbound(dst, add, bound_len);
  }

  if (extra > 0) {
    /* Since the original buffers might not go all the way up to the next 256 bits, we copy the data
     * in local buffers large enough to hold it, then do the maths in SIMD.
     */
    SWITCH_ALIGN int16_t _dst[int16_per_m256i];
    SWITCH_ALIGN int16_t _add[int16_per_m256i];
    memcpy(_dst, dst+bound_len, sizeof(int16_t) * extra);
    memcpy(_add, add+bound_len, sizeof(int16_t) * extra);
    SIMD_mux_sln_m256i_m256i_unbound((simde__m256i *)_dst, (const simde__m256i *)_add, extra);
    memcpy(dst+bound_len, _dst, sizeof(int16_t) * extra);
  }
}

/* In mod_conference we do 16-to-32 bit conversions to avoid overflow. */

/* Convert to unaligned int16_t to unaligned int32_t.
 * - unbound: might overflow the input and output buffers boundaries if samples is not a multiple of 16.
 */
inline static void SIMD_convert32_int16_unbound(int32_t *dst, const int16_t *src, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m128i;
  for ( x = 0; x < blocks; x++) {
    /* Store 8 int32 at once.
     * Apparently SIMDE doesn't define an _aligned_ store operation, but this is fine.
     */
    simde_mm256_storeu_epi32(dst+x,
      /* Sign-extend from 16-bits to 32-bits */
      simde_mm256_cvtepi16_epi32(
        /* Load 8 int16 at one */
        simde_mm_loadu_epi16(src+x)));
  }
}

/* Convert to aligned int32_t (in bunches of 8) to int16_t (in bunches of 8).
 * - unbound: might overflow the input and output buffer boundaries.
 */
inline static void SIMD_convert16_m256i_unbound(simde__m128i *dst, const simde__m256i *src, int samples)
{
  uint x;
  const uint blocks = samples / int32_per_m256i;
  for ( x = 0; x < blocks; x++) {
    simde_mm_store_si128(
      dst+x,
        simde_mm256_cvtsepi32_epi16(
          simde_mm256_load_si256(src+x)
      ));
  }

}

/* Add int16_t samples to packed int32_t values.
 * - unbound: might overflow the input and output buffer boundaries.
 */
inline static void SIMD_mux32_m256i_m128i_unbound(simde__m256i *dst, const simde__m128i *add, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m128i;
  for ( x = 0; x < blocks; x++) {
    /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_add_epi32(
        /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
        simde_mm256_load_si256(dst+x),
        simde_mm256_cvtepi16_epi32(
          simde_mm_load_si128(add+x)
      )));
  }
}

/* Add int16_t samples to packed int32_t values.
 * - unbound: might overflow the input and output buffer boundaries.
 */
inline static void SIMD_mux32_m256i_int16_unbound(simde__m256i *dst, const int16_t *add, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m128i;
  for ( x = 0; x < blocks; x++) {
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_add_epi32(
        simde_mm256_load_si256(dst+x),
        simde_mm256_cvtepi16_epi32(
          simde_mm_loadu_epi16(add+x*int16_per_m128i)
      )));
  }
}

/* Add int16_t samples to packed int32_t values. */
inline static void SIMD_mux32_sln(simde__m256i *dst, const int16_t *add, int samples)
{
  /* Round down to the nearest 128 bits block */
  uint bound_len = samples & ~mask_int16_per_m128i;
  uint extra = samples & mask_int16_per_m128i;

  const int src_aligned = SIMD_is_aligned128(add);

  /* Process as much as we can from the original buffer */
  if (src_aligned) {
    SIMD_mux32_m256i_m128i_unbound((simde__m256i *)dst, (const simde__m128i *)add, bound_len);
  } else {
    SIMD_mux32_m256i_int16_unbound(dst, add, bound_len);
  }

  if (extra > 0) {
    /* Since the original buffers might not go all the way up to the next 256 bits, we copy the data
     * in local buffers large enough to hold it, then do the maths in SIMD.
     */
    SWITCH_ALIGN int16_t _add[int16_per_m128i];
    memcpy(_add, add+bound_len, sizeof(int16_t) * extra);
    SIMD_mux32_m256i_m128i_unbound(dst, (const simde__m128i *)_add, extra);
  }
}

/* Subtract packed, aligned int16_t values from packed, aligned int32_t values.
 * - unbound: might overflow the input and output buffer boundaries.
 */
inline static void SIMD_sub32_m256i_m128i_unbound(simde__m256i *dst, const simde__m128i *sub, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m128i;
  for ( x = 0; x < blocks; x++) {
    /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_sub_epi32(
        /* AVX: Must be aligned on a 32-byte (128 bits) boundary) */
        simde_mm256_load_si256(dst+x),
        simde_mm256_cvtepi16_epi32(
          simde_mm_load_si128(sub+x)
      )));
  }
}

/* Subtract int16_t values from packed, aligned int32_t values.
 * - unbound: might overflow the input and output buffer boundaries.
 */
inline static void SIMD_sub32_m256i_int16_unbound(simde__m256i *dst, const int16_t *add, int samples)
{
  uint x;
  const uint blocks = samples / int16_per_m128i;
  for ( x = 0; x < blocks; x++) {
    simde_mm256_store_si256(
      dst+x,
      simde_mm256_sub_epi32(
        simde_mm256_load_si256(dst+x),
        simde_mm256_cvtepi16_epi32(
          simde_mm_loadu_epi16(add+x*int16_per_m128i)
      )));
  }
}

/* Subtract int16_t values from packed, aligned int32_t values.
 */
inline static void SIMD_sub32_sln(simde__m256i *dst, const int16_t *add, int samples)
{
  /* Round down to the nearest 256 bits block */
  uint bound_len = samples & ~mask_int16_per_m128i;
  uint extra = samples & mask_int16_per_m128i;

  const int src_aligned = SIMD_is_aligned128(add);

  /* Process as much as we can from the original buffer */
  if (src_aligned) {
    SIMD_sub32_m256i_m128i_unbound((simde__m256i *)dst, (const simde__m128i *)add, bound_len);
  } else {
    SIMD_sub32_m256i_int16_unbound(dst, add, bound_len);
  }

  if (extra > 0) {
    /* Since the original buffers might not go all the way up to the next 256 bits, we copy the data
     * in local buffers large enough to hold it, then do the maths in SIMD.
     */
    SWITCH_ALIGN int16_t _add[int16_per_m128i];
    memcpy(_add, add+bound_len, sizeof(int16_t) * extra);
    SIMD_sub32_m256i_m128i_unbound(dst, (const simde__m128i *)_add, extra);
  }
}

#else /* SIMD */

#define SWITCH_ALIGN

#endif /* SIMD */

#endif /* SWITCH_SIMD_H */
