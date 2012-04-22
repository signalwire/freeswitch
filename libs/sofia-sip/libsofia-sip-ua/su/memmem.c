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

/**@internal @file memmem.c
 *
 * @brief Backup implementation of memmem()
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sat Apr 12 19:32:33 2003 ppessi
 *
 */

#include "config.h"

#include <string.h>

/* Naive implementation of memmem() */
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen)
{
  size_t i;
  char const *hs = haystack;

  if (needlelen == 0)
    return (void *)haystack;

  if (needlelen > haystacklen || haystack == NULL || needle == NULL)
    return NULL;

  for (i = 0; i <= haystacklen - needlelen; i++) {
    if (memcmp(hs + i, needle, needlelen) == 0)
      return (void *)(hs + i);
  }

  return NULL;
}
