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

/**@CFILE sip_caller_prefs.c
 * @brief SIP headers related to Caller Preferences
 *
 * Implementation of header classes for Caller-Preferences-related SIP
 * headers @AcceptContact, @RejectContact, and @RequestDisposition.
 *
 * @author Remeres Jacobs <remeres.jacobs@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Th 23.01.2003
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_request_disposition Request-Disposition Header
 *
 * The Request-Disposition header syntax is defined in
 * @RFC3841 section 10 as follows:
 *
 * @code
 *      Request-Disposition  =  ( "Request-Disposition" | "d" ) HCOLON
 *                              directive *(COMMA directive)
 *      directive            =  proxy-directive / cancel-directive /
 *                              fork-directive / recurse-directive /
 *                              parallel-directive / queue-directive)
 *      proxy-directive      =  "proxy" / "redirect"
 *      cancel-directive     =  "cancel" / "no-cancel"
 *      fork-directive       =  "fork" / "no-fork"
 *      recurse-directive    =  "recurse" / "no-recurse"
 *      parallel-directive   =  "parallel" / "sequential"
 *      queue-directive      =  "queue" / "no-queue"
 * @endcode
 *
 *
 * The parsed Request-Disposition header
 * is stored in #sip_request_disposition_t structure.
 */

/**@ingroup sip_request_disposition
 * @typedef typedef struct sip_request_disposition_s sip_request_disposition_t;
 *
 * The structure #sip_request_disposition_t contains representation of
 * @RequestDisposition header.
 *
 * The #sip_request_disposition_t is defined as follows:
 * @code
 * typedef struct sip_request_disposition_s
 * {
 *   sip_common_t        rd_common[1];   // Common fragment info
 *   sip_unknown_t      *rd_next;	 // Link to next (dummy)
 *   msg_param_t        *rd_items;       // List of directives
 * } sip_request_disposition_t;
 * @endcode
 */

static msg_xtra_f sip_request_disposition_dup_xtra;
static msg_dup_f sip_request_disposition_dup_one;
#define sip_request_disposition_update NULL

msg_hclass_t sip_request_disposition_class[] =
SIP_HEADER_CLASS(request_disposition, "Request-Disposition", "d", rd_items,
		 list, request_disposition);

issize_t sip_request_disposition_d(su_home_t *home, sip_header_t *h,
				   char *s, isize_t slen)
{
  sip_request_disposition_t *rd = h->sh_request_disposition;

  return msg_commalist_d(home, &s, &rd->rd_items, msg_token_scan);
}


issize_t sip_request_disposition_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *end = b + bsiz, *b0 = b;
  sip_request_disposition_t const *o = h->sh_request_disposition;

  assert(sip_is_request_disposition(h));

  MSG_COMMALIST_E(b, end, o->rd_items, flags);

  return b - b0;
}


isize_t sip_request_disposition_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_request_disposition_t const *o = h->sh_request_disposition;

  MSG_PARAMS_SIZE(offset, o->rd_items);

  return offset;
}


/** Duplicate one #sip_request_disposition_t object */
char *sip_request_disposition_dup_one(sip_header_t *dst,
				      sip_header_t const *src,
				      char *b, isize_t xtra)
{
  char *end = b + xtra;
  sip_request_disposition_t *o_dst = dst->sh_request_disposition;
  sip_request_disposition_t const *o_src = src->sh_request_disposition;
  msg_param_t const **dst_items = (msg_param_t const **)&o_dst->rd_items;

  b = msg_params_dup(dst_items, o_src->rd_items, b, xtra);

  assert(b <= end); (void)end;

  return b;
}

/* ====================================================================== */

/**@ingroup sip_caller_prefs
 *
 * Add a parameter to a @AcceptContact or @RejectContact header object.
 *
 * @note This function does not duplicate @p param.
 *
 * @param home   memory home
 * @param cp     pointer to #sip_accept_contact_t or #sip_reject_contact_t
 * @param param  parameter string
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @deprecated Use msg_header_replace_param() instead.
 */
int sip_caller_prefs_add_param(su_home_t *home,
			       sip_caller_prefs_t *cp,
			       char const *param)
{
  return  msg_header_replace_param(home, cp->cp_common, param);
}

static
size_t span_attribute_value(char *s)
{
  size_t n;

  n = span_token_lws(s);
  if (n > 0 && s[n] == '=') {
    n += 1 + span_lws(s + n + 1);
    if (s[n] == '"')
      n += span_quoted(s + n);
    else
      n += span_token(s + n);
    n += span_lws(s + n);
  }

  return n;
}

static
issize_t sip_caller_prefs_d(su_home_t *home, sip_header_t *h,
			    char *s, isize_t slen)
{
  sip_caller_prefs_t *cp = (sip_caller_prefs_t *)h;
  url_t url[1];
  char const *ignore = NULL;
  int kludge = 0;

  assert(h);

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  /* Kludge: support PoC IS spec with a typo... */
  if (su_casenmatch(s, "*,", 2))
    s[1] = ';',  kludge = 0;
  else if (s[0] != '*' && s[0] != '<') {
    /* Kludge: missing URL -  */
    size_t n = span_attribute_value(s);
    kludge = n > 0 && (s[n] == '\0' || s[n] == ',' || s[n] == ';');
  }

  if (kludge) {
    if (msg_any_list_d(home, &s, (msg_param_t **)&cp->cp_params,
		       msg_attribute_value_scanner, ';') == -1)
      return -1;
  }
  /* Parse params (and ignore display name and url) */
  else if (sip_name_addr_d(home, &s, &ignore, url, &cp->cp_params, NULL)
	   == -1)
    return -1;
  /* Be liberal... */
  /* if (url->url_type != url_any)
     return -1; */

  return msg_parse_next_field(home, h, s, slen);
}

static
issize_t sip_caller_prefs_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_caller_prefs_t const *cp = h->sh_caller_prefs;
  char *b0 = b, *end = b + bsiz;

  MSG_CHAR_E(b, end, '*');
  MSG_PARAMS_E(b, end, cp->cp_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}


static
isize_t sip_caller_prefs_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_caller_prefs_t const *cp = h->sh_caller_prefs;

  MSG_PARAMS_SIZE(offset, cp->cp_params);

  return offset;
}

static
char *sip_caller_prefs_dup_one(sip_header_t *dst, sip_header_t const *src,
			      char *b, isize_t xtra)
{
  char *end = b + xtra;
  sip_caller_prefs_t *cp = dst->sh_caller_prefs;
  sip_caller_prefs_t const *o = src->sh_caller_prefs;

  b = msg_params_dup(&cp->cp_params, o->cp_params, b, xtra);

  assert(b <= end); (void)end;

  return b;
}

/**@SIP_HEADER sip_accept_contact Accept-Contact Header
 *
 * The Accept-Contact syntax is defined in @RFC3841 section 10 as follows:
 *
 * @code
 *    Accept-Contact  =  ("Accept-Contact" / "a") HCOLON ac-value
 *                       *(COMMA ac-value)
 *    ac-value        =  "*" *(SEMI ac-params)
 *    ac-params       =  feature-param / req-param
 *                        / explicit-param / generic-param
 *                        ;;feature param from RFC 3840
 *                        ;;generic-param from RFC 3261
 *    req-param       =  "require"
 *    explicit-param  =  "explicit"
 * @endcode
 *
 * Despite the BNF, there MUST NOT be more than one req-param or
 * explicit-param in an ac-params. Furthermore, there can only be one
 * instance of any feature tag in feature-param.
 *
 * @sa @RFC3840, @RFC3841, sip_contact_accept(), sip_contact_score().
 *
 * The parsed Accept-Contact header
 * is stored in #sip_accept_contact_t structure.
 */

/**@ingroup sip_accept_contact
 * @typedef struct sip_caller_prefs_s sip_accept_contact_t;
 *
 * The structure #sip_accept_contact_t contains representation of SIP
 * @AcceptContact header.
 *
 * The #sip_accept_contact_t is defined as follows:
 * @code
 * typedef struct caller_prefs_s {
 *   sip_common_t        cp_common[1];   // Common fragment info
 *   sip_caller_prefs_t *cp_next;        // Link to next ac-value
 *   msg_param_t const  *cp_params;      // List of parameters
 *   char const         *cp_q;           // Priority
 *   unsigned            cp_require :1;  // Shortcut to "require" parameter
 *   unsigned            cp_explicit :1; // Shortcut to "explicit" parameter
 * } sip_accept_contact_t;
 * @endcode
 */

#define sip_accept_contact_dup_xtra sip_caller_prefs_dup_xtra
#define sip_accept_contact_dup_one  sip_caller_prefs_dup_one

static int sip_accept_contact_update(msg_common_t *h,
				     char const *name, isize_t namelen,
				     char const *value);

msg_hclass_t sip_accept_contact_class[] =
SIP_HEADER_CLASS(accept_contact, "Accept-Contact", "a", cp_params, append,
		 accept_contact);

issize_t sip_accept_contact_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_caller_prefs_d(home, h, s, slen);
}


issize_t sip_accept_contact_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return sip_caller_prefs_e(b, bsiz, h, flags);
}

static int sip_accept_contact_update(msg_common_t *h,
				     char const *name, isize_t namelen,
				     char const *value)
{
  sip_caller_prefs_t *cp = (sip_caller_prefs_t *)h;

  if (name == NULL) {
    cp->cp_q = NULL;
    cp->cp_require = 0;
    cp->cp_explicit = 0;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

#if nomore
  else if (MATCH(q)) {
    cp->cp_q = value;
  }
#endif
  else if (MATCH(require)) {
    cp->cp_require = value != NULL;
  }
  else if (MATCH(explicit)) {
    cp->cp_explicit = value != NULL;
  }

#undef MATCH

  return 0;
}


/**@SIP_HEADER sip_reject_contact Reject-Contact Header
 *
 * The Reject-Contact syntax is defined in @RFC3841 section 10 as follows:
 *
 * @code
 *    Reject-Contact  =  ("Reject-Contact" / "j") HCOLON rc-value
 *                       *(COMMA rc-value)
 *    rc-value        =  "*" *(SEMI rc-params)
 *    rc-params       =  feature-param / generic-param
 *                        ;;feature param from RFC 3840
 *                        ;;generic-param from RFC 3261
 * @endcode
 *
 * Despite the BNF, there MUST NOT be more than one instance of any feature
 * tag in feature-param.
 *
 * @sa @RFC3840, @RFC3841, sip_contact_reject(), sip_contact_score().
 *
 * The parsed Reject-Contact header
 * is stored in #sip_reject_contact_t structure.
 */

/**@ingroup sip_reject_contact
 * @typedef struct sip_caller_prefs_s sip_reject_contact_t;
 *
 * The structure #sip_reject_contact_t contains representation of SIP
 * @RejectContact header.
 *
 * The #sip_reject_contact_t is defined as follows:
 * @code
 * typedef struct caller_prefs_s {
 *   sip_common_t        cp_common[1];   // Common fragment info
 *   sip_caller_prefs_t *cp_next;        // Link to next rc-value
 *   msg_param_t const  *cp_params;      // List of parameters
 * } sip_reject_contact_t;
 * @endcode
 *
 * @note Fields @c cp_q, @c cp_require and @c cp_explicit are ignored for
 * @RejectContact header.
 */

#define sip_reject_contact_dup_xtra sip_caller_prefs_dup_xtra
#define sip_reject_contact_dup_one  sip_caller_prefs_dup_one
#define sip_reject_contact_update   NULL

msg_hclass_t sip_reject_contact_class[] =
SIP_HEADER_CLASS(reject_contact, "Reject-Contact", "j", cp_params, append,
		 reject_contact);

issize_t sip_reject_contact_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_caller_prefs_d(home, h, s, slen);
}


issize_t sip_reject_contact_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return sip_caller_prefs_e(b, bsiz, h, flags);
}
