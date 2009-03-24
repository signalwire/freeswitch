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

/**@CFILE check_dlopen_sofia.c
 *
 * @brief Check dlopen()ing and dlclose() with libsofia-sip-ua
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2009 Nokia Corporation.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#if HAVE_CHECK && HAVE_LIBDL

#include <check.h>
#include <s2check.h>

#include <sofia-sip/su_types.h>
#include <dlfcn.h>

static uint64_t (*su_random64)(void);

START_TEST(load_su_uniqueid)
{
  void *sofia;
  uint64_t rnd;

  sofia = dlopen("../libsofia-sip-ua/.libs/libsofia-sip-ua.so", RTLD_NOW);
  su_random64 = dlsym(sofia, "su_random64");
  fail_unless(su_random64 != NULL);
  rnd = su_random64();
  fail_unless(sofia !=	NULL);
  fail_unless(dlclose(sofia) == 0);
}
END_TEST

TCase *dl_tcase(void)
{
  TCase *tc = tcase_create("1 - dlopen/dlclose");

  tcase_add_test(tc, load_su_uniqueid);

  return tc;
}

/* ---------------------------------------------------------------------- */

static void usage(int exitcode)
{
  fprintf(exitcode ? stderr : stdout,
	  "usage: check_dlopen_sofia [--xml=logfile] case,...\n");
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int i, failed = 0;

  Suite *suite = suite_create("dlopen()ing and dlclose() with libsofia-sip-ua");
  SRunner *runner;
  char const *xml = NULL;

  s2_select_tests(getenv("CHECK_CASES"));

  for (i = 1; argv[i]; i++) {
    if (strncmp(argv[i], "--xml=", strlen("--xml=")) == 0) {
      xml = argv[i] + strlen("--xml=");
    }
    else if (strcmp(argv[i], "--xml") == 0) {
      if (!(xml = argv[++i]))
	usage(2);
    }
    else if (strcmp(argv[i], "-?") == 0 ||
	     strcmp(argv[i], "-h") == 0 ||
	     strcmp(argv[i], "--help") == 0)
      usage(0);
    else
      s2_select_tests(argv[i]);
  }

  suite_add_tcase(suite, dl_tcase());

  runner = srunner_create(suite);
  if (xml)
    srunner_set_xml(runner, xml);
  srunner_run_all(runner, CK_ENV);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}

#else
int main(void) { return 77; }
#endif
