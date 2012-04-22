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

/**@CFILE auth_common.c
 *
 * Functions common to both client and server authentication.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Original Created: Thu Feb 22 12:10:37 2001 ppessi
 * @date Created: Wed May 17 13:37:50 EEST 2006 ppessi
 */

#include "config.h"

#include "sofia-sip/auth_common.h"
#include "sofia-sip/msg_header.h"
#include <sofia-sip/su_string.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

su_inline int has_token(char const *qstring, char const *token);

/**
 * Parse authentication parameters.
 *
 * The function auth_get_params() searches for the authentication parameters
 * in @a params. The parameter list @a params is seached for each parameter
 * given in in vararg section, and if it is found, its value is assigned to
 * the given address.
 *
 * @note The field name should contain the equal ("=") sign.
 *
 * @return
 * The function auth_get_params() returns number of parameters found in
 * params, or -1 upon an error.
 */
issize_t auth_get_params(su_home_t *home,
			 char const * const params[], ...
			 /* char const *fmt, char const **return_value */)
{
  int n, j;
  size_t len, namelen;
  char const *fmt, *expected;
  char const *value, *p, **return_value;
  va_list(ap);

  assert(params);

  if (!params) return -1;

  va_start(ap, params);

  for (n = 0; (fmt = va_arg(ap, char const *));) {
    return_value = va_arg(ap, char const **);
    len = strlen(fmt);
    if (!len)
      continue;
    namelen = strcspn(fmt, "=");
    expected = fmt + namelen + 1;
    value = NULL;

    if (expected[0]) {
      /* value match: format is name=expected,
	 if expected is found in parameter value,
	 return non-NULL pointer in *return_value */
      for (j = 0; (p = params[j++]);) {
	if (su_casematch(p, fmt)) {
	  /* Matched the whole parameter with fmt name=expected */
	  value = p;
	  break;
	}
	else if (!su_casenmatch(p, fmt, namelen) ||
		 p[namelen] != '=')
	  continue;

	p = p + namelen + 1;

	if (p[0] == '"' && has_token(p, expected)) {
	  /* Quoted parameter value has expected value,
	   * e.g., qop=auth matches qop="auth,auth-int" */
	  value = p;
	  break;
	}
	else if (su_casematch(p, expected)) {
	  /* Parameter value matches with extected value
	   * e.g., qop=auth matches qop=auth */
	  value = p;
	  break;
	}
      }
    }
    else {
      /* format is name= , return unquoted parameter value after = */
      for (j = 0; (p = params[j++]);) {
	if (!su_casenmatch(p, fmt, len))
	  continue;

	if (p[len] == '"')
	  value = msg_unquote_dup(home, p + len);
	else
	  value = su_strdup(home, p + len);

	if (value == NULL) {
	  va_end(ap);
	  return -1;
	}

	break;
      }
    }

    if (value) {
      *return_value = value;
      n++;
    }
  }

  va_end(ap);

  return n;
}

int auth_struct_copy(void *dst, void const *src, isize_t s_size)
{
  int d_size = *(int *)dst;

  if (d_size < 0)
    return -1;

  if ((size_t)d_size > s_size) {
    memcpy(dst, src, s_size);
    memset((char *)dst + s_size, 0, d_size - s_size);
  }
  else {
    memcpy(dst, src, d_size);
    *(int *)dst = d_size;
  }
  return 0;
}

su_inline int has_token(char const *qstring, char const *token)
{
  size_t n = strlen(token);
  char const *q;

  q = su_strcasestr(qstring, token);

  return (q &&
	  (q[n] == 0 || strchr("\", \t", q[n])) &&
	  (q == qstring || strchr("\", \t", q[-1])));
}


/** Compare two strings, even if they are quoted */
int auth_strcmp(char const *quoted, char const *unquoted)
{
  size_t i, j;

  if (quoted[0] != '"')
    return strcmp(quoted, unquoted);

  /* Compare quoted with unquoted  */

  for (i = 1, j = 0; ; i++, j++) {
    char q = quoted[i], u = unquoted[j];

    if (q == '"')
      q = '\0';
    else if (q == '\\' && u != '\0')
      q = quoted[i++];

    if (q - u)
      return q - u;

    if (q == '\0')
      return 0;
  }
}

