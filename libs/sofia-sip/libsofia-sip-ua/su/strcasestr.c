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

/**@internal @file strcasestr.c
 * @brief Backup implementation of strcasestr()
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#include "config.h"

#include <ctype.h>
#include <stddef.h>

/* Naive implementation of strcasestr() */
char *strcasestr(const char *haystack,
		 const char *needle)
{
  unsigned char lcn, ucn;
  unsigned i;

  if (haystack == NULL || needle == NULL)
    return NULL;

  lcn = ucn = needle[0];
  if (isupper(lcn))
    lcn = tolower(lcn);
  else if (islower(ucn))
    ucn = toupper(ucn);

  if (lcn == 0)
    return (char *)haystack;

  while (haystack[0] != 0) {
    if (lcn == haystack[0] || ucn == haystack[0]) {
      for (i = 1; ; i++) {
	char n = needle[i], h = haystack[i];
	if (n == 0)
	  return (char *)haystack;
	if (h == 0)
	  return NULL;
	if (isupper(n)) n = tolower(n);
	if (isupper(h)) h = tolower(h);
	if (n != h)
	  break;
      }
    }
    haystack++;
  }

  return NULL;		/* Not found */
}
