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

/**@internal @file memspn.c
 *
 * The memspn() replacement function.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sat Apr 12 19:32:33 2003 ppessi
 *
 */

#include "config.h"

#include <string.h>
#include <limits.h>

/**Scan memory for a set of bytes.
 *
 * The memspn() function calculates the length of the memory area @a mem
 * which consists entirely of bytes in @a accept.
 *
 * @param mem        pointer to memory area
 * @param memlen     size of @a mem in bytes
 * @param accept     pointer to table containing bytes to accept
 * @param acceptlen  size of @a accept table
 *
 * @return
 * The memspn() function returns the number of bbytes in the memory area @a
 * which consists entirely of bytes in @a accept.
 */
size_t memspn(const void *mem, size_t memlen,
	      const void *accept, size_t acceptlen)
{
  size_t i;

  unsigned char const *m = mem, *a = accept;

  char accepted[UCHAR_MAX + 1];

  if (mem == NULL || memlen == 0 || acceptlen == 0 || accept == NULL)
    return 0;

  memset(accepted, 0, sizeof accepted);

  for (i = 0; i < acceptlen; i++)
    accepted[a[i]] = 1;

  for (i = 0; i < memlen; i++)
    if (!accepted[m[i]])
      break;

  return i;
}
