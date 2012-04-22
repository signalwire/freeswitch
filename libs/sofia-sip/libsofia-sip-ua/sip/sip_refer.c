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

/**@CFILE sip_refer.c
 * @brief SIP REFER-related headers.
 *
 * The file @b sip_refer.c contains implementation of header classes for
 * REFER-related SIP headers @ReferTo and @ReferredBy.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Jan 23 13:23:45 EET 2002 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"
#include "sofia-sip/sip_extra.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_refer_to Refer-To Header
 *
 * The Refer-To header provides a URI to reference. Its syntax is defined in
 * @RFC3515 section 2.1 as follows:
 *
 * @code
 *  Refer-To = ("Refer-To" / "r") HCOLON ( name-addr / addr-spec )
 *            *(SEMI generic-param)
 * @endcode
 *
 *
 * The parsed Refer-To header is stored in #sip_refer_to_t structure.
 */

/**@ingroup sip_refer_to
 *
 * @typedef typedef struct sip_refer_to_s sip_refer_to_t;
 *
 * The structure #sip_refer_to_t contains representation of @ReferTo
 * header.
 *
 * The #sip_refer_to_t is defined as follows:
 * @code
 * typedef struct sip_refer_to_s
 * {
 *   sip_common_t        r_common[1];   // Common fragment info
 *   sip_error_t        *r_next;	// Link to next (dummy)
 *   char const          r_display;     // Display name
 *   url_t               r_url[1];	// URI to reference
 *   msg_param_t const  *r_params;      // List of generic parameters
 * } sip_refer_to_t;
 * @endcode
 */

static msg_xtra_f sip_refer_to_dup_xtra;
static msg_dup_f sip_refer_to_dup_one;
#define sip_refer_to_update NULL

msg_hclass_t sip_refer_to_class[] =
SIP_HEADER_CLASS(refer_to, "Refer-To", "r", r_params, single, refer_to);

issize_t sip_refer_to_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  issize_t retval;
  sip_refer_to_t *r = (sip_refer_to_t *)h;

  retval = sip_name_addr_d(home, &s,
			   &r->r_display,
			   r->r_url,
			   &r->r_params,
			   NULL);
  if (retval < 0)
    return retval;

  if (*s == '?' && !r->r_display && !r->r_url->url_headers) {
    /* Missing <> around URL */
    *s++ = '\0';
    r->r_url->url_headers = s;
    s += strcspn(s, " \t;,");
    if (IS_LWS(*s))
      *s++ = '\0', skip_lws(&s);
    if (*s)
      return -1;
    r->r_display = s;	/* Put empty string in display so that we encode using <> */
  }
  else if (*s)
    return -1;

  return retval;
}

issize_t sip_refer_to_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_refer_to_t const *r = h->sh_refer_to;

  assert(sip_is_refer_to(h));

  return sip_name_addr_e(b, bsiz, flags,
			 r->r_display, MSG_IS_CANONIC(flags),
			 r->r_url,
			 r->r_params,
			 NULL);
}

isize_t sip_refer_to_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_refer_to_t const *r = h->sh_refer_to;

  MSG_PARAMS_SIZE(offset, r->r_params);
  offset += MSG_STRING_SIZE(r->r_display);
  offset += url_xtra(r->r_url);

  return offset;
}

/** Duplicate one #sip_refer_to_t object */
char *sip_refer_to_dup_one(sip_header_t *dst, sip_header_t const *src,
			   char *b, isize_t xtra)
{
  sip_refer_to_t *r_dst = dst->sh_refer_to;
  sip_refer_to_t const *r_src = src->sh_refer_to;

  char *end = b + xtra;

  b = msg_params_dup(&r_dst->r_params, r_src->r_params, b, xtra);
  MSG_STRING_DUP(b, r_dst->r_display, r_src->r_display);
  URL_DUP(b, end, r_dst->r_url, r_src->r_url);

  assert(b <= end);

  return b;
}

/* ====================================================================== */

/**@SIP_HEADER sip_referred_by Referred-By Header
 *
 * The Referred-By header conveys the identity of the original referrer to
 * the referred-to party. Its syntax is defined in
 * @RFC3892 section 3 as follows:
 *
 * @code
 *    Referred-By  =  ("Referred-By" / "b") HCOLON referrer-uri
 *                   *( SEMI (referredby-id-param / generic-param) )
 *
 *    referrer-uri = ( name-addr / addr-spec )
 *
 *    referredby-id-param = "cid" EQUAL sip-clean-msg-id
 *
 *    sip-clean-msg-id = LDQUOT dot-atom "@" (dot-atom / host) RDQUOT
 *
 *    dot-atom = atom *( "." atom )
 *
 *    atom     = 1*( alphanum / "-" / "!" / "%" / "*" /
 *                        "_" / "+" / "'" / "`" / "~"   )
 * @endcode
 *
 *
 * The parsed Referred-By header is stored in #sip_referred_by_t structure.
 */

/**@ingroup sip_referred_by
 *
 * @typedef typedef struct sip_referred_by_s sip_referred_by_t;
 *
 * The structure #sip_referred_by_t contains representation of @ReferredBy
 * header.
 *
 * The #sip_referred_by_t is defined as follows:
 * @code
 * typedef struct sip_referred_by_s
 * {
 *   sip_common_t        b_common[1];   // Common fragment info
 *   sip_error_t        *b_next;	// Link to next (dummy)
 *   char const          b_display,
 *   url_t               b_url[1];	// Referrer-URI
 *   msg_param_t const  *b_params;      // List of parameters
 *   char const         *b_cid;
 * } sip_referred_by_t;
 * @endcode
 */

static msg_xtra_f sip_referred_by_dup_xtra;
static msg_dup_f sip_referred_by_dup_one;
static msg_update_f sip_referred_by_update;

msg_hclass_t sip_referred_by_class[] =
SIP_HEADER_CLASS(referred_by, "Referred-By", "b", b_params, single,
		 referred_by);

issize_t sip_referred_by_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_referred_by_t *b = h->sh_referred_by;

  if (sip_name_addr_d(home, &s,
		      &b->b_display,
		      b->b_url,
		      &b->b_params,
		      NULL) < 0 || *s /* Extra stuff? */)
    return -1;

  if (b->b_params)
    msg_header_update_params(b->b_common, 0);

  return 0;
}

issize_t sip_referred_by_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_referred_by(h));

  return sip_name_addr_e(b, bsiz, flags,
			 h->sh_referred_by->b_display,
			 MSG_IS_CANONIC(flags), h->sh_referred_by->b_url,
			 h->sh_referred_by->b_params,
			 NULL);
}

isize_t sip_referred_by_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_referred_by_t const *b = h->sh_referred_by;

  MSG_PARAMS_SIZE(offset, b->b_params);
  offset += MSG_STRING_SIZE(b->b_display);
  offset += url_xtra(b->b_url);

  return offset;
}

char *sip_referred_by_dup_one(sip_header_t *dst, sip_header_t const *src,
			      char *b,
			      isize_t xtra)
{
  sip_referred_by_t *nb = dst->sh_referred_by;
  sip_referred_by_t const *o = src->sh_referred_by;
  char *end = b + xtra;

  b = msg_params_dup(&nb->b_params, o->b_params, b, xtra);
  MSG_STRING_DUP(b, nb->b_display, o->b_display);
  URL_DUP(b, end, nb->b_url, o->b_url);

  nb->b_cid = msg_params_find(nb->b_params, "cid=");

  assert(b <= end);

  return b;
}

static int sip_referred_by_update(msg_common_t *h,
			   char const *name, isize_t namelen,
			   char const *value)
{
  sip_referred_by_t *b = (sip_referred_by_t *)h;

  if (name == NULL) {
    b->b_cid = NULL;
  }
  else if (namelen == strlen("cid") && su_casenmatch(name, "cid", namelen)) {
    b->b_cid = value;
  }

  return 0;
}


/* ====================================================================== */

/**@SIP_HEADER sip_replaces Replaces Header
 *
 * The Replaces header indicates that a single dialog identified by the
 * header field is to be shut down and logically replaced by the incoming
 * INVITE in which it is contained. Its syntax is defined in
 * @RFC3891 section 6.1 as follows:
 *
 * @code
 *    Replaces        = "Replaces" HCOLON callid *(SEMI replaces-param)
 *    replaces-param  = to-tag / from-tag / early-flag / generic-param
 *    to-tag          = "to-tag" EQUAL token
 *    from-tag        = "from-tag" EQUAL token
 *    early-flag      = "early-only"
 * @endcode
 *
 * A Replaces header field MUST contain exactly one <to-tag> and exactly
 * one <from-tag>, as they are required for unique dialog matching.  For
 * compatibility with dialogs initiated by @RFC2543 compliant UAs, a
 * tag of zero ("0") matches both tags of zero and null.  A Replaces header
 * field MAY contain the <early-only> flag.
 *
 * The parsed Replaces header is stored in #sip_replaces_t structure.
 *
 * @sa @RFC3891, nta_leg_by_replaces(), nta_leg_make_replaces()
 */

/**@ingroup sip_replaces
 *
 * @typedef typedef struct sip_replaces_s sip_replaces_t;
 *
 * The structure #sip_replaces_t contains representation of @Replaces
 * header.
 *
 * The #sip_replaces_t is defined as follows:
 * @code
 * typedef struct sip_replaces_s
 * {
 *   sip_common_t        rp_common[1];   // Common fragment info
 *   sip_error_t        *rp_next;	 // Dummy link to next
 *   char const         *rp_call_id;     // @CallID of dialog to replace
 *   msg_param_t const  *rp_params;      // List of parameters
 *   char const         *rp_to_tag;      // Value of "to-tag" parameter
 *   char const         *rp_from_tag;    // Value of "from-tag" parameter
 *   unsigned            rp_early_only;  // early-only parameter
 * } sip_replaces_t;
 * @endcode
 */

static msg_xtra_f sip_replaces_dup_xtra;
static msg_dup_f sip_replaces_dup_one;
static msg_update_f sip_replaces_update;

msg_hclass_t sip_replaces_class[] =
SIP_HEADER_CLASS(replaces, "Replaces", "", rp_params, single, replaces);

/** Decode (parse) @Replaces header */
issize_t sip_replaces_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_replaces_t *rp = h->sh_replaces;

  rp->rp_call_id = sip_word_at_word_d(&s);
  if (!rp->rp_call_id)
    return -1;
  if (*s) {
    if (msg_params_d(home, &s, &rp->rp_params) == -1)
      return -1;
    msg_header_update_params(rp->rp_common, 0);
  }

  return s - rp->rp_call_id;
}

/** Encode (print) @Replaces header */
issize_t sip_replaces_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  sip_replaces_t const *rp = h->sh_replaces;

  assert(sip_is_replaces(h));
  MSG_STRING_E(b, end, rp->rp_call_id);
  MSG_PARAMS_E(b, end, rp->rp_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra storage used by @Replaces header field */
isize_t sip_replaces_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_replaces_t const *rp = h->sh_replaces;

  MSG_PARAMS_SIZE(offset, rp->rp_params);
  offset += MSG_STRING_SIZE(rp->rp_call_id);

  return offset;
}

/** Duplicate a @Replaces header field */
char *sip_replaces_dup_one(sip_header_t *dst, sip_header_t const *src,
			   char *b, isize_t xtra)
{
  sip_replaces_t *rp_dst = dst->sh_replaces;
  sip_replaces_t const *rp_src = src->sh_replaces;

  char *end = b + xtra;

  b = msg_params_dup(&rp_dst->rp_params, rp_src->rp_params, b, xtra);
  MSG_STRING_DUP(b, rp_dst->rp_call_id, rp_src->rp_call_id);

  assert(b <= end); (void)end;

  return b;
}

/** Update parameters in @Replaces header. */
static int sip_replaces_update(msg_common_t *h,
			       char const *name, isize_t namelen,
			       char const *value)
{
  sip_replaces_t *rp = (sip_replaces_t *)h;

  if (name == NULL) {
    rp->rp_to_tag = NULL;
    rp->rp_from_tag = NULL;
    rp->rp_early_only = 0;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

  else if (MATCH(to-tag)) {
    rp->rp_to_tag = value;
  }
  else if (MATCH(from-tag)) {
    rp->rp_from_tag = value;
  }
  else if (MATCH(early-only)) {
    rp->rp_early_only = value != NULL;
  }

#undef MATCH

  return 0;
}

/* ====================================================================== */

/**@SIP_HEADER sip_refer_sub Refer-Sub Header
 *
 * SIP header field @b Refer-Sub is meaningful and MAY be used with a REFER
 * request and the corresponding 2XX response only. This header field set to
 * "false" specifies that a REFER-Issuer requests that the REFER-Recipient
 * doesn't establish an implicit subscription and the resultant dialog.
 *
 *  Refer-Sub       = "Refer-Sub" HCOLON refer-sub-value *(SEMI exten)
 *  refer-sub-value = "true" / "false"
 *  exten           = generic-param
 *
 * The parsed Refer-Sub header is stored in #sip_refer_sub_t structure.
 *
 * @NEW_1_12_5. Note that #sip_t does not contain @a sip_refer_sub field,
 * but sip_refer_sub() accessor function should be used for accessing @b
 * Refer-Sub header structure.
 *
 * @sa @RFC4488, nua_refer(), #nua_i_refer
 */

/**@ingroup sip_refer_sub
 *
 * @typedef typedef struct sip_refer_sub_s sip_refer_sub_t;
 *
 * The structure #sip_refer_sub_t contains representation of @ReferSub
 * header.
 *
 * The #sip_refer_sub_t is defined as follows:
 * @code
 * typedef struct sip_refer_sub_s
 * {
 *   sip_common_t        rs_common[1];   // Common fragment info
 *   sip_error_t        *rs_next;	 // Dummy link to next
 *   char const         *rs_value;       // "true" or "false"
 *   msg_param_t const  *rs_params;      // List of extension parameters
 * } sip_refer_sub_t;
 * @endcode
 *
 * @NEW_1_12_5.
 */

static msg_xtra_f sip_refer_sub_dup_xtra;
static msg_dup_f sip_refer_sub_dup_one;
#define sip_refer_sub_update NULL

msg_hclass_t sip_refer_sub_class[] =
SIP_HEADER_CLASS(refer_sub, "Refer-Sub", "", rs_params, single, refer_sub);

/** Decode (parse) @ReferSub header */
issize_t sip_refer_sub_d(su_home_t *home,
			 sip_header_t *h,
			 char *s, isize_t slen)
{
  sip_refer_sub_t *rs = (sip_refer_sub_t *)h;

  if (msg_token_d(&s, &rs->rs_value) < 0)
    return -1;

  if (!su_casematch(rs->rs_value, "false") &&
      !su_casematch(rs->rs_value, "true"))
    return -1;

  if (*s)
    if (msg_params_d(home, &s, &rs->rs_params) == -1)
      return -1;

  return s - rs->rs_value;
}

/** Encode (print) @ReferSub header */
issize_t sip_refer_sub_e(char b[], isize_t bsiz,
			 sip_header_t const *h,
			 int flags)
{
  char *b0 = b, *end = b + bsiz;
  sip_refer_sub_t const *rs = (sip_refer_sub_t *)h;

  assert(sip_is_refer_sub(h));
  MSG_STRING_E(b, end, rs->rs_value);
  MSG_PARAMS_E(b, end, rs->rs_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra storage used by @ReferSub header field */
isize_t sip_refer_sub_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_refer_sub_t const *rs = (sip_refer_sub_t *)h;

  MSG_PARAMS_SIZE(offset, rs->rs_params);
  offset += MSG_STRING_SIZE(rs->rs_value);

  return offset;
}

/** Duplicate a @ReferSub header field */
char *sip_refer_sub_dup_one(sip_header_t *dst, sip_header_t const *src,
			   char *b, isize_t xtra)
{
  sip_refer_sub_t *rs_dst = (sip_refer_sub_t *)dst;
  sip_refer_sub_t const *rs_src = (sip_refer_sub_t *)src;

  char *end = b + xtra;

  b = msg_params_dup(&rs_dst->rs_params, rs_src->rs_params, b, xtra);
  MSG_STRING_DUP(b, rs_dst->rs_value, rs_src->rs_value);

  assert(b <= end); (void)end;

  return b;
}
