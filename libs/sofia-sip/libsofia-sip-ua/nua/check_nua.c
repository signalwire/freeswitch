/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
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

/**@CFILE check2_sofia.c
 *
 * @brief Check-driven tester for Sofia SIP User Agent library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2007 Nokia Corporation.
 */

#include "config.h"

#include "test_s2.h"
#include "check_nua.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

static void usage(int exitcode)
{
  fprintf(exitcode ? stderr : stdout,
	  "usage: %s [--xml=logfile] case,...\n", s2_tester);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int i, failed = 0, selected = 0;
  int threading;
  char const *xml = NULL;
  Suite *suite = suite_create("Unit tests for Sofia-SIP UA Engine");
  SRunner *runner;

  s2_tester = "check_nua";

  s2_suite("N2");

  if (getenv("CHECK_NUA_VERBOSE"))
    s2_start_stop = strtoul(getenv("CHECK_NUA_VERBOSE"), NULL, 10);

  for (i = 1; argv[i]; i++) {
    if (su_strnmatch(argv[i], "--xml=", strlen("--xml="))) {
      xml = argv[i] + strlen("--xml=");
    }
    else if (su_strmatch(argv[i], "--xml")) {
      if (!(xml = argv[++i]))
	usage(2);
    }
    else if (su_strmatch(argv[i], "-v")) {
      s2_start_stop = 1;
    }
    else if (su_strmatch(argv[i], "-?") ||
	     su_strmatch(argv[i], "-h") ||
	     su_strmatch(argv[i], "--help"))
      usage(0);
    else {
      s2_select_tests(argv[i]);
      selected = 1;
    }
  }

  if (!selected)
    s2_select_tests(getenv("CHECK_NUA_CASES"));

  check_register_cases(suite, threading = 0);
  check_simple_cases(suite, threading = 0);
  check_session_cases(suite, threading = 0);
  check_etsi_cases(suite, threading = 0);

  check_register_cases(suite, threading = 1);
  check_session_cases(suite, threading = 1);
  check_etsi_cases(suite, threading = 1);
  check_simple_cases(suite, threading = 1);

  runner = srunner_create(suite);

  if (xml)
    srunner_set_xml(runner, argv[1]);

  srunner_run_all(runner, CK_ENV);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}
