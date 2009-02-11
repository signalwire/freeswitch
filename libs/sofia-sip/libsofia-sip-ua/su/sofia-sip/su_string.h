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

#ifndef SOFIA_SIP_SU_STRING_H
/** Defined when <sofia-sip/su_string.h> is included. */
#define SOFIA_SIP_SU_STRING_H

/**@file sofia-sip/su_string.h
 *
 * @brief String functions for Sofia-SIP.
 *
 * Various string comparison functions also accepting NULL pointer as empty
 * string:
 * - su_strcmp(),
 * - su_strncmp(),
 * - su_strcasecmp() (comparison with US-ASCII case folding to lower case),
 * - su_strncasecmp() (comparison with US-ASCII case folding to lower case)
 * - su_casematch() (match token with US-ASCII case folding to lower case)
 * - su_casenmatch() (match token with US-ASCII case folding to lower case)
 *
 * Also includes span functions testing at most @a n bytes:
 * - su_strncspn()
 * - su_strnspn().
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

#include <string.h>

SOFIA_BEGIN_DECLS

su_inline int su_strcmp(char const *a, char const *b)
{
  return strcmp(a ? a : "", b ? b : "");
}

su_inline int su_strncmp(char const *a, char const *b, size_t n)
{
  return strncmp(a ? a : "", b ? b : "", n);
}

SOFIAPUBFUN char *su_strcasestr(const char *haystack, const char *needle);

SOFIAPUBFUN int su_strcasecmp(char const *s1, char const *s2);
SOFIAPUBFUN int su_strncasecmp(char const *s1, char const *s2, size_t n);

SOFIAPUBFUN int su_strmatch(char const *str, char const *with);
SOFIAPUBFUN int su_strnmatch(char const *str, char const *with, size_t n);

SOFIAPUBFUN int su_casematch(char const *s1, char const *with);
SOFIAPUBFUN int su_casenmatch(char const *s1, char const *with, size_t n);

SOFIAPUBFUN size_t su_strnspn(char const *s, size_t size, char const *term);
SOFIAPUBFUN size_t su_strncspn(char const *s, size_t ssize, char const *reject);

SOFIAPUBFUN size_t su_memspn(const void *mem, size_t memlen,
			     const void *accept, size_t acceptlen);
SOFIAPUBFUN size_t su_memcspn(const void *mem, size_t memlen,
			      const void *reject, size_t rejectlen);

SOFIA_END_DECLS

#endif /* !SOFIA_SIP_SU_STRING_H */
