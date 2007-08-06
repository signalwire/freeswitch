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

#define SU_ROOT_MAGIC_T      agent_t

#include <sofia-sip/su_wait.h>

#include <msg_internal.h>

#define NTA_AGENT_MAGIC_T    agent_t
#define NTA_LEG_MAGIC_T      agent_t
#define NTA_OUTGOING_MAGIC_T agent_t
#define NTA_INCOMING_MAGIC_T agent_t
#define NTA_RELIABLE_MAGIC_T agent_t

#include "sofia-sip/nta.h"
#include "nta_internal.h"
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/htable.h>
#include <sofia-sip/sresolv.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/sofia_features.h>
#include <sofia-sip/hostdomain.h>

#include <sofia-sip/string0.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

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

int expensive_checks;

#define EXPENSIVE_CHECKS (expensive_checks)

struct sigcomp_compartment;

char const name[] = "test_nta";

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
  int             ag_status;
  unsigned        ag_canceled:1, ag_acked:1, :0;

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
  nta_leg_t      *ag_tag_remote; /**< If this is set, outgoing_callback()
				  *   tags it with the tag from remote.
				  */
  int             ag_tag_status; /**< Which response established dialog */
  msg_param_t     ag_call_tag;	 /**< Tag used to establish dialog */

  nta_reliable_t *ag_reliable;

  sip_via_t      *ag_out_via;	/**< Outgoing via */
  sip_via_t      *ag_in_via;	/**< Incoming via */

  sip_content_type_t *ag_content_type;
  sip_payload_t  *ag_payload;

  msg_t          *ag_probe_msg;

  /* Dummy servers */
  char const     *ag_sink_port;
  su_socket_t     ag_sink_socket, ag_down_socket;
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


int outgoing_callback(agent_t *ag,
		      nta_outgoing_t *orq,
		      sip_t const *sip)
{
  BEGIN();

  int status = sip->sip_status->st_status;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s %03d %s\n", name, __func__, 
	   sip->sip_status->st_version, 
	   sip->sip_status->st_status, 
	   sip->sip_status->st_phrase);
  }

  TEST_P(orq, ag->ag_orq);

  ag->ag_status = status;

  if (status < 200)
    return 0;

  if (ag->ag_comp) {
    nta_compartment_decref(&ag->ag_client_compartment);
    ag->ag_client_compartment = nta_outgoing_compartment(orq);
  }

  if (ag->ag_out_via == NULL)
    ag->ag_out_via = sip_via_dup(ag->ag_home, sip->sip_via);

  if (ag->ag_tag_remote) {
    TEST_S(nta_leg_rtag(ag->ag_tag_remote, sip->sip_to->a_tag), 
	   sip->sip_to->a_tag);
    ag->ag_tag_remote = NULL;
  }

  TEST_1(sip->sip_to && sip->sip_to->a_tag);

  nta_outgoing_destroy(orq);
  ag->ag_orq = NULL;

  END();
}

static
int test_magic_branch(agent_t *ag, sip_t const *sip) 
{
  BEGIN();
  
  if (sip) {
    TEST_1(sip->sip_via);
    TEST_S(sip->sip_via->v_branch, "MagicalBranch");
  }

  END();
}

static
int magic_callback(agent_t *ag,
		   nta_outgoing_t *orq,
		   sip_t const *sip)
{
  test_magic_branch(ag, sip);
  return outgoing_callback(ag, orq, sip);
}

void 
nta_test_run(agent_t *ag)
{
  for (ag->ag_status = 0; ag->ag_status < 200;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
}

void 
nta_test_run_until_acked(agent_t *ag)
{
  ag->ag_status = 0;
  for (ag->ag_acked = 0; !ag->ag_acked;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
}

void 
nta_test_run_until_canceled(agent_t *ag)
{
  ag->ag_status = 0;
  for (ag->ag_canceled = 0; !ag->ag_canceled;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);
  }
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
  if (str0cmp(getenv("ipv6"), "true") == 0) {
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

  ag->ag_sink_port = su_sprintf(ag->ag_home, "%u", ntohs(su.su_sin.sin_port));
  ag->ag_sink_socket = s;

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

  {
    /* Initialize our headers */
    sip_from_t from[1];
    sip_to_t to[1];
    sip_contact_t m[1];

    sip_from_init(from);
    sip_to_init(to);
    sip_contact_init(m);

    TEST_1(ag->ag_contact = nta_agent_contact(ag->ag_agent));

    *m->m_url = *ag->ag_contact->m_url;
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
		      NTATAG_USE_NAPTR(1),
		      NTATAG_USE_SRV(1),
		      NTATAG_MAX_FORWARDS(20),
		      TAG_END());
    TEST(err, 6);

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

  su_free(ag->ag_home, (void *)ag->ag_sink_port), ag->ag_sink_port = NULL;

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

/* Test transports */

int test_tports(agent_t *ag)
{
  int udp = 0, tcp = 0, sctp = 0, tls = 0;
  sip_via_t const *v, *v_udp_only = NULL;
  char const *udp_comp = NULL;
  char const *tcp_comp = NULL;

  url_t url[1];

  BEGIN();

  *url = *ag->ag_contact->m_url;
  url->url_port = "*";
  url->url_params = "transport=tcp";

  url->url_params = "transport=udp";

  TEST_1(nta_agent_add_tport(ag->ag_agent, (url_string_t *)url, 
			     TAG_END()) == 0);

  TEST_1(v = nta_agent_via(ag->ag_agent));

  for (; v; v = v->v_next) {
    if (strcasecmp(v->v_protocol, sip_transport_udp) == 0) {
      if (udp)
	v_udp_only = v;
      udp = 1;
      if (udp_comp == NULL)
	udp_comp = v->v_comp;
    }
    else if (strcasecmp(v->v_protocol, sip_transport_tcp) == 0) {
      tcp = 1;
      if (tcp_comp == NULL)
	tcp_comp = v->v_comp;
    }
    else if (strcasecmp(v->v_protocol, sip_transport_sctp) == 0) {
      sctp = 1;
    }
    else if (strcasecmp(v->v_protocol, sip_transport_tls) == 0) {
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

    *url = *ag->ag_contact->m_url;
    url->url_params = NULL;
    ag->ag_expect_leg = ag->ag_default_leg;
    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, 
			       outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.1"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       SIPTAG_HEADER_STR(p_acid),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    TEST_1(ag->ag_request);

    msg_destroy(ag->ag_request), ag->ag_request = NULL;

    TEST_1(ag->ag_out_via->v_comp == NULL);

    nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);
  }

  {
    /* Test 0.1.2: test url_headers
     *
     * Send a message from default leg to default leg.
     */
    url_t url[1];
    sip_t *sip;
    
    *url = *ag->ag_contact->m_url;
    /* Test that method parameter is stripped and headers in query are used */
    url->url_params = "method=MESSAGE;user=IP";
    url->url_headers = "organization=United%20Testers";
    ag->ag_expect_leg = ag->ag_default_leg;
    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, 
			       outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.1.2"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    TEST_1(ag->ag_request);
    TEST_1(sip = sip_object(ag->ag_request));

    TEST_1(sip->sip_organization);
    TEST_S(sip->sip_organization->g_string, "United Testers");
    TEST_S(sip->sip_request->rq_url->url_params, "user=IP");
    
    TEST_1(ag->ag_out_via->v_comp == NULL);

    nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);
  }

  /* Test 0.1.3
   * Send a message from Bob to Alice using SIGCOMP and TCP
   */
  if (tcp_comp) {
    url_t url[1];
    sip_payload_t *pl;
    size_t size = 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    if (url->url_params)
      url->url_params = su_sprintf(NULL, "%s;transport=tcp", url->url_params);
    else
      url->url_params = "transport=tcp";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_orq = 
	   nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
				ag->ag_obp,
				SIP_METHOD_MESSAGE,
				(url_string_t *)url,
				NTATAG_COMP("sigcomp"),
				SIPTAG_SUBJECT_STR("Test 0.1.3"),
				SIPTAG_FROM(ag->ag_bob),
				SIPTAG_TO(ag->ag_alice),
				SIPTAG_CONTACT(ag->ag_m_bob),
				SIPTAG_PAYLOAD(pl),
				TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_1(ag->ag_client_compartment);
    nta_compartment_decref(&ag->ag_client_compartment);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_S(ag->ag_out_via->v_comp, "sigcomp");
  }

  /* Test 0.2
   * Send a message from Bob to Alice
   * This time specify a TCP URI, and include a large payload 
   * of 512 kB
   */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    url->url_params = "transport=tcp";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_orq = 
    nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
		       NULL,
		       SIP_METHOD_MESSAGE,
		       (url_string_t *)url,
		       SIPTAG_SUBJECT_STR("Test 0.2"),
		       SIPTAG_FROM(ag->ag_bob),
		       SIPTAG_TO(ag->ag_alice),
		       SIPTAG_CONTACT(ag->ag_m_bob),
		       SIPTAG_PAYLOAD(pl),
		       NTATAG_DEFAULT_PROXY(ag->ag_obp),
		       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.3
   * Send a message from Bob to Alice
   * This time include a large payload of 512 kB, let NTA choose transport.
   */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_orq = 
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
		       ag->ag_obp,
		       SIP_METHOD_MESSAGE,
		       (url_string_t *)url,
		       SIPTAG_SUBJECT_STR("Test 0.3"),
		       SIPTAG_FROM(ag->ag_bob),
		       SIPTAG_TO(ag->ag_alice),
		       SIPTAG_CONTACT(ag->ag_m_bob),
		       SIPTAG_PAYLOAD(pl),
		       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.4.1:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, let NTA choose transport.
   */
  {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));
    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_orq = 
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
		       ag->ag_obp,
		       SIP_METHOD_MESSAGE,
		       (url_string_t *)url,
		       SIPTAG_SUBJECT_STR("Test 0.4.1"),
		       SIPTAG_FROM(ag->ag_bob),
		       SIPTAG_TO(ag->ag_alice),
		       SIPTAG_CONTACT(ag->ag_m_bob),
		       SIPTAG_PAYLOAD(pl),
		       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_out_via);
    TEST_1(strcasecmp(ag->ag_out_via->v_protocol, "SIP/2.0/TCP") == 0 ||
	   strcasecmp(ag->ag_out_via->v_protocol, "SIP/2.0/SCTP") == 0);
    su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;
  }

  /* Test 0.4.2:
   * Send a message from Bob to Alices UDP-only address
   * This time include a payload of 2 kB, let NTA choose transport.
   */
  if (v_udp_only) {
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

    ag->ag_orq = 
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
		       ag->ag_obp,
		       SIP_METHOD_MESSAGE,
		       (url_string_t *)url,
		       SIPTAG_SUBJECT_STR("Test 0.4.2"),
		       SIPTAG_FROM(ag->ag_bob),
		       SIPTAG_TO(ag->ag_alice),
		       SIPTAG_CONTACT(ag->ag_m_bob),
		       SIPTAG_PAYLOAD(pl),
		       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    TEST_1(ag->ag_in_via);
    TEST_1(strcasecmp(ag->ag_in_via->v_protocol, "SIP/2.0/UDP") == 0);
    su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;
  }

  /* Test 0.5:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, try to use UDP.
   */
  if (udp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_orq = 
      nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
		       ag->ag_obp,
		       SIP_METHOD_MESSAGE,
		       (url_string_t *)url,
		       SIPTAG_SUBJECT_STR("Test 0.5"),
		       SIPTAG_FROM(ag->ag_bob),
		       SIPTAG_TO(ag->ag_alice),
		       SIPTAG_CONTACT(ag->ag_m_bob),
		       SIPTAG_PAYLOAD(pl),
		       TPTAG_MTU(0xffffffff),
		       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_out_via);
    TEST_S(ag->ag_out_via->v_protocol, "SIP/2.0/UDP");
  }

  if (udp) {
    /* Send a message from default leg to server leg 
     * using a prefilled Via header
     */
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
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, 
			       magic_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.6"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       SIPTAG_VIA(via),
			       TAG_END());
    TEST_1(ag->ag_orq);
    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
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
    ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.7"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END());
   TEST_1(ag->ag_orq);
   su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.8: Send a too large message */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 128 * 1024;

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_MAXSIZE(65536),
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.8"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
	    	       TAG_END());
    TEST_1(ag->ag_orq);
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 413);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_MAXSIZE(2 * 1024 * 1024),
			 TAG_END());
  }

  /* Test 0.9: Timeout */
  {
    url_t url[1];

    printf("%s: starting MESSAGE timeout test, completing in 4 seconds\n",
	   name);

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_TIMEOUT_408(1),
			 NTATAG_SIP_T1(25), 
			 NTATAG_SIP_T1X64(64 * 25), 
			 NTATAG_SIP_T2(8 * 25),
			 NTATAG_SIP_T4(10 * 25),
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "timeout";
    url->url_port = ag->ag_sink_port;

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.9"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       TAG_END());

    TEST_1(ag->ag_orq);
    nta_test_run(ag);
    TEST(ag->ag_status, 408);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_SIP_T1(500),
			 NTATAG_SIP_T1X64(64 * 500),
			 NTATAG_SIP_T2(NTA_SIP_T2),
			 NTATAG_SIP_T4(NTA_SIP_T4),
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
  ag->ag_status = 1000;

  return 0;
}


int test_destroy_incoming(agent_t *ag)
{
  BEGIN();

  url_t url[1];

  *url = *ag->ag_contact->m_url;

  /* Test 3.1
   * Check that when a incoming request is destroyed in callback, 
   * a 500 response is sent
   */
  ag->ag_expect_leg = ag->ag_default_leg;
  nta_leg_bind(ag->ag_default_leg, leg_callback_destroy, ag);

  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_default_leg, 
			      outgoing_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_MESSAGE,
			      (url_string_t *)url,
			      SIPTAG_SUBJECT_STR("Test 3.1"),
			      SIPTAG_FROM(ag->ag_alice),
			      SIPTAG_TO(ag->ag_bob),
			      TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 500);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

  /* Test 3.1
   * Check that when a incoming request is destroyed, a 500 response is sent
   */
  nta_leg_bind(ag->ag_default_leg, leg_callback_save, ag);

  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_default_leg, 
			      outgoing_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_MESSAGE,
			      (url_string_t *)url,
			      SIPTAG_SUBJECT_STR("Test 3.1"),
			      SIPTAG_FROM(ag->ag_alice),
			      SIPTAG_TO(ag->ag_bob),
			      TAG_END());
  TEST_1(ag->ag_orq);

  nta_test_run(ag);
  TEST(ag->ag_status, 1000);
  TEST_1(ag->ag_irq);
  TEST_1(ag->ag_orq);
  TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  nta_incoming_destroy(ag->ag_irq), ag->ag_irq = NULL;
  nta_test_run(ag);
  TEST(ag->ag_status, 500);
  TEST_P(ag->ag_orq, NULL);

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
    if (strcasecmp(v->v_protocol, sip_transport_udp) == 0)
      udp = 1;
    else if (strcasecmp(v->v_protocol, sip_transport_tcp) == 0)
      tcp = 1;
    else if (strcasecmp(v->v_protocol, sip_transport_sctp) == 0)
      sctp = 1;
    else if (strcasecmp(v->v_protocol, sip_transport_tls) == 0)
      tls = 1;
  }

  url = url_hdup(ag->ag_home, (void *)"sip:example.org"); TEST_1(url);

  {
    /* Test 1.1
     * Send a message to sip:example.org
     */
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.1"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);
    
    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.2
     * Send a message to sip:srv.example.org
     */
    url->url_host = "srv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.2"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);
    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.3
     * Send a message to sip:ipv.example.org
     */
    url->url_host = "ipv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.3"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.1
     * Send a message to sip:down.example.org
     */
    url->url_host = "down.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.4.1"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
   TEST_1(ag->ag_orq);

   nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);

  }

  {
    /* Test 1.4.2
     * Send a message to sip:na503.example.org
     */
    url->url_host = "na503.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.4.2"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 503);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.3
     * Send a message to sip:nona.example.org
     */
    url->url_host = "nona.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.4.3"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.4.4
     * Send a message to sip:nosrv.example.org
     * After failing to find _sip._udp.nosrv.example.org,
     * second SRV with _sip._udp.srv.example.org succeeds
     */
    url->url_host = "nosrv.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.4.4"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.5.1 
     * Send a message to sip:srv.example.org;transport=tcp
     * Test outgoing_make_srv_query()
     */
    url->url_host = "srv.example.org";
    url->url_params = "transport=tcp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.5.1: outgoing_make_srv_query()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.5.2
     * Send a message to sip:srv.example.org;transport=udp
     * Test outgoing_make_srv_query()
     */
    url->url_host = "srv.example.org";
    url->url_params = "transport=udp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.5.2: outgoing_make_srv_query()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.5.3
     * Send a message to sip:srv2.example.org;transport=udp
     * Test outgoing_query_srv_a()
     */
    url->url_host = "srv2.example.org";
    url->url_params = "transport=udp";
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.5: outgoing_query_srv_a()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

  {
    /* Test 1.6.1
     * Send a message to sip:srv.example.org:$port
     * Test outgoing_make_a_aaaa_query()
     */
    url->url_host = "srv.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.6.1: outgoing_make_a_aaaa_query()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 503);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
  }

  {
    /* Test 1.6.2
     * Send a message to sip:a.example.org:$port
     * Test outgoing_make_a_aaaa_query()
     */
    url->url_host = "a.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.6.2: outgoing_make_a_aaaa_query()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_port = NULL;
  }

#if 0				/* This must be run on host *without* proxy */
  {
    /* Test 1.6c
     * Send a message to sip:na.example.org
     * Test outgoing_query_all() with NAPTR "A" flag
     */
    url->url_host = "na.example.org";
    ag->ag_expect_leg = ag->ag_default_leg;
    TEST_1(ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.6c"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END()));
    nta_test_run(ag);
    TEST(ag->ag_status, 503);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_default_leg);
  }
#endif

  {
    /* Test 1.7
     * Send a message to sip:down2.example.org:$port
     * Test A record failover.
     */
    url->url_host = "down2.example.org";
    url->url_port = ag->ag_contact->m_url->url_port;
    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.7: outgoing_make_a_aaaa_query()"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_default_leg);
    url->url_params = NULL;
  }

#if 0
  /* Test 0.1.1
   * Send a message from Bob to Alice using SIGCOMP and TCP
   */
  if (tcp_comp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
    if (url->url_params)
      url->url_params = su_sprintf(NULL, "%s;transport=tcp", url->url_params);
    else
      url->url_params = "transport=tcp";

    TEST_1(pl = test_payload(ag->ag_home, size));
    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
	   nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
				ag->ag_obp,
				SIP_METHOD_MESSAGE,
				(url_string_t *)url,
				SIPTAG_SUBJECT_STR("Test 0.1.1"),
				SIPTAG_FROM(ag->ag_bob),
				SIPTAG_TO(ag->ag_alice),
				SIPTAG_CONTACT(ag->ag_m_bob),
				SIPTAG_PAYLOAD(pl),
				TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.2
   * Send a message from Bob to Alice
   * This time specify a TCP URI, and include a large payload 
   * of 512 kB
   */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";
#if 0
    if (url->url_params)
      url->url_params = su_sprintf(NULL, "%s;transport=tcp", url->url_params);
    else
#endif
      url->url_params = "transport=tcp";


    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.2"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.3
   * Send a message from Bob to Alice
   * This time include a large payload of 512 kB, let NTA choose transport.
   */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 512 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.3"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.4:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, let NTA choose transport.
   */
  {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.4"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_out_via);
    TEST_1(strcasecmp(ag->ag_out_via->v_protocol, "SIP/2.0/TCP") == 0 ||
	   strcasecmp(ag->ag_out_via->v_protocol, "SIP/2.0/SCTP") == 0);
  }

  /* Test 0.5:
   * Send a message from Bob to Alice
   * This time include a payload of 2 kB, try to use UDP.
   */
  if (udp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 2 * 1024;

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    su_free(ag->ag_home, (void *)ag->ag_out_via), ag->ag_out_via = NULL;

    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.5"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TPTAG_MTU(0xffffffff),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
    TEST_1(ag->ag_out_via);
    TEST_S(ag->ag_out_via->v_protocol, "SIP/2.0/UDP");
  }

  if (udp) {
    /* Send a message from default leg to server leg 
     * using a prefilled Via header
     */
    sip_via_t via[1];

    sip_via_init(via);

    via->v_protocol = sip_transport_udp;
    
    via->v_host = ag->ag_contact->m_url->url_host;
    via->v_port = ag->ag_contact->m_url->url_port;
    
    sip_via_add_param(ag->ag_home, via, "branch=MagicalBranch");

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_USER_VIA(1),
			 TAG_END());

    ag->ag_expect_leg = ag->ag_server_leg;
    TEST_1(ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, 
			       magic_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.6"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       SIPTAG_VIA(via),
			       TAG_END()));
    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);

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
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.7"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_server_leg);
  }

  /* Test 0.8: Send a too large message */
  if (tcp) {
    url_t url[1];
    sip_payload_t *pl;
    usize_t size = 128 * 1024;

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_MAXSIZE(65536),
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "alice";

    TEST_1(pl = test_payload(ag->ag_home, size));

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.8"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       SIPTAG_PAYLOAD(pl),
			       TAG_END()));
    su_free(ag->ag_home, pl);

    nta_test_run(ag);
    TEST(ag->ag_status, 413);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_MAXSIZE(2 * 1024 * 1024),
			 TAG_END());
  }

  /* Test 0.9: Timeout */
  {
    url_t url[1];

    printf("%s: starting MESSAGE timeout test, test will complete in 4 seconds\n",
	   name);

    nta_agent_set_params(ag->ag_agent, 
			 NTATAG_TIMEOUT_408(1),
			 NTATAG_SIP_T1(25), 
			 NTATAG_SIP_T1X64(64 * 25), 
			 TAG_END());

    *url = *ag->ag_aliases->m_url;
    url->url_user = "timeout";
    url->url_port = ag->ag_sink_port;

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    TEST_1(ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 0.9"),
			       SIPTAG_FROM(ag->ag_bob),
			       SIPTAG_TO(ag->ag_alice),
			       SIPTAG_CONTACT(ag->ag_m_bob),
			       TAG_END()));

    nta_test_run(ag);
    TEST(ag->ag_status, 408);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, NULL);

    nta_agent_set_params(ag->ag_agent,
			 NTATAG_SIP_T1(500),
			 NTATAG_SIP_T1X64(64 * 500),
			 TAG_END());
  }
#endif  

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

    *url2 = *url;
    url2->url_port = "9";	/* discard service */

    ag->ag_expect_leg = ag->ag_default_leg;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       (url_string_t *)url,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)url2,
			       SIPTAG_SUBJECT_STR("Test 1.2"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
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
  nta_leg_bind(ag->ag_server_leg, new_leg_callback_200, ag);

  /* Send message from Alice to Bob establishing the dialog */
  ag->ag_expect_leg = ag->ag_server_leg;
  ag->ag_tag_remote = ag->ag_alice_leg;
  ag->ag_orq = 
        nta_outgoing_tcreate(ag->ag_alice_leg, outgoing_callback, ag,
			     ag->ag_obp,
			     SIP_METHOD_MESSAGE,
			     (url_string_t *)ag->ag_m_bob->m_url,
			     SIPTAG_SUBJECT_STR("Test 2.1"),
			     SIPTAG_FROM(ag->ag_alice),
			     SIPTAG_TO(ag->ag_bob),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     TAG_END());
  TEST_1(ag->ag_orq);

  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg != NULL);

  nta_leg_bind(ag->ag_server_leg, leg_callback_200, ag);

  /* Send message from Bob to Alice */
  ag->ag_expect_leg = ag->ag_alice_leg;
  ag->ag_orq = 
        nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ag,
      		       NULL,
      		       SIP_METHOD_MESSAGE,
      		       (url_string_t *)ag->ag_m_alice->m_url,
      		       SIPTAG_SUBJECT_STR("Test 2.2"),
      		       TAG_END());
  TEST_1(ag->ag_orq);

  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);

  /* Send again message from Alice to Bob */
  ag->ag_expect_leg = ag->ag_bob_leg;
  ag->ag_orq = 
        nta_outgoing_tcreate(ag->ag_alice_leg, outgoing_callback, ag,
      		       NULL,
      		       SIP_METHOD_MESSAGE,
      		       (url_string_t *)ag->ag_m_bob->m_url,
      		       SIPTAG_SUBJECT_STR("Test 2.3"),
      		       TAG_END());
  TEST_1(ag->ag_orq);

  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_bob_leg);

  /* Send message from Bob to Alice
   * This time, however, specify request URI 
   */
  {
    ag->ag_expect_leg = ag->ag_alice_leg;
    ag->ag_orq = 
          nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ag,
      			 NULL,
      			 SIP_METHOD_MESSAGE,
      			 (url_string_t *)ag->ag_m_alice->m_url,
      			 SIPTAG_SUBJECT_STR("Test 2.4"),
      			 TAG_END());
    TEST_1(ag->ag_orq);

    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST_P(ag->ag_orq, NULL);
    TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);
  }

  nta_leg_destroy(ag->ag_alice_leg), ag->ag_alice_leg = NULL;
  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;

  END();
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
  
  ag->ag_status = 200;

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

int outgoing_invite_callback(agent_t *ag,
			     nta_outgoing_t *orq,
			     sip_t const *sip)
{
  BEGIN();

  int status = sip->sip_status->st_status;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s %03d %s\n", name, __func__, 
	   sip->sip_status->st_version, 
	   sip->sip_status->st_status, 
	   sip->sip_status->st_phrase);
  }

  {
    msg_t *msg;

    TEST_1(msg = nta_outgoing_getresponse(orq));
    TEST_1(msg->m_refs == 2);
    TEST_1(sip_object(msg) == sip);
    if (ag->ag_probe_msg == NULL)
      ag->ag_probe_msg = msg;
    msg_destroy(msg);
  }

  if (status < 200) {
    if (sip->sip_require && sip_has_feature(sip->sip_require, "100rel")) {
      TEST_1(sip->sip_rseq);
      orq = nta_outgoing_prack(ag->ag_call_leg, orq, NULL, NULL,
			       NULL,
			       sip, 
			       TAG_END());
      TEST_1(orq);
      nta_outgoing_destroy(orq);
    }
    return 0;
  }

  if (status < 300) {
    nta_outgoing_t *ack;

    TEST_1(nta_leg_rtag(ag->ag_call_leg, sip->sip_to->a_tag));
    
    TEST(nta_leg_client_route(ag->ag_call_leg, 
			      sip->sip_record_route,
			      sip->sip_contact), 0);

    ack = nta_outgoing_tcreate(ag->ag_call_leg, NULL, NULL,
			       NULL,
			       SIP_METHOD_ACK,
			       NULL,
			       SIPTAG_CSEQ(sip->sip_cseq),
			       TAG_END());
    TEST_1(ack);
    nta_outgoing_destroy(ack);
  }
  else {
    ag->ag_status = status;
  }

  TEST_1(sip->sip_to && sip->sip_to->a_tag);

  nta_outgoing_destroy(orq);
  ag->ag_orq = NULL;
  ag->ag_call_leg = NULL;
  END();
}


int test_call(agent_t *ag)
{
  sip_content_type_t *c = ag->ag_content_type;
  sip_payload_t      *sdp = ag->ag_payload;
  nta_leg_t *old_leg;
  sip_replaces_t *r1, *r2;

  BEGIN();

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
  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			      outgoing_invite_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_INVITE,
			      (url_string_t *)ag->ag_m_bob->m_url,
			      SIPTAG_SUBJECT_STR("Call 1"),
			      SIPTAG_CONTACT(ag->ag_m_alice),
			      SIPTAG_CONTENT_TYPE(c),
			      SIPTAG_ACCEPT_CONTACT_STR("*;audio"),
			      SIPTAG_PAYLOAD(sdp),
			      NTATAG_USE_TIMESTAMP(1),
			      NTATAG_PASS_100(1),
			      TAG_END());
  TEST_1(ag->ag_orq);
  
  /* Try to CANCEL it immediately */
  TEST_1(nta_outgoing_cancel(ag->ag_orq) == 0);
  /* As Bob immediately answers INVITE with 200 Ok, 
     cancel should be answered with 487. */

  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg != NULL);

  TEST_1(r1 = nta_leg_make_replaces(ag->ag_alice_leg, ag->ag_home, 0));
  TEST_1(r2 = sip_replaces_format(ag->ag_home, "%s;from-tag=%s;to-tag=%s",
				  r1->rp_call_id, r1->rp_to_tag, r1->rp_from_tag));

  TEST_P(ag->ag_alice_leg, nta_leg_by_replaces(ag->ag_agent, r2));
  TEST_P(ag->ag_bob_leg, nta_leg_by_replaces(ag->ag_agent, r1));

  /* Re-INVITE from Bob to Alice.
   *
   * Alice first sends 183, waits for PRACK, then sends 184 and 185,
   * waits for PRACKs, then sends 200, waits for ACK.
   */
  ag->ag_expect_leg = ag->ag_alice_leg;
  ag->ag_orq = 
	nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_bob_leg, 
			     outgoing_invite_callback, ag,
			     NULL,
			     SIP_METHOD_INVITE,
			     NULL,
			     SIPTAG_SUBJECT_STR("Re-INVITE"),
			     SIPTAG_CONTACT(ag->ag_m_bob),
			     SIPTAG_SUPPORTED_STR("foo"),
			     SIPTAG_CONTENT_TYPE(c),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
  TEST_1(ag->ag_orq);
  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_alice_leg);

  nta_agent_set_params(ag->ag_agent, 
		       NTATAG_DEBUG_DROP_PROB(0),
		       TAG_END());

  /* Send BYE from Bob to Alice */
  old_leg = ag->ag_expect_leg = ag->ag_alice_leg;
  ag->ag_orq = 
	nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ag,
			     NULL,
			     SIP_METHOD_BYE,
			     NULL,
			     SIPTAG_SUBJECT_STR("Hangup"),
			     SIPTAG_FROM(ag->ag_alice),
			     SIPTAG_TO(ag->ag_bob),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     SIPTAG_CONTENT_TYPE(c),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, old_leg);
  TEST_P(ag->ag_alice_leg, NULL);

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;

  END();
}

/* ============================================================================ */
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
  } else {
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
  }

  END();
}


int invite_prack_callback(agent_t *ag,
			  nta_outgoing_t *orq,
			  sip_t const *sip)
{
  BEGIN();

  int status = sip->sip_status->st_status;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s %03d %s\n", name, __func__, 
	   sip->sip_status->st_version, 
	   sip->sip_status->st_status, 
	   sip->sip_status->st_phrase);
  }

  if (!ag->ag_call_tag && (status >= 200 || (status > 100 && sip->sip_rseq))) {
    nta_outgoing_t *tagged;
    TEST_1(sip->sip_to->a_tag);
    ag->ag_tag_status = status;
    ag->ag_call_tag = su_strdup(ag->ag_home, sip->sip_to->a_tag);
    TEST_S(ag->ag_call_tag, sip->sip_to->a_tag);
    TEST_S(nta_leg_rtag(ag->ag_call_leg, ag->ag_call_tag), ag->ag_call_tag);
    TEST(nta_leg_client_route(ag->ag_call_leg, 
			      sip->sip_record_route,
			      sip->sip_contact), 0);
    tagged = nta_outgoing_tagged(orq, 
				 invite_prack_callback,
				 ag,
				 ag->ag_call_tag,
				 sip->sip_rseq);
    TEST_1(tagged);
    nta_outgoing_destroy(orq);
    if (ag->ag_orq == orq)
      ag->ag_orq = tagged;
    orq = tagged;
  }

  if (status > ag->ag_status)
    ag->ag_status = status;

  if (status > 100 && status < 200 && sip->sip_rseq) {
    nta_outgoing_t *prack;
    prack = nta_outgoing_prack(ag->ag_call_leg, orq, NULL, NULL,
			       NULL,
			       sip, 
			       TAG_END());
    TEST_1(prack);
    nta_outgoing_destroy(prack);
    return 0;
  }

  if (status < 200)
    return 0;

  if (status < 300) {
    nta_outgoing_t *ack;
    msg_t *msg;
    sip_t *osip;

    TEST_1(msg = nta_outgoing_getrequest(orq));
    TEST_1(osip = sip_object(msg));

    TEST_1(nta_leg_rtag(ag->ag_call_leg, sip->sip_to->a_tag));
    
    TEST(nta_leg_client_route(ag->ag_call_leg, 
			      sip->sip_record_route,
			      sip->sip_contact), 0);

    ack = nta_outgoing_tcreate(ag->ag_call_leg, NULL, NULL,
			       NULL,
			       SIP_METHOD_ACK,
			       NULL,
			       SIPTAG_CSEQ(sip->sip_cseq),
			       NTATAG_ACK_BRANCH(osip->sip_via->v_branch),
			       TAG_END());
    TEST_1(ack);
    nta_outgoing_destroy(ack);
    msg_destroy(msg);
  }

  TEST_1(sip->sip_to && sip->sip_to->a_tag);

  nta_outgoing_destroy(orq);
  ag->ag_orq = NULL;
  ag->ag_call_leg = NULL;

  END();
}

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


int invite_183_cancel_callback(agent_t *ag,
			       nta_outgoing_t *orq,
			       sip_t const *sip)
{
  BEGIN();

  int status = sip->sip_status->st_status;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s %03d %s\n", name, __func__, 
	   sip->sip_status->st_version, 
	   sip->sip_status->st_status, 
	   sip->sip_status->st_phrase);
  }

  if (status > 100 && status < 200) {
    nta_outgoing_cancel(orq);
    return 0;
  }

  if (status < 200)
    return 0;

  if (status < 300) {
    nta_outgoing_t *ack;
    msg_t *msg;
    sip_t *osip;

    TEST_1(msg = nta_outgoing_getrequest(orq));
    TEST_1(osip = sip_object(msg));

    TEST_1(nta_leg_rtag(ag->ag_call_leg, sip->sip_to->a_tag));
    
    TEST(nta_leg_client_route(ag->ag_call_leg, 
			      sip->sip_record_route,
			      sip->sip_contact), 0);

    ack = nta_outgoing_tcreate(ag->ag_call_leg, NULL, NULL,
			       NULL,
			       SIP_METHOD_ACK,
			       NULL,
			       SIPTAG_CSEQ(sip->sip_cseq),
			       NTATAG_ACK_BRANCH(osip->sip_via->v_branch),
			       TAG_END());
    TEST_1(ack);
    nta_outgoing_destroy(ack);
    msg_destroy(msg);
  }
  else {
    ag->ag_status = status;
  }

  TEST_1(sip->sip_to && sip->sip_to->a_tag);

  nta_outgoing_destroy(orq);
  ag->ag_orq = NULL;
  ag->ag_call_leg = NULL;

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
  sip_content_type_t *c = ag->ag_content_type;
  sip_payload_t      *sdp = ag->ag_payload;
  nta_leg_t *old_leg;

  BEGIN();

  {
    /* Send a PRACK from default leg, NTA responds to it with error */
    url_t url[1];

    *url = *ag->ag_aliases->m_url;
    url->url_user = "bob";

    ag->ag_expect_leg = ag->ag_server_leg;
    ag->ag_latest_leg = NULL;
    ag->ag_orq = 
	  nta_outgoing_tcreate(ag->ag_default_leg, outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_PRACK,
			       (url_string_t *)url,
			       SIPTAG_SUBJECT_STR("Test 1.1"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       SIPTAG_RACK_STR("1432432 42332432 INVITE"),
			       TAG_END());
    TEST_1(ag->ag_orq);
    
    nta_test_run(ag);
    TEST(ag->ag_status, 481);
    TEST_P(ag->ag_orq, NULL);
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
  nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
  ag->ag_expect_leg = ag->ag_server_leg;
  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			      invite_prack_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_INVITE,
			      (url_string_t *)ag->ag_m_bob->m_url,
			      SIPTAG_SUBJECT_STR("Call 2"),
			      SIPTAG_CONTACT(ag->ag_m_alice),
			      SIPTAG_REQUIRE_STR("100rel"),
			      SIPTAG_CONTENT_TYPE(c),
			      SIPTAG_PAYLOAD(sdp),
			      TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run_until_acked(ag);
  TEST(ag->ag_status, 200);
  /*TEST(ag->ag_tag_status, 183);*/
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg != NULL);

  /* Send BYE from Bob to Alice */
  old_leg = ag->ag_expect_leg = ag->ag_alice_leg;
  ag->ag_orq = 
	nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ag,
			     NULL,
			     SIP_METHOD_BYE,
			     NULL,
			     SIPTAG_SUBJECT_STR("Hangup"),
			     SIPTAG_FROM(ag->ag_alice),
			     SIPTAG_TO(ag->ag_bob),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     SIPTAG_CONTENT_TYPE(c),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, old_leg);
  TEST_P(ag->ag_alice_leg, NULL);

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;
  if (ag->ag_call_tag)
    su_free(ag->ag_home, (void *)ag->ag_call_tag), ag->ag_call_tag = NULL;

  /* Test CANCELing a call after received PRACK */
  ag->ag_alice_leg = nta_leg_tcreate(ag->ag_agent, 
					   alice_leg_callback,
					   ag,
					   SIPTAG_FROM(ag->ag_alice),
					   SIPTAG_TO(ag->ag_bob),
					   TAG_END());
  TEST_1(ag->ag_alice_leg);
  
  TEST_1(nta_leg_tag(ag->ag_alice_leg, NULL));

  /* Send INVITE */
  nta_leg_bind(ag->ag_server_leg, bob_leg_callback3, ag);
  ag->ag_expect_leg = ag->ag_server_leg;
  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			      invite_183_cancel_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_INVITE,
			      (url_string_t *)ag->ag_m_bob->m_url,
			      SIPTAG_SUBJECT_STR("Call 2b"),
			      SIPTAG_CONTACT(ag->ag_m_alice),
			      SIPTAG_REQUIRE_STR("100rel"),
			      SIPTAG_CONTENT_TYPE(c),
			      SIPTAG_PAYLOAD(sdp),
			      TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST_1(ag->ag_status == 487 || ag->ag_status == 504);
  TEST_P(ag->ag_orq, NULL);
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

  /* Send INVITE, 
   * send precious provisional response
   * do not send PRACK, 
   * timeout (after 64 * t1 ~ 3.2 seconds),
   */
  nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
  ag->ag_expect_leg = ag->ag_server_leg;
  ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			      outgoing_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_INVITE,
			      (url_string_t *)ag->ag_m_bob->m_url,
			      SIPTAG_SUBJECT_STR("Call 3"),
			      SIPTAG_CONTACT(ag->ag_m_alice),
			      SIPTAG_REQUIRE_STR("100rel"),
			      SIPTAG_CONTENT_TYPE(c),
			      SIPTAG_PAYLOAD(sdp),
			      TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 503);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg == NULL);

  TEST(nta_agent_set_params(ag->ag_agent, 
			    NTATAG_SIP_T1(500), 
			    NTATAG_SIP_T1X64(64 * 500), 
			    TAG_END()), 2);
  }

  if (EXPENSIVE_CHECKS || 1) {
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

  /* Send INVITE, 
   * send precious provisional response
   * timeout after timer C
   */
  nta_leg_bind(ag->ag_server_leg, bob_leg_callback3, ag);
  ag->ag_expect_leg = ag->ag_server_leg;
  TEST_1(ag->ag_orq = 
	 nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			      invite_prack_callback, ag,
			      ag->ag_obp,
			      SIP_METHOD_INVITE,
			      (url_string_t *)ag->ag_m_bob->m_url,
			      SIPTAG_SUBJECT_STR("Call 4"),
			      SIPTAG_CONTACT(ag->ag_m_alice),
			      SIPTAG_REQUIRE_STR("100rel"),
			      SIPTAG_CONTENT_TYPE(c),
			      SIPTAG_PAYLOAD(sdp),
			      TAG_END()));
  nta_test_run_until_canceled(ag);
  TEST(ag->ag_status, 408);
  TEST_1(ag->ag_canceled != 0); 
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg);
  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;

  TEST(nta_agent_set_params(ag->ag_agent, 
			    NTATAG_TIMER_C(185 * 1000), 
			    TAG_END()), 1);

  nta_leg_destroy(ag->ag_bob_leg), ag->ag_bob_leg = NULL;
  ag->ag_latest_leg = NULL;
  ag->ag_call_leg = NULL;
  if (ag->ag_call_tag)
    su_free(ag->ag_home, (void *)ag->ag_call_tag), ag->ag_call_tag = NULL;
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
  sip_content_type_t *c = ag->ag_content_type;
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
  ag->ag_call_tag = NULL;

  /* Send INVITE */
  nta_leg_bind(ag->ag_server_leg, bob_leg_callback2, ag);
  ag->ag_expect_leg = ag->ag_server_leg;
  ag->ag_orq = 
	nta_outgoing_tcreate(ag->ag_call_leg = ag->ag_alice_leg, 
			     invite_prack_callback, ag,
			     ag->ag_obp,
			     SIP_METHOD_INVITE,
			     (url_string_t *)ag->ag_m_bob->m_url,
			     SIPTAG_SUBJECT_STR("Call 5"),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     SIPTAG_REQUIRE_STR("100rel"),
			     SIPTAG_CONTENT_TYPE(c),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  /*TEST(ag->ag_tag_status, 183);*/
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, ag->ag_server_leg);
  TEST_1(ag->ag_bob_leg != NULL);

  /* Send BYE from Bob to Alice */
  old_leg = ag->ag_expect_leg = ag->ag_alice_leg;
  ag->ag_orq = 
	nta_outgoing_tcreate(ag->ag_bob_leg, outgoing_callback, ag,
			     NULL,
			     SIP_METHOD_BYE,
			     NULL,
			     SIPTAG_SUBJECT_STR("Hangup"),
			     SIPTAG_FROM(ag->ag_alice),
			     SIPTAG_TO(ag->ag_bob),
			     SIPTAG_CONTACT(ag->ag_m_alice),
			     SIPTAG_CONTENT_TYPE(c),
			     SIPTAG_PAYLOAD(sdp),
			     TAG_END());
  TEST_1(ag->ag_orq);
  
  nta_test_run(ag);
  TEST(ag->ag_status, 200);
  TEST_P(ag->ag_orq, NULL);
  TEST_P(ag->ag_latest_leg, old_leg);
  TEST_P(ag->ag_alice_leg, NULL);

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

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
  su_log_soft_set_level(su_log_default, 9);
  su_log_soft_set_level(nta_log, 9);
  su_log_soft_set_level(tport_log, 9);
  setenv("SU_DEBUG", "9", 1);
  setenv("NUA_DEBUG", "9", 1);
  setenv("NTA_DEBUG", "9", 1);
  setenv("TPORT_DEBUG", "9", 1);
#endif
  su_init();

  if (!(TSTFLAGS & tst_verbatim)) {
    su_log_soft_set_level(nta_log, 0);
    su_log_soft_set_level(tport_log, 0);
  }

#if HAVE_OPEN_C
#define SINGLE_FAILURE_CHECK()						\
	  do { fflush(stdout);							\
	    if (retval && quit_on_single_failure) { su_deinit(); sleep(7); return retval; } \
	  } while(0)
#else
  #define SINGLE_FAILURE_CHECK()						\
	  do { fflush(stdout);							\
	    if (retval && quit_on_single_failure) { su_deinit(); return retval; } \
	  } while(0)
#endif
  
  retval |= test_init(ag, argv[i]); SINGLE_FAILURE_CHECK();
  if (retval == 0) {
    retval |= test_bad_messages(ag); SINGLE_FAILURE_CHECK();
    retval |= test_reinit(ag); SINGLE_FAILURE_CHECK();
    retval |= test_tports(ag); SINGLE_FAILURE_CHECK();
    retval |= test_destroy_incoming(ag); SINGLE_FAILURE_CHECK();
    retval |= test_resolv(ag, argv[i]); SINGLE_FAILURE_CHECK();
    retval |= test_routing(ag); SINGLE_FAILURE_CHECK();
    retval |= test_dialog(ag); SINGLE_FAILURE_CHECK();
    retval |= test_call(ag); SINGLE_FAILURE_CHECK();
    retval |= test_prack(ag); SINGLE_FAILURE_CHECK();
    retval |= test_fix_467(ag); SINGLE_FAILURE_CHECK();
  }
  retval |= test_deinit(ag); fflush(stdout);

  su_home_deinit(ag->ag_home);

  su_deinit();

#if HAVE_OPEN_C
  sleep(7);
#endif  
  
  return retval;
}
