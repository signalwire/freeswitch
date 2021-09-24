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

/**@page sip-date Print or parse SIP date
 *
 * @section synopsis Synopsis
 *
 * <tt>sip-date [-n] [SIP-date | [YYYYy] [DDd] [HHh] [MMm] [SS[s]]]</tt>
 *
 * @section description Description
 *
 * @em sip-date is an utility for printing a SIP date (in the format
 * specified by RFC 1123, but the timezone must always be GMT) or parsing a
 * given SIP date. The date can be given as a SIP date or by giving year,
 * day, hour, minutes and seconds separately.
 *
 * @section options Options
 *
 * The @em sip-date utility takes options as follows:
 * <dl>
 * <dt>-n</dt>
 * <dd>The @em sip-date utility prints the date as seconds elapsed since
 * epoch (01 Jan 1900 00:00:00).
 * </dd>
 * </dl>
 *
 * @section examples Examples
 *
 * You want to convert current time to SIP date:
 * @code
 * $ sip-date
 * @endcode
 * You want to find out how many seconds there was in 1900's:
 * @code
 * $ siptime -n 2000y
 * 3155673600
 * @endcode
 *
 * @section bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section author Author
 * Pekka Pessi <Pekka -dot- Pessi -at- nokia -dot- com>
 *
 * @section copyright Copyright
 * Copyright (C) 2005 Nokia Corporation.
 *
 * This program is free software; see the source for copying conditions.
 * There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/msg_date.h>

void usage(void)
{
  fprintf(stderr,
	  "usage: sip-date [-n] "
          "[SIP-date | [YYYYy] [DDd] [HHh] [MMm] [SS[s]]]\n");
  exit(1);
}

/* Epoch year. */
#define EPOCH 1900
/* Day number of New Year Day of given year */
#define YEAR_DAYS(y) \
  (((y)-1) * 365 + ((y)-1) / 4 - ((y)-1) / 100 + ((y)-1) / 400)

int main(int ac, char *av[])
{
  int numeric = 0;
  sip_time_t t = 0, t2 = 0;
  char const *s;
  char buf[1024];

  if (av[1] && strcmp(av[1], "-n") == 0)
    av++, numeric = 1;

  if (av[1] && (strcmp(av[1], "-?") == 0 || strcmp(av[1], "-h") == 0))
    usage();

  if ((s = av[1])) {
    size_t n, m;
    /* Concatenate all arguments with a space in between them */
    for (n = 0; (s = av[1]); av++) {
      m = strlen(s);
      if (n + m + 2 > sizeof(buf))
	exit(1);
      memcpy(buf + n, s, m);
      buf[n + m] = ' ';
      n += m + 1;
    }
    buf[n] = '\0';

    s = buf;

    if (s[0] < '0' || s[0] > '9') {
      if (msg_date_d(&s, &t) < 0) {
	fprintf(stderr, "sip-date: %s is not valid time\n", s);
	exit(1);
      }
    }
    else {
      for (; *s; ) {
	if (msg_delta_d(&s, &t2) < 0)
	  usage();

	switch (*s) {
	case 'y': t2 = YEAR_DAYS(t2) - YEAR_DAYS(EPOCH);
	  /*FALLTHROUGH*/
	case 'd': t += t2 * 24 * 60 * 60; s++; break;
	case 'h': t += t2 * 60 * 60; s++; break;
	case 'm': t += t2 * 60; s++; break;
	case 's':
	  s++;
	default:
	  t += t2;
	  break;
	}

	while (*s && *s == ' ')
	  s++;
      }
    }
  }
  else {
    t = sip_now();
  }

  if (numeric) {
    msg_delta_e(buf, sizeof(buf), t);
  }
  else {
    msg_date_e(buf, sizeof(buf), t);
  }

  puts(buf);

  return 0;
}
