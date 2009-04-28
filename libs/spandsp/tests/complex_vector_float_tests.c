/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_vector_float_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006,2008 Steve Underwood
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
 * $Id: complex_vector_float_tests.c,v 1.3 2009/04/26 07:00:39 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "spandsp.h"

static void cvec_mulf_dumb(complexf_t z[], const complexf_t x[], const complexf_t y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        z[i].re = x[i].re*y[i].re - x[i].im*y[i].im;
        z[i].im = x[i].re*y[i].im + x[i].im*y[i].re;
    }
}
/*- End of function --------------------------------------------------------*/

static int test_cvec_mulf(void)
{
    int i;
    complexf_t x[100];
    complexf_t y[100];
    complexf_t za[100];
    complexf_t zb[100];
    complexf_t ratio;

    for (i = 0;  i < 99;  i++)
    {
        x[i].re = rand();
        x[i].im = rand();
        y[i].re = rand();
        y[i].im = rand();
    }
    cvec_mulf(za, x, y, 99);
    cvec_mulf_dumb(zb, x, y, 99);
    for (i = 0;  i < 99;  i++)
        printf("(%f,%f) (%f,%f) (%f,%f)\n", za[i].re, za[i].im, x[i].re, x[i].im, y[i].re, y[i].im);
    for (i = 0;  i < 99;  i++)
    {
        ratio.re = za[i].re/zb[i].re;
        ratio.im = za[i].im/zb[i].im;
        if ((ratio.re < 0.9999  ||  ratio.re > 1.0001)
            ||
            (ratio.im < 0.9999  ||  ratio.im > 1.0001))
        {
            printf("cvec_mulf() - (%f,%f) (%f,%f)\n", za[i].re, za[i].im, zb[i].re, zb[i].im);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static complexf_t cvec_dot_prodf_dumb(const complexf_t x[], const complexf_t y[], int n)
{
    int i;
    complexf_t z;
    complexf_t z1;

    z = complex_setf(0.0f, 0.0f);
    for (i = 0;  i < n;  i++)
    {
        z1 = complex_mulf(&x[i], &y[i]);
        z = complex_addf(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static int test_cvec_dot_prodf(void)
{
    int i;
    complexf_t x[100];
    complexf_t y[100];
    complexf_t zsa;
    complexf_t zsb;
    complexf_t ratio;

    for (i = 0;  i < 99;  i++)
    {
        x[i].re = rand();
        x[i].im = rand();
        y[i].re = rand();
        y[i].im = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        zsa = cvec_dot_prodf(x, y, i);
        zsb = cvec_dot_prodf_dumb(x, y, i);
        ratio.re = zsa.re/zsb.re;
        ratio.im = zsa.im/zsb.im;
        if ((ratio.re < 0.9999  ||  ratio.re > 1.0001)
            ||
            (ratio.im < 0.9999  ||  ratio.im > 1.0001))
        {
            printf("cvec_dot_prodf() - (%f,%f) (%f,%f)\n", zsa.re, zsa.im, zsb.re, zsb.im);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    test_cvec_mulf();
    test_cvec_dot_prodf();

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
