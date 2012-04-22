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

/**@CFILE sip_basic.c
 * @brief Basic SIP headers.
 *
 * Implementation of header classes for basic SIP headers, like request and
 * status lines, payload, @CallID, @CSeq, @Contact, @ContentLength, @Date,
 * @Expires, @From, @Route, @RecordRoute, @To, and @Via.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date  Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#include <sofia-sip/msg_date.h>

#include <sofia-sip/su_uniqueid.h>

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

/* ====================================================================== */

/**@SIP_HEADER sip_request Request Line
 *
 * The request line is first line in a SIP request message.  Its syntax defined
 * in @RFC3261 as follows:
 *
 * @code
 *    Request-Line   =  Method SP Request-URI SP SIP-Version CRLF
 *    Request-URI    =  SIP-URI / SIPS-URI / absoluteURI
 *    absoluteURI    =  scheme ":" ( hier-part / opaque-part )
 *    hier-part      =  ( net-path / abs-path ) [ "?" query ]
 *    net-path       =  "//" authority [ abs-path ]
 *    abs-path       =  "/" path-segments
 *    opaque-part    =  uric-no-slash *uric
 *    uric           =  reserved / unreserved / escaped
 *    uric-no-slash  =  unreserved / escaped / ";" / "?" / ":" / "@"
 *                      / "&" / "=" / "+" / "$" / ","
 *    path-segments  =  segment *( "/" segment )
 *    segment        =  *pchar *( ";" param )
 *    param          =  *pchar
 *    pchar          =  unreserved / escaped /
 *                      ":" / "@" / "&" / "=" / "+" / "$" / ","
 *    scheme         =  ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
 *    authority      =  srvr / reg-name
 *    srvr           =  [ [ userinfo "@" ] hostport ]
 *    reg-name       =  1*( unreserved / escaped / "$" / ","
 *                      / ";" / ":" / "@" / "&" / "=" / "+" )
 *    query          =  *uric
 *    SIP-Version    =  "SIP" "/" 1*DIGIT "." 1*DIGIT
 * @endcode
 *
 * The parsed request-line is stored in #sip_request_t structure.
 */

/**@ingroup sip_request
 * @typedef typedef struct sip_request_s sip_request_t;
 *
 * The structure #sip_request_t contains representation of SIP request line.
 *
 * The #sip_request_t is defined as follows:
 * @code
 * typedef struct sip_request_s {
 *   sip_common_t     rq_common[1];     // Common fragment info
 *   sip_unknown_t   *rq_next;          // Link to next (dummy)
 *   sip_method_t     rq_method;        // Method enum
 *   char const      *rq_method_name;   // Method name
 *   url_t            rq_url[1];        // RequestURI
 *   char const      *rq_version;       // Protocol version
 * } sip_request_t;
 * @endcode
 */

#define sip_request_insert msg_request_insert

static msg_xtra_f sip_request_dup_xtra;
static msg_dup_f sip_request_dup_one;
#define sip_request_update NULL

msg_hclass_t sip_request_class[] =
SIP_HEADER_CLASS(request, NULL, "", rq_common, single_critical, request);

/**Parse @ref sip_request "request line" from a a SIP message. */
issize_t sip_request_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_request_t *rq = (sip_request_t *)h;
  char *uri, *version;

  if (msg_firstline_d(s, &uri, &version) < 0 || !uri || !version ||
      (rq->rq_method = sip_method_d(&s, &rq->rq_method_name)) < 0 || *s ||
      url_d(rq->rq_url, uri) < 0 ||
      sip_version_d(&version, &rq->rq_version) < 0 || *version)
    return -1;

  return 0;
}

/**Encode @ref sip_request "request line" of a a SIP message. */
issize_t sip_request_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_request_t const *rq = (sip_request_t *)h;

  return snprintf(b, bsiz, "%s " URL_FORMAT_STRING " %s" CRLF,
		  rq->rq_method_name,
		  URL_PRINT_ARGS(rq->rq_url),
		  rq->rq_version);
}

isize_t sip_request_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_request_t const *rq = (sip_request_t *)h;

  offset += url_xtra(rq->rq_url);
  if (!rq->rq_method)
    offset += MSG_STRING_SIZE(rq->rq_method_name);
  offset += sip_version_xtra(rq->rq_version);

  return offset;
}

/** Duplicate one request header. */
char *sip_request_dup_one(sip_header_t *dst, sip_header_t const *src,
			  char *b, isize_t xtra)
{
  sip_request_t *rq = (sip_request_t *)dst;
  sip_request_t const *o = (sip_request_t *)src;
  char *end = b + xtra;

  URL_DUP(b, end, rq->rq_url, o->rq_url);

  if (!(rq->rq_method = o->rq_method))
    MSG_STRING_DUP(b, rq->rq_method_name, o->rq_method_name);
  else
    rq->rq_method_name = o->rq_method_name;
  sip_version_dup(&b, &rq->rq_version, o->rq_version);

  assert(b <= end);

  return b;
}

/**@ingroup sip_request
 *
 * Create a @ref sip_request "request line" object.
 *
 * Create a request line object with
 * method enum @a method, method name @a name, request URI @a uri, and
 * protocol version @a version.  The memory for the header object is
 * allocated from the memory home @a home.
 *
 * @param home     memory home used to allocate #sip_request_t object
 * @param method   method enum
 * @param name     method name (required if method is not well-known)
 * @param uri      request URI
 * @param version  version string (defaults to "SIP/2.0" if NULL)
 *
 * @par Example
 * The following code fragment creates an OPTIONS request object:
 * @code
 *   sip_request_t *rq;
 *   rq = sip_request_create(home, SIP_METHOD_OPTIONS, requestURI, NULL);
 * @endcode

 * @note
 * If you provide an non-NULL @a version string, it is not copied. The
 * version string @b MUST remain constant.
 */
sip_request_t *sip_request_create(su_home_t *home,
				  sip_method_t method, char const *name,
				  url_string_t const *uri,
				  char const *version)
{
  size_t xtra;
  sip_request_t *rq;

  if (method)
    name = sip_method_name(method, name);

  if (!name)
    return NULL;

  if (!method)
    method = sip_method_code(name);

  xtra = url_xtra(uri->us_url) + (method ? 0 : strlen(name) + 1);

  rq = (sip_request_t *)sip_header_alloc(home, sip_request_class, xtra);

  if (rq) {
    char *b = (char *)(rq + 1), *end = b + xtra;

    rq->rq_method      = method;
    rq->rq_method_name = name;
    if (!method)
      MSG_STRING_DUP(b, rq->rq_method_name, name);

    URL_DUP(b, end, rq->rq_url, uri->us_url);

    rq->rq_version = version ? version : SIP_VERSION_CURRENT;
    assert(b == end);
  }

  return rq;
}

/* ====================================================================== */

/**@SIP_HEADER sip_status Status Line
 *
 * The status line is first line in a response message.  It is defined in
 * @RFC3261 as follows:
 *
 * @code
 *    Status-Line     =  SIP-Version SP Status-Code SP Reason-Phrase CRLF
 *    Status-Code     =  Informational
 *                   /   Redirection
 *                   /   Success
 *                   /   Client-Error
 *                   /   Server-Error
 *                   /   Global-Failure
 *                   /   extension-code
 *    extension-code  =  3DIGIT
 *    Reason-Phrase   =  *(reserved / unreserved / escaped
 *                       / UTF8-NONASCII / UTF8-CONT / SP / HTAB)
 * @endcode
 *
 * The parsed status line is stored in #sip_status_t structure.
 */

/**@ingroup sip_status
 * @typedef typedef struct sip_status_s sip_status_t;
 *
 * The structure #sip_status_t contains representation of SIP
 * @ref sip_status "status line".
 *
 * The #sip_status_t is defined as follows:
 * @code
 * typedef struct sip_status_s {
 *   sip_common_t   st_common[1];       // Common fragment info
 *   sip_unknown_t *st_next;            // Link to next (dummy)
 *   char const    *st_version;         // Protocol version
 *   int            st_status;          // Status code
 *   char const    *st_phrase;          // Status phrase
 * } sip_status_t;
 * @endcode
 */


static msg_xtra_f sip_status_dup_xtra;
static msg_dup_f sip_status_dup_one;

#define sip_status_insert msg_status_insert
#define sip_status_update NULL

msg_hclass_t sip_status_class[] =
SIP_HEADER_CLASS(status, NULL, "", st_common, single_critical, status);

/** Parse status line */
issize_t sip_status_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_status_t *st = (sip_status_t *)h;
  char *status, *phrase;
  unsigned long code;

  if (msg_firstline_d(s, &status, &phrase) < 0 ||
      sip_version_d(&s, &st->st_version) < 0 || *s ||
      (code = strtoul(status, &status, 10)) >= INT_MAX || *status)
    return -1;

  st->st_status = code;
  st->st_phrase = phrase;

  return 0;
}

issize_t sip_status_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_status_t const *st = (sip_status_t *)h;
  int status;

  assert(sip_is_status(h));

  status = st->st_status;

  if (status > 999 || status < 100)
    status = 0;

  return snprintf(b, bsiz, "%s %03u %s" CRLF,
		  st->st_version,
		  status,
		  st->st_phrase);
}

/** Extra size of a #sip_status_t object. */
isize_t sip_status_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_status_t const *st = (sip_status_t *)h;
  offset += sip_version_xtra(st->st_version);
  offset += MSG_STRING_SIZE(st->st_phrase);
  return offset;
}

/** Duplicate one status header. */
char *sip_status_dup_one(sip_header_t *dst, sip_header_t const *src,
			 char *b, isize_t xtra)
{
  sip_status_t *st = (sip_status_t *)dst;
  sip_status_t const *o = (sip_status_t *)src;
  char *end = b + xtra;

  sip_version_dup(&b, &st->st_version, o->st_version);
  st->st_status = o->st_status;
  MSG_STRING_DUP(b, st->st_phrase, o->st_phrase);

  assert(b <= end); (void)end;

  return b;
}

/**@ingroup sip_status
 *
 * Create a @ref sip_status "status line" object.
 *
 * @param home    memory home used to allocate #sip_status_t object
 * @param status  status code (in range 100 - 699)
 * @param phrase  status phrase (may be NULL)
 * @param version version string (defaults to "SIP/2.0" if NULL)
 *
 * @note
 * If you provide an non-NULL @a version string, it is not copied. The
 * string @b MUST remain constant.
 *
 * @return
 * A pointer to newly created @ref sip_status "status line"
 * structure when successful, or NULL upon an error.
 */
sip_status_t *sip_status_create(su_home_t *home,
				unsigned status,
				char const *phrase,
				char const *version)
{
  sip_status_t *st;

  if (status < 100 || status > 699)
    return NULL;

  if (phrase == NULL && (phrase = sip_status_phrase(status)) == NULL)
    phrase = "";

  if ((st = (sip_status_t *)sip_header_alloc(home, sip_status_class, 0))) {
    st->st_status = status;
    st->st_phrase = phrase;
    st->st_version = version ? version : SIP_VERSION_CURRENT;
  }

  return st;
}

/* ====================================================================== */

/**@SIP_HEADER sip_payload Message Body
 *
 * The payload structure contains the optional message body. The message
 * body stored in the #sip_payload_t structure has no internal structure,
 * but it is accessed as a byte array. Use @ref sdp_parser "SDP parser" to
 * parse SDP content, for instance.
 *
 * The message body is stored in a #sip_payload_t structure.
 */

/**@ingroup sip_payload
 * @typedef typedef struct sip_payload_s sip_payload_t;
 *
 * The structure #sip_payload_t contains representation of SIP message payload.
 *
 * The #sip_payload_t is defined as follows:
 * @code
 * typedef struct sip_payload_s {
 *   msg_common_t    pl_common[1];      // Common fragment info
 *   msg_header_t   *pl_next;           // Next payload (if multipart message)
 *   char           *pl_data;           // Data - may contain NUL
 *   unsigned        pl_len;            // Length of message payload
 * } sip_payload_t;
 * @endcode
 */

#define sip_payload_d 	     msg_payload_d
#define sip_payload_e 	     msg_payload_e
#define sip_payload_dup_xtra msg_payload_dup_xtra
#define sip_payload_dup_one  msg_payload_dup_one
#define sip_payload_update   NULL

msg_hclass_t sip_payload_class[] =
SIP_HEADER_CLASS(payload, NULL, "", pl_common, single, payload);

/**@ingroup sip_payload
 *
 * Create a @ref sip_payload "SIP payload" structure.
 *
 * Create a new SIP payload structure. it
 * copies the given data to the the payload data, and NUL terminates it (it
 * allocates one extra byte for NUL).  If a NULL pointer is given as @a data,
 * sip_payload_create() allocates and zeroes a data buffer of @a len bytes.
 *
 * @param home memory home
 * @param data payload data
 * @param len  payload length
 *
 * @return A pointer to newly created
 * payload structure, if successful, and NULL upon an error.
 */
sip_payload_t *sip_payload_create(su_home_t *home, void const *data, isize_t len)
{
  msg_hclass_t *hc = sip_payload_class;
  sip_header_t *h = sip_header_alloc(home, hc, len + 1);
  sip_payload_t *pl = (sip_payload_t *)h;

  if (pl) {
    char *b = sip_header_data(h);
    if (data) {
      memcpy(b, data, len);
      b[len] = 0;
    }
    else {
      memset(b, 0, len + 1);
    }

    h->sh_data = pl->pl_data = b;
    h->sh_len = pl->pl_len = len;
  }

  return pl;
}

/* ====================================================================== */

/**@SIP_HEADER sip_separator Separator Line
 *
 * An empty line separates message headers from the message body (payload).
 * In order to avoid modifying messages with integrity protection, the
 * separator line has its own header structure which is included in the
 * #sip_t structure.
 *
 * The parsed separator line is stored in #sip_separator_t structure.
 */

/**@ingroup sip_separator
 * @typedef typedef struct sip_separator_s sip_separator_t;
 *
 * The structure #sip_separator_t contains representation of separator line
 * between message headers and body.
 *
 * The #sip_separator_t is defined as follows:
 * @code
 * typedef struct sip_separator_s {
 *   msg_common_t    sep_common[1];     // Common fragment info
 *   msg_header_t   *sep_next;          // Pointer to next header
 *   char            sep_data[4];       // NUL-terminated separator
 * } sip_separator_t;
 * @endcode
 */

#define sip_separator_d msg_separator_d
#define sip_separator_e msg_separator_e
#define sip_separator_insert msg_separator_insert

msg_hclass_t sip_separator_class[] =
SIP_HEADER_CLASS(separator, NULL, "", sep_common, single, any);

/**@ingroup sip_separator
 *
 * Create a @ref sip_separator "SIP separator line" structure.
 */
sip_separator_t *sip_separator_create(su_home_t *home)
{
  sip_separator_t *sep = (sip_separator_t *)
    sip_header_alloc(home, sip_separator_class, 0);

  if (sep)
    strcpy(sep->sep_data, CRLF);

  return sep;
}

/* ====================================================================== */

/**@SIP_HEADER sip_unknown Unknown Headers
 *
 * The unknown headers are handled with #sip_unknown_t structure. The
 * unknown header name is stored in @a un_name field and the header field
 * following the colon is stored in @a un_value field.
 *
 * @note It is possible to speed up parsing process by creating a parser
 * which does understand only a minimum number of headers. If such a parser
 * is used, some well-known headers are regarded as unknown and put into
 * list of unknown headers.
 */

/**@ingroup sip_unknown
 * @typedef typedef struct sip_unknown_s sip_unknown_t;
 *
 * The structure #sip_unknown_t contains representation of unknown headers.
 *
 * The #sip_unknown_t is defined as follows:
 * @code
 * typedef struct msg_unknown_s {
 *   msg_common_t    un_common[1];  // Common fragment info
 *   msg_unknown_t  *un_next;       // Link to next unknown header
 *   char const     *un_name;       // Header name
 *   char const     *un_value;      // Header field value
 * } sip_unknown_t;
 * @endcode
 */

#define sip_unknown_dup_xtra msg_unknown_dup_xtra
#define sip_unknown_dup_one  msg_unknown_dup_one
#define sip_unknown_update NULL

msg_hclass_t sip_unknown_class[] =
SIP_HEADER_CLASS(unknown, "", "", un_common, append, unknown);

issize_t sip_unknown_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_unknown_d(home, h, s, slen);
}

issize_t sip_unknown_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return msg_unknown_e(b, bsiz, h, flags);
}

/* ====================================================================== */

/**@SIP_HEADER sip_error Erroneous Headers
 *
 * The erroneous headers are stored in #sip_error_t structure.
 *
 * @note Other headers (like duplicate @ContentLength headers) may be put
 * into the list of erroneous headers (@c sip->sip_error). If the list of
 * erroneous headers is processed, the header type must be validated first
 * by calling sip_is_error() (or by other relevant tests).
 */

/**@ingroup sip_error
 * @typedef typedef msg_error_t sip_error_t;
 * The structure #sip_error_t contains representation of error headers.
 *
 * The #sip_error_t is defined as follows:
 * @code
 * typedef struct msg_error_s {
 *   msg_common_t    er_common[1];  // Common fragment info
 *   msg_error_t    *er_next;       // Link to next header
 *   char const     *er_name;       // Name of bad header (if any)
 * } sip_error_t;
 * @endcode
 */

msg_hclass_t sip_error_class[] =
SIP_HEADER_CLASS(error, NULL, "", er_common, append, any);

issize_t sip_error_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return 0;
}

issize_t sip_error_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  /* There is no way to encode an erroneous header */
  return 0;
}

/* ====================================================================== */
/*
 * addr           = ("To" | "t" | "From" | "f") ":"
 *                  ( name-addr | addr-spec ) *( ";" addr-params )
 * name-addr      = [ display-name ] "<" addr-spec ">"
 * addr-spec      = SIP-URL | URI
 * display-name   = *token | quoted-string
 */

/**Parse @e name-addr.
 *
 * Parses <i>( name-addr | addr-spec )</i> construct on @Contact, @From,
 * @To, and other compatible headers. It splits the argument string in
 * four parts:
 *
 * @par
 * @e [display-name] @e addr-spec @e [parameters] @e [comment] @e [ss]
 *
 * @param home           pointer to memory home
 * @param inout_s        pointer to pointer to string to be parsed
 * @param return_display value-result parameter for @e display-name
 * @param return_url     value-result parameter for @e addr-spec
 * @param return_params  value-result paramater for @e parameters
 * @param return_comment value-result parameter for @e comment
 *
 * @note After succesful call to the function @c sip_name_addr_d(), *ss
 * contains pointer to the first character not beloging to @e name-addr,
 * most probably a comma. If that character is a separator, the last parameter
 * may not be NUL (zero) terminated. So, after examining value of @a **ss,
 * the calling function @b MUST set it to NUL.
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 *
 * @sa @From, @To, @Contact
 */
issize_t sip_name_addr_d(su_home_t *home,
			 char **inout_s,
			 char const **return_display,
			 url_t *return_url,
			 msg_param_t const **return_params,
			 char const **return_comment)
{
  char c, *s = *inout_s;
  char *display = NULL, *addr_spec = NULL;
  size_t n;

  if (*s == '\0')		/* Empty string */
    return -1;

  if (return_display && *s == '"') {
    /* Quoted string */
    if (msg_quoted_d(&s, &display) == -1)
      return -1;

    /* Now, we should have a '<' in s[0] */
    if (s[0] != '<')
      return -1;
    s++[0] = '\0';		/* NUL terminate quoted string... */
    n = strcspn(s, ">");
    addr_spec = s; s += n;
    if (*s) *s++ = '\0'; else return -1;
  }
  else {
    if (return_display)
      n = span_token_lws(s);
    else
      n = 0;

    if (s[n] == '<') {
      /* OK, we got a display name */
      display = s; s += n + 1;
      /* NUL terminate display name */
      while (n > 0 && IS_LWS(display[n - 1]))
	n--;
      if (n > 0)
	display[n] = '\0';
      else
	display = "";

      n = strcspn(s, ">");
      addr_spec = s; s += n; if (*s) *s++ = '\0'; else return -1;
    }
    else {
      /* addr-spec only */
      addr_spec = s;
      /**@sa
       * Discussion about comma, semicolon and question mark in
       * @RFC3261 section 20.10.
       */
      if (return_params)
	n = strcspn(s, " \t,;?");	/* DO NOT accept ,;? in URL */
      else
	/* P-Asserted-Identity and friends */
	n = strcspn(s, " ,"); /* DO NOT accept , in URL */
      s += n;
      if (IS_LWS(*s))
	*s++ = '\0';
    }
  }

  skip_lws(&s);

  if (return_display)
    *return_display = display;

  /* Now, url may still not be NUL terminated, e.g., if
   * it is like "Contact: url:foo,sip:bar,sip:zunk"
   */
  c = *s; *s = '\0';		/* terminate temporarily */
  /* Do not accept an empty URL */
  if (addr_spec[0] == '\0')
    return -1;
  if (url_d(return_url, addr_spec) == -1)
    return -1;
  *s = c;			/* return terminator */

  *inout_s = s;

  if (c == ';' && return_params)
    if (msg_params_d(home, inout_s, return_params) == -1)
      return -1;

  if (**inout_s == '(' && return_comment)
    if (msg_comment_d(inout_s, return_comment) == -1)
      return -1;

  return 0;
}

/**Encode @e name-addr and parameter list.
 *
 * Encodes @e name-addr headers, like @From, @To, @CallInfo, @ErrorInfo,
 * @Route, and @RecordRoute.
 *
 * @param b        buffer to store the encoding result
 * @param bsiz     size of the buffer @a b
 * @param flags    encoding flags
 * @param display  display name encoded before the @a url (may be NULL)
 * @param brackets if true, use always brackets around @a url
 * @param url      pointer to URL structure
 * @param params   pointer to parameter list (may be NULL)
 * @param comment  comment string encoded after others (may be NULL)
 *
 * @return
 * Returns number of characters in encoding, excluding the
 * final NUL.
 *
 * @note
 * The encoding result may be incomplete if the buffer size is not large
 * enough to store the whole encoding result.
 */
issize_t sip_name_addr_e(char b[], isize_t bsiz,
			 int flags,
			 char const *display,
			 int brackets, url_t const url[],
			 msg_param_t const params[],
			 char const *comment)
{
  int const compact = MSG_IS_COMPACT(flags);
  char const *u;
  char *b0 = b, *end = b + bsiz;

  brackets = brackets || display ||
    (url && (url->url_params ||
	     url->url_headers ||
	     ((u = url->url_user) && u[strcspn(u, ";,?")]) ||
	     ((u = url->url_password) && u[strcspn(u, ",")])));

  if (display && display[0]) {
    MSG_STRING_E(b, end, display);
    if (!compact) MSG_CHAR_E(b, end, ' ');
  }
  if (url) {
    if (brackets) MSG_CHAR_E(b, end, '<');
    URL_E(b, end, url);
    if (brackets) MSG_CHAR_E(b, end, '>');
  }

  MSG_PARAMS_E(b, end, params, flags);

  if (comment) {
    if (!compact) MSG_CHAR_E(b, end, ' ');
    MSG_CHAR_E(b, end, '(');
    MSG_STRING_E(b, end, comment);
    MSG_CHAR_E(b, end, ')');
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate the extra size needed to duplicate a name-addr-params construct.
 *
 * @param display  display name (may be NULL)
 * @param addr     pointer to URL structure
 * @param params   pointer to parameter list (may be NULL)
 * @param offset   base offset
 *
 * @retval Size of duplicated name-addr-params construct, including base offset.
 *
 * @NEW_1_12_7.
 */
isize_t sip_name_addr_xtra(char const *display, url_t const *addr,
			   msg_param_t const params[],
			   isize_t offset)
{
  SIP_PARAMS_SIZE(offset, params);
  offset += SIP_STRING_SIZE(display);
  offset += url_xtra(addr);
  return offset;
}

/**Duplicate a name-addr-params construct.
 *
 * @param d_display value-result parameter for copied @e name (may be NULL)
 * @param display   display name (may be NULL)
 * @param d_addr    value-result parameter for copied @e address
 * @param addr      pointer to URL address structure
 * @param d_params value-result parameter for copied parameters (may be NULL)
 * @param params   pointer to parameter list (may be NULL)
 * @param b        pointer to memory pool
 * @param xtra     size of the memory pool
 *
 * @retval End of the memory area used.
 *
 * @NEW_1_12_7.
 */
char *sip_name_addr_dup(char const **d_display, char const *display,
			url_t *d_addr, url_t const *addr,
			msg_param_t const **d_params, msg_param_t const params[],
			char *b, isize_t xtra)
{
  char *end = b + xtra;

  if (d_params)
    b = msg_params_dup(d_params, params, b, xtra);

  URL_DUP(b, end, d_addr, addr);

  if (d_display)
    MSG_STRING_DUP(b, *d_display, display);

  assert(b <= end);

  return b;
}


/** Parse @To or @From headers */
static issize_t sip_addr_d(su_home_t *home,
			   sip_header_t *h,
			   char *s,
			   isize_t slen)
{
  sip_addr_t *a = (sip_addr_t *)h;
  char const *comment = NULL;
  if (sip_name_addr_d(home,
		      &s,
		      &a->a_display,
		      a->a_url,
		      &a->a_params,
		      &comment) == -1
      || *s /* XXX - something extra? */)
    return -1;

  a->a_tag = msg_params_find(a->a_params, "tag=");

  return 0;
}

static int sip_addr_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_addr_t const *a = (sip_addr_t const *)h;

  return sip_name_addr_e(b, bsiz,
			 flags,
			 a->a_display,
			 MSG_IS_CANONIC(flags), a->a_url,
			 a->a_params,
			 NULL);
}

/**
 * Extra dup size of a sip_addr_t object.
 *
 * This function calculates extra size required when duplicating a
 * sip_addr_t object.
 *
 * @param a pointer to a sip_addr_t object
 *
 * @return
 *   Size of strings related to sip_addr_t object.
 */
static
isize_t sip_addr_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_addr_t const *a = (sip_addr_t const *)h;

  return sip_name_addr_xtra(a->a_display,
			    a->a_url,
			    a->a_params,
			    offset);
}

/**@internal
 * Duplicate one sip_addr_t object.
 */
static char *sip_addr_dup_one(sip_header_t *dst, sip_header_t const *src,
			      char *b, isize_t xtra)
{
  sip_addr_t *a = (sip_addr_t *)dst;
  sip_addr_t const *o = (sip_addr_t *)src;

  return sip_name_addr_dup(&a->a_display, o->a_display,
			   a->a_url, o->a_url,
			   &a->a_params, o->a_params,
			   b, xtra);
}

/** Update parameters in sip_addr_t object */
static int sip_addr_update(msg_common_t *h,
			   char const *name, isize_t namelen,
			   char const *value)
{
  sip_addr_t *a = (sip_addr_t *)h;

  if (name == NULL) {
    a->a_tag = NULL;
  }
  else if (namelen == strlen("tag") && su_casenmatch(name, "tag", namelen)) {
    a->a_tag = value;
  }

  return 0;
}

/** Create an address header object from URL */
static sip_addr_t *
sip_addr_make_url(su_home_t *home, msg_hclass_t *hc, url_string_t const *us)
{
  size_t n;
  sip_header_t *h;

  n = url_xtra(us->us_url);
  h = sip_header_alloc(home, hc, n);

  if (h) {
    sip_addr_t *a = (sip_to_t *)h;
    char *s2 = sip_header_data(h);

    if ((size_t)url_dup(s2, n, a->a_url, us->us_url) == n)
      return a;

    su_free(home, h);
  }

  return NULL;
}

/** Add a tag to address structure. */
static
int sip_addr_tag(su_home_t *home, sip_addr_t *a, char const *tag)
{
  if (a && tag) {
    msg_param_t value = strchr(tag, '=');

    if (value)
      value = strchr(value, '=') + 1;
    else
      value = tag;

    if (a->a_tag) {
      if (su_casematch(a->a_tag, value))
	return 0;
      else
	return -1;
    }

    if (tag == value)
      tag = su_sprintf(home, "tag=%s", tag);
    else
      tag = su_strdup(home, tag);

    if (tag)
      if (msg_header_replace_param(home, a->a_common, tag) >= 0)
	return 0;
  }

  return -1;
}

/* ====================================================================== */

/**@SIP_HEADER sip_call_id Call-ID Header
 *
 * The @b Call-ID header uniquely identifies a particular invitation or all
 * registrations of a particular client. It is defined in @RFC3261 as
 * follows:
 *
 * @code
 *    Call-ID  =  ( "Call-ID" / "i" ) HCOLON callid
 *    callid   =  word [ "@" word ]
 *    word        =  1*(alphanum / "-" / "." / "!" / "%" / "*" /
 *                   "_" / "+" / "`" / "'" / "~" / "(" / ")" / "<" / ">" /
 *                   ":" / "\" / DQUOTE / "/" / "[" / "]" / "?" / "{" / "}" )
 * @endcode
 *
 * The parsed Call-ID Header is stored in #sip_call_id_t structure.
 */

/**@ingroup sip_call_id
 * @typedef typedef struct sip_call_id_s sip_call_id_t;
 *
 * The structure #sip_call_id_t contains representation of SIP @CallID
 * header.
 *
 * The #sip_call_id_t is defined as follows:
 * @code
 * typedef struct sip_call_id_s {
 *   sip_common_t   i_common[1];        // Common fragment info
 *   sip_call_id_t *i_next;             // Link to next (dummy)
 *   char const    *i_id;               // ID value
 *   uint32_t       i_hash;             // Hash value (always nonzero)
 * } sip_call_id_t;
 * @endcode
 */

static msg_xtra_f sip_call_id_dup_xtra;
static msg_dup_f sip_call_id_dup_one;
#define sip_call_id_update NULL

msg_hclass_t sip_call_id_class[] =
SIP_HEADER_CLASS(call_id, "Call-ID", "i", i_common, single, call_id);

issize_t sip_call_id_d(su_home_t *home,
		       sip_header_t *h,
		       char *s,
		       isize_t slen)
{
  sip_call_id_t *i = (sip_call_id_t *)h;

  i->i_id = s; /* XXX - why not sip_word_at_word_d(&s); */
  i->i_hash = msg_hash_string(s);

  return 0;
}


issize_t sip_call_id_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_call_id_t const *i = (sip_call_id_t *)h;
  size_t n = strlen(i->i_id);

  if (bsiz > n)
    strcpy(b, i->i_id);

  return (issize_t)n;
}

/** Extra size of a #sip_call_id_t object. */
isize_t sip_call_id_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_call_id_t const *i = (sip_call_id_t *)h;
  return offset + MSG_STRING_SIZE(i->i_id);
}

/**Duplicate a sip_call_id object.
 *
 * Duplicate (copy deeply) a single #sip_call_id_t header object.
 *
 * @param dst   pointer to newly allocated header object
 * @param src   pointer to a header object to be duplicated
 * @param b     memory buffer used to copy external references
 * @param xtra  number bytes in buffer @a b
 *
 * @return Pointer to the new copy of #sip_call_id_t object, or @c NULL
 * upon an error.
 */
char *sip_call_id_dup_one(sip_header_t *dst, sip_header_t const *src,
			  char *b, isize_t xtra)
{
  sip_call_id_t *i = (sip_call_id_t *)dst;
  sip_call_id_t const *o = (sip_call_id_t *)src;
  char *end = b + xtra;

  MSG_STRING_DUP(b, i->i_id, o->i_id);
  if (!(i->i_hash = o->i_hash))
    i->i_hash = msg_hash_string(i->i_id);
  assert(b <= end); (void)end;

  return b;
}

/**@ingroup sip_call_id
 *
 * Create a @CallID header object.
 *
 * Create a Call-ID header object with a new unique value. It uses
 * su_guid_generate() function to generate the value. If the local host name
 * @a domain is specified, it is prepended to the generated value instead of
 * local MAC address.

 * @param home        memory home
 * @param domain      local domain name
 *
 * @return A pointer to newly created @CallID header object when
 * successful or NULL upon an error.
 *
 * @sa su_guid_generate(), su_guid_sprintf()
 */
sip_call_id_t *sip_call_id_create(su_home_t *home, char const *domain)
{
  sip_call_id_t *i;
  size_t xtra = su_guid_strlen + 1 + (domain ? strlen(domain) + 1 : 0);

  i = (sip_call_id_t *)sip_header_alloc(home, sip_call_id_class, xtra);

  if (i) {
    char *b;
    su_guid_t guid[1];

    i->i_id = b = (char *)(i + 1);

    su_guid_generate(guid);
    /*
     * Guid looks like "NNNNNNNN-NNNN-NNNN-NNNN-XXXXXXXXXXXX"
     * where NNNNNNNN-NNNN-NNNN-NNNN is timestamp and XX is MAC address
     * (but we use usually random ID for MAC because we do not have
     *  guid generator available for all processes within node)
     */
    su_guid_sprintf(b, su_guid_strlen + 1, guid);

    /* If we have a domain name don't include MAC address at the end of guid */
    if (domain) {
      b[8 + 5 + 5 + 5] = '@';
      strcpy(b + 8 + 5 + 5 + 5 + 1, domain);
    }

    i->i_hash = msg_hash_string(i->i_id);
  }

  return i;

}

/* ====================================================================== */

/**@SIP_HEADER sip_cseq CSeq Header
 *
 * The CSeq header (command sequence) uniquely identifies transactions
 * within a dialog.  It is defined in @RFC3261 as follows:
 *
 * @code
 *    CSeq              =  "CSeq" HCOLON 1*DIGIT LWS Method
 *    Method            =  INVITEm / ACKm / OPTIONSm / BYEm
 *                         / CANCELm / REGISTERm
 *                         / extension-method
 *    extension-method  =  token
 * @endcode
 *
 * The parsed CSeq header is stored in #sip_cseq_t structure.
 */

/**@ingroup sip_cseq
 * @typedef typedef struct sip_cseq_s sip_cseq_t;
 *
 * The structure #sip_cseq_t contains representation of SIP @CSeq header.
 *
 * The #sip_cseq_t is defined as follows:
 * @code
 * typedef struct sip_cseq_s {
 *   sip_common_t   cs_common[1];       // Common fragment info
 *   sip_error_t   *cs_next;            // Link to next (dummy)
 *   uint32_t       cs_seq;             // Sequence number
 *   sip_method_t   cs_method;          // Method enum
 *   char const    *cs_method_name;     // Method name
 * } sip_cseq_t;
 * @endcode
 */

static msg_xtra_f sip_cseq_dup_xtra;
static msg_dup_f sip_cseq_dup_one;
#define sip_cseq_update NULL

msg_hclass_t sip_cseq_class[] =
SIP_HEADER_CLASS(cseq, "CSeq", "", cs_common, single, cseq);

issize_t sip_cseq_d(su_home_t *home,
		    sip_header_t *h,
		    char *s,
		    isize_t slen)
{
  sip_cseq_t *cs = (sip_cseq_t *)h;

  if (msg_uint32_d(&s, &cs->cs_seq) < 0)
    return -1;

  if (*s) {
    if ((cs->cs_method = sip_method_d(&s, &cs->cs_method_name)) >= 0)
      return 0;
  }

  return -1;
}


issize_t sip_cseq_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_cseq_t const *cs = (sip_cseq_t *)h;

  assert(sip_is_cseq(h));

  return snprintf(b, bsiz, "%u %s", cs->cs_seq, cs->cs_method_name);
}

isize_t sip_cseq_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_cseq_t const *cs = (sip_cseq_t *)h;
  if (!cs->cs_method)
    return offset + MSG_STRING_SIZE(cs->cs_method_name);
  else
    return offset;
}

char *sip_cseq_dup_one(sip_header_t *dst, sip_header_t const *src,
		       char *b, isize_t xtra)
{
  sip_cseq_t *cs = (sip_cseq_t *)dst;
  sip_cseq_t const *o = (sip_cseq_t *)src;
  char *end = b + xtra;

  if (!(cs->cs_method = o->cs_method))
    MSG_STRING_DUP(b, cs->cs_method_name, o->cs_method_name);
  else
    cs->cs_method_name = o->cs_method_name;
  cs->cs_seq = o->cs_seq;

  assert(b <= end); (void)end;

  return b;
}

/**@ingroup sip_cseq
 *
 * Create a @CSeq header object.
 *
 * Create a @CSeq header object with the
 * sequence number @a seq, method enum @a method and method name @a
 * method_name.  The memory for the header object is allocated from the
 * memory home @a home.
 *
 * @param home        memory home
 * @param seq         sequence number
 * @param method      method enum
 * @param method_name method name (required if method is not well-known)
 *
 * @par Example
 * The following code fragment creates a cseq object for OPTIONS request:
 * @code
 *   sip_cseq_t *cseq;
 *   cseq = sip_cseq_create(home, agent->seq++, SIP_METHOD_OPTIONS);
 * @endcode
 *
 * @return
 * A pointer to newly created @CSeq
 * header object when successful or NULL upon an error.
 */
sip_cseq_t *sip_cseq_create(su_home_t *home,
			    uint32_t seq,
			    unsigned method,
			    char const *method_name)
{
  size_t xtra;
  sip_cseq_t *cs;

  if (method)
    method_name = sip_method_name((sip_method_t)method, method_name);

  if (method_name == NULL)
    return NULL;

  xtra = (method ? 0 : (strlen(method_name) + 1));

  cs = (sip_cseq_t *)sip_header_alloc(home, sip_cseq_class, xtra);

  if (cs) {
    cs->cs_seq = seq;
    cs->cs_method = (sip_method_t)method;
    if (!method)
      method_name = strcpy((char *)(cs + 1), method_name);
    cs->cs_method_name = method_name;
  }
  return cs;
}


/* ====================================================================== */
/**@SIP_HEADER sip_contact Contact Header
 *
 * The Contact header contain a list of URLs used to redirect future
 * requests.  Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Contact            =  ("Contact" / "m" ) HCOLON
 *                          ( STAR / (contact-param *(COMMA contact-param)))
 *    contact-param      =  (name-addr / addr-spec) *(SEMI contact-params)
 *    name-addr          =  [ display-name ] LAQUOT addr-spec RAQUOT
 *    addr-spec          =  SIP-URI / SIPS-URI / absoluteURI
 *    display-name       =  *(token LWS)/ quoted-string
 *    contact-params     =  c-p-q / c-p-expires
 *                          / contact-extension
 *    c-p-q              =  "q" EQUAL qvalue
 *    c-p-expires        =  "expires" EQUAL delta-seconds
 *    contact-extension  =  generic-param
 *    delta-seconds      =  1*DIGIT
 * @endcode
 *
 * @note
 * The @RFC2543 syntax allowed <comment>. We accept it, but don't encode it.
 *
 * Each parsed Contact header field is stored in #sip_contact_t structure.
 */

/**@ingroup sip_contact
 * @typedef typedef struct sip_contact_s sip_contact_t;
 *
 * The structure #sip_contact_t contains representation of SIP @Contact
 * header.
 *
 * The #sip_contact_t is defined as follows:
 * @code
 * typedef struct sip_contact_s {
 *   sip_common_t        m_common[1];   // Common fragment info
 *   sip_contact_t      *m_next;        // Link to next
 *   char const         *m_display;     // Display name
 *   url_t               m_url[1];      // SIP URL
 *   msg_param_t const  *m_params;      // List of contact-params
 *   char const         *m_comment;     // Comment
 *
 *   char const         *m_q;           // Priority
 *   char const         *m_expires;     // Expiration time
 * } sip_contact_t;
 * @endcode
 *
 * @note The <comment> field @ref sip_contact_s::m_comment "m_comment" is
 * deprecated: it is parsed but not included in encoding.
 */

static msg_xtra_f sip_contact_dup_xtra;
static msg_dup_f sip_contact_dup_one;
static msg_update_f sip_contact_update;

/** @showinitializer */
msg_hclass_t sip_contact_class[] =
  /*
   * Cut through the fog of macros
   * SIP_HEADER_CLASS(contact, "Contact", "m", m_params, append, contact);
   * and show here how the msg_hclass_t is initialized
   */
  {{
    /* hc_hash: */     sip_contact_hash,
    /* hc_parse: */    sip_contact_d,
    /* hc_print: */    sip_contact_e,
    /* hc_dxtra: */    sip_contact_dup_xtra,
    /* hc_dup_one: */  sip_contact_dup_one,
    /* hc_update: */   sip_contact_update,
    /* hc_name: */     "Contact",
    /* hc_len: */      sizeof("Contact") - 1,
    /* hc_short: */    "m",
    /* hc_size: */     MSG_ALIGN(sizeof(sip_contact_t), sizeof(void*)),
    /* hc_params: */   offsetof(sip_contact_t, m_params),
    /* hc_kind:	*/     msg_kind_append,
    /* hc_critical: */ 0
   }};

issize_t sip_contact_d(su_home_t *home,
		       sip_header_t *h,
		       char *s,
		       isize_t slen)
{
  sip_contact_t *m = (sip_contact_t *)h;

  assert(h);

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  if (sip_name_addr_d(home, &s, &m->m_display, m->m_url,
		      &m->m_params, &m->m_comment) == -1)
    return -1;

  return msg_parse_next_field(home, h, s, slen);
}


issize_t sip_contact_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_contact_t const *m = (sip_contact_t *)h;
  int always_lt_gt = MSG_IS_CANONIC(flags) && m->m_url->url_type != url_any;

  assert(sip_is_contact(h));

  return sip_name_addr_e(b, bsiz, flags,
			 m->m_display, always_lt_gt, m->m_url,
			 m->m_params,
			 NULL /* m->m_comment */);
}


isize_t sip_contact_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_contact_t const *m = (sip_contact_t *)h;

  return sip_name_addr_xtra(m->m_display,
			    m->m_url,
			    m->m_params,
			    offset)
    + MSG_STRING_SIZE(m->m_comment);
}

char *sip_contact_dup_one(sip_header_t *dst, sip_header_t const *src,
			  char *b, isize_t xtra)
{
  sip_contact_t *m = (sip_contact_t *)dst;
  sip_contact_t const *o = (sip_contact_t *)src;

  b = sip_name_addr_dup(&m->m_display, o->m_display,
			m->m_url, o->m_url,
			&m->m_params, o->m_params,
			b, xtra);
  MSG_STRING_DUP(b, m->m_comment, o->m_comment);
  return b;
}

/** Update parameter in #sip_contact_t */
static int sip_contact_update(msg_common_t *h,
			      char const *name, isize_t namelen,
			      char const *value)
{
  sip_contact_t *m = (sip_contact_t *)h;

  if (name == NULL) {
    m->m_q = NULL;
    m->m_expires = NULL;
  }
  else if (namelen == 1 && su_casenmatch(name, "q", 1)) {
    /* XXX - check for invalid value? */
    m->m_q = value;
  }
  else if (namelen == strlen("expires") &&
	   su_casenmatch(name, "expires", namelen)) {
    m->m_expires = value;
  }

  return 0;
}

/**@ingroup sip_contact
 *
 * Add a parameter to a @Contact header object
 *
 * Add a parameter to a @Contact
 * object. It does not copy the contents of the string @c param.
 *
 * @note This function @b does @b not duplicate @p param.
 *
 * @param home   memory home
 * @param m      #sip_contact_t object
 * @param param  parameter string
 *
 * @return 0 when successful, and -1 upon an error.
 *
 * @deprecated Use msg_header_replace_param() directly.
 */
int sip_contact_add_param(su_home_t *home,
			  sip_contact_t *m,
			  char const *param)
{
  return msg_header_replace_param(home, m->m_common, param);
}

/* ====================================================================== */

/**@SIP_HEADER sip_content_length Content-Length Header
 *
 * The Content-Length header indicates the size of the message-body in
 * decimal number of octets.  Its syntax is defined in @RFC3261 as
 * follows:
 *
 * @code
 *    Content-Length  =  ( "Content-Length" / "l" ) HCOLON 1*DIGIT
 * @endcode
 *
 * The parsed Content-Length header is stored in #sip_content_length_t
 * structure.
 */

/**@ingroup sip_content_length
 * @typedef typedef struct sip_content_length_s sip_content_length_t;
 *
 * The structure #sip_content_length_t contains representation of SIP
 * @ContentLength header.
 *
 * The #sip_content_length_t is defined as follows:
 * @code
 * typedef struct sip_content_length_s {
 *   sip_common_t   l_common[1];        // Common fragment info
 *   sip_error_t   *l_next;             // Dummy link to next
 *   uint32_t       l_length;           // Message body length in bytes
 * } sip_content_length_t;
 * @endcode
 */

msg_hclass_t sip_content_length_class[] =
SIP_HEADER_CLASS(content_length, "Content-Length", "l", l_common,
		 single_critical, any);

issize_t sip_content_length_d(su_home_t *home,
			      sip_header_t *h,
			      char *s,
			      isize_t slen)
{
  sip_content_length_t *l = (sip_content_length_t *)h;
  issize_t retval = msg_uint32_d(&s, &l->l_length);
  if (*s)
    retval = -1;
  return retval;
}

issize_t sip_content_length_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_content_length_t const *l = (sip_content_length_t const *)h;
  assert(sip_is_content_length(h));
  return snprintf(b, bsiz, "%lu", (unsigned long)l->l_length);
}

/**@ingroup sip_content_length
 *
 * Create a @ContentLength header object.
 *
 * Create a @ContentLength
 * header object with the value @a n.  The memory for the header is
 * allocated from the memory home @a home.
 *
 * @param home  memory home
 * @param n     payload size in bytes
 *
 * @return
 * A pointer to newly created @ContentLength header object when successful
 * or NULL upon an error.
 */
sip_content_length_t *sip_content_length_create(su_home_t *home, uint32_t n)
{
  sip_content_length_t *l = (sip_content_length_t *)
    sip_header_alloc(home, sip_content_length_class, 0);

  if (l)
    l->l_length = n;

  return l;
}

/* ====================================================================== */

/**@SIP_HEADER sip_date Date Header
 *
 * The Date header field reflects the time when the request or response was
 * first sent.  Its syntax is defined in @RFC3261 and @RFC2616 section 14.18 as
 * follows:
 *
 * @code
 *    Date          =  "Date" HCOLON SIP-date
 *    SIP-date      =  rfc1123-date
 *    rfc1123-date  =  wkday "," SP date1 SP time SP "GMT"
 *    date1         =  2DIGIT SP month SP 4DIGIT
 *                     ; day month year (e.g., 02 Jun 1982)
 *    time          =  2DIGIT ":" 2DIGIT ":" 2DIGIT
 *                     ; 00:00:00 - 23:59:59
 *    wkday         =  "Mon" / "Tue" / "Wed"
 *                     / "Thu" / "Fri" / "Sat" / "Sun"
 *    month         =  "Jan" / "Feb" / "Mar" / "Apr"
 *                     / "May" / "Jun" / "Jul" / "Aug"
 *                     / "Sep" / "Oct" / "Nov" / "Dec"
 * @endcode
 *
 * The parsed Date header is stored in #sip_date_t structure.
 */

/**@ingroup sip_date
 * @typedef typedef struct sip_date_s sip_date_t;
 *
 * The structure #sip_date_t contains representation of SIP @Date header.
 *
 * The #sip_date_t is defined as follows:
 * @code
 * typedef struct sip_date_s {
 *   sip_common_t   d_common[1];        // Common fragment info
 *   sip_date_t    *d_next;             // Link to next (dummy)
 *   sip_time_t     d_time;             // Seconds since Jan 1, 1900
 * } sip_date_t;
 * @endcode
 */

msg_hclass_t sip_date_class[] =
SIP_HEADER_CLASS(date, "Date", "", d_common, single, any);

issize_t sip_date_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_date_t *date = (sip_date_t *)h;

  if (msg_date_d((char const **)&s, &date->d_time) < 0 || *s)
    return -1;
  else
    return 0;
}

issize_t sip_date_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_date_t const *date = (sip_date_t *)h;

  return msg_date_e(b, bsiz, date->d_time);
}

/**@ingroup sip_date
 * @brief Create an @Date header object.
 *
 * Create a @Date header object with
 * the date @a date.  If @date is 0, current time (as returned by sip_now())
 * is used.
 *
 * @param home   memory home
 * @param date   date expressed as seconds since Mon, 01 Jan 1900 00:00:00
 *
 * @return
 * A pointer to newly created @Date header object when successful, or NULL
 * upon an error.
 */
sip_date_t *sip_date_create(su_home_t *home, sip_time_t date)
{
  sip_date_t *d = (sip_date_t *)sip_header_alloc(home, sip_date_class, 0);

  if (d) {
    if (date == 0)
      date = sip_now();
    d->d_time = date;
  }

  return d;
}

/* ====================================================================== */

/**@SIP_HEADER sip_expires Expires Header
 *
 * The Expires header field gives the date and time after which the message
 * content expires.  Its syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Expires     =  "Expires" HCOLON delta-seconds
 * @endcode
 *
 * Note that the first SIP revision (@RFC2543) also allowed absolute time in
 * Expires.
 *
 * The parsed Expires header is stored in #sip_expires_t structure.
 */

/**@ingroup sip_expires
 * @typedef typedef struct sip_expires_s sip_expires_t;
 *
 * The structure #sip_expires_t contains representation of SIP @Expires
 * header.
 *
 * The #sip_expires_t is defined as follows:
 * @code
 * typedef struct sip_expires_s {
 *   sip_common_t        ex_common[1];  // Common fragment info
 *   sip_error_t        *ex_next;       // Link to next (dummy)
 *   sip_time_t          ex_date;       // Seconds since Jan 1, 1900
 *   sip_time_t          ex_delta;      // ...or delta seconds
 * } sip_expires_t;
 * @endcode
 */

msg_hclass_t sip_expires_class[] =
SIP_HEADER_CLASS(expires, "Expires", "", ex_common, single, any);

issize_t sip_expires_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_expires_t *expires = (sip_expires_t *)h;

  if (msg_date_delta_d((char const **)&s,
		       &expires->ex_date,
		       &expires->ex_delta) < 0 || *s)
    return -1;
  else
    return 0;
}

issize_t sip_expires_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_expires_t const *expires = (sip_expires_t *)h;

  if (expires->ex_date)
    return msg_date_e(b, bsiz, expires->ex_date + expires->ex_delta);
  else
    return msg_delta_e(b, bsiz, expires->ex_delta);
}

/**@ingroup sip_expires
 * @brief Create an @Expires header object.
 *
 * Create an @Expires header object with the expiration time @a delta.
 *
 * @param home   memory home used to allocate #sip_expires_t structure
 * @param delta  relative expiration time in seconds
 *
 * @return
 * A pointer to newly created @Expires header object when successful or NULL
 * upon an error.
 */
sip_expires_t *sip_expires_create(su_home_t *home, sip_time_t delta)
{
  sip_expires_t *ex = (sip_expires_t *)
    sip_header_alloc(home, sip_expires_class, 0);

  if (ex)
    ex->ex_delta = delta;

  return ex;
}

/* ====================================================================== */

/**@SIP_HEADER sip_from From Header
 *
 * The From header indicates the initiator of the request.  It is defined in
 * @RFC3261 as follows:
 *
 * @code
 *    From        =  ( "From" / "f" ) HCOLON from-spec
 *    from-spec   =  ( name-addr / addr-spec )
 *                   *( SEMI from-param )
 *    from-param  =  tag-param / generic-param
 *    tag-param   =  "tag" EQUAL token
 * @endcode
 *
 *
 * The parsed From header is stored in #sip_from_t structure.
 */

/**@ingroup sip_from
 * @typedef typedef struct sip_addr_s sip_from_t;
 *
 * The structure #sip_from_t contains representation of @From header.
 *
 * The #sip_from_t is defined as follows:
 * @code
 * typedef struct sip_addr_s {
 *   sip_common_t       a_common[1];    // Common fragment info
 *   sip_error_t       *a_next;         // Link to next
 *   char const        *a_display;      // Display name
 *   url_t              a_url[1];       // URL
 *   msg_param_t const *a_params;       // List of from-param
 *   char const        *a_comment;      // Comment
 *   char const        *a_tag;          // Tag parameter
 * } sip_from_t;
 * @endcode
 *
 */

msg_hclass_t sip_from_class[] =
SIP_HEADER_CLASS(from, "From", "f", a_params, single, addr);

issize_t sip_from_d(su_home_t *home,
		    sip_header_t *h,
		    char *s,
		    isize_t slen)
{
  return sip_addr_d(home, h, s, slen);
}

issize_t sip_from_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_from(h));
  return sip_addr_e(b, bsiz, h, flags);
}

/**@ingroup sip_from
 *
 * Create a @From header object with URL.
 *
 * @param home      memory home used to allocate #sip_from_t structure
 * @param s         pointer to the URL or a string
 *
 * @return
 * A pointer to newly created @From header object when successful or NULL
 * upon an error.
 */
sip_from_t *
sip_from_create(su_home_t *home, url_string_t const *s)
{
  return sip_addr_make_url(home, sip_from_class, s);
}

/**@ingroup sip_from
 *
 * Add a parameter to an #sip_from_t object.
 *
 * @param home   memory home
 * @param from   a pointer to #sip_from_t object
 * @param param  parameter string
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @deprecated Use msg_header_replace_param() directly.
 */
int sip_from_add_param(su_home_t *home,
		       sip_from_t *from,
		       char const *param)
{
  return msg_header_replace_param(home, from->a_common, param);
}

/**@ingroup sip_from
 *
 * Add a tag to a @From header. If @a tag is
 * identical with the existing one, nothing will be done. An error is
 * returned, if the header already contains a different tag. The @a tag can
 * be provided either as a single token ("deadbeer") or as in parameter form
 * ("tag=deadbeer"). In both cases the tag is duplicated using the memory
 * home @a home.
 *
 * @param home memory home used to allocate new tag
 * @param from @From header to modify
 * @param tag  tag token or parameter to be added
 *
 * @retval 0 when successful
 * @retval -1 upon an error.
 */
int sip_from_tag(su_home_t *home, sip_from_t *from, char const *tag)
{
  return sip_addr_tag(home, from, tag);
}


int sip_to_tag(su_home_t *home, sip_to_t *to, char const *tag)
{
  return sip_addr_tag(home, to, tag);
}

/* ====================================================================== */

/**@SIP_HEADER sip_max_forwards Max-Forwards Header
 *
 * The Max-Forwards header is used to limit the number of proxies or
 * gateways that can forward the request.  The Max-Forwards syntax is
 * defined in @RFC3261 as follows:
 *
 * @code
 *    Max-Forwards  =  "Max-Forwards" HCOLON 1*DIGIT
 * @endcode
 *
 *
 * The parsed Max-Forwards header is stored in #sip_max_forwards_t structure.
 */

/**@ingroup sip_max_forwards
 * @typedef typedef struct sip_max_forwards_s sip_max_forwards_t;
 *
 * The structure #sip_max_forwards_t contains representation of SIP
 * @MaxForwards header.
 *
 * The #sip_max_forwards_t is defined as follows:
 * @code
 * typedef struct sip_max_forwards_s {
 *   sip_common_t        mf_common[1];  // Common fragment info
 *   sip_error_t        *mf_next;       // Link to next (dummy)
 *   unsigned long       mf_count;      // Digits
 * } sip_max_forwards_t;
 * @endcode
 */

msg_hclass_t sip_max_forwards_class[] =
SIP_HEADER_CLASS(max_forwards, "Max-Forwards", "", mf_common,
		 single, any);

issize_t sip_max_forwards_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_numeric_d(home, h, s, slen);
}

issize_t sip_max_forwards_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_max_forwards(h));
  return sip_numeric_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_min_expires Min-Expires Header
 *
 * The Min-Expires header is used to limit the number of proxies or
 * gateways that can forward the request.  The Min-Expires syntax is
 * defined in @RFC3261 as follows:
 *
 * @code
 *    Min-Expires  =  "Min-Expires" HCOLON delta-seconds
 * @endcode
 *
 * The parsed Min-Expires header is stored in #sip_min_expires_t structure.
 */

/**@ingroup sip_min_expires
 * @typedef typedef struct sip_min_expires_s sip_min_expires_t;
 *
 * The structure #sip_min_expires_t contains representation of SIP
 * @MinExpires header.
 *
 * The #sip_min_expires_t is defined as follows:
 * @code
 * typedef struct sip_min_expires_s {
 *   sip_common_t        me_common[1];  // Common fragment info
 *   sip_error_t        *me_next;       // Link to next (dummy)
 *   unsigned long       me_delta;      // Seconds
 * } sip_min_expires_t;
 * @endcode
 */

msg_hclass_t sip_min_expires_class[] =
SIP_HEADER_CLASS(min_expires, "Min-Expires", "", me_common,
		 single, any);

issize_t sip_min_expires_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_numeric_d(home, h, s, slen);
}

issize_t sip_min_expires_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_min_expires(h));
  return sip_numeric_e(b, bsiz, h, f);
}


/* ====================================================================== */

/**@SIP_HEADER sip_retry_after Retry-After Header
 *
 * The Retry-After response-header field @RFC3261 section 20.33 can be used to
 * indicate how long the service is expected to be unavailable or when the
 * called party anticipates being available again. Its syntax is defined in
 * @RFC3261 as follows:
 *
 * @code
 *      Retry-After  =  "Retry-After" HCOLON delta-seconds
 *                      [ comment ] *( SEMI retry-param )
 *      retry-param  =  ("duration" EQUAL delta-seconds)
 *                      / generic-param
 * @endcode
 *
 * The parsed Retry-After header is stored in #sip_retry_after_t structure.
 */

/**@ingroup sip_retry_after
 * @typedef struct sip_retry_after_s sip_retry_after_t;
 *
 * The structure #sip_retry_after_t contains representation of an
 * @RetryAfter header.
 *
 * The #sip_retry_after_t is defined as follows:
 * @code
 * typedef struct sip_retry_after_s {
 *   sip_common_t        af_common[1]; // Common fragment info
 *   sip_error_t        *af_next;      // Link to next (dummy)
 *   sip_time_t          af_delta;     // Seconds to before retry
 *   char const         *af_comment;   // Comment string
 *   msg_param_t const  *af_params;    // List of parameters
 *   char const         *af_duration;  // Duration parameter
 * } sip_retry_after_t;
 * @endcode
 */

static msg_xtra_f sip_retry_after_dup_xtra;
static msg_dup_f sip_retry_after_dup_one;
static msg_update_f sip_retry_after_update;

msg_hclass_t sip_retry_after_class[] =
SIP_HEADER_CLASS(retry_after, "Retry-After", "", af_params, single,
		 retry_after);

issize_t sip_retry_after_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_retry_after_t *af = (sip_retry_after_t *)h;

  if ((msg_delta_d((char const **)&s, &af->af_delta) < 0) ||
      (*s == '(' && msg_comment_d(&s, &af->af_comment) == -1) ||
      (*s == ';' && msg_params_d(home, &s, &af->af_params) == -1) ||
      (*s != '\0')) {
    if (af->af_params)
      su_free(home, (void *)af->af_params), af->af_params = NULL;
    return -1;
  }

  if (af->af_params)
    msg_header_update_params(h->sh_common, 0);

  return 0;
}

issize_t sip_retry_after_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_retry_after_t const *af = (sip_retry_after_t *)h;
  int const compact = MSG_IS_COMPACT(f);
  char *b0 = b, *end = b + bsiz;

  b += snprintf(b, bsiz, "%lu", af->af_delta);

  if (af->af_comment) {
    if (!compact)
      MSG_CHAR_E(b, end, ' ');
    MSG_CHAR_E(b, end, '(');
    MSG_STRING_E(b, end, af->af_comment);
    MSG_CHAR_E(b, end, ')');
    if (!compact && af->af_params && af->af_params[0])
      MSG_CHAR_E(b, end, ' ');
  }

  if (af->af_params)
    MSG_PARAMS_E(b, end, af->af_params, f);

  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t sip_retry_after_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_retry_after_t const *af = (sip_retry_after_t *)h;

  MSG_PARAMS_SIZE(offset, af->af_params);
  offset += MSG_STRING_SIZE(af->af_comment);

  return offset;
}

char *sip_retry_after_dup_one(sip_header_t *dst,
			      sip_header_t const *src,
			      char *b,
			      isize_t xtra)
{
  sip_retry_after_t *af = (sip_retry_after_t *)dst;
  sip_retry_after_t const *o = (sip_retry_after_t *)src;
  char *end = b + xtra;

  b = msg_params_dup(&af->af_params, o->af_params, b, xtra);
  MSG_STRING_DUP(b, af->af_comment, o->af_comment);
  af->af_delta = o->af_delta;

  assert(b <= end); (void)end;

  return b;
}

static int sip_retry_after_update(msg_common_t *h,
				  char const *name, isize_t namelen,
				  char const *value)
{
  sip_retry_after_t *af = (sip_retry_after_t *)h;

  if (name == NULL) {
    af->af_duration = NULL;
  }
  else if (namelen == strlen("duration") &&
	   su_casenmatch(name, "duration", namelen)) {
    af->af_duration = value;
  }

  return 0;
}


/* ====================================================================== */

/**Parse a @Route or a @RecordRoute header.
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
issize_t sip_any_route_d(su_home_t *home,
			 sip_header_t *h,
			 char *s,
			 isize_t slen)
{
  sip_route_t *r = (sip_route_t *)h;

  assert(h);

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  if (sip_name_addr_d(home, &s, &r->r_display,
		      r->r_url, &r->r_params, NULL) < 0)
    return -1;

  return msg_parse_next_field(home, h, s, slen);
}

issize_t sip_any_route_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  sip_route_t const *r = (sip_route_t *)h;

  return sip_name_addr_e(b, bsiz, flags,
			 r->r_display, 1, r->r_url, r->r_params, NULL);
}

isize_t sip_any_route_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_route_t const *r = (sip_route_t *)h;
  return sip_name_addr_xtra(r->r_display,
			    r->r_url,
			    r->r_params,
			    offset);
}

char *sip_any_route_dup_one(sip_header_t *dst, sip_header_t const *src,
			    char *b,
			    isize_t xtra)
{
  sip_route_t *r = (sip_route_t *)dst;
  sip_route_t const *o = (sip_route_t *)src;
  return sip_name_addr_dup(&r->r_display, o->r_display,
			   r->r_url, o->r_url,
			   &r->r_params, o->r_params,
			   b, xtra);
}

#define sip_any_route_update NULL


/** Create a route.
 *
 * Create a route or record-route entry
 * from two URLs; first one provides the URL, second maddr parameter and
 * port.
 *
 * @param home   memory home
 * @param rq_url route URL
 * @param maddr  optional route address and port
 *  */
static
sip_route_t *sip_any_route_create(su_home_t *home,
				  msg_hclass_t *hc,
				  url_t const *rq_url,
				  url_t const *maddr)
{
  sip_header_t *h;
  sip_route_t *rr;
  url_t url[1];
  size_t xtra, n, n_url, n_params, n_addr;
  char *b, *param;

  *url = *rq_url;
  if (maddr) {
    url->url_port = maddr->url_port;
    url->url_params = NULL;
  }
  n_url = url_xtra(url);

  n_params = maddr && maddr->url_params ? strlen(maddr->url_params) : 0;

  if (maddr && (!maddr->url_params ||
		!url_param(maddr->url_params, "maddr", NULL, 0)))
    n_addr = (n_params != 0) + strlen("maddr=") + strlen(maddr->url_host);
  else
    n_addr = 0;

  xtra = n_url + n_params + n_addr + (n_params || n_addr);

  h = sip_header_alloc(home, hc, xtra);
  if ((rr = (sip_record_route_t *)h)) {
    b = sip_header_data(h);
    n = url_dup(b, n_url, rr->r_url, url);
    assert(n == n_url);

    if (n_params || n_addr) {
      param = b + n_url;
      if (n_params) {
	rr->r_url->url_params = strcpy(param, maddr->url_params);
	param += n_params;
      }
      if (n_addr) {
	if (n_params)
	  *param++ = ';';
	strcpy(param, "maddr="), param += strlen("maddr=");
	strcpy(param, maddr->url_host), param += strlen(maddr->url_host);
      }
      assert(b + xtra == param + 1);
    }
  }

  return rr;
}

/* ====================================================================== */

/**@SIP_HEADER sip_route Route Header
 *
 * The Route headers is used to store the route set of a transaction.
 * The Route header is defined in @RFC3261 as follows:
 *
 * @code
 *    Route        =  "Route" HCOLON route-param *(COMMA route-param)
 *    route-param  =  name-addr *( SEMI rr-param )
 * @endcode
 *
 * The parsed Route header is stored in #sip_route_t structure.
 */

/**@ingroup sip_route
 * @typedef typedef struct sip_route_s sip_route_t;
 *
 * The structure #sip_route_t contains representation of SIP @Route header.
 *
 * The #sip_route_t is defined as follows:
 * @code
 * typedef struct sip_route_s {
 *   sip_common_t        r_common[1];   // Common fragment info
 *   sip_route_t        *r_next;        // Link to next @Route
 *   char const         *r_display;     // Display name
 *   url_t               r_url[1];      // @Route URL
 *   msg_param_t const  *r_params;      // List of route parameters
 * } sip_route_t;
 * @endcode
 */

msg_hclass_t sip_route_class[] =
SIP_HEADER_CLASS(route, "Route", "", r_params, append, any_route);

issize_t sip_route_d(su_home_t *home,
		     sip_header_t *h,
		     char *s,
		     isize_t slen)
{
  return sip_any_route_d(home, h, s, slen);
}

issize_t sip_route_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_route(h));
  return sip_any_route_e(b, bsiz, h, flags);
}

/**@ingroup sip_route
 * @brief Create a @Route header object.
 *
 * Creates a route entry from two URLs; first one provides the URL, second
 * maddr parameter and port.
 *
 * @param home   memory home
 * @param url    route URL
 * @param maddr  optional route address and port
 *
 * @return
 * Returns a pointer to newly created @Route header object when successful,
 * or NULL upon an error.
 */
sip_route_t *sip_route_create(su_home_t *home,
			      url_t const *url,
			      url_t const *maddr)
{
  return sip_any_route_create(home, sip_route_class, url, maddr);
}

/* ====================================================================== */

/**@SIP_HEADER sip_record_route Record-Route Header
 *
 * The Record-Route headers are used to establish a route for transactions
 * belonging to a session.  The Record-Route header is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Record-Route  =  "Record-Route" HCOLON rec-route *(COMMA rec-route)
 *    rec-route     =  name-addr *( SEMI rr-param )
 *    rr-param      =  generic-param
 * @endcode
 *
 * The parsed Record-Route header is stored in #sip_record_route_t structure.
 */

/**@ingroup sip_record_route
 * @typedef typedef struct sip_record_route_s sip_record_route_t;
 *
 * The structure #sip_record_route_t contains representation of SIP
 * @RecordRoute header.
 *
 * The #sip_record_route_t is defined as follows:
 * @code
 * typedef struct sip_route_s {
 *   sip_common_t        r_common[1];   // Common fragment info
 *   sip_record_route_t *r_next;        // Link to next <rec-route>
 *   char const         *r_display;     // Display name
 *   url_t               r_url[1];      // @RecordRoute URL
 *   msg_param_t const  *r_params;      // List of route parameters
 * } sip_record_route_t;
 * @endcode
 */

msg_hclass_t sip_record_route_class[] =
SIP_HEADER_CLASS(record_route, "Record-Route", "",
		 r_params, prepend, any_route);

issize_t sip_record_route_d(su_home_t *home,
			    sip_header_t *h,
			    char *s,
			    isize_t slen)
{
  return sip_any_route_d(home, h, s, slen);
}

issize_t sip_record_route_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_record_route(h));
  return sip_any_route_e(b, bsiz, h, flags);
}

/** @ingroup sip_record_route
 *
 * Create a record-route.
 *
 * Create a record-route entry from two URLs; first one provides the URL,
 * second maddr parameter and port.
 *
 * @param home   memory home
 * @param rq_url route URL
 * @param maddr  optional route address and port
 *
 * @return
 * A pointer to newly created @RecordRoute header object when successful or
 * NULL upon an error.
 */
sip_record_route_t *sip_record_route_create(su_home_t *home,
					    url_t const *rq_url,
					    url_t const *maddr)
{
  return sip_any_route_create(home, sip_record_route_class, rq_url, maddr);
}

/* ====================================================================== */

/**@SIP_HEADER sip_to To Header
 *
 * The To header field specifies the "logical" recipient of the
 * request.  It is defined in @RFC3261 as follows:
 *
 * @code
 *    To        =  ( "To" / "t" ) HCOLON ( name-addr
 *                 / addr-spec ) *( SEMI to-param )
 *    to-param  =  tag-param / generic-param
 * @endcode
 *
 * The parsed To header is stored in #sip_to_t structure.
 */

/**@ingroup sip_to
 * @typedef typedef struct sip_addr_s sip_to_t;
 *
 * The structure #sip_to_t contains representation of @To header.
 *
 * The #sip_to_t is defined as follows:
 * @code
 * typedef struct {
 *   sip_common_t       a_common[1];    // Common fragment info
 *   sip_error_t       *a_next;         // Link to next (dummy)
 *   char const        *a_display;      // Display name
 *   url_t              a_url[1];       // URL
 *   msg_param_t const *a_params;       // List of to-params
 *   char const        *a_comment;      // Comment
 *   char const        *a_tag;          // Tag parameter
 * } sip_to_t;
 * @endcode
 *
 */

msg_hclass_t sip_to_class[] =
SIP_HEADER_CLASS(to, "To", "t", a_params, single, addr);

issize_t sip_to_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_addr_d(home, h, s, slen);
}

issize_t sip_to_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  assert(sip_is_to(h));

  return sip_addr_e(b, bsiz, h, flags);
}

/**@ingroup sip_to
 *
 * Create a @To header object with URL.
 *
 * @param home      memory home
 * @param url       URL (string or pointer to url_t)
 *
 * @return
 * A pointer to newly created @To header object when successful or NULL upon
 * an error.
 */
sip_to_t *
sip_to_create(su_home_t *home, url_string_t const *url)
{
  return sip_addr_make_url(home, sip_to_class, url);
}

/**@ingroup sip_to
 *
 * Add a parameter to a #sip_to_t object.
 *
 * @note This function @b does @b not duplicate @p param.
 *
 * @param home   memory home
 * @param to     #sip_to_t structure
 * @param param  parameter string
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @deprecated Use msg_header_replace_param() directly.
 */
int sip_to_add_param(su_home_t *home,
		     sip_to_t *to,
		     char const *param)
{
  return msg_header_replace_param(home, to->a_common, param);
}

/* ====================================================================== */
/**@SIP_HEADER sip_via Via Header
 *
 * The Via header indicates the path taken by the request so far.  Via
 * headers can be used to prevent request looping and ensure replies take
 * the same path as the requests.  The Via syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Via               =  ( "Via" / "v" ) HCOLON via-parm *(COMMA via-parm)
 *    via-parm          =  sent-protocol LWS sent-by *( SEMI via-params )
 *    via-params        =  via-ttl / via-maddr
 *                         / via-received / via-branch
 *                         / via-extension
 *    via-ttl           =  "ttl" EQUAL ttl
 *    via-maddr         =  "maddr" EQUAL host
 *    via-received      =  "received" EQUAL (IPv4address / IPv6address)
 *    via-branch        =  "branch" EQUAL token
 *    via-extension     =  generic-param
 *    sent-protocol     =  protocol-name SLASH protocol-version
 *                         SLASH transport
 *    protocol-name     =  "SIP" / token
 *    protocol-version  =  token
 *    transport         =  "UDP" / "TCP" / "TLS" / "SCTP"
 *                         / other-transport
 *    sent-by           =  host [ COLON port ]
 *    ttl               =  1*3DIGIT ; 0 to 255
 * @endcode
 *
 * @note
 * The @RFC2543 syntax allowed <comment>. We accept it, but don't encode it.
 *
 * In addition to the parameters defined in @RFC3261, @RFC3486 defines a
 * parameter "comp":
 * @code
 *     via-compression  =  "comp" EQUAL ("sigcomp" / other-compression)
 *    via-params      /=  via-compression
 * @endcode
 *
 * @RFC3581 defines a parameter "rport":
 * @code
 *    response-port  =  "rport" [EQUAL 1*DIGIT]
 *    via-params    /=  response-port
 * @endcode
 *
 * The parsed Via header is stored in #sip_via_t structure.
 */

/**@ingroup sip_via
 * @typedef typedef struct sip_via_s sip_via_t;
 *
 * The structure #sip_via_t contains representation of SIP @Via header.
 *
 * The #sip_via_t is defined as follows:
 * @code
 * typedef struct sip_via_s {
 *   sip_common_t        v_common[1];   // Common fragment info
 *   sip_via_t          *v_next;        // Link to next @Via header
 *   char const         *v_protocol;    // Application and transport protocol
 *   char const         *v_host;        // Hostname
 *   char const         *v_port;        // Port number
 *   msg_param_t const  *v_params;      // List of via-params
 *   char const         *v_comment;     // Comment
 *
 *   char const         *v_ttl;         // "ttl" parameter
 *   char const         *v_maddr;       // "maddr" parameter
 *   char const         *v_received;    // "received" parameter
 *   char const         *v_branch;      // "branch" parameter
 *   char const         *v_comp;        // "comp" parameter
 *   char const         *v_rport;       // "rport" parameter
 * } sip_via_t;
 * @endcode
 */

static msg_xtra_f sip_via_dup_xtra;
static msg_dup_f sip_via_dup_one;
static msg_update_f sip_via_update;

msg_hclass_t sip_via_class[] =
SIP_HEADER_CLASS(via, "Via", "v", v_params, prepend, via);

issize_t sip_via_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_via_t *v = (sip_via_t *)h;

  assert(h);

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  /* sent-protocol sent-by *( ";" via-params ) [ comment ] */

  /* Parse protocol */
  if (sip_transport_d(&s, &v->v_protocol) == -1)
    return -1;
  /* Host (and port) */
  if (msg_hostport_d(&s, &v->v_host, &v->v_port) == -1)
    return -1;
  /* Parameters */
  if (*s == ';' && msg_params_d(home, &s, &v->v_params) == -1)
    return -1;
  /* Comment */
  if (*s == '(' && msg_comment_d(&s, &v->v_comment) == -1)
    return -1;

  return msg_parse_next_field(home, h, s, slen);
}

issize_t sip_via_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  sip_via_t const *v = (sip_via_t *)h;

  assert(sip_is_via(h));

  MSG_STRING_E(b, end, v->v_protocol);
  MSG_CHAR_E(b, end, ' ');
  MSG_STRING_E(b, end, v->v_host);
  if (v->v_port) {
    MSG_CHAR_E(b, end, ':');
    MSG_STRING_E(b, end, v->v_port);
  }
  MSG_PARAMS_E(b, end, v->v_params, flags);
#if 0
  /* Comment is deprecated in @RFC3265 - accept it, but do not send */
  if (v->v_comment) {
    if (!MSG_IS_COMPACT(flags))
      MSG_CHAR_E(b, end, ' ');
    MSG_CHAR_E(b, end, '(');
    MSG_STRING_E(b, end, v->v_comment);
    MSG_CHAR_E(b, end, ')');
  }
#endif
  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t sip_via_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_via_t const *v = (sip_via_t *)h;

  MSG_PARAMS_SIZE(offset, v->v_params);
  offset += sip_transport_xtra(v->v_protocol);
  offset += MSG_STRING_SIZE(v->v_host);
  offset += MSG_STRING_SIZE(v->v_port);
  offset += MSG_STRING_SIZE(v->v_comment);

  return offset;
}

/** Duplicate one #sip_via_t object */
char *sip_via_dup_one(sip_header_t *dst, sip_header_t const *src,
		      char *b, isize_t xtra)
{
  sip_via_t *v = (sip_via_t *)dst;
  sip_via_t const *o = (sip_via_t *)src;
  char *end = b + xtra;

  b = msg_params_dup(&v->v_params, o->v_params, b, xtra);
  sip_transport_dup(&b, &v->v_protocol, o->v_protocol);
  MSG_STRING_DUP(b, v->v_host, o->v_host);
  MSG_STRING_DUP(b, v->v_port, o->v_port);
  MSG_STRING_DUP(b, v->v_comment, o->v_comment);

  assert(b <= end); (void)end;

  return b;
}

static int sip_via_update(msg_common_t *h,
			  char const *name, isize_t namelen,
			  char const *value)
{
  sip_via_t *v = (sip_via_t *)h;

  if (name == NULL) {
    v->v_ttl = NULL;
    v->v_maddr = NULL;
    v->v_received = NULL;
    v->v_branch = NULL;
    v->v_rport = NULL;
    v->v_comp = NULL;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

   else if (MATCH(ttl)) {
     v->v_ttl = value;
   }
   else if (MATCH(maddr)) {
     v->v_maddr = value;
   }
   else if (MATCH(received)) {
     v->v_received = value;
   }
   else if (MATCH(branch)) {
     v->v_branch = value;
   }
   else if (MATCH(rport)) {
     v->v_rport = value;
   }
   else if (MATCH(comp)) {
     v->v_comp = value;
   }

 #undef MATCH

   return 0;
 }

/**@ingroup sip_via
 *
 * Add a parameter to a @Via object.
 *
 * @note This function @b does @b not duplicate @p param.
 *
 * @param home   memory home
 * @param v      #sip_via_t object
 * @param param  parameter string
 *
 * @retval 0 when successful
 * @retval -1 upon an error.
 *
 * @deprecated Use msg_header_replace_param() directly.
 */
int sip_via_add_param(su_home_t *home,
		      sip_via_t *v,
		      char const *param)
{
  return msg_header_replace_param(home, v->v_common, param);
}

/**@ingroup sip_via
 *
 * Create a @Via object.
 *
 * Create a new @Via header object with
 * given parameters.  If @a transport is NULL, the default transport
 * "SIP/2.0/UDP" is used.  A NULL-terminated list of parameters can be
 * specified after transport.
 *
 * @param home 	    memory home
 * @param host 	    host name
 * @param port 	    protocol port number
 * @param transport transport protocol (default is "SIP/2.0/UDP")
 * @param ...       NULL-terminated list of parameters
 *
 * @return
 * A pointer to newly created
 * @Via header object when successful or NULL upon an error.
 */
sip_via_t *sip_via_create(su_home_t *home,
                          char const *host,
                          char const *port,
                          char const *transport,
                          /* char const *params */
                          ...)
{
  sip_via_t *v, via[1] = {{{{ NULL }}}};
  va_list params;

  via->v_common->h_class = sip_via_class;

  if (transport)
    via->v_protocol = transport;
  else
    via->v_protocol = sip_transport_udp;

  via->v_host = host;
  via->v_port = port;

  v = (sip_via_t *)msg_header_dup_as(home, sip_via_class, (sip_header_t *)via);

  if (v) {
    char const *param;
    va_start(params, transport);

    for (param = va_arg(params, char const *);
         param;
         param = va_arg(params, char const *)) {
      if ((param = su_strdup(home, param))) {
	if (msg_header_replace_param(home, v->v_common, param) < 0)
	  break;
      }
    }

    va_end(params);
  }

  return v;
}

/**@ingroup sip_via
 *
 * Get port number corresponding to a @Via line.
 *
 * If @a using_rport is non-null, try rport.
 * If *using_rport is non-zero, try rport even if <protocol> is not UDP.
 * If <protocol> is UDP, set *using_rport to zero.
 */
char const *sip_via_port(sip_via_t const *v, int *using_rport)
{
  if (v == NULL)
    return NULL;

  if (using_rport) {
    char const *port;

    if (v->v_rport && !v->v_maddr /* multicast */) {
      if (v->v_protocol == sip_transport_udp ||
	  su_casematch(v->v_protocol, sip_transport_udp))
	port = v->v_rport, *using_rport = 0;
      else if (*using_rport)
	port = v->v_rport;
      else
	port = NULL;

      if (port && port[0])
	return port;
    }

    *using_rport = 0;		/* No, we don't... */
  }

  if (v->v_port)
    return v->v_port;

  if (sip_transport_has_tls(v->v_protocol))
    return SIPS_DEFAULT_SERV;	/* 5061 */
  else
    return SIP_DEFAULT_SERV;	/* 5060 */
}
