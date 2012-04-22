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

/* In a A-law or u-law channel, a fairly coarse step sine table is adequate to keep the spectral
   mess due to the DDS at a similar level to the spectral mess due to the A-law or u-law
   compression. */
#define SLENK       8
#define DDS_STEPS   (1 << SLENK)
#define DDS_SHIFT   (32 - 2 - SLENK)

/* This is a simple set of direct digital synthesis (DDS) functions to generate sine
   waves. This version uses a 256 entry sin/cos table to cover one quadrant. */

static const int16_t sine_table[DDS_STEPS + 1] =
{
         0,
       201,
       402,
       603,
       804,
      1005,
      1206,
      1407,
      1608,
      1809,
      2009,
      2210,
      2410,
      2611,
      2811,
      3012,
      3212,
      3412,
      3612,
      3811,
      4011,
      4210,
      4410,
      4609,
      4808,
      5007,
      5205,
      5404,
      5602,
      5800,
      5998,
      6195,
      6393,
      6590,
      6786,
      6983,
      7179,
      7375,
      7571,
      7767,
      7962,
      8157,
      8351,
      8545,
      8739,
      8933,
      9126,
      9319,
      9512,
      9704,
      9896,
     10087,
     10278,
     10469,
     10659,
     10849,
     11039,
     11228,
     11417,
     11605,
     11793,
     11980,
     12167,
     12353,
     12539,
     12725,
     12910,
     13094,
     13279,
     13462,
     13645,
     13828,
     14010,
     14191,
     14372,
     14553,
     14732,
     14912,
     15090,
     15269,
     15446,
     15623,
     15800,
     15976,
     16151,
     16325,
     16499,
     16673,
     16846,
     17018,
     17189,
     17360,
     17530,
     17700,
     17869,
     18037,
     18204,
     18371,
     18537,
     18703,
     18868,
     19032,
     19195,
     19357,
     19519,
     19680,
     19841,
     20000,
     20159,
     20317,
     20475,
     20631,
     20787,
     20942,
     21096,
     21250,
     21403,
     21554,
     21705,
     21856,
     22005,
     22154,
     22301,
     22448,
     22594,
     22739,
     22884,
     23027,
     23170,
     23311,
     23452,
     23592,
     23731,
     23870,
     24007,
     24143,
     24279,
     24413,
     24547,
     24680,
     24811,
     24942,
     25072,
     25201,
     25329,
     25456,
     25582,
     25708,
     25832,
     25955,
     26077,
     26198,
     26319,
     26438,
     26556,
     26674,
     26790,
     26905,
     27019,
     27133,
     27245,
     27356,
     27466,
     27575,
     27683,
     27790,
     27896,
     28001,
     28105,
     28208,
     28310,
     28411,
     28510,
     28609,
     28706,
     28803,
     28898,
     28992,
     29085,
     29177,
     29268,
     29358,
     29447,
     29534,
     29621,
     29706,
     29791,
     29874,
     29956,
     30037,
     30117,
     30195,
     30273,
     30349,
     30424,
     30498,
     30571,
     30643,
     30714,
     30783,
     30852,
     30919,
     30985,
     31050,
     31113,
     31176,
     31237,
     31297,
     31356,
     31414,
     31470,
     31526,
     31580,
     31633,
     31685,
     31736,
     31785,
     31833,
     31880,
     31926,
     31971,
     32014,
     32057,
     32098,
     32137,
     32176,
     32213,
     32250,
     32285,
     32318,
     32351,
     32382,
     32412,
     32441,
     32469,
     32495,
     32521,
     32545,
     32567,
     32589,
     32609,
     32628,
     32646,
     32663,
     32678,
     32692,
     32705,
     32717,
     32728,
     32737,
     32745,
     32752,
     32757,
     32761,
     32765,
     32766,
     32767
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
        step = DDS_STEPS - step;
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

    amp = (int16_t) (((int32_t) dds_lookup(*phase_acc + phase)*scale) >> 15);
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

    amp = complex_seti(((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*scale) >> 15,
                       ((int32_t) dds_lookup(*phase_acc + phase)*scale) >> 15);
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

    amp = complex_seti16((int16_t) (((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*scale) >> 15),
                         (int16_t) (((int32_t) dds_lookup(*phase_acc + phase)*scale) >> 15));
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

    amp = complex_seti32(((int32_t) dds_lookup(*phase_acc + phase + (1 << 30))*scale) >> 15,
                         ((int32_t) dds_lookup(*phase_acc + phase)*scale) >> 15);
    *phase_acc += phase_rate;
    return amp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
