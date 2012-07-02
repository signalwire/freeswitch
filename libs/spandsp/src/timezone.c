/*
 * SpanDSP - a series of DSP components for telephony
 *
 * timezone.c - Timezone handling for time interpretation
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

/* Timezone processing might not seem like a DSP activity, but getting the headers
   right on FAXes demands it. We need to handle multiple time zones within a process,
   for FAXes related to different parts of the globe, so the system timezone handling
   is not adequate. */

/* This timezone handling is derived from public domain software by Arthur David Olson
   <arthur_david_olson@nih.gov> which you may download from ftp://elsie.nci.nih.gov/pub
   at the time of writing. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/timezone.h"

#include "spandsp/private/timezone.h"

#if !defined(FALSE)
#define FALSE    0
#endif

#if !defined(TRUE)
#define TRUE    (!FALSE)
#endif

#define SECS_PER_MIN            60
#define MINS_PER_HOUR           60
#define HOURS_PER_DAY           24
#define DAYS_PER_WEEK           7
#define DAYS_PER_NON_LEAP_YEAR  365
#define DAYS_PER_LEAP_YEAR      366
#define SECS_PER_HOUR           (SECS_PER_MIN*MINS_PER_HOUR)
#define SECS_PER_DAY            ((long int) SECS_PER_HOUR*HOURS_PER_DAY)
#define MONTHS_PER_YEAR         12

#define TM_YEAR_BASE            1900

#define EPOCH_YEAR              1970
#define EPOCH_WDAY              TM_THURSDAY

#define isleap(y)               (((y)%4) == 0  &&  (((y)%100) != 0  ||  ((y)%400) == 0))

#define isleap_sum(a, b)        isleap((a)%400 + (b)%400)

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX. */
#define is_digit(c)             ((unsigned int) (c) - '0' <= 9)

#define TZ_DEF_RULE_STRING      ",M4.1.0,M10.5.0"

#define JULIAN_DAY              0       /* Jn - Julian day */
#define DAY_OF_YEAR             1       /* n - day of year */
#define MONTH_NTH_DAY_OF_WEEK   2       /* Mm.n.d - month, week, day of week */

static const char wildabbr[] = "   ";

static const char gmt[] = "GMT";

struct tz_rule_s
{
    int r_type;                         /* Type of rule--see below */
    int r_day;                          /* Day number of rule */
    int r_week;                         /* Week number of rule */
    int r_mon;                          /* Month number of rule */
    long int r_time;                    /* Transition time of rule */
};

static const int mon_lengths[2][MONTHS_PER_YEAR] =
{
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static const int year_lengths[2] =
{
    DAYS_PER_NON_LEAP_YEAR,
    DAYS_PER_LEAP_YEAR
};

static int add_with_overflow_detection(int *number, int delta)
{
    /* This needs to be considered volatile, or clever optimisation destroys
       the effect of the the rollover detection logic */
    volatile int last_number;

    last_number = *number;
    *number += delta;
    return (*number < last_number) != (delta < 0);
}
/*- End of function --------------------------------------------------------*/

static void set_tzname(tz_t *tz)
{
    struct tz_state_s *sp;
    const struct tz_ttinfo_s *ttisp;
    int i;

    sp = &tz->state;
    tz->tzname[0] = wildabbr;
    tz->tzname[1] = wildabbr;
    for (i = 0;  i < sp->typecnt;  i++)
    {
        ttisp = &sp->ttis[i];
        tz->tzname[ttisp->isdst] = &sp->chars[ttisp->abbrind];
    }
    for (i = 0;  i < sp->timecnt;  i++)
    {
        ttisp = &sp->ttis[sp->types[i]];
        tz->tzname[ttisp->isdst] = &sp->chars[ttisp->abbrind];
    }
}
/*- End of function --------------------------------------------------------*/

/* Return the number of leap years through the end of the given year
   where, to make the math easy, the answer for year zero is defined as zero. */
static int leaps_thru_end_of(const int y)
{
    return (y >= 0)  ?  (y/4 - y/100 + y/400)  :  -(leaps_thru_end_of(-(y + 1)) + 1);
}
/*- End of function --------------------------------------------------------*/

static struct tm *time_sub(const time_t * const timep, const long int offset, const struct tz_state_s * const sp, struct tm * const tmp)
{
    const struct tz_lsinfo_s *lp;
    time_t tdays;
    const int *ip;
    int32_t corr;
    int32_t seconds;
    int32_t rem;
    int idays;
    int y;
    int hit;
    int i;
    int newy;
    time_t tdelta;
    int idelta;
    int leapdays;

    corr = 0;
    hit = 0;
    i = sp->leapcnt;
    while (--i >= 0)
    {
        lp = &sp->lsis[i];
        if (*timep >= lp->trans)
        {
            if (*timep == lp->trans)
            {
                hit = ((i == 0  &&  lp->corr > 0)  ||  lp->corr > sp->lsis[i - 1].corr);
                if (hit)
                {
                    while (i > 0
                           &&
                           sp->lsis[i].trans == sp->lsis[i - 1].trans + 1
                           &&
                           sp->lsis[i].corr == sp->lsis[i - 1].corr + 1)
                    {
                        hit++;
                        --i;
                    }
                }
            }
            corr = lp->corr;
            break;
        }
    }
    y = EPOCH_YEAR;
    tdays = *timep/SECS_PER_DAY;
    rem = *timep - tdays*SECS_PER_DAY;
    while (tdays < 0  ||  tdays >= year_lengths[isleap(y)])
    {
        tdelta = tdays / DAYS_PER_LEAP_YEAR;
        idelta = tdelta;
        if (tdelta - idelta >= 1  ||  idelta - tdelta >= 1)
            return NULL;
        if (idelta == 0)
            idelta = (tdays < 0)  ?  -1  :  1;
        newy = y;
        if (add_with_overflow_detection(&newy, idelta))
            return NULL;
        leapdays = leaps_thru_end_of(newy - 1) - leaps_thru_end_of(y - 1);
        tdays -= ((time_t) newy - y)*DAYS_PER_NON_LEAP_YEAR;
        tdays -= leapdays;
        y = newy;
    }
    seconds = tdays*SECS_PER_DAY;
    tdays = seconds/SECS_PER_DAY;
    rem += seconds - tdays*SECS_PER_DAY;
    /* Given the range, we can now fearlessly cast... */
    idays = tdays;
    rem += (offset - corr);
    while (rem < 0)
    {
        rem += SECS_PER_DAY;
        idays--;
    }
    while (rem >= SECS_PER_DAY)
    {
        rem -= SECS_PER_DAY;
        idays++;
    }
    while (idays < 0)
    {
        if (add_with_overflow_detection(&y, -1))
            return NULL;
        idays += year_lengths[isleap(y)];
    }
    while (idays >= year_lengths[isleap(y)])
    {
        idays -= year_lengths[isleap(y)];
        if (add_with_overflow_detection(&y, 1))
            return NULL;
    }
    tmp->tm_year = y;
    if (add_with_overflow_detection(&tmp->tm_year, -TM_YEAR_BASE))
        return NULL;
    tmp->tm_yday = idays;
    /* The "extra" mods below avoid overflow problems. */
    tmp->tm_wday = EPOCH_WDAY
                 + ((y - EPOCH_YEAR) % DAYS_PER_WEEK)*(DAYS_PER_NON_LEAP_YEAR % DAYS_PER_WEEK)
                 + leaps_thru_end_of(y - 1)
                 - leaps_thru_end_of(EPOCH_YEAR - 1)
                 + idays;
    tmp->tm_wday %= DAYS_PER_WEEK;
    if (tmp->tm_wday < 0)
        tmp->tm_wday += DAYS_PER_WEEK;
    tmp->tm_hour = (int) (rem/SECS_PER_HOUR);
    rem %= SECS_PER_HOUR;
    tmp->tm_min = (int) (rem/SECS_PER_MIN);
    /* A positive leap second requires a special
     * representation. This uses "... ??:59:60" et seq. */
    tmp->tm_sec = (int) (rem%SECS_PER_MIN) + hit;
    ip = mon_lengths[isleap(y)];
    for (tmp->tm_mon = 0;  idays >= ip[tmp->tm_mon];  (tmp->tm_mon)++)
        idays -= ip[tmp->tm_mon];
    tmp->tm_mday = (int) (idays + 1);
    tmp->tm_isdst = 0;
    return tmp;
}
/*- End of function --------------------------------------------------------*/

/* Given a pointer into a time zone string, scan until a character that is not
 * a valid character in a zone name is found. Return a pointer to that
 * character. */
static const char *get_tzname(const char *strp)
{
    char c;

    while ((c = *strp) != '\0'  &&  !is_digit(c)  &&  c != ','  &&  c != '-'  &&  c != '+')
        strp++;
    return strp;
}
/*- End of function --------------------------------------------------------*/

/* Given a pointer into a time zone string, extract a number from that string.
 * Check that the number is within a specified range; if it is not, return
 * NULL.
 * Otherwise, return a pointer to the first character not part of the number. */
static const char *get_num(const char *strp, int * const nump, const int min, const int max)
{
    char c;
    int num;

    if (strp == NULL  ||  !is_digit(c = *strp))
        return NULL;
    num = 0;
    do
    {
        num = num*10 + (c - '0');
        if (num > max)
            return NULL;    /* Illegal value */
        c = *++strp;
    }
    while (is_digit(c));
    if (num < min)
        return NULL;        /* Illegal value */
    *nump = num;
    return strp;
}
/*- End of function --------------------------------------------------------*/

/* Given a pointer into a time zone string, extract a number of seconds,
 * in hh[:mm[:ss]] form, from the string.
 * If any error occurs, return NULL.
 * Otherwise, return a pointer to the first character not part of the number
 * of seconds. */
static const char *get_secs(const char *strp, long int * const secsp)
{
    int num;

    /* HOURS_PER_DAY*DAYS_PER_WEEK - 1 allows quasi-Posix rules like
     * "M10.4.6/26", which does not conform to Posix,
     * but which specifies the equivalent of
     * "02:00 on the first Sunday on or after 23 Oct". */
    strp = get_num(strp, &num, 0, HOURS_PER_DAY*DAYS_PER_WEEK - 1);
    if (strp == NULL)
        return NULL;
    *secsp = num*(long int) SECS_PER_HOUR;
    if (*strp == ':')
    {
        strp = get_num(strp + 1, &num, 0, MINS_PER_HOUR - 1);
        if (strp == NULL)
            return NULL;
        *secsp += num*SECS_PER_MIN;
        if (*strp == ':')
        {
            /* SECS_PER_MIN allows for leap seconds. */
            strp = get_num(strp + 1, &num, 0, SECS_PER_MIN);
            if (strp == NULL)
                return NULL;
            *secsp += num;
        }
    }
    return strp;
}
/*- End of function --------------------------------------------------------*/

/* Given a pointer into a time zone string, extract an offset, in
 * [+-]hh[:mm[:ss]] form, from the string.
 * If any error occurs, return NULL.
 * Otherwise, return a pointer to the first character not part of the time. */
static const char *get_offset(const char *strp, long int * const offsetp)
{
    int neg = 0;

    if (*strp == '-')
    {
        neg = 1;
        strp++;
    }
    else if (*strp == '+')
    {
        strp++;
    }
    strp = get_secs(strp, offsetp);
    if (strp == NULL)
        return NULL;        /* Illegal time */
    if (neg)
        *offsetp = -*offsetp;
    return strp;
}
/*- End of function --------------------------------------------------------*/

/* Given a pointer into a time zone string, extract a rule in the form
 * date[/time]. See POSIX section 8 for the format of "date" and "time".
 * If a valid rule is not found, return NULL.
 * Otherwise, return a pointer to the first character not part of the rule. */
static const char *get_rule(const char *strp, struct tz_rule_s * const rulep)
{
    if (*strp == 'J')
    {
        /* Julian day. */
        rulep->r_type = JULIAN_DAY;
        strp = get_num(strp + 1, &rulep->r_day, 1, DAYS_PER_NON_LEAP_YEAR);
    }
    else if (*strp == 'M')
    {
        /* Month, week, day. */
        rulep->r_type = MONTH_NTH_DAY_OF_WEEK;
        strp = get_num(strp + 1, &rulep->r_mon, 1, MONTHS_PER_YEAR);
        if (strp == NULL  ||  *strp++ != '.')
            return NULL;
        strp = get_num(strp, &rulep->r_week, 1, 5);
        if (strp == NULL  ||  *strp++ != '.')
            return NULL;
        strp = get_num(strp, &rulep->r_day, 0, DAYS_PER_WEEK - 1);
    }
    else if (is_digit(*strp))
    {
        /* Day of the year. */
        rulep->r_type = DAY_OF_YEAR;
        strp = get_num(strp, &rulep->r_day, 0, DAYS_PER_LEAP_YEAR - 1);
    }
    else
    {
        /* Invalid format */
        return NULL;
    }
    if (strp == NULL)
        return NULL;
    if (*strp == '/')
    {
        /* Time specified. */
        strp = get_secs(strp + 1, &rulep->r_time);
    }
    else
    {
        /* Default = 2:00:00 */
        rulep->r_time = 2*SECS_PER_HOUR;
    }
    return strp;
}
/*- End of function --------------------------------------------------------*/

/* Given the Epoch-relative time of January 1, 00:00:00 UTC, in a year, the
 * year, a rule, and the offset from UTC at the time that rule takes effect,
 * calculate the Epoch-relative time that rule takes effect. */
static time_t trans_time(const time_t janfirst, const int year, const struct tz_rule_s * const rulep, const long int offset)
{
    int leapyear;
    time_t value;
    int i;
    int d;
    int m1;
    int yy0;
    int yy1;
    int yy2;
    int dow;

    value = 0;
    leapyear = isleap(year);
    switch (rulep->r_type)
    {
    case JULIAN_DAY:
        /* Jn - Julian day, 1 == January 1, 60 == March 1 even in leap
         * years.
         * In non-leap years, or if the day number is 59 or less, just
         * add SECS_PER_DAY times the day number-1 to the time of
         * January 1, midnight, to get the day. */
        value = janfirst + (rulep->r_day - 1)*SECS_PER_DAY;
        if (leapyear  &&  rulep->r_day >= 60)
            value += SECS_PER_DAY;
        break;
    case DAY_OF_YEAR:
        /* n - day of year.
         * Just add SECS_PER_DAY times the day number to the time of
         * January 1, midnight, to get the day. */
        value = janfirst + rulep->r_day * SECS_PER_DAY;
        break;
    case MONTH_NTH_DAY_OF_WEEK:
        /* Mm.n.d - nth "dth day" of month m. */
        value = janfirst;
        for (i = 0;  i < rulep->r_mon - 1;  i++)
            value += mon_lengths[leapyear][i]*SECS_PER_DAY;

        /* Use Zeller's Congruence to get day-of-week of first day of month. */
        m1 = (rulep->r_mon + 9)%12 + 1;
        yy0 = (rulep->r_mon <= 2)  ?  (year - 1)  :  year;
        yy1 = yy0/100;
        yy2 = yy0%100;
        dow = ((26*m1 - 2)/10 + 1 + yy2 + yy2/4 + yy1/4 - 2*yy1)%7;
        if (dow < 0)
            dow += DAYS_PER_WEEK;

        /* "dow" is the day-of-week of the first day of the month. Get
         * the day-of-month (zero-origin) of the first "dow" day of the
         * month. */
        d = rulep->r_day - dow;
        if (d < 0)
            d += DAYS_PER_WEEK;
        for (i = 1;  i < rulep->r_week;  i++)
        {
            if (d + DAYS_PER_WEEK >= mon_lengths[leapyear][rulep->r_mon - 1])
                break;
            d += DAYS_PER_WEEK;
        }

        /* "d" is the day-of-month (zero-origin) of the day we want. */
        value += d*SECS_PER_DAY;
        break;
    }

    /* "value" is the Epoch-relative time of 00:00:00 UTC on the day in
     * question. To get the Epoch-relative time of the specified local
     * time on that day, add the transition time and the current offset
     * from UTC. */
    return value + rulep->r_time + offset;
}
/*- End of function --------------------------------------------------------*/

/* Given a POSIX section 8-style TZ string, fill in the rule tables as
   appropriate. */
static int tzparse(const char *name, struct tz_state_s * const sp, const int lastditch)
{
    const char *stdname;
    const char *dstname;
    size_t stdlen;
    size_t dstlen;
    long int stdoffset;
    long int dstoffset;
    long int theirstdoffset;
    long int theirdstoffset;
    long int theiroffset;
    unsigned char *typep;
    char *cp;
    int load_result;
    int isdst;
    int i;
    int j;
    int year;
    struct tz_rule_s start;
    struct tz_rule_s end;
    time_t *atp;
    time_t janfirst;
    time_t starttime;
    time_t endtime;

    dstname = NULL;
    stdname = name;
    if (lastditch)
    {
        stdlen = strlen(name);      /* Length of standard zone name */
        name += stdlen;
        if (stdlen >= sizeof(sp->chars))
            stdlen = sizeof(sp->chars) - 1;
        stdoffset = 0;
    }
    else
    {
        name = get_tzname(name);
        stdlen = name - stdname;
        if (stdlen < 3)
            return -1;
        if (*name == '\0')
            return -1;
        name = get_offset(name, &stdoffset);
        if (name == NULL)
            return -1;
    }
    load_result = -1;
    if (load_result != 0)
        sp->leapcnt = 0;            /* So, we're off a little */
    if (*name != '\0')
    {
        dstname = name;
        name = get_tzname(name);
        dstlen = name - dstname;    /* Length of DST zone name */
        if (dstlen < 3)
            return -1;
        if (*name != '\0'  &&  *name != ','  &&  *name != ';')
        {
            if ((name = get_offset(name, &dstoffset)) == NULL)
                return -1;
        }
        else
        {
            dstoffset = stdoffset - SECS_PER_HOUR;
        }
        if (*name == '\0'  &&  load_result != 0)
            name = TZ_DEF_RULE_STRING;
        if (*name == ','  ||  *name == ';')
        {
            if ((name = get_rule(name + 1, &start)) == NULL)
                return -1;
            if (*name++ != ',')
                return -1;
            if ((name = get_rule(name, &end)) == NULL)
                return -1;
            if (*name != '\0')
                return -1;
            sp->typecnt = 2;        /* Standard time and DST */
            /* Two transitions per year, from EPOCH_YEAR to 2037. */
            sp->timecnt = 2*(2037 - EPOCH_YEAR + 1);
            if (sp->timecnt > TZ_MAX_TIMES)
                return -1;
            sp->ttis[0].gmtoff = -dstoffset;
            sp->ttis[0].isdst = 1;
            sp->ttis[0].abbrind = stdlen + 1;
            sp->ttis[1].gmtoff = -stdoffset;
            sp->ttis[1].isdst = 0;
            sp->ttis[1].abbrind = 0;
            atp = sp->ats;
            typep = sp->types;
            janfirst = 0;
            for (year = EPOCH_YEAR;  year <= 2037;  year++)
            {
                starttime = trans_time(janfirst, year, &start, stdoffset);
                endtime = trans_time(janfirst, year, &end, dstoffset);
                if (starttime > endtime)
                {
                    *atp++ = endtime;
                    *typep++ = 1;    /* DST ends */
                    *atp++ = starttime;
                    *typep++ = 0;    /* DST begins */
                }
                else
                {
                    *atp++ = starttime;
                    *typep++ = 0;    /* DST begins */
                    *atp++ = endtime;
                    *typep++ = 1;    /* DST ends */
                }
                janfirst += year_lengths[isleap(year)]*SECS_PER_DAY;
            }
        }
        else
        {
            if (*name != '\0')
                return -1;
            /* Initial values of theirstdoffset and theirdstoffset. */
            theirstdoffset = 0;
            for (i = 0;  i < sp->timecnt;  i++)
            {
                j = sp->types[i];
                if (!sp->ttis[j].isdst)
                {
                    theirstdoffset = -sp->ttis[j].gmtoff;
                    break;
                }
            }
            theirdstoffset = 0;
            for (i = 0;  i < sp->timecnt;  i++)
            {
                j = sp->types[i];
                if (sp->ttis[j].isdst)
                {
                    theirdstoffset = -sp->ttis[j].gmtoff;
                    break;
                }
            }
            /* Initially we're assumed to be in standard time. */
            isdst = FALSE;
            theiroffset = theirstdoffset;
            /* Now juggle transition times and types tracking offsets as you do. */
            for (i = 0;  i < sp->timecnt;  i++)
            {
                j = sp->types[i];
                sp->types[i] = sp->ttis[j].isdst;
                if (sp->ttis[j].ttisgmt)
                {
                    /* No adjustment to transition time */
                }
                else
                {
                    /* If summer time is in effect, and the
                     * transition time was not specified as
                     * standard time, add the summer time
                     * offset to the transition time;
                     * otherwise, add the standard time
                     * offset to the transition time. */
                    /* Transitions from DST to DDST
                     * will effectively disappear since
                     * POSIX provides for only one DST
                     * offset. */
                    if (isdst  &&  !sp->ttis[j].ttisstd)
                        sp->ats[i] += (dstoffset - theirdstoffset);
                    else
                        sp->ats[i] += (stdoffset - theirstdoffset);
                }
                theiroffset = -sp->ttis[j].gmtoff;
                if (sp->ttis[j].isdst)
                    theirdstoffset = theiroffset;
                else
                    theirstdoffset = theiroffset;
            }
            /* Finally, fill in ttis. ttisstd and ttisgmt need not be handled. */
            sp->ttis[0].gmtoff = -stdoffset;
            sp->ttis[0].isdst = FALSE;
            sp->ttis[0].abbrind = 0;
            sp->ttis[1].gmtoff = -dstoffset;
            sp->ttis[1].isdst = TRUE;
            sp->ttis[1].abbrind = stdlen + 1;
            sp->typecnt = 2;
        }
    }
    else
    {
        dstlen = 0;
        sp->typecnt = 1;        /* Only standard time */
        sp->timecnt = 0;
        sp->ttis[0].gmtoff = -stdoffset;
        sp->ttis[0].isdst = 0;
        sp->ttis[0].abbrind = 0;
    }
    sp->charcnt = stdlen + 1;
    if (dstlen != 0)
        sp->charcnt += dstlen + 1;
    if ((size_t) sp->charcnt > sizeof(sp->chars))
        return -1;
    cp = sp->chars;
    strncpy(cp, stdname, stdlen);
    cp += stdlen;
    *cp++ = '\0';
    if (dstlen != 0)
    {
        strncpy(cp, dstname, dstlen);
        cp[dstlen] = '\0';
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void tz_set(tz_t *tz, const char *tzstring)
{
    const char *name = "";
    struct tz_state_s *lclptr = &tz->state;

    if (tzstring)
        name = tzstring;

    /* See if we are already set OK */
    if (tz->lcl_is_set > 0  &&  strcmp(tz->lcl_tzname, name) == 0)
        return;
    tz->lcl_is_set = strlen(name) < sizeof(tz->lcl_tzname);
    if (tz->lcl_is_set)
        strcpy(tz->lcl_tzname, name);

    if (name[0] == '\0')
    {
        /* User wants it fast rather than right, so, we're off a little. */
        lclptr->leapcnt = 0;
        lclptr->timecnt = 0;
        lclptr->typecnt = 0;
        lclptr->ttis[0].isdst = 0;
        lclptr->ttis[0].gmtoff = 0;
        lclptr->ttis[0].abbrind = 0;
        strcpy(lclptr->chars, gmt);
    }
    else if (name[0] == ':'  ||  tzparse(name, lclptr, FALSE) != 0)
    {
        tzparse(gmt, lclptr, TRUE);
    }
    set_tzname(tz);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) tz_localtime(tz_t *tz, struct tm *tmp, time_t t)
{
    struct tz_state_s *sp;
    const struct tz_ttinfo_s *ttisp;
    int i;

    sp = &tz->state;

    if (sp->timecnt == 0  ||  t < sp->ats[0])
    {
        i = 0;
        while (sp->ttis[i].isdst)
        {
            if (++i >= sp->typecnt)
            {
                i = 0;
                break;
            }
        }
    }
    else
    {
        for (i = 1;  i < sp->timecnt;  i++)
        {
            if (t < sp->ats[i])
                break;
        }
        i = (int) sp->types[i - 1];
    }
    ttisp = &sp->ttis[i];
    time_sub(&t, ttisp->gmtoff, sp, tmp);
    tmp->tm_isdst = ttisp->isdst;
    tz->tzname[tmp->tm_isdst] = &sp->chars[ttisp->abbrind];
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) tz_tzname(tz_t *tz, int isdst)
{
    return tz->tzname[(!isdst)  ?  0  :  1];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(tz_t *) tz_init(tz_t *tz, const char *tzstring)
{
    if (tz == NULL)
    {
        if ((tz = (tz_t *) malloc(sizeof(*tz))) == NULL)
            return NULL;
    }
    memset(tz, 0, sizeof(*tz));
    tz->tzname[0] =
    tz->tzname[1] = wildabbr;
    tz_set(tz, tzstring);
    return tz;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) tz_release(tz_t *tz)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) tz_free(tz_t *tz)
{
    if (tz)
        free(tz);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
