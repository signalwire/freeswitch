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

/**@CFILE check_sofia.c
 *
 * @brief Check-driven tester for Sofia SIP User Agent library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2007 Nokia Corporation.
 */

#include "config.h"

#include "check_sofia.h"

#include <stdlib.h>

/* Globals used by <sofia-sip/tstdef.h> */
int tstflags;
char const name[] = "check_sofia";

int main(int argc, char *argv[])
{
  int failed = 0;

  Suite *suite;
  SRunner *runner;

  suite = suite_for_nua();
  runner = srunner_create(suite);

  if (argv[1]) {
    srunner_set_xml(runner, argv[1]);
  }
  srunner_run_all(runner, CK_NORMAL);

  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}

