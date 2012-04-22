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

/**@internal
 * @CFILE test_nta.c
 *
 * Test functions for NTA.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 21 15:18:26 2001 ppessi
 */

#include "config.h"

typedef struct agent_t agent_t;
typedef struct client_t client_t;

#define SU_ROOT_MAGIC_T      agent_t

#include <sofia-sip/su_wait.h>

#include <msg_internal.h>

#define NTA_AGENT_MAGIC_T    agent_t
#define NTA_LEG_MAGIC_T      agent_t
#define NTA_OUTGOING_MAGIC_T client_t
#define NTA_INCOMING_MAGIC_T agent_t
#define NTA_RELIABLE_MAGIC_T agent_t

#include "sofia-sip/nta.h"
#include "sofia-sip/nta_tport.h"
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/htable.h>
#include <sofia-sip/sresolv.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/sofia_features.h>
#include <sofia-sip/hostdomain.h>
#include <sofia-sip/tport.h>

#include <sofia-sip/su_string.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "s2util.h"

#if HAVE_OPEN_C
#include <sys/param.h>
#endif

SOFIAPUBVAR su_log_t nta_log[];
SOFIAPUBVAR su_log_t tport_log[];

int tstflags = 0;
#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

#define NONE ((void *)-1)

int expensive_checks, test_nta_sips;

#define EXPENSIVE_CHECKS (expensive_checks)

struct sigcomp_compartment;

char const name[] = "test_nta";

typedef struct invite_client_t invite_client_t;

typedef int client_check_f(client_t *, nta_outgoing_t *, sip_t const *);
typedef int client_deinit_f(client_t *);

struct client_t {
  agent_t *c_ag;
  char const *c_name;
  client_check_f * c_check;
  client_check_f * const * c_checks;
  client_deinit_f * c_deinit;
  void *c_extra;
  nta_outgoing_t *c_orq;
  int c_status;
  int c_final;
  int c_errors;
};

struct invite_client_t {
  client_t ic_client[1];
  nta_outgoing_t *ic_orq;	/* Original INVITE transaction */
  int ic_tag_status;		/* Status for current branch */
  char *ic_tag;
};

struct agent_t {
  su_home_t       ag_home[1];
  int             ag_flags;
  su_root_t      *ag_root;
  msg_mclass_t   *ag_mclass;
  nta_agent_t    *ag_agent;

  url_string_t   *ag_obp;	/**< Outbound proxy. */

  nta_leg_t      *ag_server_leg; /**< Leg for sip:%@% */
  nta_leg_t      *ag_default_leg; /**< Leg for rest */

  unsigned        ag_drop;

  nta_outgoing_t *ag_orq;
  unsigned        ag_running :1, ag_canceled:1, ag_acked:1, :0;

  char const     *ag_comp;
  struct sigcomp_compartment *ag_client_compartment;

  /* Server side */
  int             ag_response;	/**< What we answer by default */
  nta_incoming_t *ag_irq;

  struct sigcomp_compartment *ag_server_compartment;

  char const     *ag_m;

  sip_contact_t const *ag_contact;
  sip_from_t     *ag_alice;
  sip_to_t       *ag_bob;

  sip_contact_t  *ag_m_alice;
  sip_contact_t  *ag_m_bob;
  sip_contact_t  *ag_aliases;

  nta_leg_t      *ag_alice_leg;
  nta_leg_t      *ag_bob_leg;

  msg_t          *ag_request;

  nta_leg_t      *ag_expect_leg;
  nta_leg_t      *ag_latest_leg;
  nta_leg_t      *ag_call_leg;
  nta_reliable_t *ag_reliable;

  sip_via_t      *ag_in_via;	/**< Incoming via */

  sip_content_type_t *ag_content_type;
  sip_payload_t  *ag_payload;

  msg_t          *ag_probe_msg;

  su_sockaddr_t   ag_su_nta[1];
  socklen_t       ag_su_nta_len;

  /* Dummy servers */
  char const     *ag_sink_port;
  su_socket_t     ag_sink_socket, ag_down_socket;
  su_wait_t       ag_sink_wait[1];
};

static int test_init(agent_t *ag, char const *resolv_conf);
static int test_deinit(agent_t *ag);
static int test_bad_messages(agent_t *ag);
static int test_routing(agent_t *ag);
static int test_tports(agent_t *ag);
static int test_resolv(agent_t *ag, char const *resolv_conf);
static int test_dialog(agent_t *ag);
static int test_call(agent_t *ag);
static int test_prack(agent_t *ag);
static int test_fix_467(agent_t *ag);

static int test_for_ack(agent_t *ag,
			nta_incoming_t *irq,
			sip_t const *sip);
static int test_for_ack_or_timeout(agent_t *ag,
				   nta_incoming_t *irq,
				   sip_t const *sip);

static int wait_for_ack_or_cancel(agent_t *ag,
				  nta_incoming_t *irq,
				  sip_t const *sip);

int agent_callback(agent_t *ag,
		   nta_agent_t *nta,
		   msg_t *msg,
		   sip_t *sip)
{
  if (tstflags & tst_verbatim) {
    if (sip->sip_request) {
      printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	     name, __func__, sip->sip_request->rq_method_name,
	     URL_PRINT_ARGS(sip->sip_request->rq_url),
	     sip->sip_request->rq_version);
    }
    else {
      printf("%s: %s: %s %03d %s\n", name, __func__,
	     sip->sip_status->st_version,
	     sip->sip_status->st_status,
	     sip->sip_status->st_phrase);
    }
  }

  msg_destroy(msg);

  return 0;
}

static
void leg_match(agent_t *ag, nta_leg_t *leg, int always, char const *func)
{
  char const *match = "unknown leg";

  if (!always && (tstflags & tst_verbatim) != tst_verbatim)
    return;

  if (leg == ag->ag_default_leg)
    match = "ag_default_leg";
  else if (leg == ag->ag_server_leg)
    match = "ag_server_leg";
  else if (leg == ag->ag_alice_leg)
    match = "ag_alice_leg";
  else if (leg == ag->ag_bob_leg)
    match = "ag_bob_leg";

  printf("%s: %s: %smatched with %s\n", name, func,
	 always ? "mis" : "", match);
}

static
void leg_zap(agent_t *ag, nta_leg_t *leg)
{
  if (leg == ag->ag_default_leg)
    ag->ag_default_leg = NULL;
  else if (leg == ag->ag_server_leg)
    ag->ag_server_leg = NULL;
  else if (leg == ag->ag_alice_leg)
    ag->ag_alice_leg = NULL;
  else if (leg == ag->ag_bob_leg)
    ag->ag_bob_leg = NULL;
  else
    printf("%s:%u: %s: did not exist\n",
	   __FILE__, __LINE__, __func__);

  nta_leg_destroy(leg);
}


int leg_callback_200(agent_t *ag,
		     nta_leg_t *leg,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  if (!sip->sip_content_length ||
      !sip->sip_via ||
      !sip->sip_from || !sip->sip_from->a_tag)
    return 500;

  if (ag->ag_in_via == NULL)
    ag->ag_in_via = sip_via_dup(ag->ag_home, sip->sip_via);

  if (ag->ag_request == NULL)
    ag->ag_request = nta_incoming_getrequest(irq);

  ag->ag_latest_leg = leg;

  if (ag->ag_expect_leg && leg != ag->ag_expect_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }
  leg_match(ag, leg, 0, __func__);

  if (sip->sip_request->rq_method == sip_method_bye) {
    leg_zap(ag, leg);
  }

  return 200;
}

int leg_callback_500(agent_t *ag,
		     nta_leg_t *leg,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  return 500;
}


int new_leg_callback_200(agent_t *ag,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  if (!sip->sip_content_length ||
      !sip->sip_via ||
      !sip->sip_from || !sip->sip_from->a_tag)
    return 500;

  ag->ag_latest_leg = leg;

  if (ag->ag_expect_leg && leg != ag->ag_expect_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }
  leg_match(ag, leg, 0, __func__);

  ag->ag_bob_leg = nta_leg_tcreate(ag->ag_agent,
				   leg_callback_200,
				   ag,
				   URLTAG_URL(sip->sip_request->rq_url),
				   SIPTAG_CALL_ID(sip->sip_call_id),
				   SIPTAG_FROM(sip->sip_to),
				   SIPTAG_TO(sip->sip_from),
				   TAG_END());
  if (!ag->ag_bob_leg ||
      !nta_leg_tag(ag->ag_bob_leg, NULL) ||
      !nta_leg_get_tag(ag->ag_bob_leg) ||
      !nta_incoming_tag(irq, nta_leg_get_tag(ag->ag_bob_leg)))
    return 500;

  return 200;
}

int new_leg_callback_180(agent_t *ag,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  int status = new_leg_callback_200(ag, leg, irq, sip);
  if (status == 200) {
    ag->ag_irq = irq;
    status = 180;
  }
  return status;
}


static client_check_f client_check_to_tag;

static client_check_f * const default_checks[] = {
  client_check_to_tag,
  NULL
};

static client_check_f * const no_default_checks[] = {
  NULL
};

/** Callback from client transaction */
int outgoing_callback(client_t *ctx,
		      nta_outgoing_t *orq,
		      sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = nta_outgoing_status(orq);
  client_check_f * const *checks;

  if (tstflags & tst_verbatim) {
    if (sip)
      printf("%s: %s: response %s %03d %s\n", name, ctx->c_name,
	     sip->sip_status->st_version,
	     sip->sip_status->st_status,
	     sip->sip_status->st_phrase);
    else
      printf("%s: %s: callback %03d\n", name, ctx->c_name,
	     status);
  }

  if (status >= 200 && ag->ag_comp) {		/* XXX */
    nta_compartment_decref(&ag->ag_client_compartment);
    ag->ag_client_compartment = nta_outgoing_compartment(ctx->c_orq);
  }

  if (status > ctx->c_status)
    ctx->c_status = status;
  if (status >= 200)
    ctx->c_final = 1;

  if (ctx->c_check && ctx->c_check(ctx, orq, sip))
    ctx->c_errors++;

  checks = ctx->c_checks;

  for (checks = checks ? checks : default_checks; *checks; checks++)
    if ((*checks)(ctx, ctx->c_orq, sip))
      ctx->c_errors++;

  return 0;
}

/** Deinit client. Return nonzero if client checks failed. */
static
int client_deinit(client_t *c)
{
  int errors = c->c_errors;

  if (c->c_deinit && c->c_deinit(c))
    errors++;

  if (c->c_orq) nta_outgoing_destroy(c->c_orq), c->c_orq = NULL;

  c->c_errors = 0;
  c->c_status = 0;

  return errors;
}


static
void nta_test_run(agent_t *ag)
{
  for (ag->ag_running = 1; ag->ag_running;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
}


/** Run client test. Return nonzero if client checks failed. */
static
int client_run_with(client_t *c, int expected, void (*runner)(client_t *c))
{
  int resulting;

  TEST_1(c->c_orq != NULL);

  runner(c);

  resulting = c->c_status;

  if (client_deinit(c))
    return 1;

  if (expected)
    TEST(resulting, expected);

  return 0;
}

static
void until_final_received(client_t *c)
{
  for (c->c_final = 0; !c->c_final; ) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(c->c_ag->ag_root, 500L);
  }
}

static
void fast_final_received(client_t *c)
{
  for (c->c_final = 0; !c->c_final; ) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    s2_fast_forward(500, c->c_ag->ag_root);
  }
}

static
int client_run(client_t *c, int expected)
{
  return client_run_with(c, expected, until_final_received);
}

static
void until_server_acked(client_t *c)
{
  agent_t *ag = c->c_ag;

  for (ag->ag_acked = 0; !ag->ag_acked;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
}

static
int client_run_until_acked(client_t *c, int expected)
{
  return client_run_with(c, expected, until_server_acked);
}

void
until_server_canceled(client_t *c)
{
  agent_t *ag = c->c_ag;

  for (ag->ag_canceled = 0; !ag->ag_canceled || c->c_status < 200;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
}

static
int client_run_until_canceled(client_t *c, int expected)
{
  return client_run_with(c, expected, until_server_canceled);
}


#include <sofia-sip/msg_mclass.h>

int test_init(agent_t *ag, char const *resolv_conf)
{
  char const *contact = "sip:*:*;comp=sigcomp";
  su_sockaddr_t su;
  socklen_t sulen, sulen0;
  su_socket_t s;
  int af, err = -1;

  BEGIN();

  ag->ag_root = su_root_create(ag);
  TEST_1(ag->ag_root);

  ag->ag_mclass = msg_mclass_clone(sip_default_mclass(), 0, 0);
  TEST_1(ag->ag_mclass);

#if SU_HAVE_IN6
  if (su_strmatch(getenv("ipv6"), "true")) {
    contact = "sip:[::]:*;comp=sigcomp";
    af = AF_INET6, sulen0 = sizeof (struct sockaddr_in6);
  }
  else {
    af = AF_INET, sulen0 = sizeof (struct sockaddr_in);
    contact = "sip:0.0.0.0:*;comp=sigcomp";
  }
#else
  af = AF_INET, sulen0 = sizeof (struct sockaddr_in);
  contact = "sip:0.0.0.0:*;comp=sigcomp";
#endif

  if (ag->ag_m)
    contact = ag->ag_m;
  else if (getenv("SIPCONTACT"))
    contact = getenv("SIPCONTACT");

  /* Sink server */
  s = su_socket(af, SOCK_DGRAM, 0); TEST_1(s != INVALID_SOCKET);
  memset(&su, 0, sulen = sulen0);
  su.su_family = af;
  if (getenv("sink")) {
    su.su_port = htons(atoi(getenv("sink")));
  }
  TEST_1(bind(s, &su.su_sa, sulen) < 0 ? (perror("bind"), 0) : 1);
  TEST_1(getsockname(s, &su.su_sa, &sulen) == 0);

  ag->ag_sink_socket = s;
  su_wait_init(ag->ag_sink_wait);
  su_wait_create(ag->ag_sink_wait, ag->ag_sink_socket, SU_WAIT_IN);

  ag->ag_sink_port = su_sprintf(ag->ag_home, "%u", ntohs(su.su_sin.sin_port));

  /* Down server */
  s = su_socket(af, SOCK_STREAM, 0); TEST_1(s != INVALID_SOCKET);
  memset(&su, 0, sulen = sulen0);
  su.su_family = af;
  if (getenv("down")) {
    su.su_port = htons(atoi(getenv("down")));
  }
  TEST_1(bind(s, &su.su_sa, sulen) < 0 ? (perror("bind"), 0) : 1);
  ag->ag_down_socket = s;

  /* Create agent */
  ag->ag_agent = nta_agent_create(ag->ag_root,
				  (url_string_t *)contact,
				  NULL,
				  NULL,
				  NTATAG_MCLASS(ag->ag_mclass),
				  NTATAG_USE_TIMESTAMP(1),
				  SRESTAG_RESOLV_CONF(resolv_conf),
				  NTATAG_USE_NAPTR(0),
				  NTATAG_USE_SRV(0),
				  NTATAG_PRELOAD(2048),
				  TAG_END());
  TEST_1(ag->ag_agent);

  contact = getenv("SIPSCONTACT");
  if (contact) {
    if (nta_agent_add_tport(ag->ag_agent, URL_STRING_MAKE(contact),
			    TPTAG_CERTIFICATE(getenv("TEST_NTA_CERTDIR")),
			    TPTAG_TLS_VERIFY_POLICY(TPTLS_VERIFY_NONE),
			    TAG_END()) == 0)
      test_nta_sips = 1;
  }

  {
    /* Initialize our headers */
    sip_from_t from[1];
    sip_to_t to[1];
    sip_contact_t m[1];

    su_sockaddr_t *su = ag->ag_su_nta;

    sip_from_init(from);
    sip_to_init(to);
    sip_contact_init(m);


    TEST_1(ag->ag_contact = nta_agent_contact(ag->ag_agent));

    *m->m_url = *ag->ag_contact->m_url;

    if (host_is_ip4_address(m->m_url->url_host)) {
      su_inet_pton(su->su_family = AF_INET,
		   m->m_url->url_host,
		   &su->su_sin.sin_addr);
      ag->ag_su_nta_len = (sizeof su->su_sin);
    }
    else {
      TEST_1(host_is_ip_address(m->m_url->url_host));
      su_inet_pton(su->su_family = AF_INET6,
		   m->m_url->url_host,
		   &su->su_sin6.sin6_addr);
      ag->ag_su_nta_len = (sizeof su->su_sin6);
    }

    su->su_port = htons(5060);
    if (m->m_url->url_port && strlen(m->m_url->url_port)) {
      unsigned long port = strtoul(m->m_url->url_port, NULL, 10);
      su->su_port = htons(port);
    }
    TEST_1(su->su_port != 0);

    m->m_url->url_user = "bob";
    TEST_1(ag->ag_m_bob = sip_contact_dup(ag->ag_home, m));

    to->a_display = "Bob";
    *to->a_url = *ag->ag_contact->m_url;
    to->a_url->url_user = "bob";
    to->a_url->url_port = NULL;
    TEST_1(ag->ag_bob = sip_to_dup(ag->ag_home, to));

    *m->m_url = *ag->ag_contact->m_url;
    m->m_url->url_user = "alice";
    TEST_1(ag->ag_m_alice = sip_contact_dup(ag->ag_home, m));

    from->a_display = "Alice";
    *from->a_url = *ag->ag_contact->m_url;
    from->a_url->url_user = "alice";
    from->a_url->url_port = NULL;
    TEST_1(ag->ag_alice = sip_from_dup(ag->ag_home, from));
  }
  {
    char const data[] =
      "v=0\r\n"
      "o=- 425432 423412 IN IP4 127.0.0.1\r\n"
      "s= \r\n"
      "c=IN IP4 127.0.0.1\r\n"
      "m=5004 audio 8 0\r\n";

    ag->ag_content_type = sip_content_type_make(ag->ag_home, "application/sdp");
    ag->ag_payload = sip_payload_make(ag->ag_home, data);
  }

  {
    sip_contact_t *m;

    ag->ag_aliases =
      sip_contact_make(ag->ag_home, "sip:127.0.0.1, sip:localhost, sip:[::1]");
    TEST_1(ag->ag_aliases);
    TEST_1(ag->ag_aliases->m_next);
    TEST_1(ag->ag_aliases->m_next->m_next);
    TEST_P(ag->ag_aliases->m_next->m_next->m_next, NULL);

    for (m = ag->ag_aliases; m; m = m->m_next)
      m->m_url->url_port = ag->ag_contact->m_url->url_port;

    TEST_1(m = sip_contact_dup(ag->ag_home, ag->ag_contact));

    m->m_next = ag->ag_aliases;
    ag->ag_aliases = m;

    err = nta_agent_set_params(ag->ag_agent,
			       NTATAG_ALIASES(ag->ag_aliases),
			       NTATAG_REL100(1),
			       NTATAG_UA(1),
			       NTATAG_MERGE_482(1),
			       NTATAG_USE_NAPTR(1),
			       NTATAG_USE_SRV(1),
			       NTATAG_MAX_FORWARDS(20),
			       TAG_END());
    TEST(err, 7);

    err = nta_agent_set_params(ag->ag_agent,
			       NTATAG_ALIASES(ag->ag_aliases),
			       NTATAG_DEFAULT_PROXY("sip:127.0.0.1"),
			       TAG_END());
    TEST(err, 2);

    err = nta_agent_set_params(ag->ag_agent,
			       NTATAG_ALIASES(ag->ag_aliases),
			       NTATAG_DEFAULT_PROXY(NULL),
			       TAG_END());
    TEST(err, 2);

    err = nta_agent_set_params(ag->ag_agent,
			       NTATAG_DEFAULT_PROXY("tel:+35878008000"),
			       TAG_END());
    TEST(err, -1);

  }

  {
    url_t url[1];

    /* Create the server leg */
    *url = *ag->ag_aliases->m_url;
    url->url_user = "%";
    ag->ag_server_leg = nta_leg_tcreate(ag->ag_agent,
					leg_callback_200,
					ag,
					NTATAG_NO_DIALOG(1),
					URLTAG_URL(url),
					TAG_END());
    TEST_1(ag->ag_server_leg);
  }

  END();
}

int test_reinit(agent_t *ag)
{
  BEGIN();
  /* Create a new default leg */
  nta_leg_destroy(ag->ag_default_leg), ag->ag_default_leg = NULL;
  TEST_1(ag->ag_default_leg = nta_leg_tcreate(ag->ag_agent,
					      leg_callback_200,
					      ag,
					      NTATAG_NO_DIALOG(1),
					      TAG_END()));
  END();
}

int test_deinit(agent_t *ag)
{
  BEGIN();

  if (ag->ag_request) msg_destroy(ag->ag_request), ag->ag_request = NULL;

  su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;

  nta_leg_destroy(ag->ag_alice_leg);
  nta_leg_destroy(ag->ag_bob_leg);
  nta_leg_destroy(ag->ag_default_leg);
  nta_leg_destroy(ag->ag_server_leg);

  nta_agent_destroy(ag->ag_agent);
  su_root_destroy(ag->ag_root);

  if (ag->ag_sink_port) {
    su_free(ag->ag_home, (void *)ag->ag_sink_port), ag->ag_sink_port = NULL;
    su_wait_destroy(ag->ag_sink_wait);
    su_close(ag->ag_sink_socket);
  }

  free(ag->ag_mclass), ag->ag_mclass = NULL;

  END();
}

static
int readfile(FILE *f, void **contents)
{
  /* Read in whole (binary!) file */
  char *buffer = NULL;
  long size;
  size_t len;

  /* Read whole file in */
  if (fseek(f, 0, SEEK_END) < 0 ||
      (size = ftell(f)) < 0 ||
      fseek(f, 0, SEEK_SET) < 0 ||
      (long)(len = (size_t)size) != size) {
    fprintf(stderr, "%s: unable to determine file size (%s)\n",
	    __func__, strerror(errno));
    return -1;
  }

  if (!(buffer = malloc(len + 2)) ||
      fread(buffer, 1, len, f) != len) {
    fprintf(stderr, "%s: unable to read file (%s)\n", __func__, strerror(errno));
    if (buffer)
      free(buffer);
    return -1;
  }

  buffer[len] = '\0';

  *contents = buffer;

  return (int)len;
}

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

static int test_bad_messages(agent_t *ag)
{
  BEGIN();

#if HAVE_DIRENT_H
  DIR *dir;
  struct dirent *d;
  char name[PATH_MAX + 1] = "../sip/tests/";
  size_t offset;
  char const *host, *port;
  su_addrinfo_t *ai,  hints[1];
  su_socket_t s;
  su_sockaddr_t su[1];
  socklen_t sulen;
  char via[64];
  size_t vlen;
  int i;

  dir = opendir(name);
  if (dir == NULL && getenv("srcdir")) {
    strncpy(name, getenv("srcdir"), PATH_MAX);
    strncat(name, "/../sip/tests/", PATH_MAX);
    dir = opendir(name);
  }

  if (dir == NULL) {
    fprintf(stderr, "test_nta: cannot find sip torture messages\n");
    fprintf(stderr, "test_nta: tried %s\n", name);
  }

  offset = strlen(name);

  TEST_1(ag->ag_default_leg = nta_leg_tcreate(ag->ag_agent,
					      leg_callback_500,
					      ag,
					      NTATAG_NO_DIALOG(1),
					      TAG_END()));

  host = ag->ag_contact->m_url->url_host;
  if (host_is_ip6_reference(host)) {
    host = strcpy(via, host + 1);
    via[strlen(via) - 1] = '\0';
  }
  port = url_port(ag->ag_contact->m_url);

  memset(hints, 0, sizeof hints);
  hints->ai_socktype = SOCK_DGRAM;
  hints->ai_protocol = IPPROTO_UDP;

  TEST(su_getaddrinfo(host, port, hints, &ai), 0); TEST_1(ai);
  s = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol); TEST_1(s != -1);
  memset(su, 0, sulen = ai->ai_addrlen);
  su->su_len = sizeof su; su->su_family = ai->ai_family;
  TEST_1(bind(s, &su->su_sa, sulen) == 0);
  TEST_1(getsockname(s, &su->su_sa, &sulen) == 0);
  sprintf(via, "v: SIP/2.0/UDP is.invalid:%u\r\n", ntohs(su->su_port));
  vlen = strlen(via);

  for (d = dir ? readdir(dir) : NULL; d; d = readdir(dir)) {
    size_t len = strlen(d->d_name);
    FILE *f;
    int blen, n;
    void *buffer; char *r;

    if (len < strlen(".txt"))
      continue;
    if (strcmp(d->d_name + len - strlen(".txt"), ".txt"))
      continue;
    strncpy(name + offset, d->d_name, PATH_MAX - offset);
    TEST_1(f = fopen(name, "rb"));
    TEST_1((blen = readfile(f, &buffer)) > 0);
    fclose(f);
    r = buffer;

    if (strncmp(r, "JUNK ", 5) == 0) {
      TEST_SIZE(su_sendto(s, r, blen, 0, ai->ai_addr, ai->ai_addrlen), blen);
    }
    else if (strncmp(r, "INVITE ", 7) != 0) {
      su_iovec_t vec[3];
      n = strcspn(r, "\r\n"); n += strspn(r + n, "\r\n");
      vec[0].siv_base = r, vec[0].siv_len = n;
      vec[1].siv_base = via, vec[1].siv_len = vlen;
      vec[2].siv_base = r + n, vec[2].siv_len = blen - n;
      TEST_SIZE(su_vsend(s, vec, 3, 0, (void *)ai->ai_addr, ai->ai_addrlen),
		blen + vlen);
    }
    free(buffer);
    su_root_step(ag->ag_root, 1);
  }

  TEST_SIZE(su_sendto(s, "\r\n\r\n", 4, 0, (void *)ai->ai_addr, ai->ai_addrlen), 4);

  su_root_step(ag->ag_root, 1);

  TEST_SIZE(su_sendto(s, "", 0, 0, ai->ai_addr, ai->ai_addrlen), 0);

  su_close(s);

  for (i = 0; i < 20; i++)
    su_root_step(ag->ag_root, 1);

  nta_leg_destroy(ag->ag_default_leg), ag->ag_default_leg = NULL;

  if (dir)
    closedir(dir);

#endif /* HAVE_DIRENT_H */

  END();
}

static unsigned char const code[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#include <sofia-sip/su_uniqueid.h>

sip_payload_t *test_payload(su_home_t *home, size_t size)
{
  sip_payload_t *pl = sip_payload_create(home, NULL, (isize_t)size);

  if (pl) {
    size_t i;
    char *data = (char *)pl->pl_data;

    for (i = 0; i < size; i++) {
      if ((i & 63) != 63)
	data[i] = code[su_randint(0, 63)];
      else
	data[i] = '\n';
    }
  }

  return pl;
}

static
int client_check_to_tag(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip)
    TEST_1(sip->sip_to && sip->sip_to->a_tag);
  return 0;
}

static
int check_magic_branch(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip) {
    TEST_1(sip->sip_via);
    TEST_S(sip->sip_via->v_branch, "MagicalBranch");
  }
  return 0;
}

static
int check_via_with_sigcomp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip && sip->sip_via) {
    TEST_S(sip->sip_via->v_comp, "sigcomp");
  }
  return 0;
}

static
int check_via_without_sigcomp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip && sip->sip_via) {
    TEST_1(sip->sip_via->v_comp == NULL);
  }
  return 0;
}

static
int check_via_with_tcp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip && sip->sip_via) {
    TEST_S(sip->sip_via->v_protocol, "SIP/2.0/TCP");
  }
  return 0;
}

static
int check_via_with_sctp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip && sip->sip_via) {
    TEST_S(sip->sip_via->v_protocol, "SIP/2.0/SCTP");
  }
  return 0;
}

static
int check_via_with_udp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (sip && sip->sip_via) {
    TEST_S(sip->sip_via->v_protocol, "SIP/2.0/UDP");
  }
  return 0;
}

static
int save_and_check_tcp(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  if (ctx->c_status >= 200 && ctx->c_extra) {
    tport_t *tport = nta_outgoing_transport(orq);
    TEST_1(tport);
    *(tport_t **)ctx->c_extra = tport;
  }

  return check_via_with_tcp(ctx, orq, sip);
}




/* Test transports */

int test_tports(agent_t *ag)
{
  int udp = 0, tcp = 0, sctp = 0, tls = 0;
  sip_via_t const *v, *v_udp_only = NULL;
  char const *udp_comp = NULL;
  char const *tcp_comp = NULL;
  tport_t *tcp_tport = NULL;

  url_t url[1];

  BEGIN();

  nta_leg_bind(ag->ag_server_leg, leg_callback_200, ag);

  *url = *ag->ag_contact->m_url;
  url->url_port = "*";
  url->url_params = "transport=tcp";

  url->url_params = "transport=udp";

  TEST_1(nta_agent_add_tport(ag->ag_agent, (url_string_t *)url,
			     TAG_END()) == 0);

  TEST_1(v = nta_agent_via(ag->ag_agent));

  for (; v; v = v->v_next) {
    if (su_casematch(v->v_protocol, sip_transport_udp)) {
      if (udp)
	v_udp_only = v;
      udp = 1;
      if (udp_comp == NULL)
	udp_comp = v->v_comp;
    }
    else if (su_casematch(v->v_protocol, sip_transport_tcp)) {
      tcp = 1;
      if (tcp_comp == NULL)
	tcp_comp = v->v_comp;
    }
    else if (su_casematch(v->v_protocol, sip_transport_sctp)) {
      sctp = 1;
    }
    else if (su_casematch(v->v_protocol, sip_transport_tls)) {
      tls = 1;
    }
  }

  *url = *ag->ag_aliases->m_url;
  url->url_user = "bob";

  if (udp_comp || tcp_comp)
    ag->ag_comp = "sigcomp";

  {
    /* Test 0.1
     * Send a message from default leg to default leg
     */
    char const p_acid[] = "P-Access-Network-Info: IEEE-802.11g\n";
    url_t url[1];
    client_t ctx[1] = {{ ag, "Test 0.1", check_via_without_sigcomp }};

    *url = *ag->ag_contact->m_url;
    url->url_params = NULL;
    ag->ag_expect_leg = ag->ag_default_leg;

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_HEADER_STR(p_acid),
			   TAG_END());

    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

    TEST_1(ag->ag_request);
    msg_destroy(ag->ag_request), ag->ag_request = NULL;

    nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);
  }

  {
    /* Test 0.1.2: test url_headers
     *
     * Send a message from default leg to default leg.
     */
    url_t url[1];
    sip_t *sip;
    client_t ctx[1] = {{ ag, "Test 0.1.2", check_via_without_sigcomp }};

    *url = *ag->ag_contact->m_url;
    /* Test that method parameter is stripped and headers in query are used */
    url->url_params = "method=MESSAGE;user=IP";
    url->url_headers = "organization=United%20Testers";
    ag->ag_expect_leg = ag->ag_default_leg;

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    TEST_1(ag->ag_request);
    TEST_1(sip = sip_object(ag->ag_request));

    TEST_1(sip->sip_organization);
    TEST_S(sip->sip_organization->g_string, "United Testers");
    TEST_S(sip->sip_request->rq_url->url_params, "user=IP");

    nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);
  }

  /* Test 0.1.3
   * Send a message from Bob to Alice using SIGCOMP and TCP
   */
  if (tcp_comp) {
    url_t url[1];
    sip_payload_t *pl;
    size_t size = 1024;
    client_t ctx[1] = {{ ag, "Test 0.1.3", check_via_with_sigcomp }};

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    if (url->url_params)
      url->url_params = su_sprintf(NULL, "%s;transport=tcp", url->url_params);
    else
      url->url_params = "transport=tcp";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   NTATAG_COMP("sigcomp"),
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);

    TEST_1(!client_run(ctx, 200));

    TEST_1(ag->ag_client_compartment);
    nta_compartment_decref(&ag->ag_client_compartment);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.2
   * Send a message from Bob to Alice
   * This time specify a TCP URI, and include a large payload
   * of 512 kB
   */
  if (tcp) {
    client_t ctx[1] = {{ ag, "Test 0.2", save_and_check_tcp, }};
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    ctx->c_extra = &tcp_tport;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    url->url_params = "transport=tcp";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   NTATAG_DEFAULT_PROXY(ag->ag_obp),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 200));
    TEST_1(tcp_tport);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  if (tcp_tport) {
    /* Test 0.2.1 - always use transport connection from NTATAG_TPORT()
     *
     * Test bug reported by geaaru
     * - NTATAG_TPORT() is not used if NTATAG_DEFAULT_PROXY() is given
     */
    client_t ctx[1] = {{ ag, "Test 0.2.1", save_and_check_tcp }};
    url_t url[1];
    sip_payload_t *pl;
    tport_t *used_tport = NULL;

    ctx->c_extra = &used_tport;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST(tport_shutdown(tcp_tport, 1), 0); /* Not going to send anymore */

    TEST_1(pl = test_payload(ag->ag_home, 512));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   NTATAG_DEFAULT_PROXY(ag->ag_obp),
			   NTATAG_TPORT(tcp_tport),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 503));

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);

    TEST_1(used_tport == tcp_tport);

    tport_unref(tcp_tport), tcp_tport = NULL;

    if (v_udp_only)		/* Prepare for next test */
      TEST_1(tcp_tport = tport_ref(tport_parent(used_tport)));
    tport_unref(used_tport);
  }

  if (tcp_tport) {
    /* test 0.2.2 - select transport protocol using NTATAG_TPORT()
     *
     * Use primary NTATAG_TPORT() to select transport
     */
    client_t ctx[1] = {{ ag, "Test 0.2.2", save_and_check_tcp }};
    url_t url[1];
    sip_payload_t *pl;
    tport_t *used_tport = NULL;

    ctx->c_extra = &used_tport;
    TEST_1(tport_is_primary(tcp_tport));

    TEST_1(pl = test_payload(ag->ag_home, 512));

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    url->url_host = v_udp_only->v_host;
    url->url_port = v_udp_only->v_port;
    url->url_params = NULL;	/* No sigcomp */

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   (url_string_t *)url,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   NTATAG_TPORT(tcp_tport),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 503));

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);

    TEST_1(used_tport);
    TEST_1(tport_is_tcp(used_tport));
    tport_unref(used_tport);
    tport_unref(tcp_tport), tcp_tport = NULL;
  }

  /* Test 0.3
   * Send a message from Bob to Alice
   * This time include a large payload of 512 kB, let NTA choose transport.
   */
  if (tcp) {
    client_t ctx[1] = {{ ag, "Test 0.3" }};
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.4.1:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, let NTA choose transport.
   */
  {
    client_t ctx[1] = {{ ag, "Test 0.4.1", check_via_with_tcp }};
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);

    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;
  }

  /* Test 0.4.2:
   * Send a message from Bob to Alices UDP-only address
   * This time include a payload of 2 kB, let NTA choose transport.
   */
  if (v_udp_only) {
    client_t ctx[1] = {{ ag, "Test 0.4.2", check_via_with_udp }};
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    url->url_host = v_udp_only->v_host;
    url->url_port = v_udp_only->v_port;
    url->url_params = NULL;	/* No sigcomp */

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_default_leg;

    su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);

    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

    TEST_1(ag->ag_in_via);
    TEST_1(su_casematch(ag->ag_in_via->v_protocol, "SIP/2.0/UDP"));
    su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;
  }

  /* Test 0.5:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, try to use UDP.
   */
  if (udp) {
    client_t ctx[1] = {{ ag, "Test 0.5", check_via_with_udp }};
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TPTAG_MTU(0xffffffff),
			   TAG_END());
    su_free(ag->ag_home, pl);

    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  if (udp) {
    /* Test 0.6
     * Send a message from default leg to server leg
     * using a prefilled Via header
     */
    client_t ctx[1] = {{ ag, "Test 0.6", check_magic_branch }};

    sip_via_t via[1];

    sip_via_init(via);

    via->v_protocol = sip_transport_udp;

    via->v_host = ag->ag_contact->m_url->url_host;
    via->v_port = ag->ag_contact->m_url->url_port;

    sip_via_add_param(ag->ag_home, via, "branch=MagicalBranch");

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_ALIASES(ag->ag_aliases),
			 NTATAG_USER_VIA(1),
			 TAG_END());

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_VIA(via),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_USER_VIA(0),
			 TAG_END());
  }

  /* Test 0.7
   * Send a message from Bob to Alice using SCTP
   */
  if (sctp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 16 * 1024;
    client_t ctx[1] = {{ ag, "Test 0.7", check_via_with_sctp }};

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
#if 0
    if (url->url_params)
      url->url_params = su_sprintf(NULL, "%s;transport=sctp", url->url_params);
    else
#endif
      url->url_params = "transport=sctp";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.8: Send a too large message */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 128 * 1024;
    client_t ctx[1] = {{ ag, "Test 0.8" }};

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_MAXSIZE(65536),
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_PAYLOAD(pl),
			   TAG_END());
    su_free(ag->ag_home, pl);
    TEST_1(!client_run(ctx, 413));
    TEST_P(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_MAXSIZE(2 * 1024 * 1024),
			 TAG_END());
  }

  /* Test 0.9: Timeout */
  {
    url_t url[1];
    client_t ctx[1] = {{ ag, "Test 0.9" }};

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_TIMEOUT_408(1),
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "timeout";
    url->url_port = ag->ag_sink_port;

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_bob),
			   SIPTAG_TO(ag->ag_alice),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   TAG_END());

    TEST_1(!client_run_with(ctx, 408, fast_final_received));
    TEST_P(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent,
			 TAG_END());
  }


  END();
}

int leg_callback_destroy(agent_t *ag,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  ag->ag_latest_leg = leg;

  nta_incoming_destroy(irq);

  return 0;
}

int leg_callback_save(agent_t *ag,
		      nta_leg_t *leg,
		      nta_incoming_t *irq,
		      sip_t const *sip)
{
  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  ag->ag_latest_leg = leg;
  ag->ag_irq = irq;

  ag->ag_running = 0;

  return 0;
}


int test_destroy_incoming(agent_t *ag)
{
  BEGIN();

  url_t url[1];

  *url = *ag->ag_contact->m_url;

  {
    client_t ctx[1] = {{ ag, "Test 3.1" }};

    /* Test 3.1
     * Check that when a incoming request is destroyed in callback,
     * a 500 response is sent
     */
    ag->ag_expect_leg = ag->ag_default_leg;
    nta_leg_bind(ag->ag_default_leg, leg_callback_destroy, ag);

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   TAG_END());
    TEST_1(!client_run(ctx, 500));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 3.2
     * Check that when an incoming request is destroyed, a 500 response is sent
     */
    client_t ctx[1] = {{ ag, "Test 3.2" }};

    nta_leg_bind(ag->ag_default_leg, leg_callback_save, ag);

    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   TAG_END());
    TEST_1(ctx->c_orq);
    nta_test_run(ag);
    TEST(ctx->c_status, 0);
    TEST_1(ag->ag_irq);
    TEST_1(ctx->c_orq);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

    nta_incoming_destroy(ag->ag_irq), ag->ag_irq = NULL;

    TEST_1(!client_run(ctx, 500));
  }

  END();
}

int test_resolv(agent_t *ag, char const *resolv_conf)
{
  int udp = 0, tcp = 0, sctp = 0, tls = 0;
  sip_via_t const *v;

  url_t *url;

  if (!resolv_conf)
    return 0;

  BEGIN();

  nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);

  nta_agent_set_params(ag->ag_agent,
		       NTATAG_SIP_T1(8 * 25),
		       NTATAG_SIP_T1X64(64 * 25),
		       NTATAG_SIP_T4(10 * 25),
		       TAG_END());


  TEST_1(v = nta_agent_via(ag->ag_agent));
  for (; v; v = v->v_next) {
    if (su_casematch(v->v_protocol, sip_transport_udp))
      udp = 1;
    else if (su_casematch(v->v_protocol, sip_transport_tcp))
      tcp = 1;
    else if (su_casematch(v->v_protocol, sip_transport_sctp))
      sctp = 1;
    else if (su_casematch(v->v_protocol, sip_transport_tls))
      tls = 1;
  }

  url = url_hdup(ag->ag_home, (void *)"sip:example.org"); TEST_1(url);

  {
    /* Test 1.1
     * Send a message to sip:example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.1" }};
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.2
     * Send a message to sip:srv.example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.2" }};
    url->url_host = "srv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.3
     * Send a message to sip:ipv.example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.3" }};
    url->url_host = "ipv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.1
     * Send a message to sip:down.example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.4.1" }};
    url->url_host = "down.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

  }

  {
    /* Test 1.4.2
     * Send a message to sip:na503.example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.4.2" }};
    url->url_host = "na503.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 503));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.3
     * Send a message to sip:nona.example.org
     */
    client_t ctx[1] = {{ ag, "Test 1.4.3" }};
    url->url_host = "nona.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.4
     * Send a message to sip:nosrv.example.org
     * After failing to find _sip._udp.nosrv.example.org,
     * second SRV with _sip._udp.srv.example.org succeeds
     */
    client_t ctx[1] = {{ ag, "Test 1.4.4" }};
    url->url_host = "nosrv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.5.1
     * Send a message to sip:srv.example.org;transport=tcp
     * Test outgoing_make_srv_query()
     */
    client_t ctx[1] = {{ ag, "Test 1.5.1: outgoing_make_srv_query()" }};
    url->url_host = "srv.example.org";
    url->url_params = "transport=tcp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.5.2
     * Send a message to sip:srv.example.org;transport=udp
     * Test outgoing_make_srv_query()
     */
    client_t ctx[1] = {{ ag, "Test 1.5.2: outgoing_make_srv_query()" }};

    url->url_host = "srv.example.org";
    url->url_params = "transport=udp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.5.3
     * Send a message to sip:srv2.example.org;transport=udp
     * Test outgoing_query_srv_a()
     */
    client_t ctx[1] = {{ ag, "Test 1.5: outgoing_query_srv_a()" }};

    url->url_host = "srv2.example.org";
    url->url_params = "transport=udp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.6.1
     * Send a message to sip:srv.example.org:$port
     * Test outgoing_make_a_aaaa_query()
     */
    client_t ctx[1] = {{ ag, "Test 1.6.1: outgoing_make_a_aaaa_query()" }};

    url->url_host = "srv.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 503));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.6.2
     * Send a message to sip:a.example.org:$port
     * Test outgoing_make_a_aaaa_query()
     */
    client_t ctx[1] = {{ ag, "Test 1.6.2: outgoing_make_a_aaaa_query()" }};

    url->url_host = "a.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_port = NULL;
  }

#if 0				/* This must be run on host *without* proxy */
  {
    /* Test 1.6c
     * Send a message to sip:na.example.org
     * Test outgoing_query_all() with NAPTR "A" flag
     */
    client_t ctx[1] = {{ ag, "Test 1.6c" }};

    url->url_host = "na.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    TEST_1(ctx->c_orq =
	   nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
				ag->ag_obp,
				SIP_METHOD_MESSAGE,
				(url_string_t *)url,
				SIPTAG_SUBJECT_STR(ctx->c_name),
				SIPTAG_FROM(ag->ag_alice),
				SIPTAG_TO(ag->ag_bob),
				SIPTAG_CONTACT(ag->ag_m_alice),
				TAG_END()));
    TEST_1(!client_run(ctx, 503));
    TEST(ag->ag_latest_leg, ag->ag_default_leg);
  }
#endif

  {
    /* Test 1.7
     * Send a message to sip:down2.example.org:$port
     * Test A record failover.
     */
    client_t ctx[1] = {{ ag, "Test 1.7: outgoing_make_a_aaaa_query()" }};

    url->url_host = "down2.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  if (test_nta_sips) {
    /* Test 1.8 - Send a message to sips:example.org
     *
     * Tests sf.net bug #1292657 (SIPS resolving with NAPTR).
     */
    client_t ctx[1] = {{ ag, "Test 1.8" }};
    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)"sips:example.org",
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  nta_agent_set_params(ag->ag_agent,
		       NTATAG_SIP_T1(500),
		       NTATAG_SIP_T1X64(64 * 500),
		       NTATAG_SIP_T2(NTA_SIP_T2),
		       NTATAG_SIP_T4(NTA_SIP_T4),
		       TAG_END());

  END();
}

/* Test default routing */

int test_routing(agent_t *ag)
{
  url_t url[1];

  *url = *ag->ag_aliases->m_url;
  url->url_user = "bob";

  nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);

  nta_agent_set_params(ag->ag_agent,
		       NTATAG_MAXSIZE(2 * 1024 * 1024),
		       TAG_END());

  BEGIN();

  {
    /*
     * Send a message from default leg to default leg
     *
     * We are now using url with an explicit port that does not match with
     * our own port number.
     */
    url_t url2[1];
    client_t ctx[1] = {{ ag, "Test 1.2" }};

    *url2 = *url;
    url2->url_port = "9";	/* discard service */

    ag->ag_expect_leg = ag->ag_default_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   (url_string_t *)url,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)url2,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  END();
}

/* Test dialogs and the tag handling */

int test_dialog(agent_t *ag)
{
  BEGIN();

  /*
   * Test establishing a dialog
   *
   * Alice sends a message to Bob, then Bob back to the Alice, and again
   * Alice to Bob.
   */

  ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				     leg_callback_200,
				     ag,
				     SIPTAG_FROM(ag->ag_alice),
				     SIPTAG_TO(ag->ag_bob),
				     TAG_END());
  TEST_1(ag->ag_alice_leg);
  TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

  {
    client_t ctx[1] = {{ ag, "Test 2.1" }};

    nta_leg_bind(ag->ag_server_leg, new_leg_callback_200, ag);

    /* Send message from Alice to Bob establishing the dialog */
    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_alice_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_bob_leg != NULL);
  }

  {
    /* Send message from Bob to Alice */
    client_t ctx[1] = {{ ag, "Test 2.2" }};

    nta_leg_bind(ag->ag_server_leg, leg_callback_200, ag);


    ag->ag_expect_leg = ag->ag_alice_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)ag->ag_m_alice->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);
  }

  {
    /* Send again message from Alice to Bob */
    client_t ctx[1] = {{ ag, "Test 2.3" }};
    ag->ag_expect_leg = ag->ag_bob_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_alice_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_bob_leg);
  }

  {
    /* Send message from Bob to Alice
     * This time, however, specify request URI
     */
    client_t ctx[1] = {{ ag, "Test 2.4" }};
    ag->ag_expect_leg = ag->ag_alice_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_MESSAGE,
			   (url_string_t *)ag->ag_m_alice->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);
  }

  nta_leg_destroy(ag->ag_alice_leg), ag->ag_alice_leg = NULL;
  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;

  END();
}

#ifndef MSG_TRUNC
#define MSG_TRUNC 0
#endif

static ssize_t recv_udp(agent_t *ag, void *b, size_t size)
{
  ssize_t n;

  memset(b, 0, size);

  for (;;) {
    su_root_step(ag->ag_root, 10L);
    if (su_wait(ag->ag_sink_wait, 1, 0) == 0) {
      n = su_recv(ag->ag_sink_socket, b, size, MSG_TRUNC);
      if (n > 0)
	return n;
    }
  }
}

/* Test merging  */
int test_merging(agent_t *ag)
{
  BEGIN();

  /*
   * Test merging: send two messages with same
   * from tag/call-id/cseq number to nta,
   * expect 200 and 408.
   */

  char const rfc3261prefix[] = "z9hG4bK";

  char const template[] =
    "%s " URL_PRINT_FORMAT " SIP/2.0\r\n"
    "Via: SIP/2.0/UDP 127.0.0.1:%s;branch=%s.%p\r\n"
    "Via: SIP/2.0/TCP fake.address.for.via.example.net;branch=z9hG4bK.%p\r\n"
    "CSeq: %u %s\r\n"
    "Call-ID: dfsjfhsduifhsjfsfjkfsd.%p@dfsdhfsjkhsdjk\r\n"
    "From: Evil Forker <sip:evel@forker.com>;tag=test_nta-%s\r\n"
    "To: Bob the Builder <sip:bob@example.net>%s\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

  url_t u1[1], u2[2];

  char m1[1024], m2[1024];
  char r1[1024], r2[1024];

  size_t len, l1, l2;
  su_sockaddr_t *su = ag->ag_su_nta;
  socklen_t sulen = ag->ag_su_nta_len;

  /* Empty sink socket */
  su_setblocking(ag->ag_sink_socket, 0);
  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;
  su_wait(ag->ag_sink_wait, 1, 0);
  su_wait(ag->ag_sink_wait, 1, 0);

  {
    /* RFC 3261 8.2.2.2 Merged Requests:

   If the request has no tag in the To header field, the UAS core MUST
   check the request against ongoing transactions.  If the From tag,
   Call-ID, and CSeq exactly match those associated with an ongoing
   transaction, but the request does not match that transaction (based
   on the matching rules in Section 17.2.3), the UAS core SHOULD
   generate a 482 (Loop Detected) response and pass it to the server
   transaction.
    */
    nta_leg_bind(ag->ag_server_leg, leg_callback_200, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;

    *u1 = *ag->ag_m_bob->m_url;
    snprintf(m1, sizeof m1,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m1,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 13, "MESSAGE",
	     /* Call-ID */ (void *)ag,
	     /* From tag */ "2.5.1",
	     /* To tag */ "");
    l1 = strlen(m1);

    *u2 = *ag->ag_m_bob->m_url;

    snprintf(m2, sizeof m2,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u2),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m2,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 13, "MESSAGE",
	     /* Call-ID */ (void *)ag,
	     /* From tag */ "2.5.1",
	     /* To tag */ "");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    len = strlen("SIP/2.0 200 ");
    TEST_1(memcmp(r1, "SIP/2.0 200 ", len) == 0);
    TEST_1(memcmp(r2, "SIP/2.0 482 ", len) == 0);

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  {
    /*
     * Check that request with same call-id, cseq and from-tag
     * are not merged if the method is different.
     */
    nta_leg_bind(ag->ag_server_leg, leg_callback_200, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;

    *u1 = *ag->ag_m_bob->m_url;
    snprintf(m1, sizeof m1,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m1,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 14, "MESSAGE",
	     /* Call-ID */ (void *)ag,
	     /* From tag */ "2.5.2",
	     /* To tag */ "");
    l1 = strlen(m1);

    *u2 = *ag->ag_m_bob->m_url;

    snprintf(m2, sizeof m2,
	     template,
	     "OPTIONS", URL_PRINT_ARGS(u2),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m2,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 14, "OPTIONS",
	     /* Call-ID */ (void *)ag,
	     /* From tag */ "2.5.2",
	     /* To tag */ "");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    len = strlen("SIP/2.0 200 ");
    TEST_1(memcmp(r1, "SIP/2.0 200 ", len) == 0);
    TEST_1(memcmp(r2, "SIP/2.0 482 ", len) != 0);

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  {
    /* test with rfc2543 */

    snprintf(m1, sizeof m1,
	     template,
	     "MASSAGE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, "0.", (void *)0,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 14, "MASSAGE",
	     /* Call-ID */ (void *)(ag + 1),
	     /* From tag */ "2.5.3",
	     /* To tag */ "");
    l1 = strlen(m1);

    u2->url_user = "bob+2";

    snprintf(m2, sizeof m2,
	     template,
	     "MASSAGE", URL_PRINT_ARGS(u2),
	     /* Via */ ag->ag_sink_port, "0.", (void *)0,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 14, "MASSAGE",
	     /* Call-ID */ (void *)(ag + 1),
	     /* From tag */ "2.5.3",
	     /* To tag */ "");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    l1 = strlen("SIP/2.0 200 ");
    TEST_1(memcmp(r1, "SIP/2.0 200 ", l1) == 0);
    TEST_1(memcmp(r2, "SIP/2.0 482 ", l1) == 0);

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  {
    /* test with to-tag */

    snprintf(m1, sizeof m1,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m1,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 15, "MESSAGE",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.5.4",
	     /* To tag */ ";tag=in-dialog");
    l1 = strlen(m1);

    u2->url_user = "bob+2";

    snprintf(m2, sizeof m2,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u2),
	     /* Via */ ag->ag_sink_port, rfc3261prefix, (void *)m2,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 15, "MESSAGE",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.5.4",
	     /* To tag */ ";tag=in-dialog");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    l1 = strlen("SIP/2.0 200 ");
    TEST_1(memcmp(r1, "SIP/2.0 200 ", l1) == 0);
    TEST_1(memcmp(r2, "SIP/2.0 482 ", l1) != 0);

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  {
    /* test with rfc2543 and to-tag */

    snprintf(m1, sizeof m1,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, "0.", (void *)0,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 15, "MESSAGE",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.5.5",
	     /* To tag */ ";tag=in-dialog");
    l1 = strlen(m1);

    snprintf(m2, sizeof m2,
	     template,
	     "MESSAGE", URL_PRINT_ARGS(u2),
	     /* Via */ ag->ag_sink_port, "0.", (void *)0,
	     /* 2nd Via */ (void *)ag,
	     /* CSeq */ 15, "MESSAGE",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.5.5",
	     /* To tag */ ";tag=in-dialog");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    l1 = strlen("SIP/2.0 200 ");
    TEST_1(memcmp(r1, "SIP/2.0 200 ", l1) == 0);
    TEST_1(memcmp(r2, "SIP/2.0 482 ", l1) != 0);

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  {
    /* test INVITE/CANCEL with rfc2543 */
    char const template2[] =
      "%s " URL_PRINT_FORMAT " SIP/2.0\r\n"
      "Via: SIP/2.0/UDP 127.0.0.1:%s;x-kuik=%p\r\n"
      "CSeq: %u %s\r\n"
      "Call-ID: %p.dfsdhfsjkhsdjk.dfsjfhsduifhsjfsfjkfsd\r\n"
      "From: Evil Forker <sip:evel@forker.com>;tag=test_nta-%s\r\n"
      "To: Bob the Builder <sip:bob@example.net>%s\r\n"
      "Content-Length: 0\r\n"
      "\r\n";

    nta_leg_bind(ag->ag_server_leg, new_leg_callback_180, ag);

    snprintf(m1, sizeof m1,
	     template2,
	     "INVITE", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, m1,
	     /* CSeq */ 15, "INVITE",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.6.1",
	     /* To tag */ "");
    l1 = strlen(m1);
    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m1, l1, 0, su, sulen) == l1);
    recv_udp(ag, r1, sizeof r1);

    l1 = strlen("SIP/2.0 180 ");
    TEST_1(memcmp(r1, "SIP/2.0 180 ", l1) == 0);

    TEST_1(ag->ag_irq);
    nta_incoming_bind(ag->ag_irq, wait_for_ack_or_cancel, ag);

    snprintf(m2, sizeof m2,
	     template2,
	     "CANCEL", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, m1,
	     /* CSeq */ 15, "CANCEL",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.6.1",
	     /* To tag */ "");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);
    recv_udp(ag, r1, sizeof r1);
    recv_udp(ag, r2, sizeof r2);

    l1 = strlen("SIP/2.0 200 ");
    TEST_1(strstr(r1, "15 CANCEL"));
    TEST_1(memcmp(r1, "SIP/2.0 200 ", l1) == 0);

    TEST_1(strstr(r2, "15 INVITE"));
    TEST_1(memcmp(r2, "SIP/2.0 487 ", l1) == 0);

    TEST_1(nta_incoming_status(ag->ag_irq) == 487);

    snprintf(m2, sizeof m2,
	     template2,
	     "ACK", URL_PRINT_ARGS(u1),
	     /* Via */ ag->ag_sink_port, m1,
	     /* CSeq */ 15, "ACK",
	     /* Call-ID */ (void *)(ag + 2),
	     /* From tag */ "2.6.1",
	     /* To tag */ "");
    l2 = strlen(m2);

    TEST_1((size_t)su_sendto(ag->ag_sink_socket, m2, l2, 0, su, sulen) == l2);

    nta_leg_destroy(ag->ag_bob_leg); ag->ag_bob_leg = NULL;
  }

  while (su_recv(ag->ag_sink_socket, m1, sizeof m1, MSG_TRUNC) >= 0)
    ;

  END();
}

static
int wait_for_ack_or_cancel(agent_t *ag,
			   nta_incoming_t *irq,
			   sip_t const *sip)
{
  sip_method_t method;

  method = sip ? sip->sip_request->rq_method : sip_method_unknown;

  if (method == sip_method_cancel) {
    nta_incoming_treply(ag->ag_irq, SIP_487_REQUEST_CANCELLED, TAG_END());
  }
  else if (method == sip_method_ack) {
    nta_incoming_destroy(irq);
    ag->ag_irq = NULL;
    ag->ag_running = 0;
  }
  else {			/* Timeout */
    nta_incoming_destroy(irq);
    ag->ag_irq = NULL;
    ag->ag_running = 0;
  }

  return 0;
}


/* ---------------------------------------------------------------------- */
/* Test INVITE, dialogs */

static
int test_for_ack(agent_t *ag,
		 nta_incoming_t *irq,
		 sip_t const *sip)
{
  sip_method_t method;

  BEGIN();

  method = sip ? sip->sip_request->rq_method : sip_method_unknown;

  nta_incoming_destroy(irq);
  TEST_P(irq, ag->ag_irq);
  ag->ag_irq = NULL;

  TEST(method, sip_method_ack);

  ag->ag_running = 0;

  END();
}

static
int test_for_prack(agent_t *ag,
		   nta_reliable_t *rel,
		   nta_incoming_t *prack,
		   sip_t const *sip)
{
  sip_method_t method = sip ? sip->sip_request->rq_method : sip_method_unknown;

  nta_incoming_treply(ag->ag_irq,
		      SIP_200_OK,
		      SIPTAG_CONTACT(ag->ag_m_alice),
		      TAG_END());

  TEST(method, sip_method_prack);

  return 200;
}

int alice_leg_callback(agent_t *ag,
		       nta_leg_t *leg,
		       nta_incoming_t *irq,
		       sip_t const *sip)
{
  BEGIN();

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  if (sip->sip_request->rq_method == sip_method_prack)
    return 481;

  ag->ag_latest_leg = leg;

  if (leg != ag->ag_alice_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }

  if (sip->sip_request->rq_method == sip_method_invite) {
    TEST_1(sip_has_feature(sip->sip_supported, "100rel"));
    nta_incoming_bind(irq, test_for_ack, ag);
    nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_DEBUG_DROP_PROB(ag->ag_drop),
			 TAG_END());

    ag->ag_reliable =
      nta_reliable_treply(irq,
			  NULL, NULL,
			  SIP_183_SESSION_PROGRESS,
			  SIPTAG_CONTENT_TYPE(ag->ag_content_type),
			  SIPTAG_PAYLOAD(ag->ag_payload),
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    ag->ag_reliable =
      nta_reliable_treply(irq,
			  NULL, NULL,
			  184, "Next",
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    ag->ag_reliable =
      nta_reliable_treply(irq,
			  test_for_prack, ag,
			  185, "Last",
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    ag->ag_irq = irq;
    return 0;
  }

  if (sip->sip_request->rq_method == sip_method_bye) {
    leg_zap(ag, leg);
  }
  if (sip)
    return 200;

  END();
}


int bob_leg_callback(agent_t *ag,
		     nta_leg_t *leg,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  BEGIN();

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  if (sip->sip_request->rq_method == sip_method_prack)
    return 481;

  ag->ag_latest_leg = leg;

  if (ag->ag_bob_leg && leg != ag->ag_bob_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }

  if (ag->ag_bob_leg == NULL) {
    nta_leg_bind(leg, leg_callback_500, ag);
    ag->ag_bob_leg = nta_leg_tcreate(ag->ag_agent,
				     bob_leg_callback,
				     ag,
				     SIPTAG_CALL_ID(sip->sip_call_id),
				     SIPTAG_FROM(sip->sip_to),
				     SIPTAG_TO(sip->sip_from),
				     TAG_END());
    TEST_1(ag->ag_bob_leg);
    TEST_1(nta_leg_tag(ag->ag_bob_leg, NULL));
    TEST_1(nta_leg_get_tag(ag->ag_bob_leg));
    TEST_1(nta_incoming_tag(irq, nta_leg_get_tag(ag->ag_bob_leg)));
    TEST(nta_leg_server_route(ag->ag_bob_leg,
			      sip->sip_record_route,
			      sip->sip_contact), 0);
  }

  if (sip->sip_request->rq_method != sip_method_invite) {
    return 200;
  } else {
    nta_incoming_bind(irq, test_for_ack, ag);
#if 1
    nta_incoming_treply(irq,
			SIP_180_RINGING,
			SIPTAG_CONTACT(ag->ag_m_bob),
			TAG_END());
    nta_incoming_treply(irq,
			SIP_180_RINGING,
			SIPTAG_CONTACT(ag->ag_m_bob),
			TAG_END());
#endif
    nta_incoming_treply(irq,
			SIP_200_OK,
			SIPTAG_CONTENT_TYPE(ag->ag_content_type),
			SIPTAG_PAYLOAD(ag->ag_payload),
			SIPTAG_CONTACT(ag->ag_m_bob),
			TAG_END());
    ag->ag_irq = irq;
  }

  END();
}

static
int invite_client_deinit(client_t *c)
{
  agent_t *ag = c->c_ag;
  invite_client_t *ic = (invite_client_t *)c;

  if (ic->ic_orq) nta_outgoing_destroy(ic->ic_orq), ic->ic_orq = NULL;
  if (ic->ic_tag) su_free(ag->ag_home, ic->ic_tag), ic->ic_tag = NULL;

  return 0;
}

static
int check_prack_sending(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = ctx->c_status;

  if (100 < status && status < 200) {
    if (sip->sip_require && sip_has_feature(sip->sip_require, "100rel")) {
      nta_outgoing_t *prack = NULL;

      TEST_1(sip->sip_rseq);

      prack = nta_outgoing_prack(ag->ag_call_leg, orq, NULL, NULL,
				 NULL,
				 sip,
				 TAG_END());
      nta_outgoing_destroy(prack);
      TEST_1(prack != NULL);
    }
  }
  return 0;
}


static
int check_leg_tagging(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = ctx->c_status;

  if (200 <= status && status < 300) {
    TEST_1(nta_leg_rtag(ag->ag_call_leg, sip->sip_to->a_tag));

    TEST(nta_leg_client_route(ag->ag_call_leg,
			      sip->sip_record_route,
			      sip->sip_contact), 0);
  }

  return 0;
}


static
int check_tu_ack(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = ctx->c_status;

  if (200 <= status && status < 300) {
    nta_outgoing_t *ack;
    ack = nta_outgoing_tcreate(ag->ag_call_leg, NULL, NULL,
			       NULL,
			       SIP_METHOD_ACK,
			       NULL,
			       SIPTAG_CSEQ(sip->sip_cseq),
			       TAG_END());
    nta_outgoing_destroy(ack);
    TEST_1(ack);
  }

  return 0;
}


static
int check_final_error(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = ctx->c_status;

  if (status >= 300)
    ag->ag_call_leg = NULL;

  return 0;
}


/** Cancel call after receiving 1XX response */
static
int cancel_invite(client_t *ctx, nta_outgoing_t *orq, sip_t const *sip)
{
  int status = ctx->c_status;

  if (100 < status && status < 200) {
    nta_outgoing_cancel(orq);
    ctx->c_status = 0;
  }
  else if (status >= 200) {
    TEST_1(status == 487 || status == 504);
  }

  return 0;
}

static client_check_f * const checks_for_invite[] = {
  client_check_to_tag,
  check_leg_tagging,
  check_tu_ack,
  check_final_error,
  NULL,
};

static client_check_f * const checks_for_reinvite[] = {
  client_check_to_tag,
  check_prack_sending,
  check_leg_tagging,
  check_tu_ack,
  NULL,
};

int test_call(agent_t *ag)
{
  sip_content_type_t *ct = ag->ag_content_type;
  sip_payload_t      *sdp = ag->ag_payload;
  nta_leg_t *old_leg;
  sip_replaces_t *r1, *r2;

  BEGIN();

  {
    invite_client_t ic[1] =
      {{{{ ag, "Call 1", NULL, checks_for_invite, invite_client_deinit }}}};
    client_t *ctx = ic->ic_client;

    /*
     * Test establishing a call
     *
     * Alice sends a INVITE to Bob, then Bob sends 200 Ok.
     */
    ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				       alice_leg_callback,
				       ag,
				       SIPTAG_FROM(ag->ag_alice),
				       SIPTAG_TO(ag->ag_bob),
				       TAG_END());
    TEST_1(ag->ag_alice_leg);

    TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));
    nta_leg_bind(ag->ag_server_leg, bob_leg_callback, ag);

    /* Send INVITE */
    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_INVITE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_ACCEPT_CONTACT_STR("*;audio"),
			   SIPTAG_PAYLOAD(sdp),
			   NTATAG_USE_TIMESTAMP(1),
			   NTATAG_PASS_100(1),
			   TAG_END());
    TEST_1(ctx->c_orq);
    /* Try to CANCEL it immediately */
    TEST_1(nta_outgoing_cancel(ctx->c_orq) == 0);
    /* As Bob immediately answers INVITE with 200 Ok,
       cancel should be answered with 481 and 200 Ok is returned to INVITE. */
    TEST_1(!client_run(ctx, 200));

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_bob_leg != NULL);
  }

  TEST_1(r1 = nta_leg_make_replaces(ag->ag_alice_leg, ag->ag_home, 0));
  TEST_1(r2 = sip_replaces_format(ag->ag_home,
				  "%s;from-tag=%s;to-tag=%s",
				  r1->rp_call_id,
				  r1->rp_to_tag,
				  r1->rp_from_tag));

  TEST_P(ag->ag_alice_leg, nta_leg_by_replaces(ag->ag_agent, r2));
  TEST_P(ag->ag_bob_leg, nta_leg_by_replaces(ag->ag_agent, r1));

  {
    invite_client_t ic[1] =
      {{{{
	      ag, "Re-INVITE in Call 1",
	      NULL, checks_for_reinvite, invite_client_deinit
      }}}};
    client_t *ctx = ic->ic_client;

    /* Re-INVITE from Bob to Alice.
     *
     * Alice first sends 183, waits for PRACK, then sends 184 and 185,
     * waits for PRACKs, then sends 200, waits for ACK.
     */
    ag->ag_expect_leg = ag->ag_alice_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_bob_leg,
			   outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_INVITE,
			   NULL,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_CONTACT(ag->ag_m_bob),
			   SIPTAG_SUPPORTED_STR("foo"),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);
  }

  {
    client_t ctx[1] = {{ ag, "Hangup" }};

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_DEBUG_DROP_PROB(0),
			 TAG_END());

    /* Send BYE from Bob to Alice */
    old_leg = ag->ag_expect_leg = ag->ag_alice_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_BYE,
			   NULL,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, old_leg);
    TEST_P(ag->ag_alice_leg, NULL);
  }

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;

  END();
}

/* ========================================================================== */
/* Test early dialogs, PRACK */

int test_for_ack_or_timeout(agent_t *ag,
			    nta_incoming_t *irq,
			    sip_t const *sip)
{
  BEGIN();

  sip_method_t method = sip ? sip->sip_request->rq_method : sip_method_unknown;

  if (method == sip_method_ack) {
    TEST(method, sip_method_ack);
    ag->ag_acked = 1;
  }
  else if (method == sip_method_cancel) {
    nta_incoming_treply(irq, SIP_487_REQUEST_CANCELLED, TAG_END());
    ag->ag_canceled = 1;
  }
  else {
    if (ag->ag_bob_leg) {
      nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
    }
  }

  nta_incoming_destroy(irq);
  TEST_P(irq, ag->ag_irq);
  ag->ag_irq = NULL;

  END();
}

/* */
int bob_leg_callback2(agent_t *ag,
		      nta_leg_t *leg,
		      nta_incoming_t *irq,
		      sip_t const *sip)
{
  BEGIN();

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  ag->ag_latest_leg = leg;

  if (ag->ag_bob_leg && leg != ag->ag_bob_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }

  if (ag->ag_bob_leg == NULL) {
    nta_leg_bind(leg, leg_callback_500, ag);
    ag->ag_bob_leg = nta_leg_tcreate(ag->ag_agent,
				     bob_leg_callback,
				     ag,
				     SIPTAG_CALL_ID(sip->sip_call_id),
				     SIPTAG_FROM(sip->sip_to),
				     SIPTAG_TO(sip->sip_from),
				     TAG_END());
    TEST_1(ag->ag_bob_leg);
    TEST_1(nta_leg_tag(ag->ag_bob_leg, NULL));
    TEST_1(nta_leg_get_tag(ag->ag_bob_leg));
    TEST_1(nta_incoming_tag(irq, nta_leg_get_tag(ag->ag_bob_leg)));
    TEST(nta_leg_server_route(ag->ag_bob_leg,
			      sip->sip_record_route,
			      sip->sip_contact), 0);
  }

  if (sip->sip_request->rq_method != sip_method_invite) {
    return 200;
  }

  nta_incoming_bind(irq, test_for_ack_or_timeout, ag);
  nta_incoming_treply(irq,
		      SIP_183_SESSION_PROGRESS,
		      SIPTAG_CONTENT_TYPE(ag->ag_content_type),
		      SIPTAG_PAYLOAD(ag->ag_payload),
		      SIPTAG_CONTACT(ag->ag_m_bob),
		      TAG_END());
  if (0)
    nta_incoming_treply(irq,
			SIP_180_RINGING,
			SIPTAG_CONTENT_TYPE(ag->ag_content_type),
			SIPTAG_PAYLOAD(ag->ag_payload),
			SIPTAG_CONTACT(ag->ag_m_bob),
			TAG_END());
  nta_incoming_treply(irq,
		      SIP_200_OK,
		      SIPTAG_CONTACT(ag->ag_m_bob),
		      TAG_END());
  ag->ag_irq = irq;

  END();
}

/** Fork the original INVITE. */
static
int check_orq_tagging(client_t *ctx,
		      nta_outgoing_t *orq,
		      sip_t const *sip)
{
  agent_t *ag = ctx->c_ag;
  int status = ctx->c_status;
  invite_client_t *ic = (invite_client_t *)ctx;

  if (100 < status &&  status < 200) {
    TEST_1(sip->sip_rseq);
    TEST_1(sip->sip_to->a_tag);

    TEST_1(orq == ctx->c_orq);

    TEST_1(ic); TEST_1(ic->ic_orq == NULL);
    TEST_1(ic->ic_tag == NULL);

    ic->ic_orq = orq;
    ic->ic_tag = su_strdup(ag->ag_home, sip->sip_to->a_tag); TEST_1(ic->ic_tag);
    ic->ic_tag_status = status;

    TEST_S(nta_leg_rtag(ag->ag_call_leg, ic->ic_tag), ic->ic_tag);

    TEST(nta_leg_client_route(ag->ag_call_leg,
			      sip->sip_record_route,
			      sip->sip_contact), 0);

    orq = nta_outgoing_tagged(orq,
			      outgoing_callback,
			      ctx,
			      ic->ic_tag,
			      sip->sip_rseq);
    TEST_1(orq);
    if (ic->ic_orq != ctx->c_orq)
      nta_outgoing_destroy(ctx->c_orq);
    ctx->c_orq = orq;

    TEST_1(ctx->c_checks && ctx->c_checks[0] == check_orq_tagging);

    ctx->c_checks++;
  }

  return 0;
}

static client_check_f * const checks_for_100rel[] = {
  check_orq_tagging,
  client_check_to_tag,
  check_prack_sending,
  check_leg_tagging,
  check_tu_ack,
  NULL,
};



static int process_prack(nta_reliable_magic_t *arg,
			 nta_reliable_t *rel,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  agent_t *ag = (agent_t *)arg;

  if (irq) {
    return 200;
  }
  else if (ag->ag_irq) {
    nta_incoming_treply(ag->ag_irq,
			504, "Reliable Response Timeout",
			TAG_END());
    nta_incoming_destroy(ag->ag_irq);
    return 487;
  }

  return 487;
}

/* respond with 183 when receiving invite */
int bob_leg_callback3(agent_t *ag,
		      nta_leg_t *leg,
		      nta_incoming_t *irq,
		      sip_t const *sip)
{
  BEGIN();

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  ag->ag_latest_leg = leg;

  if (ag->ag_bob_leg && leg != ag->ag_bob_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }

  if (ag->ag_bob_leg == NULL) {
    nta_leg_bind(leg, leg_callback_500, ag);
    ag->ag_bob_leg = nta_leg_tcreate(ag->ag_agent,
				     bob_leg_callback,
				     ag,
				     SIPTAG_CALL_ID(sip->sip_call_id),
				     SIPTAG_FROM(sip->sip_to),
				     SIPTAG_TO(sip->sip_from),
				     TAG_END());
    TEST_1(ag->ag_bob_leg);
    TEST_1(nta_leg_tag(ag->ag_bob_leg, NULL));
    TEST_1(nta_leg_get_tag(ag->ag_bob_leg));
    TEST_1(nta_incoming_tag(irq, nta_leg_get_tag(ag->ag_bob_leg)));
    TEST(nta_leg_server_route(ag->ag_bob_leg,
			      sip->sip_record_route,
			      sip->sip_contact), 0);
  }

  if (sip->sip_request->rq_method != sip_method_invite) {
    return 200;
  }
  else {
    nta_reliable_t *rel;
    nta_incoming_bind(irq, test_for_ack_or_timeout, ag);
    rel = nta_reliable_treply(irq, process_prack, ag,
			      SIP_183_SESSION_PROGRESS,
			      SIPTAG_CONTENT_TYPE(ag->ag_content_type),
			      SIPTAG_PAYLOAD(ag->ag_payload),
			      SIPTAG_CONTACT(ag->ag_m_bob),
			      TAG_END());
    ag->ag_irq = irq;
  }

  END();
}


/*
 * Test establishing a call with an early dialog / 100 rel / timeout
 *
 * Alice sends a INVITE to Bob, then Bob sends 183, Alice sends PRACK,
 * Bob sends 200 to PRACK, Bob sends 200 to INVITE.
 * Bob sends BYE, Alice 200.
 */

int test_prack(agent_t *ag)
{
  BEGIN();

  sip_content_type_t *ct = ag->ag_content_type;
  sip_payload_t      *sdp = ag->ag_payload;
  nta_leg_t *old_leg;

  {
    /* Send a PRACK from default leg, NTA responds to it with error */
    url_t url[1];
    client_t ctx[1] = {{ ag, "Test 1.1" }};

    *url = *ag->ag_aliases->m_url;
    url->url_user = "bob";

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_PRACK,
			   (url_string_t *)url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_RACK_STR("1432432 42332432 INVITE"),
			   TAG_END());
    TEST_1(!client_run(ctx, 481));
    TEST_P(ag->ag_latest_leg, NULL);
  }

  ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				     alice_leg_callback,
				     ag,
				     SIPTAG_FROM(ag->ag_alice),
				     SIPTAG_TO(ag->ag_bob),
				     TAG_END());
  TEST_1(ag->ag_alice_leg);

  TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

  /* Send INVITE */
  {
    invite_client_t ic[1] =
      {{{{ ag, "Call 2",  NULL, checks_for_100rel, invite_client_deinit }}}};
    client_t *ctx = ic->ic_client;

    nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_INVITE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_REQUIRE_STR("100rel"),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());

    TEST_1(!client_run_until_acked(ctx, 200));

    /*TEST(ic->ic_tag_status, 183); */

    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_bob_leg != NULL);
  }

  {
    client_t ctx[1] = {{ ag, "Hangup" }};

    /* Send BYE from Bob to Alice */
    old_leg = ag->ag_expect_leg = ag->ag_alice_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_BYE,
			   NULL,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, old_leg);
    TEST_P(ag->ag_alice_leg, NULL);
  }

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;

  /* Test CANCELing a call after receiving 100rel response */
  ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				     alice_leg_callback,
				     ag,
				     SIPTAG_FROM(ag->ag_alice),
				     SIPTAG_TO(ag->ag_bob),
				     TAG_END());
  TEST_1(ag->ag_alice_leg);

  TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

  {
    invite_client_t ic[1] =
      {{{{
	      ag, "Call 2b",
	      cancel_invite, checks_for_invite, invite_client_deinit
      }}}};
    client_t *ctx = ic->ic_client;

    /* Send INVITE */
    nta_leg_bind(ag->ag_server_leg, bob_leg_callback3, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
			   outgoing_callback, ctx,
			   ag->ag_obp,
			   SIP_METHOD_INVITE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_REQUIRE_STR("100rel"),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());
    TEST_1(!client_run(ctx, 0));
  }

  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg != NULL);

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;

  if (EXPENSIVE_CHECKS) {
    printf("%s: starting 100rel timeout test, test will complete in 4 seconds\n",
	   name);

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_SIP_T1(25),
			      NTATAG_SIP_T1X64(64 * 25),
			      TAG_END()), 2);

    ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				       alice_leg_callback,
				       ag,
				       SIPTAG_FROM(ag->ag_alice),
				       SIPTAG_TO(ag->ag_bob),
				       TAG_END());
    TEST_1(ag->ag_alice_leg);

    TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

    {
      invite_client_t ic[1] =
	{{{{ ag, "Call 3",  NULL, checks_for_invite, invite_client_deinit }}}};
      client_t *ctx = ic->ic_client;

      /* Send INVITE,
       * send precious provisional response
       * do not send PRACK,
       * timeout (after 64 * t1 ~ 3.2 seconds),
       */
      nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
      ag->ag_expect_leg = ag->ag_server_leg;
      ctx->c_orq =
	nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
			     outgoing_callback, ctx,
			     ag->ag_obp,
			     SIP_METHOD_INVITE,
			     (url_string_t *)ag->ag_m_bob->m_url,
			     SIPTAG_SUBJECT_STR(ctx->c_name),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     SIPTAG_REQUIRE_STR("100rel"),
			     SIPTAG_CONTENT_TYPE(ct),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
      TEST_1(ctx->c_orq);

      nta_test_run(ag);
      TEST(ctx->c_status, 503);
      TEST_P(ctx->c_orq, NULL);
      TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
      TEST_1(ag->ag_bob_leg == NULL);
    }

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_SIP_T1(500),
			      NTATAG_SIP_T1X64(64 * 500),
			      TAG_END()), 2);
  }

  if (EXPENSIVE_CHECKS || 1) {
    /*
     * client sends INVITE,
     * server sends provisional response,
     * client PRACKs it,
     * client timeouts after timer C
     */

    invite_client_t ic[1] =
      {{{{ ag, "Call 4",  NULL, checks_for_100rel, invite_client_deinit }}}};
    client_t *ctx = ic->ic_client;

    printf("%s: starting timer C, test will complete in 1 seconds\n",
	   name);

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_TIMER_C(1000),
			      TAG_END()), 1);

    TEST_1(ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
					      alice_leg_callback,
					      ag,
					      SIPTAG_FROM(ag->ag_alice),
					      SIPTAG_TO(ag->ag_bob),
					      TAG_END()));
    TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

    nta_leg_bind(ag->ag_server_leg, bob_leg_callback3, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ctx->c_orq =
	   nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
				outgoing_callback, ic->ic_client,
				ag->ag_obp,
				SIP_METHOD_INVITE,
				(url_string_t *)ag->ag_m_bob->m_url,
				SIPTAG_SUBJECT_STR(ctx->c_name),
				SIPTAG_CONTACT(ag->ag_m_alice),
				SIPTAG_REQUIRE_STR("100rel"),
				SIPTAG_CONTENT_TYPE(ct),
				SIPTAG_PAYLOAD(sdp),
				TAG_END()));

    /* Run until 1) server gets CANCEL and 2) client gets 487 */
    /* Note: this has been changed in 1.12.11 */
    TEST_1(!client_run_until_canceled(ctx, 487));

    TEST_1(ag->ag_canceled != 0);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_bob_leg);
    nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_TIMER_C(185 * 1000),
			      TAG_END()), 1);

    nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
    ag->ag_latest_leg = NULL;
    ag->ag_call_leg = NULL;
  }

  END();
}

int alice_leg_callback2(agent_t *ag,
			nta_leg_t *leg,
			nta_incoming_t *irq,
			sip_t const *sip)
{
  BEGIN();

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  if (sip->sip_request->rq_method == sip_method_prack)
    return 481;

  ag->ag_latest_leg = leg;

  if (leg != ag->ag_alice_leg) {
    leg_match(ag, leg, 1, __func__);
    return 500;
  }

  if (sip->sip_request->rq_method == sip_method_invite) {
    TEST_1(sip_has_feature(sip->sip_supported, "100rel"));
    nta_incoming_bind(irq, test_for_ack, ag);
    nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_DEBUG_DROP_PROB(ag->ag_drop),
			 TAG_END());
    ag->ag_reliable =
      nta_reliable_treply(irq,
			  NULL, NULL,
			  SIP_183_SESSION_PROGRESS,
			  SIPTAG_CONTENT_TYPE(ag->ag_content_type),
			  SIPTAG_PAYLOAD(ag->ag_payload),
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    ag->ag_reliable =
      nta_reliable_treply(irq,
			  NULL, NULL,
			  184, "Next",
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    ag->ag_reliable =
      nta_reliable_treply(irq,
			  NULL, NULL,
			  185, "Last",
			  SIPTAG_CONTACT(ag->ag_m_alice),
			  TAG_END());
    TEST_1(ag->ag_reliable);
    TEST(nta_incoming_treply(irq, SIP_200_OK, TAG_END()), 0);
    ag->ag_irq = irq;
    return 0;
  }

  if (sip->sip_request->rq_method == sip_method_bye) {
    leg_zap(ag, leg);
  }

  if(sip)
    return 200;

  END();
}
/*
 * Test establishing a call with an early dialog / 100 rel / timeout
 *
 * Alice sends a INVITE to Bob, then Bob sends 183, 184, 185, and 200.
 * Bob sends BYE, Alice 200.
 *
 * See bug #467.
 */
int test_fix_467(agent_t *ag)
{
  sip_content_type_t *ct = ag->ag_content_type;
  sip_payload_t      *sdp = ag->ag_payload;
  nta_leg_t *old_leg;

  BEGIN();

  ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent,
				     alice_leg_callback2,
				     ag,
				     SIPTAG_FROM(ag->ag_alice),
				     SIPTAG_TO(ag->ag_bob),
				     TAG_END());
  TEST_1(ag->ag_alice_leg);

  TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));
  ag->ag_bob_leg = NULL;

  {
    invite_client_t ic[1] =
      {{{{ ag, "Call 5",  NULL, checks_for_100rel, invite_client_deinit }}}};
    client_t *ctx = ic->ic_client;

    /* Send INVITE */
    nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
    ag->ag_expect_leg = ag->ag_server_leg;
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg,
			   outgoing_callback, ic->ic_client,
			   ag->ag_obp,
			   SIP_METHOD_INVITE,
			   (url_string_t *)ag->ag_m_bob->m_url,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_REQUIRE_STR("100rel"),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());

    TEST_1(!client_run(ctx, 200));

    /*TEST(ag->ag_tag_status, 183);*/
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_bob_leg != NULL);
  }

  old_leg = ag->ag_expect_leg = ag->ag_alice_leg;

  {
    client_t ctx[1] = {{ ag, "Hangup" }};

    /* Send BYE from Bob to Alice */
    ctx->c_orq =
      nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ctx,
			   NULL,
			   SIP_METHOD_BYE,
			   NULL,
			   SIPTAG_SUBJECT_STR(ctx->c_name),
			   SIPTAG_FROM(ag->ag_alice),
			   SIPTAG_TO(ag->ag_bob),
			   SIPTAG_CONTACT(ag->ag_m_alice),
			   SIPTAG_CONTENT_TYPE(ct),
			   SIPTAG_PAYLOAD(sdp),
			   TAG_END());
    TEST_1(!client_run(ctx, 200));
    TEST_P(ag->ag_latest_leg, old_leg);
    TEST_P(ag->ag_alice_leg, NULL);
  }

  END();
  /*
    nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
    ag->ag_latest_leg = NULL;
    ag->ag_call_leg = NULL;
  */
}

#if HAVE_ALARM
#include <unistd.h>
#include <signal.h>

static RETSIGTYPE sig_alarm(int s)
{
  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}
#endif

static
char const nta_test_usage[] =
  "usage: %s OPTIONS\n"
  "where OPTIONS are\n"
  "   -v | --verbose    be verbose\n"
  "   -a | --abort      abort() on error\n"
  "   -q | --quiet      be quiet\n"
  "   --expensive       run expensive tests, too\n"
  "   -1                quit on first error\n"
  "   -l level          set logging level (0 by default)\n"
  "   -p uri            specify uri of outbound proxy\n"
  "   -m uri            bind to local uri\n"
  "   --attach          print pid, wait for a debugger to be attached\n"
#if HAVE_ALARM
  "   --no-alarm        don't ask for guard ALARM\n"
#endif
  ;

void usage(int exitcode)
{
  fprintf(stderr, nta_test_usage, name);
  exit(exitcode);
}

#if HAVE_OPEN_C
int posix_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
  int retval;

  tstflags |= tst_verbatim;

  su_log_set_level(su_log_default, 9);
  su_log_set_level(nta_log, 9);
  su_log_set_level(tport_log, 9);

  retval = posix_main(argc, argv);

  sleep(7);

  return retval;
}

#define main posix_main
#endif

int main(int argc, char *argv[])
{
  int retval = 0, quit_on_single_failure = 0;
  int i, o_attach = 0, o_alarm = 1;

  agent_t ag[1] = {{ { SU_HOME_INIT(ag) }, 0, NULL }};

  expensive_checks = getenv("EXPENSIVE_CHECKS") != NULL;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--abort") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
      tstflags &= ~tst_verbatim;
    else if (strcmp(argv[i], "--expensive") == 0)
      expensive_checks = 1;
    else if (strcmp(argv[i], "-1") == 0)
      quit_on_single_failure = 1;
    else if (strncmp(argv[i], "-l", 2) == 0) {
      int level = 3;
      char *rest = NULL;

      if (argv[i][2])
	level = strtol(argv[i] + 2, &rest, 10);
      else if (argv[i + 1])
	level = strtol(argv[i + 1], &rest, 10), i++;
      else
	level = 3, rest = "";

      if (rest == NULL || *rest)
	usage(1);

      su_log_set_level(nta_log, level);
      su_log_set_level(tport_log, level);
    }
    else if (strncmp(argv[i], "-p", 2) == 0) {
      if (argv[i][2])
	ag->ag_obp = (url_string_t *)(argv[i] + 2);
      else if (argv[i + 1])
	ag->ag_obp = (url_string_t *)(argv[++i]);
      else
	usage(1);
    }
    else if (strncmp(argv[i], "-m", 2) == 0) {
      if (argv[i][2])
	ag->ag_m = argv[i] + 2;
      else if (argv[i + 1])
	ag->ag_m = argv[++i];
      else
	usage(1);
    }
    else if (strcmp(argv[i], "--attach") == 0) {
      o_attach = 1;
    }
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

  if (o_attach) {
    char line[10], *got;
    printf("nua_test: pid %u\n", getpid());
    printf("<Press RETURN to continue>\n");
    got = fgets(line, sizeof line, stdin); (void)got;
  }
#if HAVE_ALARM
  else if (o_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

  su_init();

  if (!(TSTFLAGS & tst_verbatim)) {
    su_log_soft_set_level(nta_log, 0);
    su_log_soft_set_level(tport_log, 0);
  }

#define SINGLE_FAILURE_CHECK()						\
  do { fflush(stdout);							\
    if (retval && quit_on_single_failure) { su_deinit(); return retval; } \
  } while(0)

  retval |= test_init(ag, argv[i]); SINGLE_FAILURE_CHECK();
  if (retval == 0) {
    retval |= test_bad_messages(ag); SINGLE_FAILURE_CHECK();
    retval |= test_reinit(ag); SINGLE_FAILURE_CHECK();
    retval |= test_merging(ag); SINGLE_FAILURE_CHECK();
    retval |= test_tports(ag); SINGLE_FAILURE_CHECK();
    retval |= test_destroy_incoming(ag); SINGLE_FAILURE_CHECK();
    retval |= test_resolv(ag, argv[i]); SINGLE_FAILURE_CHECK();
    retval |= test_routing(ag); SINGLE_FAILURE_CHECK();
    retval |= test_dialog(ag); SINGLE_FAILURE_CHECK();
    retval |= test_call(ag); SINGLE_FAILURE_CHECK();
    retval |= test_prack(ag); SINGLE_FAILURE_CHECK();
    retval |= test_fix_467(ag); SINGLE_FAILURE_CHECK();
  }

  s2_fast_forward(64000, ag->ag_root);

  retval |= test_deinit(ag); fflush(stdout);

  su_home_deinit(ag->ag_home);

  su_deinit();

  return retval;
}
