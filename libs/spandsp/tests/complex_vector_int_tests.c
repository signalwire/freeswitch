/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_vector_int_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: complex_vector_int_tests.c,v 1.2 2009/04/26 07:00:39 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "spandsp.h"

static complexi32_t cvec_dot_prodi16_dumb(const complexi16_t x[], const complexi16_t y[], int n)
{
    complexi32_t z;
    int i;

    z = complex_seti32(0, 0);
    for (i = 0;  i < n;  i++)
    {
        z.re += ((int32_t) x[i].re*(int32_t) y[i].re - (int32_t) x[i].im*(int32_t) y[i].im);
        z.im += ((int32_t) x[i].re*(int32_t) y[i].im + (int32_t) x[i].im*(int32_t) y[i].re);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static int test_cvec_dot_prodi16(void)
{
    int i;
    complexi32_t za;
    complexi32_t zb;
    complexi16_t x[99];
    complexi16_t y[99];

    for (i = 0;  i < 99;  i++)
    {
        x[i].re = rand();
        x[i].im = rand();
        y[i].re = rand();
        y[i].im = rand();
    }

    for (i = 1;  i < 99;  i++)
    {
        za = cvec_dot_prodi16(x, y, i);
        zb = cvec_dot_prodi16_dumb(x, y, i);
        if (za.re != zb.re  ||  za.im != zb.im)
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_cvec_circular_dot_prodi16(void)
{
    int i;
    int j;
    int pos;
    int len;
    complexi32_t za;
    complexi32_t zb;
    complexi16_t x[99];
    complexi16_t y[99];

    /* Verify that we can do circular sample buffer "dot" linear coefficient buffer
       operations properly, by doing two sub-dot products. */
    for (i = 0;  i < 99;  i++)
    {
        x[i].re = rand();
        x[i].im = rand();
        y[i].re = rand();
        y[i].im = rand();
    }

    len = 95;
    for (pos = 0;  pos < len;  pos++)
    {
        za = cvec_circular_dot_prodi16(x, y, len, pos);
        zb = complex_seti32(0, 0);
        for (i = 0;  i < len;  i++)
        {
            j = (pos + i) % len;
            zb.re += ((int32_t) x[j].re*(int32_t) y[i].re - (int32_t) x[j].im*(int32_t) y[i].im);
            zb.im += ((int32_t) x[j].re*(int32_t) y[i].im + (int32_t) x[j].im*(int32_t) y[i].re);
        }

        if (za.re != zb.re  ||  za.im != zb.im)
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    test_cvec_dot_prodi16();
    test_cvec_circular_dot_prodi16();

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
