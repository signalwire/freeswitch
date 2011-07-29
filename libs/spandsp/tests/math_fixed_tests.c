/*
 * SpanDSP - a series of DSP components for telephony
 *
 * math_fixed_tests.c - Test the fixed point math functions.
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

/*! \file */

/*! \page math_fixed_tests_page Fixed point math function tests
\section math_fixed_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

static void fixed_reciprocal16_tests(void)
{
    int x;
    uint16_t yu16;
    uint32_t yu32;
    double z;
    double ratio;
    double max;
    double min;
    int shift;

    /* The reciprocal should be with about 0.4%, except for 0, which is obviously a
       special case. */
    printf("fixed_reciprocal16() function tests\n");
    if (fixed_reciprocal16(0, &shift) != 0xFFFF  ||  shift != 0)
    {
        printf("Test failed\n");
        exit(2);
    }
    min = 999999.0;
    max = -999999.0;
    for (x = 1;  x < 65536;  x++)
    {
        yu16 = fixed_reciprocal16(x, &shift);
        yu32 = ((uint32_t) yu16) << shift;
        z = 32768.0*32768.0/x;
        ratio = z/yu32;
        //printf("%6x %15d %f %f\n", x, yu32, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < 0.996  ||  ratio > 1.004)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_divide16_tests(void)
{
    int x;
    int y;
    uint16_t yu16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_divide16() function tests\n");
    if (fixed_divide16(12345, 0) != 0xFFFF)
    {
        printf("Test failed\n");
        exit(2);
    }
    min = 999999.0;
    max = -999999.0;
    for (y = 32;  y < 65536;  y += 16)
    {
        for (x = y*16;  x < 65536;  x += 16)
        {
            yu16 = fixed_divide16(y, x);
            z = 32768.0*y/x;
            ratio = z/yu16;
            //printf("%6d %6d %6d %f %f\n", x, y, yu16, z, ratio);
            if (ratio < min)
                min = ratio;
            if (ratio > max)
                max = ratio;
            if (ratio < 0.996  ||  ratio > 1.07)
            {
                printf("Test failed\n");
                exit(2);
            }
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_divide32_tests(void)
{
    uint32_t xu32;
    uint16_t yu16;
    uint32_t yu32;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_divide32() function tests\n");
    if (fixed_divide32(12345, 0) != 0xFFFF)
    {
        printf("Test failed\n");
        exit(2);
    }
    min = 999999.0;
    max = -999999.0;
    for (yu32 = 32;  yu32 < 65536;  yu32 += 16)
    {
        for (xu32 = yu32*16;  xu32 < 65535;  xu32 += 16)
        {
            yu16 = fixed_divide32(yu32, xu32);
            z = 32768.0*yu32/xu32;
            ratio = z/yu16;
            //printf("%6u %6u %6u %f %f\n", xu32, yu32, yu16, z, ratio);
            if (ratio < min)
                min = ratio;
            if (ratio > max)
                max = ratio;
            if (ratio < 0.996  ||  ratio > 1.07)
            {
                printf("Test failed\n");
                exit(2);
            }
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_log10_16_tests(void)
{
    int x;
    int16_t yi16;
    double z;
    double ratio;
    double max;
    double min;

    printf("Log10 16 bit function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (x = 1;  x < 32500;  x++)
    {
        yi16 = fixed_log10_16(x);
        z = 4096.0*log10(x/32768.0);
        ratio = z - yi16;
        //printf("%6d %15d %f %f\n", x, yi16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < -8.0  ||  ratio > 8.0)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_log10_32_tests(void)
{
    int x;
    int32_t yi32;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_log10_32() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (x = 1;  x < 32767*65536;  x += 0x4000)
    {
        yi32 = fixed_log10_32(x);
        z = 4096.0*log10(x/(32768.0*65536.0));
        ratio = z - yi32;
        //printf("%6d %15d %f %f\n", x, yi32, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < -8.0  ||  ratio > 8.0)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_sqrt16_tests(void)
{
    int x;
    uint16_t yu16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_sqrt16() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (x = 1;  x < 65536;  x++)
    {
        yu16 = fixed_sqrt16(x);
        z = sqrt(x)*256.0;
        ratio = z/yu16;
        //printf("%6d %6d %f %f\n", x, yu16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < 0.999  ||  ratio > 1.008)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_sqrt32_tests(void)
{
    uint32_t xu32;
    uint16_t yu16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_sqrt32() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (xu32 = 20000;  xu32 < 0xFFFF0000;  xu32 += 10000)
    {
        yu16 = fixed_sqrt32(xu32);
        z = sqrt(xu32);
        ratio = z/yu16;
        //printf("%10u %6d %f %f\n", xu32, yu16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < 0.999  ||  ratio > 1.009)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_sin_tests(void)
{
    int x;
    int16_t yi16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_sin() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (x = 0;  x < 65536;  x++)
    {
        yi16 = fixed_sin(x);
        z = sin(2.0*3.1415926535*x/65536.0)*32768.0;
        ratio = z - yi16;
        //printf("%6d %6d %f %f\n", x, yi16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < -2.0  ||  ratio > 2.0)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_cos_tests(void)
{
    int x;
    int16_t yi16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_cos() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (x = 0;  x < 65536;  x++)
    {
        yi16 = fixed_cos(x);
        z = cos(2.0*3.1415926535*x/65536.0)*32768.0;
        ratio = z - yi16;
        //printf("%6d %6d %f %f\n", x, yi16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < -2.0  ||  ratio > 2.0)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

static void fixed_atan2_tests(void)
{
    int i;
    int x;
    int y;
    uint16_t yu16;
    double z;
    double ratio;
    double max;
    double min;

    printf("fixed_atan2() function tests\n");
    min = 999999.0;
    max = -999999.0;
    for (i = 0;  i < 65536;  i++)
    {
        x = 16384.0*cos(i*2.0*3.1415926535/65536.0);
        y = 16384.0*sin(i*2.0*3.1415926535/65536.0);
        yu16 = fixed_atan2(y, x);
        z = atan2(y/32768.0, x/32768.0)*65536.0/(2.0*3.1415926535);
        if (z < 0.0)
            z += 65536.0;
        ratio = z - yu16;
        //printf("%6d %6d %6d %6d %f %f\n", i, x, y, yu16, z, ratio);
        if (ratio < min)
            min = ratio;
        if (ratio > max)
            max = ratio;
        if (ratio < -43.0  ||  ratio > 43.0)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    printf("min %f, max %f\n", min, max);
    printf("Test passed\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    fixed_reciprocal16_tests();
    fixed_divide16_tests();
    fixed_divide32_tests();
    fixed_log10_16_tests();
    fixed_log10_32_tests();
    fixed_sqrt16_tests();
    fixed_sqrt32_tests();
    fixed_sin_tests();
    fixed_cos_tests();
    fixed_atan2_tests();

    printf("Tests passed\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
