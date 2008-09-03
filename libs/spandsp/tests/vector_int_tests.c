/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_int_tests.c
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
 * $Id: vector_int_tests.c,v 1.8 2008/05/13 13:17:27 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <audiofile.h>

#include "spandsp.h"

static int32_t vec_dot_prodi16_dumb(const int16_t x[], const int16_t y[], int n)
{
    int32_t z;
    int i;

    z = 0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return  z;
}
/*- End of function --------------------------------------------------------*/

static int32_t vec_min_maxi16_dumb(const int16_t x[], int n, int16_t out[])
{
    int i;
    int16_t min;
    int16_t max;
    int16_t temp;
    int32_t z;
    
    max = INT16_MIN;
    min = INT16_MAX;
    for (i = 0;  i < n;  i++)
    {
        temp = x[i];
        if (temp > max)
            max = temp;
        /*endif*/
        if (temp < min)
            min = temp;
        /*endif*/
    }
    /*endfor*/
    out[0] = max;
    out[1] = min;
    z = abs(min);
    if (z > max)
        return z;
    return max;
}
/*- End of function --------------------------------------------------------*/
    
int main(int argc, char *argv[])
{
    int i;
    int32_t za;
    int32_t zb;
    int16_t x[99];
    int16_t y[99];
    int16_t outa[2];
    int16_t outb[2];

    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }

    za = vec_dot_prodi16(x, y, 99);
    zb = vec_dot_prodi16_dumb(x, y, 99);
    if (za != zb)
    {
        printf("Tests failed\n");
        exit(2);
    }

    x[42] = -32768;
    za = vec_min_maxi16_dumb(x, 99, outa);
    zb = vec_min_maxi16(x, 99, outb);
    if (za != zb
        ||
        outa[0] != outb[0]
        ||
        outa[1] != outb[1])
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
