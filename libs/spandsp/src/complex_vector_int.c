/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_vector_int.c - Integer complex vector arithmetic routines.
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
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"

SPAN_DECLARE(complexi32_t) cvec_dot_prodi16(const complexi16_t x[], const complexi16_t y[], int n)
{
    int i;
    complexi32_t z;

    z = complex_seti32(0, 0);
    for (i = 0;  i < n;  i++)
    {
        z.re += ((int32_t) x[i].re*(int32_t) y[i].re - (int32_t) x[i].im*(int32_t) y[i].im);
        z.im += ((int32_t) x[i].re*(int32_t) y[i].im + (int32_t) x[i].im*(int32_t) y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi32_t) cvec_dot_prodi32(const complexi32_t x[], const complexi32_t y[], int n)
{
    int i;
    complexi32_t z;

    z = complex_seti32(0, 0);
    for (i = 0;  i < n;  i++)
    {
        z.re += (x[i].re*y[i].re - x[i].im*y[i].im);
        z.im += (x[i].re*y[i].im + x[i].im*y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi32_t) cvec_circular_dot_prodi16(const complexi16_t x[], const complexi16_t y[], int n, int pos)
{
    complexi32_t z;
    complexi32_t z1;

    z = cvec_dot_prodi16(&x[pos], &y[0], n - pos);
    z1 = cvec_dot_prodi16(&x[0], &y[n - pos], pos);
    z = complex_addi32(&z, &z1);
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) cvec_lmsi16(const complexi16_t x[], complexi16_t y[], int n, const complexi16_t *error)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        y[i].re += (int16_t) (((int32_t) x[i].im*(int32_t) error->im + (int32_t) x[i].re*(int32_t) error->re) >> 12);
        y[i].im += (int16_t) (((int32_t) x[i].re*(int32_t) error->im - (int32_t) x[i].im*(int32_t) error->re) >> 12);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) cvec_circular_lmsi16(const complexi16_t x[], complexi16_t y[], int n, int pos, const complexi16_t *error)
{
    cvec_lmsi16(&x[pos], &y[0], n - pos, error);
    cvec_lmsi16(&x[0], &y[n - pos], pos, error);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
