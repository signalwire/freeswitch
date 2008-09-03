/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_float.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: vector_float.h,v 1.10 2008/04/17 14:27:01 steveu Exp $
 */

#if !defined(_SPANDSP_VECTOR_FLOAT_H_)
#define _SPANDSP_VECTOR_FLOAT_H_

#if defined(__cplusplus)
extern "C"
{
#endif

void vec_copyf(float z[], const float x[], int n);

void vec_copy(double z[], const double x[], int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_copyl(long double z[], const long double x[], int n);
#endif

void vec_zerof(float z[], int n);

void vec_zero(double z[], int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_zerol(long double z[], int n);
#endif

void vec_setf(float z[], float x, int n);

void vec_set(double z[], double x, int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_setl(long double z[], long double x, int n);
#endif

void vec_addf(float z[], const float x[], const float y[], int n);

void vec_add(double z[], const double x[], const double y[], int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_addl(long double z[], const long double x[], const long double y[], int n);
#endif

void vec_scaled_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n);

void vec_scaled_add(double z[], const double x[], double x_scale, const double y[], double y_scale, int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_scaled_addl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n);
#endif

void vec_subf(float z[], const float x[], const float y[], int n);

void vec_sub(double z[], const double x[], const double y[], int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_subl(long double z[], const long double x[], const long double y[], int n);
#endif

void vec_scaled_subf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n);

void vec_scaled_sub(double z[], const double x[], double x_scale, const double y[], double y_scale, int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_scaled_subl(long double z[], const long double x[], long double x_scale, const long double y[], long double y_scale, int n);
#endif

void vec_scalar_mulf(float z[], const float x[], float y, int n);

void vec_scalar_mul(double z[], const double x[], double y, int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_scalar_mull(long double z[], const long double x[], long double y, int n);
#endif

void vec_mulf(float z[], const float x[], const float y[], int n);

void vec_mul(double z[], const double x[], const double y[], int n);

#if defined(HAVE_LONG_DOUBLE)
void vec_mull(long double z[], const long double x[], const long double y[], int n);
#endif

float vec_dot_prodf(const float x[], const float y[], int n);

double vec_dot_prod(const double x[], const double y[], int n);

#if defined(HAVE_LONG_DOUBLE)
long double vec_dot_prodl(const long double x[], const long double y[], int n);
#endif

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
