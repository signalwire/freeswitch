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

/**@CFILE http_extra.c
 *
 * Extra HTTP headers
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

#include <sofia-sip/su_string.h>

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u

#include "sofia-sip/http_parser.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

/* ========================================================================== */

/**@HTTP_HEADER http_proxy_connection Proxy-Connection extension header. */

#define http_proxy_connection_d msg_list_d
#define http_proxy_connection_e msg_list_e
msg_hclass_t http_proxy_connection_class[] =
HTTP_HEADER_CLASS_LIST(proxy_connection, "Proxy-Connection", list);

/* ====================================================================== */
/**@HTTP_HEADER http_cookie Cookie extension header.
 *
 * The Cookie header is used to transmit state information from server
 * back to the http client. Its syntax is defined in RFC 2109 section 4.3.4
 * as follows:
 *
 * @code
 *   cookie         = "Cookie:" cookie-version
 *                    1*((";" | ",") cookie-value)
 *   cookie-value   = NAME "=" VALUE [";" path] [";" domain]
 *   cookie-version = "$Version" "=" value
 *   NAME           = attr
 *   VALUE          = value
 *   path           = "$Path" "=" value
 *   domain         = "$Domain" "=" value
 * @endcode
 *
 */

/**@ingroup http_cookie
 *
 * @typedef typedef struct http_cookie_s http_cookie_t;
 *
 * The structure http_cookie_t contains representation of @b Cookie
 * header. Please note that a single http_cookie_t can contain many
 * cookies.
 *
 * The http_cookie_t is defined as follows:
 * @code
 * typedef struct http_cookie_s
 * {
 * } http_cookie_t;
 * @endcode
 */

/**Update Cookie parameters.
 *
 * The function http_cookie_update() updates a @b Cookie parameter
 * shortcuts.
 *
 * @param sc pointer to a @c http_cookie_t object
 */
su_inline
void http_cookie_update(http_cookie_t *c)
{
  size_t i;

  c->c_name = NULL;
  c->c_version = NULL, c->c_domain = NULL, c->c_path = NULL;

  if (!c->c_params)
    return;

  if (!(MSG_PARAM_MATCH(c->c_version, c->c_params[0], "$Version")))
    return;
  if (!c->c_params[1] || c->c_params[1][0] == '$')
    return;

  c->c_name = c->c_params[1];

  for (i = 2; ; i++) {
    msg_param_t p = c->c_params[i];
    if (!p || *p++ != '$')
      break;
    switch (p[0]) {
    case 'd': case 'D':
      MSG_PARAM_MATCH(c->c_domain, p, "Domain");
      break;
    case 'p': case 'P':
      MSG_PARAM_MATCH(c->c_path, p, "Path");
      break;
    }
  }
}

/* Scan a cookie parameter */
static issize_t cookie_scanner(char *s)
{
  char *p = s;
  size_t tlen;

  skip_token(&s);

  if (s == p)		/* invalid parameter name */
    return -1;

  tlen = s - p;

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

  if (*s == '=') {
    char *v;
    s++;
    skip_lws(&s);

    v = s;

    /* get value */
    if (*s == '"') {
      size_t qlen = span_quoted(s);
      if (!qlen)
	return -1;
      s += qlen;
    }
    else {
      s += strcspn(s, ",;" LWS);
      if (s == v)
	return -1;
    }

    if (p + tlen + 1 != v) {
      memmove(p + tlen + 1, v, s - v);
      p[tlen] = '=';
      p[tlen + 1 + (s - v)] = '\0';
    }
  }

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

  return s - p;
}

/** Decode (parse) a Cookie header */
issize_t http_cookie_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  http_cookie_t *c = (http_cookie_t *)h;

  assert(h); assert(sizeof(*h));

  for (;*s;) {
    /* Ignore empty entries (comma-whitespace) */
    if (*s == ',') { *s++ = '\0'; skip_lws(&s); continue; }

    if (msg_any_list_d(home, &s, (msg_param_t **)&c->c_params,
		       cookie_scanner, ';') == -1)
      return -1;

    if (*s != '\0' && *s != ',')
      return -1;

    if (!c->c_params)
      return -1;
  }

  http_cookie_update(c);

  return 0;
}

/** Encode (print) a Cookie header */
issize_t http_cookie_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  http_cookie_t const *c = (http_cookie_t *)h;
  size_t i;

  if (c->c_params) {
    for (i = 0; c->c_params[i]; i++) {
      if (i > 0) MSG_CHAR_E(b, end, ';');
      MSG_STRING_E(b, end, c->c_params[i]);
    }
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra storage used by Cookie header field */
isize_t http_cookie_dup_xtra(msg_header_t const *h, isize_t offset)
{
  http_cookie_t const *c = (http_cookie_t *)h;

  MSG_PARAMS_SIZE(offset, c->c_params);

  return offset;
}

/** Duplicate a Cookie header field */
char *http_cookie_dup_one(msg_header_t *dst, msg_header_t const *src,
			  char *b, isize_t xtra)
{
  http_cookie_t *c = (http_cookie_t *)dst;
  http_cookie_t const *o = (http_cookie_t const *)src;
  char *end = b + xtra;

  b = msg_params_dup(&c->c_params, o->c_params, b, xtra);
  http_cookie_update(c);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_cookie_class[] =
HTTP_HEADER_CLASS(cookie, "Cookie", c_params, append, cookie);

/* ====================================================================== */

/**@HTTP_HEADER http_set_cookie Set-Cookie extension header.
 *
 * The Set-Cookie header is used to transmit state information from server
 * back to the http client. Its syntax is defined in RFC 2109 section 4.2.2
 * as follows:
 *
 * @code
 * set-cookie      =       "Set-Cookie:" cookies
 * cookies         =       1#cookie
 * cookie          =       NAME "=" VALUE *(";" cookie-av)
 * NAME            =       attr
 * VALUE           =       value
 * cookie-av       =       "Comment" "=" value
 *                 |       "Domain" "=" value
 *                 |       "Max-Age" "=" value
 *                 |       "Path" "=" value
 *                 |       "Secure"
 *                 |       "Version" "=" 1*DIGIT
 *
 * @endcode
 *
 */

/**@ingroup http_set_cookie
 *
 * @typedef typedef struct http_set_cookie_s http_set_cookie_t;
 *
 * The structure http_set_cookie_t contains representation of @b Set-Cookie
 * header.
 *
 * The http_set_cookie_t is defined as follows:
 * @code
 * typedef struct http_set_cookie_s
 * {
 * } http_set_cookie_t;
 * @endcode
 */

/**Update Set-Cookie parameters.
 *
 * The function http_set_cookie_update() updates a @b Set-Cookie parameter
 * shortcuts.
 *
 * @param sc pointer to a @c http_set_cookie_t object
 */
su_inline
void http_set_cookie_update(http_set_cookie_t *sc)
{
  size_t i;

  sc->sc_name = NULL;
  sc->sc_version = NULL, sc->sc_domain = NULL, sc->sc_path = NULL;
  sc->sc_comment = NULL, sc->sc_max_age = NULL, sc->sc_secure = 0;

  if (!sc->sc_params)
    return;

  sc->sc_name = sc->sc_params[0];

  for (i = 1; sc->sc_params[i]; i++) {
    msg_param_t p = sc->sc_params[i];
    switch (p[0]) {
    case 'c': case 'C':
      MSG_PARAM_MATCH(sc->sc_comment, p, "Comment");
      break;
    case 'd': case 'D':
      MSG_PARAM_MATCH(sc->sc_domain, p, "Domain");
      break;
    case 'm': case 'M':
      MSG_PARAM_MATCH(sc->sc_max_age, p, "Max-Age");
      break;
    case 'p': case 'P':
      MSG_PARAM_MATCH(sc->sc_path, p, "Path");
      break;
    case 's': case 'S':
      MSG_PARAM_MATCH_P(sc->sc_secure, p, "Secure");
      break;
    case 'v': case 'V':
      MSG_PARAM_MATCH(sc->sc_version, p, "Version");
      break;
    }
  }

}

#include <sofia-sip/msg_date.h>

/* Scan a cookie parameter */
static issize_t set_cookie_scanner(char *s)
{
  char *rest;

#define LOOKING_AT(s, what) \
  (su_casenmatch((s), what, strlen(what)) && (rest = s + strlen(what)))

  /* Special cases from Netscape spec */
  if (LOOKING_AT(s, "expires=")) {
    msg_time_t value;
    msg_date_d((char const **)&rest, &value);
  } else if (LOOKING_AT(s, "path=/")) {
    for (;;) {
      rest += span_unreserved(rest);
      if (*rest != '/')
	break;
      rest++;
    }
  } else {
    return msg_attribute_value_scanner(s);
  }
#undef LOOKING_AT

  if (IS_LWS(*rest)) {
    *rest++ = '\0'; skip_lws(&rest);
  }

  return rest - s;
}

/** Decode (parse) Set-Cookie header */
issize_t http_set_cookie_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_header_t **hh = &h->sh_succ, *h0 = h;
  http_set_cookie_t *sc = (http_set_cookie_t *)h;
  msg_param_t *params;

  assert(h); assert(sizeof(*h));

  for (;*s;) {
    /* Ignore empty entries (comma-whitespace) */
    if (*s == ',') { *s++ = '\0'; skip_lws(&s); continue; }

    if (!h) {      /* Allocate next header structure */
      if (!(h = msg_header_alloc(home, h0->sh_class, 0)))
	return -1;
      *hh = h; h->sh_prev = hh; hh = &h->sh_succ;
      sc = sc->sc_next = (http_set_cookie_t *)h;
    }

    /* "Set-Cookie:" 1#(NAME "=" VALUE *(";" cookie-av))) */
    params = su_zalloc(home, MSG_PARAMS_NUM(1) * sizeof(msg_param_t));
    if (!params)
      return -1;

    params[0] = s, sc->sc_params = params;
    s += strcspn(s, ",;" LWS);

    if (*s) {
      *s++ = '\0';
      skip_lws(&s);
      if (*s && msg_any_list_d(home, &s, (msg_param_t **)&sc->sc_params,
			       set_cookie_scanner, ';') == -1)
	return -1;
    }

    if (*s != '\0' && *s != ',')
      return -1;

    if (sc->sc_params)
      http_set_cookie_update(sc);

    h = NULL;
  }

  return 0;
}

/** Encode (print) Set-Cookie header */
issize_t http_set_cookie_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  http_set_cookie_t const *sc = (http_set_cookie_t *)h;
  size_t i;

  if (sc->sc_params) {
    for (i = 0; sc->sc_params[i]; i++) {
      if (i > 0) MSG_CHAR_E(b, end, ';');
      MSG_STRING_E(b, end, sc->sc_params[i]);
    }
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra storage used by Set-Cookie header field */
isize_t http_set_cookie_dup_xtra(msg_header_t const *h, isize_t offset)
{
  http_set_cookie_t const *sc = (http_set_cookie_t *)h;

  MSG_PARAMS_SIZE(offset, sc->sc_params);

  return offset;
}

/** Duplicate a Set-Cookie header field */
char *http_set_cookie_dup_one(msg_header_t *dst, msg_header_t const *src,
			      char *b, isize_t xtra)
{
  http_set_cookie_t *sc = (http_set_cookie_t *)dst;
  http_set_cookie_t const *o = (http_set_cookie_t const *)src;
  char *end = b + xtra;

  b = msg_params_dup(&sc->sc_params, o->sc_params, b, xtra);
  http_set_cookie_update(sc);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t http_set_cookie_class[] =
HTTP_HEADER_CLASS(set_cookie, "Set-Cookie", sc_params, append, set_cookie);
