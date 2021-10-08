/*
 * SpanDSP - a series of DSP components for telephony
 *
 * math_fixed.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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
#include <fcntl.h>
#include <string.h>
#include <float.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>

#include "math_fixed_tables.h"

#include "spandsp/telephony.h"
#include "spandsp/bit_operations.h"
#include "spandsp/math_fixed.h"

#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(uint16_t) sqrtu32_u16(uint32_t x)
{
    uint16_t zz;
    uint16_t z;
    uint16_t i;

    z = 0;
    for (i = 0x8000;  i;  i >>= 1)
    {
        zz = z | i;
        if (((int32_t) zz*zz) <= x)
            z = zz;
    }
    return z;
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(uint16_t) fixed_reciprocal16(uint16_t x, int *shift)
{
    if (x == 0)
    {
        *shift = 0;
        return 0xFFFF;
    }
    *shift = 15 - top_bit(x);
    x <<= *shift;
    return fixed_reciprocal_table[((x + 0x80) >> 8) - 128];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) fixed_divide16(uint16_t y, uint16_t x)
{
    int shift;
    uint32_t z;
    uint16_t recip;

    if (x == 0)
        return 0xFFFF;
    recip = fixed_reciprocal16(x, &shift);
    z = (((uint32_t) y*recip) >> 15) << shift;
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) fixed_divide32(uint32_t y, uint16_t x)
{
    int shift;
    uint32_t z;
    uint16_t recip;

    if (x == 0)
        return 0xFFFF;
    recip = fixed_reciprocal16(x, &shift);
    z = (((uint32_t) y*recip) >> 15) << shift;
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) fixed_log10_16(uint16_t x)
{
    int shift;

    if (x == 0)
        return 0;
    shift = 14 - top_bit(x);
    x <<= shift;
    return (fixed_log10_table[((x + 0x40) >> 7) - 128] >> 3) - shift*1233;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) fixed_log10_32(uint32_t x)
{
    int shift;

    if (x == 0)
        return 0;
    shift = 30 - top_bit(x);
    x <<= shift;
    return (fixed_log10_table[((x + 0x400000) >> 23) - 128] >> 3) - shift*1233;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) fixed_sqrt16(uint16_t x)
{
    int shift;

    if (x == 0)
        return 0;
    shift = 14 - (top_bit(x) & ~1);
    x <<= shift;
    //return fixed_sqrt_table[(((x + 0x80) >> 8) & 0xFF) - 64] >> (shift >> 1);
    return fixed_sqrt_table[((x >> 8) & 0xFF) - 64] >> (shift >> 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) fixed_sqrt32(uint32_t x)
{
    int shift;

    if (x == 0)
        return 0;
    shift = 30 - (top_bit(x) & ~1);
    x <<= shift;
    //return fixed_sqrt_table[(((x + 0x800000) >> 24) & 0xFF) - 64] >> (shift >> 1);
    return fixed_sqrt_table[((x >> 24) & 0xFF) - 64] >> (shift >> 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) fixed_sin(uint16_t x)
{
    int step;
    int step_after;
    int16_t frac;
    int16_t z;

    step = (x & 0x3FFF) >> 6;
    frac = x & 0x3F;
    if ((x & 0x4000))
    {
        step = 256 - step;
        step_after = step - 1;
    }
    else
    {
        step_after = step + 1;
    }
    z = fixed_sine_table[step] + ((frac*(fixed_sine_table[step_after] - fixed_sine_table[step])) >> 6);
    if ((x & 0x8000))
        z = -z;
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) fixed_cos(uint16_t x)
{
    int step;
    int step_after;
    int16_t frac;
    int16_t z;

    x += 0x4000;
    step = (x & 0x3FFF) >> 6;
    frac = x & 0x3F;
    if ((x & 0x4000))
    {
        step = 256 - step;
        step_after = step - 1;
    }
    else
    {
        step_after = step + 1;
    }
    z = fixed_sine_table[step] + ((frac*(fixed_sine_table[step_after] - fixed_sine_table[step])) >> 6);
    if ((x & 0x8000))
        z = -z;
    return z;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) fixed_atan2(int16_t y, int16_t x)
{
    int16_t abs_x;
    int16_t abs_y;
    uint16_t angle;
    uint16_t recip;
    uint32_t z;
    int step;
    int shift;

    if (y == 0)
        return (x & 0x8000);
    if (x == 0)
        return ((y & 0x8000) | 0x4000);
    abs_x = abs(x);
    abs_y = abs(y);

    if (abs_y < abs_x)
    {
        recip = fixed_reciprocal16(abs_x, &shift);
        z = (((uint32_t) recip*abs_y) >> 15) << shift;
        step = z >> 7;
        angle = fixed_arctan_table[step];
    }
    else
    {
        recip = fixed_reciprocal16(abs_y, &shift);
        z = (((uint32_t) recip*abs_x) >> 15) << shift;
        step = z >> 7;
        angle = 0x4000 - fixed_arctan_table[step];
    }
    /* If we are in quadrant II or III, flip things around */
    if (x < 0)
        angle = 0x8000 - angle;
    /* If we are in quadrant III or IV, negate to return an
       answer in the full circle range. */
    if (y < 0)
        angle = -angle;
    return (uint16_t) angle;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
