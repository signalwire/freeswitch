/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/timezone.h - Timezone handling for time interpretation
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

#if !defined(_SPANDSP_PRIVATE_TIMEZONE_H_)
#define _SPANDSP_PRIVATE_TIMEZONE_H_

#define TZ_MAX_CHARS            50      /* Maximum number of abbreviation characters */

#define TZ_MAX_LEAPS            50      /* Maximum number of leap second corrections */

#define SPANDSP_TZNAME_MAX              255

/* The TZ_MAX_TIMES value below is enough to handle a bit more than a
 * year's worth of solar time (corrected daily to the nearest second) or
 * 138 years of Pacific Presidential Election time
 * (where there are three time zone transitions every fourth year). */
#define TZ_MAX_TIMES            370

#if !defined(NOSOLAR)
#define TZ_MAX_TYPES            256     /* Limited by what (unsigned char)'s can hold */
#else
/* Must be at least 14 for Europe/Riga as of Jan 12 1995,
 * as noted by Earl Chew <earl@hpato.aus.hp.com>. */
#define TZ_MAX_TYPES            20      /* Maximum number of local time types */
#endif

#define TZ_BIGGEST(a, b)        (((a) > (b)) ? (a) : (b))

/* Time type information */
struct tz_ttinfo_s
{
    int32_t gmtoff;             /* UTC offset in seconds */
    int isdst;                  /* Used to set tm_isdst */
    int abbrind;                /* Abbreviation list index */
    bool ttisstd;               /* True if transition is std time */
    bool ttisgmt;               /* True if transition is UTC */
};

/* Leap second information */
struct tz_lsinfo_s
{
    time_t trans;               /* Transition time */
    int32_t corr;               /* Correction to apply */
};

struct tz_state_s
{
    int leapcnt;
    int timecnt;
    int typecnt;
    int charcnt;
    time_t ats[TZ_MAX_TIMES];
    uint8_t types[TZ_MAX_TIMES];
    struct tz_ttinfo_s ttis[TZ_MAX_TYPES];
    char chars[TZ_BIGGEST(TZ_MAX_CHARS + 1, (2*(SPANDSP_TZNAME_MAX + 1)))];
    struct tz_lsinfo_s lsis[TZ_MAX_LEAPS];
};

struct tz_s
{
    struct tz_state_s state;
    char lcl_tzname[SPANDSP_TZNAME_MAX + 1];
    int lcl_is_set;
    const char *tzname[2];
};

#endif
/*- End of file ------------------------------------------------------------*/
