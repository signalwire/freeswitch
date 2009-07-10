/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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

/**@CFILE check_nta_client.c
 *
 * @brief Check-driven tester for NTA client transactions
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2009 Nokia Corporation.
 */

#include "config.h"

#include "check_nta.h"
#include "s2base.h"
#include "s2dns.h"

#include <sofia-sip/nta.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/sres_sip.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NONE ((void *)-1)

static void
client_setup(void)
{
  s2_nta_setup("NTA", NULL, TAG_END());
  s2_nta_agent_setup(URL_STRING_MAKE("sip:0.0.0.0:*"), NULL, NULL,
		     NTATAG_DEFAULT_PROXY("sip:example.org"),
		     TAG_END());
}

static void
client_setup_udp_only_server(void)
{
  char const * const transports[] = { "udp", NULL };

  s2_nta_setup("NTA", transports, TAG_END());
  s2_nta_agent_setup(URL_STRING_MAKE("sip:0.0.0.0:*"), NULL, NULL,
		     NTATAG_DEFAULT_PROXY(s2sip->contact->m_url),
		     TAG_END());
}

static void
client_setup_tcp_only_server(void)
{
  char const * const transports[] = { "tcp", NULL };

  s2_nta_setup("NTA", transports, TAG_END());
  s2_nta_agent_setup(URL_STRING_MAKE("sip:0.0.0.0:*"), NULL, NULL,
		     NTATAG_DEFAULT_PROXY(s2sip->contact->m_url),
		     TAG_END());
}

static void
client_teardown(void)
{
  mark_point();
  s2_nta_teardown();
}

START_TEST(client_2_0_0)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;

  S2_CASE("client-2.0.0", "Send MESSAGE",
	  "Basic non-INVITE transaction with outbound proxy");

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL, NULL,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     TAG_END());
  fail_unless(orq != NULL);
  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  s2_sip_respond_to(request, NULL, 200, "2.0.0", TAG_END());
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_sip_free_message(request);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

START_TEST(client_2_0_1)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;

  S2_CASE("client-2.0.1", "Send MESSAGE",
	  "Basic non-INVITE transaction with "
	  "numeric per-transaction outbound proxy");

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL,
			     (url_string_t *)s2sip->contact->m_url,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     TAG_END());
  fail_unless(orq != NULL);
  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  s2_sip_respond_to(request, NULL, 200, "OK 2.0.1", TAG_END());
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_sip_free_message(request);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

START_TEST(client_2_0_2)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;

  char payload[2048];

  S2_CASE("client-2.0.2", "Send MESSAGE",
	  "Basic non-INVITE transaction exceeding "
	  "default path MTU (1300 bytes)");

  memset(payload, 'x', sizeof payload);
  payload[(sizeof payload) - 1] = '\0';

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL, NULL,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     SIPTAG_PAYLOAD_STR(payload),
			     TAG_END());
  fail_unless(orq != NULL);
  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  fail_unless(request->sip->sip_via->v_protocol == sip_transport_tcp);

  s2_sip_respond_to(request, NULL, 200, "OK 2.0.2", TAG_END());
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

/* ---------------------------------------------------------------------- */

TCase *check_nta_client_2_0(void)
{
  TCase *tc = tcase_create("NTA 2.0 - Client");

  tcase_add_checked_fixture(tc, client_setup, client_teardown);

  tcase_set_timeout(tc, 2);

  tcase_add_test(tc, client_2_0_0);
  tcase_add_test(tc, client_2_0_1);
  tcase_add_test(tc, client_2_0_2);

  return tc;
}

/* ---------------------------------------------------------------------- */

START_TEST(client_2_1_0)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;

  char payload[2048];

  S2_CASE("client-2.1.0", "Try UDP after trying with TCP",
	  "TCP connect() is refused");

  memset(payload, 'x', sizeof payload);
  payload[(sizeof payload) - 1] = '\0';

  client_setup_udp_only_server();

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL, NULL,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     SIPTAG_PAYLOAD_STR(payload),
			     TAG_END());
  fail_unless(orq != NULL);

  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  fail_unless(request->sip->sip_via->v_protocol == sip_transport_udp);
  s2_sip_respond_to(request, NULL, 200, "OK", TAG_END());
  s2_sip_free_message(request);

  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

#undef SU_LOG

#include "tport_internal.h"

tport_vtable_t hacked_tcp_vtable;

/* Make TCP connection to 192.168.255.2:9999 */
static tport_t *
hacked_tcp_connect(tport_primary_t *pri,
		   su_addrinfo_t *ai,
		   tp_name_t const *tpn)
{
  su_addrinfo_t fake_ai[1];
  su_sockaddr_t fake_addr[1];
  uint32_t fake_ip = htonl(0xc0a8ff02); /* 192.168.255.2 */

  *fake_ai = *ai;
  assert(ai->ai_addrlen <= (sizeof fake_addr));
  fake_ai->ai_addr = memcpy(fake_addr, ai->ai_addr, ai->ai_addrlen);

  fake_ai->ai_family = AF_INET;
  fake_addr->su_family = AF_INET;
  memcpy(&fake_addr->su_sin.sin_addr, &fake_ip, sizeof fake_ip);
  fake_addr->su_sin.sin_port = htons(9999);

  return tport_base_connect(pri, fake_ai, ai, tpn);
}

START_TEST(client_2_1_1)
{
  tport_t *tp;

  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;

  char payload[2048];

  S2_CASE("client-2.1.1", "Try UDP after trying with TCP",
	  "TCP connect() times out");

  memset(payload, 'x', sizeof payload);
  payload[(sizeof payload) - 1] = '\0';

  client_setup_udp_only_server();

  hacked_tcp_vtable = tport_tcp_vtable;
  hacked_tcp_vtable.vtp_connect = hacked_tcp_connect;
  fail_unless(tport_tcp_vtable.vtp_connect == NULL);

  for (tp = tport_primaries(nta_agent_tports(s2->nta));
       tp;
       tp = tport_next(tp)) {
    if (tport_is_tcp(tp)) {
      tp->tp_pri->pri_vtable = &hacked_tcp_vtable;
      break;
    }
  }

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL,
			     NULL,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     SIPTAG_PAYLOAD_STR(payload),
			     TAG_END());
  fail_unless(orq != NULL);

  s2_fast_forward(1, s2->root);
  s2_fast_forward(1, s2->root);
  s2_fast_forward(1, s2->root);
  s2_fast_forward(1, s2->root);
  s2_fast_forward(1, s2->root);

  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  fail_unless(request->sip->sip_via->v_protocol == sip_transport_udp);
  s2_sip_respond_to(request, NULL, 200, "OK", TAG_END());
  s2_sip_free_message(request);

  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

START_TEST(client_2_1_2)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;
  url_t udpurl[1];

  S2_CASE("client-2.1.2", "Send MESSAGE",
	  "Non-INVITE transaction to TCP-only server");

  client_setup_tcp_only_server();

  *udpurl = *s2sip->tcp.contact->m_url;
  udpurl->url_params = "transport=udp";

  /* Create DNS records for both UDP and TCP, resolver matches UDP */
  s2_dns_domain("udptcp.org", 1,
		"s2", 1, udpurl,
		"s2", 2, s2sip->tcp.contact->m_url,
		NULL);

  /* Sent to tport selected by resolver */
  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL,
			     URL_STRING_MAKE("sip:udptcp.org"),
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     TAG_END());
  fail_unless(orq != NULL);
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 503,
			     0);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);

  /* Message size exceeds 1300, tries to use TCP even if NAPTR points to UDP */
  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL,
			     URL_STRING_MAKE("sip:udptcp.org"),
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.0.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
#define ROW "012345678901234567890123456789012345678901234\n"
			     SIPTAG_PAYLOAD_STR( /* > 1300 bytes */
       	       	       	       "0000 " ROW "0050 " ROW
			       "0100 " ROW "0150 " ROW
			       "0200 " ROW "0250 " ROW
			       "0300 " ROW "0350 " ROW
			       "0400 " ROW "0450 " ROW
			       "0500 " ROW "0550 " ROW
			       "0600 " ROW "0650 " ROW
			       "0700 " ROW "0750 " ROW
			       "0800 " ROW "0850 " ROW
			       "0900 " ROW "0950 " ROW
			       "1000 " ROW "1050 " ROW
			       "1100 " ROW "1150 " ROW
       	       	       	       "1200 " ROW "1250 " ROW
						 ),
#undef ROW
			     TAG_END());
  fail_unless(orq != NULL);
  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  fail_unless(request->sip->sip_via->v_protocol == sip_transport_tcp);
  s2_sip_respond_to(request, NULL, 200, "2.1.2", TAG_END());
  s2_sip_free_message(request);
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST


TCase *check_nta_client_2_1(void)
{
  TCase *tc = tcase_create("NTA 2.1 - Client");

  tcase_add_checked_fixture(tc, NULL, client_teardown);

  tcase_set_timeout(tc, 20);

  tcase_add_test(tc, client_2_1_0);
  tcase_add_test(tc, client_2_1_1);
  tcase_add_test(tc, client_2_1_2);

  return tc;
}

/* ---------------------------------------------------------------------- */

#include <sofia-resolv/sres_record.h>

START_TEST(client_2_2_0)
{
  nta_outgoing_t *orq;
  struct message *request;
  struct event *response;
  static tp_name_t const tpn[1] = {{ "*", "*", "*", "5060", NULL, NULL }};
  static char const * const default_protocols[] = { "udp", "tcp", NULL };

  char proxy[] = "sip:cname.example.org:0000000";

  S2_CASE("client-2.2.0", "Send MESSAGE",
	  "Basic non-INVITE transaction with target using CNAME");
  /* Test for sf.net bug #2531152 */

  s2_nta_setup("NTA", NULL, TAG_END());

  fail_unless(s2sip->udp.contact != NULL);

  if (s2sip->udp.contact->m_url->url_port == NULL ||
      tport_tbind(s2sip->master, tpn, default_protocols,
		  TPTAG_SERVER(1),
		  TAG_END()) == -1) {
    snprintf(proxy, sizeof proxy, "sip:cname.example.org:%s",
	     s2sip->udp.contact->m_url->url_port);
  }
  else {
    strcpy(proxy, "sip:cname.example.org");
  }

  s2_dns_default("example.org.");

  s2_dns_record("cname.example.org.", sres_type_a,
		"", sres_type_cname, "a.example.org.",
		"a", sres_type_a, s2sip->udp.contact->m_url->url_host,
		NULL);

  s2_dns_record("cname.example.org.", sres_type_naptr,
		"", sres_type_cname, "a.example.org.",
		NULL);

  s2_dns_record("cname.example.org.", sres_type_aaaa,
		"", sres_type_cname, "a.example.org.",
		NULL);

  s2_dns_record("a.example.org.", sres_type_a,
		"", sres_type_a, s2sip->udp.contact->m_url->url_host,
		NULL);


  s2_nta_agent_setup(URL_STRING_MAKE("sip:0.0.0.0:*"), NULL, NULL,
		     NTATAG_DEFAULT_PROXY(proxy),
		     TAG_END());

  orq = nta_outgoing_tcreate(s2->default_leg,
			     s2_nta_orq_callback, NULL, NULL,
			     SIP_METHOD_MESSAGE,
			     URL_STRING_MAKE("sip:test2.2.example.org"),
			     SIPTAG_FROM_STR("<sip:client@example.net>"),
			     TAG_END());
  fail_unless(orq != NULL);
  request = s2_sip_wait_for_request(SIP_METHOD_MESSAGE);
  fail_unless(request != NULL);
  s2_sip_respond_to(request, NULL, 200, "2.2.0", TAG_END());
  response = s2_nta_wait_for(wait_for_orq, orq,
			     wait_for_status, 200,
			     0);
  s2_sip_free_message(request);
  s2_nta_free_event(response);
  nta_outgoing_destroy(orq);
}
END_TEST

TCase *
check_nta_client_2_2(void)
{
  TCase *tc = tcase_create("NTA 2.2 - Client");

  tcase_add_checked_fixture(tc, NULL, client_teardown);

  tcase_set_timeout(tc, 2);

  tcase_add_test(tc, client_2_2_0);

  return tc;
}

