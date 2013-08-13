/*
 * SpanDSP - a series of DSP components for telephony
 *
 * swept_tone.c - Swept tone generation
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"
#include "spandsp/dds.h"

#include "spandsp/swept_tone.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/swept_tone.h"

SPAN_DECLARE(swept_tone_state_t *) swept_tone_init(swept_tone_state_t *s, float start, float end, float level, int duration, int repeating)
{
    if (s == NULL)
    {
        if ((s = (swept_tone_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->current_phase_inc =
    s->starting_phase_inc = dds_phase_rate(start);
    s->phase_inc_step = dds_phase_rate((end - start)/(float) duration);
    s->scale = dds_scaling_dbm0(level);
    s->duration = duration;
    s->repeating = repeating;
    s->pos = 0;
    s->phase = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) swept_tone(swept_tone_state_t *s, int16_t amp[], int max_len)
{
    int i;
    int len;
    int chunk_len;

    for (len = 0;  len < max_len;  )
    {
        chunk_len = max_len - len;
        if (chunk_len > s->duration - s->pos)
            chunk_len = s->duration - s->pos;
        for (i = len;  i < len + chunk_len;  i++)
        {
            amp[i] = (dds(&s->phase, s->current_phase_inc)*s->scale) >> 15;
            s->current_phase_inc += s->phase_inc_step;
        }
        len += chunk_len;
        s->pos += chunk_len;
        if (s->pos >= s->duration)
        {
            if (!s->repeating)
                break;
            s->pos = 0;
            s->current_phase_inc = s->starting_phase_inc;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) swept_tone_current_frequency(swept_tone_state_t *s)
{
    return dds_frequency(s->current_phase_inc);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) swept_tone_release(swept_tone_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) swept_tone_free(swept_tone_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
