/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * utilities.c
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>

#if defined(G722_1_USE_MMX)
#include <mmintrin.h>
#endif
#if defined(G722_1_USE_SSE)
#include <xmmintrin.h>
#endif
#if defined(G722_1_USE_SSE2)
#include <emmintrin.h>
#endif
#if defined(G722_1_USE_SSE3)
#include <pmmintrin.h>
#include <tmmintrin.h>
#endif
#if defined(G722_1_USE_SSE4_1)
#include <smmintrin.h>
#endif
#if defined(G722_1_USE_SSE4_2)
#include <nmmintrin.h>
#endif
#if defined(G722_1_USE_SSE4A)
#include <ammintrin.h>
#endif
#if defined(G722_1_USE_SSE5)
#include <bmmintrin.h>
#endif

#include "utilities.h"

#if defined(G722_1_USE_FIXED_POINT)
void vec_copyi16(int16_t z[], const int16_t x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

int32_t vec_dot_prodi16(const int16_t x[], const int16_t y[], int n)
{
    int32_t z;

#if defined(__GNUC__)  &&  defined(G722_1_USE_MMX)
#if defined(__x86_64__)
    __asm__ __volatile__(
        " emms;\n"
        " pxor %%mm0,%%mm0;\n"
        " leal -32(%%rsi,%%eax,2),%%edx;\n"     /* edx = top - 32 */

        " cmpl %%rdx,%%rsi;\n"
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

        " addl $32,%%rsi;\n"
        " addl $32,%%rdi;\n"
        " cmpl %%rdx,%%rsi;\n"
        " jbe 2b;\n"

        " .p2align 2;\n"
        "1:\n"
        " addl $24,%%rdx;\n"                  /* Now edx = top - 8 */
        " cmpl %%rdx,%%rsi;\n"
        " ja 3f;\n"

        /* Work in blocks of 4 int16_t's until we are near the end */
        " .p2align 2;\n"
        "4:\n"
        " movq (%%rdi),%%mm1;\n"
        " movq (%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addl $8,%%rsi;\n"
        " addl $8,%%rdi;\n"
        " cmpl %%rdx,%%rsi;"
        " jbe 4b;\n"

        " .p2align 2;\n"
        "3:\n"
        " addl $4,%%rdx;\n"                  /* Now edx = top - 4 */
        " cmpl %%rdx,%%rsi;\n"
        " ja 5f;\n"

        /* Work in a block of 2 int16_t's */
        " movd (%%rdi),%%mm1;\n"
        " movd (%%rsi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"

        " addl $4,%%rsi;\n"
        " addl $4,%%rdi;\n"

        " .p2align 2;\n"
        "5:\n"
        " addl $2,%%rdx;\n"                  /* Now edx = top - 2 */
        " cmpl %%rdx,%%rsi;\n"
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
#else
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
#endif
#else
    int i;

    z = 0;
    for (i = 0;  i < n;  i++)
        z += (int32_t) x[i]*(int32_t) y[i];
#endif
    return z;
}
/*- End of function --------------------------------------------------------*/
#else
#if defined(__GNUC__)  &&  defined(G722_1_USE_SSE2)
void vec_copyf(float z[], const float x[], int n)
{
    int i;
    __m128 n1;
 
    if ((i = n & ~3))
    {
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            _mm_storeu_ps(z + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3];
    case 2:
        z[n - 2] = x[n - 2];
    case 1:
        z[n - 1] = x[n - 1];
    }
}
#else
void vec_copyf(float z[], const float x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
#endif
/*- End of function --------------------------------------------------------*/

#if defined(__GNUC__)  &&  defined(G722_1_USE_SSE2)
void vec_zerof(float z[], int n)
{
    int i;
    __m128 n1;
 
    if ((i = n & ~3))
    {
        n1 = _mm_setzero_ps();
        for (i -= 4;  i >= 0;  i -= 4)
            _mm_storeu_ps(z + i, n1);
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = 0;
    case 2:
        z[n - 2] = 0;
    case 1:
        z[n - 1] = 0;
    }
}
#else
void vec_zerof(float z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0f;
}
#endif
/*- End of function --------------------------------------------------------*/

void vec_subf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(__GNUC__)  &&  defined(G722_1_USE_SSE2)
void vec_mulf(float z[], const float x[], const float y[], int n)
{
    int i;
    __m128 n1;
    __m128 n2;
    __m128 n3;
 
    if ((i = n & ~3))
    {
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n3 = _mm_mul_ps(n1, n2);
            _mm_storeu_ps(z + i, n3);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3]*y[n - 3];
    case 2:
        z[n - 2] = x[n - 2]*y[n - 2];
    case 1:
        z[n - 1] = x[n - 1]*y[n - 1];
    }
}
#else
void vec_mulf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
#endif
/*- End of function --------------------------------------------------------*/

#if defined(__GNUC__)  &&  defined(G722_1_USE_SSE2)
float vec_dot_prodf(const float x[], const float y[], int n)
{
    int i;
    float z;
    __m128 n1;
    __m128 n2;
    __m128 n3;
    __m128 n4;
 
    z = 0.0f;
    if ((i = n & ~3))
    {    
        n4 = _mm_setzero_ps();  //sets sum to zero
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n3 = _mm_mul_ps(n1, n2);
            n4 = _mm_add_ps(n4, n3);
        }
        n4 = _mm_add_ps(_mm_movehl_ps(n4, n4), n4);
        n4 = _mm_add_ss(_mm_shuffle_ps(n4, n4, 1), n4);
        _mm_store_ss(&z, n4);
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z += x[n - 3]*y[n - 3];
    case 2:
        z += x[n - 2]*y[n - 2];
    case 1:
        z += x[n - 1]*y[n - 1];
    }
    return z;
}
#else
float vec_dot_prodf(const float x[], const float y[], int n)
{
    int i;
    float z;

    z = 0.0f;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_scalar_mulf(float z[], const float x[], float y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y;
}
/*- End of function --------------------------------------------------------*/

void vec_scaled_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

void vec_scaled_subf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
