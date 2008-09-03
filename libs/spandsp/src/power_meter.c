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
 *
 * $Id: power_meter.c,v 1.24 2008/07/02 14:48:26 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <float.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/power_meter.h"

power_meter_t *power_meter_init(power_meter_t *s, int shift)
{
    if (s == NULL)
    {
        if ((s = (power_meter_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->shift = shift;
    s->reading = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

power_meter_t *power_meter_damping(power_meter_t *s, int shift)
{
    s->shift = shift;
    return s;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_update(power_meter_t *s, int16_t amp)
{
    s->reading += ((amp*amp - s->reading) >> s->shift);
    return s->reading;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_level_dbm0(float level)
{
    float l;

    level -= DBM0_MAX_POWER;
    if (level > 0.0)
        level = 0.0;
    l = powf(10.0f, level/10.0f)*(32767.0f*32767.0f);
    return (int32_t) l;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_level_dbov(float level)
{
    float l;

    if (level > 0.0)
        level = 0.0;
    l = powf(10.0f, level/10.0f)*(32767.0f*32767.0f);
    return (int32_t) l;
}
/*- End of function --------------------------------------------------------*/

int32_t power_meter_current(power_meter_t *s)
{
    return s->reading;
}
/*- End of function --------------------------------------------------------*/

float power_meter_current_dbm0(power_meter_t *s)
{
    if (s->reading <= 0)
        return FLT_MIN;
    /* This is based on A-law, but u-law is only 0.03dB different, so don't worry. */
    return log10f((float) s->reading/(32767.0f*32767.0f))*10.0f + DBM0_MAX_POWER;
}
/*- End of function --------------------------------------------------------*/

float power_meter_current_dbov(power_meter_t *s)
{
    if (s->reading <= 0)
        return FLT_MIN;
    return log10f((float) s->reading/(32767.0f*32767.0f))*10.0f;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
