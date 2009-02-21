/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dds.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 *
 * $Id: dds_int.c,v 1.16 2009/02/21 04:27:46 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"

#if !defined(M_PI)
# define M_PI           3.14159265358979323846  /* pi */
#endif

/* In a A-law or u-law channel, a 128 step sine table is adequate to keep the spectral
   mess due to the DDS at a similar level to the spectral mess due to the A-law or u-law
   compression. */
#define SLENK       7
#define DDS_STEPS   (1 << SLENK)
#define DDS_SHIFT   (32 - 2 - SLENK)

/* This is a simple set of direct digital synthesis (DDS) functions to generate sine
   waves. This version uses a 128 entry sin/cos table to cover one quadrant. */

static const int16_t sine_table[DDS_STEPS] =
{
       201,
       603,
      1005,
      1407,
      1809,
      2210,
      2611,
      3012,
      3412,
      3812,
      4211,
      4609,
      5007,
      5404,
      5800,
      6195,
      6590,
      6983,
      7376,
      7767,
      8157,
      8546,
      8933,
      9319,
      9704,
     10088,
     10469,
     10850,
     11228,
     11605,
     11980,
     12354,
     12725,
     13095,
     13463,
     13828,
     14192,
     14553,
     14912,
     15269,
     15624,
     15976,
     16326,
     16673,
     17018,
     17361,
     17700,
     18037,
     18372,
     18703,
     19032,
     19358,
     19681,
     20001,
     20318,
     20632,
     20943,
     21251,
     21555,
     21856,
     22154,
     22449,
     22740,
     23028,
     23312,
     23593,
     23870,
     24144,
     24414,
     24680,
     24943,
     25202,
     25457,
     25708,
     25956,
     26199,
     26439,
     26674,
     26906,
     27133,
     27357,
     27576,
     27791,
     28002,
     28209,
     28411,
     28610,
     28803,
     28993,
     29178,
     29359,
     29535,
     29707,
     29875,
     30038,
     30196,
     30350,
     30499,
     30644,
     30784,
     30920,
     31050,
     31177,
     31298,
     31415,
     31527,
     31634,
     31737,
     31834,
     31927,
     32015,
     32099,
     32177,
     32251,
     32319,
     32383,
     32442,
     32496,
     32546,
     32590,
     32629,
     32664,
     32693,
     32718,
     32738,
     32753,
     32762,
     32767,
};

SPAN_DECLARE(int32_t) dds_phase_rate(float frequency)
{
    return (int32_t) (frequency*65536.0f*65536.0f/SAMPLE_RATE);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) dds_frequency(int32_t phase_rate)
{
    return (float) phase_rate*(float) SAMPLE_RATE/(65536.0f*65536.0f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds_scaling_dbm0(float level)
{
    return (int16_t) (powf(10.0f, (level - DBM0_MAX_SINE_POWER)/20.0f)*32767.0f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds_scaling_dbov(float level)
{
    return (int16_t) (powf(10.0f, (level - DBOV_MAX_SINE_POWER)/20.0f)*32767.0f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds_lookup(uint32_t phase)
{
    uint32_t step;
    int16_t amp;

    phase >>= DDS_SHIFT;
    step = phase & (DDS_STEPS - 1);
    if ((phase & DDS_STEPS))
        step = (DDS_STEPS - 1) - step;
    amp = sine_table[step];
    if ((phase & (2*DDS_STEPS)))
    	amp = -amp;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds_offset(uint32_t phase_acc, int32_t phase_offset)
{
    return dds_lookup(phase_acc + phase_offset);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) dds_advance(uint32_t *phase_acc, int32_t phase_rate)
{
    *phase_acc += phase_rate;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds(uint32_t *phase_acc, int32_t phase_rate)
{
    int16_t amp;

    amp = dds_lookup(*phase_acc);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) dds_mod(uint32_t *phase_acc, int32_t phase_rate, int16_t scale, int32_t phase)
{
    int16_t amp;

    amp = (int16_t) (((int32_t) dds_lookup(*phase_acc + phase)*(int32_t) scale) >> 15);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi_t) dds_lookup_complexi(uint32_t phase)
{
    return complex_seti(dds_lookup(phase + (1 << 30)), dds_lookup(phase));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi_t) dds_complexi(uint32_t *phase_acc, int32_t phase_rate)
{
    complexi_t amp;

    amp = complex_seti(dds_lookup(*phase_acc + (1 << 30)), dds_lookup(*phase_acc));
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi_t) dds_complexi_mod(uint32_t *phase_acc, int32_t phase_rate, int16_t scale, int32_t phase)
{
    complexi_t amp;

    amp = complex_seti(((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*(int32_t) scale) >> 15,
                       ((int32_t) dds_lookup(*phase_acc + phase)*(int32_t) scale) >> 15);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi16_t) dds_lookup_complexi16(uint32_t phase)
{
    return complex_seti16(dds_lookup(phase + (1 << 30)), dds_lookup(phase));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi16_t) dds_complexi16(uint32_t *phase_acc, int32_t phase_rate)
{
    complexi16_t amp;

    amp = complex_seti16(dds_lookup(*phase_acc + (1 << 30)), dds_lookup(*phase_acc));
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi16_t) dds_complexi16_mod(uint32_t *phase_acc, int32_t phase_rate, int16_t scale, int32_t phase)
{
    complexi16_t amp;

    amp = complex_seti16((int16_t) (((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*(int32_t) scale) >> 15),
                         (int16_t) (((int32_t) dds_lookup(*phase_acc + phase)*(int32_t) scale) >> 15));
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi32_t) dds_lookup_complexi32(uint32_t phase)
{
    return complex_seti32(dds_lookup(phase + (1 << 30)), dds_lookup(phase));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi32_t) dds_complexi32(uint32_t *phase_acc, int32_t phase_rate)
{
    complexi32_t amp;

    amp = complex_seti32(dds_lookup(*phase_acc + (1 << 30)), dds_lookup(*phase_acc));
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexi32_t) dds_complexi32_mod(uint32_t *phase_acc, int32_t phase_rate, int16_t scale, int32_t phase)
{
    complexi32_t amp;

    amp = complex_seti32(((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*(int32_t) scale) >> 15,
                         ((int32_t) dds_lookup(*phase_acc + phase)*(int32_t) scale) >> 15);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
