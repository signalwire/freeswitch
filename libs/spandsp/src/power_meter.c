/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter.c
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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/power_meter.h"

#include "spandsp/private/power_meter.h"

SPAN_DECLARE(power_meter_t *) power_meter_init(power_meter_t *s, int shift)
{
    if (s == NULL)
    {
        if ((s = (power_meter_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->shift = shift;
    s->reading = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) power_meter_release(power_meter_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) power_meter_free(power_meter_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(power_meter_t *) power_meter_damping(power_meter_t *s, int shift)
{
    s->shift = shift;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) power_meter_update(power_meter_t *s, int16_t amp)
{
    s->reading += ((amp*amp - s->reading) >> s->shift);
    return s->reading;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) power_meter_level_dbm0(float level)
{
    float l;

    level -= DBM0_MAX_POWER;
    if (level > 0.0)
        level = 0.0;
    l = powf(10.0f, level/10.0f)*(32767.0f*32767.0f);
    return (int32_t) l;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) power_meter_level_dbov(float level)
{
    float l;

    if (level > 0.0)
        level = 0.0;
    l = powf(10.0f, level/10.0f)*(32767.0f*32767.0f);
    return (int32_t) l;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) power_meter_current(power_meter_t *s)
{
    return s->reading;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) power_meter_current_dbm0(power_meter_t *s)
{
    if (s->reading <= 0)
        return -96.329f + DBM0_MAX_POWER;
    /* This is based on A-law, but u-law is only 0.03dB different, so don't worry. */
    return 10.0f*log10f((float) s->reading/(32767.0f*32767.0f) + 1.0e-10f) + DBM0_MAX_POWER;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) power_meter_current_dbov(power_meter_t *s)
{
    if (s->reading <= 0)
        return -96.329f;
    return 10.0f*log10f((float) s->reading/(32767.0f*32767.0f) + 1.0e-10f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int32_t) power_surge_detector(power_surge_detector_state_t *s, int16_t amp)
{
    int32_t pow_short;
    int32_t pow_medium;

    pow_short = power_meter_update(&s->short_term, amp);
    pow_medium = power_meter_update(&s->medium_term, amp);
    if (pow_medium < s->min)
        return 0;
    if (!s->signal_present)
    {
        if (pow_short <= s->surge*(pow_medium >> 10))
            return 0;
        s->signal_present = true;
        s->medium_term.reading = s->short_term.reading;
    }
    else
    {
        if (pow_short < s->sag*(pow_medium >> 10))
        {
            s->signal_present = false;
            s->medium_term.reading = s->short_term.reading;
            return 0;
        }
    }
    return pow_short;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) power_surge_detector_current_dbm0(power_surge_detector_state_t *s)
{
    return power_meter_current_dbm0(&s->short_term);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) power_surge_detector_current_dbov(power_surge_detector_state_t *s)
{
    return power_meter_current_dbov(&s->short_term);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(power_surge_detector_state_t *) power_surge_detector_init(power_surge_detector_state_t *s, float min, float surge)
{
    float ratio;

    if (s == NULL)
    {
        if ((s = (power_surge_detector_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    power_meter_init(&s->short_term, 4);
    power_meter_init(&s->medium_term, 7);
    ratio = powf(10.0f, surge/10.0f);
    s->surge = 1024.0f*ratio;
    s->sag = 1024.0f/ratio;
    s->min = power_meter_level_dbm0(min);
    s->medium_term.reading = s->min + 1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) power_surge_detector_release(power_surge_detector_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) power_surge_detector_free(power_surge_detector_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
