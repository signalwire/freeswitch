/*
 * SpanDSP - a series of DSP components for telephony
 *
 * noise.c - A low complexity audio noise generator, suitable for
 *           real time generation (current AWGN, and Hoth)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/noise.h"

#include "spandsp/private/noise.h"

SPAN_DECLARE(int16_t) noise(noise_state_t *s)
{
    int32_t val;
    int i;

    /* The central limit theorem says if you add a few random numbers together,
       the result starts to look Gaussian. Quantities above 7 give diminishing
       returns. Quantites above 20 are exceedingly Gaussian. */
    val = 0;
    for (i = 0;  i < s->quality;  i++)
    {
        s->rndnum = 1664525U*s->rndnum + 1013904223U;
        val += ((int32_t) s->rndnum) >> 22;
    }
    if (s->class_of_noise == NOISE_CLASS_HOTH)
    {
        /* Hoth noise is room-like. It should be sculpted, at the high and low ends,
           and roll off at 5dB/octave across the main part of the band. However,
           merely rolling off at 6dB/octave across the band gets you close
           to the subjective effect. */
        s->state = (3*val + 5*s->state) >> 3;
        /* Bring the overall power level back to the pre-filtered level. This
           simple approx. leaves the signal about 0.35dB low. */
        val = s->state << 1;
    }
    return saturate((val*s->rms) >> 10);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(noise_state_t *) noise_init_dbov(noise_state_t *s, int seed, float level, int class_of_noise, int quality)
{
    float rms;

    if (s == NULL)
    {
        if ((s = (noise_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->rndnum = (uint32_t) seed;
    rms = 32768.0f*powf(10.0f, level/20.0f);
    if (quality < 4)
        s->quality = 4;
    else if (quality > 20)
        s->quality = 20;
    else
        s->quality = quality;
    if (class_of_noise == NOISE_CLASS_HOTH)
    {
        /* Allow for the gain of the filter */
        rms *= 1.043f;
    }
    s->rms = (int32_t) (rms*sqrtf(12.0f/s->quality));
    s->class_of_noise = class_of_noise;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(noise_state_t *) noise_init_dbm0(noise_state_t *s, int seed, float level, int class_of_noise, int quality)
{
    return noise_init_dbov(s, seed, (level - DBM0_MAX_POWER), class_of_noise, quality);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) noise_release(noise_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) noise_free(noise_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
