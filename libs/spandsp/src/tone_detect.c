/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.c - General telephony tone detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001-2003, 2005 Steve Underwood
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
 * $Id: tone_detect.c,v 1.53 2009/04/12 09:12:10 steveu Exp $
 */
 
/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"

#include "spandsp/private/tone_detect.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

SPAN_DECLARE(void) make_goertzel_descriptor(goertzel_descriptor_t *t, float freq, int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    t->fac = 16383.0f*2.0f*cosf(2.0f*M_PI*(freq/(float) SAMPLE_RATE));
#else
    t->fac = 2.0f*cosf(2.0f*M_PI*(freq/(float) SAMPLE_RATE));
#endif
    t->samples = samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(goertzel_state_t *) goertzel_init(goertzel_state_t *s,
                                               goertzel_descriptor_t *t)
{
    if (s == NULL)
    {
        if ((s = (goertzel_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
#if defined(SPANDSP_USE_FIXED_POINT)
    s->v2 =
    s->v3 = 0;
#else
    s->v2 =
    s->v3 = 0.0f;
#endif
    s->fac = t->fac;
    s->samples = t->samples;
    s->current_sample = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) goertzel_release(goertzel_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) goertzel_free(goertzel_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) goertzel_reset(goertzel_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    s->v2 =
    s->v3 = 0;
#else
    s->v2 =
    s->v3 = 0.0f;
#endif
    s->current_sample = 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) goertzel_update(goertzel_state_t *s,
                                  const int16_t amp[],
                                  int samples)
{
    int i;
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t x;
    int16_t v1;
#else
    float v1;
#endif

    if (samples > s->samples - s->current_sample)
        samples = s->samples - s->current_sample;
    for (i = 0;  i < samples;  i++)
    {
        v1 = s->v2;
        s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT)
        x = (((int32_t) s->fac*s->v2) >> 14);
        /* Scale down the input signal to avoid overflows. 9 bits is enough to
           monitor the signals of interest with adequate dynamic range and
           resolution. In telephony we generally only start with 13 or 14 bits,
           anyway. */
        s->v3 = x - v1 + (amp[i] >> 7);
#else
        s->v3 = s->fac*s->v2 - v1 + amp[i];
#endif
    }
    s->current_sample += samples;
    return samples;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(int32_t) goertzel_result(goertzel_state_t *s)
#else
SPAN_DECLARE(float) goertzel_result(goertzel_state_t *s)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t v1;
    int32_t x;
    int32_t y;
#else
    float v1;
#endif

    /* Push a zero through the process to finish things off. */
    v1 = s->v2;
    s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT)
    x = (((int32_t) s->fac*s->v2) >> 14);
    s->v3 = x - v1;
#else
    s->v3 = s->fac*s->v2 - v1;
#endif
    /* Now calculate the non-recursive side of the filter. */
    /* The result here is not scaled down to allow for the magnification
       effect of the filter (the usual DFT magnification effect). */
#if defined(SPANDSP_USE_FIXED_POINT)
    x = (int32_t) s->v3*s->v3;
    y = (int32_t) s->v2*s->v2;
    x += y;
    y = ((int32_t) s->v3*s->fac) >> 14;
    y *= s->v2;
    x -= y;
    x <<= 1;
    goertzel_reset(s);
    /* The number returned in a floating point build will be 16384^2 times
       as big as for a fixed point build, due to the 14 bit shifts
       (or the square of the 7 bit shifts, depending how you look at it). */
    return x;
#else
    v1 = s->v3*s->v3 + s->v2*s->v2 - s->v2*s->v3*s->fac;
    v1 *= 2.0;
    goertzel_reset(s);
    return v1;
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexf_t) periodogram(const complexf_t coeffs[], const complexf_t amp[], int len)
{
    complexf_t sum;
    complexf_t diff;
    complexf_t x;
    int i;

    x = complex_setf(0.0f, 0.0f);
    for (i = 0;  i < len/2;  i++)
    {
        sum = complex_addf(&amp[i], &amp[len - 1 - i]);
        diff = complex_subf(&amp[i], &amp[len - 1 - i]);
        x.re += (coeffs[i].re*sum.re - coeffs[i].im*diff.im);
        x.im += (coeffs[i].re*sum.im + coeffs[i].im*diff.re);
    }
    return x;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) periodogram_prepare(complexf_t sum[], complexf_t diff[], const complexf_t amp[], int len)
{
    int i;

    for (i = 0;  i < len/2;  i++)
    {
        sum[i] = complex_addf(&amp[i], &amp[len - 1 - i]);
        diff[i] = complex_subf(&amp[i], &amp[len - 1 - i]);
    }
    return len/2;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexf_t) periodogram_apply(const complexf_t coeffs[], const complexf_t sum[], const complexf_t diff[], int len)
{
    complexf_t x;
    int i;

    x = complex_setf(0.0f, 0.0f);
    for (i = 0;  i < len/2;  i++)
    {
        x.re += (coeffs[i].re*sum[i].re - coeffs[i].im*diff[i].im);
        x.im += (coeffs[i].re*sum[i].im + coeffs[i].im*diff[i].re);
    }
    return x;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) periodogram_generate_coeffs(complexf_t coeffs[], float freq, int sample_rate, int window_len)
{
    float window;
    float sum;
    float x;
    int i;

    sum = 0.0f;
    for (i = 0;  i < window_len/2;  i++)
    {
        /* Apply a Hamming window as we go */
        window = 0.53836f - 0.46164f*cosf(2.0f*3.1415926535f*i/(window_len - 1.0f));
        x = (i - window_len/2.0f + 0.5f)*freq*2.0f*3.1415926535f/sample_rate;
        coeffs[i].re = cosf(x)*window;
        coeffs[i].im = -sinf(x)*window;
        sum += window;
    }
    /* Rescale for unity gain in the periodogram. The 2.0 factor is to allow for the full window,
       rather than just the half over which we have summed the coefficients. */
    sum = 1.0f/(2.0f*sum);
    for (i = 0;  i < window_len/2;  i++)
    {
        coeffs[i].re *= sum;
        coeffs[i].im *= sum;
    }
    return window_len/2;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) periodogram_generate_phase_offset(complexf_t *offset, float freq, int sample_rate, int interval)
{
    float x;

    /* The phase offset is how far the phase rotates in one frame */
    x = 2.0f*3.1415926535f*(float) interval/(float) sample_rate;
    offset->re = cosf(freq*x);
    offset->im = sinf(freq*x);
    return 1.0f/x;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) periodogram_freq_error(const complexf_t *phase_offset, float scale, const complexf_t *last_result, const complexf_t *result)
{
    complexf_t prediction;

    /* Rotate the last result by the expected phasor offset to the current result. Then
       find the difference between that predicted position, and the actual one. When
       scaled by the current signal level, this gives us the frequency error. */
    prediction = complex_mulf(last_result, phase_offset);
    return scale*(result->im*prediction.re - result->re*prediction.im)/(result->re*result->re + result->im*result->im);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
