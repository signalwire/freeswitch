/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_cielab_luts.c - Create the look up tables for CIELab colour management
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

/*! \file */

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <math.h>

typedef struct
{
    float L;
    float a;
    float b;
} cielab_t;

int main(int argc, char *argv[])
{
    float r;
    uint8_t srgb;
    int i;

    printf("static const float srgb_to_linear[256] =\n");
    printf("{\n");
    for (i = 0;  i < 256;  i++)
    {
        /* Start with "i" as the sRGB value */
        r = i/256.0f;

        /* sRGB to Linear RGB */
        r = (r > 0.04045f)  ?  powf((r + 0.055f)/1.055f, 2.4f)  :  r/12.92f;

        printf((i < 255)  ?  "    %f,\n"  :  "    %f\n", r);
    }
    printf("};\n");

    printf("static const uint8_t linear_to_srgb[4096] =\n");
    printf("{\n");
    for (i = 0;  i < 4096;  i++)
    {
        /* Start with "i" as the linear RGB value */
        /* Linear RGB to sRGB */
        r = i/4096.0f;

        r = (r > 0.0031308f)  ?  (1.055f*powf(r, 1.0f/2.4f) - 0.055f)  :  r*12.92f;

        r = floorf(r*256.0f);

        srgb = (uint8_t) ((r < 0)  ?  0  :  (r <= 255)  ?  r  :  255);

        printf((i < 4095)  ?  "    %d,\n"  :  "    %d\n", srgb);
    }
    printf("};\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
