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

/**@CFILE ucs4.c UCS-4 routines
 *
 * @author Pekka Pessi <pessi@research.nokia.com>
 *
 * @date Created: Tue Apr 21 15:32:02 1998 pessi
 */

#include "config.h"

#include <sofia-sip/utf8.h>
#include "utf8internal.h"

/*
 * Decode utf8 string into ucs4 string,
 * return number of ucs4 characters decoded
 */
size_t ucs4decode(ucs4 *dst, size_t dst_size, const utf8 *s)
{
  ucs4 v, *d = dst;

  if (s) while (*s) {
    if (dst_size == 0)
      break;
    if (IS_UTF8_S1(s))
      v = UCS4_S1(s), s += 1;
    else if (IS_UTF8_S2(s))
      v = UCS4_S2(s), s += 2;
    else if (IS_UTF8_S3(s))
      v = UCS4_S3(s), s += 3;
    else if (IS_UTF8_S4(s))
      v = UCS4_S4(s), s += 4;
    else if (IS_UTF8_S5(s))
      v = UCS4_S5(s), s += 5;
    else if (IS_UTF8_S6(s))
      v = UCS4_S6(s), s += 6;
    else {
      s++;
      continue;			/* skip illegal characters */
    }
    *d++ = v;
    dst_size--;
  };

  if (dst_size)
    *d = 0;

  return d - dst;
}

/*
 * Encode ucs4 string into utf8 string,
 * return number of utf8 bytes encoded including final zero
 *
 * 'quote' may contain an optional quoting table containing
 * non-zero for all ASCII characters to quote
 *
 */
size_t ucs4encode(utf8 *dst, const ucs4 *s, size_t n, const char quote[128])
{
  utf8 *d = dst;
  ucs4 c;

  if (s) while (n-- > 0) {
    c = *s++;

    if (IS_UCS4_1(c)) {
      if (quote && quote[c]) {
	UTF8_S2(d, c);
	d += 2;
      }
      else {
	if (!c)			/* zero must be represented as UTF8_2 */
	  break;
	UTF8_S1(d, c);
	d += 1;
      }
    }
    else if (IS_UCS4_2(c)) {
      UTF8_S2(d, c);
      d += 2;
    }
    else if (IS_UCS4_3(c)) {
      UTF8_S3(d, c);
      d += 3;
    }
    else if (IS_UCS4_4(c)) {
      UTF8_S4(d, c);
      d += 4;
    }
    else if (IS_UCS4_5(c)) {
      UTF8_S5(d, c);
      d += 5;
    }
    else if (IS_UCS4_6(c)) {
      UTF8_S6(d, c);
      d += 6;
    }
    else {
      /* skip illegal (negative) characters */
    }
  }

  *d++ = 0;
  return d - dst;
}

/*
 * Length of UCS4 string decoded from UTF8
 */
size_t ucs4declen(const utf8 *s)
{
  size_t len = 0;
  size_t errors = 0;		/* errors */

  UTF8_ANALYZE(s, len, len, len, len, errors);

  if (errors)
    return 0;

  return len;
}

/*
 * Length of UTF8 encoding of a UCS4 string, including final zero
 */
size_t ucs4enclen(const ucs4 *s, size_t n, const char quote[128])
{
  size_t len = 1;
  ucs4 c;

  while (n-- > 0) {
    c = *s++;
    if (c < 0x80u)
      if (quote && quote[c])
	len += 2;
      else {
	if (!c) break;
	len += 1;
      }
    else if (c < 0x800u)
      len += 2;
    else if (c < 0x10000u)
      len += 3;
    else if (c < 0x200000u)
      len += 4;
    else if (c < 0x4000000u)
      len += 5;
    else if (c < 0x80000000u)
      len += 6;
  }

  return len;
}

/*
 * Length of UCS4 string (number of non-zero UCS4 characters before zero)
 */
size_t ucs4len(ucs4 const *s)
{
  size_t len = 0;

  if (s) while (*s++)
    len++;

  return len;
}

/*
 * Compare UCS4 string
 */
int ucs4cmp(ucs4 const *s1, ucs4 const *s2)
{
  int retval;

  while ((retval = (*s1 - *s2)) && (*s1++) && (*s2++))
    ;

  return retval;
}

/*
 * Compare UCS4 string prefix
 */
int ucs4ncmp(ucs4 const *s1, ucs4 const *s2, size_t n)
{
  int retval = 0;

  while (n-- > 0 && (retval = (*s1 - *s2)) && (*s1++) && (*s2++))
    ;

  return retval;
}
