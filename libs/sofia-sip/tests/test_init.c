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

/**@CFILE test_init.c
 * @brief Init nua test context
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"

#include <sofia-sip/tport_tag.h>
#include <sofia-sip/auth_module.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

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

int test_nua_init(struct context *ctx,
		  int start_proxy,
		  url_t const *o_proxy,
		  int start_nat,
		  tag_type_t tag, tag_value_t value, ...)
{
  BEGIN();
  struct event *e;
  sip_contact_t const *m = NULL;
  sip_from_t const *sipaddress = NULL;
  sip_allow_t const *allow = NULL;
  sip_supported_t const *supported = NULL;
  char const *appl_method = NULL;
  url_t const *p_uri, *a_uri, *b_uri;		/* Proxy URI */
  char const *initial_route = NULL;	/* Initial route towards internal proxy */
  char const *a_bind, *a_bind2;
  url_t *e_proxy = NULL;
  int err = -1;
  url_t b_proxy[1];

  a_bind = a_bind2 = "sip:0.0.0.0:*";

#if SU_HAVE_OSX_CF_API
  if (ctx->osx_runloop)
    ctx->root = su_root_osx_runloop_create(NULL);
  else
#endif
  ctx->root = su_root_create(NULL);
  TEST_1(ctx->root);

  /* Disable threading by command line switch -s */
  su_root_threading(ctx->root, ctx->threading);

  if (start_proxy && !o_proxy) {
    int temp;

    if (print_headings)
      printf("TEST NUA-2.1.1: init proxy P\n");

#ifndef _WIN32
    temp = mkstemp(passwd_name);
#else
    temp = open(passwd_name, O_WRONLY|O_CREAT|O_TRUNC, 666);
#endif
    TEST_1(temp != -1);
    atexit(remove_tmp);		/* Make sure temp file is unlinked */

    TEST_SIZE(write(temp, passwd, strlen(passwd)), strlen(passwd));

    TEST_1(close(temp) == 0);

    ctx->p = test_proxy_create(ctx->root,
			       TAG_IF(ctx->proxy_logging, TPTAG_LOG(1)),
			       TAG_END());

    if (ctx->p) {
      ctx->a.domain =
	test_proxy_add_domain(ctx->p,
			      URL_STRING_MAKE("sip:example.com")->us_url,
			      AUTHTAG_METHOD("Digest"),
			      AUTHTAG_REALM("test-proxy"),
			      AUTHTAG_OPAQUE("kuik"),
			      AUTHTAG_DB(passwd_name),
			      AUTHTAG_QOP("auth-int"),
			      AUTHTAG_ALGORITHM("md5"),
			      AUTHTAG_NEXT_EXPIRES(60),
			      TAG_END());

      ctx->b.domain =
	test_proxy_add_domain(ctx->p,
			      URL_STRING_MAKE("sip:example.org")->us_url,
			      AUTHTAG_METHOD("Digest"),
			      AUTHTAG_REALM("test-proxy"),
			      AUTHTAG_OPAQUE("kuik"),
			      AUTHTAG_DB(passwd_name),
			      AUTHTAG_QOP("auth-int"),
			      AUTHTAG_ALGORITHM("md5"),
			      AUTHTAG_NEXT_EXPIRES(60),
			      TAG_END());

      test_proxy_domain_set_outbound(ctx->b.domain, 1);

      ctx->c.domain =
	test_proxy_add_domain(ctx->p,
			      URL_STRING_MAKE("sip:example.net")->us_url,
			      AUTHTAG_METHOD("Digest"),
			      AUTHTAG_REALM("test-proxy"),
			      AUTHTAG_OPAQUE("kuik"),
			      AUTHTAG_DB(passwd_name),
			      AUTHTAG_QOP("auth-int"),
			      AUTHTAG_ALGORITHM("md5"),
			      AUTHTAG_NEXT_EXPIRES(60),
			      AUTHTAG_MAX_NCOUNT(1),
			      AUTHTAG_ALLOW("ACK, CANCEL"),
			      TAG_END());

      test_proxy_domain_set_record_route(ctx->c.domain, 1);

      ctx->proxy_tests = 1;
    }


    if (print_headings)
      printf("TEST NUA-2.1.1: PASSED\n");
  }

  p_uri = a_uri = b_uri = test_proxy_uri(ctx->p);

  if (o_proxy) {
    TEST_1(e_proxy = url_hdup(ctx->home, (void *)o_proxy));
    ctx->external_proxy = e_proxy;
  }

  if (start_nat && p_uri == NULL)
    p_uri = e_proxy;

  if (ctx->p)
    initial_route = test_proxy_route_uri(ctx->p, &ctx->lr);

  if (start_nat && p_uri != NULL) {
    int family = 0;
    su_sockaddr_t su[1];
    socklen_t sulen = sizeof su;
    char b[64];
    size_t len;
    ta_list ta;

    if (print_headings)
      printf("TEST NUA-2.1.2: creating test NAT\n");

    /* Try to use different family than proxy. */
    if (p_uri->url_host[0] == '[')
      family = AF_INET;
#if defined(SU_HAVE_IN6)
    else
      family = AF_INET6;
#endif

    ta_start(ta, tag, value);
    ctx->nat = test_nat_create(ctx->root, family, ta_tags(ta));
    ta_end(ta);

    /*
     * NAT thingy works so that we set the outgoing proxy URI to point
     * towards its "private" address and give the real address of the proxy
     * as its "public" address. If we use different IP families here, we may
     * even manage to test real connectivity problems as proxy and endpoint
     * can not talk to each other.
     */

    if (test_nat_private(ctx->nat, su, &sulen) < 0) {
      printf("%s:%u: NUA-2.1.2: failed to get private NAT address\n",
	     __FILE__, __LINE__);
    }

#if defined(SU_HAVE_IN6)
    else if (su->su_family == AF_INET6) {
      a_uri = (void *)
	su_sprintf(ctx->home, "sip:[%s]:%u",
		   su_inet_ntop(su->su_family, SU_ADDR(su), b, sizeof b),
		   ntohs(su->su_port));
      a_bind = "sip:[::]:*";
    }
#endif
    else if (su->su_family == AF_INET) {
      a_uri = (void *)
	su_sprintf(ctx->home, "sip:%s:%u",
		   su_inet_ntop(su->su_family, SU_ADDR(su), b, sizeof b),
		   ntohs(su->su_port));
    }

#if defined(SU_HAVE_IN6)
    if (p_uri->url_host[0] == '[') {
      su->su_len = sulen = (sizeof su->su_sin6), su->su_family = AF_INET6;
      len = strcspn(p_uri->url_host + 1, "]"); assert(len < sizeof b);
      memcpy(b, p_uri->url_host + 1, len); b[len] = '\0';
      su_inet_pton(su->su_family, b, SU_ADDR(su));
    }
    else {
      su->su_len = sulen = (sizeof su->su_sin), su->su_family = AF_INET;
      su_inet_pton(su->su_family, p_uri->url_host, SU_ADDR(su));
    }
#else
    su->su_len = sulen = (sizeof su->su_sin), su->su_family = AF_INET;
    su_inet_pton(su->su_family, p_uri->url_host, SU_ADDR(su));
#endif

    su->su_port = htons(strtoul(url_port(p_uri), NULL, 10));

    if (test_nat_public(ctx->nat, su, sulen) < 0) {
      printf("%s:%u: NUA-2.1.2: failed to set public address\n",
	     __FILE__, __LINE__);
      a_uri = NULL;
    }

    if (print_headings) {
      if (ctx->nat && a_uri) {
	printf("TEST NUA-2.1.2: PASSED\n");
      } else {
	printf("TEST NUA-2.1.2: FAILED\n");
      }
    }
  }

  if (print_headings)
    printf("TEST NUA-2.2.1: init endpoint A\n");

  if (a_uri == NULL)
    a_uri = p_uri;

  ctx->a.instance = nua_generate_instance_identifier(ctx->home);

  ctx->a.nua = nua_create(ctx->root, a_callback, ctx,
			  NUTAG_PROXY(a_uri ? a_uri : e_proxy),
			  NUTAG_INITIAL_ROUTE_STR(initial_route),
			  SIPTAG_FROM_STR("sip:alice@example.com"),
			  NUTAG_URL(a_bind),
			  TAG_IF(a_bind != a_bind2, NUTAG_SIPS_URL(a_bind2)),
			  SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
			  NTATAG_SIP_T1X64(2000),
			  NUTAG_INSTANCE(ctx->a.instance),
			  TAG_IF(ctx->a.logging, TPTAG_LOG(1)),
			  TAG_END());
  TEST_1(ctx->a.nua);

  nua_get_params(ctx->a.nua, TAG_ANY(), TAG_END());
  run_a_until(ctx, nua_r_get_params, save_until_final_response);
  TEST_1(e = ctx->a.specials->head);
  err = tl_gets(e->data->e_tags,
	            NTATAG_CONTACT_REF(m),
	            SIPTAG_FROM_REF(sipaddress),
	            SIPTAG_ALLOW_REF(allow),
	            NUTAG_APPL_METHOD_REF(appl_method),
	            SIPTAG_SUPPORTED_REF(supported),
	            TAG_END());
  TEST(err, 5);
  TEST_1(m);
  TEST_1(ctx->a.contact = sip_contact_dup(ctx->home, m));
  TEST_1(ctx->a.to = sip_to_dup(ctx->home, sipaddress));
  TEST_1(ctx->a.allow = sip_allow_dup(ctx->home, allow));
  TEST_1(ctx->a.appl_method = su_strdup(ctx->home, appl_method));
  TEST_1(ctx->a.supported = sip_supported_dup(ctx->home, supported));

  free_events_in_list(ctx, ctx->a.specials);

  if (print_headings)
    printf("TEST NUA-2.2.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.2.2: init endpoint B\n");

  ctx->b.instance = nua_generate_instance_identifier(ctx->home);

  if (ctx->p) {
    /* B uses TCP when talking with proxy */
    *b_proxy = *b_uri;
    b_uri = b_proxy;
    b_proxy->url_params = "transport=tcp";
  }

  ctx->b.nua = nua_create(ctx->root, b_callback, ctx,
			  NUTAG_PROXY(b_uri ? b_uri : e_proxy),
			  SIPTAG_FROM_STR("sip:bob@example.org"),
			  NUTAG_URL("sip:0.0.0.0:*"),
			  SOATAG_USER_SDP_STR("m=audio 5006 RTP/AVP 8 0"),
			  NUTAG_INSTANCE(ctx->b.instance),
			  /* Quicker timeout */
			  NTATAG_SIP_T1X64(2000),
			  TPTAG_KEEPALIVE(100),
			  TAG_IF(ctx->b.logging, TPTAG_LOG(1)),
			  TAG_END());
  TEST_1(ctx->b.nua);

  nua_get_params(ctx->b.nua, TAG_ANY(), TAG_END());
  run_b_until(ctx, nua_r_get_params, save_until_final_response);
  TEST_1(e = ctx->b.specials->head);
  err = tl_gets(e->data->e_tags,
	            NTATAG_CONTACT_REF(m),
	            SIPTAG_FROM_REF(sipaddress),
	            SIPTAG_ALLOW_REF(allow),
	            NUTAG_APPL_METHOD_REF(appl_method),
	            SIPTAG_SUPPORTED_REF(supported),
	            TAG_END());
  TEST(err, 5); TEST_1(m);

  TEST_1(ctx->b.contact = sip_contact_dup(ctx->home, m));
  TEST_1(ctx->b.to = sip_to_dup(ctx->home, sipaddress));
  TEST_1(ctx->b.allow = sip_allow_dup(ctx->home, allow));
  TEST_1(ctx->b.appl_method = su_strdup(ctx->home, appl_method));
  TEST_1(ctx->b.supported = sip_supported_dup(ctx->home, supported));

  free_events_in_list(ctx, ctx->b.specials);

  if (print_headings)
    printf("TEST NUA-2.2.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.2.3: init endpoint C\n");

  /* ctx->c.instance = nua_generate_instance_identifier(ctx->home); */

  ctx->c.to = sip_from_make(ctx->home, "Charlie <sip:charlie@example.net>");

  ctx->c.nua = nua_create(ctx->root, c_callback, ctx,
			  NUTAG_PROXY(p_uri ? p_uri : e_proxy),
			  NUTAG_URL("sip:0.0.0.0:*"),
			  SOATAG_USER_SDP_STR("m=audio 5400 RTP/AVP 8 0"),
			  NUTAG_INSTANCE(ctx->c.instance),
			  TAG_IF(ctx->c.logging, TPTAG_LOG(1)),
			  TAG_END());
  TEST_1(ctx->c.nua);

  nua_get_params(ctx->c.nua, TAG_ANY(), TAG_END());
  run_c_until(ctx, nua_r_get_params, save_until_final_response);
  TEST_1(e = ctx->c.specials->head);
  err = tl_gets(e->data->e_tags,
	            NTATAG_CONTACT_REF(m),
	            SIPTAG_ALLOW_REF(allow),
	            NUTAG_APPL_METHOD_REF(appl_method),
	            SIPTAG_SUPPORTED_REF(supported),
	            TAG_END());

  TEST(err, 4); TEST_1(m);
  TEST_1(ctx->c.contact = sip_contact_dup(ctx->home, m));
  TEST_1(ctx->c.allow = sip_allow_dup(ctx->home, allow));
  TEST_1(ctx->c.appl_method = su_strdup(ctx->home, appl_method));
  TEST_1(ctx->c.supported = sip_supported_dup(ctx->home, supported));
  free_events_in_list(ctx, ctx->c.specials);

  if (print_headings)
    printf("TEST NUA-2.2.3: PASSED\n");

  END();
}


/* ====================================================================== */

int test_deinit(struct context *ctx)
{
  BEGIN();

  struct call *call;

  if (!ctx->threading)
    su_root_step(ctx->root, 100);

  if (ctx->a.nua) {
    for (call = ctx->a.call; call; call = call->next)
      nua_handle_destroy(call->nh), call->nh = NULL;

    nua_shutdown(ctx->a.nua);
    run_a_until(ctx, nua_r_shutdown, until_final_response);
    free_events_in_list(ctx, ctx->a.events);
    free_events_in_list(ctx, ctx->a.specials);
    nua_destroy(ctx->a.nua), ctx->a.nua = NULL;
  }

  if (ctx->b.nua) {
    for (call = ctx->b.call; call; call = call->next)
      nua_handle_destroy(call->nh), call->nh = NULL;

    nua_shutdown(ctx->b.nua);
    run_b_until(ctx, nua_r_shutdown, until_final_response);
    free_events_in_list(ctx, ctx->b.events);
    free_events_in_list(ctx, ctx->b.specials);
    nua_destroy(ctx->b.nua), ctx->b.nua = NULL;
  }

  if (ctx->c.nua) {
    for (call = ctx->c.call; call; call = call->next)
      nua_handle_destroy(call->nh), call->nh = NULL;

    nua_shutdown(ctx->c.nua);
    run_c_until(ctx, nua_r_shutdown, until_final_response);
    free_events_in_list(ctx, ctx->c.events);
    free_events_in_list(ctx, ctx->c.specials);
    nua_destroy(ctx->c.nua), ctx->c.nua = NULL;
  }

  test_proxy_destroy(ctx->p), ctx->p = NULL;

  test_nat_destroy(ctx->nat), ctx->nat = NULL;

  su_root_destroy(ctx->root), ctx->root = NULL;

  END();
}
