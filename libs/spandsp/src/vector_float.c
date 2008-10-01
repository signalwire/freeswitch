/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_float.c - Floating vector arithmetic routines.
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
 *
 * $Id: vector_float.c,v 1.14 2008/09/18 13:54:32 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>

#if defined(SPANDSP_USE_MMX)
#include <mmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE)
#include <xmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE2)
#include <emmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE3)
#include <pmmintrin.h>
#include <tmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4_1)
#include <smmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4_2)
#include <nmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4A)
#include <ammintrin.h>
#endif
#if defined(SPANDSP_USE_SSE5)
#include <bmmintrin.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/vector_float.h"

void vec_copyf(float z[], const float x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

void vec_copy(double z[], const double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_copyl(long double z[], const long double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_zerof(float z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0f;
}
/*- End of function --------------------------------------------------------*/

void vec_zero(double z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_zerol(long double z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0L;
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_setf(float z[], float x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/

void vec_set(double z[], double x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_setl(long double z[], long double x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_addf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/

void vec_add(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/

void vec_addl(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/

void vec_scaled_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

void vec_scaled_add(double z[], const double x[], double x_scale, const double y[], double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_scaled_addl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_subf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/

void vec_sub(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_subl(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_scaled_subf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

void vec_scaled_sub(double z[], const double x[], double x_scale, const double y[], double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_scaled_subl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
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

void vec_scalar_mul(double z[], const double x[], double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_scalar_mull(long double z[], const long double x[], long double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y;
}
/*- End of function --------------------------------------------------------*/
#endif

void vec_mulf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/

void vec_mul(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
void vec_mull(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
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
    /* Now deal with the last 1 to 3 elements, which don't fill in an SSE2 register */
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

double vec_dot_prod(const double x[], const double y[], int n)
{
    int i;
    double z;

    z = 0.0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
long double vec_dot_prodl(const long double x[], const long double y[], int n)
{
    int i;
    long double z;

    z = 0.0L;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/
#endif

float vec_circular_dot_prodf(const float x[], const float y[], int n, int pos)
{
    float z;

    z = vec_dot_prodf(&x[pos], &y[0], n - pos);
    z += vec_dot_prodf(&x[0], &y[n - pos], pos);
    return z;
}
/*- End of function --------------------------------------------------------*/

void vec_lmsf(const float x[], float y[], int n, float error)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        y[i] += x[i]*error;
        /* Leak a little to tame uncontrolled wandering */
        y[i] *= 0.9999f;
    }
}
/*- End of function --------------------------------------------------------*/

void vec_circular_lmsf(const float x[], float y[], int n, int pos, float error)
{
    vec_lmsf(&x[pos], &y[0], n - pos, error);
    vec_lmsf(&x[0], &y[n - pos], pos, error);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
