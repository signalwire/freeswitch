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

/**@internal @file token64.c
 *
 * Token encoding.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr  3 10:45:47 2002 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#include "sofia-sip/token64.h"

static const char code[65] =
"0123456789-abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/** Encode data as a SIP/HTTP token.
 *
 * @note
 * A token is case-independent, so this is really not a good idea.
 * Use msg_random_token() instead.
 */
isize_t token64_e(char b[], isize_t bsiz, void const *data, isize_t dlen)
{
  size_t i, n, slack;
  unsigned char const *h = data;
  char *s = b, *end = b + bsiz;
  long w;

  if (dlen <= 0) {
    if (bsiz && b) *b = '\0';
    return 0;
  }

  n = (8 * dlen + 5) / 6;
  if (bsiz == 0 || b == NULL)
    return n;

  if (b + n >= end)
    dlen = 6 * bsiz / 8;
  else
    end = b + n + 1;

  slack = dlen % 3;
  dlen -= slack;

  for (i = 0; i < dlen; i += 3, s += 4) {
    unsigned char h0 = h[i], h1 = h[i + 1], h2 = h[i + 2];

    s[0] = code[h0 >> 2];
    s[1] = code[((h0 << 4)|(h1 >> 4)) & 63];
    s[2] = code[((h1 << 4)|(h2 >> 6)) & 63];
    s[3] = code[(h2) & 63];
  }

  if (slack) {
    if (slack == 2)
      w = (h[i] << 16) | (h[i+1] << 8);
    else
      w = (h[i] << 16);

    if (s < end) *s++ = code[(w >> 18) & 63];
    if (s < end) *s++ = code[(w >> 12) & 63];
    if (s < end && slack == 2) *s++ = code[(w >> 6) & 63];
  }

  if (s < end)
    *s++ = '\0';
  else
    end[-1] = '\0';

  assert(end == s);

  return n;
}
