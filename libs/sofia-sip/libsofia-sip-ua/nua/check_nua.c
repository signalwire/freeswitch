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

static char const * const default_patterns[] = { "*", NULL };
static char const * const *test_patterns = default_patterns;

void check_nua_tcase_add_test(TCase *tc, TFun tf, char const *name)
{
  char const * const *patterns;

#if HAVE_FNMATCH_H
  for (patterns = test_patterns; *patterns; patterns++) {
    if (!fnmatch(*patterns, name, 0)) {
      if (strcmp(*patterns, "*")) {
	printf("%s: running\n", name);
      }
      _tcase_add_test(tc, tf, name, 0, 0, 1);
      return;
    }
  }
#else
  for (patterns = test_patterns; *patterns; patterns++) {
    if (!strcmp(*patterns, name) || !strcmp(*patterns, "*")) {
      if (strcmp(*patterns, "*")) {
	printf("%s: running\n", name);
      }
      _tcase_add_test(tc, tf, name, 0, 0, 1);
      return;
    }
  }
#endif
}

int main(int argc, char *argv[])
{
  int failed = 0;

  Suite *suite = suite_create("Unit tests for Sofia-SIP UA Engine");
  SRunner *runner;

  if (getenv("CHECK_NUA_CASES")) {
    size_t i;
    char *s, **patterns;
    char *cases = strdup(getenv("CHECK_NUA_CASES"));

    /* Count commas */
    for (i = 2, s = cases; (s = strchr(s, ',')); s++, i++);

    patterns = calloc(i, sizeof *patterns);

    /* Split by commas */
    for (i = 0, s = cases;; i++) {
      patterns[i] = s;
      if (s == NULL)
	break;
      s = strchr(s, ',');
      if (s)
	*s++ = '\0';
    }

    test_patterns = (char const * const *)patterns;
  }

  check_register_cases(suite);
  check_session_cases(suite);

  runner = srunner_create(suite);

  if (argv[1]) {
    srunner_set_xml(runner, argv[1]);
  }
  srunner_run_all(runner, CK_ENV);

  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}
