/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: complex_tests.c,v 1.2 2009/01/28 03:41:27 steveu Exp $
 */

/*! \page complex_tests_page Complex arithmetic tests
\section complex_tests_page_sec_1 What does it do?

\section complex_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    complexf_t fa;
    complexf_t fb;
    complexf_t fz;
    complexi16_t i16a;
    complexi16_t i16b;
    complexi16_t i16z;
#if 0
    complexi32_t i32a;
    complexi32_t i32b;
    complexi32_t i32z;
#endif

    fa = complex_setf(0.5f, 0.25f);
    fb = complex_setf(0.25f, 0.5f);
    fz = complex_mulf(&fa, &fb);
    printf("(%f, %f) * (%f, %f) => (%f, %f)\n", fa.re, fa.im,  fb.re, fb.im,  fz.re, fz.im);

    i16a = complex_seti16(16383, 8191);
    i16b = complex_seti16(8191, 16383);
    i16z = complex_mul_q1_15(&i16a, &i16b);
    printf("(%f, %f) * (%f, %f) => (%f, %f)\n", i16a.re/32768.0, i16a.im/32768.0,  i16b.re/32768.0, i16b.im/32768.0,  i16z.re/32768.0, i16z.im/32768.0);

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
