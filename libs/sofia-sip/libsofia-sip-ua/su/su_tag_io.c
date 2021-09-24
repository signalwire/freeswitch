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

/**@SU_TAG
 *
 * @CFILE su_tag_io.c
 * @brief Printing tag lists.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 12:12:27 2001 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/su_tag_inline.h>

/** Print tags */
void tl_print(FILE *f, char const *title, tagi_t const lst[])
{
  fputs(title, f);

  for (; lst; lst = t_next(lst)) {
    char buffer[4096];
    char const *fmt = "   %s\n";
    int n;

    buffer[0] = '\0';

    n = t_snprintf(lst, buffer, sizeof(buffer));

    if (n + 1 < (int)sizeof(buffer)) {
      if (n > 0 && buffer[n - 1] == '\n')
	fmt = "   %s";
    }
    else
      buffer[sizeof(buffer) - 1] = '\0';
    fprintf(f, fmt, buffer);
  }
}
