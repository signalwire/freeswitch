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

/**@internal @file memccpy.c
 * @brief The memccpy() replacement function.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Nov 17 17:45:51 EET 2005 ppessi
 */

#include "config.h"

#include <string.h>
#include <limits.h>

/**Copy memory until @a c is found.
 *
 * Copies no more than @a n bytes from memory area @a src to memory area @a
 * dest, stopping after the character @a c is copied and found.
 *
 * @param dest       pointer to destination area
 * @param src        pointer to source area
 * @param c          terminating byte
 * @param n          size of destination area
 *
 * @return
 * Returns a pointer to the next character in @a dest after @a c,
 * or NULL if @a c was not found in the first @a n characters of @a src.
 */
void *memccpy(void *dest, const void *src, int c, size_t n)
{
  char *d;
  char const *s;

  if (!src || !dest)
    return dest;

  for (d = dest, s = src; n-- > 0;) {
    if (c == (*d++ = *s++))
      return d;
  }

  return NULL;
}

