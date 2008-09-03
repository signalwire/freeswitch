/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_float_tests.c
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
 * $Id: vector_float_tests.c,v 1.9 2008/05/13 13:17:27 steveu Exp $
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

static double vec_dot_prod_dumb(const double x[], const double y[], int n)
{
    int i;
    double z;

    z = 0.0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    double x[100];
    double y[100];
    double zsa;
    double zsb;

    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    zsa = vec_dot_prod(x, y, 99);
    zsb = vec_dot_prod_dumb(x, y, 99);
    if (zsa != zsb)
    {
        printf("Tests failed\n");
        exit(2);
    }

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
