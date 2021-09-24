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

/**@CFILE test_http.c
 *
 * Testing functions for HTTP parser.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Mar  6 18:33:42 2001 ppessi
 */

#include "config.h"

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u

#include <sofia-sip/su.h>

#include <sofia-sip/su_types.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_io.h>

#include <sofia-sip/http_parser.h>

#include <sofia-sip/http_tag.h>
#include <sofia-sip/url_tag.h>

#include <sofia-sip/http_header.h>
#include <sofia-sip/msg_addr.h>

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

char const *name = "test_http";

static int tag_test(void);
static int tag_test2(void);
static int http_header_handling_test(void);
static int http_header_test(void);
static int http_parser_test(void);
static int test_http_encoding(void);
static int http_chunk_test(void);
static int http_tag_test(void);
static int test_query_parser(void);

static msg_t *read_message(char const string[]);
msg_mclass_t const *test_mclass = NULL;

char *lastpart(char *path)
{
  if (strchr(path, '/'))
    return strrchr(path, '/') + 1;
  else
    return path;
}

int tstflags;

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  name = lastpart(argv[0]);  /* Set our name */

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
#endif

  if (!test_mclass)
    test_mclass = http_default_mclass();

  retval |= tag_test(); fflush(stdout);
  retval |= tag_test2(); fflush(stdout);
  retval |= http_header_handling_test(); fflush(stdout);
  retval |= http_header_test(); fflush(stdout);
  retval |= http_tag_test(); fflush(stdout);
  retval |= http_parser_test(); fflush(stdout);
  retval |= test_http_encoding(); fflush(stdout);
  retval |= http_chunk_test(); fflush(stdout);
  retval |= test_query_parser(); fflush(stdout);

#if HAVE_OPEN_C
  sleep(5);
#endif

  return retval;
}

msg_t *read_message(char const buffer[])
{
  int i, n, m;
  msg_t *msg;
  msg_iovec_t iovec[2];

  n = strlen(buffer);
  if (n == 0)
    return NULL;

  msg = msg_create(test_mclass, MSG_DO_EXTRACT_COPY);

  for (i = 0; i < n;) {
    if (msg_recv_iovec(msg, iovec, 2, 10, 0) < 0) {
      perror("msg_recv_iovec");
      return NULL;
    }
    *(char *)(iovec->mv_base) = buffer[i++];
    msg_recv_commit(msg, 1, i == n);

    m = msg_extract(msg);
    if (m < 0) {
      fprintf(stderr, "test_http: parsing error\n");
      return NULL;
    }
    if (m > 0)
      break;
  }

  if (i != n) {
    fprintf(stderr, "test_http: parser error (len=%u, read=%u)\n", n, i);
    msg_destroy(msg), msg = NULL;
  }

  return msg;
}

/* Read message byte-by-byte */
msg_t *read_message_byte_by_byte(char const buffer[])
{
  int i, n, m;
  msg_t *msg;
  msg_iovec_t iovec[msg_n_fragments];

  n = strlen(buffer);
  if (n == 0)
    return NULL;

  msg = msg_create(test_mclass, MSG_DO_EXTRACT_COPY);

  for (i = 0; i < n;) {
    /* This prevent msg_recv_iovec() from allocating extra slack */
    int msg_buf_exact(msg_t *, int);
    msg_buf_exact(msg, 10 + 1);

    if (msg_recv_iovec(msg, iovec, msg_n_fragments, 10, 0) < 0) {
      perror("msg_recv_iovec");
      return NULL;
    }
    assert(iovec->mv_len > 0);
    *(char *)(iovec->mv_base) = buffer[i++];
    msg_recv_commit(msg, 1, i == n);

    m = msg_extract(msg);
    if (m < 0) {
      fprintf(stderr, "test_http: parsing error\n");
      return NULL;
    }
    if (m > 0)
      break;
  }

  if (i != n) {
    fprintf(stderr, "test_http: parser error (len=%u, read=%u)\n", n, i);
    msg_destroy(msg), msg = NULL;
  }

  return msg;
}

static
int header_size(http_header_t *h)
{
  int offset = 0;

  for (; h; h = h->sh_next) {
    offset += SU_ALIGN(offset) + h->sh_class->hc_size;
    offset = h->sh_class->hc_dxtra(h, offset);
  }

  return offset;
}

#define XTRA(xtra, h) SU_ALIGN(xtra) + header_size((http_header_t*)h)

/** Test header filtering and duplicating */
static int tag_test(void)
{
  su_home_t *home = su_home_new(sizeof *home);
  http_request_t *request =
    http_request_make(home, "GET /test/path HTTP/1.1");
  http_via_t *via = http_via_make(home, "1.1 http.example.com, 1.0 fred");
  http_host_t *host = http_host_make(home, "http.example.com:8080");
  http_max_forwards_t *mf = http_max_forwards_make(home, "16");
  url_t *url = url_hdup(home, (url_t *)"http://host:80/test/path");

  tagi_t *lst, *dup;
  int xtra;

  BEGIN();

  su_home_check(home);

  TEST_1(home); TEST_1(request); TEST_1(host);
  TEST_1(via); TEST_1(via->v_next);

  lst = tl_list(HTTPTAG_REQUEST(request),
		HTTPTAG_HOST(host),
		HTTPTAG_VIA(via),
		HTTPTAG_MAX_FORWARDS(mf),
		URLTAG_URL(url),
		TAG_NULL());

  xtra = 0;
  xtra += XTRA(xtra, request);
  xtra += XTRA(xtra, host);
  xtra += XTRA(xtra, via);
  xtra += XTRA(xtra, mf);
  xtra += SU_ALIGN(xtra) + sizeof(*url) + url_xtra(url);

  TEST_SIZE(tl_xtra(lst, 0), xtra);
  TEST_SIZE(tl_len(lst), 6 * sizeof(tagi_t));

  dup = tl_adup(NULL, lst);

  TEST_1(dup != NULL);
  TEST_SIZE(tl_len(dup), 6 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(dup, 0), xtra);

  if (tstflags & tst_verbatim)
    tl_print(stdout, "dup:\n", dup);

  su_free(NULL, dup);
  tl_vfree(lst);

  su_home_unref(home);

  END();
}

/** Test advanced tag features */
static int tag_test2(void)
{
  BEGIN();

#if 0
  tagi_t
    *lst, *dup, *filter1, *filter2, *filter3, *filter4,
    *b1, *b2, *b3, *b4;

  msg_t *msg;
  http_t *http;
  su_home_t *home;
  int xtra;

  home = su_home_new(sizeof *home);

  msg = read_message("HTTP/2.0 401 Unauthorized\r\n"
		     "Content-Length: 0\r\n"
		     "\r\n");

  http = msg_object(msg);

  TEST_1(home && msg && http);

  TEST_1(http_status_p((http_header_t *)http->http_status));
  TEST_1(http_content_length_p((http_header_t *)http->http_content_length));

  lst = tl_list(HTTPTAG_VIA(http->http_via),
		HTTPTAG_RECORD_ROUTE(http->http_record_route),
		TAG_SKIP(2),
		HTTPTAG_CSEQ(http->http_cseq),
		HTTPTAG_PAYLOAD(http->http_payload),
		TAG_NULL());
  filter1 = tl_list(HTTPTAG_VIA(0),
		    TAG_NULL());
  filter2 = tl_list(HTTPTAG_CALL_ID(0),
		    HTTPTAG_FROM(0),
		    HTTPTAG_ROUTE(0),
		    HTTPTAG_CSEQ(0),
		    TAG_NULL());
  filter3 = tl_list(HTTPTAG_CSEQ(0),
		    HTTPTAG_CONTENT_LENGTH(0),
		    TAG_NULL());
  filter4 = tl_list(HTTPTAG_STATUS(0),
		    HTTPTAG_VIA(0),
		    HTTPTAG_RECORD_ROUTE(0),
		    HTTPTAG_FROM(0),
		    HTTPTAG_TO(0),
		    HTTPTAG_CALL_ID(0),
		    HTTPTAG_CSEQ(0),
		    HTTPTAG_WWW_AUTHENTICATE(0),
		    HTTPTAG_PROXY_AUTHENTICATE(0),
		    HTTPTAG_CONTENT_LENGTH(0),
		    TAG_NULL());

  TEST_1(lst && filter1 && filter2 && filter3 && filter4);

  b1 = tl_afilter(home, filter1, lst);
  TEST(tl_len(b1), 2 * sizeof(tagi_t));
  TEST_1(((http_via_t *)b1->t_value)->v_next);
  xtra = http_header_size((http_header_t *)http->http_via);
  xtra += SU_ALIGN(xtra);
  xtra += http_header_size((http_header_t *)http->http_via->v_next);
  TEST(tl_xtra(b1, 0), xtra);

  dup = tl_adup(home, lst);

  TEST(tl_len(dup), tl_len(lst));
  TEST(tl_xtra(dup, 0), tl_xtra(lst, 0));

  tl_vfree(lst);

  lst = tl_list(HTTPTAG_HTTP(http), TAG_NULL());

  b2 = tl_afilter(home, filter2, lst);
  TEST(tl_len(b2), 4 * sizeof(tagi_t));
  xtra = 0;
  xtra += XTRA(xtra, http->http_call_id);
  xtra += XTRA(xtra, http->http_from);
  xtra += XTRA(xtra, http->http_cseq);
  TEST(tl_xtra(b2, 0), xtra);

  b3 = tl_afilter(home, filter3, lst);

  TEST(tl_len(b3), 3 * sizeof(tagi_t));
  TEST(tl_xtra(b3, 0),
       sizeof(http_content_length_t) + sizeof(http_cseq_t));


  b4 = tl_afilter(home, filter4, lst);
  TEST(tl_len(b4), 11 * sizeof(tagi_t));
  xtra = 0;
  xtra += XTRA(xtra, http->http_status);
  xtra += XTRA(xtra, http->http_via);
  xtra += XTRA(xtra, http->http_via->v_next);
  xtra += XTRA(xtra, http->http_record_route);
  xtra += XTRA(xtra, http->http_from);
  xtra += XTRA(xtra, http->http_to);
  xtra += XTRA(xtra, http->http_call_id);
  xtra += XTRA(xtra, http->http_cseq);
  xtra += XTRA(xtra, http->http_www_authenticate);
  xtra += XTRA(xtra, http->http_proxy_authenticate);
  xtra += XTRA(xtra, http->http_content_length);
  TEST(tl_xtra(b4, 0), xtra);

  tl_vfree(filter1); tl_vfree(filter2); tl_vfree(filter3); tl_vfree(filter4);
  tl_vfree(lst);

  su_home_check(home);

  su_free(home, b4);
  su_free(home, b3);
  su_free(home, b2);
  su_free(home, dup);
  su_free(home, b1);

  su_home_check(home);

  su_home_unref(home);
#endif

  END();
}

/** Test parser and header manipulation */
static int http_header_handling_test(void)
{
  msg_t *msg;
  http_t *http;
  su_home_t *home;

  http_request_t            http_request[1];
  http_status_t             http_status[1];
  http_unknown_t            http_unknown[1];
  http_separator_t          http_separator[1];
  http_payload_t            http_payload[1];
  http_via_t                http_via[1];
  http_host_t               http_host[1];
  http_from_t               http_from[1];
  http_referer_t            http_referer[1];
  http_connection_t         http_connection[1];
  http_accept_t             http_accept[1];
  http_accept_charset_t     http_accept_charset[1];
  http_accept_encoding_t    http_accept_encoding[1];
  http_accept_language_t    http_accept_language[1];
  http_accept_ranges_t      http_accept_ranges[1];
  http_allow_t              http_allow[1];
  http_te_t                 http_te[1];
  http_authorization_t      http_authorization[1];
  http_www_authenticate_t   http_www_authenticate[1];
  http_proxy_authenticate_t http_proxy_authenticate[1];
  http_proxy_authorization_t http_proxy_authorization[1];
  http_age_t                http_age[1];
  http_cache_control_t      http_cache_control[1];
  http_date_t               http_date[1];
  http_expires_t            http_expires[1];
  http_if_match_t           http_if_match[1];
  http_if_modified_since_t  http_if_modified_since[1];
  http_if_none_match_t      http_if_none_match[1];
  http_if_range_t           http_if_range[1];
  http_if_unmodified_since_t http_if_unmodified_since[1];
  http_etag_t               http_etag[1];
  http_expect_t             http_expect[1];
  http_last_modified_t      http_last_modified[1];
  http_location_t           http_location[1];
  http_max_forwards_t       http_max_forwards[1];
  http_pragma_t             http_pragma[1];
  http_range_t              http_range[1];
  http_retry_after_t        http_retry_after[1];
  http_trailer_t            http_trailer[1];
  http_upgrade_t            http_upgrade[1];
  http_vary_t               http_vary[1];
  http_warning_t            http_warning[1];
  http_user_agent_t         http_user_agent[1];
  http_server_t             http_server[1];
  http_mime_version_t       http_mime_version[1];
  http_content_language_t   http_content_language[1];
  http_content_location_t   http_content_location[1];
  http_content_md5_t        http_content_md5[1];
  http_content_range_t      http_content_range[1];
  http_content_encoding_t   http_content_encoding[1];
  http_transfer_encoding_t  http_transfer_encoding[1];
  http_content_type_t       http_content_type[1];
  http_content_length_t     http_content_length[1];

  BEGIN();

  http_request_init(http_request);
  http_status_init(http_status);
  http_unknown_init(http_unknown);
  http_separator_init(http_separator);
  http_payload_init(http_payload);
  http_via_init(http_via);
  http_host_init(http_host);
  http_from_init(http_from);
  http_referer_init(http_referer);
  http_connection_init(http_connection);

  http_accept_init(http_accept);
  http_accept_charset_init(http_accept_charset);
  http_accept_encoding_init(http_accept_encoding);
  http_accept_language_init(http_accept_language);
  http_accept_ranges_init(http_accept_ranges);
  http_allow_init(http_allow);
  http_te_init(http_te);

  http_authorization_init(http_authorization);
  http_www_authenticate_init(http_www_authenticate);
  http_proxy_authenticate_init(http_proxy_authenticate);
  http_proxy_authorization_init(http_proxy_authorization);

  http_age_init(http_age);
  http_cache_control_init(http_cache_control);
  http_date_init(http_date);
  http_expires_init(http_expires);
  http_if_match_init(http_if_match);
  http_if_modified_since_init(http_if_modified_since);
  http_if_none_match_init(http_if_none_match);
  http_if_range_init(http_if_range);
  http_if_unmodified_since_init(http_if_unmodified_since);

  http_etag_init(http_etag);
  http_expect_init(http_expect);
  http_last_modified_init(http_last_modified);
  http_location_init(http_location);
  http_max_forwards_init(http_max_forwards);
  http_pragma_init(http_pragma);
  http_range_init(http_range);
  http_retry_after_init(http_retry_after);
  http_trailer_init(http_trailer);
  http_upgrade_init(http_upgrade);
  http_vary_init(http_vary);
  http_warning_init(http_warning);

  http_user_agent_init(http_user_agent);
  http_server_init(http_server);

  http_mime_version_init(http_mime_version);
  http_content_language_init(http_content_language);
  http_content_location_init(http_content_location);
  http_content_md5_init(http_content_md5);
  http_content_range_init(http_content_range);

  http_content_encoding_init(http_content_encoding);
  http_transfer_encoding_init(http_transfer_encoding);

  http_content_type_init(http_content_type);
  http_content_length_init(http_content_length);

  home = su_home_new(sizeof *home);

  {
    int i;

    struct { http_method_t number; char const *name; } methods[] = {
      { http_method_get, "GET" },
      { http_method_post, "POST" },
      { http_method_head, "HEAD" },
      { http_method_options, "OPTIONS" },
      { http_method_put, "PUT" },
      { http_method_delete, "DELETE" },
      { http_method_trace, "TRACE" },
      { http_method_connect, "CONNECT" },
      { 0, NULL }
    };

    for (i = 0; methods[i].name; i++) {
      TEST_1(strcmp(methods[i].name,
		   http_method_name(methods[i].number, "")) == 0);
    }
  }

  msg = read_message(
    "PUT /Foo HTTP/1.1\r\n"
    "Host: [::1]:8080\r\n"
    "From: webmaster@w3.org\r\n"
    "Via: 1.0 fred, 1.1 nowhere.com (Apache/1.1)\r\n"
    "Referer: http://www.w3.org/hypertext/DataSources/Overview.html\r\n"
    "Connection: close\r\n"
    "Accept: audio/*; q=0.2, audio/basic\r\n"
    "Accept-Charset: iso-8859-5, unicode-1-1;q=0.8\r\n"
    "Accept-Encoding: gzip;q=1.0, identity; q=0.5, *;q=0\r\n"
    "Accept-Language: da, en-gb;q=0.8, en;q=0.7\r\n"
    "Accept-Ranges: bytes\r\n"
    "Age: 212\r\n"
    "Allow: GET, HEAD, PUT\r\n"
    "Cache-Control: private, community=\"UCI\"\r\n"
    "Content-Encoding: identity\r\n"
    "Content-Language: da\r\n"
    "Content-Type: text/html\r\n"
    "Content-Location: http://localhost/Foo\r\n"
    "Content-Length: 28\r\n"
    "Content-MD5: f48BLMCjkX5M5PgWoelogA==\r\n"
    "Content-Range: bytes 0-27/*\r\n"
    "\r\n"
    "<html><body></body></html>\r\n");

  http = msg_object(msg);

  TEST_1(home && msg && http);

  TEST_1(http_is_request((http_header_t *)http->http_request));
  TEST_1(http->http_via); TEST_1(http->http_via->v_next);
  TEST_1(http->http_via->v_next->v_next == NULL);

  TEST_1(http->http_host);
  TEST_1(http->http_from);
  TEST_1(http->http_referer);
  TEST_1(http->http_connection);
  TEST_1(http->http_accept);
  TEST_1(http->http_accept_charset);
  TEST_1(http->http_accept_encoding);
  TEST_1(http->http_accept_language);
  TEST_1(http->http_accept_ranges);
  TEST_1(http->http_age);
  TEST_1(http->http_allow);
  TEST_1(http->http_cache_control);
  TEST_1(http->http_content_encoding);
  TEST_1(http->http_content_language);
  TEST_1(http->http_content_type);
  TEST_1(http->http_content_location);
  TEST_1(http->http_content_length);
  TEST_1(http->http_content_md5);
  TEST_1(http->http_content_range);

  /* Quiet lots of warnings */
#define _msg_header_offset msg_header_offset
#define msg_header_offset(msg, http, h)			\
  _msg_header_offset(msg, http, (http_header_t *)h)

  TEST_P(msg_header_offset(msg, http, http_request), &http->http_request);
  TEST_P(msg_header_offset(msg, http, http_status), &http->http_status);
  TEST_P(msg_header_offset(msg, http, http_unknown), &http->http_unknown);
  TEST_P(msg_header_offset(msg, http, http_separator), &http->http_separator);
  TEST_P(msg_header_offset(msg, http, http_payload), &http->http_payload);
  TEST_P(msg_header_offset(msg, http, http_via), &http->http_via);

  TEST_P(msg_header_offset(msg, http, http_via), &http->http_via);
  TEST_P(msg_header_offset(msg, http, http_host), &http->http_host);
  TEST_P(msg_header_offset(msg, http, http_from), &http->http_from);
  TEST_P(msg_header_offset(msg, http, http_referer), &http->http_referer);
  TEST_P(msg_header_offset(msg, http, http_connection),
	 &http->http_connection);

  TEST_P(msg_header_offset(msg, http, http_accept), &http->http_accept);
  TEST_P(msg_header_offset(msg, http, http_accept_charset),
	 &http->http_accept_charset);
  TEST_P(msg_header_offset(msg, http, http_accept_encoding),
	 &http->http_accept_encoding);
  TEST_P(msg_header_offset(msg, http, http_accept_language),
	 &http->http_accept_language);
  TEST_P(msg_header_offset(msg, http, http_accept_ranges),
	 &http->http_accept_ranges);
  TEST_P(msg_header_offset(msg, http, http_allow),
	 &http->http_allow);
  TEST_P(msg_header_offset(msg, http, http_te),
	 &http->http_te);

  TEST_P(msg_header_offset(msg, http, http_authorization),
	 &http->http_authorization);
  TEST_P(msg_header_offset(msg, http, http_www_authenticate),
	 &http->http_www_authenticate);
  TEST_P(msg_header_offset(msg, http, http_proxy_authenticate),
	 &http->http_proxy_authenticate);
  TEST_P(msg_header_offset(msg, http, http_proxy_authorization),
	 &http->http_proxy_authorization);

  TEST_P(msg_header_offset(msg, http, http_age),
	 &http->http_age);
  TEST_P(msg_header_offset(msg, http, http_cache_control),
	 &http->http_cache_control);
  TEST_P(msg_header_offset(msg, http, http_date),
	 &http->http_date);
  TEST_P(msg_header_offset(msg, http, http_expires),
	 &http->http_expires);
  TEST_P(msg_header_offset(msg, http, http_if_match),
	 &http->http_if_match);
  TEST_P(msg_header_offset(msg, http, http_if_modified_since),
	 &http->http_if_modified_since);
  TEST_P(msg_header_offset(msg, http, http_if_none_match),
	 &http->http_if_none_match);
  TEST_P(msg_header_offset(msg, http, http_if_range),
	 &http->http_if_range);
  TEST_P(msg_header_offset(msg, http, http_if_unmodified_since),
	 &http->http_if_unmodified_since);

  TEST_P(msg_header_offset(msg, http, http_etag),
	 &http->http_etag);
  TEST_P(msg_header_offset(msg, http, http_expect),
	 &http->http_expect);
  TEST_P(msg_header_offset(msg, http, http_last_modified),
	 &http->http_last_modified);
  TEST_P(msg_header_offset(msg, http, http_location),
	 &http->http_location);
  TEST_P(msg_header_offset(msg, http, http_max_forwards),
	 &http->http_max_forwards);
  TEST_P(msg_header_offset(msg, http, http_pragma),
	 &http->http_pragma);
  TEST_P(msg_header_offset(msg, http, http_range),
	 &http->http_range);
  TEST_P(msg_header_offset(msg, http, http_retry_after),
	 &http->http_retry_after);
  TEST_P(msg_header_offset(msg, http, http_trailer),
	 &http->http_trailer);
  TEST_P(msg_header_offset(msg, http, http_upgrade),
	 &http->http_upgrade);
  TEST_P(msg_header_offset(msg, http, http_vary),
	 &http->http_vary);
  TEST_P(msg_header_offset(msg, http, http_warning),
	 &http->http_warning);

  TEST_P(msg_header_offset(msg, http, http_user_agent),
	 &http->http_user_agent);
  TEST_P(msg_header_offset(msg, http, http_server),
	 &http->http_server);

  TEST_P(msg_header_offset(msg, http, http_mime_version),
	 &http->http_mime_version);
  TEST_P(msg_header_offset(msg, http, http_content_language),
	 &http->http_content_language);
  TEST_P(msg_header_offset(msg, http, http_content_location),
	 &http->http_content_location);
  TEST_P(msg_header_offset(msg, http, http_content_md5),
	 &http->http_content_md5);
  TEST_P(msg_header_offset(msg, http, http_content_range),
	 &http->http_content_range);

  TEST_P(msg_header_offset(msg, http, http_content_encoding),
	 &http->http_content_encoding);
  TEST_P(msg_header_offset(msg, http, http_transfer_encoding),
	 &http->http_transfer_encoding);

  TEST_P(msg_header_offset(msg, http, http_content_type),
	 &http->http_content_type);
  TEST_P(msg_header_offset(msg, http, http_content_length),
	 &http->http_content_length);

  TEST_SIZE(http_via_class->hc_params,
	    offsetof(http_via_t, v_common));
  TEST_SIZE(http_host_class->hc_params,
	    offsetof(http_host_t, h_common));
  TEST_SIZE(http_from_class->hc_params,
	    offsetof(http_from_t, g_common));
  TEST_SIZE(http_referer_class->hc_params,
	    offsetof(http_referer_t, loc_common));
  TEST_SIZE(http_connection_class->hc_params,
	    offsetof(http_connection_t, k_items));

  TEST_SIZE(http_accept_class->hc_params,
	    offsetof(http_accept_t, ac_params));
  TEST_SIZE(http_accept_charset_class->hc_params,
	    offsetof(http_accept_charset_t, aa_params));
  TEST_SIZE(http_accept_encoding_class->hc_params,
	    offsetof(http_accept_encoding_t, aa_params));
  TEST_SIZE(http_accept_language_class->hc_params,
	    offsetof(http_accept_language_t, aa_params));
  TEST_SIZE(http_accept_ranges_class->hc_params,
	    offsetof(http_accept_ranges_t, k_items));
  TEST_SIZE(http_allow_class->hc_params,
	    offsetof(http_allow_t, k_items));
  TEST_SIZE(http_te_class->hc_params,
	    offsetof(http_te_t, te_params));

  TEST_SIZE(http_authorization_class->hc_params,
	    offsetof(http_authorization_t, au_params));
  TEST_SIZE(http_www_authenticate_class->hc_params,
	    offsetof(http_www_authenticate_t, au_params));
  TEST_SIZE(http_proxy_authenticate_class->hc_params,
	    offsetof(http_proxy_authenticate_t, au_params));
  TEST_SIZE(http_proxy_authorization_class->hc_params,
	    offsetof(http_proxy_authorization_t, au_params));

  TEST_SIZE(http_age_class->hc_params,
	    offsetof(http_age_t, x_common));
  TEST_SIZE(http_cache_control_class->hc_params,
	    offsetof(http_cache_control_t, k_items));
  TEST_SIZE(http_date_class->hc_params,
	    offsetof(http_date_t, d_common));
  TEST_SIZE(http_expires_class->hc_params,
	    offsetof(http_expires_t, d_common));
  TEST_SIZE(http_if_match_class->hc_params,
	    offsetof(http_if_match_t, k_items));
  TEST_SIZE(http_if_modified_since_class->hc_params,
	    offsetof(http_if_modified_since_t, d_common));
  TEST_SIZE(http_if_none_match_class->hc_params,
	    offsetof(http_if_none_match_t, k_items));
  TEST_SIZE(http_if_range_class->hc_params,
	    offsetof(http_if_range_t, ifr_common));
  TEST_SIZE(http_if_unmodified_since_class->hc_params,
	    offsetof(http_if_unmodified_since_t, d_common));

  TEST_SIZE(http_etag_class->hc_params,
	    offsetof(http_etag_t, g_common));
  TEST_SIZE(http_expect_class->hc_params,
	    offsetof(http_expect_t, g_common));
  TEST_SIZE(http_last_modified_class->hc_params,
	    offsetof(http_last_modified_t, d_common));
  TEST_SIZE(http_location_class->hc_params,
	    offsetof(http_location_t, loc_common));
  TEST_SIZE(http_max_forwards_class->hc_params,
	    offsetof(http_max_forwards_t, mf_common));
  TEST_SIZE(http_pragma_class->hc_params,
	    offsetof(http_pragma_t, k_items));
  TEST_SIZE(http_range_class->hc_params,
	    offsetof(http_range_t, rng_specs));
  TEST_SIZE(http_retry_after_class->hc_params,
	    offsetof(http_retry_after_t, ra_common));
  TEST_SIZE(http_trailer_class->hc_params,
	    offsetof(http_trailer_t, k_items));
  TEST_SIZE(http_upgrade_class->hc_params,
	    offsetof(http_upgrade_t, k_items));
  TEST_SIZE(http_vary_class->hc_params,
	    offsetof(http_vary_t, k_items));
  TEST_SIZE(http_warning_class->hc_params,
	    offsetof(http_warning_t, w_common));

  TEST_SIZE(http_user_agent_class->hc_params,
	    offsetof(http_user_agent_t, g_common));
  TEST_SIZE(http_server_class->hc_params,
	    offsetof(http_server_t, g_common));

  TEST_SIZE(http_mime_version_class->hc_params,
	    offsetof(http_mime_version_t, g_common));
  TEST_SIZE(http_content_language_class->hc_params,
	    offsetof(http_content_language_t, k_items));
  TEST_SIZE(http_content_location_class->hc_params,
	    offsetof(http_content_location_t, g_common));
  TEST_SIZE(http_content_md5_class->hc_params,
	    offsetof(http_content_md5_t, g_common));
  TEST_SIZE(http_content_range_class->hc_params,
	    offsetof(http_content_range_t, cr_common));

  TEST_SIZE(http_content_encoding_class->hc_params,
	    offsetof(http_content_encoding_t, k_items));
  TEST_SIZE(http_transfer_encoding_class->hc_params,
	    offsetof(http_transfer_encoding_t, k_items));

  TEST_SIZE(http_content_type_class->hc_params,
	    offsetof(http_content_type_t, c_params));
  TEST_SIZE(http_content_length_class->hc_params,
	    offsetof(http_content_length_t, l_common));

  su_home_unref(home);

  END();
}

int count(msg_common_t *h)
{
  http_header_t *sh = (http_header_t *)h;
  unsigned n;

  for (n = 0; sh; sh = sh->sh_next)
    n++;

  return n;
}

int len(msg_common_t *h)
{
  msg_header_t *sh = (msg_header_t *)h;
  unsigned n;

  for (n = 0; sh; sh = sh->sh_next) {
    if (n) n +=2;
    n += msg_header_field_e(NULL, 0, sh, 0);
  }

  return n;
}

static int http_header_test(void)
{
  su_home_t home[1] = { SU_HOME_INIT(home) };

  BEGIN();

  {
    http_request_t *rq;

    TEST_1(rq = http_request_make(home, "GET / HTTP/1.0"));

    TEST(rq->rq_method, http_method_get);
    TEST_S(rq->rq_method_name, "GET");
    TEST_S(rq->rq_url->url_path, "");
    TEST_1(rq->rq_url->url_root);
    TEST_S(url_as_string(home, rq->rq_url), "/");
    TEST_P(rq->rq_version, http_version_1_0);

    TEST_1(rq = http_request_make(home, "GET / HTTP/1.2"));

    TEST(rq->rq_method, http_method_get);
    TEST_S(rq->rq_method_name, "GET");
    TEST_S(rq->rq_url->url_path, "");
    TEST_1(rq->rq_url->url_root);
    TEST_S(url_as_string(home, rq->rq_url), "/");
    TEST_S(rq->rq_version, "HTTP/1.2");

    TEST_1(rq = http_request_make(home, "GET /foo"));
    TEST(rq->rq_method, http_method_get);
    TEST_S(rq->rq_method_name, "GET");
    TEST_S(rq->rq_url->url_path, "foo");
    TEST_1(rq->rq_url->url_root);
    TEST_S(url_as_string(home, rq->rq_url), "/foo");
    TEST_S(rq->rq_version, "");
    TEST_P(rq->rq_version, http_version_0_9);
  }

  {
    http_status_t *st;

    st = http_status_make(home, "HTTP/1.0 100 Continue"); TEST_1(st);
    TEST_S(st->st_version, "HTTP/1.0");
    TEST_P(st->st_version, http_version_1_0);
    TEST(st->st_status, 100);
    TEST_S(st->st_phrase, "Continue");

    st = http_status_make(home, "HTTP/1.1 200"); TEST_1(st);
    TEST_S(st->st_version, "HTTP/1.1");
    TEST(st->st_status, 200);
    TEST_S(st->st_phrase, "");

    st = http_status_make(home, "HTTP/1.1  200  Ok"); TEST_1(st);
    TEST_S(st->st_version, "HTTP/1.1");
    TEST_P(st->st_version, http_version_1_1);
    TEST(st->st_status, 200);
    TEST_S(st->st_phrase, "Ok");

    st = http_status_make(home, "HTTP  99  Ok "); TEST_1(st);
    TEST_S(st->st_version, "HTTP");
    TEST(st->st_status, 99);
    TEST_S(st->st_phrase, "Ok");

    st = http_status_make(home, "HTTP/1.2 200 Ok"); TEST_1(st);
    TEST_S(st->st_version, "HTTP/1.2");
    TEST(st->st_status, 200);
    TEST_S(st->st_phrase, "Ok");
  }

  {
    http_content_range_t *cr;

    cr = http_content_range_make(home, "bytes 0 - 499 / *"); TEST_1(cr);
    TEST64(cr->cr_first, 0);
    TEST64(cr->cr_last, 499);
    TEST64(cr->cr_length, (http_off_t)-1);

    cr = http_content_range_make(home, "bytes 500-999/9913133"); TEST_1(cr);
    TEST64(cr->cr_first, 500);
    TEST64(cr->cr_last, 999);
    TEST64(cr->cr_length, 9913133);

    TEST_1(!http_content_range_make(home, "bytes = 0 - 499 / *,"));
  }

  {
    http_cookie_t *c;

    c = http_cookie_make(home, "$Version=1;"
			 "foo=bar;$Domain=.nokia.com;$Path=\"\"");
    TEST_1(c);
    TEST_1(c->c_params);
    TEST_S(c->c_version, "1");
    TEST_S(c->c_name, "foo=bar");
    TEST_S(c->c_domain, ".nokia.com");
    TEST_S(c->c_path, "\"\"");

    c = http_cookie_make(home, "$Version=1;"
			 "foo=bar;$Domain=.nokia.com;$Path=\"\",  , "
			 "bar=bazzz;$Domain=.research.nokia.com;$Path=\"\";"
			 "hum=ham;$Domain=.nokia.fi;$Path=\"/sofia\""
			 );

    TEST_1(c);
    TEST_1(c->c_params);
    TEST_S(c->c_version, "1");
    TEST_S(c->c_name, "foo=bar");
    TEST_S(c->c_domain, ".nokia.com");
    TEST_S(c->c_path, "\"\"");

    c = http_cookie_make(home, "foo=bar=baz1");

    TEST_1(c);
    TEST_1(c->c_params);
    TEST_S(c->c_params[0], "foo=bar=baz1");
  }

  {
    http_set_cookie_t *sc;

    sc = http_set_cookie_make(home, "foo=bar;Domain=.nokia.com;Path=\"\""
			      ";Foo=bar;Version=1;Secure;Max-age=1212;"
			      "Comment=\"Jummi Jammmi\"");
    TEST_1(sc);
    TEST_1(sc->sc_params);
    TEST_S(sc->sc_name, "foo=bar");
    TEST_S(sc->sc_version, "1");
    TEST_S(sc->sc_domain, ".nokia.com");
    TEST_S(sc->sc_max_age, "1212");
    TEST_S(sc->sc_path, "\"\"");
    TEST_S(sc->sc_comment, "\"Jummi Jammmi\"");
    TEST(sc->sc_secure, 1);

    sc = http_set_cookie_make(home, "foo=bar;Domain=.nokia.com;Path=\"\""
			      ";Foo=bar;Version=1");

    TEST_1(sc);
    TEST_1(sc->sc_params);
    TEST_S(sc->sc_name, "foo=bar");
    TEST_S(sc->sc_version, "1");
    TEST_S(sc->sc_domain, ".nokia.com");
    TEST_P(sc->sc_max_age, NULL);
    TEST_S(sc->sc_path, "\"\"");
    TEST_S(sc->sc_comment, NULL);
    TEST(sc->sc_secure, 0);


    sc = http_set_cookie_make(home,
			      "CUSTOMER=WILE_E_COYOTE; "
			      "path=/; "
			      "expires=Wednesday, 09-Nov-99 23:12:40 GMT");

    TEST_1(sc);
    TEST_1(sc->sc_params);
    TEST_S(sc->sc_name, "CUSTOMER=WILE_E_COYOTE");
    TEST_S(sc->sc_version, NULL);
    TEST_S(sc->sc_domain, NULL);
    TEST_S(sc->sc_max_age, NULL);
    TEST_S(sc->sc_path, "/");
    TEST_S(sc->sc_comment, NULL);
    TEST(sc->sc_secure, 0);
  }

  {
    http_range_t *rng;

    rng = http_range_make(home, "bytes = 0 - 499"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "0-499");
    TEST_P(rng->rng_specs[1], NULL);

    rng = http_range_make(home, "bytes=,500 - 999"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "500-999");
    TEST_P(rng->rng_specs[1], NULL);

    rng = http_range_make(home, "bytes= - 500"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "-500");
    TEST_P(rng->rng_specs[1], NULL);

    rng = http_range_make(home, "bytes=9500-"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "9500-");
    TEST_P(rng->rng_specs[1], NULL);

    rng = http_range_make(home, "bytes=0- 0 , -  1"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "0-0");
    TEST_S(rng->rng_specs[1], "-1");
    TEST_P(rng->rng_specs[2], NULL);

    rng = http_range_make(home, "bytes=500-600 , 601 - 999 ,,"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "500-600");
    TEST_S(rng->rng_specs[1], "601-999");
    TEST_P(rng->rng_specs[2], NULL);

    rng = http_range_make(home, "bytes=500-700,601-999"); TEST_1(rng);
    TEST_S(rng->rng_unit, "bytes");
    TEST_1(rng->rng_specs);
    TEST_S(rng->rng_specs[0], "500-700");
    TEST_S(rng->rng_specs[1], "601-999");
    TEST_P(rng->rng_specs[2], NULL);
  }

  {
    http_date_t *d;
    char const *s;
    char b[64];

    d = http_date_make(home, s = "Wed, 15 Nov 1995 06:25:24 GMT"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 816416724);
    TEST_1(http_date_e(b, sizeof b, (msg_header_t*)d, 0) > 0);
    TEST_S(b, s);
    d = http_date_make(home, s = "Wed, 15 Nov 1995 04:58:08 GMT"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 816411488);
    TEST_1(http_date_e(b, sizeof b, (msg_header_t*)d, 0) > 0);
    TEST_S(b, s);
    d = http_date_make(home, s = "Tue, 15 Nov 1994 08:12:31 GMT"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 784887151);
    TEST_1(http_date_e(b, sizeof b, (msg_header_t*)d, 0) > 0);
    TEST_S(b, s);
    d = http_date_make(home, "Fri Jan 30 12:21:09 2004"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 1075465269);
    d = http_date_make(home, "Fri Jan  1 12:21:09 2004"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 1072959669);
    d = http_date_make(home, "Tuesday, 15-Nov-94 08:12:31 GMT"); TEST_1(d);
    TEST(d->d_time, 2208988800UL + 784887151);
  }

  {
    http_retry_after_t *ra;
    char const *s;
    char b[64];

    ra = http_retry_after_make(home, s = "Wed, 15 Nov 1995 06:25:24 GMT");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 816416724);
    TEST_1(http_retry_after_e(b, sizeof b, (msg_header_t*)ra, 0) > 0);
    TEST_S(b, s);
    ra = http_retry_after_make(home, s = "Wed, 15 Nov 1995 04:58:08 GMT");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 816411488);
    TEST_1(http_retry_after_e(b, sizeof b, (msg_header_t*)ra, 0) > 0);
    TEST_S(b, s);
    ra = http_retry_after_make(home, s = "Tue, 15 Nov 1994 08:12:31 GMT");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 784887151);
    TEST_1(http_retry_after_e(b, sizeof b, (msg_header_t*)ra, 0) > 0);
    TEST_S(b, s);
    ra = http_retry_after_make(home, "Fri Jan 30 12:21:09 2004");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 1075465269);
    ra = http_retry_after_make(home, "Fri Jan  1 12:21:09 2004");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 1072959669);
    ra = http_retry_after_make(home, "Tuesday, 15-Nov-94 08:12:31 GMT");
    TEST_1(ra);
    TEST(ra->ra_date + ra->ra_delta, 2208988800UL + 784887151);
    ra = http_retry_after_make(home, "121");
    TEST_1(ra);
    TEST(ra->ra_date, 0);
    TEST(ra->ra_delta, 121);
  }

  {
    http_location_t *l;

    TEST_1(l = http_location_make(home, "http://www.google.fi/cxfer?c=PREF%3D:TM%3D1105378671:S%3DfoewuOwfszMIFJbP&prev=/"));
  }

  su_home_deinit(home);

  END();
}

static int http_parser_test(void)
{
  msg_t *msg;
  http_t *http;

  BEGIN();

  {
    char data[] =
      "HTTP/1.1 200\r\n"
      "Server: Apache/1.3.11 (Unix) tomcat/1.0\r\n"
      "Transfer-Encoding: chunked, gzip\r\n"
      "Transfer-Encoding: deflate\r\n"
      "TE: chunked, gzip\r\n"
      "TE: deflate\r\n"
      "Content-Encoding: identity, gzip\r\n"
      "Content-Type: text/html\r\n"
      "Content-Encoding: deflate\r\n"
      "Set-Cookie: PREF=ID=1eab07c269cd7e5a:LD=fi:TM=1094601448:LM=1094601448:S=Ik6IEs3W3vamd8Xu; "
      "expires=Sun, 17-Jan-2038 19:14:07 GMT ; path=/; domain=.google.fi\r\n"
      "Set-Cookie: CUSTOMER=WILE_E_COYOTE; "
      "path=/; "
      "expires=Wednesday, 09-Nov-99 23:12:40 GMT\r\n"
      "\r\n"
      "4c\r\n"
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n"
      "\r\n"
      "<html>\r\n"
      "\r\n"
      "\r\n9\r\n"
      "</html>\r\n"
      "\r\n0\r\n"
      "Date: Mon, 21 Oct 2002 13:10:41 GMT\r\n"
      "\n";

    TEST_1(msg = read_message_byte_by_byte(data));

    http = msg_object(msg); TEST_1(http);

    TEST_1(http->http_status);
    TEST_1(http->http_payload);
    TEST_1(http->http_payload->pl_next);
    TEST_1(http->http_date);
    TEST_1(http->http_te);
    TEST_1(http->http_transfer_encoding);
    TEST_1(http->http_transfer_encoding->k_items);
    TEST_1(http->http_content_encoding);
    TEST_1(http->http_content_encoding->k_items);
    TEST_1(http->http_content_encoding->k_items);
    TEST_S(http->http_content_encoding->k_items[0], "identity");
    TEST_S(http->http_content_encoding->k_items[1], "gzip");
    TEST_S(http->http_content_encoding->k_items[2], "deflate");
    TEST_P(http->http_content_encoding->k_items[3], NULL);
    TEST_1(http->http_set_cookie);
    TEST_1(http->http_set_cookie->sc_next);

    msg_destroy(msg);
  }

  END();
}

static int test_http_encoding(void)
{
  http_header_t *h, *h1;
  msg_t *msg;
  http_t *http;
  su_home_t *home;
  char b[160];
  size_t n;

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));

  msg = read_message(
    "GET http://bar.com/foo/ HTTP/1.1\r\n"
    "Accept: text/html\r\n"
    "Accept-Charset: iso-8859-1\r\n"
    "Accept-Encoding: gzip\r\n"
    "Accept-Language: fi\r\n"
    "Authorization: Basic dXNlcjE6c2VjcmV0\r\n"
    "Expect: 100-continue\r\n"
    "From: user@nokia.com\r\n"
    "Host: www.nokia.com:80\r\n"
    "If-Match: \"entity_tag001\"\r\n"
    "If-Modified-Since: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "If-None-Match: \"entity_tag001\", \"tag02\"\r\n"
    "If-Range: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "If-Unmodified-Since: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "Location: a+a+://\r\n"
    "Max-Forwards: 3\r\n"
    "Proxy-Authorization: Basic dXNlcjE6c2VjcmV0\r\n"
    "Range: bytes=100-599\r\n"
    "Referer: http://www.microsoft.com/resources.asp\r\n"
    "TE: trailers\r\n"
    "User-Agent: Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 5.0)\r\n"
    "Cookie: $Version=\"1\";user=\"WILE_E_COYOTE\";$Path=\"/acme\"\r\n"
    "Cache-Control: max-age=10\r\n"
    "Pragma: no-cache\r\n"
    "Transfer-Encoding: chunked, deflate\r\n"
    "Upgrade: SHTTP/1.3, TLS/1.0\r\n"
    "Via: HTTP/1.1 Proxy1\r\n"
    "Proxy-Connection: keep-alive\r\n"
    "Connection: close\r\n"
    "Date: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "Trailer: Date, Connection\r\n"
    "Warning: 2112 www.nokia.com \"Disconnected Operation\"\r\n"
    /* This is here just because we cannot include two Retry-After
       headers in a single message */
    "Retry-After: 60\r\n"
    "\r\n"
    );
  http = http_object(msg);

  TEST_1(msg); TEST_1(http); TEST_1(!http->http_error);

  for (h = (http_header_t *)http->http_request; h; h = h->sh_succ) {

    if (h == (http_header_t*)http->http_payload)
      break;

    TEST_1(h1 = msg_header_dup(home, h));
    n = msg_header_e(b, sizeof b, h1, 0);
    if (n != h->sh_len)
      TEST_SIZE(n, h->sh_len);
    TEST_M(b, h->sh_data, n);
    su_free(home, h1);
  }

  msg_destroy(msg), msg = NULL;

  msg = read_message(
    "HTTP/1.1 200 Ok\r\n"
    "Accept-Ranges: none\r\n"
    "Age: 2147483648\r\n"
    "ETag: \"b38b9-17dd-367c5dcd\"\r\n"
    "Location: http://localhost/redirecttarget.asp\r\n"
    "Proxy-Authenticate: Basic realm=\"Nokia Internet Proxy\"\r\n"
    "Retry-After: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "Server: Microsoft-IIS/5.0\r\n"
    "Vary: Date\r\n"
    "WWW-Authenticate: Digest realm=\"Nokia Intranet\", nonce=\"3wWGOvaWn3n+hFv8PK2ABQ==\", opaque=\"+GNywA==\", algorithm=MD5, qop=\"auth-int\"\r\n"
    "Set-Cookie: user=\"WILE_E_COYOTE\";Version=\"1\";Path=\"/acme\"\r\n"
    "Allow: GET, HEAD\r\n"
    /* This is here just because we cannot include two If-Range
       headers in a single message */
    "If-Range: \"tag02\"\r\n"
    "Content-Encoding: gzip\r\n"
    "Content-Language: en\r\n"
    "Content-Length: 70\r\n"
    "Content-Location: http://localhost/page.asp\r\n"
    "Content-MD5: LLO7gLaGqGt4BI6HouiWng==\r\n"
    "Content-Range: bytes 2543-4532/7898\r\n"
    "Content-Type: text/html\r\n"
    "Expires: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "Last-Modified: Tue, 11 Jul 2000 18:23:51 GMT\r\n"
    "\r\n"
    "<html><head><title>Heippa!</title></head><body>Heippa!</body></html>"
    "\r\n");
  http = http_object(msg);

  TEST_1(msg); TEST_1(http); TEST_1(!http->http_error);

  for (h = (http_header_t *)http->http_status; h; h = h->sh_succ) {
    if (h == (http_header_t*)http->http_payload)
      break;

    TEST_1(h1 = msg_header_dup(home, h));
    n = msg_header_e(b, sizeof b, h1, 0);
    TEST_SIZE(n, h->sh_len);
    TEST_M(b, h->sh_data, n);
    su_free(home, h1);
  }

  msg_destroy(msg), msg = NULL;

  su_home_check(home);
  su_home_zap(home);

  END();
}


static int http_chunk_test(void)
{
  msg_t *msg;
  http_t *http;

  BEGIN();

  {
    char data[] =
      "\nHTTP/1.1 200 OK\r\n"
      "Server: Apache/1.3.11 (Unix) tomcat/1.0\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"
      "4c\r\n"
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n"
      "\r\n"
      "<html>\r\n"
      "\r\n"
      "\r\n9\r\n"
      "</html>\r\n"
      "\r\n0\r\n"
      "Date: Mon, 21 Oct 2002 13:10:41 GMT\r\n"
      "\n";

    TEST_1(msg = read_message_byte_by_byte(data));

    http = msg_object(msg); TEST_1(http);

    TEST_1(http->http_status);
    TEST_1(http->http_payload);
    TEST_1(http->http_payload->pl_next);
    TEST_1(http->http_date);

    msg_destroy(msg);
  }

  {
    /* Use LF only as line delimiter */
    char data[] =
      "HTTP/1.1 200 OK\n"
      "Server: Apache/1.3.11 (Unix) tomcat/1.0\n"
      "Transfer-Encoding: chunked\n"
      "Content-Type: text/html\n"
      "\n"
      "48\n"
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
      "\n"
      "<html>\n"
      "\n"
      "\n8\n"
      "</html>\n"
      "\n0\n"
      "Date: Mon, 21 Oct 2002 13:10:41 GMT\n"
      "\n";

    TEST_1(msg = read_message_byte_by_byte(data));

    http = msg_object(msg); TEST_1(http);

    TEST_1(http->http_status);
    TEST_1(http->http_payload);
    TEST_1(http->http_payload->pl_next);
    TEST_1(http->http_date);

    msg_destroy(msg);
  }

  {
    /* Use CR only as line delimiter */
    char data[] =
      "HTTP/1.1 200 OK\r"
      "Server: Apache/1.3.11 (Unix) tomcat/1.0\r"
      "Transfer-Encoding: chunked\r"
      "Content-Type: text/html\r"
      "\r"
      "48\r"
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r"
      "\r"
      "<html>\r"
      "\r"
      "\r8\r"
      "</html>\r"
      "\r0\r"
      "Date: Mon, 21 Oct 2002 13:10:41 GMT\r"
      "\r";

    TEST_1(msg = read_message_byte_by_byte(data));

    http = msg_object(msg); TEST_1(http);

    TEST_1(http->http_status);
    TEST_1(http->http_payload);
    TEST_1(http->http_payload->pl_next);
    TEST_1(http->http_date);

    msg_destroy(msg);
  }

  END();
}

static int http_tag_test(void)
{
  BEGIN();

  {
    msg_t *msg;
    http_t *http;

    http_referer_t *r;
    http_upgrade_t *u;

    char data[] =
      "HTTP/1.1 200 OK\r\n"
      "Server: Apache/1.3.11 (Unix) tomcat/1.0\r\n"
      "\r\n";

    TEST_1(msg = read_message_byte_by_byte(data));

    http = msg_object(msg); TEST_1(http);

    TEST_1(http->http_status);
    TEST_1(http->http_server);

    r = http_referer_make(NULL, "ftp://ftp.funet.fi"); TEST_1(r);
    u = http_upgrade_make(NULL, "HTTP/1.1, TLS/1.1"); TEST_1(u);

    TEST_1(u->k_items);
    TEST_S(u->k_items[0], "HTTP/1.1");
    TEST_S(u->k_items[1], "TLS/1.1");
    TEST_1(!u->k_items[2]);

    TEST(http_add_tl(msg, http,
		     HTTPTAG_SERVER(HTTP_NONE->sh_server),
		     HTTPTAG_USER_AGENT_STR(NULL),
		     HTTPTAG_USER_AGENT(NULL),
		     HTTPTAG_REFERER(r),
		     HTTPTAG_MAX_FORWARDS_STR("1"),
		     HTTPTAG_HEADER((http_header_t*)u),
		     HTTPTAG_HEADER_STR("Vary: *\r\n\r\nfoo"),
		     TAG_END()),
	 5);

    TEST_P(http->http_server, NULL);
    TEST_1(http->http_referer);
    TEST_1(http->http_max_forwards);
    TEST_1(http->http_upgrade);
    TEST_1(http->http_vary);
    TEST_1(http->http_payload);

    msg_destroy(msg);
  }

  END();
}

static
int test_query_parser(void)
{
  BEGIN();

  {
    char query[] = "foo=bar&bar=baz";
    char *foo = NULL, *bar = NULL, *baz = "default";

    TEST_SIZE(http_query_parse(NULL, NULL), -1);

    TEST_SIZE(http_query_parse(query,
			       "foo=", &foo,
			       "bar=", &bar,
			       "baz=", &baz,
			       NULL), 2);
    TEST_S(foo, "bar");
    TEST_S(bar, "baz");
    TEST_S(baz, "default");
  }

  {
    char q2[] = "f%6fo=b%61r&bar=baz&bazibazuki";

    char *foo = NULL, *bar = NULL, *baz = NULL;

    TEST_SIZE(http_query_parse(q2,
			       "foo=", &foo,
			       "bar=", &bar,
			       "baz", &baz,
			       NULL),
	      3);

    TEST_S(foo, "bar");
    TEST_S(bar, "baz");
    TEST_S(baz, "ibazuki");

  }

  END();
}
