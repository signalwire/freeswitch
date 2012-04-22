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

/**@file http-client.c  Simple HTTP tool.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Fri Mar 30 12:05:21 2001 ppessi
 */

#include "config.h"

/**@page http_client Make HTTP request
 *
 * @par Name    
 * http-client - HTTP request tool
 *
 * @par Synopsis
 *
 * <tt>http-client [OPTIONS] url</tt>
 *
 * @par Description
 *
 * The @em http-client utility sends a HTTP request to an HTTP server or proxy.
 *
 * @par
 *
 * The @em http-client tool will print out status line and interesting
 * headers from the response. The message body is also printed.
 *
 * @par Options
 *
 * The @e http-client utility accepts following command line options:
 * <dl>
 * <dt>--method=name</dt>
 * <dd>Specify the request method name (GET by default).
 * </dd>
 * <dt>--proxy=url</dt>
 * <dd>Specifies the proxy via which the request will be sent.
 * </dd>
 * <dt>--ua=value</dt>
 * <dd>Specifies the User-Agent header field.
 * </dd>
 * <dt>--mf=n</dt>
 * <dd>Specify the initial Max-Forwards count.
 * </dd>
 * <dt>--pipe</dt>
 * <dd>Use pipelining (do not shutdown client connection after request).
 * </dd>
 * <dt>--extra</dt>
 * <dd>Insert standard input to the requests.
 * </dd>
 * </dl>
 *
 * @par Examples
 *
 * You want to query supported features of http://connecting.nokia.com:
 * @code
 * $ http-client --method OPTIONS http://connecting.nokia.com
 * @endcode
 *
 * @par Environment
 * @c NTH_DEBUG, @c TPORT_DEBUG, @c TPORT_LOG.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * <hr>
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef struct context_s context_t;
#define NTH_CLIENT_MAGIC_T context_t

#include <sofia-sip/nth.h>
#include <sofia-sip/http_header.h>
#include <sofia-sip/http_tag.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/auth_client.h>

struct context_s {
  su_home_t   	  c_home[1];
  su_root_t   	 *c_root;
  nth_engine_t 	 *c_engine;
  nth_client_t   *c_clnt;
  char const     *c_user;
  char const     *c_pass;
  auth_client_t  *c_auth;
  int             c_pre;
  int             c_pending;
};

static
char const name[] = "http-client";

static
int header_print(FILE *stream, char const *fmt, http_header_t const *h)
{
  char s[1024];

  msg_header_field_e(s, sizeof(s), (msg_header_t*)h, 0);
  s[sizeof(s) - 1] = '\0';

  if (fmt && strcmp(fmt, "%s"))
    return fprintf(stream, fmt, s);
  if (fputs(s, stream) >= 0)
    return strlen(s);
  return -1;
}

static
int payload_print(FILE *stream, msg_payload_t const *pl)
{
  for (; pl; pl = pl->pl_next) {
    fprintf(stream, "%.*s", (int)pl->pl_len, pl->pl_data);
  }
  return 0;
}

static
char *read_file(FILE *stream)
{
  int n;
  char *buf;
  off_t used, size;

  if (stream == NULL) {
    errno = EINVAL;
    return NULL;
  }

  /* Read block by block */
  used = 0;
  size = 512;
  buf = malloc(size);

  while (buf) {
    n = fread(buf + used, 1, size - used - 1, stream);
    used += n;
    if (n < size - used - 1) {
      if (feof(stream))
	;
      else if (ferror(stream))
	free(buf), buf = NULL;
      break;
    }
    buf = realloc(buf, 2 * size);
  }

  if (buf)
    if (used < size)
      buf[used] = '\0';

  return buf;
}

char const _usage[] =
"usage: %s [OPTIONS] url\n"
"       where OPTIONS are as follows\n"
"       --method=name\n"
"       --proxy=url\n"
"       --user=user:password\n"
"       --ua=value\n"
"       --mf=n\n"
"       --pipe\n"
"       --extra\n";

static
void usage(int rc)
{
  fprintf(stderr, _usage, name);
  exit(rc);
}

static int response(context_t *context,
			       nth_client_t *oreq,
			       http_t const *http);

int main(int argc, char *argv[])
{
  su_home_t *home;
  context_t context[1] = {{{SU_HOME_INIT(context)}}};
  http_method_t method;
  char
    *o_proxy = NULL,
    *o_user = NULL,
    *o_max_forwards = NULL,
    *o_method_name = "GET",
    *o_user_agent = "http-client/1.0 " "nth/" NTH_VERSION;
  int
    o_pipe = 0, o_extra = 0;

  char *extra = NULL;
  char *v;

#define MATCH(s, o) \
      ((strcmp(s, o) == 0))
#define MATCH1(s, o) \
      ((strncmp(s, o, strlen(o)) == 0) && \
       (v = (s[strlen(o)] ? s + strlen(o) : argv++[1])))
#define MATCH2(s, o) \
      ((strncmp(s, o, strlen(o)) == 0) && \
       (s[strlen(o)] == '=' || s[strlen(o)] == '\0') && \
       (v = s[strlen(o)] ? s + strlen(o) + 1 : argv++[1]))

  while ((v = argv++[1])) {
    if (v[0] != '-')                 { argv--; break; }
    else if (MATCH(v, "-"))          { break; }
    else if (MATCH2(v, "--method"))  { o_method_name = v;  continue; }
    else if (MATCH2(v, "--mf"))      { o_max_forwards = v; continue; }
    else if (MATCH2(v, "--proxy"))   { o_proxy = v;        continue; }
    else if (MATCH2(v, "--user"))    { o_user = v;         continue; }
    else if (MATCH2(v, "--ua"))      { o_user_agent = v;   continue; }
    else if (MATCH(v, "--pipe"))     { o_pipe = 1;         continue; }
    else if (MATCH(v, "--extra"))    { o_extra = 1;        continue; }
    else if (MATCH(v, "--help"))     { usage(0);           continue; }
    else
      usage(1);
  }

  if (!argv[1])
    usage(1);

  method = http_method_code(o_method_name);

  if (o_user) {
    char *pass = strchr(o_user, ':');
    if (pass) *pass++ = '\0';
    context->c_user = o_user, context->c_pass = pass;
  }

  su_init();

  su_home_init(home = context->c_home);

  if (o_extra) {
    if (isatty(0))
      fprintf(stderr,
	      "Type extra HTTP headers, empty line then HTTP message body "
	      "(^D when complete):\n");
    fflush(stderr);

    extra = read_file(stdin);
  }

  context->c_root = su_root_create(context);

  if (context->c_root) {
    context->c_engine =
      nth_engine_create(context->c_root,
			NTHTAG_ERROR_MSG(0),
			TAG_END());

    if (context->c_engine) {
      while ((v = argv++[1])) {
	nth_client_t *clnt;
	clnt = nth_client_tcreate(context->c_engine,
				  response, context,
				  method, o_method_name,
				  URL_STRING_MAKE(v),
				  NTHTAG_PROXY(o_proxy),
				  HTTPTAG_USER_AGENT_STR(o_user_agent),
				  HTTPTAG_MAX_FORWARDS_STR(o_max_forwards),
				  TPTAG_REUSE(o_pipe),
				  HTTPTAG_HEADER_STR(extra),
				  TAG_END());
	if (clnt)
	  context->c_pending++;
      }

      if (context->c_pending)
	su_root_run(context->c_root);

      nth_engine_destroy(context->c_engine), context->c_engine = NULL;
    }
    su_root_destroy(context->c_root);
  }

  su_deinit();

  return 0;
}

/** Handle responses to request */
static
int response(context_t *c,
	     nth_client_t *clnt,
	     http_t const *http)
{
  nth_client_t *newclnt = NULL;
  int status;

  if (http) {
    status = http->http_status->st_status;
  } else {
    status = nth_client_status(clnt);
    fprintf(stderr, "HTTP/1.1 %u Error\n", status);
  }

  if (http && (c->c_pre || status >= 200)) {
    http_header_t *h = (http_header_t *)http->http_status;
    char hname[64];

    for (; h; h = (http_header_t *)h->sh_succ) {
      if (h == (http_header_t *)http->http_payload)
	continue;
      else if (h == (http_header_t *)http->http_separator)
	continue;
      else if (!h->sh_class->hc_name)
	header_print(stdout, NULL, h);
      else if (h->sh_class->hc_name[0]) {
	snprintf(hname, sizeof hname, "%s: %%s\n", h->sh_class->hc_name);
	header_print(stdout, hname, h);
      } else {
	header_print(stdout, "%s\n", h);
      }
    }

    printf("\n");
    if (http->http_payload)
      payload_print(stdout, http->http_payload);

    fflush(stdout);
  }

  if (status < 200)
    return 0;

  if (status == 401 && http->http_www_authenticate) {
    char const *user = c->c_user;
    char const *pass = c->c_pass;

    if (!user || !pass) {
      url_t const *url = nth_client_url(clnt);
      if (url) {
	user = url->url_user, pass = url->url_password;
       }
    }

    //if (user && pass &&
    if (
	auc_challenge(&c->c_auth, c->c_home,
		      http->http_www_authenticate,
		      http_authorization_class) > 0) {
      char const *scheme = NULL;
      char const *realm = NULL;

      scheme = http->http_www_authenticate->au_scheme;
      realm = msg_params_find(http->http_www_authenticate->au_params,
				"realm=");
      if (auc_all_credentials(&c->c_auth, scheme, realm, user, pass)
	  >= 0)
	newclnt = nth_client_tcreate(c->c_engine,
				     NULL, NULL, HTTP_NO_METHOD, NULL,
				     NTHTAG_AUTHENTICATION(&c->c_auth),
				     NTHTAG_TEMPLATE(clnt),
				     TAG_END());
    }
  }

  if (status == 302 && http->http_location) {
    url_t loc[1];

    *loc = *http->http_location->loc_url;

    newclnt = nth_client_tcreate(c->c_engine, NULL, NULL,
				 HTTP_NO_METHOD,
				 (url_string_t *)loc,
				 NTHTAG_TEMPLATE(clnt),
				 TAG_END());
  }


  if (newclnt)
    c->c_pending++;

  nth_client_destroy(clnt);
  if (c->c_pending-- == 1)
    su_root_break(c->c_root);

  return 0;
}
