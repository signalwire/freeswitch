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

/**@ingroup msg_headers
 * @CFILE msg_header_make.c
 *
 * Creating message headers from strings.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 14:06:34 2001 ppessi
 *
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>

#include "sofia-sip/msg.h"
#include "sofia-sip/bnf.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/msg_header.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#if defined(va_copy)
/* Xyzzy */
#elif defined(__va_copy)
#define va_copy(dst, src) __va_copy((dst), (src))
#else
#define va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

#include <assert.h>

/** Make a header from a value string. */
msg_header_t *msg_header_make(su_home_t *home,
			      msg_hclass_t *hc,
			      char const *s)
{
  size_t xtra;
  msg_header_t *h;
  int normal = hc->hc_name ||
    (hc->hc_hash != msg_payload_hash &&
     hc->hc_hash != msg_separator_hash &&
     hc->hc_hash != msg_error_hash);

  if (s == NULL)
    return NULL;

  /* For normal headers, strip LWS from both ends */
  if (normal)
    skip_lws(&s);
  xtra = strlen(s);
  if (normal)
    while (xtra > 0 && IS_LWS(s[xtra - 1]))
      xtra--;

  h = msg_header_alloc(home, hc, xtra + 1);

  if (h) {
    char *b = MSG_HEADER_DATA(h);

    strncpy(b, s, xtra)[xtra] = 0;

    if (hc->hc_parse(home, h, b, xtra) == -1) {
      /* Note: parsing function is responsible to free
	 everything it has allocated (like parameter lists) */
      /* XXX - except header structures */
      su_free(home, h), h = NULL;
    }
  }

  return h;
}

/** Make a MSG header with formatting provided. */
msg_header_t *msg_header_vformat(su_home_t *home,
				msg_hclass_t *hc,
				char const *fmt,
				va_list ap)
{
  msg_header_t *h;

  int n;
  size_t xtra = 64;		/* reasonable default */

  /* Quick path */
  if (!fmt || !strchr(fmt, '%'))
    return msg_header_make(home, hc, fmt);

  /* Another quickie */
  if (strcmp(fmt, "%s") == 0) {
    fmt = va_arg(ap, char const *);
    return msg_header_make(home, hc, fmt);
  }

  if (!(h = msg_header_alloc(home, hc, xtra)))
    return NULL;

  for (;;) {
    va_list aq;

    va_copy(aq, ap);
    n = vsnprintf(MSG_HEADER_DATA(h), xtra, fmt, aq);
    va_end(aq);

    if (n >= 0 && (size_t)n < xtra)
      break;

    /* Try again with more space */
    su_free(home, h);

    if (xtra >= INT_MAX)
      return NULL;

    if (n >= 0)
      xtra = n + 1; /* precisely what is needed */
    else
      xtra *= 2;    /* glibc 2.0 - twice the old size */

    if (xtra > INT_MAX)
      xtra = INT_MAX;

    if (!(h = msg_header_alloc(home, hc, xtra)))
      return NULL;
  }

  if (hc->hc_parse(home, h, MSG_HEADER_DATA(h), (size_t)n) == -1) {
    /* Note: parsing function is responsible to free
       everything it has allocated (like parameter lists) */
    su_free(home, h), h = NULL;
  }

  return h;
}

msg_header_t *msg_header_format(su_home_t *home,
				msg_hclass_t *hc,
				char const *fmt,
				...)
{
  msg_header_t *h;
  va_list ap;

  va_start(ap, fmt);

  h = msg_header_vformat(home, hc, fmt, ap);

  va_end(ap);

  return h;
}
