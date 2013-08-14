/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_tx.c - Flexible telephony supervisory tone generation.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_tx.h"

#include "spandsp/private/tone_generate.h"
#include "spandsp/private/super_tone_tx.h"

/*
    The tone played to wake folk up when they have left the phone off hook is an
    oddity amongst supervisory tones. It is designed to sound loud and nasty. Most
    tones are one or two pure sine pitches, or one AM moduluated pitch. This alert
    tone varies between countries, but AT&T are a typical example.

    AT&T Receiver Off-Hook Tone is 1400 Hz, 2060 Hz, 2450 Hz and 2600 Hz at 0dBm0/frequency
    on and off every .1 second. On some older space division switching systems
    Receiver Off-Hook was 1400 Hz, 2060 Hz, 2450 Hz and 2600 Hz at +5 VU on and
    off every .1 second. On a No. 5 ESS this continues for 30 seconds. On a No.
    2/2B ESS this continues for 40 seconds. On some other AT&T switches there are
    two iterations of 50 seconds each.
*/

SPAN_DECLARE(super_tone_tx_step_t *) super_tone_tx_make_step(super_tone_tx_step_t *s,
                                                             float f1,
                                                             float l1,
                                                             float f2,
                                                             float l2,
                                                             int length,
                                                             int cycles)
{
    if (s == NULL)
    {
        if ((s = (super_tone_tx_step_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    if (f1 >= 1.0f)
    {
        s->tone[0].phase_rate = dds_phase_ratef(f1);
        s->tone[0].gain = dds_scaling_dbm0f(l1);
    }
    else
    {
        s->tone[0].phase_rate = 0;
        s->tone[0].gain = 0.0f;
    }
    if (f2 >= 1.0f)
    {
        s->tone[1].phase_rate = dds_phase_ratef(f2);
        s->tone[1].gain = dds_scaling_dbm0f(l2);
    }
    else
    {
        s->tone[1].phase_rate = 0;
        s->tone[1].gain = 0.0f;
    }
    s->tone_on = (f1 > 0.0f);
    s->length = length*SAMPLE_RATE/1000;
    s->cycles = cycles;
    s->next = NULL;
    s->nest = NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_tx_free_tone(super_tone_tx_step_t *s)
{
    super_tone_tx_step_t *t;

    while (s)
    {
        /* Follow nesting... */
        if (s->nest)
            super_tone_tx_free_tone(s->nest);
        t = s;
        s = s->next;
        span_free(t);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(super_tone_tx_state_t *) super_tone_tx_init(super_tone_tx_state_t *s, super_tone_tx_step_t *tree)
{
    if (tree == NULL)
        return NULL;
    if (s == NULL)
    {
        if ((s = (super_tone_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->level = 0;
    s->levels[0] = tree;
    s->cycles[0] = tree->cycles;

    s->current_position = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_tx_release(super_tone_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_tx_free(super_tone_tx_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_tx(super_tone_tx_state_t *s, int16_t amp[], int max_samples)
{
    int samples;
    int limit;
    int len;
    int i;
    float xamp;
    super_tone_tx_step_t *tree;

    if (s->level < 0  ||  s->level > 3)
        return 0;
    samples = 0;
    tree = s->levels[s->level];
    while (tree  &&  samples < max_samples)
    {
        if (tree->tone_on)
        {
            /* A period of tone. A length of zero means infinite length. */
            if (s->current_position == 0)
            {
                /* New step - prepare the tone generator */
                for (i = 0;  i < 4;  i++)
                    s->tone[i] = tree->tone[i];
            }
            len = tree->length - s->current_position;
            if (tree->length == 0)
            {
                len = max_samples - samples;
                /* We just need to make current position non-zero */
                s->current_position = 1;
            }
            else if (len > max_samples - samples)
            {
                len = max_samples - samples;
                s->current_position += len;
            }
            else
            {
                s->current_position = 0;
            }
            if (s->tone[0].phase_rate < 0)
            {
                for (limit = len + samples;  samples < limit;  samples++)
                {
                    /* There must be two, and only two tones */
                    xamp = dds_modf(&s->phase[0], -s->tone[0].phase_rate, s->tone[0].gain, 0)
                         *(1.0f + dds_modf(&s->phase[1], s->tone[1].phase_rate, s->tone[1].gain, 0));
                    amp[samples] = (int16_t) lfastrintf(xamp);
                }
            }
            else
            {
                for (limit = len + samples;  samples < limit;  samples++)
                {
                    xamp = 0.0f;
                    for (i = 0;  i < 4;  i++)
                    {
                        if (s->tone[i].phase_rate == 0)
                            break;
                        xamp += dds_modf(&s->phase[i], s->tone[i].phase_rate, s->tone[i].gain, 0);
                    }
                    amp[samples] = (int16_t) lfastrintf(xamp);
                }
            }
            if (s->current_position)
                return samples;
        }
        else if (tree->length)
        {
            /* A period of silence. The length must always
               be explicitly stated. A length of zero does
               not give infinite silence. */
            len = tree->length - s->current_position;
            if (len > max_samples - samples)
            {
                len = max_samples - samples;
                s->current_position += len;
            }
            else
            {
                s->current_position = 0;
            }
            memset(amp + samples, 0, sizeof(uint16_t)*len);
            samples += len;
            if (s->current_position)
                return samples;
        }
        /* Nesting has priority... */
        if (tree->nest)
        {
            tree = tree->nest;
            s->levels[++s->level] = tree;
            s->cycles[s->level] = tree->cycles;
        }
        else
        {
            /* ...Next comes repeating, and finally moving forward a step. */
            /* When repeating, note that zero cycles really means endless cycles. */
            while (tree->cycles  &&  --s->cycles[s->level] <= 0)
            {
                tree = tree->next;
                if (tree)
                {
                    /* A fresh new step. */
                    s->levels[s->level] = tree;
                    s->cycles[s->level] = tree->cycles;
                    break;
                }
                /* If we are nested we need to pop, otherwise this is the end. */
                if (s->level <= 0)
                {
                    /* Mark the tone as completed */
                    s->levels[0] = NULL;
                    break;
                }
                tree = s->levels[--s->level];
            }
        }
    }
    return samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
