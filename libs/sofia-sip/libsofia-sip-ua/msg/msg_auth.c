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

/**@ingroup msg_parser
 * @CFILE msg_auth.c
 *
 * Functions for handling authentication-related headers
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 *
 */

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <sofia-sip/msg.h>
#include <sofia-sip/msg_parser.h>
#include <sofia-sip/msg_header.h>
#include <sofia-sip/bnf.h>

#if 0
/**
 * Scan and compact an authentication parameter.
 *
 * Scan an authentication parameter, which has syntax as follows:
 * @code
 * auth-item = auth-param | base64-string
 * auth-param = token [ "=" (token | quoted-string)]
 * @endcode
 *
 * Parameters:
 * @param s      pointer to string to scan
 *
 * @return Number of characters scanned, or zero upon an error.
 */
static size_t msg_auth_item_scan(char *start)
{
  char *p, *s;

  p = s = start;

  /* XXX */

  return start - s;
}
#endif

/* ====================================================================== */
/*
 * auth           = ("Authorization" | "Encryption" |
 *                   "Proxy-Authenticate" | "Proxy-Authorization" |
 *                   "Response-Key" | "WWW-Authenticate") ":"
 *                    scheme 1*SP #auth-param
 * scheme         = token
 * auth-param     = token | token "=" token | token "=" quoted-string
 */

/** Parse security headers. */
issize_t msg_auth_d(su_home_t *home,
		    msg_header_t *h,
		    char *s,
		    isize_t slen)
{
  msg_auth_t *au = (msg_auth_t *)h;

  au->au_scheme = s;

  skip_token(&s);
  if (!IS_LWS(*s)) return -1;
  *s++ = '\0';			/* NUL-terminate scheme */

  return msg_commalist_d(home, &s, (msg_param_t **)&au->au_params,
			 NULL /* msg_auth_item_scan */);
}

issize_t msg_auth_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  msg_auth_t const *au = (msg_auth_t *)h;
  int compact = MSG_IS_COMPACT(f);
  char *b0 = b, *end = b + bsiz;

  MSG_STRING_E(b, end, au->au_scheme);
  if (au->au_params) {
    MSG_CHAR_E(b, end, ' ');
    MSG_COMMALIST_E(b, end, au->au_params, compact);
  }
  MSG_TERM_E(b, end);

  return b - b0;
}

/**@internal
 * Extra size of a msg_auth_t object.
 *
 * This function calculates extra size required by a msg_auth_t object.
 *
 * @param a pointer to a msg_auth_t object
 *
 * @return
 *   Size of strings related to msg_auth_t object.
 */
isize_t msg_auth_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_auth_t const *au = (msg_auth_t *)h;

  MSG_PARAMS_SIZE(offset, au->au_params);
  offset += MSG_STRING_SIZE(au->au_scheme);

  return offset;
}

/**Duplicate one msg_auth_t object. */
char *msg_auth_dup_one(msg_header_t *dst,
		       msg_header_t const *src,
		       char *b,
		       isize_t xtra)
{
  msg_auth_t *au = (msg_auth_t *)dst;
  msg_auth_t const *o = (msg_auth_t const *)src;
  char *end = b + xtra;

  b = msg_params_dup(&au->au_params, o->au_params, b, xtra);
  MSG_STRING_DUP(b, au->au_scheme, o->au_scheme);

  assert(b <= end); (void)end;

  return b;
}
