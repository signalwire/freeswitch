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

#ifndef STRING0_H
/** Defined when <sofia-sip/string0.h> is included. */
#define STRING0_H

/**@file sofia-sip/string0.h
 *
 * @brief Extra string function.
 *
 * String comparison functions accepting NULL pointers: str0cmp(),
 * str0ncmp(), str0casecmp(), str0ncasecmp(). Also includes span functions
 * testing at most @a n bytes: strncspn(), strnspn().
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

#include <string.h>

SOFIA_BEGIN_DECLS

su_inline int str0cmp(char const *a, char const *b)
{
  if (a == NULL) a = "";
  if (b == NULL) b = "";
  return strcmp(a, b);
}

su_inline int str0ncmp(char const *a, char const *b, size_t n)
{
  if (a == NULL) a = "";
  if (b == NULL) b = "";
  return strncmp(a, b, n);
}

su_inline int str0casecmp(char const *a, char const *b)
{
  if (a == NULL) a = "";
  if (b == NULL) b = "";
  return strcasecmp(a, b);
}

su_inline int str0ncasecmp(char const *a, char const *b, size_t n)
{
  if (a == NULL) a = "";
  if (b == NULL) b = "";
  return strncasecmp(a, b, n);
}

#if !SU_HAVE_INLINE
SOFIAPUBFUN size_t strnspn(char const *s, size_t size, char const *term);
SOFIAPUBFUN size_t strncspn(char const *s, size_t ssize, char const *reject);
#else
su_inline size_t strnspn(char const *s, size_t ssize, char const *term)
{
  size_t n;
  size_t tsize = strlen(term);

  if (tsize == 0) {
    return 0;
  }
  else if (tsize == 1) {
    char c, t = term[0];
    for (n = 0; n < ssize && (c = s[n]) && c == t; n++)
      ;
  }
  else if (tsize == 2) {
    char c, t1 = term[0], t2 = term[1];
    for (n = 0; n < ssize && (c = s[n]) && (c == t1 || c == t2); n++)
      ;
  }
  else {
    size_t i;
    char c, t1 = term[0], t2 = term[1];
    for (n = 0; n < ssize && (c = s[n]) && (c == t1 || c == t2); n++) {
      for (i = 2; i < tsize; i++)
	if (c == term[i])
	  return n;
    }
  }

  return n;
}

su_inline size_t strncspn(char const *s, size_t ssize, char const *reject)
{
  size_t n;
  size_t rsize = strlen(reject);

  if (rsize == 0) {
    for (n = 0; n < ssize && s[n]; n++)
      ;
  }
  else if (rsize == 1) {
    char c, rej = reject[0];
    for (n = 0; n < ssize && (c = s[n]) && c != rej; n++)
      ;
  }
  else if (rsize == 2) {
    char c, rej1 = reject[0], rej2 = reject[1];
    for (n = 0; n < ssize && (c = s[n]) && c != rej1 && c != rej2; n++)
      ;
  }
  else {
    size_t i;
    char c, rej1 = reject[0], rej2 = reject[1];
    for (n = 0; n < ssize && (c = s[n]) && c != rej1 && c != rej2; n++) {
      for (i = 2; i < rsize; i++)
	if (c == reject[i])
	  return n;
    }
  }

  return n;
}

#endif

SOFIA_END_DECLS

#endif /* !STRING0_H */
