/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_int.c - Integer vector arithmetic
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>

#include "floating_fudge.h"
#include "mmx_sse_decs.h"

#include "spandsp/telephony.h"
#include "spandsp/vector_int.h"

SPAN_DECLARE(int32_t) vec_dot_prodi16(const int16_t x[], const int16_t y[], int n)
{
    int32_t z;

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__x86_64__)
    __asm__ __volatile__(
        " emms;\n"
        " pxor %%mm0,%%mm0;\n"
        " leaq -32(%%rsi,%%rax,2),%%rdx;\n"     /* rdx = top - 32 */

        " cmpq %%rdx,%%rsi;\n"
        " ja 1f;\n"

        /* Work in blocks of 16 int16_t's until we are near the end */
        " .p2align 2;\n"
        "2:\n"
        " movq (%%rdi),%%mm1;\n"
        " movq (%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 8(%%rdi),%%mm1;\n"
        " movq 8(%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 16(%%rdi),%%mm1;\n"
        " movq 16(%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 24(%%rdi),%%mm1;\n"
        " movq 24(%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addq $32,%%rsi;\n"
        " addq $32,%%rdi;\n"
        " cmpq %%rdx,%%rsi;\n"
        " jbe 2b;\n"

        " .p2align 2;\n"
        "1:\n"
        " addq $24,%%rdx;\n"                  /* Now edx = top - 8 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 3f;\n"

        /* Work in blocks of 4 int16_t's until we are near the end */
        " .p2align 2;\n"
        "4:\n"
        " movq (%%rdi),%%mm1;\n"
        " movq (%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addq $8,%%rsi;\n"
        " addq $8,%%rdi;\n"
        " cmpq %%rdx,%%rsi;"
        " jbe 4b;\n"

        " .p2align 2;\n"
        "3:\n"
        " addq $4,%%rdx;\n"                  /* Now edx = top - 4 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 5f;\n"

        /* Work in a block of 2 int16_t's */
        " movd (%%rdi),%%mm1;\n"
        " movd (%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addq $4,%%rsi;\n"
        " addq $4,%%rdi;\n"

        " .p2align 2;\n"
        "5:\n"
        " addq $2,%%rdx;\n"                  /* Now edx = top - 2 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 6f;\n"

        /* Deal with the very last int16_t, when n is odd */
        " movswl (%%rdi),%%eax;\n"
        " andl $65535,%%eax;\n"
        " movd %%eax,%%mm1;\n"
        " movswl (%%rsi),%%eax;\n"
        " andl $65535,%%eax;\n"
        " movd %%eax,%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " .p2align 2;\n"
        "6:\n"
        /* Merge the pieces of the answer */
        " movq %%mm0,%%mm1;\n"
        " punpckhdq %%mm0,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        /* Et voila, eax has the final result */
        " movd %%mm0,%%eax;\n"

        " emms;\n"
        : "=a" (z)
        : "S" (x), "D" (y), "a" (n)
        : "cc"
    );
#elif defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__i386__)
    __asm__ __volatile__(
        " emms;\n"
        " pxor %%mm0,%%mm0;\n"
        " leal -32(%%esi,%%eax,2),%%edx;\n"     /* edx = top - 32 */

        " cmpl %%edx,%%esi;\n"
        " ja 1f;\n"

        /* Work in blocks of 16 int16_t's until we are near the end */
        " .p2align 2;\n"
        "2:\n"
        " movq (%%edi),%%mm1;\n"
        " movq (%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 8(%%edi),%%mm1;\n"
        " movq 8(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 16(%%edi),%%mm1;\n"
        " movq 16(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 24(%%edi),%%mm1;\n"
        " movq 24(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addl $32,%%esi;\n"
        " addl $32,%%edi;\n"
        " cmpl %%edx,%%esi;\n"
        " jbe 2b;\n"

        " .p2align 2;\n"
        "1:\n"
        " addl $24,%%edx;\n"                  /* Now edx = top - 8 */
        " cmpl %%edx,%%esi;\n"
        " ja 3f;\n"

        /* Work in blocks of 4 int16_t's until we are near the end */
        " .p2align 2;\n"
        "4:\n"
        " movq (%%edi),%%mm1;\n"
        " movq (%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addl $8,%%esi;\n"
        " addl $8,%%edi;\n"
        " cmpl %%edx,%%esi;"
        " jbe 4b;\n"

        " .p2align 2;\n"
        "3:\n"
        " addl $4,%%edx;\n"                  /* Now edx = top - 4 */
        " cmpl %%edx,%%esi;\n"
        " ja 5f;\n"

        /* Work in a block of 2 int16_t's */
        " movd (%%edi),%%mm1;\n"
        " movd (%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addl $4,%%esi;\n"
        " addl $4,%%edi;\n"

        " .p2align 2;\n"
        "5:\n"
        " addl $2,%%edx;\n"                  /* Now edx = top - 2 */
        " cmpl %%edx,%%esi;\n"
        " ja 6f;\n"

        /* Deal with the very last int16_t, when n is odd */
        " movswl (%%edi),%%eax;\n"
        " andl $65535,%%eax;\n"
        " movd %%eax,%%mm1;\n"
        " movswl (%%esi),%%eax;\n"
        " andl $65535,%%eax;\n"
        " movd %%eax,%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " .p2align 2;\n"
        "6:\n"
        /* Merge the pieces of the answer */
        " movq %%mm0,%%mm1;\n"
        " punpckhdq %%mm0,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        /* Et voila, eax has the final result */
        " movd %%mm0,%%eax;\n"

        " emms;\n"
        : "=a" (z)
        : "S" (x), "D" (y), "a" (n)
        : "cc"
    );
#else
    int i;

    z = 0;
    for (i = 0;  i < n;  i++)
        z += (int32_t) x[i]*(int32_t) y[i];
#endif
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) vec_circular_dot_prodi16(const int16_t x[], const int16_t y[], int n, int pos)
{
    int32_t z;

    z = vec_dot_prodi16(&x[pos], &y[0], n - pos);
    z += vec_dot_prodi16(&x[0], &y[n - pos], pos);
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_lmsi16(const int16_t x[], int16_t y[], int n, int16_t error)
{
    int i;

    for (i = 0;  i < n;  i++)
        y[i] += (int16_t) (((int32_t) x[i]*(int32_t) error) >> 15);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_circular_lmsi16(const int16_t x[], int16_t y[], int n, int pos, int16_t error)
{
    vec_lmsi16(&x[pos], &y[0], n - pos, error);
    vec_lmsi16(&x[0], &y[n - pos], pos, error);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) vec_min_maxi16(const int16_t x[], int n, int16_t out[])
{
#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__x86_64__)
    static const int32_t lower_bound = 0x80008000;
    static const int32_t upper_bound = 0x7FFF7FFF;
    int32_t max;

    __asm__ __volatile__(
        " emms;\n"
        " pushq %%rdx;\n"
        " leaq -8(%%rsi,%%rax,2),%%rdx;\n"

        " cmpq %%rdx,%%rsi;\n"
        " jbe 2f;\n"
        " movd %[lower],%%mm0;\n"
        " movd %[upper],%%mm1;\n"
        " jmp 1f;\n"

        " .p2align 2;\n"
        "2:\n"
        " movq (%%rsi),%%mm0;\n"   /* mm0 will be max's */
        " movq %%mm0,%%mm1;\n"     /* mm1 will be min's */
        " addq $8,%%rsi;\n"
        " cmpq %%rdx,%%rsi;\n"
        " ja 4f;\n"

        "3:\n"
        " movq (%%rsi),%%mm2;\n"

        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " movq %%mm3,%%mm4;\n"
        " pand %%mm2,%%mm3;\n"     /* mm3 is mm2 masked to new max's */
        " pandn %%mm0,%%mm4;\n"    /* mm4 is mm0 masked to its max's */
        " por %%mm3,%%mm4;\n"
        " movq %%mm4,%%mm0;\n"     /* Now mm0 is updated max's */
        
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " addq $8,%%rsi;\n"
        " cmpq %%rdx,%%rsi;\n"
        " jbe 3b;\n"

        " .p2align 2;\n"
        "4:\n"
        /* Merge down the 4-word max/mins to lower 2 words */
        " movq %%mm0,%%mm2;\n"
        " psrlq $32,%%mm2;\n"
        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new max's */
        " pandn %%mm0,%%mm3;\n"    /* mm3 is mm0 masked to its max's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm0;\n"     /* now mm0 is updated max's */

        " movq %%mm1,%%mm2;\n"
        " psrlq $32,%%mm2;\n"
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " .p2align 2;\n"
        "1:\n"
        " addq $4,%%rdx;\n"        /* now dx = top-4 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 5f;\n"
        /* Here, there are >= 2 words of input remaining */
        " movd (%%rsi),%%mm2;\n"

        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " movq %%mm3,%%mm4;\n"
        " pand %%mm2,%%mm3;\n"     /* mm3 is mm2 masked to new max's */
        " pandn %%mm0,%%mm4;\n"    /* mm4 is mm0 masked to its max's */
        " por %%mm3,%%mm4;\n"
        " movq %%mm4,%%mm0;\n"     /* now mm0 is updated max's */

        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " addq $4,%%rsi;\n"

        " .p2align 2;\n"
        "5:\n"
        /* Merge down the 2-word max/mins to 1 word */
        " movq %%mm0,%%mm2;\n"
        " psrlq $16,%%mm2;\n"
        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new max's */
        " pandn %%mm0,%%mm3;\n"    /* mm3 is mm0 masked to its max's */
        " por %%mm3,%%mm2;\n"
        " movd %%mm2,%%ecx;\n"     /* cx is max so far */

        " movq %%mm1,%%mm2;\n"
        " psrlq $16,%%mm2;\n"
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movd %%mm2,%%eax;\n"     /* ax is min so far */
        
        " addq $2,%%rdx;\n"        /* now dx = top-2 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 6f;\n"

        /* Here, there is one word of input left */
        " cmpw (%%rsi),%%cx;\n"
        " jge 9f;\n"
        " movw (%%rsi),%%cx;\n"
        " .p2align 2;\n"
        "9:\n"
        " cmpw (%%rsi),%%ax;\n"
        " jle 6f;\n"
        " movw (%%rsi),%%ax;\n"

        " .p2align 2;\n"
        "6:\n"
        /* (finally!) cx is the max, ax the min */
        " movswl %%cx,%%ecx;\n"
        " movswl %%ax,%%eax;\n"

        " popq %%rdx;\n"            /* ptr to output max,min vals */
        " andq %%rdx,%%rdx;\n"
        " jz 7f;\n"
        " movw %%cx,(%%rdx);\n"    /* max */
        " movw %%ax,2(%%rdx);\n"   /* min */
        " .p2align 2;\n"
        "7:\n"
        /* Now calculate max absolute value */
        " negl %%eax;\n"
        " cmpl %%ecx,%%eax;\n"
        " jge 8f;\n"
        " movl %%ecx,%%eax;\n"
        " .p2align 2;\n"
        "8:\n"
        " emms;\n"
        : "=a" (max)
        : "S" (x), "a" (n), "d" (out), [lower] "m" (lower_bound), [upper] "m" (upper_bound)
        : "ecx"
    );
#elif defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__i386__)
    static const int32_t lower_bound = 0x80008000;
    static const int32_t upper_bound = 0x7FFF7FFF;
    int32_t max;

    __asm__ __volatile__(
        " emms;\n"
        " pushl %%edx;\n"
        " leal -8(%%esi,%%eax,2),%%edx;\n"

        " cmpl %%edx,%%esi;\n"
        " jbe 2f;\n"
        " movd %[lower],%%mm0;\n"
        " movd %[upper],%%mm1;\n"
        " jmp 1f;\n"

        " .p2align 2;\n"
        "2:\n"
        " movq (%%esi),%%mm0;\n"   /* mm0 will be max's */
        " movq %%mm0,%%mm1;\n"     /* mm1 will be min's */
        " addl $8,%%esi;\n"
        " cmpl %%edx,%%esi;\n"
        " ja 4f;\n"

        " .p2align 2;\n"
        "3:\n"
        " movq (%%esi),%%mm2;\n"

        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " movq %%mm3,%%mm4;\n"
        " pand %%mm2,%%mm3;\n"     /* mm3 is mm2 masked to new max's */
        " pandn %%mm0,%%mm4;\n"    /* mm4 is mm0 masked to its max's */
        " por %%mm3,%%mm4;\n"
        " movq %%mm4,%%mm0;\n"     /* Now mm0 is updated max's */
        
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " addl $8,%%esi;\n"
        " cmpl %%edx,%%esi;\n"
        " jbe 3b;\n"

        " .p2align 2;\n"
        "4:\n"
        /* Merge down the 4-word max/mins to lower 2 words */
        " movq %%mm0,%%mm2;\n"
        " psrlq $32,%%mm2;\n"
        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new max's */
        " pandn %%mm0,%%mm3;\n"    /* mm3 is mm0 masked to its max's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm0;\n"     /* now mm0 is updated max's */

        " movq %%mm1,%%mm2;\n"
        " psrlq $32,%%mm2;\n"
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " .p2align 2;\n"
        "1:\n"
        " addl $4,%%edx;\n"        /* now dx = top-4 */
        " cmpl %%edx,%%esi;\n"
        " ja 5f;\n"
        /* Here, there are >= 2 words of input remaining */
        " movd (%%esi),%%mm2;\n"

        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " movq %%mm3,%%mm4;\n"
        " pand %%mm2,%%mm3;\n"     /* mm3 is mm2 masked to new max's */
        " pandn %%mm0,%%mm4;\n"    /* mm4 is mm0 masked to its max's */
        " por %%mm3,%%mm4;\n"
        " movq %%mm4,%%mm0;\n"     /* now mm0 is updated max's */

        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movq %%mm2,%%mm1;\n"     /* now mm1 is updated min's */

        " addl $4,%%esi;\n"

        " .p2align 2;\n"
        "5:\n"
        /* Merge down the 2-word max/mins to 1 word */
        " movq %%mm0,%%mm2;\n"
        " psrlq $16,%%mm2;\n"
        " movq %%mm2,%%mm3;\n"
        " pcmpgtw %%mm0,%%mm3;\n"  /* mm3 is bitmask for words where mm2 > mm0 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new max's */
        " pandn %%mm0,%%mm3;\n"    /* mm3 is mm0 masked to its max's */
        " por %%mm3,%%mm2;\n"
        " movd %%mm2,%%ecx;\n"     /* cx is max so far */

        " movq %%mm1,%%mm2;\n"
        " psrlq $16,%%mm2;\n"
        " movq %%mm1,%%mm3;\n"
        " pcmpgtw %%mm2,%%mm3;\n"  /* mm3 is bitmask for words where mm2 < mm1 */ 
        " pand %%mm3,%%mm2;\n"     /* mm2 is mm2 masked to new min's */
        " pandn %%mm1,%%mm3;\n"    /* mm3 is mm1 masked to its min's */
        " por %%mm3,%%mm2;\n"
        " movd %%mm2,%%eax;\n"     /* ax is min so far */
        
        " addl $2,%%edx;\n"        /* now dx = top-2 */
        " cmpl %%edx,%%esi;\n"
        " ja 6f;\n"

        /* Here, there is one word of input left */
        " cmpw (%%esi),%%cx;\n"
        " jge 9f;\n"
        " movw (%%esi),%%cx;\n"
        " .p2align 2;\n"
        "9:\n"
        " cmpw (%%esi),%%ax;\n"
        " jle 6f;\n"
        " movw (%%esi),%%ax;\n"

        " .p2align 2;\n"
        "6:\n"
        /* (finally!) cx is the max, ax the min */
        " movswl %%cx,%%ecx;\n"
        " movswl %%ax,%%eax;\n"

        " popl %%edx;\n"            /* ptr to output max,min vals */
        " andl %%edx,%%edx;\n"
        " jz 7f;\n"
        " movw %%cx,(%%edx);\n"    /* max */
        " movw %%ax,2(%%edx);\n"   /* min */
        " .p2align 2;\n"
        "7:\n"
        /* Now calculate max absolute value */
        " negl %%eax;\n"
        " cmpl %%ecx,%%eax;\n"
        " jge 8f;\n"
        " movl %%ecx,%%eax;\n"
        " .p2align 2;\n"
        "8:\n"
        " emms;\n"
        : "=a" (max)
        : "S" (x), "a" (n), "d" (out), [lower] "m" (lower_bound), [upper] "m" (upper_bound)
        : "ecx"
    );
#else
    int i;
    int16_t min;
    int16_t max;
    int16_t temp;
    int32_t z;
    
    max = INT16_MIN;
    min = INT16_MAX;
    for (i = 0;  i < n;  i++)
    {
        temp = x[i];
        if (temp > max)
            max = temp;
        /*endif*/
        if (temp < min)
            min = temp;
        /*endif*/
    }
    /*endfor*/
    if (out)
    {
        out[0] = max;
        out[1] = min;
    }
    z = abs(min);
    if (z > max)
        return z;
#endif
    return max;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
