/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
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

/**@CFILE check_sres_sip.c
 *
 * @brief Check-driven tester for SIP URI resolver
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2009 Nokia Corporation.
 */

#include "config.h"

#define SRES_SIP_MAGIC_T void

#include <sofia-sip/sres_sip.h>

#include "s2check.h"
#include "s2dns.h"

#include <sofia-sip/su_wait.h>
#include <sofia-sip/sresolv.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_string.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

SOFIAPUBVAR su_log_t sresolv_log[];

static void
setup_logs(int level)
{
  su_log_soft_set_level(sresolv_log, level);
}

/* */
struct context {
  su_root_t *root;
  sres_resolver_t *sres;
} x[1];

static su_addrinfo_t hint_udp_tcp[2];
static su_addrinfo_t hint_udp_tcp_tls[2];
static su_addrinfo_t hint_udp_tcp_ip4[2];
static su_addrinfo_t hint_tls[1];
static su_addrinfo_t hint_tls_udp_tcp[1];

static void
resolver_setup(void)
{
  x->root = su_root_create(NULL);
  assert(x->root);
  s2_dns_setup(x->root);

  s2_dns_record("test.example.com.", sres_type_a,
		"test.example.com.", sres_type_a, "12.13.14.25",
		NULL);

  x->sres = sres_resolver_create(
    x->root, NULL,
#if HAVE_WIN32
    SRESTAG_RESOLV_CONF("NUL"),
#else
    SRESTAG_RESOLV_CONF("/dev/null"),
#endif
    TAG_END());

  hint_udp_tcp[0].ai_protocol = TPPROTO_UDP;
  hint_udp_tcp[0].ai_next = &hint_udp_tcp[1];
  hint_udp_tcp[1].ai_protocol = TPPROTO_TCP;

  hint_udp_tcp_ip4[0].ai_protocol = TPPROTO_UDP;
  hint_udp_tcp_ip4[0].ai_family = AF_INET;
  hint_udp_tcp_ip4[0].ai_next = &hint_udp_tcp_ip4[1];
  hint_udp_tcp_ip4[1].ai_protocol = TPPROTO_TCP;
  hint_udp_tcp_ip4[1].ai_family = AF_INET;

  hint_tls[0].ai_protocol = TPPROTO_TLS;

  hint_tls_udp_tcp[0].ai_protocol = TPPROTO_TLS;
  hint_tls_udp_tcp[0].ai_next = hint_udp_tcp;

  hint_udp_tcp_tls[0].ai_protocol = TPPROTO_UDP;
  hint_udp_tcp_tls[0].ai_next = &hint_udp_tcp_tls[1];
  hint_udp_tcp_tls[1].ai_protocol = TPPROTO_TCP;
  hint_udp_tcp_tls[1].ai_next = &hint_udp_tcp_tls[2];
  hint_udp_tcp_tls[2].ai_protocol = TPPROTO_TLS;
  hint_udp_tcp_tls[2].ai_next = NULL;

  setup_logs(0);
}

static void
resolver_teardown(void)
{
  sres_resolver_unref(x->sres), x->sres = NULL;
  s2_dns_teardown();
}

/* ---------------------------------------------------------------------- */

static uint16_t
ai_port(su_addrinfo_t const *ai)
{
  return ai && ai->ai_addr ?
    ntohs(((struct sockaddr_in *)ai->ai_addr)->sin_port) : 0;
}

static int
ai_ip4_match(su_addrinfo_t const *ai,
	     char const *addr,
	     uint16_t port)
{
  struct in_addr in, in_in_ai;

  if (ai_port(ai) != port)
    return 0;

  if (ai->ai_family != AF_INET)
    return 0;

  if (su_inet_pton(AF_INET, addr, &in) <= 0)
    return 0;

  in_in_ai = ((struct sockaddr_in *)ai->ai_addr)->sin_addr;

  return memcmp(&in, &in_in_ai, (sizeof in)) == 0;
}

/* ---------------------------------------------------------------------- */

START_TEST(invalid)
{
  sres_sip_t *srs;

#if 0
  S2_CASE("1.0", "Check invalid input values",
	  "Detailed explanation for empty test case.");
#endif

  srs = sres_sip_new(NULL, (void *)"sip:test.example.com.", NULL,
		    1, 1,
		    NULL, NULL);
  fail_if(srs != NULL);

  srs = sres_sip_new(x->sres, NULL, NULL,
		    1, 1,
		    NULL, NULL);

  fail_if(srs != NULL);

  srs = sres_sip_new(x->sres, (void *)"tilu.liu.", NULL, 1, 1, NULL, NULL);
  fail_unless(srs != NULL);
  fail_if(sres_sip_error(srs) != SRES_SIP_ERR_BAD_URI);
  fail_if(sres_sip_next_step(srs));
  fail_if(sres_sip_results(srs) != NULL);
  sres_sip_unref(srs);
}
END_TEST

static void
resolver_callback(void *userdata,
		  sres_sip_t *srs,
		  int final)
{
  if (final)
    su_root_break((su_root_t *)userdata);
}

START_TEST(not_found)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;

  srs = sres_sip_new(x->sres, (void *)"sip:notfound.example.net", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  fail_if(ai != NULL);

  fail_if(sres_sip_error(srs) != SRES_SIP_ERR_NO_NAME);

  sres_sip_unref(srs);
}
END_TEST

START_TEST(failure)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;

  /* there is no server at all */
  s2_dns_teardown();

  srs = sres_sip_new(x->sres, (void *)"sip:timeout.example.net", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  fail_if(ai != NULL);

  fail_if(sres_sip_error(srs) != SRES_SIP_ERR_AGAIN);

  sres_sip_unref(srs);
}
END_TEST

/* time() replacement */
static time_t offset;

time_t
time(time_t *return_time)
{
  su_time_t tv;
  time_t now;

  su_time(&tv);

  now = tv.tv_sec + offset - 2208988800UL; /* NTP_EPOCH */

  if (return_time)
    *return_time = now;

  return now;
}

/* Drop packet */
static int drop(void *data, size_t len, void *userdata)
{
  return 0;
}

/* Fast forward time, call resolver timer */
static void wakeup(su_root_magic_t *magic,
		   su_timer_t *timer,
		   su_timer_arg_t *extra)
{
  offset++;
  sres_resolver_timer(x->sres, 0);
}

START_TEST(timeout)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  su_timer_t *faster = su_timer_create(su_root_task(x->root), 10);

  su_timer_run(faster, wakeup, NULL);

  s2_dns_set_filter(drop, NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:timeout.example.net", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  fail_if(ai != NULL);

  fail_if(sres_sip_error(srs) != SRES_SIP_ERR_AGAIN);

  sres_sip_unref(srs);

  s2_dns_set_filter(NULL, NULL);

  su_timer_destroy(faster);
}
END_TEST

START_TEST(found_a)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;

  srs = sres_sip_new(x->sres, (void *)"sip:test.example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_ip)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  url_string_t *uri;

  srs = sres_sip_new(x->sres, (void *)"sip:127.21.50.37", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

  srs = sres_sip_new(x->sres, (void *)"sip:127.21.50.37:", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

#if SU_HAVE_IN6

  uri = (void *)"sip:[3ff0:0010:3012:c000:02c0:95ff:fee2:4b78]";

  srs = sres_sip_new(x->sres, uri, NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

#endif
}
END_TEST

START_TEST(found_maddr_a)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  url_string_t *uri;

  s2_dns_record("maddr.example.com", sres_type_a,
		"", sres_type_a, "11.12.13.14",
		NULL);

  uri = (void *)"sip:test.example.com;lr=1;maddr=maddr.example.com;more";

  srs = sres_sip_new(x->sres, uri, NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_unless(ai_ip4_match(ai, "11.12.13.14", 5060));
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_cname)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  url_string_t *uri;

  s2_dns_default("example.com.");

  s2_dns_record("cname1.example.com.", sres_type_a,
		"", sres_type_cname, "a.example.com.",
		"a", sres_type_a, "11.12.13.14",
		NULL);

  s2_dns_record("cname1.example.com.", sres_type_naptr,
		"", sres_type_cname, "a.example.com.",
		NULL);

  s2_dns_record("cname1.example.com.", sres_type_aaaa,
		"", sres_type_cname, "a.example.com.",
		NULL);

  s2_dns_record("a.example.com.", sres_type_a,
		"", sres_type_a, "11.12.13.14",
		NULL);

  s2_dns_record("cname2.example.com.", sres_type_a,
		"", sres_type_cname, "a.example.com.",
		"test.example.com.", sres_type_a, "11.12.13.14",
		NULL);

  uri = (void *)"sip:cname1.example.com";

  srs = sres_sip_new(x->sres, uri, NULL, 1, 1, resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_unless(ai_ip4_match(ai, "11.12.13.14", 5060));
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_maddr_ip)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  url_string_t *uri;

  uri = (void *)"sip:not.existing.net;maddr=127.21.50.37";

  srs = sres_sip_new(x->sres, uri, NULL, 1, 1, resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

#if SU_HAVE_IN6

  uri = (void *)"sip:test.example.com:"
    ";maddr=3ff0:0010:3012:c000:02c0:95ff:fee2:4b78";

  srs = sres_sip_new(x->sres, uri, NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

  uri = (void *)"sip:test.example.com:"
    ";maddr=[3ff0:0010:3012:c000:02c0:95ff:fee2:4b78]";

  srs = sres_sip_new(x->sres, uri, NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);
  fail_if(sres_sip_next_step(srs));
  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);

#endif
}
END_TEST

START_TEST(found_a_aaaa)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;

  s2_dns_default("example.com.");

  s2_dns_record("ip6ip4.example.com.", sres_type_a,
		"", sres_type_a, "12.13.14.25",
		"", sres_type_aaaa, "3ff0:0010:3012:c000:02c0:95ff:fee2:4b78",
		NULL);

  s2_dns_record("ip6ip4", sres_type_aaaa,
		"", sres_type_aaaa, "3ff0:0010:3012:c000:02c0:95ff:fee2:4b78",
		"", sres_type_a, "12.13.14.25",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:ip6ip4.example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
#if SU_HAVE_IN6
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_unless(ai->ai_family == AF_INET6);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET6);
  fail_if(!(ai = ai->ai_next));
#endif
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_naptr)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		d, sres_type_naptr, 20, 50, "s", "SIP+D2T", "", "_sip._tcp",
		/* priority weight port target */
		"_sip._tcp", sres_type_srv, 2, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.15",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_naptr_nohint)
{
  sres_sip_t *srs;
  char const *d;

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		d, sres_type_naptr, 20, 50, "s", "SIP+D2Z", "", "_sip._tilulilu",
		/* priority weight port target */
		"_sip._tilulilu", sres_type_srv, 2, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.15",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  fail_unless(sres_sip_results(srs) == NULL);
  fail_unless(sres_sip_error(srs) == SRES_SIP_ERR_NO_TPORT);

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_bad_naptr)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		d, sres_type_naptr, 20, 50, "s", "ZIP+D2T", "", "_zip._tcp",
		NULL);

  s2_dns_record(d, sres_type_a,
		"", sres_type_a, "11.12.13.14",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_run(x->root);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_if((ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(ignore_naptr)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		d, sres_type_naptr, 20, 50, "s", "SIP+D2T", "", "_sip._tcp",
		/* priority weight port target */
		"_sip._tcp", sres_type_srv, 2, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.15",
		NULL);

  s2_dns_record(d, sres_type_a,
		"", sres_type_a, "13.14.15.16",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com:5060", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  ai = ai->ai_next; assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  ai = ai->ai_next; assert(ai == NULL);

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_naptr2)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  /* Test case without caching, two A addresses */

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		"", sres_type_naptr, 20, 50, "s", "SIP+D2T", "", "_sip._tcp",
		NULL);

  s2_dns_record("_sip._tcp", sres_type_srv,
		/* priority weight port target */
		"", sres_type_srv, 2, 100, 5060, "sip00",
		NULL);

  s2_dns_record("sip00", sres_type_a,
		"", sres_type_a, "12.13.14.15",
		"", sres_type_a, "12.13.14.16",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_naptr3)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  /* Check that sip: resolves only to TLS */

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		d, sres_type_naptr, 30, 50, "s", "SIP+D2T", "", "_sip._tcp.srv00",
		d, sres_type_naptr, 20, 50, "s", "SIPS+D2T", "", "_sips._tcp.srv00",
		/* priority weight port target */
		"_sips._tcp.srv00", sres_type_srv, 2, 100, 5061, "sips00",
		"sips00", sres_type_a, "12.13.14.15",
		"_sip._tcp.srv00", sres_type_srv, 2, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.16",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TLS);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);

  /* Hint no support for TLS */
  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hint_udp_tcp,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_naptr_with_a)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;
  int i;

  s2_dns_default(d = "example.com.");

  s2_dns_record(d, sres_type_naptr,
		/* order priority flags services regexp target */
		"", sres_type_naptr, 20, 50, "a", "SIP+D2U", "", "sip00",
		"", sres_type_naptr, 20, 50, "a", "SIP+D2T", "", "sip00",
		"", sres_type_naptr, 20, 50, "a", "SIPS+D2T", "", "sip00",
		"sip00", sres_type_a, "12.13.14.15",
		NULL);

  /* Check that cache does not change ordering */
  for (i = 0; i < 3; i++) {
    srs = sres_sip_new(x->sres, (void *)"sip:example.com", NULL,
		      1, 1,
		      resolver_callback, x->root);
    fail_if(srs == NULL);


    if (i == 0) {
      while (sres_sip_next_step(srs))
	su_root_step(x->root, 1000);
    }
    else {
      fail_if(sres_sip_next_step(srs));
    }

    ai = sres_sip_results(srs);
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_UDP);
    fail_unless(ai->ai_family == AF_INET);

    ai = ai->ai_next;
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_TCP);
    fail_unless(ai->ai_family == AF_INET);

    ai = ai->ai_next;
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_TLS);
    fail_unless(ai->ai_family == AF_INET);

    fail_if(ai->ai_next);

    sres_sip_unref(srs);
  }
}
END_TEST

START_TEST(found_srv)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;

  /* Check that hints are followed: */

  s2_dns_default(d = "example.com.");

  s2_dns_record("_sips._tcp", sres_type_srv,
		"", sres_type_srv, 2, 100, 5061, "sips00",
		"sips00", sres_type_a, "12.13.14.15",
		NULL);
  s2_dns_record("_sip._tcp", sres_type_srv,
		"", sres_type_srv, 2, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.16",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hint_udp_tcp,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);

  /* Hints prefer TLS */
  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hint_tls_udp_tcp,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TLS);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);

  /* Hints support TLS, prefer TCP */
  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hint_udp_tcp_tls,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TLS);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
  sres_resolver_unref(x->sres);

  x->sres = sres_resolver_create(
    x->root, NULL,
#if HAVE_WIN32
    SRESTAG_RESOLV_CONF("NUL"),
#else
    SRESTAG_RESOLV_CONF("/dev/null"),
#endif
    TAG_END());

  /* Add UDP record */
  s2_dns_record("_sip._udp", sres_type_srv,
		"", sres_type_srv, 1, 100, 5060, "sip00",
		"sip00", sres_type_a, "12.13.14.16",
		NULL);

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hint_udp_tcp_tls,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);
  assert(ai != NULL);
  fail_if(ai->ai_protocol != TPPROTO_UDP);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TCP);
  fail_unless(ai->ai_family == AF_INET);
  fail_if(!(ai = ai->ai_next));
  fail_if(ai->ai_protocol != TPPROTO_TLS);
  fail_unless(ai->ai_family == AF_INET);
  fail_unless(!(ai = ai->ai_next));

  sres_sip_unref(srs);
}
END_TEST

START_TEST(found_multi_srv)
{
  sres_sip_t *srs;
  su_addrinfo_t const *ai;
  char const *d;
  int i;
  su_addrinfo_t hints[1];

  /* Check that srv records are selected correctly by priority and weight */

  s2_dns_default(d = "example.com.");

  s2_dns_record("_sips._tcp", sres_type_srv,
		"", sres_type_srv, 2, 100, 5061, "sips00",
		"", sres_type_srv, 2, 100, 5061, "sips01",
		"", sres_type_srv, 2, 200, 5061, "sips02",
		"sips00", sres_type_a, "12.13.0.10",
		"sips01", sres_type_a, "12.13.1.10",
		"sips02", sres_type_a, "12.13.2.10",
		NULL);

  mark_point();

  s2_dns_record("_sip._tcp", sres_type_srv,
		"", sres_type_srv, 4, 1, 5060, "sip00",
		"", sres_type_srv, 3, 100, 5060, "sip01",
		"", sres_type_srv, 3, 10, 5060, "sip02",
		"sip00", sres_type_a, "12.13.0.1",
		"sip00", sres_type_a, "12.13.0.2",
		"sip01", sres_type_a, "12.13.1.2",
		"sip01", sres_type_a, "12.13.1.1",
		"sip02", sres_type_a, "12.13.2.1",
		"sip02", sres_type_a, "12.13.2.2",
		NULL);
  mark_point();

  s2_dns_record("_sip._udp", sres_type_srv,
		"", sres_type_srv, 10, 100, 5060, "sip00",
		"", sres_type_srv, 10, 0, 5060, "sip01",
		"", sres_type_srv, 1, 100, 5060, "sip02",
		"sip00", sres_type_a, "12.13.0.1",
		"sip00", sres_type_a, "12.13.0.2",
		"sip01", sres_type_a, "12.13.1.2",
		"sip01", sres_type_a, "12.13.1.1",
		"sip02", sres_type_a, "12.13.2.1",
		"sip02", sres_type_a, "12.13.2.2",
		NULL);

  mark_point();

  /* Ask resolver to return canonic names */
  *hints = *hint_udp_tcp_tls,  hints->ai_flags |= AI_CANONNAME;

  srs = sres_sip_new(x->sres, (void *)"sip:example.com", hints,
		    1, 1,
		    resolver_callback, x->root);
  fail_if(srs == NULL);

  while (sres_sip_next_step(srs))
    su_root_step(x->root, 1000);

  ai = sres_sip_results(srs);

  for (i = 0; i < 6; i++) {
    static char const * const names[] = {
      "sip02.", "sip02.",	/* Selected because priority */
      "sip00.", "sip00.",	/* Because of sip01 has weight=0  */
      "sip01.", "sip01."
    };
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_UDP);
    fail_unless(ai->ai_family == AF_INET);
    fail_unless(ai_port(ai) == 5060);
    fail_if(memcmp(ai->ai_canonname, names[i], strlen(names[i])) != 0);
    ai = ai->ai_next;
  }

  for (i = 0; i < 6; i++) {
    static char const * const names[] = {
      "sip", "sip",
      "sip", "sip",
      "sip00.", "sip00."
    };
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_TCP);
    fail_unless(ai->ai_family == AF_INET);
    fail_unless(ai_port(ai) == 5060);
    fail_if(memcmp(ai->ai_canonname, names[i], strlen(names[i])) != 0);
    ai = ai->ai_next;
  }

  for (i = 0; i < 3; i++) {
    assert(ai != NULL);
    fail_if(ai->ai_protocol != TPPROTO_TLS);
    fail_unless(ai->ai_family == AF_INET);
    fail_unless(ai_port(ai) == 5061);
    ai = ai->ai_next;
  }

  sres_sip_unref(srs);
}
END_TEST

TCase *api_tcase(void)
{
  TCase *tc = tcase_create("1 - Test simple resolving");

  tcase_add_checked_fixture(tc, resolver_setup, resolver_teardown);

  tcase_add_test(tc, invalid);
  tcase_add_test(tc, not_found);
  tcase_add_test(tc, failure);
  tcase_add_test(tc, timeout);
  tcase_add_test(tc, found_a);
  tcase_add_test(tc, found_cname);
  tcase_add_test(tc, found_ip);
  tcase_add_test(tc, found_maddr_a);
  tcase_add_test(tc, found_maddr_ip);
  tcase_add_test(tc, found_a_aaaa);
  tcase_add_test(tc, found_naptr);
  tcase_add_test(tc, found_bad_naptr);
  tcase_add_test(tc, found_naptr_nohint);
  tcase_add_test(tc, found_naptr2);
  tcase_add_test(tc, found_naptr3);
  tcase_add_test(tc, found_naptr_with_a);
  tcase_add_test(tc, ignore_naptr);
  tcase_add_test(tc, found_srv);
  tcase_add_test(tc, found_multi_srv);

  return tc;
}

/* ---------------------------------------------------------------------- */

static void usage(int exitcode)
{
  fprintf(exitcode ? stderr : stdout,
	  "usage: check_sres_sip [--xml=logfile] case,...\n");
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int i, failed = 0;

  Suite *suite = suite_create("Unit tests for SIP URI resolver");
  SRunner *runner;
  char const *xml = NULL;

  s2_select_tests(getenv("CHECK_CASES"));

  for (i = 1; argv[i]; i++) {
    if (su_strnmatch(argv[i], "--xml=", strlen("--xml="))) {
      xml = argv[i] + strlen("--xml=");
    }
    else if (su_strmatch(argv[i], "--xml")) {
      if (!(xml = argv[++i]))
	usage(2);
    }
    else if (su_strmatch(argv[i], "-?") ||
	     su_strmatch(argv[i], "-h") ||
	     su_strmatch(argv[i], "--help"))
      usage(0);
    else
      s2_select_tests(argv[i]);
  }

  suite_add_tcase(suite, api_tcase());

  runner = srunner_create(suite);
  if (xml)
    srunner_set_xml(runner, xml);
  srunner_run_all(runner, CK_ENV);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}
