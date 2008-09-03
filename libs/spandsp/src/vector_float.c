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
 * $Id: vector_float.c,v 1.11 2008/07/02 14:48:26 steveu Exp $
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
        z[i] = 0.0;
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

    z = 0.0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
