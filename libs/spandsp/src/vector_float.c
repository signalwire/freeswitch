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
 * $Id: vector_float.c,v 1.22 2009/07/12 09:23:09 steveu Exp $
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
#include "spandsp/vector_float.h"

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_copyf(float z[], const float x[], int n)
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
SPAN_DECLARE(void) vec_copyf(float z[], const float x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_copy(double z[], const double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_copyl(long double z[], const long double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_negatef(float z[], const float x[], int n)
{
    int i;
	static const uint32_t mask = 0x80000000;
	static const float *fmask = (float *) &mask;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        n2 = _mm_set1_ps(*fmask);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
		    n1 = _mm_xor_ps(n1, n2);
            _mm_storeu_ps(z + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = -x[n - 3];
    case 2:
        z[n - 2] = -x[n - 2];
    case 1:
        z[n - 1] = -x[n - 1];
    }
}
#else
SPAN_DECLARE(void) vec_negatef(float z[], const float x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = -x[i];
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_negate(double z[], const double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = -x[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_negatel(long double z[], const long double x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = -x[i];
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_zerof(float z[], int n)
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
SPAN_DECLARE(void) vec_zerof(float z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0f;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_zero(double z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_zerol(long double z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = 0.0L;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_setf(float z[], float x, int n)
{
    int i;
    __m128 n1;
 
    if ((i = n & ~3))
    {
        n1 = _mm_set1_ps(x);
        for (i -= 4;  i >= 0;  i -= 4)
            _mm_storeu_ps(z + i, n1);
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x;
    case 2:
        z[n - 2] = x;
    case 1:
        z[n - 1] = x;
    }
}
#else
SPAN_DECLARE(void) vec_setf(float z[], float x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_set(double z[], double x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_setl(long double z[], long double x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_addf(float z[], const float x[], const float y[], int n)
{
    int i;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n2 = _mm_add_ps(n1, n2);
            _mm_storeu_ps(z + i, n2);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3] + y[n - 3];
    case 2:
        z[n - 2] = x[n - 2] + y[n - 2];
    case 1:
        z[n - 1] = x[n - 1] + y[n - 1];
    }
}
#else
SPAN_DECLARE(void) vec_addf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_add(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_addl(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_scaledxy_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;
    __m128 n1;
    __m128 n2;
    __m128 n3;
    __m128 n4;
 
    if ((i = n & ~3))
    {
        n3 = _mm_set1_ps(x_scale);
        n4 = _mm_set1_ps(y_scale);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n1 = _mm_mul_ps(n1, n3);
            n2 = _mm_mul_ps(n2, n4);
            n2 = _mm_add_ps(n1, n2);
            _mm_storeu_ps(z + i, n2);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3]*x_scale + y[n - 3]*y_scale;
    case 2:
        z[n - 2] = x[n - 2]*x_scale + y[n - 2]*y_scale;
    case 1:
        z[n - 1] = x[n - 1]*x_scale + y[n - 1]*y_scale;
    }
}
#else
SPAN_DECLARE(void) vec_scaledxy_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scaledxy_add(double z[], const double x[], double x_scale, const double y[], double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_scaledxy_addl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_scaledy_addf(float z[], const float x[], const float y[], float y_scale, int n)
{
    int i;
    __m128 n1;
    __m128 n2;
    __m128 n3;
 
    if ((i = n & ~3))
    {
        n3 = _mm_set1_ps(y_scale);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n2 = _mm_mul_ps(n2, n3);
            n2 = _mm_add_ps(n1, n2);
            _mm_storeu_ps(z + i, n2);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3] + y[n - 3]*y_scale;
    case 2:
        z[n - 2] = x[n - 2] + y[n - 2]*y_scale;
    case 1:
        z[n - 1] = x[n - 1] + y[n - 1]*y_scale;
    }
}
#else
SPAN_DECLARE(void) vec_scaledy_addf(float z[], const float x[], const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i]*y_scale;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scaledy_add(double z[], const double x[], const double y[], double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_scaledy_addl(long double z[], const long double x[], const long double y[], long double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_subf(float z[], const float x[], const float y[], int n)
{
    int i;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n2 = _mm_sub_ps(n1, n2);
            _mm_storeu_ps(z + i, n2);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3] - y[n - 3];
    case 2:
        z[n - 2] = x[n - 2] - y[n - 2];
    case 1:
        z[n - 1] = x[n - 1] - y[n - 1];
    }
}
#else
SPAN_DECLARE(void) vec_subf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_sub(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_subl(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(void) vec_scaledxy_subf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scaledxy_sub(double z[], const double x[], double x_scale, const double y[], double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_scaledxy_subl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale - y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_scalar_mulf(float z[], const float x[], float y, int n)
{
    int i;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        n2 = _mm_set1_ps(y);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n1 = _mm_mul_ps(n1, n2);
            _mm_storeu_ps(z + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3]*y;
    case 2:
        z[n - 2] = x[n - 2]*y;
    case 1:
        z[n - 1] = x[n - 1]*y;
    }
}
#else
SPAN_DECLARE(void) vec_scalar_mulf(float z[], const float x[], float y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scalar_mul(double z[], const double x[], double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y;
}
/*- End of function --------------------------------------------------------*/

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_scalar_addf(float z[], const float x[], float y, int n)
{
    int i;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        n2 = _mm_set1_ps(y);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n1 = _mm_add_ps(n1, n2);
            _mm_storeu_ps(z + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3] + y;
    case 2:
        z[n - 2] = x[n - 2] + y;
    case 1:
        z[n - 1] = x[n - 1] + y;
    }
}
#else
SPAN_DECLARE(void) vec_scalar_addf(float z[], const float x[], float y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scalar_add(double z[], const double x[], double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_scalar_addl(long double z[], const long double x[], long double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_scalar_subf(float z[], const float x[], float y, int n)
{
    int i;
    __m128 n1;
    __m128 n2;
 
    if ((i = n & ~3))
    {
        n2 = _mm_set1_ps(y);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n1 = _mm_sub_ps(n1, n2);
            _mm_storeu_ps(z + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        z[n - 3] = x[n - 3] - y;
    case 2:
        z[n - 2] = x[n - 2] - y;
    case 1:
        z[n - 1] = x[n - 1] - y;
    }
}
#else
SPAN_DECLARE(void) vec_scalar_subf(float z[], const float x[], float y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y;
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_scalar_sub(double z[], const double x[], double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_scalar_subl(long double z[], const long double x[], long double y, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_mulf(float z[], const float x[], const float y[], int n)
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
SPAN_DECLARE(void) vec_mulf(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(void) vec_mul(double z[], const double x[], const double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
SPAN_DECLARE(void) vec_mull(long double z[], const long double x[], const long double y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(float) vec_dot_prodf(const float x[], const float y[], int n)
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
SPAN_DECLARE(float) vec_dot_prodf(const float x[], const float y[], int n)
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

SPAN_DECLARE(double) vec_dot_prod(const double x[], const double y[], int n)
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
SPAN_DECLARE(long double) vec_dot_prodl(const long double x[], const long double y[], int n)
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

SPAN_DECLARE(float) vec_circular_dot_prodf(const float x[], const float y[], int n, int pos)
{
    float z;

    z = vec_dot_prodf(&x[pos], &y[0], n - pos);
    z += vec_dot_prodf(&x[0], &y[n - pos], pos);
    return z;
}
/*- End of function --------------------------------------------------------*/

#define LMS_LEAK_RATE   0.9999f

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_SSE2)
SPAN_DECLARE(void) vec_lmsf(const float x[], float y[], int n, float error)
{
    int i;
    __m128 n1;
    __m128 n2;
    __m128 n3;
    __m128 n4;
 
    if ((i = n & ~3))
    {
        n3 = _mm_set1_ps(error);
        n4 = _mm_set1_ps(LMS_LEAK_RATE);
        for (i -= 4;  i >= 0;  i -= 4)
        {
            n1 = _mm_loadu_ps(x + i);
            n2 = _mm_loadu_ps(y + i);
            n1 = _mm_mul_ps(n1, n3);
            n2 = _mm_mul_ps(n2, n4);
            n1 = _mm_add_ps(n1, n2);
            _mm_storeu_ps(y + i, n1);
        }
    }
    /* Now deal with the last 1 to 3 elements, which don't fill an SSE2 register */
    switch (n & 3)
    {
    case 3:
        y[n - 3] = y[n - 3]*LMS_LEAK_RATE + x[n - 3]*error;
    case 2:
        y[n - 2] = y[n - 2]*LMS_LEAK_RATE + x[n - 2]*error;
    case 1:
        y[n - 1] = y[n - 1]*LMS_LEAK_RATE + x[n - 1]*error;
    }
}
#else
SPAN_DECLARE(void) vec_lmsf(const float x[], float y[], int n, float error)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        /* Leak a little to tame uncontrolled wandering */
        y[i] = y[i]*LMS_LEAK_RATE + x[i]*error;
    }
}
#endif
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) vec_circular_lmsf(const float x[], float y[], int n, int pos, float error)
{
    vec_lmsf(&x[pos], &y[0], n - pos, error);
    vec_lmsf(&x[0], &y[n - pos], pos, error);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
