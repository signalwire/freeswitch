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

/**@CFILE http_parser.c
 *
 * HTTP parser.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Oct  5 14:01:24 2000 ppessi
 */

#include "config.h"

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>
#include "sofia-sip/http_parser.h"
#include <sofia-sip/msg_parser.h>
#include <sofia-sip/http_header.h>
#include <sofia-sip/http_status.h>
#include <sofia-sip/msg_mclass.h>

#include <sofia-sip/su_tagarg.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>

/** HTTP version 1.1. */
char const http_version_1_1[] = "HTTP/1.1";
/** HTTP version 1.0. */
char const http_version_1_0[] = "HTTP/1.0";
/** HTTP version 0.9 is an empty string. */
char const http_version_0_9[] = "";

msg_mclass_t const *http_default_mclass(void)
{
  extern msg_mclass_t const http_mclass[];

  return http_mclass;
}

static
issize_t http_extract_chunk(msg_t *, http_t *, char b[], isize_t bsiz, int eos);

/** Calculate length of line ending (0, 1 or 2) */
#define CRLF_TEST(s) \
  (((s)[0]) == '\r' ? (((s)[1]) == '\n') + 1 : ((s)[0])=='\n')

/** Extract the HTTP message body, including separator line.
 *
 * @retval -1    error
 * @retval 0     cannot proceed
 * @retval other number of bytes extracted
 */
issize_t http_extract_body(msg_t *msg, http_t *http, char b[], isize_t bsiz, int eos)
{
  issize_t m = 0;
  size_t body_len;

  int flags = http->http_flags;

  if (eos && bsiz == 0) {
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);
    return 0;
  }

  if (flags & MSG_FLG_TRAILERS) {
    /* The empty line after trailers */
    if (!eos && (bsiz == 0 || (bsiz == 1 && b[0] == '\r')))
      return 0;

    m = CRLF_TEST(b);

    assert(m > 0 || eos); /* We should be looking at an empty line */

    /* We have completed trailers */
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);

    return m;
  }

  if (flags & MSG_FLG_CHUNKS)
    return http_extract_chunk(msg, http, b, bsiz, eos);

  if (!(flags & MSG_FLG_BODY)) {
    /* We are looking at a potential empty line */
    m = msg_extract_separator(msg, http, b, bsiz, eos);

    if (m == 0)			/* Not yet */
      return 0;

    http->http_flags |= MSG_FLG_BODY;
    b += m, bsiz -= m;
  }

  /* body_len is determined by rules in RFC2616 sections 4.3 and 4.4 */

  /* 1XX, 204, 304 do not have message-body, ever */
  if (http->http_status) {
    int status = http->http_status->st_status;

    if (status < 200 || status == 204 || status == 304)
      flags |= HTTP_FLG_NO_BODY;
  }

  if (flags & HTTP_FLG_NO_BODY) {
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);
    return m;
  }

  if (http->http_transfer_encoding) {
    if (/* NOTE - there is really no Transfer-Encoding: identity in RFC 2616
	 * but it was used in drafts...
	 */
	http->http_transfer_encoding->k_items &&
	http->http_transfer_encoding->k_items[0] &&
	!su_casematch(http->http_transfer_encoding->k_items[0], "identity")) {
      http->http_flags |= MSG_FLG_CHUNKS;

      if (http->http_flags & MSG_FLG_STREAMING)
	msg_set_streaming(msg, msg_start_streaming);

      if (m)
	return m;

      return http_extract_chunk(msg, http, b, bsiz, eos);
    }
  }


  if (http->http_content_length)
    body_len = http->http_content_length->l_length;
  /* We cannot parse multipart/byteranges ... */
  else if (http->http_content_type && http->http_content_type->c_type &&
	   su_casematch(http->http_content_type->c_type, "multipart/byteranges"))
    return -1;
  else if (MSG_IS_MAILBOX(flags)) /* message fragments */
    body_len = 0;
  else if (http->http_request)
    body_len = 0;
  else if (eos)
    body_len = bsiz;
  else
    return 0;			/* XXX */

  if (body_len == 0) {
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);
    return m;
  }

  if (http->http_flags & MSG_FLG_STREAMING)
    msg_set_streaming(msg, msg_start_streaming);

  if (m)
    return m;

  m = msg_extract_payload(msg, http, NULL, body_len, b, bsiz, eos);
  if (m == -1)
    return -1;

  /* We have now all message fragments in place */
  http->http_flags |= MSG_FLG_FRAGS;
  if (bsiz >= body_len) {
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);
  }

  return m;
}

/** Extract a chunk.
 *
 * @retval -1    error
 * @retval 0     cannot proceed
 * @retval other number of bytes extracted
 */
issize_t http_extract_chunk(msg_t *msg, http_t *http, char b[], isize_t bsiz, int eos)
{
  size_t n;
  unsigned crlf, chunk_len;
  char *b0 = b, *s;
  union {
    msg_header_t *header;
    msg_payload_t *chunk;
  } h = { NULL };
  size_t bsiz0 = bsiz;

  if (bsiz == 0)
    return 0;

  /* We should be looking at an empty line followed by the chunk header */
  while ((crlf = CRLF_TEST(b))) {
    if (bsiz == 1 && crlf == 1 && b[0] == '\r' && !eos)
      return 0;

    if (crlf == bsiz) {
      if (eos) {
	msg_mark_as_complete(msg, MSG_FLG_COMPLETE | MSG_FLG_FRAGS);
	return (b - b0) + crlf;
      }
      else
	return 0;
    }
    assert(crlf < bsiz);

    /* Skip crlf */
    b += crlf; bsiz -= crlf;
  }

  /* Now, looking at the chunk header */
  n = strcspn(b, CRLF);
  if (!eos && n == bsiz)
    return 0;
  crlf = CRLF_TEST(b + n);

  if (n == 0) {
    if (crlf == bsiz && eos) {
      msg_mark_as_complete(msg, MSG_FLG_COMPLETE | MSG_FLG_FRAGS);
      return crlf;
    }
    else
      return -1;		/* XXX - should we be more liberal? */
  }

  if (!eos && n + crlf == bsiz && (crlf == 0 || (crlf == 1 && b[n] == '\r')))
    return 0;

  chunk_len = strtoul(b, &s, 16);
  if (s == b)
    return -1;
  skip_ws(&s);
  if (s != b + n && s[0] != ';') /* Extra stuff that is not parameter */
    return -1;

  if (chunk_len == 0) {  /* We found last-chunk */
    b += n + crlf, bsiz -= n + crlf;

    crlf = bsiz > 0 ? CRLF_TEST(b) : 0;

    if ((eos && bsiz == 0) || crlf == 2 ||
	(crlf == 1 && (bsiz > 1 || b[0] == '\n'))) {
      /* Shortcut - We got empty trailers */
      b += crlf;
      msg_mark_as_complete(msg, MSG_FLG_COMPLETE | MSG_FLG_FRAGS);
    } else {
      /* We have to parse trailers */
      http->http_flags |= MSG_FLG_TRAILERS;
    }

    return b - b0;
  }
  else {
    issize_t chunk;

    b += n + crlf, bsiz -= n + crlf;

    /* Extract chunk */
    chunk = msg_extract_payload(msg, http,
				&h.header, chunk_len + (b - b0),
				b0, bsiz0, eos);

    if (chunk != -1 && h.header) {
      assert(h.chunk->pl_data);
      h.chunk->pl_data += (b - b0);
      h.chunk->pl_len -= (b - b0);
    }

    return chunk;
  }
}

/** Parse HTTP version.
 *
 *  The function http_version_d() parses a HTTP method.
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int http_version_d(char **ss, char const **ver)
{
  char *s = *ss;
  char const *result;
  int const version_size = sizeof(http_version_1_1) - 1;

  if (su_casenmatch(s, http_version_1_1, version_size) &&
      !IS_TOKEN(s[version_size])) {
    result = http_version_1_1;
    s += version_size;
  }
  else if (su_casenmatch(s, http_version_1_0, version_size) &&
	   !IS_TOKEN(s[version_size])) {
    result = http_version_1_0;
    s += version_size;
  }
  else if (s[0] == '\0') {
    result = http_version_0_9;
  } else {
    /* Version consists of one or two tokens, separated by / */
    size_t l1 = 0, l2 = 0, n;

    result = s;

    l1 = span_token(s);
    for (n = l1; IS_LWS(s[n]); n++)
      s[n] = '\0';
    if (s[n] == '/') {
      for (n = n + 1; IS_LWS(s[n]); n++)
        {}
      l2 = span_token(s + n);
      n += l2;
    }

    if (l1 == 0)
      return -1;

    /* If there is extra ws between tokens, compact version */
    if (l2 > 0 && n > l1 + 1 + l2) {
      s[l1] = '/';
      memmove(s + l1 + 1, s + n - l2, l2);
      s[l1 + 1 + l2] = 0;

      /* Compare again with compacted version */
      if (su_casematch(s, http_version_1_1))
	result = http_version_1_1;
      else if (su_casematch(s, http_version_1_0))
	result = http_version_1_0;
    }

    s += n;
  }

  while (IS_LWS(*s)) *s++ = '\0';

  *ss = s;

  if (ver)
    *ver = result;

  return 0;
}

/** Calculate extra space required by version string */
isize_t http_version_xtra(char const *version)
{
  if (version == http_version_1_1)
    return 0;
  else if (version == http_version_1_0)
    return 0;
  else
    return MSG_STRING_SIZE(version);
}

/** Duplicate a transport string */
void http_version_dup(char **pp, char const **dd, char const *s)
{
  if (s == http_version_1_1)
    *dd = s;
  else if (s == http_version_1_0)
    *dd = s;
  else
    MSG_STRING_DUP(*pp, *dd, s);
}

/** Well-known HTTP method names. */
static char const * const methods[] = {
  "<UNKNOWN>",
  http_method_name_get,
  http_method_name_post,
  http_method_name_head,
  http_method_name_options,
  http_method_name_put,
  http_method_name_delete,
  http_method_name_trace,
  http_method_name_connect,
  NULL,
  /* If you add something here, add also them to http_method_d! */
};

char const http_method_name_get[]     = "GET";
char const http_method_name_post[]    = "POST";
char const http_method_name_head[]    = "HEAD";
char const http_method_name_options[] = "OPTIONS";
char const http_method_name_put[]     = "PUT";
char const http_method_name_delete[]  = "DELETE";
char const http_method_name_trace[]   = "TRACE";
char const http_method_name_connect[] = "CONNECT";

char const *http_method_name(http_method_t method, char const *name)
{
  if (method > 0 && (size_t)method < sizeof(methods)/sizeof(methods[0]))
    return methods[method];
  else if (method == 0)
    return name;
  else
    return NULL;
}

/**Parse a HTTP method name.
 *
 * The function @c http_method_d() parses a HTTP method, and returns a code
 * corresponding to the method.  It stores the address of the first non-LWS
 * character after method name in @c *ss.
 *
 * @param ss    pointer to pointer to string to be parsed
 * @param nname pointer to value-result parameter formethod name
 *
 * @note
 * If there is no whitespace after method name, the value in @a *nname
 * may not be NUL-terminated.  The calling function @b must NUL terminate
 * the value by setting the @a **ss to NUL after first examining its value.
 *
 * @return The function @c http_method_d returns the method code if method
 * was identified, 0 (@c http_method_unknown) if method is not known, or @c -1
 * (@c http_method_invalid) if an error occurred.
 *
 * If the value-result argument @a nname is not @c NULL, http_method_d()
 * stores a pointer to the method name to it.
 */
http_method_t http_method_d(char **ss, char const **nname)
{
  char *s = *ss, c = *s;
  char const *name;
  int code = http_method_unknown;
  size_t n = 0;

#define MATCH(s, m) (su_casenmatch(s, m, n = sizeof(m) - 1))

  if (c >= 'a' && c <= 'z')
    c += 'A' - 'a';

  switch (c) {
  case 'C': if (MATCH(s, "CONNECT")) code = http_method_connect; break;
  case 'D': if (MATCH(s, "DELETE")) code = http_method_delete; break;
  case 'G': if (MATCH(s, "GET")) code = http_method_get; break;
  case 'H': if (MATCH(s, "HEAD")) code = http_method_head; break;
  case 'O': if (MATCH(s, "OPTIONS")) code = http_method_options; break;
  case 'P': if (MATCH(s, "POST")) code = http_method_post;
            else
            if (MATCH(s, "PUT")) code = http_method_put; break;
  case 'T': if (MATCH(s, "TRACE")) code = http_method_trace; break;
  }

#undef MATCH

  if (!code || IS_NON_WS(s[n])) {
    /* Unknown method */
    code = http_method_unknown;
    name = s;
    for (n = 0; IS_UNRESERVED(s[n]); n++)
      ;
    if (s[n]) {
      if (!IS_LWS(s[n]))
	return http_method_invalid;
      if (nname)
	s[n++] = '\0';
    }
  }
  else {
    name = methods[code];
  }

  while (IS_LWS(s[n]))
    n++;

  *ss = (s + n);
  if (nname) *nname = name;

  return (http_method_t)code;
}

/** Get method enum corresponding to method name */
http_method_t http_method_code(char const *name)
{
  /* Note that http_method_d() does not change string if nname is NULL */
  return http_method_d((char **)&name, NULL);
}

/**Parse HTTP query string.
 *
 * The function http_query_parse() searches for the given keys in HTTP @a
 * query. For each key, a query element (in the form name=value) is searched
 * from the query string. If a query element has a beginning matching with
 * the key, a copy of the rest of the element is returned in corresponding
 * return_value argument.
 *
 * @note The @a query string will be modified.
 *
 * @return
 * The function http_query_parse() returns number keys that matched within
 * the @a query string.
 */
issize_t http_query_parse(char *query,
			  /* char const *key, char **return_value, */
			  ...)
{
  va_list ap;
  char *q, *q_next;
  char *name, *value, **return_value;
  char const *key;
  size_t namelen, valuelen, keylen;
  isize_t N;
  int has_value;

  if (!query)
    return -1;

  for (q = query, N = 0; *q; q = q_next) {
    namelen = strcspn(q, "=&");
    valuelen = namelen + strcspn(q + namelen, "&");

    q_next = q + valuelen;
    if (*q_next)
      *q_next++ = '\0';

    value = q + namelen;
    has_value = (*value) != '\0'; /* is the part in form of name=value? */
    if (has_value)
      *value++ = '\0';

    name = url_unescape(q, q);

    if (has_value) {
      namelen = strlen(name);
      name[namelen] = '=';
      url_unescape(name + namelen + 1, value);
    }

    va_start(ap, query);

    while ((key = va_arg(ap, char const *))) {
      return_value = va_arg(ap, char **);
      keylen = strlen(key);

      if (strncmp(key, name, keylen) == 0) {
	*return_value = name + keylen;
	N++;
      }
    }

    va_end(ap);
  }

  return N;
}
