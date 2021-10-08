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

/**@CFILE sip_session.c
 * @brief Session Timer SIP headers.
 *
 * The file @b sip_session.c contains implementation of header classes for
 * session-timer-related SIP headers @SessionExpires and @MinSE.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Sep 13 21:24:15 EEST 2001 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/msg_date.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_session_expires Session-Expires Header
 *
 * The Session-Expires header is used to convey the lifetime of the session.
 * Its syntax is defined in @RFC4028 as follows:
 *
 * @code
 *      Session-Expires  =  ("Session-Expires" | "x") HCOLON delta-seconds
 *                           *(SEMI se-params)
 *      se-params        = refresher-param / generic-param
 *      refresher-param  = "refresher" EQUAL  ("uas" / "uac")
 * @endcode
 *
 * The parsed Session-Expires header is stored in #sip_session_expires_t structure.
 */

/**@ingroup sip_session_expires
 * @typedef typedef struct sip_session_expires_s sip_session_expires_t;
 *
 * The structure #sip_session_expires_t contains representation of the SIP
 * @SessionExpires header.
 *
 * The #sip_session_expires_t is defined as follows:
 * @code
 * typedef struct sip_session_expires_s
 * {
 *  sip_common_t    x_common[1];
 *  sip_unknown_t  *x_next;
 *  unsigned long   x_delta; //Delta Seconds
 *  msg_param_t    *x_params;
 *  char const     *x_refresher; //Who will send the refresh UAS or UAC
 * } sip_session_expires_t;
 * @endcode
 */

static msg_xtra_f sip_session_expires_dup_xtra;
static msg_dup_f sip_session_expires_dup_one;
static msg_update_f sip_session_expires_update;

msg_hclass_t sip_session_expires_class[] =
SIP_HEADER_CLASS(session_expires, "Session-Expires", "x", x_params, single,
		 session_expires);

issize_t sip_session_expires_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_session_expires_t *x = h->sh_session_expires;

  if (msg_delta_d((char const **) &s, &x->x_delta) < 0)
    return -1;
  if (*s == ';') {
    if (msg_params_d(home, &s, &x->x_params) < 0 || *s)
      return -1;
     x->x_refresher = msg_params_find(x->x_params, "refresher");
  }
  return 0;
}

issize_t sip_session_expires_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *end = b + bsiz, *b0 = b;
  int n = 0;
  sip_session_expires_t const *o = h->sh_session_expires;

  n = snprintf(b, bsiz, "%lu", o->x_delta);
  b += n;
  MSG_PARAMS_E(b, end, o->x_params, flags);

  return b - b0;
}

isize_t sip_session_expires_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_session_expires_t const *o = h->sh_session_expires;

  MSG_PARAMS_SIZE(offset, o->x_params);

  return offset;
}

/** Duplicate one #sip_session_expires_t object */
char *sip_session_expires_dup_one(sip_header_t *dst, sip_header_t const *src,
				  char *b, isize_t xtra)
{
  sip_session_expires_t *o_dst = dst->sh_session_expires;
  sip_session_expires_t const *o_src = src->sh_session_expires;

  char *end = b + xtra;
  b = msg_params_dup(&o_dst->x_params, o_src->x_params, b, xtra);
  o_dst->x_delta = o_src->x_delta;
  assert(b <= end); (void)end;

  return b;
}

/** Update parameters in @SessionExpires header. */
static int sip_session_expires_update(msg_common_t *h,
				      char const *name, isize_t namelen,
				      char const *value)
{
  sip_session_expires_t *x = (sip_session_expires_t *)h;

  if (name == NULL) {
    x->x_refresher = NULL;
  }
  else if (namelen == strlen("refresher") &&
	   su_casenmatch(name, "refresher", namelen)) {
    x->x_refresher = value;
  }

  return 0;
}



/**@SIP_HEADER sip_min_se Min-SE Header
 *
 * The Min-SE header is used to indicate the minimum value for the session
 * interval. Its syntax is defined in @RFC4028 as follows:
 *
 * @code
 *      MMin-SE  =  "Min-SE" HCOLON delta-seconds *(SEMI generic-param)
 * @endcode
 *
 * The parsed Min-SE header is stored in #sip_min_se_t structure.
 */

/**@ingroup sip_min_se
 * @typedef typedef struct sip_min_se_s sip_min_se_t;
 *
 * The structure #sip_min_se_t contains representation of the SIP
 * @MinSE header.
 *
 * The #sip_min_se_t is defined as follows:
 * @code
 * typedef struct sip_min_se_s
 * {
 *   sip_common_t    min_common[1];
 *   sip_unknown_t  *min_next;
 *   unsigned long   min_delta;   // Delta seconds
 *   sip_params_t   *min_params;  // List of extension parameters
 * } sip_min_se_t;
 * @endcode
 */

static msg_xtra_f sip_min_se_dup_xtra;
static msg_dup_f sip_min_se_dup_one;
#define sip_min_se_update NULL

msg_hclass_t sip_min_se_class[] =
SIP_HEADER_CLASS(min_se, "Min-SE", "", min_params, single, min_se);

issize_t sip_min_se_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_min_se_t *min = h->sh_min_se;

  if (msg_delta_d((char const **) &s, &min->min_delta) < 0)
    return -1;
  if (*s == ';') {
    if (msg_params_d(home, &s, &min->min_params) < 0 || *s)
      return -1;
  }

  return 0;
}

issize_t sip_min_se_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *end = b + bsiz, *b0 = b;
  int n = 0;
  sip_min_se_t const *o = (sip_min_se_t *)h;

  n = snprintf(b, bsiz, "%lu", o->min_delta);
  b += n;
  MSG_PARAMS_E(b, end, o->min_params, flags);

  return b - b0;
}

isize_t sip_min_se_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_min_se_t const *o = (sip_min_se_t *)h;

  MSG_PARAMS_SIZE(offset, o->min_params);

  return offset;
}

/** Duplicate one #sip_min_se_t object */
char *sip_min_se_dup_one(sip_header_t *dst, sip_header_t const *src,
			char *b, isize_t xtra)
{
  sip_min_se_t *o_dst = (sip_min_se_t *)dst;
  sip_min_se_t const *o_src = (sip_min_se_t *)src;

  char *end = b + xtra;
  b = msg_params_dup(&o_dst->min_params, o_src->min_params, b, xtra);
  o_dst->min_delta = o_src->min_delta;
  assert(b <= end); (void)end;

  return b;
}
