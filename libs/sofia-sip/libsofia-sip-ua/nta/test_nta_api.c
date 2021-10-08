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
 * @CFILE test_nta_api.c
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
#include <sofia-sip/msg_mclass.h>
#include <sofia-sip/sofia_features.h>
#include <sofia-sip/hostdomain.h>
#include <sofia-sip/su_string.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

extern su_log_t nta_log[];
extern su_log_t tport_log[];

int tstflags = 0;
#define TSTFLAGS tstflags
char const name[] = "test_nta_api";

#include <sofia-sip/tstdef.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

#define NONE ((void *)-1)

struct sigcomp_compartment;

struct agent_t {
  su_home_t       ag_home[1];
  int             ag_flags;
  su_root_t      *ag_root;
  msg_mclass_t   *ag_mclass;
  nta_agent_t    *ag_agent;

  nta_leg_t      *ag_default_leg; /**< Leg for rest */
  nta_leg_t      *ag_server_leg;  /**< Leg for <sip:%@%>;methods=<PUBLISH>;events=<presence> */

  unsigned        ag_drop;

  nta_outgoing_t *ag_orq;
  int             ag_status;
  msg_t          *ag_response;

  /* Server side */
  nta_incoming_t *ag_irq;

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
};


static int incoming_callback_1(agent_t *ag,
			       nta_incoming_t *irq,
			       sip_t const *sip)
{
  return 0;
}

static int incoming_callback_2(agent_t *ag,
			       nta_incoming_t *irq,
			       sip_t const *sip)
{
  return 0;
}

int agent_callback(agent_t *ag,
		   nta_agent_t *nta,
		   msg_t *msg,
		   sip_t *sip)
{
  msg_destroy(msg);
  return 0;
}

int leg_callback(agent_t *ag,
		 nta_leg_t *leg,
		 nta_incoming_t *irq,
		 sip_t const *sip)
{
  BEGIN();
  msg_t *msg;
  char const *tag;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s " URL_PRINT_FORMAT " %s\n",
	   name, __func__, sip->sip_request->rq_method_name,
	   URL_PRINT_ARGS(sip->sip_request->rq_url),
	   sip->sip_request->rq_version);
  }

  TEST_1(sip->sip_content_length);
  TEST_1(sip->sip_via);
  TEST_1(sip->sip_from && sip->sip_from->a_tag);

  TEST_VOID(nta_incoming_bind(irq, incoming_callback_1, ag));
  TEST_P(nta_incoming_magic(irq, incoming_callback_1), ag);
  TEST_P(nta_incoming_magic(irq, incoming_callback_2), 0);

  TEST_1(tag = nta_incoming_tag(irq, "tag=foofaa"));
  TEST_S(nta_incoming_gettag(irq), tag);
  TEST_S(tag, "foofaa");
  TEST_1(tag = nta_incoming_tag(irq, "foofaa"));

  TEST(nta_incoming_status(irq), 0);
  TEST(nta_incoming_method(irq), sip_method_message);
  TEST_S(nta_incoming_method_name(irq), "MESSAGE");
  TEST_1(nta_incoming_url(irq) != NULL);
  TEST_1(nta_incoming_cseq(irq) != 0);

  TEST(nta_incoming_set_params(irq, TAG_END()), 0);

  TEST_1(msg = nta_incoming_getrequest(irq)); msg_destroy(msg);
  TEST_P(nta_incoming_getrequest_ackcancel(irq), NULL);
  TEST_P(nta_incoming_getresponse(irq), NULL);

  TEST(nta_incoming_treply(irq, SIP_100_TRYING, TAG_END()), 0);
  TEST_1(msg = nta_incoming_getresponse(irq)); msg_destroy(msg);
  msg = nta_msg_create(ag->ag_agent, 0);
  TEST(nta_incoming_complete_response(irq, msg, SIP_200_OK, TAG_END()), 0);
  TEST(nta_incoming_mreply(irq, msg), 0);

  END();
}

int outgoing_callback(agent_t *ag,
		      nta_outgoing_t *orq,
		      sip_t const *sip)
{
  BEGIN();
  msg_t *msg;

  int status = sip->sip_status->st_status;

  if (tstflags & tst_verbatim) {
    printf("%s: %s: %s %03d %s\n", name, __func__,
	   sip->sip_status->st_version,
	   sip->sip_status->st_status,
	   sip->sip_status->st_phrase);
  }

  ag->ag_status = status;

  if (status < 200)
    return 0;

  TEST_1(sip->sip_to && sip->sip_to->a_tag);

  /* Test API functions */
  TEST(nta_outgoing_status(orq), status);
  TEST_1(nta_outgoing_request_uri(orq));
  TEST_1(!nta_outgoing_route_uri(orq));
  TEST(nta_outgoing_method(orq), sip_method_message);
  TEST_S(nta_outgoing_method_name(orq), "MESSAGE");
  TEST(nta_outgoing_cseq(orq), sip->sip_cseq->cs_seq);
  TEST_1(nta_outgoing_delay(orq) < UINT_MAX);

  TEST_1(msg = nta_outgoing_getresponse(orq));
  msg_destroy(msg);

  TEST_1(msg = nta_outgoing_getrequest(orq));
  msg_destroy(msg);

  nta_outgoing_destroy(orq);
  /* Call it twice */
  nta_outgoing_destroy(orq);

  ag->ag_orq = NULL;

  END();
}

void
nta_test_run(agent_t *ag)
{
  time_t now = time(NULL);

  for (ag->ag_status = 0; ag->ag_status < 200;) {
    if (tstflags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(ag->ag_root, 500L);

    if (!getenv("NTA_TEST_DEBUG") && time(NULL) > now + 5) {
      fprintf(stderr, "nta_test_run: timeout\n");
      return;
    }
  }
}

int api_test_init(agent_t *ag)
{
  BEGIN();

  char const *contact = NULL;

  if (getenv("SIPCONTACT"))
    contact = getenv("SIPCONTACT");

  if (contact == NULL || contact[0] == '\0')
    contact = "sip:0.0.0.0:*;comp=sigcomp";

  TEST_1(ag->ag_root = su_root_create(ag));
  TEST_1(ag->ag_mclass = msg_mclass_clone(sip_default_mclass(), 0, 0));

  /* Create agent */
  TEST_1(ag->ag_agent = nta_agent_create(ag->ag_root,
					 (url_string_t *)contact,
					 NULL,
					 NULL,
					 NTATAG_MCLASS(ag->ag_mclass),
					 NTATAG_USE_TIMESTAMP(1),
					 NTATAG_USE_NAPTR(0),
					 NTATAG_USE_SRV(0),
					 NTATAG_PRELOAD(2048),
					 TAG_END()));
  /* Create a default leg */
  TEST_1(ag->ag_default_leg = nta_leg_tcreate(ag->ag_agent,
					     leg_callback,
					     ag,
					     NTATAG_NO_DIALOG(1),
					     TAG_END()));

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

    url_strip_transport(ag->ag_bob->a_url);

    *m->m_url = *ag->ag_contact->m_url;
    m->m_url->url_user = "alice";
    TEST_1(ag->ag_m_alice = sip_contact_dup(ag->ag_home, m));

    from->a_display = "Alice";
    *from->a_url = *ag->ag_contact->m_url;
    from->a_url->url_user = "alice";
    from->a_url->url_port = NULL;

    TEST_1(ag->ag_alice = sip_from_dup(ag->ag_home, from));

    url_strip_transport(ag->ag_alice->a_url);
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

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_ALIASES(ag->ag_aliases),
			      NTATAG_REL100(1),
			      NTATAG_UA(1),
			      NTATAG_USE_NAPTR(1),
			      NTATAG_USE_SRV(1),
			      TAG_END()),
	 5);

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_ALIASES(ag->ag_aliases),
			      NTATAG_DEFAULT_PROXY("sip:127.0.0.1"),
			      TAG_END()), 2);

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_ALIASES(ag->ag_aliases),
			      NTATAG_DEFAULT_PROXY(NULL),
			      TAG_END()), 2);

    TEST(nta_agent_set_params(ag->ag_agent,
			      NTATAG_DEFAULT_PROXY("tel:+35878008000"),
			      TAG_END()), -1);

  }

  {
    url_t url[1];

    /* Create the server leg */
    *url = *ag->ag_aliases->m_url;
    url->url_user = "%";
    TEST_1(ag->ag_server_leg = nta_leg_tcreate(ag->ag_agent,
					       leg_callback,
					       ag,
					       NTATAG_NO_DIALOG(1),
					       URLTAG_URL(url),
					       TAG_END()));
  }

  END();
}

int api_test_deinit(agent_t *ag)
{
  BEGIN();

  if (ag->ag_request) msg_destroy(ag->ag_request), ag->ag_request = NULL;
  if (ag->ag_response) msg_destroy(ag->ag_response), ag->ag_response = NULL;

  su_free(ag->ag_home, ag->ag_in_via), ag->ag_in_via = NULL;

  nta_leg_destroy(ag->ag_alice_leg);
  nta_leg_destroy(ag->ag_bob_leg);
  nta_leg_destroy(ag->ag_default_leg);
  nta_leg_destroy(ag->ag_server_leg);

  nta_agent_destroy(ag->ag_agent);
  su_root_destroy(ag->ag_root);

  free(ag->ag_mclass), ag->ag_mclass = NULL;

  END();
}

static int api_test_destroy(agent_t *ag)
{
  nta_agent_t *nta;
  su_root_t *root;
  su_home_t home[1];
  nta_outgoing_t *orq;
  nta_leg_t *leg;
  int i;

  BEGIN();

  memset(home, 0, sizeof home);
  home->suh_size = sizeof home;
  su_home_init(home);

  TEST_1(root = su_root_create(NULL));

  for (i = 0; i < 2; i++) {
    TEST_1(nta = nta_agent_create(root,
				  (url_string_t *)"sip:*:*",
				  NULL,
				  NULL,
				  TAG_END()));
    TEST_1(leg = nta_leg_tcreate(nta, NULL, NULL,
				 NTATAG_NO_DIALOG(1),
				 TAG_END()));
    /* This creates a delayed response message */
    orq = nta_outgoing_tcreate(leg, outgoing_callback, ag, NULL,
			       SIP_METHOD_MESSAGE,
			       URL_STRING_MAKE("sip:foo.bar;transport=none"),
			       SIPTAG_FROM_STR("<sip:bar.foo>"),
			       SIPTAG_TO_STR("<sip:foo.bar>"),
			       TAG_END());
    TEST_1(orq);

    TEST_VOID(nta_outgoing_destroy(orq));
    TEST_VOID(nta_leg_destroy(leg));
    TEST_VOID(nta_agent_destroy(nta));
  }

  TEST_VOID(su_root_destroy(root));
  TEST_VOID(su_home_deinit(home));

  END();
}


/* Get and check parameters */
int api_test_params(agent_t *ag)
{
  BEGIN();
  nta_agent_t *nta;

  sip_contact_t const *aliases = (void *)-1;
  msg_mclass_t *mclass = (void *)-1;
  sip_contact_t const *contact = (void *)-1;
  url_string_t const *default_proxy = (void *)-1;
  void *smime = (void *)-1;

  unsigned blacklist = -1;
  unsigned debug_drop_prob = -1;
  unsigned max_forwards = -1;
  usize_t maxsize = -1;
  unsigned preload = -1;
  unsigned progress = -1;
  unsigned sip_t1 = -1;
  unsigned sip_t2 = -1;
  unsigned sip_t4 = -1;
  unsigned timer_c = -1;
  unsigned udp_mtu = -1;

  int cancel_2543     = -1;
  int cancel_487      = -1;
  int client_rport    = -1;
  int extra_100       = -1;
  int merge_482       = -1;
  int pass_100        = -1;
  int pass_408        = -1;
  int rel100          = -1;
  int server_rport    = -1;
  int stateless       = -1;
  int tag_3261        = -1;
  int timeout_408     = -1;
  int ua              = -1;
  int use_naptr       = -1;
  int use_srv         = -1;
  int use_timestamp   = -1;
  int user_via        = -1;

  char const *s = NONE;

  TEST_1(nta = nta_agent_create(ag->ag_root, (url_string_t *)"sip:*:*",
				NULL, NULL, TAG_END()));
  TEST(nta_agent_get_params(nta,
			    NTATAG_ALIASES_REF(aliases),
			    NTATAG_BLACKLIST_REF(blacklist),
			    NTATAG_CANCEL_2543_REF(cancel_2543),
			    NTATAG_CANCEL_487_REF(cancel_487),
			    NTATAG_CLIENT_RPORT_REF(client_rport),
			    NTATAG_CONTACT_REF(contact),
			    NTATAG_DEBUG_DROP_PROB_REF(debug_drop_prob),
			    NTATAG_DEFAULT_PROXY_REF(default_proxy),
			    NTATAG_EXTRA_100_REF(extra_100),
			    NTATAG_MAXSIZE_REF(maxsize),
			    NTATAG_MAX_FORWARDS_REF(max_forwards),
			    NTATAG_MCLASS_REF(mclass),
			    NTATAG_MERGE_482_REF(merge_482),
			    NTATAG_PASS_100_REF(pass_100),
			    NTATAG_PASS_408_REF(pass_408),
			    NTATAG_PRELOAD_REF(preload),
			    NTATAG_PROGRESS_REF(progress),
			    NTATAG_REL100_REF(rel100),
			    NTATAG_SERVER_RPORT_REF(server_rport),
			    NTATAG_SIP_T1_REF(sip_t1),
			    NTATAG_SIP_T2_REF(sip_t2),
			    NTATAG_SIP_T4_REF(sip_t4),
			    NTATAG_SMIME_REF(smime),
			    NTATAG_STATELESS_REF(stateless),
			    NTATAG_TAG_3261_REF(tag_3261),
			    NTATAG_TIMEOUT_408_REF(timeout_408),
			    NTATAG_TIMER_C_REF(timer_c),
			    NTATAG_UA_REF(ua),
			    NTATAG_UDP_MTU_REF(udp_mtu),
			    NTATAG_USER_VIA_REF(user_via),
			    NTATAG_USE_NAPTR_REF(use_naptr),
			    NTATAG_USE_SRV_REF(use_srv),
			    NTATAG_USE_TIMESTAMP_REF(use_timestamp),
			    TAG_END()),
       /* Number of parameters */ 33);

  TEST_P(mclass, sip_default_mclass());
  TEST_P(aliases, NULL);
  TEST_1(contact != (void *)-1 && contact != NULL);
  TEST_1(default_proxy == NULL);
  TEST_1(smime == NULL);

  TEST_1(blacklist != (unsigned)-1);
  TEST(debug_drop_prob, 0);
  TEST_1(max_forwards >= 20);
  TEST_1(maxsize >= 65536);
  TEST_1(preload != (unsigned)-1);
  TEST_1(progress <= 60 * 1000);
  TEST(sip_t1, NTA_SIP_T1);
  TEST(sip_t2, NTA_SIP_T2);
  TEST(sip_t4, NTA_SIP_T4);
  TEST_1(timer_c > 180 * 1000);
  TEST(udp_mtu, 1300);

  TEST_1(cancel_2543 != -1);
  TEST_1(cancel_487 != -1);
  TEST_1(client_rport != -1);
  TEST_1(extra_100 != -1);
  TEST_1(merge_482 != -1);
  TEST_1(pass_100 != -1);
  TEST_1(pass_408 != -1);
  TEST_1(rel100 != -1);
  TEST_1(server_rport != -1);
  TEST_1(stateless == 0);
  TEST_1(timeout_408 != -1);
  TEST_1(ua == 0);
  TEST_1(use_naptr != -1);
  TEST_1(use_srv != -1);
  TEST_1(use_timestamp != -1);
  TEST_1(user_via == 0);

  TEST(nta_agent_set_params(NULL,
			    NTATAG_PRELOAD(2048),
			    TAG_END()), -1);
  TEST(nta_agent_get_params(NULL,
			    NTATAG_PRELOAD_REF(preload),
			    TAG_END()), -1);

  TEST(nta_agent_set_params(nta,
			    NTATAG_PRELOAD(2048),
			    TAG_END()), 1);
  TEST(nta_agent_get_params(nta,
			    NTATAG_PRELOAD_REF(preload),
			    TAG_END()), 1);
  TEST(preload, 2048);

  TEST(nta_agent_set_params(nta,
			    NTATAG_SIGCOMP_OPTIONS("sip"),
			    TAG_END()), 1);
  TEST(nta_agent_set_params(nta,
			    NTATAG_SIGCOMP_OPTIONS(","),
			    TAG_END()), -1);
  TEST(nta_agent_set_params(nta,
			    NTATAG_SIGCOMP_OPTIONS("sip;dms=16384"),
			    TAG_END()), 1);
  s = NONE;
  TEST(nta_agent_get_params(nta,
			    NTATAG_SIGCOMP_OPTIONS_REF(s),
			    TAG_END()), 1);
  TEST_S(s, "sip;dms=16384");

  TEST_VOID(nta_agent_destroy(nta));

  END();
}

int api_test_stats(agent_t *ag)
{
  BEGIN();

  nta_agent_t *nta;

  usize_t irq_hash = -1, orq_hash = -1, leg_hash = -1;
  usize_t recv_msg = -1, sent_msg = -1;
  usize_t recv_request = -1, recv_response = -1;
  usize_t bad_message = -1, bad_request = -1, bad_response = -1;
  usize_t drop_request = -1, drop_response = -1;
  usize_t client_tr = -1, server_tr = -1, dialog_tr = -1;
  usize_t acked_tr = -1, canceled_tr = -1;
  usize_t trless_request = -1, trless_to_tr = -1, trless_response = -1;
  usize_t trless_200 = -1, merged_request = -1;
  usize_t sent_request = -1, sent_response = -1;
  usize_t retry_request = -1, retry_response = -1, recv_retry = -1;
  usize_t tout_request = -1, tout_response = -1;

  TEST_1(nta = nta_agent_create(ag->ag_root, (url_string_t *)"sip:*:*",
				NULL, NULL, TAG_END()));

  TEST(nta_agent_get_stats(NULL,
      		     NTATAG_S_TOUT_REQUEST_REF(tout_request),
      		     NTATAG_S_TOUT_RESPONSE_REF(tout_response),
      		     TAG_END()), -1);

  TEST(nta_agent_get_stats(nta,
      		     NTATAG_S_IRQ_HASH_REF(irq_hash),
      		     NTATAG_S_ORQ_HASH_REF(orq_hash),
      		     NTATAG_S_LEG_HASH_REF(leg_hash),
      		     NTATAG_S_RECV_MSG_REF(recv_msg),
      		     NTATAG_S_SENT_MSG_REF(sent_msg),
      		     NTATAG_S_RECV_REQUEST_REF(recv_request),
      		     NTATAG_S_RECV_RESPONSE_REF(recv_response),
      		     NTATAG_S_BAD_MESSAGE_REF(bad_message),
      		     NTATAG_S_BAD_REQUEST_REF(bad_request),
      		     NTATAG_S_BAD_RESPONSE_REF(bad_response),
      		     NTATAG_S_DROP_REQUEST_REF(drop_request),
      		     NTATAG_S_DROP_RESPONSE_REF(drop_response),
      		     NTATAG_S_CLIENT_TR_REF(client_tr),
      		     NTATAG_S_SERVER_TR_REF(server_tr),
      		     NTATAG_S_DIALOG_TR_REF(dialog_tr),
      		     NTATAG_S_ACKED_TR_REF(acked_tr),
      		     NTATAG_S_CANCELED_TR_REF(canceled_tr),
      		     NTATAG_S_TRLESS_REQUEST_REF(trless_request),
      		     NTATAG_S_TRLESS_TO_TR_REF(trless_to_tr),
      		     NTATAG_S_TRLESS_RESPONSE_REF(trless_response),
      		     NTATAG_S_TRLESS_200_REF(trless_200),
      		     NTATAG_S_MERGED_REQUEST_REF(merged_request),
      		     NTATAG_S_SENT_REQUEST_REF(sent_request),
      		     NTATAG_S_SENT_RESPONSE_REF(sent_response),
      		     NTATAG_S_RETRY_REQUEST_REF(retry_request),
      		     NTATAG_S_RETRY_RESPONSE_REF(retry_response),
      		     NTATAG_S_RECV_RETRY_REF(recv_retry),
      		     NTATAG_S_TOUT_REQUEST_REF(tout_request),
      		     NTATAG_S_TOUT_RESPONSE_REF(tout_response),
      		     TAG_END()), 29);

  TEST_1(irq_hash == HTABLE_MIN_SIZE);
  TEST_1(orq_hash == HTABLE_MIN_SIZE);
  TEST_1(leg_hash == HTABLE_MIN_SIZE);
  TEST_1(recv_msg == 0);
  TEST_1(sent_msg == 0);
  TEST_1(recv_request == 0);
  TEST_1(recv_response == 0);
  TEST_1(bad_message == 0);
  TEST_1(bad_request == 0);
  TEST_1(bad_response == 0);
  TEST_1(drop_request == 0);
  TEST_1(drop_response == 0);
  TEST_1(client_tr == 0);
  TEST_1(server_tr == 0);
  TEST_1(dialog_tr == 0);
  TEST_1(acked_tr == 0);
  TEST_1(canceled_tr == 0);
  TEST_1(trless_request == 0);
  TEST_1(trless_to_tr == 0);
  TEST_1(trless_response == 0);
  TEST_1(trless_200 == 0);
  TEST_1(merged_request == 0);
  TEST_1(sent_request == 0);
  TEST_1(sent_response == 0);
  TEST_1(retry_request == 0);
  TEST_1(retry_response == 0);
  TEST_1(recv_retry == 0);
  TEST_1(tout_request == 0);
  TEST_1(tout_response == 0);

  TEST_VOID(nta_agent_destroy(nta));

  END();
}

/* Test handling transports */
int api_test_tport(agent_t *ag)
{
  sip_via_t const *v;

  url_t url[1];

  BEGIN();

  nta_agent_t *agent;
  sip_contact_t const *m;

  *url = *ag->ag_contact->m_url;
  url->url_port = "*";
  url->url_params = "transport=tcp";

  TEST_1(agent = nta_agent_create(ag->ag_root, NONE, NULL, NULL, TAG_END()));
  TEST_1(!nta_agent_via(agent));
  TEST_1(!nta_agent_public_via(agent));
  TEST_1(!nta_agent_contact(agent));

  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url, TAG_END()) == 0);
  TEST_1(v = nta_agent_via(agent)); TEST_1(!v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_tcp), 0);
  TEST_1(m = nta_agent_contact(agent));
  TEST_S(m->m_url->url_params, "transport=tcp");

  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url,
			     TPTAG_SERVER(0), TAG_END()) == 0);
  TEST_1(v = nta_agent_public_via(agent)); TEST_1(!v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_tcp), 0);
  TEST_1(host_has_domain_invalid(v->v_host));
  TEST_1(m = nta_agent_contact(agent));
  TEST_S(m->m_url->url_params, "transport=tcp");

  url->url_params = "transport=udp";
  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url, TAG_END()) == 0);
  TEST_1(v = nta_agent_via(agent)); TEST_1(v = v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_udp), 0);

  TEST_VOID(nta_agent_destroy(agent));

  TEST_1(agent = nta_agent_create(ag->ag_root, NONE, NULL, NULL, TAG_END()));
  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url, TAG_END()) == 0);
  TEST_1(v = nta_agent_via(agent)); TEST_1(!v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_udp), 0);
  TEST_1(m = nta_agent_contact(agent));
  TEST_S(m->m_url->url_params, "transport=udp");
  TEST_VOID(nta_agent_destroy(agent));

  url->url_params = "transport=tcp,udp";

  TEST_1(agent = nta_agent_create(ag->ag_root, NONE, NULL, NULL, TAG_END()));
  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url, TAG_END()) == 0);
  TEST_1(v = nta_agent_via(agent));
  TEST(!su_casematch(v->v_protocol, sip_transport_tcp), 0);
  TEST_1(v = v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_udp), 0);
  TEST_1(m = nta_agent_contact(agent));
  TEST_1(!m->m_url->url_params);
  TEST_VOID(nta_agent_destroy(agent));

  url->url_params = NULL;

  TEST_1(agent = nta_agent_create(ag->ag_root, NONE, NULL, NULL, TAG_END()));
  TEST_1(nta_agent_add_tport(agent, (url_string_t *)url, TAG_END()) == 0);
  TEST_1(v = nta_agent_via(agent));
  TEST(!su_casematch(v->v_protocol, sip_transport_udp), 0);
  TEST_1(v = v->v_next);
  TEST(!su_casematch(v->v_protocol, sip_transport_tcp), 0);
  TEST_1(m = nta_agent_contact(agent));
  TEST_1(!m->m_url->url_params);
  TEST_VOID(nta_agent_destroy(agent));


  END();
}

static int api_test_dialogs(agent_t *ag)
{
  BEGIN();
#if 0
  {
    /* Test 0.1
     * Send a message from default leg to default leg
     */
    char const p_acid[] = "P-Access-Network-Info: IEEE-802.11g\n";
    msg_t *msg;

    ag->ag_expect_leg = ag->ag_default_leg;

    TEST_1(ag->ag_orq =
	  nta_outgoing_tcreate(ag->ag_default_leg,
			       outgoing_callback, ag,
			       ag->ag_obp,
			       SIP_METHOD_MESSAGE,
			       (url_string_t *)ag->ag_contact->m_url,
			       SIPTAG_SUBJECT_STR("Test 0.1"),
			       SIPTAG_FROM(ag->ag_alice),
			       SIPTAG_TO(ag->ag_bob),
			       SIPTAG_CONTACT(ag->ag_m_alice),
			       SIPTAG_HEADER_STR(p_acid),
			       TAG_END()));

    TEST(nta_outgoing_getresponse(ag->ag_orq), NULL);
    TEST_1(msg = nta_outgoing_getrequest(ag->ag_orq));
    TEST(nta_outgoing_method(ag->ag_orq), sip_method_message);
    TEST_S(nta_outgoing_method_name(ag->ag_orq), "MESSAGE");
    msg_destroy(msg);

    TEST(nta_outgoing_delay(ag->ag_orq), UINT_MAX);
    nta_test_run(ag);
    TEST(ag->ag_status, 200);
    TEST(ag->ag_orq, NULL);
    TEST(ag->ag_latest_leg, ag->ag_default_leg);
    TEST_1(ag->ag_request);

    nta_leg_bind(ag->ag_default_leg, leg_callback_200, ag);
  }
#endif

  END();
}


/* Test that NULL host and/or port fields of user supplied Via header are
   filled in automatically */
int api_test_user_via_fillin(agent_t *ag)
{
  su_home_t home[1];
  su_root_t *root;
  nta_agent_t *nta;
  nta_leg_t *leg;
  nta_outgoing_t *orq0, *orq1;
  msg_t *msg0, *msg1;
  sip_t *sip0, *sip1;
  sip_via_t *via0, *via1;
  sip_via_t via[1];
  static char *via_params[] = { "param1=value1", "param2=value2" };
  size_t i;

  BEGIN();

  memset(home, 0, sizeof home);
  su_home_init(home);

  TEST_1(root = su_root_create(NULL));

  TEST_1(nta = nta_agent_create(root,
  			  (url_string_t *)"sip:*:*",
  			  NULL,
  			  NULL,
  			  TAG_END()));
  TEST_1(leg = nta_leg_tcreate(nta, NULL, NULL,
  			 NTATAG_NO_DIALOG(1),
  			 TAG_END()));

  /* This creates a delayed response message */
  orq0 = nta_outgoing_tcreate(leg, outgoing_callback, ag, NULL,
  		       SIP_METHOD_MESSAGE,
  		       URL_STRING_MAKE("sip:foo.bar;transport=none"),
  		       SIPTAG_FROM_STR("<sip:bar.foo>"),
  		       SIPTAG_TO_STR("<sip:foo.bar>"),
  		       TAG_END());
  TEST_1(orq0);
  TEST_1(msg0 = nta_outgoing_getrequest(orq0));
  TEST_1(sip0 = sip_object(msg0));
  TEST_1(via0 = sip0->sip_via);

  /* create user Via template to be filled in by NTA */
  sip_via_init(via);
  via->v_protocol = "*";
  for (i = 0; i < sizeof(via_params) / sizeof(via_params[0]); i++)
    sip_via_add_param(home, via, via_params[i]); /* add param to the template */

  /* This creates a delayed response message */
  orq1 = nta_outgoing_tcreate(leg, outgoing_callback, ag, NULL,
			      SIP_METHOD_MESSAGE,
			      URL_STRING_MAKE("sip:foo.bar;transport=none"),
			      SIPTAG_FROM_STR("<sip:bar.foo>"),
			      SIPTAG_TO_STR("<sip:foo.bar>"),
			      NTATAG_USER_VIA(1),
			      SIPTAG_VIA(via),
			      TAG_END());
  TEST_1(orq1);
  TEST_1(msg1 = nta_outgoing_getrequest(orq1));
  TEST_1(sip1 = sip_object(msg1));
  TEST_1(via1 = sip1->sip_via);

  /* check that template has been filled correctly */
  TEST_S(via0->v_protocol, via1->v_protocol);
  TEST_S(via0->v_host, via1->v_host);
  TEST_S(via0->v_port, via1->v_port);
  /* check that the parameter has been preserved */
  for (i = 0; i < sizeof(via_params)/sizeof(via_params[0]); i++)
    TEST_S(via1->v_params[i], via_params[i]);

  TEST_VOID(nta_outgoing_destroy(orq0));
  TEST_VOID(nta_outgoing_destroy(orq1));
  TEST_VOID(nta_leg_destroy(leg));
  TEST_VOID(nta_agent_destroy(nta));

  TEST_VOID(su_root_destroy(root));
  TEST_VOID(su_home_deinit(home));

  END();
}


int outgoing_default(agent_t *ag,
		     nta_outgoing_t *orq,
		     sip_t const *sip)
{
  BEGIN();
  msg_t *msg;

  int status = sip->sip_status->st_status;

  ag->ag_status = status;

  if (status < 200)
    return 0;

  /* Test API functions */
  TEST(nta_outgoing_status(orq), status);
  TEST_1(!nta_outgoing_request_uri(orq));
  TEST_1(!nta_outgoing_route_uri(orq));
  TEST(nta_outgoing_method(orq), sip_method_invalid);
  TEST_S(nta_outgoing_method_name(orq), "*");
  TEST(nta_outgoing_cseq(orq), 0);
  TEST_1(nta_outgoing_delay(orq) == UINT_MAX);

  TEST_1(msg = nta_outgoing_getresponse(orq));
  if (ag->ag_response == NULL)
    ag->ag_response = msg;
  else
    msg_destroy(msg);

  TEST_1(!nta_outgoing_getrequest(orq));

  END();
}

/* Test default incoming and outgoing */
static int api_test_default(agent_t *ag)
{
  BEGIN();
  nta_agent_t *nta;
  nta_incoming_t *irq;
  nta_outgoing_t *orq;
  sip_via_t via[1];
  su_nanotime_t nano;

  TEST_1(nta = ag->ag_agent);

  TEST_1(irq = nta_incoming_default(nta));

  TEST_VOID(nta_incoming_bind(irq, incoming_callback_1, ag));
  TEST_P(nta_incoming_magic(irq, incoming_callback_1), ag);
  TEST_P(nta_incoming_magic(irq, incoming_callback_2), 0);

  TEST_P(nta_incoming_tag(irq, NULL), NULL);
  TEST_P(nta_incoming_gettag(irq), NULL);

  TEST(nta_incoming_status(irq), 0);
  TEST(nta_incoming_method(irq), sip_method_invalid);
  TEST_S(nta_incoming_method_name(irq), "*");
  TEST_P(nta_incoming_url(irq), NULL);
  TEST(nta_incoming_cseq(irq), 0);

  TEST(nta_incoming_received(irq, &nano), nano / 1000000000);

  TEST(nta_incoming_set_params(irq, TAG_END()), 0);

  TEST_P(nta_incoming_getrequest(irq), NULL);
  TEST_P(nta_incoming_getrequest_ackcancel(irq), NULL);
  TEST_P(nta_incoming_getresponse(irq), NULL);

  TEST(nta_incoming_complete_response(irq, NULL, SIP_200_OK, TAG_END()), -1);

  TEST(nta_incoming_treply(irq, SIP_200_OK, TAG_END()), -1);
  TEST(nta_incoming_mreply(irq, NULL), -1);

  TEST_VOID(nta_incoming_destroy(irq));

  TEST_1(orq = nta_outgoing_default(nta, outgoing_default, ag));

  TEST(nta_outgoing_status(orq), 0);
  TEST(nta_outgoing_method(orq), sip_method_invalid);
  TEST_S(nta_outgoing_method_name(orq), "*");
  TEST(nta_outgoing_cseq(orq), 0);

  TEST(nta_outgoing_delay(orq), UINT_MAX);
  TEST_P(nta_outgoing_request_uri(orq), NULL);
  TEST_P(nta_outgoing_route_uri(orq), NULL);

  TEST_P(nta_outgoing_getresponse(orq), NULL);
  TEST_P(nta_outgoing_getrequest(orq), NULL);

  TEST_P(nta_outgoing_tagged(orq, NULL, NULL, NULL, NULL), NULL);
  TEST(nta_outgoing_cancel(orq), -1);
  TEST_P(nta_outgoing_tcancel(orq, NULL, NULL, TAG_END()), NULL);

  TEST_VOID(nta_outgoing_destroy(orq));

  TEST_1(irq = nta_incoming_default(nta));
  TEST_1(orq = nta_outgoing_default(nta, outgoing_default, ag));

  via[0] = nta_agent_via(nta)[0];
  via->v_next = NULL;

  TEST_1(nta_incoming_treply
	 (irq,
	  SIP_200_OK,
	  SIPTAG_VIA(via),
	  SIPTAG_CALL_ID_STR("oishciucnkrcoihciunskcisj"),
	  SIPTAG_CSEQ_STR("1 MESSAGE"),
	  SIPTAG_FROM_STR("Arska <sip:arska@example.com>;tag=aiojcidscd0i"),
	  SIPTAG_TO_STR("Jaska <sip:jaska@example.net>;tag=iajf8wru"),
	  TAG_END()) == 0);

  for (ag->ag_status = 0; ag->ag_status < 200; ) {
    su_root_step(ag->ag_root, 200);
  }

  TEST(nta_outgoing_status(orq), 0);

  TEST_VOID(nta_outgoing_destroy(orq));
  TEST_VOID(nta_incoming_destroy(irq));

  END();
}

/** Test API for errors */
static int api_test_errors(agent_t *ag)
{
  nta_agent_t *nta;
  su_root_t *root;
  su_home_t home[1];
  su_nanotime_t nano;

  BEGIN();

  memset(home, 0, sizeof home);
  home->suh_size = sizeof home;
  su_home_init(home);

  TEST_P(nta_agent_create(NULL,
			  (url_string_t *)"sip:*:*",
			  NULL,
			  NULL,
			  TAG_END()), NULL);

  TEST_1(root = su_root_create(NULL));

  TEST_P(nta_agent_create(root,
			  (url_string_t *)"http://localhost:*/invalid/bind/url",
			  NULL,
			  NULL,
			  TAG_END()), NULL);

  TEST_P(nta_agent_create(root,
			  (url_string_t *)"sip:*:*;transport=XXX",
			  NULL,
			  NULL,
			  TAG_END()), NULL);

  TEST_1(nta = nta_agent_create(root,
				(url_string_t *)"sip:*:*",
				NULL,
				NULL,
				TAG_END()));

  TEST_VOID(nta_agent_destroy(NULL));
  TEST_VOID(nta_agent_destroy(nta));

  TEST_1(nta = nta_agent_create(root,
				(url_string_t *)"sip:*:*",
				agent_callback,
				ag,
				TAG_END()));

  TEST_P(nta_agent_contact(NULL), NULL);
  TEST_P(nta_agent_via(NULL), NULL);
  TEST_S(nta_agent_version(nta), nta_agent_version(NULL));
  TEST_P(nta_agent_magic(NULL), NULL);
  TEST_P(nta_agent_magic(nta), (void *)ag);
  TEST(nta_agent_add_tport(NULL, NULL, TAG_END()), -1);
  TEST_P(nta_agent_newtag(home, "tag=%s", NULL), NULL);
  TEST_1(nta_agent_newtag(home, "tag=%s", nta));

  {
    msg_t *msg;
    TEST_1(nta_msg_create(NULL, 0) == NULL);
    TEST(nta_msg_complete(NULL), -1);

    TEST_1(msg = nta_msg_create(nta, 0));
    TEST(nta_msg_complete(msg), -1);
    TEST(nta_msg_request_complete(msg, NULL,
				  sip_method_unknown, "FOO", NULL), -1);
    TEST(nta_is_internal_msg(NULL), 0);
    TEST(nta_is_internal_msg(msg), 0);
    TEST_1(msg_set_flags(msg, NTA_INTERNAL_MSG));
    TEST(nta_is_internal_msg(msg), 1);
    TEST_VOID(msg_destroy(msg));
  }

  TEST_P(nta_leg_tcreate(NULL, NULL, NULL, TAG_END()), NULL);
  TEST_VOID(nta_leg_destroy(NULL));
  TEST_P(nta_leg_magic(NULL, NULL), NULL);
  TEST_VOID(nta_leg_bind(NULL, NULL, NULL));
  TEST_P(nta_leg_tag(NULL, "fidsafsa"), NULL);
  TEST_P(nta_leg_rtag(NULL, "fidsafsa"), NULL);
  TEST_P(nta_leg_get_tag(NULL), NULL);
  TEST(nta_leg_client_route(NULL, NULL, NULL), -1);
  TEST(nta_leg_server_route(NULL, NULL, NULL), -1);
  TEST_P(nta_leg_by_uri(NULL, NULL), NULL);
  TEST_P(nta_leg_by_dialog(NULL,  NULL, NULL, NULL, NULL, NULL, NULL), NULL);
  TEST_P(nta_leg_by_dialog(nta, NULL, NULL, NULL, NULL, NULL, NULL), NULL);

  TEST_P(nta_leg_make_replaces(NULL, NULL, 1), NULL);
  TEST_P(nta_leg_by_replaces(NULL, NULL), NULL);

  TEST_P(nta_incoming_create(NULL, NULL, NULL, NULL, TAG_END()), NULL);
  TEST_P(nta_incoming_create(nta, NULL, NULL, NULL, TAG_END()), NULL);

  TEST_VOID(nta_incoming_bind(NULL, NULL, NULL));
  TEST_P(nta_incoming_magic(NULL, NULL), NULL);

  TEST_P(nta_incoming_find(NULL, NULL, NULL), NULL);
  TEST_P(nta_incoming_find(nta, NULL, NULL), NULL);

  TEST_P(nta_incoming_tag(NULL, NULL), NULL);
  TEST_P(nta_incoming_gettag(NULL), NULL);

  TEST(nta_incoming_status(NULL), 400);
  TEST(nta_incoming_method(NULL), sip_method_invalid);
  TEST_P(nta_incoming_method_name(NULL), NULL);
  TEST_P(nta_incoming_url(NULL), NULL);
  TEST(nta_incoming_cseq(NULL), 0);
  TEST(nta_incoming_received(NULL, &nano), 0);
  TEST64(nano, 0);

  TEST(nta_incoming_set_params(NULL, TAG_END()), -1);

  TEST_P(nta_incoming_getrequest(NULL), NULL);
  TEST_P(nta_incoming_getrequest_ackcancel(NULL), NULL);
  TEST_P(nta_incoming_getresponse(NULL), NULL);

  TEST(nta_incoming_complete_response(NULL, NULL, 800, "foo", TAG_END()), -1);

  TEST(nta_incoming_treply(NULL, SIP_200_OK, TAG_END()), -1);
  TEST(nta_incoming_mreply(NULL, NULL), -1);

  TEST_VOID(nta_incoming_destroy(NULL));

  TEST_P(nta_outgoing_tcreate(NULL, outgoing_callback, ag,
			    URL_STRING_MAKE("sip:localhost"),
			    SIP_METHOD_MESSAGE,
			    URL_STRING_MAKE("sip:localhost"),
			    TAG_END()), NULL);

  TEST_P(nta_outgoing_mcreate(NULL, outgoing_callback, ag,
			    URL_STRING_MAKE("sip:localhost"),
			    NULL,
			    TAG_END()), NULL);

  TEST_P(nta_outgoing_default(NULL, NULL, NULL), NULL);

  TEST(nta_outgoing_status(NULL), 500);
  TEST(nta_outgoing_method(NULL), sip_method_invalid);
  TEST_P(nta_outgoing_method_name(NULL), NULL);
  TEST(nta_outgoing_cseq(NULL), 0);

  TEST(nta_outgoing_delay(NULL), UINT_MAX);
  TEST_P(nta_outgoing_request_uri(NULL), NULL);
  TEST_P(nta_outgoing_route_uri(NULL), NULL);

  TEST_P(nta_outgoing_getresponse(NULL), NULL);
  TEST_P(nta_outgoing_getrequest(NULL), NULL);

  TEST_P(nta_outgoing_tagged(NULL, NULL, NULL, NULL, NULL), NULL);
  TEST(nta_outgoing_cancel(NULL), -1);
  TEST_P(nta_outgoing_tcancel(NULL, NULL, NULL, TAG_END()), NULL);
  TEST_VOID(nta_outgoing_destroy(NULL));

  TEST_P(nta_outgoing_find(NULL, NULL, NULL, NULL), NULL);
  TEST_P(nta_outgoing_find(nta, NULL, NULL, NULL), NULL);

  TEST(nta_outgoing_status(NONE), 500);
  TEST(nta_outgoing_method(NONE), sip_method_invalid);
  TEST_P(nta_outgoing_method_name(NONE), NULL);
  TEST(nta_outgoing_cseq(NONE), 0);

  TEST(nta_outgoing_delay(NONE), UINT_MAX);
  TEST_P(nta_outgoing_request_uri(NONE), NULL);
  TEST_P(nta_outgoing_route_uri(NONE), NULL);

  TEST_P(nta_outgoing_getresponse(NONE), NULL);
  TEST_P(nta_outgoing_getrequest(NONE), NULL);

  TEST_P(nta_outgoing_tagged(NONE, NULL, NULL, NULL, NULL), NULL);
  TEST(nta_outgoing_cancel(NONE), -1);
  TEST_P(nta_outgoing_tcancel(NONE, NULL, NULL, TAG_END()), NULL);
  TEST_VOID(nta_outgoing_destroy(NONE));

  TEST_P(nta_reliable_treply(NULL, NULL, NULL, 0, NULL, TAG_END()), NULL);
  TEST_P(nta_reliable_mreply(NULL, NULL, NULL, NULL), NULL);
  TEST_VOID(nta_reliable_destroy(NULL));

  TEST_VOID(nta_agent_destroy(nta));
  TEST_VOID(su_root_destroy(root));
  TEST_VOID(su_home_deinit(home));

  END();
}

static int api_test_dialog_matching(agent_t *ag)
{
  nta_agent_t *nta;
  su_root_t *root;
  su_home_t home[1];
  nta_leg_t *leg, *dialog1, *dialog2, *dst, *defdst;
  sip_from_t *a1, *a2;
  sip_call_id_t *i;

  BEGIN();

  memset(home, 0, sizeof home);
  home->suh_size = sizeof home;
  su_home_init(home);

  TEST_1(root = su_root_create(NULL));

  TEST_1(nta = nta_agent_create(root,
				(url_string_t *)"sip:*:*",
				NULL,
				NULL,
				TAG_END()));

  TEST_1(dst = nta_leg_tcreate(nta, NULL, NULL,
				NTATAG_NO_DIALOG(1),
				URLTAG_URL("sip:joe@localhost"),
				TAG_END()));

  TEST_1(defdst = nta_leg_tcreate(nta, NULL, NULL,
				  NTATAG_NO_DIALOG(1),
				  TAG_END()));

  TEST_1(dialog1 =
	 nta_leg_tcreate(nta, NULL, NULL,
			 URLTAG_URL("sip:pc.al.us"),
			 SIPTAG_CALL_ID_STR("foobarbaz"),
			 /* local */
			 SIPTAG_FROM_STR("<sip:me.myself.i@foo.com>;tag=foo"),
			 /* remote */
			 SIPTAG_TO_STR("<sip:joe.boy@al.us>"),
			 TAG_END()));

  TEST_1(a1 = sip_from_make(home, "<sip:me.myself.i@foo.com>;tag=foo"));
  TEST_1(a2 = sip_from_make(home, "<sip:joe.boy@al.us>;tag=al"));
  TEST_1(i = sip_call_id_make(home, "foobarbaz"));

  TEST_1(dialog2 =
	 nta_leg_tcreate(nta, NULL, NULL,
			 SIPTAG_CALL_ID(i),
			 /* local */
			 SIPTAG_FROM(a2),
			 /* remote */
			 SIPTAG_TO(a1),
			 TAG_END()));

  TEST_1(!nta_leg_by_dialog(nta, NULL, NULL,
			    a1->a_tag, a1->a_url, a2->a_tag, a2->a_url));
  TEST_1(!nta_leg_by_dialog(NULL, NULL, i,
			    a1->a_tag, a1->a_url, a2->a_tag, a2->a_url));
  TEST_1(!nta_leg_by_dialog(nta, (void *)"sip:no.such.url", i,
			    a2->a_tag, a2->a_url, a1->a_tag, a1->a_url));
  TEST_1(!nta_leg_by_dialog(nta, a2->a_url, i,
			    a2->a_tag, a2->a_url, a1->a_tag, a1->a_url));

  TEST_P(leg = nta_leg_by_dialog(nta, NULL, i,
				 /* local */ a1->a_tag, a1->a_url,
				 /* remote */ a2->a_tag, a2->a_url),
	 dialog2);
  TEST_P(leg = nta_leg_by_dialog(nta, (void *)"sip:no.such.url", i,
				 /* local */ a1->a_tag, a1->a_url,
				 /* remote */ a2->a_tag, a2->a_url),
	 dialog2);
  TEST_P(leg = nta_leg_by_dialog(nta, a2->a_url, i,
				 a1->a_tag, a1->a_url, a2->a_tag, a2->a_url),
	 dialog2);

  TEST_P(leg = nta_leg_by_dialog(nta, NULL, i,
				 a2->a_tag, a2->a_url, a1->a_tag, a1->a_url),
	 dialog1);
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, a1->a_tag, a1->a_url),
	 dialog1);
  /* local tag is required because there is tag */
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, "xyzzy", a1->a_url),
	 NULL);
  /* local URI is ignored because we have tag */
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, a1->a_tag, a2->a_url),
	 dialog1);

  /* remote tag is ignored because there is no tag */
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 "xyzzy", a2->a_url, a1->a_tag, a1->a_url),
	 dialog1);
#if nomore
  /* remote url is required */
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a1->a_url, a1->a_tag, a1->a_url),
	 NULL);
#endif
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, NULL, a1->a_tag, a1->a_url),
	 dialog1);

  /* local url is used if there is no local tag */ /* XXX - not really */
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, NULL, NULL),
	 NULL);

  nta_leg_tag(dialog1, "al");
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, a1->a_tag, a1->a_url),
	 dialog1);
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, "xyzzy", a1->a_url),
	 NULL);
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, a1->a_tag, a1->a_url),
	 dialog1);
  TEST_P(leg = nta_leg_by_dialog(nta, (url_t *)"sip:pc.al.us", i,
				 a2->a_tag, a2->a_url, NULL, a1->a_url),
	 NULL);

  nta_leg_destroy(defdst);
  nta_leg_destroy(dst);
  nta_leg_destroy(dialog1);
  nta_leg_destroy(dialog2);

  TEST_VOID(nta_agent_destroy(nta));
  TEST_VOID(su_root_destroy(root));
  TEST_VOID(su_home_deinit(home));

  END();

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
char const nta_test_api_usage[] =
  "usage: %s OPTIONS\n"
  "where OPTIONS are\n"
  "   -v | --verbose    be verbose\n"
  "   -a | --abort      abort() on error\n"
  "   -q | --quiet      be quiet\n"
  "   -1                quit on first error\n"
  "   -l level          set logging level (0 by default)\n"
  "   --attach          print pid, wait for a debugger to be attached\n"
#if HAVE_ALARM
  "   --no-alarm        don't ask for guard ALARM\n"
#endif
  ;

void usage(int exitcode)
{
  fprintf(stderr, nta_test_api_usage, name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0, quit_on_single_failure = 0;
  int i, o_attach = 0, o_alarm = 1;

  agent_t ag[1] = {{ { SU_HOME_INIT(ag) }, 0, NULL }};

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--abort") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
      tstflags &= ~tst_verbatim;
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
    char *response, line[10];
    printf("nua_test: pid %lu\n", (unsigned long)getpid());
    printf("<Press RETURN to continue>\n");
    response = fgets(line, sizeof line, stdin);
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
  do { fflush(stderr); fflush(stdout); \
       if (retval && quit_on_single_failure) { su_deinit(); return retval; } \
  } while(0)

  retval |= api_test_init(ag); fflush(stdout); SINGLE_FAILURE_CHECK();
  if (retval == 0) {
    retval |= api_test_errors(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_destroy(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_params(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_stats(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_dialog_matching(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_tport(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_dialogs(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_default(ag); SINGLE_FAILURE_CHECK();
    retval |= api_test_user_via_fillin(ag); SINGLE_FAILURE_CHECK();
  }
  retval |= api_test_deinit(ag); fflush(stdout);

  su_home_deinit(ag->ag_home);

  su_deinit();

  return retval;
}
