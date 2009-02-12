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

#include "check_nua.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#include "test_s2.h"

int main(int argc, char *argv[])
{
  int failed = 0;
  int threading;

  SRunner *runner;

  Suite *suite = suite_create("Unit tests for Sofia-SIP UA Engine");

  s2_tester = "check_nua";

  if (getenv("CHECK_NUA_VERBOSE"))
    s2_start_stop = strtoul(getenv("CHECK_NUA_VERBOSE"), NULL, 10);

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

  if (argv[1]) {
    srunner_set_xml(runner, argv[1]);
  }
  srunner_run_all(runner, CK_ENV);

  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}
