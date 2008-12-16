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

/**@internal @file memcspn.c
 * @brief The memcspn() replacement function.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Nov 17 17:45:51 EET 2005 ppessi
 */

#include "config.h"

#include <string.h>
#include <limits.h>

/**Search memory for bytes not in a given set.
 *
 * The memcspn() function calculates the length of the memory area @a mem
 * which consists entirely of bytes not in @a reject.
 *
 * @param mem        pointer to memory area
 * @param memlen     size of @a mem in bytes
 * @param reject     pointer to table containing bytes to reject
 * @param rejectlen  size of @a reject table
 *
 * @return
 * The memspn() function returns the number of bytes in the memory area @a
 * which consists entirely of bytes not in @a reject.
 * @par
 * If @a rejectlen is 0, or @a reject is NULL, it returns @a memlen, size of
 * the memory area.
 */
size_t memcspn(const void *mem, size_t memlen,
	       const void *reject, size_t rejectlen)
{
  size_t i;

  unsigned char const *m = mem, *r = reject;

  char rejected[UCHAR_MAX + 1];

  if (rejectlen == 0 || reject == 0)
    return memlen;

  if (mem == NULL || memlen == 0)
    return 0;

  memset(rejected, 0, sizeof rejected);

  for (i = 0; i < rejectlen; i++)
    rejected[r[i]] = 1;

  for (i = 0; i < memlen; i++)
    if (rejected[m[i]])
      break;

  return i;
}

