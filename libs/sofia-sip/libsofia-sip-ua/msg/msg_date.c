/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup msg_parser
 * @CFILE msg_date.c
 * @brief Parser for HTTP-Date and HTTP-Delta.
 *
 * Parsing functions for @RFC1123 (GMT) date.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr 11 18:57:06 2001 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/msg_date.h>
#include <sofia-sip/bnf.h>

#include <sofia-sip/su_time.h>

/** Return current time as seconds since Mon, 01 Jan 1900 00:00:00 GMT. */
msg_time_t msg_now(void)
{
  return su_now().tv_sec;
}

#define is_digit(c) ((c) >= '0' && (c) <= '9')

/**Epoch year. @internal
 *
 * First day of the epoch year should be Monday.
 */
#define EPOCH 1900
/** Is this year a leap year? @internal */
#define LEAP_YEAR(y) ((y) % 4 == 0 && ((y) % 100 != 0 || (y) % 400 == 0))
/** Day number of New Year Day of given year. @internal */
#define YEAR_DAYS(y) \
  (((y)-1) * 365 + ((y)-1) / 4 - ((y)-1) / 100 + ((y)-1) / 400)


/* ====================================================================== */

static unsigned char const days_per_months[12] =
  {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

/** Offset of first day of the month with formula day = 30 * month + offset. */
static signed char const first_day_offset[12] =
  {
    0, 1, -1, 0, 0, 1, 1, 2, 3, 3, 4, 4
  };

static char const wkdays[7 * 4] =
  "Mon\0" "Tue\0" "Wed\0" "Thu\0" "Fri\0" "Sat\0" "Sun";

static char const months[12 * 4] =
  "Jan\0" "Feb\0" "Mar\0" "Apr\0" "May\0" "Jun\0"
  "Jul\0" "Aug\0" "Sep\0" "Oct\0" "Nov\0" "Dec";

/** Parse a month name.
 *
 * @return The function month_d() returns 0..11 if given first three letters
 * of month name, or -1 if no month name matches.
 */
su_inline
int month_d(char const *s)
{
  unsigned const uc = ('a' - 'A') << 16 | ('a' - 'A') << 8 | ('a' - 'A');
  unsigned m;

  if (!s[0] || !s[1] || !s[2])
    return -1;

  #define MONTH_V(s) (uc | (((s[0]) << 16) + ((s[1]) << 8) + ((s[2]))))
  m = MONTH_V(s);

  #define MONTH_D(n) \
  if (m == (uc | (months[4*(n)]<<16)|(months[4*(n)+1]<<8)|(months[4*(n)+2])))\
    return (n)

  MONTH_D(0);
  MONTH_D(1);
  MONTH_D(2);
  MONTH_D(3);
  MONTH_D(4);
  MONTH_D(5);
  MONTH_D(6);
  MONTH_D(7);
  MONTH_D(8);
  MONTH_D(9);
  MONTH_D(10);
  MONTH_D(11);

  return -1;
}

/* Parse SP 2DIGIT ":" 2DIGIT ":" 2DIGIT SP */
su_inline
int time_d(char const **ss,
	   unsigned long *hour, unsigned long *min, unsigned long *sec)
{
  char const *s = *ss;
  if (!IS_LWS(*s))
    return -1;
  skip_lws(&s);
  if (!is_digit(*s)) return -1;
  *hour = *s++ - '0'; if (is_digit(*s)) *hour = 10 * (*hour) + *s++ - '0';
  if (*s++ != ':' || !is_digit(s[0]) || !is_digit(s[1]))
    return -1;
  *min = 10 * s[0] + s[1] - 11 * '0'; s += 2;
  if (*s++ != ':' || !is_digit(s[0]) || !is_digit(s[1]))
    return -1;
  *sec = 10 * s[0] + s[1] - 11 * '0'; s += 2;
  if (*s) {
    if (!IS_LWS(*s)) return -1; skip_lws(&s);
  }
  *ss = s;
  return 0;
}

/**Decode RFC1123-date, RFC822-date or asctime-date.
 *
 * The function msg_date_d() decodes <HTTP-date>, which may have
 * different formats.
 *
 * @code
 * HTTP-date    = rfc1123-date / rfc850-date / asctime-date
 *
 * rfc1123-date = wkday "," SP date1 SP time SP "GMT"
 * date1        = 2DIGIT SP month SP 4DIGIT
 *                ; day month year (e.g., 02 Jun 1982)
 *
 * rfc850-date  = weekday "," SP date2 SP time SP "GMT"
 * date2        = 2DIGIT "-" month "-" 2DIGIT
 *                ; day-month-year (e.g., 02-Jun-82)
 *
 * asctime-date = wkday SP date3 SP time SP 4DIGIT
 * date3        = month SP ( 2DIGIT / ( SP 1DIGIT ))
 *                ; month day (e.g., Jun  2)
 *
 * time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
 *                ; 00:00:00 - 23:59:59
 *
 * wkday        = "Mon" / "Tue" / "Wed" / "Thu" / "Fri" / "Sat" / "Sun"
 * weekday      = "Monday" / "Tuesday" / "Wednesday"
 *              / "Thursday" / "Friday" / "Saturday" / "Sunday"
 * month        = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun"
 *              / "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
 * @endcode
 */
issize_t msg_date_d(char const **ss, msg_time_t *date)
{
  char const *s = *ss;
  //char const *wkday;
  char const *tz;
  unsigned long day, year, hour, min, sec;
  int mon;

  if (!IS_TOKEN(*s) || !date)
    return -1;

  //wkday = s; 
  skip_token(&s); if (*s == ',') s++;
  while (IS_LWS(*s)) s++;

  if (is_digit(*s)) {
    day = *s++ - '0'; if (is_digit(*s)) day = 10 * day + *s++ - '0';

    if (*s == ' ') {
      /* rfc1123-date =
	 wkday "," SP 2DIGIT SP month SP 4DIGIT SP time SP "GMT"
       */
      mon = month_d(++s); skip_token(&s);
      if (mon < 0 || !IS_LWS(*s)) return -1;
      s++;
      if (!is_digit(s[0]) || !is_digit(s[1]) ||
	  !is_digit(s[2]) || !is_digit(s[3]))
	return -1;
      year = 1000 * s[0] + 100 * s[1] + 10 * s[2] + s[3] - 1111 * '0'; s += 4;
    }
    else if (*s == '-') {
      /* rfc822-date =
	 weekday "," SP 2DIGIT "-" month "-" 2DIGIT SP time SP "GMT"
       */

      mon = month_d(++s);
      if (mon < 0 || s[3] != '-' ||
	  !is_digit(s[4]) || !is_digit(s[5]))
	return -1;
      year = 10 * s[4] + s[5] - 11 * '0';
      if (is_digit(s[6]) && is_digit(s[7])) {
	/* rfc2822-date =
	   weekday "," SP 2DIGIT "-" month "-" 4DIGIT SP time SP "GMT"
	*/
	year = year * 100 + 10 * s[6] + s[7] - 11 * '0';
	s += 8;
      }
      else {
	if (year >= 70)
	  year = 1900 + year;
	else
	  year = 2000 + year;
	s += 6;
      }
    }
    else
      return -1;

    if (time_d(&s, &hour, &min, &sec) < 0) return -1;
    if (*s) {
      tz = s; skip_token(&s); skip_lws(&s);
      if (!su_casenmatch(tz, "GMT", 3) && !su_casenmatch(tz, "UCT", 3))
	return -1;
    }
  }
  else {
    /* actime-date =
       wkday SP month SP ( 2DIGIT | ( SP 1DIGIT )) SP time SP 4DIGIT */
    mon = month_d(s); skip_token(&s);
    if (mon < 0 || !IS_LWS(*s)) return -1; s++;
    while (IS_LWS(*s)) s++;
    if (!is_digit(*s)) return -1;
    day = *s++ - '0'; if (is_digit(*s)) day = 10 * day + *s++ - '0';
    if (time_d(&s, &hour, &min, &sec) < 0) return -1;
    /* Accept also unix date (if it is GMT) */
    if ((s[0] == 'G' && s[1] == 'M' && s[2] == 'T' && s[3] == ' ') ||
	(s[0] == 'U' && s[1] == 'T' && s[2] == 'C' && s[3] == ' '))
      s += 4;
    else if (s[0] == 'U' && s[1] == 'T' && s[2] == ' ')
      s += 3;
    if (!is_digit(s[0]) || !is_digit(s[1]) ||
	!is_digit(s[2]) || !is_digit(s[3]))
      return -1;
    year = 1000 * s[0] + 100 * s[1] + 10 * s[2] + s[3] - 1111 * '0'; s += 4;
  }

  if (hour > 24 || min >= 60 || sec >= 60 ||
      (hour == 24 && min > 0 && sec > 0))
    return -1;

  if (day == 0 || day > days_per_months[mon]) {
    if (day != 29 || mon != 1 || !LEAP_YEAR(year))
      return -1;
  }

  if (year < EPOCH) {
    *date = 0;
  }
  else if (year > EPOCH + 135) {
    *date = 0xfdeefb80;	/* XXX: EPOCH + 135 years */
  }
  else {
    int leap_year = LEAP_YEAR(year);
    msg_time_t ydays = YEAR_DAYS(year) - YEAR_DAYS(EPOCH);

#if 0
    printf("Year %d%s starts %ld = %d - %d days since epoch (%d)\n",
	       year, leap_year ? " (leap year)" : "",
	   ydays, YEAR_DAYS(year), YEAR_DAYS(EPOCH), EPOCH);
#endif

    *date = sec + 60 *
      (min + 60 *
	   (hour + 24 *
	    (day - 1 + mon * 30 + first_day_offset[mon] +
	     (leap_year && mon > 2) + ydays)));
  }
  *ss = s;

  return 0;
}


/**Encode RFC1123-date.
 *
 * The function msg_date_e() prints @e http-date in the <rfc1123-date>
 * format. The format is as follows:
 *
 * @code
 * rfc1123-date = wkday "," SP date SP time SP "GMT"
 * wkday        = "Mon" | "Tue" | "Wed"
 *              | "Thu" | "Fri" | "Sat" | "Sun"
 * date         = 2DIGIT SP month SP 4DIGIT
 *                ; day month year (e.g., 02 Jun 1982)
 * month        = "Jan" | "Feb" | "Mar" | "Apr"
 *              | "May" | "Jun" | "Jul" | "Aug"
 *              | "Sep" | "Oct" | "Nov" | "Dec"
 * time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
 *                ; 00:00:00 - 23:59:59
 * @endcode
 *
 * @param b         buffer to print the date
 * @param bsiz      size of the buffer
 * @param http_date seconds since 01 Jan 1900.
 *
 * @return The function msg_date_e() returns the size of the formatted date.
 */
issize_t msg_date_e(char b[], isize_t bsiz, msg_time_t http_date)
{
  msg_time_t sec, min, hour, wkday, day, month, year;
  msg_time_t days_per_month, leap_year;

  sec  = http_date % 60; http_date /= 60;
  min  = http_date % 60; http_date /= 60;
  hour = http_date % 24; http_date /= 24;

  wkday = http_date % 7;
  day = http_date + YEAR_DAYS(EPOCH);
  year = EPOCH + http_date / 365;

  for (;;) {
    if (day >= YEAR_DAYS(year + 1))
      year++;
    else if (day < YEAR_DAYS(year))
      year--;
    else
      break;
  }

  day -= YEAR_DAYS(year);
  leap_year = LEAP_YEAR(year);

  month = 0, days_per_month = 31;
  while (day >= days_per_month) {
    day -= days_per_month;
    month++;
    days_per_month = days_per_months[month] + (leap_year && month == 2);
  }

  return snprintf(b, bsiz, "%s, %02ld %s %04ld %02ld:%02ld:%02ld GMT",
		  wkdays + wkday * 4, day + 1, months + month * 4,
		  year, hour, min, sec);
}


/**Decode a delta-seconds.
 *
 * The function msg_delta_d() decodes a <delta-seconds> field.
 *
 * The <delta-seconds> is defined as follows:
 * @code
 * delta-seconds  = 1*DIGIT
 * @endcode
 *
 * Note, however, that <delta-seconds> may not be larger than #MSG_TIME_MAX.
 */
issize_t msg_delta_d(char const **ss, msg_time_t *delta)
{
  char const *s = *ss;

  if (!is_digit(*s))
    return -1;

  *delta = strtoul(*ss, (char **)ss, 10);
  skip_lws(ss);

  return *ss - s;
}

/**Encode @ref msg_delta_d() "<delta-seconds>" field.
 */
issize_t msg_delta_e(char b[], isize_t bsiz, msg_time_t delta)
{
  return snprintf(b, bsiz, "%lu", (unsigned long)delta);
}

/** Decode a HTTP date or delta
 *
 * Decode a @ref msg_date_d() "<http-date>" or
 * @ref msg_delta_d() "<delta-seconds>" field.
 */
issize_t msg_date_delta_d(char const **ss,
			  msg_time_t *date,
			  msg_time_t *delta)
{
  if (delta && is_digit(**ss)) {
    return msg_delta_d(ss, delta);
  }
  else if (date && IS_TOKEN(**ss)) {
    return msg_date_d(ss, date);
  }
  return -1;
}

