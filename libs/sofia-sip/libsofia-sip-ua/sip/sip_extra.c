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

/**@CFILE sip_extra.c
 * @brief Non-critical SIP headers
 *
 * This file contains implementation of @CallInfo, @ErrorInfo,
 * @Organization, @Priority, @RetryAfter, @Server, @Subject,
 * @Timestamp, and @UserAgent headers.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u


#include "sofia-sip/sip_parser.h"
#include "sofia-sip/sip_extra.h"
#include "../su/sofia-sip/su_alloc.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#
#include <assert.h>

/* ====================================================================== */

static issize_t sip_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen);

static isize_t sip_info_dup_xtra(sip_header_t const *h, isize_t offset);
static char *sip_info_dup_one(sip_header_t *dst,
			      sip_header_t const *src,
			      char *b,
			      isize_t xtra);

#define sip_info_update NULL

/* ====================================================================== */

/**@SIP_HEADER sip_call_info Call-Info Header
 *
 * The Call-Info header provides additional information about the caller or
 * callee. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Call-Info   =  "Call-Info" HCOLON info *(COMMA info)
 *    info        =  LAQUOT absoluteURI RAQUOT *( SEMI info-param)
 *    info-param  =  ( "purpose" EQUAL ( "icon" / "info"
 *                   / "card" / token ) ) / generic-param
 * @endcode
 *
 *
 * The parsed Call-Info header is stored in #sip_call_info_t structure.
 */

/**@ingroup sip_call_info
 * @typedef struct sip_call_info_s sip_call_info_t;
 *
 * The structure #sip_call_info_t contains representation of an
 * @CallInfo header.
 *
 * The #sip_call_info_t is defined as follows:
 * @code
 * struct sip_call_info_s
 * {
 *   sip_common_t        ci_common[1]; // Common fragment info
 *   sip_call_info_t    *ci_next;      // Link to next @CallInfo
 *   url_t               ci_url[1];    // URI to call info
 *   msg_param_t const  *ci_params;    // List of parameters
 *   char const         *ci_purpose;   // Value of @b purpose parameter
 * };
 * @endcode
 */

#define sip_call_info_dup_xtra  sip_info_dup_xtra
#define sip_call_info_dup_one   sip_info_dup_one
static msg_update_f sip_call_info_update;

msg_hclass_t sip_call_info_class[] =
SIP_HEADER_CLASS(call_info, "Call-Info", "",
		 ci_params, append, call_info);

issize_t sip_call_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  issize_t retval = sip_info_d(home, h, s, slen);

  if (retval == 0)
    for (;h; h = h->sh_next)
      msg_header_update_params(h->sh_common, 0);

  return retval;
}

issize_t sip_call_info_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_call_info_t *ci = (sip_call_info_t *)h;

  assert(sip_call_info_p(h));

  return sip_name_addr_e(b, bsiz, f, NULL, 1, ci->ci_url, ci->ci_params, NULL);
}

/** @internal
 * Update parameter in a @CallInfo object.
 *
 */
static int
sip_call_info_update(msg_common_t *h,
		     char const *name, isize_t namelen,
		     char const *value)
{
  sip_call_info_t *ci = (sip_call_info_t *)h;

  if (name == NULL) {
    ci->ci_purpose = NULL;
  }
  else if (namelen == strlen("purpose") &&
	   su_casenmatch(name, "purpose", namelen)) {
    ci->ci_purpose = value;
  }

  return 0;
}

/* ====================================================================== */

/**@SIP_HEADER sip_error_info Error-Info Header
 *
 * The Error-Info header provides a pointer to additional information about
 * the error status response. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Error-Info  =  "Error-Info" HCOLON error-uri *(COMMA error-uri)
 *    error-uri   =  LAQUOT absoluteURI RAQUOT *( SEMI generic-param )
 * @endcode
 *
 *
 * The parsed Error-Info header is stored in #sip_error_info_t structure.
 */

/**@ingroup sip_error_info
 * @typedef struct sip_error_info_s sip_error_info_t;
 *
 * The structure #sip_error_info_t contains representation of an
 * @ErrorInfo header.
 *
 * The #sip_error_info_t is defined as follows:
 * @code
 * struct sip_error_info_s
 * {
 *   sip_common_t        ei_common[1]; // Common fragment info
 *   sip_error_info_t   *ei_next;      // Link to next @ErrorInfo
 *   url_t               ei_url[1];    // URI to error info
 *   msg_param_t const  *ei_params;    // List of parameters
 * };
 * @endcode
 */

msg_hclass_t sip_error_info_class[] =
SIP_HEADER_CLASS(error_info, "Error-Info", "",
		 ei_params, append, info);

issize_t sip_error_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_info_d(home, h, s, slen);
}

issize_t sip_error_info_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_error_info_t const *ei = h->sh_error_info;

  assert(sip_error_info_p(h));

  return sip_name_addr_e(b, bsiz, f,
			 NULL, 1, ei->ei_url, ei->ei_params, NULL);
}

/* ====================================================================== */

/**@SIP_HEADER sip_alert_info Alert-Info Header
 *
 * When present in an INVITE request, the Alert-Info header field
 * specifies an alternative ring tone to the UAS.  When present in a 180
 * (Ringing) response, the Alert-Info header field specifies an
 * alternative ringback tone to the UAC.  A typical usage is for a proxy
 * to insert this header field to provide a distinctive ring feature.
 *
 * @code
 *    Alert-Info   =  "Alert-Info" HCOLON alert-param *(COMMA alert-param)
 *    alert-param  =  LAQUOT absoluteURI RAQUOT *(SEMI generic-param)
 * @endcode
 *
 * The parsed Alert-Info header is stored in #sip_alert_info_t structure.
 *
 * @NEW_1_12_7. In order to use @b Alert-Info header, initialize the SIP
 * parser before calling nta_agent_create() or nua_create() with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * The #sip_t structure does not contain a @a sip_alert_info field, but
 * sip_alert_info() function should be used for accessing the @b Alert-Info
 * header structure.
 */

/**@ingroup sip_alert_info
 * @typedef struct sip_alert_info_s sip_alert_info_t;
 *
 * The structure #sip_alert_info_t contains representation of an
 * @AlertInfo header.
 *
 * The #sip_alert_info_t is defined as follows:
 * @code
 * struct sip_alert_info_s
 * {
 *   sip_common_t        ai_common[1]; // Common fragment info
 *   sip_alert_info_t   *ai_next;      // Link to next @AlertInfo
 *   url_t               ai_url[1];    // URI to alert info
 *   msg_param_t const  *ai_params;    // List of optional parameters
 * };
 * @endcode
 *
 * @NEW_1_12_7.
 */

msg_hclass_t sip_alert_info_class[] =
SIP_HEADER_CLASS(alert_info, "Alert-Info", "",
		 ai_params, append, info);

issize_t sip_alert_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_info_d(home, h, s, slen);
}

issize_t sip_alert_info_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_alert_info_t *ai = (sip_alert_info_t *)h;
  return sip_name_addr_e(b, bsiz, f, NULL, 1, ai->ai_url, ai->ai_params, NULL);
}

/* ====================================================================== */

/**@SIP_HEADER sip_reply_to Reply-To Header
 *
 * The @b Reply-To header field contains a logical return URI that may be
 * different from the @From header field. For example, the URI MAY be used to
 * return missed calls or unestablished sessions. If the user wished to
 * remain anonymous, the header field SHOULD either be omitted from the
 * request or populated in such a way that does not reveal any private
 * information. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *   Reply-To      =  "Reply-To" HCOLON rplyto-spec
 *   rplyto-spec   =  ( name-addr / addr-spec )
 *                   *( SEMI rplyto-param )
 *   rplyto-param  =  generic-param
 * @endcode
 *
 * The parsed Reply-To header is stored in #sip_reply_to_t structure.
 *
 * @sa sip_update_default_mclass()
 *
 * @NEW_1_12_7. In order to use @b Reply-To header,
 * initialize the SIP parser before calling nta_agent_create() or
 * nua_create() with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t structure does not contain a @a sip_reply_to field, but
 * sip_reply_to() function should be used for accessing the @b Reply-To
 * header structure.
 */

/**@ingroup sip_reply_to
 * @typedef struct msg_list_s sip_reply_to_t;
 *
 * The structure #sip_reply_to_t contains representation of SIP
 * @ReplyTo header.
 *
 * The #sip_reply_to_t is defined as follows:
 * @code
 * struct sip_reply_to_s
 * {
 *   sip_common_t       rplyto_common[1]; // Common fragment info

 *   sip_error_t       *rplyto_next;	 // Dummy link to next header
 *   char const        *rplyto_display;	 // Display name
 *   url_t              rplyto_url[1];	 // Return URI
 *   msg_param_t const *rplyto_params;	 // List of optional parameters
 * };
 * @endcode
 */

static isize_t sip_reply_to_dup_xtra(sip_header_t const *h, isize_t offset);
static char *sip_reply_to_dup_one(sip_header_t *dst,
				  sip_header_t const *src,
				  char *b,
				  isize_t xtra);
#define sip_reply_to_update NULL

msg_hclass_t sip_reply_to_class[] =
  SIP_HEADER_CLASS(reply_to, "Reply-To", "", rplyto_params, single, reply_to);

issize_t sip_reply_to_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_reply_to_t *rplyto = (sip_reply_to_t *)h;

  return sip_name_addr_d(home,
			 &s,
			 &rplyto->rplyto_display,
			 rplyto->rplyto_url,
			 &rplyto->rplyto_params,
			 NULL);
}

issize_t sip_reply_to_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_reply_to_t *rplyto = (sip_reply_to_t *)h;

  return sip_name_addr_e(b, bsiz,
			 flags,
			 rplyto->rplyto_display,
			 MSG_IS_CANONIC(flags), rplyto->rplyto_url,
			 rplyto->rplyto_params,
			 NULL);
}

static isize_t sip_reply_to_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_reply_to_t const *rplyto = (sip_reply_to_t const *)h;

  return sip_name_addr_xtra(rplyto->rplyto_display,
			    rplyto->rplyto_url,
			    rplyto->rplyto_params,
			    offset);
}

/**@internal Duplicate one sip_reply_to_t object. */
static char *sip_reply_to_dup_one(sip_header_t *dst, sip_header_t const *src,
				  char *b, isize_t xtra)
{
  sip_reply_to_t *rplyto = (sip_reply_to_t *)dst;
  sip_reply_to_t const *o = (sip_reply_to_t *)src;

  return sip_name_addr_dup(&rplyto->rplyto_display, o->rplyto_display,
			   rplyto->rplyto_url, o->rplyto_url,
			   &rplyto->rplyto_params, o->rplyto_params,
			   b, xtra);
}

/* ====================================================================== */

/**@SIP_HEADER sip_in_reply_to In-Reply-To Header
 *
 * The @b In-Reply-To request header field enumerates the
 * @ref sip_call_id "Call-IDs" that this call references or returns.
 * Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    In-Reply-To  =  "In-Reply-To" HCOLON callid *(COMMA callid)
 * @endcode
 *
 * The parsed In-Reply-To header is stored in #sip_in_reply_to_t structure.
 */

/**@ingroup sip_in_reply_to
 * @typedef struct msg_list_s sip_in_reply_to_t;
 *
 * The structure #sip_in_reply_to_t contains representation of SIP
 * @InReplyTo header.
 *
 * The #sip_in_reply_to_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;       // Link to next header
 *   msg_param_t       *k_items;      // List of call ids
 * } sip_in_reply_to_t;
 * @endcode
 */

msg_hclass_t sip_in_reply_to_class[] =
SIP_HEADER_CLASS_LIST(in_reply_to, "In-Reply-To", "", list);

issize_t sip_in_reply_to_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, h, s, slen);
}

issize_t sip_in_reply_to_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_in_reply_to_p(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_organization Organization Header
 *
 * The Organization header field conveys the name of the organization to
 * which the entity issuing the request or response belongs. Its syntax is
 * defined in @RFC3261 as follows:
 *
 * @code
 *    Organization  =  "Organization" HCOLON [TEXT-UTF8-TRIM]
 * @endcode
 *
 *
 * The parsed Organization header is stored in #sip_organization_t structure.
 */

/**@ingroup sip_organization
 * @typedef struct msg_generic_s sip_organization_t;
 *
 * The structure #sip_organization_t contains representation of a SIP
 * @Organization header.
 *
 * The #sip_organization_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Organization text
 * } sip_organization_t;
 * @endcode
 */

msg_hclass_t sip_organization_class[] =
SIP_HEADER_CLASS_G(organization, "Organization", "", single);

issize_t sip_organization_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_generic_d(home, h, s, slen);
}

issize_t sip_organization_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_organization_p(h));
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_priority Priority Header
 *
 * The Priority request-header field indicates the urgency of the request as
 * perceived by the client. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Priority        =  "Priority" HCOLON priority-value
 *    priority-value  =  "emergency" / "urgent" / "normal"
 *                       / "non-urgent" / other-priority
 *    other-priority  =  token
 * @endcode
 *
 *
 * The parsed Priority header is stored in #sip_priority_t structure.
 */

/**@ingroup sip_priority
 * @typedef struct msg_generic_s sip_priority_t;
 *
 * The structure #sip_priority_t contains representation of a SIP
 * @Priority header.
 *
 * The #sip_priority_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Dummy link to next header
 *   char const    *g_string;	    // Priority token
 * } sip_priority_t;
 * @endcode
 */

msg_hclass_t sip_priority_class[] =
SIP_HEADER_CLASS_G(priority, "Priority", "", single);

issize_t sip_priority_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_priority_t *priority = (sip_priority_t *)h;

  if (msg_token_d(&s, &priority->g_string) < 0)
    return -1;

  if (*s && !IS_LWS(*s))	/* Something extra after priority token? */
    return -1;

  return 0;
}

issize_t sip_priority_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_priority_p(h));
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_server Server Header
 *
 * The Server response-header field contains information about the software
 * used by the user agent server to handle the request. Its syntax is
 * defined in @RFC2616 section 14.38 and @RFC3261 as follows:
 *
 * @code
 *    Server           =  "Server" HCOLON server-val *(LWS server-val)
 *    server-val       =  product / comment
 *    product          =  token [SLASH product-version]
 *    product-version  =  token
 * @endcode
 *
 * The parsed Server header is stored in #sip_server_t structure.
 */

/**@ingroup sip_server
 * @typedef struct msg_generic_s sip_server_t;
 *
 * The structure #sip_server_t contains representation of a SIP
 * @Server header.
 *
 * The #sip_server_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Server tokens
 * } sip_server_t;
 * @endcode
 */

msg_hclass_t sip_server_class[] =
SIP_HEADER_CLASS_G(server, "Server", "", single);

issize_t sip_server_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_generic_d(home, h, s, slen);
}

issize_t sip_server_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_server_p(h));
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_subject Subject Header
 *
 * The Subject header provides a summary or indicates the nature of the
 * request. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Subject  =  ( "Subject" / "s" ) HCOLON [TEXT-UTF8-TRIM]
 * @endcode
 *
 * The parsed Subject header is stored in #sip_subject_t structure.
 */

/**@ingroup sip_subject
 * @typedef struct msg_generic_s sip_subject_t;
 *
 * The structure #sip_subject_t contains representation of a SIP
 * @Subject header.
 *
 * The #sip_subject_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Subject text
 * } sip_subject_t;
 * @endcode
 */

msg_hclass_t sip_subject_class[] =
SIP_HEADER_CLASS_G(subject, "Subject", "s", single);

issize_t sip_subject_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_generic_d(home, h, s, slen);
}

issize_t sip_subject_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_subject_p(h));
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_timestamp Timestamp Header
 *
 * The @b Timestamp header describes when the client sent the request to the
 * server, and it is used by the client to adjust its retransmission
 * intervals. Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Timestamp  =  "Timestamp" HCOLON 1*(DIGIT)
 *                   [ "." *(DIGIT) ] [ LWS delay ]
 *    delay      =  *(DIGIT) [ "." *(DIGIT) ]
 * @endcode
 *
 * The parsed Timestamp header is stored in #sip_timestamp_t structure.
 */

/**@ingroup sip_timestamp
 * @typedef struct sip_timestamp_s sip_timestamp_t;
 *
 * The structure #sip_timestamp_t contains representation of a SIP
 * @Timestamp header.
 *
 * The #sip_timestamp_t is defined as follows:
 * @code
 * typedef struct sip_timestamp_s
 * {
 *   sip_common_t        ts_common[1]; // Common fragment info
 *   sip_error_t        *ts_next;      // Dummy link
 *   char const         *ts_stamp;     // Original timestamp
 *   char const         *ts_delay;     // Delay at UAS
 * } sip_timestamp_t;
 * @endcode
 */

static isize_t sip_timestamp_dup_xtra(sip_header_t const *h, isize_t offset);
static char *sip_timestamp_dup_one(sip_header_t *dst,
				   sip_header_t const *src,
				   char *b,
				   isize_t xtra);
#define sip_timestamp_update NULL

msg_hclass_t sip_timestamp_class[] =
SIP_HEADER_CLASS(timestamp, "Timestamp", "", ts_common, single,
		 timestamp);

issize_t sip_timestamp_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_timestamp_t *ts = (sip_timestamp_t*)h;

  ts->ts_stamp = s;
  s += span_digit(s);
  if (s == ts->ts_stamp)
    return -1;
  if (*s == '.') { s += span_digit(s + 1) + 1; }

  if (IS_LWS(*s)) {
    *s = '\0';
    s += span_lws(s + 1) + 1;
    ts->ts_delay = s;
    s += span_digit(s); if (*s == '.') { s += span_digit(s + 1) + 1; }
  }

  if (!*s || IS_LWS(*s))
    *s++ = '\0';
  else
    return -1;

  return 0;
}

issize_t sip_timestamp_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_timestamp_t const *ts = h->sh_timestamp;
  char *end = b + bsiz, *b0 = b;

  assert(sip_timestamp_p(h));

  MSG_STRING_E(b, end, ts->ts_stamp);
  if (ts->ts_delay) {
    MSG_CHAR_E(b, end, ' ');
    MSG_STRING_E(b, end, ts->ts_delay);
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

static
isize_t sip_timestamp_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_timestamp_t const *ts = h->sh_timestamp;

  offset += MSG_STRING_SIZE(ts->ts_stamp);
  offset += MSG_STRING_SIZE(ts->ts_delay);

  return offset;
}

static
char *sip_timestamp_dup_one(sip_header_t *dst,
			    sip_header_t const *src,
			    char *b,
			    isize_t xtra)
{
  sip_timestamp_t *ts = dst->sh_timestamp;
  sip_timestamp_t const *o = src->sh_timestamp;
  char *end = b + xtra;

  MSG_STRING_DUP(b, ts->ts_stamp, o->ts_stamp);
  MSG_STRING_DUP(b, ts->ts_delay, o->ts_delay);

  assert(b <= end); (void)end;

  return b;
}

/* ====================================================================== */

/**@SIP_HEADER sip_user_agent User-Agent Header
 *
 * The User-Agent header contains information about the client user agent
 * originating the request. Its syntax is defined in [H14.43, S10.45] as
 * follows:
 *
 * @code
 *    User-Agent       =  "User-Agent" HCOLON server-val *(LWS server-val)
 *    server-val       =  product / comment
 *    product          =  token [SLASH product-version]
 *    product-version  =  token
 * @endcode
 *
 * The parsed User-Agent header is stored in #sip_user_agent_t structure.
 */

/**@ingroup sip_user_agent
 * @typedef struct msg_generic_s sip_user_agent_t;
 *
 * The structure #sip_user_agent_t contains representation of a SIP
 * @UserAgent header.
 *
 * The #sip_user_agent_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // User-Agent components
 * } sip_user_agent_t;
 * @endcode
 */

msg_hclass_t sip_user_agent_class[] =
SIP_HEADER_CLASS_G(user_agent, "User-Agent", "", single);

issize_t sip_user_agent_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_generic_d(home, h, s, slen);
}

issize_t sip_user_agent_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_user_agent_p(h));
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_etag SIP-ETag Header
 *
 * The @b SIP-ETag header field identifies the published event state. Its
 * syntax is defined in @RFC3903 as follows:
 *
 * @code
 *      SIP-ETag           = "SIP-ETag" HCOLON entity-tag
 *      entity-tag         = token
 * @endcode
 *
 * The parsed SIP-ETag header is stored in #sip_etag_t structure.
 */

/**@ingroup sip_etag
 * @typedef struct msg_generic_s sip_etag_t;
 *
 * The structure #sip_etag_t contains representation of a SIP
 * @SIPETag header.
 *
 * The #sip_etag_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // entity-tag
 * } sip_etag_t;
 * @endcode
 */

msg_hclass_t sip_etag_class[] =
SIP_HEADER_CLASS_G(etag, "SIP-ETag", "", single);

issize_t sip_etag_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_etag_t *etag = (sip_etag_t *)h;

  return msg_token_d(&s, &etag->g_value);
}

issize_t sip_etag_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_if_match SIP-If-Match Header
 *
 * The @b SIP-If-Match header field identifies the specific entity of event
 * state that the request is refreshing, modifying or removing. Its syntax
 * is defined in @RFC3903 as follows:
 *
 * @code
 *      SIP-If-Match       = "SIP-If-Match" HCOLON entity-tag
 *      entity-tag         = token
 * @endcode
 *
 * The parsed SIP-If-Match header is stored in #sip_if_match_t structure.
 */

/**@ingroup sip_if_match
 * @typedef struct msg_generic_s sip_if_match_t;
 *
 * The structure #sip_if_match_t contains representation of a SIP
 * @SIPIfMatch header.
 *
 * The #sip_if_match_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // entity-tag
 * } sip_if_match_t;
 * @endcode
 */

msg_hclass_t sip_if_match_class[] =
SIP_HEADER_CLASS_G(if_match, "SIP-If-Match", "", single);

issize_t sip_if_match_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_etag_d(home, h, s, slen);
}

issize_t sip_if_match_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return sip_etag_e(b, bsiz, h, f);
}

/* ====================================================================== */

/** Parsing @CallInfo, @ErrorInfo. */
static
issize_t sip_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_call_info_t *ci = h->sh_call_info;
  char *end = s + slen;

  for(;;) {
	  ci = h->sh_call_info;
	  end = s + slen;

	  while (*s == ',')
		  s += span_lws(s + 1) + 1;

	  if (sip_name_addr_d(home, &s, NULL, ci->ci_url, &ci->ci_params, NULL) < 0)
		  return -1;

	  slen = end - s;
	  msg_parse_next_field_without_recursion();
  }
}

isize_t sip_info_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_call_info_t const *ci = h->sh_call_info;

  return sip_name_addr_xtra(NULL,
			    ci->ci_url,
			    ci->ci_params,
			    offset);
}

char *sip_info_dup_one(sip_header_t *dst,
		       sip_header_t const *src,
		       char *b,
		       isize_t xtra)
{
  sip_call_info_t *ci = dst->sh_call_info;
  sip_call_info_t const *o = src->sh_call_info;

  return sip_name_addr_dup(NULL, NULL,
			   ci->ci_url, o->ci_url,
			   &ci->ci_params, o->ci_params,
			   b, xtra);
}

/* ====================================================================== */

#if SU_HAVE_EXPERIMENTAL

/**@SIP_HEADER sip_suppress_body_if_match Suppress-Body-If-Match Header
 *
 * The @b Suppress-Body-If-Match header field identifies a SIP event content
 * already known by the watcher. Its syntax is defined in
 * draft-niemi-sip-subnot-etags-01 as follows:
 *
 * @code
 *    Suppress-Body-If-Match = "Suppress-Body-If-Match" HCOLON entity-tag
 *    entity-tag             = token
 * @endcode
 *
 * The parsed Suppress-Body-If-Match header is stored in
 * #sip_suppress_body_if_match_t structure.
 *
 * @sa @RFC3265, draft-niemi-sip-subnot-etags-01.txt
 *
 * @EXP_1_12_5.
 * In order to use @b Suppress-Body-If-Match header,
 * initialize the SIP parser with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t structure does not contain a @a
 * sip_suppress_body_if_match field, but sip_suppress_body_if_match()
 * function should be used for accessing the @b Suppress-Body-If-Match
 * header structure.
 */

/**@ingroup sip_suppress_body_if_match
 * @typedef struct sip_suppress_body_if_match_s sip_suppress_body_if_match_t;
 *
 * The structure #sip_suppress_body_if_match_t contains representation of a
 * SIP @SuppressBodyIfMatch header.
 *
 * The #sip_suppress_body_if_match_t is defined as follows:
 * @code
 * typedef struct sip_suppress_body_if_match_s
 * {
 *   sip_common_t   sbim_common[1]; // Common fragment info
 *   sip_error_t   *sbim_next;      // Dummy link to next header
 *   char const    *sbim_tag;       // entity-tag
 * } sip_suppress_body_if_match_t;
 * @endcode
 */

#define sip_suppress_body_if_match_dup_xtra  msg_generic_dup_xtra
#define sip_suppress_body_if_match_dup_one   msg_generic_dup_one
#define sip_suppress_body_if_match_update NULL

msg_hclass_t sip_suppress_body_if_match_class[] =
SIP_HEADER_CLASS(suppress_body_if_match,
		 "Suppress-Body-If-Match", "",
		 sbim_common, single, suppress_body_if_match);

issize_t sip_suppress_body_if_match_d(su_home_t *home,
				      sip_header_t *h,
				      char *s, isize_t slen)
{
  sip_suppress_body_if_match_t *sbim = (void *)h;
  return msg_token_d(&s, &sbim->sbim_tag);
}

issize_t sip_suppress_body_if_match_e(char b[], isize_t bsiz,
				      sip_header_t const *h,
				      int f)
{
  return sip_etag_e(b, bsiz, h, f);
}


/* ====================================================================== */

/**@SIP_HEADER sip_suppress_notify_if_match Suppress-Notify-If-Match Header
 *
 * The @b Suppress-Notify-If-Match header is used to suppress
 * superfluous NOTIFY transactions. Its syntax is defined in
 * draft-niemi-sip-subnot-etags-01 as follows:
 *
 * @code
 *    Suppress-Notify-If-Match = "Suppress-Notify-If-Match" HCOLON entity-tag
 *    entity-tag               = token
 * @endcode
 *
 * The parsed Suppress-Notify-If-Match header is stored in
 * #sip_suppress_notify_if_match_t structure.
 *
 * @sa @RFC3265, draft-niemi-sip-subnot-etag-01
 *
 * @EXP_1_12_5.
 * In order to use @b Suppress-Notify-If-Match header,
 * initialize the SIP parser with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t struct does not contain @a sip_suppress_notify_if_match field,
 * but sip_suppress_notify_if_match() function should be used for accessing
 * the @b Suppress-Notify-If-Match header structure.
 */

/**@ingroup sip_suppress_notify_if_match
 * @typedef struct sip_suppress_notify_if_match_s \
 * sip_suppress_notify_if_match_t;
 *
 * The structure #sip_suppress_notify_if_match_t contains representation of a
 * SIP @SuppressNotifyIfMatch header.
 *
 * The #sip_suppress_notify_if_match_t is defined as follows:
 * @code
 * typedef struct sip_suppress_notify_if_match_s
 * {
 *   sip_common_t   snim_common[1]; // Common fragment info
 *   sip_error_t   *snim_next;      // Dummy link to next header
 *   char const    *snim_tag;       // entity-tag
 * } sip_suppress_notify_if_match_t;
 * @endcode
 */

#define sip_suppress_notify_if_match_dup_xtra  msg_generic_dup_xtra
#define sip_suppress_notify_if_match_dup_one   msg_generic_dup_one
#define sip_suppress_notify_if_match_update NULL

msg_hclass_t sip_suppress_notify_if_match_class[] =
SIP_HEADER_CLASS(suppress_notify_if_match,
		 "Suppress-Notify-If-Match", "",
		 snim_common, single, suppress_notify_if_match);

issize_t sip_suppress_notify_if_match_d(su_home_t *home,
					sip_header_t *h,
					char *s, isize_t slen)
{
  sip_suppress_notify_if_match_t *snim = (void *)h;
  return msg_token_d(&s, &snim->snim_tag);
}

issize_t sip_suppress_notify_if_match_e(char b[], isize_t bsiz,
					sip_header_t const *h,
					int f)
{
  return msg_generic_e(b, bsiz, h, f);
}

#endif

#if SIP_HAVE_REMOTE_PARTY_ID

/**@SIP_HEADER sip_remote_party_id Remote-Party-ID Header
 *
 * The syntax of the Remote-Party-ID header is described as follows:
 * @code
 *   Remote-Party-ID  = "Remote-Party-ID" HCOLON rpid *(COMMA rpid)
 *
 *   rpid             =  [display-name] LAQUOT addr-spec RAQUOT
 *                                                 *(SEMI rpi-token)
 *
 *   rpi-token        = rpi-screen / rpi-pty-type /
 *                       rpi-id-type / rpi-privacy / other-rpi-token
 *
 *   rpi-screen       = "screen" EQUAL ("no" / "yes")
 *
 *   rpi-pty-type     = "party" EQUAL ("calling" / "called" / token)
 *
 *   rpi-id-type      = "id-type" EQUAL ("subscriber" / "user" /
 *                                                  "term"  / token)
 *
 *   rpi-privacy      = "privacy" EQUAL
 *                      ( rpi-priv-element
 *                        / (LDQUOT rpi-priv-element
 *                          *(COMMA rpi-priv-element) RDQUOT) )
 *
 *   rpi-priv-element = ("full" / "name" / "uri" / "off" / token)
 *                                      ["-" ( "network" / token )]
 *
 *   other-rpi-token  = ["-"] token [EQUAL (token / quoted-string)]
 *
 * @endcode
 *
 * @sa sip_update_default_mclass(), draft-ietf-sip-privacy-04.txt, @RFC3325
 *
 * @NEW_1_12_7. In order to use @b Remote-Party-ID header,
 * initialize the SIP parser before calling nta_agent_create() or
 * nua_create() with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t structure does not contain @a sip_remote_party_id field, but
 * sip_remote_party_id() function should be used for accessing the @b
 * Remote-Party-ID header structure.
 */

/**@ingroup sip_remote_party_id
 * @typedef typedef struct sip_remote_party_id_s sip_remote_party_id_t;
 *
 * The structure #sip_remote_party_id_t contains representation of SIP
 * @RemotePartyID header.
 *
 * The #sip_remote_party_id_t is defined as follows:
 * @code
 * typedef struct sip_remote_party_id_s {
 *   sip_common_t           rpid_common[1]; // Common fragment info
 *   sip_remote_party_id_t *rpid_next;      // Link to next
 *   char const        *rpid_display;       // Display name
 *   url_t              rpid_url[1];        // URL
 *   sip_param_t const *rpid_params;        // Parameters
 *   // Shortcuts to screen, party, id-type and privacy parameters
 *   char const        *rpid_screen, *rpid_party, *rpid_id_type, *rpid_privacy;
 * } sip_remote_party_id_t;
 * @endcode
 */

extern msg_xtra_f sip_remote_party_id_dup_xtra;
extern msg_dup_f sip_remote_party_id_dup_one;

static msg_update_f sip_remote_party_id_update;

msg_hclass_t sip_remote_party_id_class[] =
SIP_HEADER_CLASS(remote_party_id, "Remote-Party-ID", "",
		 rpid_params, append, remote_party_id);

issize_t sip_remote_party_id_d(su_home_t *home, sip_header_t *h,
			       char *s, isize_t slen)
{
	sip_remote_party_id_t *rpid;

	for(;;) {
		rpid = (sip_remote_party_id_t *)h;

		while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
			*s = '\0', s += span_lws(s + 1) + 1;

		if (sip_name_addr_d(home, &s,
							&rpid->rpid_display,
							rpid->rpid_url,
							&rpid->rpid_params, NULL) == -1)
			return -1;

		msg_parse_next_field_without_recursion();
	}

}

issize_t sip_remote_party_id_e(char b[], isize_t bsiz,
			       sip_header_t const *h, int f)
{
  sip_remote_party_id_t const *rpid = (sip_remote_party_id_t *)h;

  return sip_name_addr_e(b, bsiz, f,
			 rpid->rpid_display, 1,
			 rpid->rpid_url,
			 rpid->rpid_params,
			 NULL);
}

/** Calculate size of extra data required for duplicating one
 *  sip_remote_party_id_t header.
 */
isize_t sip_remote_party_id_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_remote_party_id_t const *rpid = (sip_remote_party_id_t *)h;
  return sip_name_addr_xtra(rpid->rpid_display,
			    rpid->rpid_url,
			    rpid->rpid_params,
			    offset);
}

/** Duplicate one sip_remote_party_id_t object */
char *sip_remote_party_id_dup_one(sip_header_t *dst,
				  sip_header_t const *src,
				  char *b, isize_t xtra)
{
  sip_remote_party_id_t *rpid = (sip_remote_party_id_t *)dst;
  sip_remote_party_id_t const *o = (sip_remote_party_id_t const *)src;

  return sip_name_addr_dup(&rpid->rpid_display, o->rpid_display,
			   rpid->rpid_url, o->rpid_url,
			   &rpid->rpid_params, o->rpid_params,
			   b, xtra);
}

static int sip_remote_party_id_update(msg_common_t *h,
				      char const *name, isize_t namelen,
				      char const *value)
{
  sip_remote_party_id_t *rpid = (sip_remote_party_id_t *)h;

  if (name == NULL) {
    rpid->rpid_screen = NULL;
    rpid->rpid_party = NULL;
    rpid->rpid_id_type = NULL;
    rpid->rpid_privacy = NULL;
  }

#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

  else if (MATCH(screen))
    rpid->rpid_screen = value;
  else if (MATCH(party))
    rpid->rpid_party = value;
  else if (MATCH(id-type))
    rpid->rpid_id_type = value;
  else if (MATCH(privacy))
    rpid->rpid_privacy = value;

#undef MATCH

  return 0;
}

#endif

#if SIP_HAVE_P_ASSERTED_IDENTITY

/**@SIP_HEADER sip_p_asserted_identity P-Asserted-Identity Header
 *
 * The P-Asserted-Identity header is used used among trusted SIP entities
 * (typically intermediaries) to carry the identity of the user sending a
 * SIP message as it was verified by authentication. It is "defined" in
 * @RFC3325 section 9.1 as follows:
 *
 * @code
 *    PAssertedID = "P-Asserted-Identity" HCOLON PAssertedID-value
 *                    *(COMMA PAssertedID-value)
 *    PAssertedID-value = name-addr / addr-spec
 * @endcode
 *
 * @sa @RFC3325, @PPreferredIdentity
 *
 * @NEW_1_12_7. In order to use @b P-Asserted-Identity header,
 * initialize the SIP parser before calling nta_agent_create() or
 * nua_create() with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t structure does not contain @a sip_p_asserted_identity field,
 * but sip_p_asserted_identity() function should be used for accessing the
 * @b P-Asserted-Identity header structure.
 */

/**@ingroup sip_p_asserted_identity
 * @typedef typedef struct sip_p_asserted_identity_s sip_p_asserted_identity_t;
 *
 * The structure #sip_p_asserted_identity_t contains representation of SIP
 * @PAssertedIdentity header.
 *
 * The #sip_p_asserted_identity_t is defined as follows:
 * @code
 * typedef struct sip_p_asserted_identity_s {
 *   sip_common_t           paid_common[1];   // Common fragment info
 *   sip_p_asserted_identity_t *paid_next;    // Link to next
 *   char const                *paid_display; // Display name
 *   url_t                      paid_url[1];  // URL
 * } sip_p_asserted_identity_t;
 * @endcode
 */

static msg_xtra_f sip_p_asserted_identity_dup_xtra;
static msg_dup_f sip_p_asserted_identity_dup_one;

#define sip_p_asserted_identity_update NULL

msg_hclass_t sip_p_asserted_identity_class[] =
SIP_HEADER_CLASS(p_asserted_identity, "P-Asserted-Identity", "",
		 paid_common, append, p_asserted_identity);

issize_t sip_p_asserted_identity_d(su_home_t *home, sip_header_t *h,
				   char *s, isize_t slen)
{
	sip_p_asserted_identity_t *paid;

	for(;;) {
		paid = (sip_p_asserted_identity_t *)h;
		while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
			*s = '\0', s += span_lws(s + 1) + 1;

		if (sip_name_addr_d(home, &s,
							&paid->paid_display,
							paid->paid_url,
							NULL, NULL) == -1)
			return -1;

		msg_parse_next_field_without_recursion();
	}

}

issize_t sip_p_asserted_identity_e(char b[], isize_t bsiz,
				   sip_header_t const *h, int f)
{
  sip_p_asserted_identity_t const *paid = (sip_p_asserted_identity_t *)h;

  return sip_name_addr_e(b, bsiz, f,
			 paid->paid_display, MSG_IS_CANONIC(f),
			 paid->paid_url,
			 NULL,
			 NULL);
}

isize_t sip_p_asserted_identity_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_p_asserted_identity_t const *paid = (sip_p_asserted_identity_t *)h;

  return sip_name_addr_xtra(paid->paid_display,
			    paid->paid_url,
			    NULL,
			    offset);
}

/** Duplicate one sip_p_asserted_identity_t object */
char *sip_p_asserted_identity_dup_one(sip_header_t *dst,
				      sip_header_t const *src,
				      char *b, isize_t xtra)
{
  sip_p_asserted_identity_t *paid = (sip_p_asserted_identity_t *)dst;
  sip_p_asserted_identity_t const *o = (sip_p_asserted_identity_t *)src;

  return sip_name_addr_dup(&paid->paid_display, o->paid_display,
			   paid->paid_url, o->paid_url,
			   NULL, NULL,
			   b, xtra);
}

#endif

#if SIP_HAVE_P_PREFERRED_IDENTITY

/**@SIP_HEADER sip_p_preferred_identity P-Preferred-Identity Header
 *
 * The P-Preferred-Identity header is used used among trusted SIP entities
 * (typically intermediaries) to carry the identity of the user sending a
 * SIP message as it was verified by authentication. It is "defined" in
 * @RFC3325 section 9.1 as follows:
 *
 * @code
 *    PPreferredID = "P-Preferred-Identity" HCOLON PPreferredID-value
 *                    *(COMMA PPreferredID-value)
 *    PPreferredID-value = name-addr / addr-spec
 * @endcode
 *
 * @sa @RFC3325, @PAssertedIdentity
 *
 * @NEW_1_12_7. In order to use @b P-Preferred-Identity header,
 * initialize the SIP parser before calling nta_agent_create() or
 * nua_create() with, e.g.,
 * sip_update_default_mclass(sip_extend_mclass(NULL)).
 *
 * @note
 * The #sip_t structure does not contain @a sip_p_preferred_identity field,
 * but sip_p_preferred_identity() function should be used for accessing the
 * @b P-Preferred-Identity header structure.
 */

/**@ingroup sip_p_preferred_identity
 * @typedef typedef struct sip_p_preferred_identity_s sip_p_preferred_identity_t;
 *
 * The structure #sip_p_preferred_identity_t contains representation of SIP
 * @PPreferredIdentity header.
 *
 * The #sip_p_preferred_identity_t is defined as follows:
 * @code
 * typedef struct sip_p_preferred_identity_s {
 *   sip_common_t           ppid_common[1]; // Common fragment info
 *   sip_p_preferred_identity_t *ppid_next; // Link to next
 *   char const        *ppid_display;       // Display name
 *   url_t              ppid_url[1];        // URL
 * } sip_p_preferred_identity_t;
 * @endcode
 */


msg_hclass_t sip_p_preferred_identity_class[] =
SIP_HEADER_CLASS(p_preferred_identity, "P-Preferred-Identity", "",
		 ppid_common, append, p_asserted_identity);

issize_t sip_p_preferred_identity_d(su_home_t *home, sip_header_t *h,
				    char *s, isize_t slen)
{
  return sip_p_asserted_identity_d(home, h, s, slen);
}

issize_t sip_p_preferred_identity_e(char b[], isize_t bsiz,
				    sip_header_t const *h, int f)
{
  return sip_p_asserted_identity_e(b, bsiz, h, f);
}

#endif
