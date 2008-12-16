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

/**@CFILE http_header.c
 *
 * HTTP header handling.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

#include <assert.h>

#include <sofia-sip/su_alloc.h>

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u
#define HTTP_STATIC_INLINE

#include "sofia-sip/http_parser.h"

#include <sofia-sip/http_header.h>
#include <sofia-sip/http_status.h>

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffU)
#endif

/** Complete a HTTP request. */
int http_request_complete(msg_t *msg)
{
  size_t len = 0;
  http_t *http = http_object(msg);
  http_payload_t const *pl;
  su_home_t *home = msg_home(msg);

  if (!http)
    return -1;
  if (!http->http_request)
    return -1;
  if (!http->http_host)
    return -1;

    for (pl = http->http_payload; pl; pl = pl->pl_next)
      len += pl->pl_len;

  if (len > UINT32_MAX)
    return -1;

  if (!http->http_content_length) {
    http->http_content_length = http_content_length_create(home, (uint32_t)len);
  }
  else {
    if (http->http_content_length->l_length != len) {
      http->http_content_length->l_length = (uint32_t)len;
      msg_fragment_clear(http->http_content_length->l_common);
    }
  }

  if (!http->http_separator)
    http->http_separator = http_separator_create(home);

  return 0;
}

/** Remove schema, host, port, and fragment from HTTP/HTTPS URL */
int http_strip_hostport(url_t *url)
{
  if (url->url_type == url_http || url->url_type == url_https) {
    url->url_type = url_unknown;
    url->url_scheme = NULL;
    url->url_user = NULL;
    url->url_password = NULL;
    url->url_host = NULL;
    url->url_port = NULL;
    if (url->url_path == NULL) {
      url->url_root = '/';
      url->url_path = "";
    }
  }

  url->url_fragment = NULL;

  return 0;
}

/** Add a Content-Length and separator to a message */
int http_message_complete(msg_t *msg, http_t *http)
{
#if 1
  if (!http->http_content_length) {
    http_content_length_t *l;
    http_payload_t *pl;
    size_t len = 0;

    for (pl = http->http_payload; pl; pl = pl->pl_next)
      len += pl->pl_len;

    if (len > UINT32_MAX)
      return -1;

    l = http_content_length_create(msg_home(msg), (uint32_t)len);

    if (msg_header_insert(msg, http, (http_header_t *)l) < 0)
      return -1;
  }
#endif

  if (!http->http_separator) {
    http_separator_t *sep = http_separator_create(msg_home(msg));
    if (msg_header_insert(msg, http, (http_header_t *)sep) < 0)
      return -1;
  }

  return 0;
}

/** Add headers from the request to the response message. */
int http_complete_response(msg_t *msg,
			   int status, char const *phrase,
			   http_t const *request)
{
  su_home_t *home = msg_home(msg);
  http_t *http = msg_object(msg);

  if (!http || !request || !request->http_request)
    return -1;

  if (!http->http_status)
    http->http_status = http_status_create(home, status, phrase, NULL);

  if (!http->http_status)
    return -1;

  if (!http->http_separator) {
    http_separator_t *sep = http_separator_create(msg_home(msg));
    if (msg_header_insert(msg, http, (http_header_t *)sep) < 0)
      return -1;
  }

  return 0;
}

/** Copy a HTTP header. */
http_header_t *http_header_copy(su_home_t *home, http_header_t const *h)
{
  if (h == NULL || h == HTTP_NONE)
    return NULL;
  return msg_header_copy_as(home, h->sh_class, h);
}

/** Duplicate a HTTP header. */
http_header_t *http_header_dup(su_home_t *home, http_header_t const *h)
{
  if (h == NULL || h == HTTP_NONE)
    return NULL;
  return msg_header_dup_as(home, h->sh_class, h);

}

/** Decode a HTTP header. */
http_header_t *http_header_d(su_home_t *home, msg_t const *msg, char const *b)
{
  return msg_header_d(home, msg, b);
}

/** Encode a HTTP header. */
int http_header_e(char b[], int bsiz, http_header_t const *h, int flags)
{
  return msg_header_e(b, bsiz, h, flags);
}

/** Encode HTTP header contents. */
int http_header_field_e(char b[], int bsiz, http_header_t const *h, int flags)
{
  assert(h); assert(h->sh_class);

  return h->sh_class->hc_print(b, bsiz, h, flags);
}

http_header_t *http_header_format(su_home_t *home,
				  msg_hclass_t *hc,
				  char const *fmt,
				  ...)
{
  http_header_t *h;
  va_list ap;

  va_start(ap, fmt);

  h = http_header_vformat(home, hc, fmt, ap);

  va_end(ap);

  return h;
}

/** Add a duplicate of header object to a HTTP message. */
int http_add_dup(msg_t *msg,
		 http_t *http,
		 http_header_t const *o)
{
  if (o == HTTP_NONE)
    return 0;

  if (msg == NULL || o == NULL)
    return -1;

  return msg_header_insert(msg, http, msg_header_dup(msg_home(msg), o));
}

int http_add_make(msg_t *msg,
		 http_t *http,
		 msg_hclass_t *hc,
		 char const *s)
{
  if (s == NULL)
    return 0;

  if (msg == NULL)
    return -1;

  return msg_header_insert(msg, http, msg_header_make(msg_home(msg), hc, s));
}

int http_add_format(msg_t *msg,
		    http_t *http,
		    msg_hclass_t *hc,
		    char const *fmt,
		    ...)
{
  http_header_t *h;
  va_list ap;

  if (fmt == NULL)
    return 0;

  if (msg == NULL)
    return -1;

  va_start(ap, fmt);
  h = http_header_vformat(msg_home(msg), hc, fmt, ap);
  va_end(ap);

  return msg_header_insert(msg, http, h);
}

/** Compare two HTTP URLs. */
int http_url_cmp(url_t const *a, url_t const *b)
{
  int rv;

  if ((rv = url_cmp(a, b)))
    return rv;

  if (a->url_path != b->url_path) {
    if (a->url_path == NULL) return -1;
    if (b->url_path == NULL) return +1;
    if ((rv = strcmp(a->url_path, b->url_path)))
      return rv;
  }

  /* Params? */

  /* Query */
  if (a->url_headers != b->url_headers) {
    if (a->url_headers == NULL) return -1;
    if (b->url_headers == NULL) return +1;
    if ((rv = strcmp(a->url_headers, b->url_headers)))
      return rv;
  }

  return 0;
}

