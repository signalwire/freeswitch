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

/**
 * @file torture_base64.c
 * @brief Test BASE64 encoding/decoding
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com> \n
 *
 * @date Created: Tue Feb  1 13:29:09 EET 2005 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sofia-sip/base64.h"

int tstflags = 0;
#define TSTFLAGS tstflags

char const *name = "torture_base64";

#include <sofia-sip/tstdef.h>

char const constant[] = "not changed";

int test_encoding(void)
{
  char buffer[32];

  BEGIN();

  TEST_SIZE(base64_e(buffer, sizeof buffer, "\0\020\203", 3), 4);
  TEST_S(buffer, "ABCD");

  strcpy(buffer + 5, "not changed");
  TEST_SIZE(base64_e(buffer, 5, "\0\020\203", 3), 4);
  TEST_S(buffer + 5, "not changed");
  TEST_S(buffer, "ABCD");

  strcpy(buffer + 4, "not changed");
  TEST_SIZE(base64_e(buffer, 4, "\0\020\203", 3), 4);
  TEST_S(buffer + 4, "not changed");
  TEST_S(buffer, "ABC");

  strcpy(buffer + 3, "not changed");
  TEST_SIZE(base64_e(buffer, 3, "\0\020\203", 3), 4);
  TEST_S(buffer + 3, "not changed");
  TEST_S(buffer, "AB");

  strcpy(buffer + 2, "not changed");
  TEST_SIZE(base64_e(buffer, 2, "\0\020\203", 3), 4);
  TEST_S(buffer + 2, "not changed");
  TEST_S(buffer, "A");

  strcpy(buffer + 1, "not changed");
  TEST_SIZE(base64_e(buffer, 1, "\0\020\203", 3), 4);
  TEST_S(buffer + 1, "not changed");
  TEST_S(buffer, "");

  strcpy(buffer + 5, "not changed");
  TEST_SIZE(base64_e(buffer, 5, "\0\020", 2), 4);
  TEST_S(buffer + 5, "not changed");
  TEST_S(buffer, "ABA=");

  strcpy(buffer + 4, "not changed");
  TEST_SIZE(base64_e(buffer, 4, "\0\020", 2), 4);
  TEST_S(buffer + 4, "not changed");
  TEST_S(buffer, "ABA");

  strcpy(buffer + 3, "not changed");
  TEST_SIZE(base64_e(buffer, 3, "\0\020", 2), 4);
  TEST_S(buffer + 3, "not changed");
  TEST_S(buffer, "AB");

  strcpy(buffer + 2, "not changed");
  TEST_SIZE(base64_e(buffer, 2, "\0\020", 2), 4);
  TEST_S(buffer + 2, "not changed");
  TEST_S(buffer, "A");

  strcpy(buffer + 1, "not changed");
  TEST_SIZE(base64_e(buffer, 1, "\0\020", 2), 4);
  TEST_S(buffer + 1, "not changed");
  TEST_S(buffer, "");

  strcpy(buffer + 5, "not changed");
  TEST_SIZE(base64_e(buffer, 5, "\0", 1), 4);
  TEST_S(buffer + 5, "not changed");
  TEST_S(buffer, "AA==");

  strcpy(buffer + 4, "not changed");
  TEST_SIZE(base64_e(buffer, 4, "\0", 1), 4);
  TEST_S(buffer + 4, "not changed");
  TEST_S(buffer, "AA=");

  strcpy(buffer + 3, "not changed");
  TEST_SIZE(base64_e(buffer, 3, "\0", 1), 4);
  TEST_S(buffer + 3, "not changed");
  TEST_S(buffer, "AA");

  strcpy(buffer + 2, "not changed");
  TEST_SIZE(base64_e(buffer, 2, "\0", 1), 4);
  TEST_S(buffer + 2, "not changed");
  TEST_S(buffer, "A");

  strcpy(buffer + 1, "not changed");
  TEST_SIZE(base64_e(buffer, 1, "\0", 1), 4);
  TEST_S(buffer + 1, "not changed");
  TEST_S(buffer, "");

  END();
}

int test_decoding(void)
{
  char buffer[32];

  BEGIN();

  strcpy(buffer + 0, "not changed");
  TEST_SIZE(base64_d((void *)buffer, 0, "ABCD"), 3);
  TEST_S(buffer + 0, "not changed");

  TEST_SIZE(base64_d((void *)buffer, 3, "ABCD"), 3);
  TEST_M(buffer, "\0\020\203", 3);

  TEST_SIZE(base64_d(NULL, 3, "ABCD"), 3);

  TEST_SIZE(base64_d((void *)buffer, 3, "A B C D !!"), 3);
  TEST_M(buffer, "\0\020\203", 3);

  END();
}


void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

  retval |= test_encoding(); fflush(stdout);
  retval |= test_decoding(); fflush(stdout);

  return retval;
}
