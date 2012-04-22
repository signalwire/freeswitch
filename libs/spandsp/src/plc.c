/*
 * SpanDSP - a series of DSP components for telephony
 *
 * plc.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <limits.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/plc.h"

/* We do a straight line fade to zero volume in 50ms when we are filling in for missing data. */
#define ATTENUATION_INCREMENT       0.0025f     /* Attenuation per sample */

static void save_history(plc_state_t *s, int16_t *buf, int len)
{
    if (len >= PLC_HISTORY_LEN)
    {
        /* Just keep the last part of the new data, starting at the beginning of the buffer */
        memcpy(s->history, buf + len - PLC_HISTORY_LEN, sizeof(int16_t)*PLC_HISTORY_LEN);
        s->buf_ptr = 0;
        return;
    }
    if (s->buf_ptr + len > PLC_HISTORY_LEN)
    {
        /* Wraps around - must break into two sections */
        memcpy(s->history + s->buf_ptr, buf, sizeof(int16_t)*(PLC_HISTORY_LEN - s->buf_ptr));
        len -= (PLC_HISTORY_LEN - s->buf_ptr);
        memcpy(s->history, buf + (PLC_HISTORY_LEN - s->buf_ptr), sizeof(int16_t)*len);
        s->buf_ptr = len;
        return;
    }
    /* Can use just one section */
    memcpy(s->history + s->buf_ptr, buf, sizeof(int16_t)*len);
    s->buf_ptr += len;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void normalise_history(plc_state_t *s)
{
    int16_t tmp[PLC_HISTORY_LEN];

    if (s->buf_ptr == 0)
        return;
    memcpy(tmp, s->history, sizeof(int16_t)*s->buf_ptr);
    memcpy(s->history, s->history + s->buf_ptr, sizeof(int16_t)*(PLC_HISTORY_LEN - s->buf_ptr));
    memcpy(s->history + PLC_HISTORY_LEN - s->buf_ptr, tmp, sizeof(int16_t)*s->buf_ptr);
    s->buf_ptr = 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int amdf_pitch(int min_pitch, int max_pitch, int16_t amp[], int len)
{
    int i;
    int j;
    int acc;
    int min_acc;
    int pitch;

    pitch = min_pitch;
    min_acc = INT_MAX;
    for (i = max_pitch;  i <= min_pitch;  i++)
    {
        acc = 0;
        for (j = 0;  j < len;  j++)
            acc += abs(amp[i + j] - amp[j]);
        if (acc < min_acc)
        {
            min_acc = acc;
            pitch = i;
        }
    }
    return pitch;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) plc_rx(plc_state_t *s, int16_t amp[], int len)
{
    int i;
    int pitch_overlap;
    float old_step;
    float new_step;
    float old_weight;
    float new_weight;
    float gain;
    
    if (s->missing_samples)
    {
        /* Although we have a real signal, we need to smooth it to fit well
           with the synthetic signal we used for the previous block */

        /* The start of the real data is overlapped with the next 1/4 cycle
           of the synthetic data. */
        pitch_overlap = s->pitch >> 2;
        if (pitch_overlap > len)
            pitch_overlap = len;
        gain = 1.0f - s->missing_samples*ATTENUATION_INCREMENT;
        if (gain < 0.0f)
            gain = 0.0f;
        new_step = 1.0f/pitch_overlap;
        old_step = new_step*gain;
        new_weight = new_step;
        old_weight = (1.0f - new_step)*gain;
        for (i = 0;  i < pitch_overlap;  i++)
        {
            amp[i] = fsaturate(old_weight*s->pitchbuf[s->pitch_offset] + new_weight*amp[i]);
            if (++s->pitch_offset >= s->pitch)
                s->pitch_offset = 0;
            new_weight += new_step;
            old_weight -= old_step;
            if (old_weight < 0.0f)
                old_weight = 0.0f;
        }
        s->missing_samples = 0;
    }
    save_history(s, amp, len);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) plc_fillin(plc_state_t *s, int16_t amp[], int len)
{
    int i;
    int pitch_overlap;
    float old_step;
    float new_step;
    float old_weight;
    float new_weight;
    float gain;
    int orig_len;

    orig_len = len;
    if (s->missing_samples == 0)
    {
        /* As the gap in real speech starts we need to assess the last known pitch,
           and prepare the synthetic data we will use for fill-in */
        normalise_history(s);
        s->pitch = amdf_pitch(PLC_PITCH_MIN, PLC_PITCH_MAX, s->history + PLC_HISTORY_LEN - CORRELATION_SPAN - PLC_PITCH_MIN, CORRELATION_SPAN);
        /* We overlap a 1/4 wavelength */
        pitch_overlap = s->pitch >> 2;
        /* Cook up a single cycle of pitch, using a single of the real signal with 1/4
           cycle OLA'ed to make the ends join up nicely */
        /* The first 3/4 of the cycle is a simple copy */
        for (i = 0;  i < s->pitch - pitch_overlap;  i++)
            s->pitchbuf[i] = s->history[PLC_HISTORY_LEN - s->pitch + i];
        /* The last 1/4 of the cycle is overlapped with the end of the previous cycle */
        new_step = 1.0f/pitch_overlap;
        new_weight = new_step;
        for (  ;  i < s->pitch;  i++)
        {
            s->pitchbuf[i] = s->history[PLC_HISTORY_LEN - s->pitch + i]*(1.0f - new_weight) + s->history[PLC_HISTORY_LEN - 2*s->pitch + i]*new_weight;
            new_weight += new_step;
        }
        /* We should now be ready to fill in the gap with repeated, decaying cycles
           of what is in pitchbuf */

        gain = 1.0f;
        /* We need to OLA the first 1/4 wavelength of the synthetic data, to smooth
           it into the previous real data. To avoid the need to introduce a delay
           in the stream, reverse the last 1/4 wavelength, and OLA with that. */
        new_step = 1.0f/pitch_overlap;
        old_step = new_step;
        new_weight = new_step;
        old_weight = 1.0f - new_step;
        for (i = 0;  i < pitch_overlap;  i++)
        {
            amp[i] = fsaturate(old_weight*s->history[PLC_HISTORY_LEN - 1 - i] + new_weight*s->pitchbuf[i]);
            new_weight += new_step;
            old_weight -= old_step;
            if (old_weight < 0.0f)
                old_weight = 0.0f;
        }
        s->pitch_offset = i;
    }
    else
    {
        gain = 1.0f - s->missing_samples*ATTENUATION_INCREMENT;
        i = 0;
    }
    for (  ;  gain > 0.0f  &&  i < len;  i++)
    {
        amp[i] = (int16_t) (s->pitchbuf[s->pitch_offset]*gain);
        gain -= ATTENUATION_INCREMENT;
        if (++s->pitch_offset >= s->pitch)
            s->pitch_offset = 0;
    }
    for (  ;  i < len;  i++)
        amp[i] = 0;
    s->missing_samples += orig_len;
    save_history(s, amp, len);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(plc_state_t *) plc_init(plc_state_t *s)
{
    if (s == NULL)
    {
        if ((s = (plc_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) plc_release(plc_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) plc_free(plc_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
