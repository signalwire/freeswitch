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

/**@CFILE sip_reason.c
 * @brief @Reason header.
 *
 * The file @b sip_reason.c contains implementation of header class for
 * SIP header @Reason.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

/**@SIP_HEADER sip_reason Reason Header
 *
 * The Reason header is used to indicate why a SIP request was issued or why
 * a provisional response was sent. It can be used with HRPF scenarios. It
 * is defined in @RFC3326 as follows:
 *
 * @code
 *   Reason            =  "Reason" HCOLON reason-value *(COMMA reason-value)
 *   reason-value      =  protocol *(SEMI reason-params)
 *   protocol          =  "SIP" / "Q.850" / token
 *   reason-params     =  protocol-cause / reason-text
 *                        / reason-extension
 *   protocol-cause    =  "cause" EQUAL cause
 *   cause             =  1*DIGIT
 *   reason-text       =  "text" EQUAL quoted-string
 *   reason-extension  =  generic-param
 * @endcode
 *
 * The parsed Reason header is stored in #sip_reason_t structure.
 */

/**@ingroup sip_reason
 * @typedef typedef struct sip_reason_s sip_reason_t;
 *
 * The structure #sip_reason_t contains representation of SIP @Reason header.
 *
 * The #sip_reason_t is defined as follows:
 * @code
 * typedef struct sip_reason_s
 * {
 *   sip_common_t        re_common[1]; // Common fragment info
 *   sip_reason_t       *re_next;      // Link to next <reason-value>
 *   char const         *re_protocol;  // Protocol
 *   msg_param_t const  *re_params;    // List of reason parameters
 *   char const         *re_cause;     // Value of cause parameter
 *   char const         *re_text;      // Value of text parameter
 * } sip_reason_t;
 * @endcode
 */

static msg_xtra_f sip_reason_dup_xtra;
static msg_dup_f sip_reason_dup_one;
static msg_update_f sip_reason_update;

msg_hclass_t sip_reason_class[] =
SIP_HEADER_CLASS(reason, "Reason", "", re_params, append, reason);

issize_t sip_reason_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_reason_t *re = (sip_reason_t *)h;
  size_t n;

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  re->re_protocol = s;
  if ((n = span_token(s)) == 0)
    return -1;
  s += n; while (IS_LWS(*s)) *s++ = '\0';
  if (*s == ';' && msg_params_d(home, &s, &re->re_params) < 0)
    return -1;

  return msg_parse_next_field(home, h, s, slen);
}

issize_t sip_reason_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  char *end = b + bsiz, *b0 = b;
  sip_reason_t const *re = h->sh_reason;

  assert(sip_is_reason(h));
  MSG_STRING_E(b, end, re->re_protocol);
  MSG_PARAMS_E(b, end, re->re_params, flags);

  return b - b0;
}

isize_t sip_reason_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_reason_t const *re = h->sh_reason;

  MSG_PARAMS_SIZE(offset, re->re_params);
  offset += MSG_STRING_SIZE(re->re_protocol);

  return offset;
}

/** Duplicate one #sip_reason_t object */
char *sip_reason_dup_one(sip_header_t *dst, sip_header_t const *src,
			char *b, isize_t xtra)
{
  sip_reason_t *re_dst = dst->sh_reason;
  sip_reason_t const *re_src = src->sh_reason;

  char *end = b + xtra;
  b = msg_params_dup(&re_dst->re_params, re_src->re_params, b, xtra);
  MSG_STRING_DUP(b, re_dst->re_protocol, re_src->re_protocol);
  assert(b <= end); (void)end;

  return b;
}

/** Update parameter values for @Reason header */
static int sip_reason_update(msg_common_t *h,
			     char const *name, isize_t namelen,
			     char const *value)
{
  sip_reason_t *re = (sip_reason_t *)h;

  if (name == NULL) {
    re->re_cause = NULL;
    re->re_text = NULL;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

  else if (MATCH(cause)) {
    re->re_cause = value;
  }
  else if (MATCH(text)) {
    re->re_text = value;
  }

#undef MATCH

  return 0;
}
