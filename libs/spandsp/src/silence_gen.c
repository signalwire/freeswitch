/*
 * SpanDSP - a series of DSP components for telephony
 *
 * silence_gen.c - A silence generator, for inserting timed silences.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: silence_gen.c,v 1.16 2008/09/07 12:45:16 steveu Exp $
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
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <limits.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/silence_gen.h"

int silence_gen(silence_gen_state_t *s, int16_t *amp, int max_len)
{
    if (s->remaining_samples != INT_MAX)
    {
        if (max_len >= s->remaining_samples)
        {
            max_len = s->remaining_samples;
            if (max_len  &&  s->status_handler)
                s->status_handler(s->status_user_data, SIG_STATUS_SHUTDOWN_COMPLETE);
        }
        s->remaining_samples -= max_len;
    }
    if (INT_MAX - s->total_samples >= max_len)
        s->total_samples += max_len;
    memset(amp, 0, max_len*sizeof(int16_t));
    return max_len;
}
/*- End of function --------------------------------------------------------*/

void silence_gen_always(silence_gen_state_t *s)
{
    s->remaining_samples = INT_MAX;
}
/*- End of function --------------------------------------------------------*/

void silence_gen_set(silence_gen_state_t *s, int silent_samples)
{
    s->remaining_samples = silent_samples;
    s->total_samples = 0;
}
/*- End of function --------------------------------------------------------*/

void silence_gen_alter(silence_gen_state_t *s, int silent_samples)
{
    /* Block negative silences */
    if (silent_samples < 0)
    {
        if (-silent_samples > s->remaining_samples)
            silent_samples = -s->remaining_samples;
    }
    s->remaining_samples += silent_samples;
    s->total_samples += silent_samples;
}
/*- End of function --------------------------------------------------------*/

int silence_gen_remainder(silence_gen_state_t *s)
{
    return s->remaining_samples;
}
/*- End of function --------------------------------------------------------*/

int silence_gen_generated(silence_gen_state_t *s)
{
    return s->total_samples;
}
/*- End of function --------------------------------------------------------*/

void silence_gen_status_handler(silence_gen_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

silence_gen_state_t *silence_gen_init(silence_gen_state_t *s, int silent_samples)
{
    if (s == NULL)
    {
        if ((s = (silence_gen_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->remaining_samples = silent_samples;
    return s;
}
/*- End of function --------------------------------------------------------*/

/* The following dummy routines, to absorb data, don't really have a proper home,
   so they have been put here. */

int span_dummy_rx(void *user_data, const int16_t amp[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int span_dummy_mod(void *user_data, int16_t amp[], int len)
{
    return len;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
