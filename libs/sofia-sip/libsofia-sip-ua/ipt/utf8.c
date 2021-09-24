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

/**@CFILE utf8.c
 *
 * utf8 string handling.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 21 15:32:02 1998 pessi
 */

#include "config.h"

#include <sofia-sip/utf8.h>
#include "utf8internal.h"

#ifndef _WIN32
#include <assert.h>
#endif

/** Width of an UTF8 character cell (1, 2 or 4 bytes) */
size_t utf8_width(const utf8 *s)
{
  size_t w8 = 0, w16 = 0, w32 = 0;
  size_t errors = 0;		/* errors */

  UTF8_ANALYZE(s, w8, w8, w16, w32, errors);

  if (errors)
    return 0;

  return w32 ? 4 : (w16 ? 2 : 1);
}

/** Convert UTF8 string @a s to ISO-Latin-1 string @a dst. */
size_t ucs18decode(char *dst, size_t dst_size, const utf8 *s)
{
#ifndef _WIN32
  assert(!"implemented");
#endif
  return 0;
}

/** Convert ISO-Latin-1 string @a s to UTF8 string in @a dst. */
size_t ucs1encode(utf8 *dst, const ucs1 *s, size_t n, const char quote[128])
{
#ifndef _WIN32
  assert(!"implemented");
#endif
  return 0;
}

/** Calculate number of characters in UTF8 string @a s. */
size_t ucs1declen(const utf8 *s)
{
#ifndef _WIN32
  assert(!"implemented");
#endif
  return 0;
}

/** Calculate length of UTF8 encoding of string @a s. */
size_t ucs1enclen(const ucs1 *s, size_t n, const char quote[128])
{
#ifndef _WIN32
  assert(!"implemented");
#endif
  return 0;
}
