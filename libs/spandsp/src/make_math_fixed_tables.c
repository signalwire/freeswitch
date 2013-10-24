/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_fixed_point_math_tables.c - Generate lookup tables for some of the
 *                                  fixed point maths functions.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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
 */

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <math.h>

int main(int argc, char *argv[])
{
    int i;
    double val;
    int ival;

    printf("static const uint16_t fixed_reciprocal_table[129] =\n");
    printf("{\n");
    for (i = 0;  i < 129;  i++)
    {
        val = 32768.0*128.0/(128 + i) + 0.5;
        ival = (int) val;
        if (i < 128)
            printf("    %6d,\n", ival);
        else
            printf("    %6d\n", ival);
    }
    printf("};\n\n");

    printf("static const uint16_t fixed_sqrt_table[193] =\n");
    printf("{\n");
    for (i = 64;  i <= 256;  i++)
    {
        ival = (int) (sqrt(i/256.0)*65536.0 + 0.5);
        if (ival > 65535)
            ival = 65535;
        if (i < 256)
            printf("    %6d,\n", ival);
        else
            printf("    %6d\n", ival);
    }
    printf("};\n\n");

    printf("static const int16_t fixed_log10_table[129] =\n");
    printf("{\n");
    for (i = 128;  i <= 256;  i++)
    {
        ival = (int) (log10(i/256.0)*32768.0 - 0.5);
        if (i <= 255)
            printf("    %6d,\n", ival);
        else
            printf("    %6d\n", ival);
    }
    printf("};\n\n");

    printf("static const int16_t fixed_sine_table[257] =\n");
    printf("{\n");
    for (i = 0;  i <= 256;  i++)
    {
        val = sin(i*3.1415926535/512.0)*32768.0;
        ival = (int) (val + 0.5);
        if (ival > 32767)
            ival = 32767;
        if (i <= 255)
            printf("    %6d,\n", ival);
        else
            printf("    %6d\n", ival);
    }
    printf("};\n\n");

    printf("static const uint16_t fixed_arctan_table[257] =\n");
    printf("{\n");
    for (i = 0;  i <= 256;  i++)
    {
        val = atan(i/256.0)*65536.0/(2.0*3.1415926535);
        ival = (int) (val + 0.5);
        /* Nudge the result away from zero, so things sit consistently on
           the correct side of the axes. */
        if (ival == 0)
            ival = 1;
        if (i <= 255)
            printf("    %6d,\n", ival);
        else
            printf("    %6d\n", ival);
    }
    printf("};\n\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
