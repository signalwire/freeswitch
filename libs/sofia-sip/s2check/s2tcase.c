/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#include "config.h"

#if HAVE_CHECK

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <check.h>

#if HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

static char const * const default_patterns[] = { "*", NULL };
static char const * const *test_patterns = default_patterns;

/** tcase_add_test() replacement.
 *
 * A special version of tcase_add_test() that inserts test function into
 * tcase only if its name matches given pattern.
 */
void s2_tcase_add_test(TCase *tc, TFun tf, char const *name,
		       int signo, int start, int end)
{
  char const * const *patterns;

#if HAVE_FNMATCH_H
  for (patterns = test_patterns; *patterns; patterns++) {
    if (!fnmatch(*patterns, name, 0)) {
      if (strcmp(*patterns, "*")) {
	printf("%s: selected\n", name);
      }
      _tcase_add_test(tc, tf, name, signo, start, end);
      return;
    }
  }
#else
  for (patterns = test_patterns; *patterns; patterns++) {
    if (!strcmp(*patterns, name) || !strcmp(*patterns, "*")) {
      if (strcmp(*patterns, "*")) {
	printf("%s: selected\n", name);
      }
      _tcase_add_test(tc, tf, name, signo, start, end);
      return;
    }
  }
#endif
}

/** Select tests based on pattern.
 *
 * Allow easy selection of test cases to run. Pattern can be a shell glob,
 * or consists of multiple comma-separated patterns.
 */
void s2_select_tests(char const *pattern)
{
  size_t i, n;
  char *cases, *s, **patterns;

  if (!pattern)
    return;

  cases = strdup(pattern);

  /* Count commas */
  for (i = 2, s = cases; (s = strchr(s, ',')); s++)
    i++;

  if (test_patterns != default_patterns) {
    patterns = (char **)test_patterns;
    for (n = 0; patterns[n]; n++)
      ;
    patterns = realloc(patterns, (n + i) * (sizeof *patterns));
  }
  else {
    n = 0;
    patterns = malloc(i * (sizeof *patterns));
  }

  assert(patterns);
  memset(&patterns[n], 0, i * (sizeof *patterns));

  /* Split by commas */
  for (i = n, s = cases;; i++) {
    patterns[i] = s;
    if (s == NULL)
      break;
    s = strchr(s, ',');
    if (s)
      *s++ = '\0';
  }

  test_patterns = (char const * const *)patterns;
}

#endif	/* HAVE_CHECK */
