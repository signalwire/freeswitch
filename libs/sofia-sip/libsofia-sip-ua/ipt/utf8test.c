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
 * @CFILE utf8test.c UTF8 encoding - decoding tests
 *
 * @author Pekka Pessi <pessi@research.nokia.com>
 *
 * @date Created: Tue Apr 21 15:32:38 1998 pessi
 */

#include "config.h"

#include <sofia-sip/utf8.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
  static ucs4 ucs4test0[] = {
    0x41u, 0xC1u, 0x841u, 0x10041u, 0x200041u, 0x4000041u, 0
  };
  static utf8 ucs4test1[] =
    "A"
    "\303\201"
    "\340\241\201"
    "\360\220\201\201"
    "\370\210\200\201\201"
    "\374\204\200\200\201\201";

  static ucs2 ucs2test0[] = {
    0x41u, 0xC1u, 0x841u, 0
  };
  static utf8 ucs2test1[] =
    "A"
    "\303\201"
    "\340\241\201";

  ucs4 ucs4s[1024] = { 0 };
  ucs2 ucs2s[1024] = { 0 };
  utf8 utf8s[1024] = { 0 };

  size_t len;
  int result = 0;
  int i;

  puts("testing ucs4len(ucs4test0)");

  len = ucs4len(ucs4test0);
  if (len != 6) {
    printf("ucs4len(ucs4test0) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs4declen(ucs4test1)");

  len = ucs4declen(ucs4test1);
  if (len != 6) {
    printf("ucs4declen(ucs4test1) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs4enclen(ucs4test0, 6, NULL)");

  len = ucs4enclen(ucs4test0, 6, NULL);
  if (len != 22) {
    printf("ucs4enclen(ucs4test0, 6, NULL) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs4enclen(ucs4test0, 5, NULL)");

  len = ucs4enclen(ucs4test0, 5, NULL);
  if (len != 16) {
    printf("ucs4enclen(ucs4test0, 5, NULL) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs4encode(utf8s, ucs4test0, 6, NULL)");

  if (ucs4encode(utf8s, ucs4test0, 6, NULL) != 22 ||
      strcmp((char *)utf8s, (char*)ucs4test1)) {
    printf("ucs4encode(utf8s, ucs4test0, 6, NULL) fails\n");
    result = 1;
    printf("\tutf8s=\"%s\"\n", utf8s);
  }
  else puts("OK");

  puts("testing ucs4decode(ucs4s, sizeof(ucs4s), ucs4test1)");

  if (ucs4decode(ucs4s, sizeof(ucs4s)/sizeof(*ucs4s), ucs4test1) != 6
      || ucs4cmp(ucs4s, ucs4test0)) {
    printf("ucs4decode(ucs4s, sizeof(ucs4s), ucs4test1) fails\n");
    result = 1;
    for (i = 0; i < 8; i++) {
      printf("\tucs4s[%d]=0x%x\n", i, ucs4s[i]);
    }
  }
  else puts("OK");

  /* UCS2 */

  puts("testing ucs2len(ucs2test0)");

  len = ucs2len(ucs2test0);
  if (len != 3) {
    printf("ucs2len(ucs2test0) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs2declen(ucs2test1)");

  len = ucs2declen(ucs2test1);
  if (len != 3) {
    printf("ucs2declen(ucs2test1) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs2enclen(ucs2test0, 3, NULL)");

  len = ucs2enclen(ucs2test0, 3, NULL);
  if (len != 7) {
    printf("ucs2enclen(ucs2test1, 3, NULL) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs2enclen(ucs2test0, 2, NULL)");

  len = ucs2enclen(ucs2test0, 2, NULL);
  if (len != 4) {
    printf("ucs2enclen(ucs2test1, NULL) returns %u\n", len);
    result = 1;
  }
  else puts("OK");

  puts("testing ucs2encode(utf8s, ucs2test0, 3, NULL)");

  if (ucs2encode(utf8s, ucs2test0, 3, NULL) != 7 ||
      strcmp((char *)utf8s, (char*)ucs2test1)) {
    printf("ucs2encode(utf8s, ucs2test0, 3, NULL) fails\n");
    result = 1;
    printf("\tutf8s=\"%s\"\n", utf8s);
  }
  else puts("OK");

  puts("testing ucs2decode(ucs2s, sizeof(ucs2s)/sizeof(*ucs2s), ucs2test1)");

  if (ucs2decode(ucs2s, sizeof(ucs2s)/sizeof(*ucs2s), ucs2test1) != 3 ||
      ucs2cmp(ucs2s, ucs2test0)) {
    printf("ucs2decode(ucs2s, sizeof(ucs2s)/sizeof(*ucs2s), ucs2test1) fails\n");
    result = 1;
    for (i = 0; i < 8; i++) {
      printf("\tucs2s[%d]=0x%x\n", i, ucs2s[i]);
    }
  }
  else puts("OK");


  return result;
}
