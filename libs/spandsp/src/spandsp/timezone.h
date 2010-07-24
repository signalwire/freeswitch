/*
 * SpanDSP - a series of DSP components for telephony
 *
 * timezone.h - Timezone handling for time interpretation
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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

#if !defined(_SPANDSP_TIMEZONE_H_)
#define _SPANDSP_TIMEZONE_H_

/*! \page timezone_page Timezone handling

\section timezone_sec_1 What does it do?

\section timezone_sec_2 How does it work?

*/

typedef struct tz_s tz_t;

enum
{
    TM_SUNDAY = 0,
    TM_MONDAY,
    TM_TUESDAY,
    TM_WEDNESDAY,
    TM_THURSDAY,
    TM_FRIDAY,
    TM_SATURDAY
};

enum
{
    TM_JANUARY = 0,
    TM_FEBRUARY,
    TM_MARCH,
    TM_APRIL,
    TM_MAY,
    TM_JUNE,
    TM_JULY,
    TM_AUGUST,
    TM_SEPTEMBER,
    TM_OCTOBER,
    TM_NOVEMBER,
    TM_DECEMBER
};

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(tz_t *) tz_init(tz_t *tz, const char *tzstring);

SPAN_DECLARE(int) tz_release(tz_t *tz);

SPAN_DECLARE(int) tz_free(tz_t *tz);

SPAN_DECLARE(int) tz_localtime(tz_t *tz, struct tm *tm, time_t t);

SPAN_DECLARE(const char *) tz_tzname(tz_t *tz, int isdst);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
