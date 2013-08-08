/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_vector_float.c - Floating complex vector arithmetic routines.
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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include <assert.h>

#include "floating_fudge.h"
#include "mmx_sse_decs.h"

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE3)
SPAN_DECLARE(void) cvec_mulf(complexf_t z[], const complexf_t x[], const complexf_t y[], int n)
{
    int i;
    __m128 n0;
    __m128 n1;
    __m128 n2;
    __m128 n3;

    if ((i = n & ~1))
    {
        i <<= 1;
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n3 = _mm_loadu_ps((float *) x + i);
            n0 = _mm_moveldup_ps(n3);
            n1 = _mm_loadu_ps((float *) y + i);
            n0 = _mm_mul_ps(n0, n1);
            n1 = _mm_shuffle_ps(n1, n1, 0xB1);
            n2 = _mm_movehdup_ps(n3);
            n2 = _mm_mul_ps(n2, n1);
            n0 = _mm_addsub_ps(n0, n2);
            _mm_storeu_ps((float *) z + i, n0);
        }
    }
    /* Now deal with the last element, which doesn't fill an SSE2 register */
    switch (n & 1)
    {
    case 1:
        z[n - 1].re = x[n - 1].re*y[n - 1].re - x[n - 1].im*y[n - 1].im;
        z[n - 1].im = x[n - 1].re*y[n - 1].im + x[n - 1].im*y[n - 1].re;
    }
}
#else
SPAN_DECLARE(void) cvec_mulf(complexf_t z[], const complexf_t x[], const complexf_t y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        z[i].re = x[i].re*y[i].re - x[i].im*y[i].im;
        z[i].im = x[i].re*y[i].im + x[i].im*y[i].re;
    }
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) cvec_mul(complex_t z[], const complex_t x[], const complex_t y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        z[i].re = x[i].re*y[i].re - x[i].im*y[i].im;
        z[i].im = x[i].re*y[i].im + x[i].im*y[i].re;
    }
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) cvec_mull(complexl_t z[], const complexl_t x[], const complexl_t y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        z[i].re = x[i].re*y[i].re - x[i].im*y[i].im;
        z[i].im = x[i].re*y[i].im + x[i].im*y[i].re;
    }
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(complexf_t) cvec_dot_prodf(const complexf_t x[], const complexf_t y[], int n)
{
    int i;
    complexf_t z;

    z = complex_setf(0.0f, 0.0f);
    for (i = 0;  i < n;  i++)
    {
        z.re += (x[i].re*y[i].re - x[i].im*y[i].im);
        z.im += (x[i].re*y[i].im + x[i].im*y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complex_t) cvec_dot_prod(const complex_t x[], const complex_t y[], int n)
{
    int i;
    complex_t z;

    z = complex_set(0.0, 0.0);
    for (i = 0;  i < n;  i++)
    {
        z.re += (x[i].re*y[i].re - x[i].im*y[i].im);
        z.im += (x[i].re*y[i].im + x[i].im*y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(complexl_t) cvec_dot_prodl(const complexl_t x[], const complexl_t y[], int n)
{
    int i;
    complexl_t z;

    z = complex_setl(0.0L, 0.0L);
    for (i = 0;  i < n;  i++)
    {
        z.re += (x[i].re*y[i].re - x[i].im*y[i].im);
        z.im += (x[i].re*y[i].im + x[i].im*y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(complexf_t) cvec_circular_dot_prodf(const complexf_t x[], const complexf_t y[], int n, int pos)
{
    complexf_t z;
    complexf_t z1;

    z = cvec_dot_prodf(&x[pos], &y[0], n - pos);
    z1 = cvec_dot_prodf(&x[0], &y[n - pos], pos);
    z = complex_addf(&z, &z1);
    return z;
}
/*- End of function --------------------------------------------------------*/

#define LMS_LEAK_RATE   0.9999f

SPAN_DECLARE(void) cvec_lmsf(const complexf_t x[], complexf_t y[], int n, const complexf_t *error)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        /* Leak a little to tame uncontrolled wandering */
        y[i].re = y[i].re*LMS_LEAK_RATE + (x[i].im*error->im + x[i].re*error->re);
        y[i].im = y[i].im*LMS_LEAK_RATE + (x[i].re*error->im - x[i].im*error->re);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) cvec_circular_lmsf(const complexf_t x[], complexf_t y[], int n, int pos, const complexf_t *error)
{
    cvec_lmsf(&x[pos], &y[0], n - pos, error);
    cvec_lmsf(&x[0], &y[n - pos], pos, error);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
