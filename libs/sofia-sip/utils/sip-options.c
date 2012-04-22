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

/**@page sip-options Query SIP OPTIONS
 *
 * @section synopsis Synopsis
 * <tt>sip-options [OPTIONS] target-uri </tt>
 *
 * @section description Description
 * The @em sip-options utility sends a SIP OPTIONS request (or any other SIP
 * request) to a SIP server.
 *
 * The @em sip-options tool will print out status line and interesting headers
 * from the response, excluding From, Via, Call-ID, and CSeq. The message
 * body is also printed.
 *
 * @section options Command Line Options
 * The @e options utility accepts following command line options:
 * <dl>
 * <dt>-m url | --contact=url | --bind=url</dt>
 * <dd>Specifies the SIP URL to which the @em options utility binds.
 * </dd>
 * <dt>--1XX | -1</dt>
 * <dd>Print also preliminary responses. If this option is not present,
 *     preliminary responses are silently discarded.
 * </dd>
 * <dt>--all | -a</dt>
 * <dd>All SIP headers will be printed. If the --all option is given,
 *     the @em options utility also prints @b From, @b Via, @b Call-ID or
 *     @b CSeq headers.
 * </dd>
 * <dt>--from=url</dt>
 * <dd>Specifies the @b From header. Unless this option is used or the
 *     environment variable @c SIPADDRESS is set, local Contact URL is used
 *     as @b From header as well.
 * </dd>
 * <dt>--mf=n</dt>
 * <dd>Specify the initial Max-Forwards count (defaults to 70, stack default).
 * </dd>
 * <dt>--method=s</dt>
 * <dd>Specify the request method (defaults to OPTIONS).
 * </dd>
 * <dt>--extra | -x/dt>
 * <dd>Read extra headers (and optionally a message body) from the standard
 *     input
 * </dd>
 * </dl>
 *
 * @section return Return Codes
 * <table>
 * <tr><td>0<td>when successful (a 2XX-series response is received)
 * <tr><td>1<td>when unsuccessful (a 3XX..6XX-series response is received)
 * <tr><td>2<td>initialization failure
 * </table>
 *
 * @section examples Examples
 * You want to query supported features of sip:essip00net.nokia.com:
 * @code
 * $ options sip:essip00net.nokia.com
 * @endcode
 *
 * @section environment Environment
 * #SIPADDRESS, #sip_proxy, #NTA_DEBUG, #TPORT_DEBUG, #TPORT_LOG.
 *
 * @section bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section author Author
 * Written by Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 *
 * @section copyright Copyright
 * Copyright (C) 2005 Nokia Corporation.
 *
 * This program is free software; see the source for copying conditions.
 * There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef struct context_s context_t;
#define NTA_OUTGOING_MAGIC_T context_t

#include <sofia-sip/nta.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/sl_utils.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/auth_client.h>
#include <sofia-sip/tport_tag.h>

struct context_s {
  su_home_t	  c_home[1];
  su_root_t	 *c_root;
  nta_agent_t	 *c_agent;
  url_t          *c_proxy;
  char const     *c_username;
  char const     *c_password;
  nta_leg_t	 *c_leg;
  nta_outgoing_t *c_orq;
  auth_client_t  *c_proxy_auth;
  auth_client_t  *c_auth;
  unsigned        c_proxy_auth_retries;
  unsigned        c_auth_retries;
  int             c_all;
  int             c_pre;
  int             c_retval;
};

char const name[] = "sip-options";

static
void usage(int rc)
{
  fprintf(rc ? stderr : stdout,
	  "usage: %s OPTIONS url [extra-file]\n"
	  "where OPTIONS are\n"
	  "    --mf=count | --max-forwards=count\n"
	  "    --contact=url | --bind=url | -m=url\n"
	  "    --from=url\n"
	  "    --ua=user-agent\n"
	  "    --all | -a\n"
	  "    --1XX | -1\n"
	  "    --x | --extra \n",
	  name);
  exit(rc);
}

#include "apps_utils.h"

static int response_to_options(context_t *context,
			       nta_outgoing_t *oreq,
			       sip_t const *sip);
static char *readfile(FILE *f);

int main(int argc, char *argv[])
{
  su_home_t *home;
  context_t context[1] = {{{SU_HOME_INIT(context)}}};
  char
    *extra = NULL,
    *o_bind = "sip:*:*",
    *o_from = getenv("SIPADDRESS"),
    *o_http_proxy = NULL,
    *o_max_forwards = NULL,
    *o_method = NULL,
    *o_to = NULL;

  char *s, *v;

  sip_method_t method = sip_method_options;

#define MATCH(s, o) \
      ((strncmp(s, o, strlen(o)) == 0))
#define MATCH1(s, o) \
      ((strncmp(s, o, strlen(o)) == 0) && \
       (v = (s[strlen(o)] ? s + strlen(o) : argv++[1])))
#define MATCH2(s, o) \
      ((strncmp(s, o, strlen(o)) == 0) && \
       (s[strlen(o)] == '=' || s[strlen(o)] == '\0') && \
       (v = s[strlen(o)] ? s + strlen(o) + 1 : argv++[1]))

  while ((s = argv++[1])) {
    if (!MATCH(s, "-"))             { o_to = s;           break; }
    else if (strcmp(s, "") == 0)    { o_to = argv++[1];   break; }
    else if (MATCH(s, "-a") || MATCH(s, "--all"))
                                    { context->c_all = 1; }
    else if (MATCH(s, "-x") || MATCH(s, "--extra"))
                                    { extra = "-"; }
    else if (MATCH(s, "-1") || MATCH(s, "--1XX"))
                                    { context->c_pre = 1; }
    else if (MATCH2(s, "--mf"))     { o_max_forwards = v; }
    else if (MATCH2(s, "--http-proxy"))     { o_http_proxy = v; }
    else if (MATCH2(s, "--max-forwards"))     { o_max_forwards = v; }
    else if (MATCH2(s, "--bind"))   { o_bind = v; }
    else if (MATCH1(s, "-m"))       { o_bind = v; }
    else if (MATCH2(s, "--contact")){ o_bind = v; }
    else if (MATCH2(s, "--from"))   { o_from = v; }
    else if (MATCH2(s, "--method")) { o_method = v; }
    else if (MATCH(s, "--help"))    { usage(0); }
    else
      usage(2);
  }

  if (!o_to)
    usage(2);

  if (argv[1])
    extra = argv++[1];

  su_init();

  su_home_init(home = context->c_home);

  context->c_root = su_root_create(context);
  context->c_retval = 2;

  if (context->c_root) {
    url_string_t *r_uri;

    context->c_agent =
      nta_agent_create(context->c_root,
		       URL_STRING_MAKE(o_bind),
		       NULL, NULL, /* Ignore incoming messages */
		       TPTAG_HTTP_CONNECT(o_http_proxy),
		       TAG_END());

    if (context->c_agent) {
      sip_addr_t *from, *to;
      sip_contact_t const *m = nta_agent_contact(context->c_agent);

      to = sip_to_create(home, (url_string_t *)o_to);

      if (o_from)
	from = sip_from_make(home, o_from);
      else
	from = sip_from_create(home, (url_string_t const *)m->m_url);

      if (!from) {
	fprintf(stderr, "%s: no valid From address\n", name);
	exit(2);
      }

      tag_from_header(context->c_agent, context->c_home, from);

      if (o_method) {
	method = sip_method_code(o_method);
      } else {
	isize_t len;
	char const *params = to->a_url->url_params;

	len = url_param(params, "method", NULL, 0);

	if (len > 0) {
	  o_method = su_alloc(home, len + 1);
	  if (o_method == 0 ||
	      url_param(params, "method", o_method, len + 1) != len) {
	    fprintf(stderr, "%s: %s\n", name,
		    o_method ? "internal error" : strerror(errno));
	    exit(2);
	  }
	  method = sip_method_code(o_method);
	}
      }

      r_uri = (url_string_t *)url_hdup(home, to->a_url);

      sip_aor_strip(to->a_url);
      sip_aor_strip(from->a_url);

      context->c_username = from->a_url->url_user;
      context->c_password = from->a_url->url_password;
      from->a_url->url_password = NULL;

      if (extra) {
	FILE *hf;

	if (strcmp(extra, "-"))
	  hf = fopen(extra, "rb");
	else
	  hf = stdin;

	extra = readfile(hf);
      }

      context->c_proxy = url_hdup(context->c_home,
				  (url_t *)getenv("sip_proxy"));

      nta_agent_set_params(context->c_agent,
			   NTATAG_SIPFLAGS(MSG_FLG_EXTRACT_COPY),
			   NTATAG_DEFAULT_PROXY(context->c_proxy),
			   TAG_END());

      context->c_leg =
	nta_leg_tcreate(context->c_agent,
			NULL, NULL,      /* ignore incoming requests */
			SIPTAG_FROM(from), /* who is sending OPTIONS? */
			SIPTAG_TO(to), /* whom we are sending OPTIONS? */
			TAG_END());

      if (context->c_leg) {
	context->c_orq =
	  nta_outgoing_tcreate(context->c_leg,
			       response_to_options, context,
			       NULL,
			       method, o_method, r_uri,
			       SIPTAG_USER_AGENT_STR("options"),
			       SIPTAG_MAX_FORWARDS_STR(o_max_forwards),
			       SIPTAG_HEADER_STR(extra),
			       TAG_END());

	if (context->c_orq) {
	  su_root_run(context->c_root);
	  nta_outgoing_destroy(context->c_orq), context->c_orq = NULL;
	}

	nta_leg_destroy(context->c_leg), context->c_leg = NULL;
      }

      nta_agent_destroy(context->c_agent), context->c_agent = NULL;
    }

    su_root_destroy(context->c_root);
  }

  su_deinit();

  return context->c_retval;
}

/** Handle responses to OPTIONS request */
static
int response_to_options(context_t *context,
			nta_outgoing_t *oreq,
			sip_t const *sip)
{
  if (proxy_authenticate(context, oreq, sip, response_to_options))
    return 0;
  if (server_authenticate(context, oreq, sip, response_to_options))
    return 0;

  if (sip->sip_status->st_status >= 200 || context->c_pre) {
    sip_header_t *h = (sip_header_t *)sip->sip_status;
    char hname[64];

    for (; h; h = (sip_header_t *)h->sh_succ) {
      if (!context->c_all) {
	if (sip_is_from(h) ||
	    sip_is_via(h) ||
	    sip_is_call_id(h) ||
	    sip_is_cseq(h) ||
	    sip_is_content_length(h))
	  continue;
      }

      if (h->sh_class->hc_name == NULL) {
	sl_header_print(stdout, NULL, h);
      }
      else if (h->sh_class->hc_name[0] == '\0') {
	sl_header_print(stdout, "%s\n", h);
      }
      else {
	snprintf(hname, sizeof hname, "%s: %%s\n", h->sh_class->hc_name);
	sl_header_print(stdout, hname, h);
      }
    }
  }

  if (sip->sip_status->st_status >= 200) {
    context->c_retval = sip->sip_status->st_status >= 300;
    su_root_break(context->c_root);
  }

  return 0;
}

/* Read in whole (binary!) file */
char *readfile(FILE *f)
{
  char *buffer = NULL;
  long size;
  size_t len;

  if (f == NULL)
    return NULL;

  for (size = 8192, buffer = NULL, len = 0; !feof(f); size += 8192) {
    buffer = realloc(buffer, size + 1);
    if (!buffer)
      exit(2);
    len += fread(buffer + len, 1, 8192, f);
  }

  buffer[len] = '\0';

  return buffer;
}
