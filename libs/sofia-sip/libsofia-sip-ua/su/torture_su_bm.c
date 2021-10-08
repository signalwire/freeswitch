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
 * @file torture_su_bm.c
 * @brief Test string search with Boyer-Moore algorithm
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @date Created: Sun Apr 17 21:02:10 2005 ppessi
 */

#include "config.h"

#define TSTFLAGS tstflags

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sofia-sip/tstdef.h>

char const *name = "torture_su_bm";
int tstflags;

#define TORTURELOG TEST_LOG

#include "su_bm.c"

int test_bm(void)
{
  BEGIN();

  {
    char const hs[] =
      "A Boyer-Moore string searching test consisting of a Long String";
    char const *s;

    s = bm_memmem(hs, strlen(hs), "sting", 5, NULL);

    TEST_S(s, hs + 41);

    s = bm_memmem(hs, strlen(hs), "String", 6, NULL);
    TEST_S(s, hs + 57);

    s = bm_memmem(hs, strlen(hs), "S", 1, NULL);
    TEST_S(s, hs + 57);

    s = bm_memmem(hs, strlen(hs), "M", 1, NULL);
    TEST_S(s, hs + 8);

    s = bm_memcasemem(hs, strlen(hs), "M", 1, NULL);
    TEST_S(s, hs + 8);

    s = bm_memcasemem(hs, strlen(hs), "trings", 6, NULL);
    TEST_1(s == NULL);

    s = bm_memcasemem(hs, strlen(hs), "String", 6, NULL);
    TEST_S(s, hs + 14);

    s = bm_memcasemem(hs, strlen(hs), "StRiNg", 6, NULL);
    TEST_S(s, hs + 14);

    s = bm_memcasemem(hs, strlen(hs), "OnG", 3, NULL);
    TEST_S(s, hs + 53);

    /* Special cases */
    TEST_1(bm_memmem(hs, strlen(hs), "", 0, NULL) == hs);
    TEST_1(bm_memmem(NULL, strlen(hs), "ong", 3, NULL) == NULL);
    TEST_1(bm_memmem(hs, strlen(hs), NULL, 1, NULL) == NULL);
    TEST_1(bm_memmem("ong", 3, hs, strlen(hs), NULL) == NULL);
    TEST_1(bm_memmem(hs, 0, "ong", 3, NULL) == NULL);
    TEST_1(bm_memmem(hs, strlen(hs), "Z", 1, NULL) == NULL);

    TEST_1(bm_memcasemem(hs, strlen(hs), "", 0, NULL) == hs);
    TEST_1(bm_memcasemem(NULL, strlen(hs), "OnG", 3, NULL) == NULL);
    TEST_1(bm_memcasemem(hs, strlen(hs), NULL, 1, NULL) == NULL);
    TEST_1(bm_memcasemem("OnG", 3, hs, strlen(hs), NULL) == NULL);
    TEST_1(bm_memcasemem(hs, 0, "OnG", 3, NULL) == NULL);
    TEST_1(bm_memcasemem(hs, strlen(hs), "Z", 1, NULL) == NULL);
  }

  END();
}

int test_bm_long(void)
{
  BEGIN();

  {
    char const hs[] =
"A Boyer-Moore string searching test consisting of a Very Long String\n"

"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Integer felis. "
"Suspendisse potenti. Morbi malesuada erat eget enim. Sed dui lorem, aliquam "
"eu, lobortis eget, dapibus vitae, velit. Cras non purus. Suspendisse massa. "
"Curabitur gravida condimentum massa. Donec nunc magna, lacinia non, "
"pellentesque ac, laoreet vel, eros. Praesent lectus leo, vestibulum eu, "
"tempus eu, ullamcorper tristique, mi. Duis fringilla ultricies lacus. Ut "
"non pede. Donec id libero. Cum sociis natoque penatibus et magnis dis "
"parturient montes, nascetur ridiculus mus. Phasellus bibendum.\n"

"Vestibulum turpis. Nunc euismod. Maecenas venenatis, purus at pharetra "
"ultrices, orci orci blandit nisl, eget vulputate enim tortor sed nunc. "
"Proin sit amet elit. Donec ut justo. In quis nisi. Praesent posuere. "
"Maecenas porta. Curabitur pharetra. Class aptent taciti sociosqu ad litora "
"torquent per conubia nostra, per inceptos hymenaeos. Donec suscipit ligula. "
"Quisque facilisis ante eget mi. Nunc ac est.\n"

"Quisque in sapien eget justo aliquam laoreet. Nullam ultricies est id dolor. "
"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Suspendisse ante "
"velit, eleifend at, ullamcorper ut, rutrum et, ipsum. Mauris luctus, tellus "
"non elementum convallis, nunc ipsum hendrerit lectus, ut lacinia elit nulla "
"ac tellus. In sit amet velit. Maecenas non dolor. Sed commodo diam a pede. "
"Ut non pede. Vestibulum condimentum turpis vel lacus consectetuer dictum. "
"Nulla ullamcorper mi eu pede. Donec mauris tortor, facilisis vitae, "
"hendrerit nec, lobortis nec, eros. Nam sit amet mi. Ut pharetra, orci nec "
"porta convallis, lacus velit blandit sapien, luctus nonummy lacus dolor vel "
"sapien. Suspendisse placerat. Donec ante turpis, volutpat eu, hendrerit "
"vel, eleifend ac, arcu. Nulla facilisi. Sed faucibus facilisis lectus. "
"Aliquam congue justo nec dui.\n"

"Donec dapibus dui sed nisl. Proin congue. Curabitur placerat diam id eros. "
"Pellentesque vitae nulla. Quisque at lorem et dolor auctor consequat. Sed "
"sed tellus non nibh imperdiet venenatis. Integer ultrices dapibus nisi. "
"Aenean vehicula malesuada risus. Fusce egestas malesuada leo. In "
"ullamcorper pretium lorem. Vestibulum ante ipsum primis in faucibus orci "
"luctus et ultrices posuere cubilia Curae;\n"

"Phasellus congue. Morbi lectus arcu, mattis non, pulvinar et, condimentum "
"non, mi. Suspendisse vestibulum nunc eu neque. Sed rutrum felis aliquam "
"urna. Ut tincidunt orci vitae ipsum. Nullam eros. Quisque augue. Quisque "
"lacinia. Nunc ligula diam, nonummy a, porta in, tristique quis, leo. "
"Phasellus nunc nulla, fringilla vel, lacinia et, suscipit a, turpis. "
"Integer a est. Curabitur mauris lacus, vehicula sit amet, sodales vel, "
"iaculis vitae, massa. Nam diam est, ultrices vitae, varius et, tempor a, "
"leo. Class aptent taciti sociosqu ad litora torquent per conubia nostra, "
"per inceptos hymenaeos. Fusce felis nibh, ullamcorper non, malesuada eget, "
      "facilisis vel, purus.\n";

char const needle[] =
"Proin congue. Curabitur placerat diam id eros. "
"Pellentesque vitae nulla. Quisque at lorem et dolor auctor consequat. Sed "
"sed tellus non nibh imperdiet venenatis. Integer ultrices dapibus nisi. "
"Aenean vehicula malesuada risus. Fusce egestas malesuada leo. In "
"ullamcorper pretium lorem. Vestibulum ante ipsum primis in faucibus orci "
"luctus et ultrices posuere cubilia Curae;\n";

char const Needle[] =
"PROIN CONGUE. CURABITUR PLACERAT DIAM ID EROS. "
"PELLENTESQUE VITAE NULLA. QUISQUE AT LOREM ET DOLOR AUCTOR CONSEQUAT. SED "
"SED TELLUS NON NIBH IMPERDIET VENENATIS. INTEGER ULTRICES DAPIBUS NISI. "
"AENEAN VEHICULA MALESUADA RISUS. FUSCE EGESTAS MALESUADA LEO. IN "
"ULLAMCORPER PRETIUM LOREM. VESTIBULUM ANTE IPSUM PRIMIS IN FAUCIBUS ORCI "
"LUCTUS ET ULTRICES POSUERE CUBILIA CURAE;\n";

    size_t nlen = strlen(needle);

    bm_fwd_table_t *fwd;

    char const *s;

    s = bm_memmem(hs, strlen(hs), needle, nlen, NULL);

    TEST_S(s, hs + 1919);

    fwd = bm_memmem_study(needle, nlen);

    s = bm_memmem(hs, strlen(hs), needle, nlen, fwd);

    free(fwd);

    TEST_S(s, hs + 1919);

    TEST_1(bm_memmem(hs, strlen(hs), Needle, nlen, NULL) == 0);

    s = bm_memcasemem(hs, strlen(hs), Needle, nlen, NULL);

    fwd = bm_memcasemem_study(Needle, nlen);

    s = bm_memcasemem(hs, strlen(hs), Needle, nlen, fwd);

    TEST_S(s, hs + 1919);

    free(fwd);
  }

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

  /* Set our name */
  if (strchr(argv[0], '/'))
    name = strrchr(argv[0], '/') + 1;
  else
    name = argv[0];

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "-l") == 0)
      tstflags |= tst_log;
    else
      usage(1);
  }

  retval |= test_bm(); fflush(stdout);
  retval |= test_bm_long(); fflush(stdout);

  return retval;
}
