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

/**@ingroup su_alloc
 *
 * @CFILE su_sprintf.c  su_*sprintf() functions
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 17 20:05:21 2001 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#if defined(va_copy)
/* Xyzzy */
#elif defined(__va_copy)
#define va_copy(dst, src) __va_copy((dst), (src))
#else
#define va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

#include "sofia-sip/su_alloc.h"

/**Copy a formatted string.
 *
 * The function su_vsprintf() print a string according to a @a fmt like
 * vprintf() or vsnprintf(). The resulting string is copied to a memory area
 * fresly allocated from a memory @a home. The returned string is reclaimed
 * when @a home is destroyed. It can explicitly be freed with su_free() or
 * free() if @a home is NULL.
 *
 * @param home pointer to memory home (may be NULL)
 * @param fmt format string
 * @param ap @e stdarg argument list (must match with the @a fmt format string)
 *
 * @return A pointer to a fresh copy of formatting result, or NULL upon an
 * error.
 */
char *su_vsprintf(su_home_t *home, char const *fmt, va_list ap)
{
  int n;
  size_t len;
  char *rv, s[128];
  va_list aq;

  va_copy(aq, ap);
  n = vsnprintf(s, sizeof(s), fmt, aq);
  va_end(aq);

  if (n >= 0 && (size_t)n + 1 < sizeof(s))
    return su_strdup(home, s);

  len = n > 0 ? (size_t)n + 1 : 2 * sizeof(s);

  for (rv = su_alloc(home, len);
       rv;
       rv = su_realloc(home, rv, len)) {
    va_copy(aq, ap);
    n = vsnprintf(rv, len, fmt, aq);
    va_end(aq);
    if (n > -1 && (size_t)n < len)
      break;
    if (n > -1)			/* glibc >2.1 */
      len = (size_t)n + 1;
    else			/* glibc 2.0 */
      len *= 2;

    if (len > INT_MAX)
      return (void)su_free(home, rv), NULL;
  }

  return rv;
}

/**Copy a formatted string.
 *
 * The function su_sprintf() print a string according to a @a fmt like
 * printf() or snprintf(). The resulting string is copied to a memory area
 * freshly allocated from a memory @a home. The returned string is reclaimed
 * when @a home is destroyed. It can explicitly be freed with su_free() or
 * free() if @a home is NULL.
 *
 * @param home pointer to memory home (may be NULL)
 * @param fmt format string
 * @param ... argument list (must match with the @a fmt format string)
 *
 * @return A pointer to a fresh copy of formatting result, or NULL upon an
 * error.
 */
char *su_sprintf(su_home_t *home, char const *fmt, ...)
{
  va_list ap;
  char *rv;

  va_start(ap, fmt);
  rv = su_vsprintf(home, fmt, ap);
  va_end(ap);

  return rv;
}
