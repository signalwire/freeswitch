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

/**@internal
 *
 * @CFILE test_date.c
 *
 * Tester for SIP date parser
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Wed Mar 21 19:12:13 2001 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <sofia-sip/su_string.h>
#include <stddef.h>
#include <stdlib.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/msg_date.h>

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: test_date [SIP-date] "
	  "[YYYYy][DDd][HHh][MMm][SS[s]]\n");
  exit(exitcode);
}

int main(int ac, char *av[])
{
  int i;
  sip_time_t t, delta, t2;
  char const *s;
  int verbatim = 0, retval = 0;

  t = ((31 + 27) * 24) * 60 * 60;
  delta = (365 * 24 + 6) * 60 * 60;

  if (su_strmatch(av[1], "-v"))
    verbatim = 1, av++;

  if ((s = av[1])) {
    if (msg_date_d(&s, &t) < 0) {
      fprintf(stderr, "test_date: %s is not valid time\n", s);
      exit(1);
    }

    if ((s = av[2])) {
      for (delta = 0; *s; ) {
	t2 = 0;
	msg_delta_d(&s, &t2);

	switch (*s++) {
	case 'y': delta += t2 * (365 * 24 + 6) * 60 * 60; break;
	case 'd': delta += t2 * 24 * 60 * 60; break;
	case 'h': delta += t2 * 60 * 60; break;
	case 'm': delta += t2 * 60; break;
	case '\0': --s;		/* FALLTHROUGH */
	case 's': delta += t2; break;
	default:
	  fprintf(stderr, "test_date: %s is not valid time offset\n" , av[2]);
	  usage(1);
	  break;
	}
      }
    }
  }

  for (i = 0; i < 20; i++) {
    char buf[80];

    msg_date_e(buf, sizeof(buf), t);

    if (verbatim)
      printf("%08lx is %s\n", t, buf);

    s = buf, t2 = 0;
    if (msg_date_d(&s, &t2) < 0) {
      fprintf(stderr, "test_date: decoding %s failed\n", buf);
      retval = 1;
      break;
    }
    else if (t2 != t) {
      fprintf(stderr, "test_date: %lu != %lu\n", t, t2);
      retval = 1;
      break;
    }
    t += delta;
  }

  return retval;
}
