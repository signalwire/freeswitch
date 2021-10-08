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

/**@file test_nth.c
 * @brief Tests for nth module
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Oct 22 20:52:37 2002 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#if HAVE_ALARM
#include <unistd.h>
#include <signal.h>
#endif

typedef struct tester tester_t;
typedef struct site site_t;
typedef struct client client_t;

#define SU_ROOT_MAGIC_T tester_t

#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_wait.h>

#define NTH_CLIENT_MAGIC_T client_t
#define NTH_SITE_MAGIC_T site_t

#include "sofia-sip/nth.h"
#include <sofia-sip/http_header.h>
#include <sofia-sip/msg_mclass.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/auth_module.h>

int tstflags = 0;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

char const name[] = "test_nth";

static int init_test(tester_t *t);
static int deinit_test(tester_t *t);
static int test_nth_client_api(tester_t *t);
static int test_nth_server_api(tester_t *t);
static int init_server(tester_t *t);
static int test_requests(tester_t *t);
static int init_engine(tester_t *t);

struct site
{
  site_t       *s_next, *s_parent;
  tester_t     *s_tester;
  url_string_t *s_url;
  nth_site_t   *s_ns;
  int           s_called;
  int           s_status;
  char const   *s_phrase;
  tagi_t       *s_tags;
};

struct client
{
  unsigned      c_status;
};

struct tester
{
  su_home_t     t_home[1];
  su_root_t    *t_root;
  msg_mclass_t *t_mclass;
  url_string_t *t_proxy;
  nth_engine_t *t_engine;

  char const   *t_srcdir;
  char const   *t_pem;

  su_sockaddr_t t_addr[1];
  socklen_t     t_addrlen;

  su_socket_t   t_sink;
  url_string_t *t_sinkuri;
  su_sockaddr_t t_sinkaddr[1];
  socklen_t     t_sinkaddrlen;

  site_t       *t_sites;
  site_t       *t_master;
};

static int test_site(site_t *t,
		     nth_site_t *server,
		     nth_request_t *req,
		     http_t const *http,
		     char const *path);

static site_t *site_create(tester_t *t, site_t *parent,
			   char const *url,
			   int status, char const *phrase,
			   tag_type_t tag, tag_value_t value, ...)
{
  nth_site_t *pns = parent ? parent->s_ns : NULL;
  site_t *s;
  ta_list ta;

  if (url == NULL)
    return NULL;

  s = su_zalloc(t->t_home, sizeof *s);
  if (s == NULL)
    return NULL;

  s->s_url = URL_STRING_MAKE(url);
  s->s_tester = t;
  s->s_next = t->t_sites;
  s->s_status = status;
  s->s_phrase = phrase;

  ta_start(ta, tag, value);

  s->s_tags = tl_adup(t->t_home, ta_args(ta));
  if (s->s_tags)
    s->s_ns = nth_site_create(pns, test_site, s,
			      (url_string_t *)s->s_url,
			      NTHTAG_ROOT(t->t_root),
			      ta_tags(ta));

  ta_end(ta);

  if (s->s_ns == NULL)
    return NULL;

  t->t_sites = s;

  return s;
}

static int init_test(tester_t *t)
{
  su_socket_t s;

  BEGIN();

  t->t_root = su_root_create(t); TEST_1(t->t_root);
  t->t_mclass = msg_mclass_clone(http_default_mclass(), 0, 0);
  TEST_1(t->t_mclass);

  t->t_addr->su_len = (sizeof t->t_addr->su_sin);
  s = su_socket(t->t_addr->su_family = AF_INET, SOCK_STREAM, 0);
  TEST_1(s != INVALID_SOCKET);
  TEST_1(su_inet_pton(AF_INET, "127.0.0.1", &t->t_addr->su_sin.sin_addr) >= 0);
  TEST_1(bind(s, &t->t_addr->su_sa,
	      t->t_addrlen = (sizeof t->t_addr->su_sin)) != -1);
  TEST_1(getsockname(s, &t->t_addr->su_sa, &t->t_addrlen) != -1);
  TEST_1(t->t_addr->su_port != 0);
  TEST_1(su_close(s) != -1);

  t->t_pem = su_sprintf(t->t_home, "%s/agent.pem", t->t_srcdir);

  END();
}

static int deinit_test(tester_t *t)
{
  site_t *s, *s_next;

  BEGIN();

  nth_engine_destroy(t->t_engine);

  for (s = t->t_sites; s; s = s_next) {
    s_next = s->s_next;
    nth_site_destroy(s->s_ns), s->s_ns = NULL;
    su_free(t->t_home, s);
  }

  su_root_destroy(t->t_root);

  su_home_deinit(t->t_home);

  memset(t, 0, sizeof t);

  END();
}


static int test_nth_client_api(tester_t *t)
{
  char const *s;

  BEGIN();

  s = nth_engine_version();
  TEST_1(s); TEST_1(strlen(s)); TEST_S(s, "sofia-http-client/" NTH_CLIENT_VERSION);

  TEST_1(nth_engine_create(NULL, TAG_END()) == NULL);
  TEST(errno, EINVAL);
  TEST_VOID(nth_engine_destroy(NULL));
  TEST_1(nth_engine_get_params(NULL, TAG_END()) == -1);
  TEST_1(nth_engine_set_params(NULL, TAG_END()) == -1);
  TEST_1(!nth_client_tcreate(NULL, NULL, NULL,
			     HTTP_METHOD_OPTIONS,
			     URL_STRING_MAKE("*"),
			     TAG_END()));
  TEST(nth_client_status(NULL), 400);
  TEST(nth_client_method(NULL), http_method_invalid);
  TEST(nth_client_is_streaming(NULL), 0);
  TEST_P(nth_client_url(NULL), NULL);
  TEST_P(nth_client_request(NULL), NULL);
  TEST_P(nth_client_response(NULL), NULL);
  TEST_VOID(nth_client_destroy(NULL));

  t->t_engine = nth_engine_create(t->t_root,
				  NTHTAG_ERROR_MSG(2),
				  NTHTAG_MCLASS(t->t_mclass),
				  NTHTAG_MFLAGS(MSG_DO_CANONIC|MSG_DO_COMPACT),
				  NTHTAG_STREAMING(0),
				  NTHTAG_PROXY("http://localhost:8888"),
				  TAG_END());
  TEST_1(t->t_engine);

  {
    int error_msg = -1;
    msg_mclass_t const *mclass = (void *)-1;
    int mflags = -1;
    unsigned expires = -1;
    int streaming = -1;
    url_string_t const *proxy = (void *)-1;

    char *proxy_str;

    TEST(nth_engine_get_params(t->t_engine,
			       NTHTAG_ERROR_MSG_REF(error_msg),
			       NTHTAG_MCLASS_REF(mclass),
			       NTHTAG_MFLAGS_REF(mflags),
			       NTHTAG_EXPIRES_REF(expires),
			       NTHTAG_STREAMING_REF(streaming),
			       NTHTAG_PROXY_REF(proxy),
			       TAG_END()),
	 6);

    TEST(error_msg, 1);
    TEST_P(mclass, t->t_mclass);
    TEST(mflags, MSG_DO_CANONIC|MSG_DO_COMPACT);
    TEST(expires, 32000);
    TEST(streaming, 0);
    TEST_1(proxy != NULL);
    TEST_1(proxy_str = url_as_string(t->t_home, proxy->us_url));
    TEST_S(proxy_str, "http://localhost:8888");

    proxy = URL_STRING_MAKE("http://127.0.0.1:80");

    TEST(nth_engine_set_params(t->t_engine,
			       NTHTAG_ERROR_MSG(0),
			       NTHTAG_MCLASS(http_default_mclass()),
			       NTHTAG_MFLAGS(0),
			       NTHTAG_EXPIRES(10000),
			       NTHTAG_STREAMING(2),
			       NTHTAG_PROXY(proxy),
			       TAG_END()),
	 6);

    error_msg = -1;
    mclass = (void *)-1;
    mflags = -1;
    expires = (unsigned)-1;
    streaming = -1;
    proxy = (void *)-1;

    TEST(nth_engine_get_params(t->t_engine,
			       NTHTAG_ERROR_MSG_REF(error_msg),
			       NTHTAG_MCLASS_REF(mclass),
			       NTHTAG_MFLAGS_REF(mflags),
			       NTHTAG_EXPIRES_REF(expires),
			       NTHTAG_STREAMING_REF(streaming),
			       NTHTAG_PROXY_REF(proxy),
			       TAG_END()),
	 6);

    TEST(error_msg, 0);
    TEST_P(mclass, NULL);
    TEST(mflags, 0);
    TEST(expires, 10000);
    TEST(streaming, 1);
    TEST_1(proxy != NULL);
    TEST_1(proxy_str = url_as_string(t->t_home, proxy->us_url));
    TEST_S(proxy_str, "http://127.0.0.1:80");
  }

  TEST_1(nth_engine_get_stats(NULL, TAG_END()) == -1);

  {
    msg_t *msg;
    http_t *http;

    TEST_1(nth_engine_msg_create(NULL, -1) == NULL);
    TEST_1(msg = nth_engine_msg_create(t->t_engine, -1));
    TEST_1(http = http_object(msg));
    TEST(http->http_flags, MSG_FLG_USERMASK);
    msg_destroy(msg);

    /* Use mflags set by set_params (+ streaming flag) */
    TEST_1(msg = nth_engine_msg_create(t->t_engine, 0));
    TEST_1(http = http_object(msg));
    TEST(http->http_flags, MSG_FLG_STREAMING | t->t_mclass->mc_flags);
    msg_destroy(msg);
  }

  TEST_VOID(nth_engine_destroy(t->t_engine));
  t->t_engine = NULL;

  END();
}

static int site_check_all(site_t *t,
			  nth_site_t *server,
			  nth_request_t *req,
			  http_t const *http,
			  char const *path);

static int test_nth_server_api(tester_t *t)

{
  char const *v;
  site_t s[1];

  BEGIN();

  memset(s, 0, sizeof s);

  v = nth_site_server_version();
  TEST_1(v); TEST_1(strlen(v)); TEST_S(v, "nth/" NTH_SERVER_VERSION);

  /* Fails because no parent site, no root */
  TEST_1(!nth_site_create(NULL, test_site, s,
			  URL_STRING_MAKE("http://127.0.0.1:8888"),
			  TAG_END()));

  /* Fails because url specifies both host and path */
  TEST_1(!nth_site_create(NULL, site_check_all, s,
			  URL_STRING_MAKE("http://127.0.0.1:8888/foo/"),
			  NTHTAG_ROOT(t->t_root), TAG_END()));

  TEST_VOID(nth_site_destroy(NULL));
  TEST_P(nth_site_magic(NULL), NULL);
  TEST_VOID(nth_site_bind(NULL, test_site, s));
  TEST_1(nth_site_set_params(NULL, TAG_END()) == -1);
  TEST_1(nth_site_get_params(NULL, TAG_END()) == -1);
  TEST_1(nth_site_get_stats(NULL, TAG_END()) == -1);
  TEST(nth_request_status(NULL), 400);
  TEST(nth_request_method(NULL), http_method_invalid);
  TEST_P(nth_request_message(NULL), NULL);
  TEST_1(nth_request_treply(NULL, HTTP_200_OK, TAG_END()) == -1);
  TEST_VOID(nth_request_destroy(NULL));

  END();
}

static int test_site(site_t *s,
		     nth_site_t *ns,
		     nth_request_t *req,
		     http_t const *http,
		     char const *path)
{
  if (s == NULL || ns == NULL || req == NULL)
    return 500;

  TEST_1(nth_request_treply(req, s->s_status, s->s_phrase,
			    TAG_NEXT(s->s_tags)) != -1);

  TEST_VOID(nth_request_destroy(req));

  return s->s_status;
}


static int site_check_all(site_t *s,
			  nth_site_t *ns,
			  nth_request_t *req,
			  http_t const *http,
			  char const *path)
{
  msg_t *msg;
  auth_status_t *as;

  TEST_1(s); TEST_1(ns); TEST_1(req); TEST_1(http); TEST_1(path);

  if (s == NULL || ns == NULL || req == NULL)
    return 500;

  TEST(nth_request_status(req), 0);
  TEST(nth_request_method(req), http_method_get);
  TEST_1(msg = nth_request_message(req));

  msg_destroy(msg);

  as = nth_request_auth(req);

  TEST_1(nth_request_treply(req, s->s_status, s->s_phrase,
			    TAG_NEXT(s->s_tags)) != -1);

  TEST_VOID(nth_request_destroy(req));

  return s->s_status;
}

static char passwd_name[] = "tmp_sippasswd.XXXXXX";

static void remove_tmp(void)
{
  if (passwd_name[0])
    unlink(passwd_name);
}

static char const passwd[] =
  "alice:secret:\n"
  "bob:secret:\n"
  "charlie:secret:\n";

static int init_server(tester_t *t)
{
  BEGIN();

  site_t *m = t->t_master, *sub2;
  auth_mod_t *am;
  int temp;

  TEST_1(t->t_master = m =
	 site_create(t, NULL,
		     su_sprintf(t->t_home, "HTTP://127.0.0.1:%u",
				htons(t->t_addr->su_port)),
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR("<html><body>Hello</body></html>\n"),
		     TPTAG_CERTIFICATE(t->t_pem),
		     TAG_END()));

  TEST_1(site_create(t, m, "/sub/sub",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR
		     ("<html><body>sub/sub</body></html>\n"),
		     TAG_END()));

  TEST_1(site_create(t, m, "/sub/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR("<html><body>sub/</body></html>\n"),
		     TAG_END()));

  TEST_1(site_create(t, m, "/sub/sub/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR
		     ("<html><body>sub/sub/</body></html>\n"),
		     TAG_END()));

  TEST_1(sub2 =
	 site_create(t, m, "/sub2/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR("<html><body>sub2/</body></html>\n"),
		     TAG_END()));

  TEST_1(site_create(t, sub2, "sub/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR
		     ("<html><body>sub2/sub/</body></html>\n"),
		     TAG_END()));


#ifndef _WIN32
  temp = mkstemp(passwd_name);
#else
  temp = open(passwd_name, O_WRONLY|O_CREAT|O_TRUNC, 666);
#endif
  TEST_1(temp != -1);
  atexit(remove_tmp);		/* Make sure temp file is unlinked */

  TEST_SIZE(write(temp, passwd, strlen(passwd)), strlen(passwd));

  TEST_1(close(temp) == 0);

  am = auth_mod_create(t->t_root,
		       AUTHTAG_METHOD("Digest"),
		       AUTHTAG_REALM("auth"),
		       AUTHTAG_DB(passwd_name),
		       TAG_END());
  TEST_1(am);

  TEST_1(site_create(t, m, "auth/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR
		     ("<html><body>auth/</body></html>\n"),
		     NTHTAG_AUTH_MODULE(am),
		     TAG_END()));

  auth_mod_unref(am);


  am = auth_mod_create(t->t_root,
		       AUTHTAG_METHOD("Delayed+Basic"),
		       AUTHTAG_REALM("auth2"),
		       AUTHTAG_DB(passwd_name),
		       TAG_END());
  TEST_1(am);

  TEST_1(site_create(t, m, "auth2/",
		     HTTP_200_OK,
		     HTTPTAG_CONTENT_TYPE_STR("text/html"),
		     HTTPTAG_PAYLOAD_STR
		     ("<html><body>auth/</body></html>\n"),
		     NTHTAG_AUTH_MODULE(am),
		     TAG_END()));

  auth_mod_unref(am);

  END();
}

static int send_request(tester_t *t, char const *req, size_t reqlen,
			int close_socket,
			char reply[], int rlen,
			int *return_len)
{
  static su_socket_t c = INVALID_SOCKET;
  int m, r;
  su_wait_t w[1];

  BEGIN();

  if (c == INVALID_SOCKET) {
    c = su_socket(t->t_addr->su_family, SOCK_STREAM, 0); TEST_1(c != SOCK_STREAM);
    TEST_1(su_setblocking(c, 1) != -1);
    TEST_1(connect(c, &t->t_addr->su_sa, t->t_addrlen) != -1);

    while (su_root_step(t->t_root, 1) == 0);
  }

  if (reqlen == (size_t)-1)
    reqlen = strlen(req);

  TEST_SIZE(su_send(c, req, reqlen, 0), reqlen);

  if (close_socket == 1)
    shutdown(c, 1);

  TEST(su_wait_create(w, c, SU_WAIT_IN), 0);

  while (su_root_step(t->t_root, 1) == 0 || su_wait(w, 1, 0) < 0);

  for (r = 0;;) {
    TEST_1((m = recv(c, reply, rlen - r - 1, 0)) != -1);
    r += m;
    if (m == 0 || r == rlen - 1)
      break;
  }
  reply[r] = '\0';

  if (close_socket != -1)
    su_close(c), c = -1;

  *return_len = r;

  END();
}

int sspace(char const *buffer)
{
  int m = strcspn(buffer, " ");

  if (buffer[m])
    m += 1 + strcspn(buffer + m + 1, " ");

  return m;
}

#define CRLF "\r\n"

static int test_requests(tester_t *t)
{
  char buffer[4096 + 1];
  int m;

  BEGIN();

  {
    static char const get[] =
      "GET / HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      CRLF;

    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);

    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 200");
  }

  {
    static char const get[] =
      "GET / HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      CRLF;

    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);

    m = strcspn(buffer, CRLF); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
  }

  {
    static char const request[] =
      "GET %s HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      CRLF;
    char *get;

    get = su_sprintf(NULL, request, "/sub");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 301");
    m += strcspn(buffer + m, CRLF) + 1;
    free(get);

    get = su_sprintf(NULL, request, "/sub/");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = strcspn(buffer, CRLF); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
    TEST_1(strstr(buffer + m, "<body>sub/</body>"));
    free(get);

    get = su_sprintf(NULL, request, "/sub2/");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = strcspn(buffer, CRLF); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
    TEST_1(strstr(buffer + m, "<body>sub2/</body>"));
    free(get);

    get = su_sprintf(NULL, request, "/sub2/hub");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = strcspn(buffer, CRLF); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
    TEST_1(strstr(buffer + m, "<body>sub2/</body>"));
    free(get);

    /* Test that absolute path for subdir site is calculated correctly */
    get = su_sprintf(NULL, request, "/sub2/sub");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 301");
    TEST_1(strstr(buffer + m, "/sub2/sub/" CRLF));
    free(get);

    get = su_sprintf(NULL, request, "/sub2/sub/");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = strcspn(buffer, CRLF); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
    TEST_1(strstr(buffer + m, "<body>sub2/sub/</body>"));
    free(get);

    get = su_sprintf(NULL, request, "/sub/sub");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = strcspn(buffer, CRLF); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200 OK");
    TEST_1(strstr(buffer + m, "<body>sub/sub</body>"));
    free(get);

    get = su_sprintf(NULL, request, "/auth/");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 401");
    free(get);

    get = su_sprintf(NULL, request, "/auth2/");
    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 401");
    free(get);
  }

  {
    static char const get[] =
      "GET /auth2/ HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      /* alice:secret in base64 */
      "Authorization: Basic YWxpY2U6c2VjcmV0" CRLF
      CRLF;

    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m++] = '\0';
    TEST_S(buffer, "HTTP/1.1 200");
  }

  {
    static char const kuik[] =
      "kuik" CRLF CRLF;

    TEST(send_request(t, kuik, -1, 0, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 400");
  }

  {
    static char const kuik[] =
      "POST / HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "Content-Length: 4294967296" CRLF
      CRLF;

    TEST(send_request(t, kuik, -1, 1, buffer, sizeof(buffer), &m), 0);
    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 400");
  }

  {
    static char const get[] =
      "GET / HTTP/10.10" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      CRLF;

    TEST(send_request(t, get, -1, 0, buffer, sizeof(buffer), &m), 0);

    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 505");
  }

  {
    static char const get[] =
      "GET /" CRLF;

    TEST(send_request(t, get, -1, 1, buffer, sizeof(buffer) - 1, &m), 0);

    buffer[6] = '\0';
    TEST_S(buffer, "<html>");
  }

  if (0)
  {
    static char const post[] =
      "POST /foo HTTP/1.1" CRLF
      "Host: 127.0.0.1" CRLF
      "User-Agent: Test-Tool" CRLF
      "Connection: close" CRLF
      "Content-Length: 7" CRLF
      "Expect: 100-continue" CRLF
      CRLF;
    static char const body[] =
      "<html/>";

    TEST(send_request(t, post, -1, -1, buffer, sizeof(buffer) - 1, &m), 0);

    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 100");

    TEST(send_request(t, body, -1, 0, buffer, sizeof(buffer) - 1, &m), 0);

    m = sspace(buffer); buffer[m] = '\0';
    TEST_S(buffer, "HTTP/1.1 200");
  }

  END();
}


static int init_engine(tester_t *t)
{
  BEGIN();
  su_socket_t s;

  t->t_engine = nth_engine_create(t->t_root,
				  NTHTAG_STREAMING(0),
				  TAG_END());
  TEST_1(t->t_engine);

  t->t_sink = s = su_socket(AF_INET, SOCK_STREAM, 0); TEST_1(s != -1);
  TEST(bind(s, &t->t_sinkaddr->su_sa,
	    t->t_sinkaddrlen = (sizeof t->t_sinkaddr->su_sin)),
       0);
  TEST_1(getsockname(s, &t->t_sinkaddr->su_sa, &t->t_sinkaddrlen) != -1);
  TEST(listen(t->t_sink, 5), 0);

  TEST_1(t->t_sinkuri = (url_string_t *)
	 su_sprintf(t->t_home, "HTTP://127.0.0.1:%u",
		    htons(t->t_sinkaddr->su_port)));

  END();
}


static int response_to_client(client_t *c,
			      nth_client_t *hc,
			      http_t const *http)
{
  if (http) {
    c->c_status = http->http_status->st_status;
  }
  else {
    c->c_status = nth_client_status(hc);
  }

  return 0;
}


static int test_client(tester_t *t)
{
  BEGIN();

  nth_client_t *hc;
  char *uri;
  client_t client[1];

  memset(client, 0, sizeof client);

  TEST_1(uri = su_strcat(NULL, t->t_master->s_url->us_str, "/"));
  TEST_1(hc = nth_client_tcreate(t->t_engine,
				 response_to_client, client,
				 HTTP_METHOD_GET,
				 URL_STRING_MAKE(uri),
				 TAG_END()));
  while (client->c_status == 0) su_root_step(t->t_root, 1);
  TEST(client->c_status, 200);
  nth_client_destroy(hc);
  su_free(NULL, uri);

  memset(client, 0, sizeof client);

  TEST_1(hc = nth_client_tcreate(t->t_engine,
				 response_to_client, client,
				 HTTP_METHOD_GET,
				 URL_STRING_MAKE(t->t_sinkuri),
				 NTHTAG_EXPIRES(1000),
				 TAG_END()));
  while (client->c_status == 0) su_root_step(t->t_root, 1);
  TEST(client->c_status, 408);
  nth_client_destroy(hc);

  END();
}
#if HAVE_ALARM
static RETSIGTYPE sig_alarm(int s)
{
  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}
#endif

void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v|-q] [-a] [-p proxy-uri]\n", name);
  exit(exitcode);
}

int main(int argc, char **argv)
{
  int i;
  int retval = 0;
  int o_alarm = 1;

  tester_t t[1] = {{{ SU_HOME_INIT(t) }}};

  char const *srcdir = getenv("srcdir");

  if (srcdir == NULL)
    srcdir = ".";

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "-q") == 0)
      tstflags &= ~tst_verbatim;
    else if (strcmp(argv[i], "-p") == 0 && argv[i + 1])
      t->t_proxy = (url_string_t *)argv[++i];
    else if (strcmp(argv[i], "-s") == 0 && argv[i + 1])
      srcdir = argv[++i];
    else if (strcmp(argv[i], "--no-alarm") == 0) {
      o_alarm = 0;
    }
    else if (strcmp(argv[i], "-") == 0) {
      i++; break;
    }
    else if (argv[i][0] != '-') {
      break;
    }
    else
      usage(1);
  }

  t->t_srcdir = srcdir;

#if HAVE_ALARM
  if (o_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

  su_init();

  retval |= init_test(t);
  retval |= test_nth_client_api(t);
  retval |= test_nth_server_api(t);
  retval |= init_server(t);
  retval |= test_requests(t);
  retval |= init_engine(t);
  retval |= test_client(t);
  retval |= deinit_test(t);

  su_deinit();

  return retval;
}
