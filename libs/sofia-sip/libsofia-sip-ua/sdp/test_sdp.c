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
 * @CFILE test_sdp.c
 *
 * Simple SDP tester
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created      : Fri Feb 18 10:25:08 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sofia-sip/sdp.h"

int diff(const char *olds, const char *news, int *linep, int *pos)
{
  const char *o, *n;

  *linep = 0;

  for (o = olds, n = news; *o && *n && *o == *n ; o++, n++) {
    if (*o == '\n') ++*linep;
  }

  *pos = o - olds;

  return *o != *n;
}

int main(int argc, char *argv[])
{
  char buffer[2048];
  int  n;
  su_home_t *home = su_home_create();
  int exitcode = 1;
  FILE *f;

  if (argv[1] && strcmp(argv[1], "-"))
    f = fopen(argv[1], "rb");
  else
    f = stdin;

  n = fread(buffer, 1, sizeof(buffer) - 1, f);
  buffer[n] = 0;

  if (feof(f)) {
    sdp_parser_t *p = sdp_parse(home, buffer, n + 1, 0);

    su_home_check(home);
    su_home_check((su_home_t *)p);

    if (sdp_session(p)) {
      sdp_session_t *sdp, *dup;
      sdp_printer_t *printer;

      sdp = sdp_session(p);
      dup = sdp_session_dup(home, sdp);
      su_home_check(home);

      sdp_parser_free(p), p = NULL;
      su_home_check(home);

      printer = sdp_print(home, dup, NULL, 0, 0);
      su_home_check(home);

      if (sdp_message(printer)) {
	int line, pos;

	if (diff(buffer, sdp_message(printer), &line, &pos)) {
	  fprintf(stdout, "test_sdp:%d: messages differ:\n", line);
	  fputs(buffer, stdout);
	  fprintf(stdout, ">>>new message:\n");
	  fputs(sdp_message(printer), stdout);
	}
	else {
	  exitcode = 0;
	}
      }
      else {
	fprintf(stderr, "test_sdp: %s\n", sdp_printing_error(printer));
      }
      sdp_printer_free(printer);

      su_home_check(home);
    }
    else {
      fprintf(stderr, "test_sdp: %s\n", sdp_parsing_error(p));
      sdp_parser_free(p);
      exit(1);
    }
  }
  else {
    if (ferror(f)) {
      perror("test_sdp");
    }
    else {
      fprintf(stderr, "test_sdp: maximum length of sdp messages is %u bytes\n",
	      (unsigned)sizeof(buffer));
    }
    exit(1);
  }

  su_home_unref(home);

  return exitcode;
}
