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

/**@ingroup test_msg
 * @CFILE msg_name_hash.c
 *
 * Calculate hash for given header name.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Tue Aug 20 16:27:01 EEST 2002 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>

#include <sofia-sip/su_alloc.h>

#include <sofia-sip/msg_mclass_hash.h>

int main(int argc, char *argv[])
{
  if (!argv[1] || argv[2]) {
    fprintf(stderr, "usage: msg_name_hash Header-Name\n");
    exit(1);
  }
  printf("%d\n", msg_header_name_hash(argv[1], NULL));
  exit(0);
}
