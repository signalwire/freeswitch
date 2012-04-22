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

/**@nofile http-server.c
 * @brief Test HTTP server
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Sat Oct 19 02:56:23 2002 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_SIGNAL
#include <signal.h>
#endif

typedef struct context_s context_t;
#define NTH_SITE_MAGIC_T context_t
#define SU_ROOT_MAGIC_T context_t

#include <sofia-sip/nth.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/http_header.h>

struct context_s {
  su_home_t   	  c_home[1];
  su_root_t   	 *c_root;
  nth_site_t 	 *c_site;
  char const     *c_server;
  char const     *c_expires;
  char const     *c_content_type;
  http_content_length_t *c_content_length;
  msg_payload_t *c_body;
};

char const name[] = "http-server";

static
void usage(int rc)
{
  fprintf(rc ? stderr : stdout,
	  "usage: %s OPTIONS url [content]\n",
	  name
	  );
  exit(rc);
}

static int request(context_t *context,
		   nth_site_t *site,
		   nth_request_t *req,
		   http_t const *http,
		   char const *path);
su_msg_r server_intr_msg = SU_MSG_R_INIT;

#if HAVE_SIGNAL
static RETSIGTYPE server_intr_handler(int signum);
#endif
static void server_break(context_t *c, su_msg_r msg, su_msg_arg_t *arg);

static msg_payload_t *read_payload(su_home_t *home, char const *fname);
static msg_payload_t *fread_payload(su_home_t *home, FILE *f);

int main(int argc, char *argv[])
{
  su_home_t *home;
  context_t *c, context[1] = {{{SU_HOME_INIT(context)}}};
  char *o_url = NULL, *o_body = NULL;
  int o_extra = 0;
  int o_timeout = 300;
  char *s;

  c = context;
  su_init();
  su_home_init(home = context->c_home);

#define MATCH(s, v) \
      ((strncmp(s, v, strlen(v)) == 0))
#define MATCH1(s, v) \
      ((strncmp(s, v, strlen(v)) == 0) && \
       (s = (s[strlen(v)] ? s + strlen(v) : argv++[1])))
#define MATCH2(s, v) \
      ((strncmp(s, v, strlen(v)) == 0) && \
       (s[strlen(v)] == '=' ? (s = s + strlen(v) + 1) : (s = argv++[1])))

  while ((s = argv++[1])) {
    if (*s != '-') break;
    s++;
    if (MATCH2(s, "-expires")) 	         { c->c_expires = s; 	    continue; }
    else if (MATCH2(s, "-tcp-timeout"))  { o_timeout = strtoul(s, &s, 0); continue; }
    else if (MATCH2(s, "-ct"))           { c->c_content_type = s;   continue; }
    else if (MATCH2(s, "-content-type")) { c->c_content_type = s;   continue; }
    else if (MATCH2(s, "-server"))       { c->c_server = s;         continue; }
    else if (MATCH(s, "-help"))          { usage(0);                continue; }
    else if (MATCH(s, "x")||MATCH(s, "-extra")) { o_extra = 1;      continue; }
    else
      usage(2);
  }

  if (!(o_url = s))
    usage(1);
  else
    o_body = argv++[1];

  context->c_root = su_root_create(context);

  context->c_body = read_payload(context->c_home, o_body);
  if (!context->c_body) {
    perror("contents");
    exit(1);
  }

  context->c_content_length =
    msg_content_length_create(context->c_home, context->c_body->pl_len);

  su_msg_create(server_intr_msg,
		su_root_task(context->c_root),
		su_root_task(context->c_root),
		server_break, 0);

#if HAVE_SIGNAL
  signal(SIGINT, server_intr_handler);
#if HAVE_SIGQUIT
  signal(SIGQUIT, server_intr_handler);
  signal(SIGHUP, server_intr_handler);
#endif
#endif

  if (context->c_root) {
    context->c_site =
      nth_site_create(NULL,	             /* This is a top-level site */
		      request, context,
		      (url_string_t *)o_url,
		      NTHTAG_ROOT(context->c_root),
		      TPTAG_TIMEOUT(o_timeout * 1000),
		      TAG_END());

    if (context->c_site) {
      su_root_run(context->c_root);
      nth_site_destroy(context->c_site);
    }

    su_root_destroy(context->c_root);
  }

  su_deinit();

  return 0;
}

static void server_break(context_t *c, su_msg_r msg, su_msg_arg_t *arg)
{
  fprintf(stderr, "%s: received signal, exiting\n", name);

  su_root_break(c->c_root);
}

static RETSIGTYPE server_intr_handler(int signum)
{
  su_msg_send(server_intr_msg);
}


static int request(context_t *c,
		   nth_site_t *site,
		   nth_request_t *req,
		   http_t const *http,
		   char const *path)
{
  fprintf(stderr, "request to /%s\n", path);

  if (path && strlen(path))
    return 404;

  if (http->http_request->rq_method != http_method_get) {
    nth_request_treply(req, HTTP_405_NOT_ALLOWED,
		       HTTPTAG_ALLOW_STR("GET"),
		       TAG_END());
    return 405;
  }

  nth_request_treply(req, HTTP_200_OK,
		     HTTPTAG_SERVER_STR(c->c_server),
		     HTTPTAG_EXPIRES_STR(c->c_expires),
		     HTTPTAG_CONTENT_TYPE_STR(c->c_content_type),
		     HTTPTAG_CONTENT_LENGTH(c->c_content_length),
		     HTTPTAG_PAYLOAD(c->c_body),
		     TAG_END());
  return 200;
}

/** Read message body from named file.
 *
 * The function read_payload() reads the contents to a SIP payload
 * structure from a the named file. If @a fname is NULL, the payload
 * contents are read from standard input.
 */
msg_payload_t *read_payload(su_home_t *home, char const *fname)
{
  FILE *f;
  msg_payload_t *pl;

  if (fname == NULL || strcmp(fname, "-") == 0)
    f = stdin, fname = "<stdin>";
  else
    f = fopen(fname, "rb");

  if (f == NULL)
    return NULL;

  pl = fread_payload(home, f);
  if (f != stdin)
    fclose(f);

  return pl;
}

msg_payload_t *fread_payload(su_home_t *home, FILE *f)
{
  msg_payload_t *pl;
  int n;
  char *buf;
  off_t used, size;


  if (f == NULL) {
    errno = EINVAL;
    return NULL;
  }

  pl = msg_payload_create(home, NULL, 0);

  if (pl == NULL)
    return NULL;

  /* Read block by block */
  used = 0;
  size = 4096;
  buf = malloc(size);

  while (buf) {
    n = fread(buf + used, 1, size - used, f);
    used += n;
    if (n < size - used) {
      if (feof(f))
	;
      else if (ferror(f))
	buf = NULL;
      break;
    }
    buf = realloc(buf, size = 2 * size);
  }
  if (buf == NULL) {
    perror("fread_payload: realloc");
    return NULL;
  }

  if (used < size)
    buf[used] = '\0';

  pl->pl_common->h_data = pl->pl_data = buf;
  pl->pl_common->h_len = pl->pl_len = used;

  return pl;
}
