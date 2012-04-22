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

/**@CFILE http_basic.c
 * @brief HTTP basic header
 *
 * The file @b http_basic.c contains implementation of header classes for
 * basic HTTP headers, like request and status lines, payload, @b Call-ID, @b
 * CSeq, @b Contact, @b Content-Length, @b Date, @b Expires, @b From, @b
 * Route, @b Record-Route, @b To, and @b Via.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date  Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include <sofia-sip/http_parser.h>
#include <sofia-sip/http_header.h>
#include <sofia-sip/http_status.h>

#include <sofia-sip/msg_mime_protos.h>
#include <sofia-sip/msg_date.h>

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>

/* ====================================================================== */

/**@HTTP_HEADER http_request HTTP request line.
 *
 * The HTTP request line contains the method, URL, and an optional HTTP
 * protocol version. The missing version indicates version 0.9 without any
 * request headers.
 */

/**
 * Parse request line of a HTTP message.
 *
 * The function @c http_request_d() parses the request line from a a HTTP
 * message.
 */
issize_t http_request_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_request_t *rq = h->sh_request;
  char *uri, *version;

  if (msg_firstline_d(s, &uri, &version) < 0 ||
      (rq->rq_method = http_method_d(&s, &rq->rq_method_name)) < 0 || *s ||
      url_d(rq->rq_url, uri) < 0 ||
      (http_version_d(&version, &rq->rq_version) < 0 || version[0]))
    return -1;

  return 0;
}

/**
 * Encode a HTTP request line.
 *
 * The function @c http_request_e() prints a HTTP request line.
 */
issize_t http_request_e(char b[], isize_t bsiz, http_header_t const *h, int flags)
{
  http_request_t const *rq = h->sh_request;

  return snprintf(b, bsiz, "%s " URL_FORMAT_STRING "%s%s" CRLF,
		  rq->rq_method_name,
		  URL_PRINT_ARGS(rq->rq_url),
		  rq->rq_version ? " " : "",
		  rq->rq_version ? rq->rq_version : "");
}

isize_t http_request_dup_xtra(http_header_t const *h, isize_t offset)
{
  http_request_t const *rq = h->sh_request;

  offset += url_xtra(rq->rq_url);
  if (!rq->rq_method)
    offset += MSG_STRING_SIZE(rq->rq_method_name);
  if (rq->rq_version)
    offset += http_version_xtra(rq->rq_version);

  return offset;
}

/** Duplicate one request header. */
char *http_request_dup_one(http_header_t *dst, http_header_t const *src,
			   char *b, isize_t xtra)
{
  http_request_t *rq = dst->sh_request;
  http_request_t const *o = src->sh_request;
  char *end = b + xtra;

  URL_DUP(b, end, rq->rq_url, o->rq_url);

  if (!(rq->rq_method = o->rq_method))
    MSG_STRING_DUP(b, rq->rq_method_name, o->rq_method_name);
  else
    rq->rq_method_name = o->rq_method_name;
  http_version_dup(&b, &rq->rq_version, o->rq_version);

  assert(b <= end);

  return b;
}

/** Create a request line object.
 *
 * Note that version string is not copied; it @b MUST remain constant during
 * lifetime of the @c http_request_t object. You can use constants
 * http_version_1_1 or http_version_1_0 declared in <sofia-sip/http_header.h>.
 */
http_request_t *http_request_create(su_home_t *home,
				    http_method_t method, char const *name,
				    url_string_t const *url,
				    char const *version)
{
  size_t xtra;
  http_request_t *rq;

  if (method)
    name = http_method_name(method, name);

  if (!name)
    return NULL;

  xtra = url_xtra(url->us_url) + (method ? 0 : strlen(name) + 1);

  rq = msg_header_alloc(home, http_request_class, (isize_t)xtra)->sh_request;

  if (rq) {
    char *b = (char *)(rq + 1), *end = b + xtra;

    rq->rq_method      = method;
    rq->rq_method_name = name;
    if (!method)
      MSG_STRING_DUP(b, rq->rq_method_name, name);

    URL_DUP(b, end, rq->rq_url, url->us_url);

    rq->rq_version = version ? version : HTTP_VERSION_CURRENT;
    assert(b == end);
  }

  return rq;
}

msg_hclass_t http_request_class[] =
HTTP_HEADER_CLASS(request, NULL, rq_common, single_critical, request);

/* ====================================================================== */

/**@HTTP_HEADER http_status HTTP status line.
 *
 * The HTTP status line contains the HTTP protocol version, a reason code
 * (100..599) and reason phrase.
 */

/** Parse status line */
issize_t http_status_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_status_t *st = h->sh_status;
  char *status, *phrase;
  uint32_t code;

  if (msg_firstline_d(s, &status, &phrase) < 0 ||
      http_version_d(&s, &st->st_version) < 0 || *s ||
      msg_uint32_d(&status, &code) == -1 ||
      status[0])
    return -1;

  st->st_status = code;
  st->st_phrase = phrase;

  return 0;
}

issize_t http_status_e(char b[], isize_t bsiz, http_header_t const *h, int flags)
{
  http_status_t const *st = h->sh_status;
  char const *phrase = st->st_phrase;

  if (phrase == NULL)
    phrase = "";

  if (st->st_version)
    return snprintf(b, bsiz, "%s %03u %s" CRLF,
		    st->st_version,
		    st->st_status,
		    phrase);
  else
    return snprintf(b, bsiz, "%03u %s" CRLF,
		    st->st_status,
		    phrase);
}

/** Extra size of a http_status_t object. */
isize_t http_status_dup_xtra(http_header_t const *h, isize_t offset)
{
  if (h->sh_status->st_version)
    offset += http_version_xtra(h->sh_status->st_version);
  offset += MSG_STRING_SIZE(h->sh_status->st_phrase);
  return offset;
}

/** Duplicate one status header. */
char *http_status_dup_one(http_header_t *dst, http_header_t const *src,
			  char *b, isize_t xtra)
{
  http_status_t *st = dst->sh_status;
  http_status_t const *o = src->sh_status;
  char *end = b + xtra;

  if (o->st_version)
    http_version_dup(&b, &st->st_version, o->st_version);
  st->st_status = o->st_status;
  MSG_STRING_DUP(b, st->st_phrase, o->st_phrase);

  assert(b <= end); (void)end;

  return b;
}

/** Create a status line object.
 *
 * Note that version is not copied; it @b MUST remain constant during
 * lifetime of the @c http_status_t object.
 */
http_status_t *http_status_create(su_home_t *home,
				  unsigned status,
				  char const *phrase,
				  char const *version)
{
  http_status_t *st;

  if (phrase == NULL && (phrase = http_status_phrase(status)) == NULL)
    return NULL;

  if ((st = msg_header_alloc(home, http_status_class, 0)->sh_status)) {
    st->st_status = status;
    st->st_phrase = phrase;
    st->st_version = version ? version : HTTP_VERSION_CURRENT;
  }

  return st;
}

msg_hclass_t http_status_class[] =
HTTP_HEADER_CLASS(status, NULL, st_common, single_critical, status);

/* ====================================================================== */
/**@HTTP_HEADER http_accept Accept header.
 *
 * We use MIME Accept header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_accept_charset Accept-Charset header.
 *
 * We use MIME Accept-Charset header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_accept_encoding Accept-Encoding header.
 *
 * We use MIME Accept-Encoding header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_accept_language Accept-Language header.
 *
 * We use MIME Accept-Language header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_accept_ranges Accept-Ranges header. */

#define http_accept_ranges_d msg_list_d
#define http_accept_ranges_e msg_list_e
msg_hclass_t http_accept_ranges_class[] =
HTTP_HEADER_CLASS_LIST(accept_ranges, "Accept-Ranges", list);

/* ====================================================================== */
/**@HTTP_HEADER http_age Age header. */

#define http_age_d msg_numeric_d
#define http_age_e msg_numeric_e
#define http_age_dup_xtra msg_default_dup_xtra
#define http_age_dup_one  msg_default_dup_one
msg_hclass_t http_age_class[] =
HTTP_HEADER_CLASS(age, "Age", x_common, single, age);

/* ====================================================================== */
/**@HTTP_HEADER http_allow Allow header. */

#define http_allow_d msg_list_d
#define http_allow_e msg_list_e
msg_hclass_t http_allow_class[] =
HTTP_HEADER_CLASS_LIST(allow, "Allow", list);

/* ====================================================================== */
/**@HTTP_HEADER http_authentication_info Authentication-Info header.
 * @sa RFC 2617
 */

#define http_authentication_info_d msg_list_d
#define http_authentication_info_e msg_list_e
#define http_authentication_info_dup_xtra msg_list_dup_xtra
#define http_authentication_info_dup_one msg_list_dup_one

msg_hclass_t http_authentication_info_class[] =
HTTP_HEADER_CLASS(authentication_info, "Authentication-Info",
		  ai_params, list, authentication_info);

/* ====================================================================== */
/**@HTTP_HEADER http_authorization Authorization header.
 *
 * We use MIME Authorization header.
 */

#define http_authorization_d msg_auth_d
#define http_authorization_e msg_auth_e

msg_hclass_t http_authorization_class[] =
HTTP_HEADER_CLASS_AUTH(authorization, "Authorization", single);

/* ====================================================================== */
/**@HTTP_HEADER http_cache_control Cache-Control header. */

#define http_cache_control_d msg_list_d
#define http_cache_control_e msg_list_e

msg_hclass_t http_cache_control_class[] =
  HTTP_HEADER_CLASS_LIST(cache_control, "Cache-Control", list);

/* ====================================================================== */
/**@HTTP_HEADER http_connection Connection header. */

#define http_connection_d msg_list_d
#define http_connection_e msg_list_e
msg_hclass_t http_connection_class[] =
HTTP_HEADER_CLASS_LIST(connection, "Connection", list_critical);

/* ====================================================================== */
/**@HTTP_HEADER http_content_encoding Content-Encoding header.
 *
 * We use MIME Content-Encoding header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_content_language Content-Language header.
 *
 * We use MIME Content-Language header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_content_length Content-Length header.
 *
 * We use MIME Content-Length header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_content_location Content-Location header.
 *
 * We use MIME Content-Location header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_content_md5 Content-MD5 header.
 *
 * We use MIME Content-MD5 header.
 */

/* ====================================================================== */
/**@HTTP_HEADER http_content_range Content-Range header.
 *
 * The Content-Range entity-header is sent with a partial entity-body to
 * specify where in the full entity-body the partial body should be
 * applied. Its syntax is defined in [H14.16] as follows:
 *
 * @code
 *     Content-Range = "Content-Range" ":" content-range-spec
 *     content-range-spec      = byte-content-range-spec
 *     byte-content-range-spec = bytes-unit SP
 *                               byte-range-resp-spec "/"
 *                               ( instance-length | "*" )
 *
 *     byte-range-resp-spec    = (first-byte-pos "-" last-byte-pos)
 *                                    | "*"
 *     instance-length         = 1*DIGIT
 * @endcode
 *
 */

/**@ingroup http_content_range
 * @typedef typedef struct http_content_range_s http_content_range_t;
 *
 * The structure #http_content_range_t contains representation of
 * @b Content-Range header.
 *
 * The #http_content_range_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t      cr_common[1];
 *   msg_error_t      *cr_next;
 *   off_t             cr_first;   // First-byte-pos
 *   off_t             cr_last;    // Last-byte-pos
 *   off_t             cr_length;  // Instance-length
 * } http_content_range_t;
 * @endcode
 */

issize_t http_content_range_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_content_range_t *cr = h->sh_content_range;

  if (!su_casenmatch(s, "bytes", 5))
    return -1;
  s += 5; skip_lws(&s);
  if (s[0] == '*') {
    cr->cr_first = cr->cr_last = (http_off_t)-1;
    s++; skip_lws(&s);
  } else {
    if (msg_delta_d((char const **)&s, &cr->cr_first) < 0)
      return -1;
    if (s[0] != '-')
      return -1;
    s++; skip_lws(&s);
    if (msg_delta_d((char const **)&s, &cr->cr_last) < 0)
      return -1;
  }

  if (s[0] != '/')
    return -1;
  s++; skip_lws(&s);

  if (s[0] == '*') {
    cr->cr_length = (http_off_t)-1;
    s++; skip_lws(&s);
  } else {
    if (msg_delta_d((char const **)&s, &cr->cr_length) < 0)
      return -1;
  }

  return s[0] ? -1 : 0;
}

issize_t http_content_range_e(char b[], isize_t bsiz, http_header_t const *h, int f)
{
  http_content_range_t const *cr = h->sh_content_range;

  if (cr->cr_first == (http_off_t)-1) {
    if (cr->cr_length == (http_off_t)-1)
      return snprintf(b, bsiz, "bytes */*");
    else
      return snprintf(b, bsiz, "bytes */%lu", cr->cr_length);
  }
  else {
    if (cr->cr_length == (http_off_t)-1)
      return snprintf(b, bsiz, "bytes %lu-%lu/*", cr->cr_first, cr->cr_last);
    else
      return snprintf(b, bsiz, "bytes %lu-%lu/%lu",
		      cr->cr_first, cr->cr_last, cr->cr_length);
  }
}

msg_hclass_t http_content_range_class[] =
HTTP_HEADER_CLASS(content_range, "Content-Range", cr_common, single, default);


/* ====================================================================== */

/**@HTTP_HEADER http_content_type Content-Type header.
 *
 * We use MIME Content-Type header.
 */

/* ====================================================================== */

/**@HTTP_HEADER http_date Date header.
 *
 * The Date header field reflects the time when the request or response was
 * first sent.  Its syntax is defined in [H14.18] as
 * follows:
 *
 * @code
 *    Date          =  "Date" HCOLON HTTP-date
 *    HTTP-date      =  rfc1123-date
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
 */

/**@ingroup http_date
 * @typedef typedef struct http_date_s http_date_t;
 *
 * The structure #http_date_t contains representation of @b Date header.
 *
 * The #http_date_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t    d_common[1];        // Common fragment info
 *   msg_error_t    *d_next;             // Link to next (dummy)
 *   http_time_t     d_time;             // Seconds since Jan 1, 1900
 * } http_date_t;
 * @endcode
 */

issize_t http_date_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_date_t *date = h->sh_date;

  if (msg_date_d((char const **)&s, &date->d_time) < 0 || *s)
    return -1;
  else
    return 0;
}


issize_t http_date_e(char b[], isize_t bsiz, http_header_t const *h, int f)
{
  http_date_t const *date = h->sh_date;

  return msg_date_e(b, bsiz, date->d_time);
}

/**@ingroup http_date
 * @brief Create an @b Date header object.
 *
 * The function http_date_create() creates a Date header object with the
 * date @a date. If @date is 0, current time (as returned by msg_now()) is
 * used.
 *
 * @param home   memory home
 * @param date   date expressed as seconds since Mon, 01 Jan 1900 00:00:00
 *
 * @return
 * The function http_date_create() returns a pointer to newly created
 * @b Date header object when successful, or NULL upon an error.
 */
http_date_t *http_date_create(su_home_t *home, http_time_t date)
{
  http_header_t *h = msg_header_alloc(home, http_date_class, 0);

  if (h) {
    if (date == 0)
      date = msg_now();
    h->sh_date->d_time = date;
  }

  return h->sh_date;
}


msg_hclass_t http_date_class[] =
HTTP_HEADER_CLASS(date, "Date", d_common, single, default);


/* ====================================================================== */

/**@HTTP_HEADER http_etag ETag header. */

#define http_etag_d msg_generic_d
#define http_etag_e msg_generic_e
msg_hclass_t http_etag_class[] =
HTTP_HEADER_CLASS_G(etag, "ETag", single);

/* ====================================================================== */

/**@HTTP_HEADER http_expect Expect header. */

#define http_expect_d msg_generic_d
#define http_expect_e msg_generic_e
msg_hclass_t http_expect_class[] =
HTTP_HEADER_CLASS_G(expect, "Expect", single);

/* ====================================================================== */

/**@HTTP_HEADER http_expires Expires header.
 *
 * The Expires header field gives the date and time after which the message
 * content expires. Its syntax is defined in RFC 1428 section 14.21 as
 * follows:
 *
 * @code
 *    Expires     =  "Expires:" HTTP-date
 * @endcode
 *
 */

/**@ingroup http_expires
 * @typedef typedef struct http_expires_s http_expires_t;
 *
 * The structure #http_expires_t contains representation of @b Expires
 * header.
 *
 * The #http_expires_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t    d_common[1];        // Common fragment info
 *   msg_error_t    *d_next;             // Link to next (dummy)
 *   http_time_t     d_time;             // Seconds since Jan 1, 1900
 * } http_expires_t;
 * @endcode
 */

#define http_expires_d http_date_d
#define http_expires_e http_date_e

msg_hclass_t http_expires_class[] =
HTTP_HEADER_CLASS(expires, "Expires", d_common, single, default);

/* ====================================================================== */
/**@HTTP_HEADER http_from From header.
 *
 * @code
 *    From   = "From" ":" mailbox
 * @endcode
 */


#define http_from_d msg_generic_d
#define http_from_e msg_generic_e
msg_hclass_t http_from_class[] =
HTTP_HEADER_CLASS_G(from, "From", single);

/* ====================================================================== */

/**@HTTP_HEADER http_host Host header.
 *
 * @code
 *    Host = "Host" ":" host [ ":" port ]
 * @endcode
 */

/** Parse Host header */
issize_t http_host_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_host_t *host = h->sh_host;

  if (msg_hostport_d(&s, &host->h_host, &host->h_port) < 0 || *s)
    return -1;

  return 0;
}

/** Print Host header */
issize_t http_host_e(char b[], isize_t bsiz, http_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;

  MSG_STRING_E(b, end, h->sh_host->h_host);
  if (h->sh_host->h_port) {
    MSG_CHAR_E(b, end, ':');
    MSG_STRING_E(b, end, h->sh_host->h_port);
  }

  return b - b0;
}

/** Extra size of a http_host_t object. */
static
isize_t http_host_dup_xtra(http_header_t const *h, isize_t offset)
{
  offset += MSG_STRING_SIZE(h->sh_host->h_host);
  offset += MSG_STRING_SIZE(h->sh_host->h_port);
  return offset;
}

/** Duplicate one Host header. */
static
char *http_host_dup_one(http_header_t *dst, http_header_t const *src,
			char *b, isize_t xtra)
{
  http_host_t *h = dst->sh_host;
  http_host_t const *o = src->sh_host;
  char *end = b + xtra;

  MSG_STRING_DUP(b, h->h_host, o->h_host);
  MSG_STRING_DUP(b, h->h_port, o->h_port);

  assert(b <= end); (void)end;

  return b;
}

/**Create a Host object. */
http_host_t *http_host_create(su_home_t *home,
			      char const *host,
			      char const *port)
{
  http_host_t h[1];

  http_host_init(h);

  h->h_host = host, h->h_port = port;

  if (host) {
    return http_host_dup(home, h);
  }
  else
    return NULL;
}

msg_hclass_t http_host_class[] =
HTTP_HEADER_CLASS(host, "Host", h_common, single, host);

/* ====================================================================== */
/**@HTTP_HEADER http_if_match If-Match header. */

#define http_if_match_d msg_list_d
#define http_if_match_e msg_list_e
msg_hclass_t http_if_match_class[] =
HTTP_HEADER_CLASS_LIST(if_match, "If-Match", list);

/* ====================================================================== */
/**@HTTP_HEADER http_if_modified_since If-Modified-Since header.
 *
 * The If-Modified-Since header field The If-Modified-Since request-header
 * field is used with a method to make it conditional: if the requested
 * variant has not been modified since the time specified in this field, an
 * entity will not be returned from the server; instead, a 304 (not
 * modified) response will be returned without any message-body. Its syntax
 * is defined in RFC 2616 secion 14.25 as follows:
 *
 * @code
 *    If-Modified-Since =  "If-Modified-Since" ":" HTTP-date
 * @endcode
 *
 */

/**@ingroup http_if_modified_since
 * @typedef typedef struct http_if_modified_since_s http_if_modified_since_t;
 *
 * The structure #http_if_modified_since_t contains representation of
 * @b If-Modified-Since header.
 *
 * The #http_if_modified_since_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t    d_common[1];        // Common fragment info
 *   msg_error_t    *d_next;             // Link to next (dummy)
 *   http_time_t     d_time;             // Seconds since Jan 1, 1900
 * } http_if_modified_since_t;
 * @endcode
 */

#define http_if_modified_since_d http_date_d
#define http_if_modified_since_e http_date_e

msg_hclass_t http_if_modified_since_class[] =
HTTP_HEADER_CLASS(if_modified_since, "If-Modified-Since",
		  d_common, single, default);

/* ====================================================================== */
/**@HTTP_HEADER http_if_none_match If-None-Match header. */

#define http_if_none_match_d msg_list_d
#define http_if_none_match_e msg_list_e
msg_hclass_t http_if_none_match_class[] =
HTTP_HEADER_CLASS_LIST(if_none_match, "If-None-Match", list);

/* ====================================================================== */
/**@HTTP_HEADER http_if_range If-Range header.
 *
 * The @b If-Range header is used when a client has a partial copy of an
 * entity in its cache, and wishes to have an up-to-date copy of the entire
 * entity. Informally, its meaning is `if the entity is unchanged, send
 * me the part(s) that I am missing; otherwise, send me the entire new
 * entity'. Its syntax is defined in RFC 2616 as follows:
 *
 * @code
 *   If-Range = "If-Range" ":" ( entity-tag / HTTP-date )
 * @endcode
 */

/** Parse If-Range header */
issize_t http_if_range_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_if_range_t *ifr = (http_if_range_t *)h;

  if (s[0] == '"' || su_casenmatch(s, "W/\"", 3)) {
    ifr->ifr_tag = s;
    return 0;
  } else {
    return msg_date_d((char const **)&s, &ifr->ifr_time);
  }
}

/** Print If-Range header */
issize_t http_if_range_e(char b[], isize_t bsiz, http_header_t const *h, int flags)
{
  http_if_range_t const *ifr = (http_if_range_t const *)h;
  char *b0 = b, *end = b + bsiz;

  if (ifr->ifr_tag) {
    MSG_STRING_E(b, end, ifr->ifr_tag);
    return b - b0;
  } else {
    return msg_date_e(b, bsiz, ifr->ifr_time);
  }
}

/** Extra size of a http_if_range_t object. */
static
isize_t http_if_range_dup_xtra(http_header_t const *h, isize_t offset)
{
  http_if_range_t const *ifr = (http_if_range_t const *)h;
  offset += MSG_STRING_SIZE(ifr->ifr_tag);
  return offset;
}

/** Duplicate one If-Range header. */
static
char *http_if_range_dup_one(http_header_t *dst, http_header_t const *src,
			    char *b, isize_t xtra)
{
  http_if_range_t *ifr = dst->sh_if_range;
  http_if_range_t const *o = src->sh_if_range;
  char *end = b + xtra;

  MSG_STRING_DUP(b, ifr->ifr_tag, o->ifr_tag);

  ifr->ifr_time = o->ifr_time;

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_if_range_class[] =
HTTP_HEADER_CLASS(if_range, "If-Range", ifr_common, single, if_range);


/* ====================================================================== */

/**@HTTP_HEADER http_if_unmodified_since If-Unmodified-Since header.
 *
 * The @b If-Unmodified-Since header is used with a method to make it
 * conditional. If the requested resource has not been modified since the
 * time specified in this field, the server SHOULD perform the requested
 * operation as if the If-Unmodified-Since header were not present. Its
 * syntax is defined in RFC 2616 14.28 as follows:
 *
 * @code
 *    If-Unmodified-Since     =  "If-Unmodified-Since:" HTTP-date
 * @endcode
 *
 */

/**@ingroup http_if_unmodified_since
 * @typedef typedef http_date_t http_if_unmodified_since_t;
 *
 * The structure #http_if_unmodified_since_t contains representation of
 * @b If-Unmodified-Since header.
 *
 * The #http_if_unmodified_since_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t    d_common[1];        // Common fragment info
 *   msg_error_t    *d_next;             // Link to next (dummy)
 *   http_time_t     d_time;             // Seconds since Jan 1, 1900
 * } http_if_unmodified_since_t;
 * @endcode
 */

#define http_if_unmodified_since_d http_date_d
#define http_if_unmodified_since_e http_date_e

msg_hclass_t http_if_unmodified_since_class[] =
HTTP_HEADER_CLASS(if_unmodified_since, "If-Unmodified-Since",
		  d_common, single, default);


/* ====================================================================== */
/**@HTTP_HEADER http_last_modified Last-Modified header.
 *
 * The Last-Modified header field gives the date and time after which the
 * message content last_modified. Its syntax is defined in [] as follows:
 *
 * @code
 *    Last-Modified     =  "Last-Modified:" HTTP-date
 * @endcode
 *
 */

/**@ingroup http_last_modified
 * @typedef typedef struct http_last_modified_s http_last_modified_t;
 *
 * The structure #http_last_modified_t contains representation of @b
 * Last-Modified header.
 *
 * The #http_last_modified_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t    d_common[1];        // Common fragment info
 *   msg_error_t    *d_next;             // Link to next (dummy)
 *   http_time_t     d_time;             // Seconds since Jan 1, 1900
 * } http_last_modified_t;
 * @endcode
 */

#define http_last_modified_d http_date_d
#define http_last_modified_e http_date_e

msg_hclass_t http_last_modified_class[] =
HTTP_HEADER_CLASS(last_modified, "Last-Modified", d_common, single, default);

/* ====================================================================== */
/**@HTTP_HEADER http_location Location Header
 *
 * The Location header is used to redirect the recipient to a location other
 * than the Request-URI for completion of the request or identification of a
 * new resource. Its syntax is defined in RFC 2616 section 14.30 as follows:
 *
 * @code
 *    Location       = "Location" ":" absoluteURI
 * @endcode
 *
 */

/**@ingroup http_location
 *
 * @typedef typedef struct http_location_s http_location_t;
 *
 * The structure http_location_t contains representation of @b Location
 * header.
 *
 * The http_location_t is defined as follows:
 * @code
 * typedef struct http_location_s
 * {
 *   msg_common_t         loc_common[1];
 *   msg_error_t         *loc_next;
 *   url_t                loc_url[1];
 * } http_location_t;
 * @endcode
 */

/** Decode (parse) a Location header */
issize_t http_location_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  http_location_t *loc = (http_location_t *)h;

  return url_d(loc->loc_url, s);
}

/** Encode (print) a Location header */
issize_t http_location_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  http_location_t const *loc = (http_location_t *)h;

  return url_e(b, bsiz, loc->loc_url);
}

/** Calculate extra storage used by Location header field */
isize_t http_location_dup_xtra(msg_header_t const *h, isize_t offset)
{
  http_location_t const *loc = (http_location_t *)h;

  offset += url_xtra(loc->loc_url);

  return offset;
}

/** Duplicate a Location header field */
char *http_location_dup_one(msg_header_t *dst, msg_header_t const *src,
			    char *b, isize_t xtra)
{
  http_location_t *loc = (http_location_t *)dst;
  http_location_t const *o = (http_location_t const *)src;
  char *end = b + xtra;

  URL_DUP(b, end, loc->loc_url, o->loc_url);

  assert(b <= end);

  return b;
}

msg_hclass_t http_location_class[] =
HTTP_HEADER_CLASS(location, "Location", loc_common, single, location);

/* ====================================================================== */
/**@HTTP_HEADER http_max_forwards Max-Forwards header. */

#define http_max_forwards_d msg_numeric_d
#define http_max_forwards_e msg_numeric_e
msg_hclass_t http_max_forwards_class[] =
HTTP_HEADER_CLASS(max_forwards, "Max-Forwards", mf_common, single, numeric);

/* ====================================================================== */
/**@HTTP_HEADER http_pragma Pragma header. */

#define http_pragma_d msg_list_d
#define http_pragma_e msg_list_e
msg_hclass_t http_pragma_class[] =
HTTP_HEADER_CLASS_LIST(pragma, "Pragma", list);

/* ====================================================================== */
/**@HTTP_HEADER http_proxy_authenticate Proxy-Authenticate header. */

#define http_proxy_authenticate_d msg_auth_d
#define http_proxy_authenticate_e msg_auth_e

msg_hclass_t http_proxy_authenticate_class[] =
HTTP_HEADER_CLASS_AUTH(proxy_authenticate, "Proxy-Authenticate", append);

/* ====================================================================== */
/**@HTTP_HEADER http_proxy_authorization Proxy-Authorization header. */

#define http_proxy_authorization_d msg_auth_d
#define http_proxy_authorization_e msg_auth_e

msg_hclass_t http_proxy_authorization_class[] =
HTTP_HEADER_CLASS_AUTH(proxy_authorization, "Proxy-Authorization", append);

/* ====================================================================== */

/**@HTTP_HEADER http_range Range header.
 *
 * The Range header is used to GET one or more sub-ranges of an entity
 * instead of the entire entity. Its syntax is defined in RFC 2616 section
 * 14.35 as follows:
 *
 * @code
 *    Range = "Range" ":" ranges-specifier
 *    ranges-specifier = byte-ranges-specifier
 *    byte-ranges-specifier = bytes-unit "=" byte-range-set
 *    byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
 *    byte-range-spec = first-byte-pos "-" [last-byte-pos]
 *    first-byte-pos  = 1*DIGIT
 *    last-byte-pos   = 1*DIGIT
 * @endcode
 *
 */

/**@ingroup http_range
 *
 * @typedef typedef struct http_range_s http_range_t;
 *
 * The structure http_range_t contains representation of @b Range header.
 *
 * The http_range_t is defined as follows:
 * @code
 * typedef struct http_range_s
 * {
 *   msg_common_t         rng_common[1];
 *   msg_error_t         *rng_next;
 *   char const          *rng_unit;
 *   char const  * const *rng_specs;
 * } http_range_t;
 * @endcode
 */

static issize_t range_spec_scan(char *start);

/** Decode (parse) a Range header */
issize_t http_range_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  http_range_t *rng = (http_range_t *)h;

  rng->rng_unit = s;
  skip_token(&s);
  if (s == rng->rng_unit)
    return -1;
  if (IS_LWS(*s)) {
    *s++ = '\0';
    skip_lws(&s);
  }
  if (*s != '=')
    return -1;
  *s++ = '\0';
  skip_lws(&s);

  /* XXX - use range-scanner */
  return msg_commalist_d(home, &s, &rng->rng_specs, range_spec_scan);
}

/** Scan and compact a range spec. */
static
issize_t range_spec_scan(char *start)
{
  size_t tlen;
  char *s, *p;

  s = p = start;

  if (s[0] == ',')
    return 0;

  /* Three forms: 1*DIGIT "-" 1*DIGIT | 1*DIGIT "-" | "-" 1*DIGIT */

  if (*s != '-') {
    tlen = span_digit(s);
    if (tlen == 0)
      return -1;
    p += tlen; s += tlen;
    skip_lws(&s);
  }

  if (*s != '-')
    return -1;

  if (p != s)
    *p = *s;
  p++, s++; skip_lws(&s);

  if (IS_DIGIT(*s)) {
    tlen = span_digit(s);
    if (tlen == 0)
      return -1;
    if (p != s)
      memmove(p, s, tlen);
    p += tlen; s += tlen;
    skip_lws(&s);
  }

  if (p != s)
    *p = '\0';

  return s - start;
}


/** Encode (print) a Range header */
issize_t http_range_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  http_range_t const *rng = (http_range_t *)h;
  char *b0 = b, *end = b + bsiz;

  MSG_STRING_E(b, end, rng->rng_unit);
  MSG_CHAR_E(b, end, '=');
  MSG_COMMALIST_E(b, end, rng->rng_specs, MSG_IS_COMPACT(flags));
  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra storage used by Range header field */
isize_t http_range_dup_xtra(msg_header_t const *h, isize_t offset)
{
  http_range_t const *rng = (http_range_t *)h;

  MSG_PARAMS_SIZE(offset, rng->rng_specs);
  offset += MSG_STRING_SIZE(rng->rng_unit);

  return offset;
}

/** Duplicate a Range header field */
char *http_range_dup_one(msg_header_t *dst, msg_header_t const *src,
			 char *b, isize_t xtra)
{
  http_range_t *rng = (http_range_t *)dst;
  http_range_t const *o = (http_range_t const *)src;
  char *end = b + xtra;

  b = msg_params_dup((msg_param_t const **)&rng->rng_specs,
		     o->rng_specs, b, xtra);
  MSG_STRING_DUP(b, rng->rng_unit, o->rng_unit);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_range_class[] =
HTTP_HEADER_CLASS(range, "Range", rng_specs, single, range);

/* ====================================================================== */

/**@HTTP_HEADER http_referer Referer header.
 *
 * The Referer header is used to redirect the recipient to a referer other
 * than the Request-URI for completion of the request or identification of a
 * new resource. Its syntax is defined in RFC 2616 section 14.30 as follows:
 *
 * @code
 *    Referer       = "Referer" ":" absoluteURI
 * @endcode
 *
 */

/**@ingroup http_referer
 *
 * @typedef typedef struct http_referer_s http_referer_t;
 *
 * The structure http_referer_t contains representation of @b Referer
 * header.
 *
 * The http_referer_t is defined as follows:
 * @code
 * typedef struct http_referer_s
 * {
 *   msg_common_t         loc_common[1];
 *   msg_error_t         *loc_next;
 *   url_t                loc_url[1];
 * } http_referer_t;
 * @endcode
 */

#define http_referer_d http_location_d
#define http_referer_e http_location_e

msg_hclass_t http_referer_class[] =
HTTP_HEADER_CLASS(referer, "Referer", loc_common, single, location);

/* ====================================================================== */

/**@HTTP_HEADER http_mime_version MIME-Version header.
 *
 * We use MIME MIME-Version header.
 */

/* ====================================================================== */

/**@HTTP_HEADER http_retry_after Retry-After header.
 *
 * The Retry-After response-header field can be used with a 503 (Service
 * Unavailable) response to indicate how long the service is expected to be
 * unavailable to the requesting client. This field MAY also be used with
 * any 3xx (Redirection) response to indicate the minimum time the
 * user-agent is asked wait before issuing the redirected request. Its
 * syntax is defined in RFC 2616 section 14.37 as follows:
 *
 * @code
 *    Retry-After   =  "Retry-After" ":" ( HTTP-date / delta-seconds )
 * @endcode
 *
 */

/**@ingroup http_retry_after
 * @typedef typedef struct http_retry_after_s http_retry_after_t;
 *
 * The structure #http_retry_after_t contains representation of @b
 * Retry-After header.
 *
 * The #http_retry_after_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t         ra_common[1]; // Common fragment info
 *   msg_error_t         *ra_next;      // Link to next (dummy)
 *   http_time_t          ra_date;      // When to retry
 *   http_time_t          ra_delta;     // Seconds to before retry
 * } http_retry_after_t;
 * @endcode
 */

issize_t http_retry_after_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_retry_after_t *ra = h->sh_retry_after;

  if (msg_date_delta_d((char const **)&s,
		       &ra->ra_date,
		       &ra->ra_delta) < 0 || *s)
    return -1;
  else
    return 0;
}

issize_t http_retry_after_e(char b[], isize_t bsiz, http_header_t const *h, int f)
{
  http_retry_after_t const *ra = h->sh_retry_after;

  if (ra->ra_date)
    return msg_date_e(b, bsiz, ra->ra_date + ra->ra_delta);
  else
    return msg_delta_e(b, bsiz, ra->ra_delta);
}

msg_hclass_t http_retry_after_class[] =
HTTP_HEADER_CLASS(retry_after, "Retry-After", ra_common, single, default);

/* ====================================================================== */
/**@HTTP_HEADER http_server Server header. */

#define http_server_d msg_generic_d
#define http_server_e msg_generic_e
msg_hclass_t http_server_class[] =
HTTP_HEADER_CLASS_G(server, "Server", single);

/* ====================================================================== */

/**@HTTP_HEADER http_te TE header.
 *
 * The TE request-header field indicates what extension transfer-codings it
 * is willing to accept in the response and whether or not it is willing to
 * accept trailer fields in a chunked transfer-coding. Its value may consist
 * of the keyword "trailers" and/or a comma-separated list of extension
 * transfer-coding names with optional accept parameters. Its syntax is
 * defined in [H14.39] as follows:
 *
 * @code
 *     TE        = "TE" ":" #( t-codings )
 *     t-codings = "trailers" | ( transfer-extension [ accept-params ] )
 * @endcode
 *
 */

/**@ingroup http_te
 * @typedef typedef strucy http_te_s http_te_t;
 *
 * The structure http_te_t contains representation of @b TE header.
 *
 * The http_te_t is defined as follows:
 * @code
 * typedef struct http_te_s {
 * } http_te_t;
 * @endcode
 */

su_inline
void http_te_update(http_te_t *te)
{
  te->te_q = msg_header_find_param(te->te_common, "q");
}

issize_t http_te_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_header_t **hh = &h->sh_succ, *h0 = h;
  http_te_t *te = (http_te_t *)h;

  assert(h); assert(sizeof(*h));

  for (;*s;) {
    /* Ignore empty entries (comma-whitespace) */
    if (*s == ',') { *s++ = '\0'; skip_lws(&s); continue; }

    if (!h) {      /* Allocate next header structure */
      if (!(h = msg_header_alloc(home, h0->sh_class, 0)))
	break;
      *hh = h; h->sh_prev = hh; hh = &h->sh_succ;
      te = te->te_next = (http_te_t *)h;
    }

    /* "TE:" #(transfer-extension ; *(parameters))) */
    if (msg_token_d(&s, &te->te_extension) == -1)
      return -1;

    if (*s == ';' && msg_params_d(home, &s, &te->te_params) == -1)
      return -1;

    if (*s != '\0' && *s != ',')
      return -1;

    if (te->te_params)
      http_te_update(te);

    h = NULL;
  }

  return 0;
}

issize_t http_te_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  http_te_t const *te = (http_te_t *)h;

  assert(http_is_te(h));

  MSG_STRING_E(b, end, te->te_extension);
  MSG_PARAMS_E(b, end, te->te_params, flags);

  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t http_te_dup_xtra(msg_header_t const *h, isize_t offset)
{
  http_te_t const *te = (http_te_t const *)h;

  MSG_PARAMS_SIZE(offset, te->te_params);
  offset += MSG_STRING_SIZE(te->te_extension);

  return offset;
}

/** Duplicate one http_te_t object */
char *http_te_dup_one(msg_header_t *dst, msg_header_t const *src,
		      char *b, isize_t xtra)
{
  http_te_t *te = (http_te_t *)dst;
  http_te_t const *o = (http_te_t const *)src;
  char *end = b + xtra;

  b = msg_params_dup(&te->te_params, o->te_params, b, xtra);
  MSG_STRING_DUP(b, te->te_extension, o->te_extension);
  if (te->te_params) http_te_update(te);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_te_class[] =
HTTP_HEADER_CLASS(te, "TE", te_params, append, te);

/* ====================================================================== */
/**@HTTP_HEADER http_trailer Trailer header. */

#define http_trailer_d msg_list_d
#define http_trailer_e msg_list_e
msg_hclass_t http_trailer_class[] =
HTTP_HEADER_CLASS_LIST(trailer, "Trailer", list_critical);


/* ====================================================================== */
/**@HTTP_HEADER http_transfer_encoding Transfer-Encoding header. */

#define http_transfer_encoding_d msg_list_d
#define http_transfer_encoding_e msg_list_e
msg_hclass_t http_transfer_encoding_class[] =
HTTP_HEADER_CLASS_LIST(transfer_encoding, "Transfer-Encoding", list_critical);

/* ====================================================================== */
/**@HTTP_HEADER http_upgrade Upgrade header. */

#define http_upgrade_d msg_list_d
#define http_upgrade_e msg_list_e
msg_hclass_t http_upgrade_class[] =
HTTP_HEADER_CLASS_LIST(upgrade, "Upgrade", list_critical);

/* ====================================================================== */
/**@HTTP_HEADER http_user_agent User-Agent header. */

#define http_user_agent_d msg_generic_d
#define http_user_agent_e msg_generic_e
msg_hclass_t http_user_agent_class[] =
HTTP_HEADER_CLASS_G(user_agent, "User-Agent", single);

/* ====================================================================== */
/**@HTTP_HEADER http_vary Vary header. */

#define http_vary_d msg_list_d
#define http_vary_e msg_list_e
msg_hclass_t http_vary_class[] =
HTTP_HEADER_CLASS_LIST(vary, "Vary", list);

/* ====================================================================== */
/**@HTTP_HEADER http_via Via header.
 *
 * @code
 *    Via =  "Via" ":" 1#( received-protocol received-by [ comment ] )
 *    received-protocol = [ protocol-name "/" ] protocol-version
 *    protocol-name     = token
 *    protocol-version  = token
 *    received-by       = ( host [ ":" port ] ) | pseudonym
 *    pseudonym         = token
 * @endcode
 */

issize_t http_via_d(su_home_t *home, http_header_t *h, char *s, isize_t slen)
{
  http_header_t **hh = &h->sh_succ, *h0 = h;
  http_via_t *v = h->sh_via;

  assert(h && h->sh_class);

  for (;*s;) {
    /* Ignore empty entries (comma-whitespace) */
    if (*s == ',') { *s++ = '\0'; skip_lws(&s); continue; }

    if (!h) {      /* Allocate next header structure */
      if (!(h = msg_header_alloc(home, h0->sh_class, 0)))
	return -1;
      *hh = h; h->sh_prev = hh; hh = &h->sh_succ;
      v = v->v_next = h->sh_via;
    }

    if (http_version_d(&s, &v->v_version) == -1) /* Parse protocol version */
      return -1;
    if (msg_hostport_d(&s, &v->v_host, &v->v_port) == -1) /* Host (and port) */
      return -1;
    if (*s == '(' && msg_comment_d(&s, &v->v_comment) == -1) /* Comment */
      return -1;
    if (*s != '\0' && *s != ',') /* Extra before next header field? */
      return -1;

    h = NULL;
  }

  if (h)		/* List without valid header via */
    return -1;

  return 0;
}

issize_t http_via_e(char b[], isize_t bsiz, http_header_t const *h, int flags)
{
  int const compact = MSG_IS_COMPACT(flags);
  char *b0 = b, *end = b + bsiz;
  http_via_t const *v = h->sh_via;

  MSG_STRING_E(b, end, v->v_version);
  MSG_CHAR_E(b, end, ' ');
  MSG_STRING_E(b, end, v->v_host);
  if (v->v_port) {
    MSG_CHAR_E(b, end, ':');
    MSG_STRING_E(b, end, v->v_port);
  }
  if (v->v_comment) {
    if (!compact) MSG_CHAR_E(b, end, ' ');
    MSG_CHAR_E(b, end, '(');
    MSG_STRING_E(b, end, v->v_comment);
    MSG_CHAR_E(b, end, ')');
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

static isize_t http_via_dup_xtra(http_header_t const *h, isize_t offset)
{
  http_via_t const *v = h->sh_via;

  offset += MSG_STRING_SIZE(v->v_version);
  offset += MSG_STRING_SIZE(v->v_host);
  offset += MSG_STRING_SIZE(v->v_port);
  offset += MSG_STRING_SIZE(v->v_comment);

  return offset;
}

/** Duplicate one http_via_t object */
static char *http_via_dup_one(http_header_t *dst, http_header_t const *src,
			      char *b, isize_t xtra)
{
  http_via_t *v = dst->sh_via;
  http_via_t const *o = src->sh_via;
  char *end = b + xtra;

  MSG_STRING_DUP(b, v->v_version, o->v_version);
  MSG_STRING_DUP(b, v->v_host, o->v_host);
  MSG_STRING_DUP(b, v->v_port, o->v_port);
  MSG_STRING_DUP(b, v->v_comment, o->v_comment);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_via_class[] =
HTTP_HEADER_CLASS(via, "Via", v_common, prepend, via);

/* ====================================================================== */
/**@HTTP_HEADER http_warning Warning header. */

#define http_warning_d msg_warning_d
#define http_warning_e msg_warning_e
#define http_warning_dup_xtra msg_warning_dup_xtra
#define http_warning_dup_one msg_warning_dup_one

msg_hclass_t http_warning_class[] =
  HTTP_HEADER_CLASS(warning, "Warning", w_common, append, warning);

/* ====================================================================== */
/**@HTTP_HEADER http_www_authenticate WWW-Authenticate header. */

#define http_www_authenticate_d msg_auth_d
#define http_www_authenticate_e msg_auth_e

msg_hclass_t http_www_authenticate_class[] =
HTTP_HEADER_CLASS_AUTH(www_authenticate, "WWW-Authenticate", single);

/* ====================================================================== */

/**@HTTP_HEADER http_error Erroneous headers.
 *
 * We use erroneous header object from @b msg module.
 */

/* ====================================================================== */

/**@HTTP_HEADER http_unknown Unknown headers.
 *
 * We use unknown header object from @b msg module.
 */
/* ====================================================================== */

/**@HTTP_HEADER http_separator Header separator.
 *
 * We use header separator object from @b msg module.
 */
/* ====================================================================== */

/**@HTTP_HEADER http_payload Message payload.
 *
 * We use message body object from @b msg module.
 */

