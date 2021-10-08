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

/**@CFILE test_register.c
 * @brief Test registering, outbound, nat traversal.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"
#include <sofia-sip/su_tag_class.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_register"
#endif

/* ======================================================================== */
/* Test REGISTER */

int test_clear_registrations(struct context *ctx);
int test_outbound_cases(struct context *ctx);
int test_register_a(struct context *ctx);
int test_register_b(struct context *ctx);
int test_register_c(struct context *ctx);
int test_register_refresh(struct context *ctx);

int test_register_to_proxy(struct context *ctx)
{
  return
    test_clear_registrations(ctx) ||
    test_outbound_cases(ctx) ||
    test_register_a(ctx) ||
    test_register_b(ctx) ||
    test_register_c(ctx) ||
    test_register_refresh(ctx);
}

int test_clear_registrations(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct call *a_reg = a->reg, *b_reg = b->reg, *c_reg = c->reg;

  if (print_headings)
    printf("TEST NUA-2.3.0.1: un-REGISTER a\n");

  TEST_1(a_reg->nh = nua_handle(a->nua, a_reg, TAG_END()));
  UNREGISTER(a, a_reg, a_reg->nh, SIPTAG_TO(a->to),
	     SIPTAG_CONTACT_STR("*"),
	     TAG_END());
  run_a_until(ctx, -1, until_final_response);
  AUTHENTICATE(a, a_reg, a_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":alice:secret"), TAG_END());
  run_a_until(ctx, -1, until_final_response);
  nua_handle_destroy(a_reg->nh);

  if (print_headings)
    printf("TEST NUA-2.3.0.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.3.0.2: un-REGISTER b\n");

  TEST_1(b_reg->nh = nua_handle(b->nua, b_reg, TAG_END()));
  UNREGISTER(b, b_reg, b_reg->nh, SIPTAG_TO(b->to),
	     SIPTAG_CONTACT_STR("*"),
	     TAG_END());
  run_b_until(ctx, -1, until_final_response);
  AUTHENTICATE(b, b_reg, b_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":bob:secret"), TAG_END());
  run_b_until(ctx, -1, until_final_response);
  nua_handle_destroy(b_reg->nh);

  if (print_headings)
    printf("TEST NUA-2.3.0.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.3.0.3: un-REGISTER c\n");

  TEST_1(c_reg->nh = nua_handle(c->nua, c_reg, TAG_END()));
  UNREGISTER(c, c_reg, c_reg->nh,
	     SIPTAG_FROM(c->to), SIPTAG_TO(c->to),
	     SIPTAG_CONTACT_STR("*"),
	     TAG_END());
  run_c_until(ctx, -1, until_final_response);
  AUTHENTICATE(c, c_reg, c_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"), TAG_END());
  run_c_until(ctx, -1, until_final_response);
  nua_handle_destroy(c_reg->nh);

  if (print_headings)
    printf("TEST NUA-2.3.0.3: PASSED\n");

  END();
}

int test_outbound_cases(struct context *ctx)
{
  BEGIN();

#if 0

  struct endpoint *a = &ctx->a, *x;
  struct call *a_reg = a->reg;
  struct event *e;
  sip_t const *sip;
  sip_contact_t m[1];

/* REGISTER test

   A			R
   |------REGISTER----->|
   |<-------401---------|
   |------REGISTER----->|
   |<-------200---------|
   |			|

*/

  if (print_headings)
    printf("TEST NUA-2.3.1: REGISTER a\n");

  test_proxy_domain_set_expiration(ctx->a.domain, 5, 5, 10);

  TEST_1(a_reg->nh = nua_handle(a->nua, a_reg, TAG_END()));

  sip_contact_init(m);
  m->m_display = "Lissu";
  *m->m_url = *a->contact->m_url;
  m->m_url->url_user = "a";
  m->m_url->url_params = "transport=udp";

  REGISTER(a, a_reg, a_reg->nh, SIPTAG_TO(a->to),
	   NUTAG_OUTBOUND("use-rport no-options-keepalive"),
	   SIPTAG_CONTACT(m),
	   TAG_END());
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 401);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 401);
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  AUTHENTICATE(a, a_reg, a_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":alice:secret"), TAG_END());
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Lissu");
  TEST_S(sip->sip_contact->m_url->url_user, "a");
  TEST_1(strstr(sip->sip_contact->m_url->url_params, "transport=udp"));

  if (ctx->nat) {
    TEST_1(e = a->specials->head);
  }

  test_proxy_domain_set_expiration(ctx->a.domain, 600, 3600, 36000);

  if (print_headings)
    printf("TEST NUA-2.3.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.3.4: refresh REGISTER\n");

  if (!ctx->p) {
    free_events_in_list(ctx, a->events);
    return 0;
  }

  /* Wait for A to refresh its registrations */

  /*
   * Avoid race condition: if X has already refreshed registration
   * with expiration time of 3600 seconds, do not wait for new refresh
   */
  a->next_condition = save_until_final_response;

  for (x = a; x; x = NULL) {
    for (e = x->events->head; e; e = e->next) {
      if (e->data->e_event == nua_r_register &&
	  e->data->e_status == 200 &&
	  (sip = sip_object(e->data->e_msg)) &&
	  sip->sip_contact &&
	  sip->sip_contact->m_expires &&
	  strcmp(sip->sip_contact->m_expires, "3600") == 0) {
	x->next_condition = NULL;
	break;
      }
    }
  }

  run_a_until(ctx, -1, a->next_condition);

  for (e = a->events->head; e; e = e->next) {
    TEST_E(e->data->e_event, nua_r_register);
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_contact);
    if (!e->next)
      break;
  }
  TEST_1(e);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_S(sip->sip_contact->m_expires, "3600");
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-2.3.4: PASSED\n");

  TEST_1(0);

#endif

  END();
}

int test_register_a(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a;
  struct call *a_reg = a->reg;
  struct event *e;
  sip_t const *sip;
  sip_cseq_t cseq[1];

/* REGISTER test

   A			R
   |------REGISTER----->|
   |<-------401---------|
   |------REGISTER----->|
   |<-------200---------|
   |			|

*/

  if (print_headings)
    printf("TEST NUA-2.3.1: REGISTER a\n");

  test_proxy_domain_set_expiration(ctx->a.domain, 5, 5, 10);

  TEST_1(a_reg->nh = nua_handle(a->nua, a_reg, TAG_END()));

  sip_cseq_init(cseq)->cs_seq = 12;
  cseq->cs_method = sip_method_register;
  cseq->cs_method_name = sip_method_name_register;

  REGISTER(a, a_reg, a_reg->nh, SIPTAG_TO(a->to),
	   NUTAG_OUTBOUND("natify options-keepalive validate"),
	   NUTAG_KEEPALIVE(1000),
	   NUTAG_M_DISPLAY("A&A"),
	   NUTAG_M_USERNAME("a"),
	   NUTAG_M_PARAMS("foo=bar"),
	   NUTAG_M_FEATURES("q=0.9"),
	   SIPTAG_CSEQ(cseq),
	   TAG_END());
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head);
  TEST_1(sip = sip_object(e->data->e_msg));
  if (ctx->nat && e->data->e_status == 100) {
    TEST_E(e->data->e_event, nua_r_register);
    TEST(e->data->e_status, 100);
    TEST(sip->sip_status->st_status, 406);
    /* Check that CSeq included in tags is actually used in the request */
    TEST(sip->sip_cseq->cs_seq, 13);
    TEST_1(!sip->sip_contact);
    TEST_1(e = e->next);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST(sip->sip_cseq->cs_seq, 14);
  }
  else {
    /* Check that CSeq included in tags is actually used in the request */
    TEST(sip->sip_cseq->cs_seq, 13);
  }
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 401);
  TEST(sip->sip_status->st_status, 401);
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  AUTHENTICATE(a, a_reg, a_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":alice:secret"), TAG_END());
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  { char const *expect_m_display = "\"A&A\"";
    /* VC does not dig \" with TEST_S() */
  TEST_S(sip->sip_contact->m_display, expect_m_display); }
  TEST_S(sip->sip_contact->m_url->url_user, "a");
  TEST_1(strstr(sip->sip_contact->m_url->url_params, "foo=bar"));
  TEST_S(sip->sip_contact->m_q, "0.9");

  if (ctx->nat) {
    TEST_1(e = a->specials->head);
  }

  test_proxy_domain_set_expiration(ctx->a.domain, 600, 3600, 36000);

  if (print_headings)
    printf("TEST NUA-2.3.1: PASSED\n");

  END();
}

int test_register_b(struct context *ctx)
{
  BEGIN();

  struct endpoint  *b = &ctx->b;
  struct call *b_reg = b->reg;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-2.3.2: REGISTER b\n");

  test_proxy_domain_set_expiration(ctx->b.domain, 5, 5, 10);

  TEST_1(b_reg->nh = nua_handle(b->nua, b_reg, TAG_END()));

  /* Test application-supplied contact */
  {
    sip_contact_t m[1];
    sip_contact_init(m)->m_url[0] = b->contact->m_url[0];

    m->m_display = "B";
    m->m_url->url_user = "b";

    /* Include "tcp" transport parameter in Contact */
    if (ctx->p)
      m->m_url->url_params = "transport=tcp";

    REGISTER(b, b_reg, b_reg->nh, SIPTAG_TO(b->to),
	     SIPTAG_CONTACT(m),
	     /* Do not include credentials unless challenged */
	     NUTAG_AUTH_CACHE(nua_auth_cache_challenged),
	     TAG_END());
  }
  run_ab_until(ctx, -1, save_events, -1, save_until_final_response);

  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 401);
  TEST(sip->sip_status->st_status, 401);
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  AUTHENTICATE(b, b_reg, b_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":bob:secret"), TAG_END());
  run_ab_until(ctx, -1, save_events, -1, save_until_final_response);

  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "B");
  TEST_S(sip->sip_contact->m_url->url_user, "b");
  free_events_in_list(ctx, b->events);

  test_proxy_domain_set_expiration(ctx->b.domain, 600, 3600, 36000);

  if (print_headings)
    printf("TEST NUA-2.3.2: PASSED\n");

  END();
}

int test_register_c(struct context *ctx)
{
  BEGIN();

  struct endpoint *c = &ctx->c;
  struct call *c_reg = c->reg;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-2.3.3: REGISTER c\n");

  test_proxy_domain_set_expiration(ctx->c.domain, 600, 3600, 36000);
  test_proxy_domain_set_authorize(ctx->c.domain, "test-proxy-0");

  TEST_1(c_reg->nh = nua_handle(c->nua, c_reg, TAG_END()));

  REGISTER(c, c_reg, c_reg->nh, SIPTAG_TO(c->to),
	   SIPTAG_FROM(c->to),
	   NUTAG_OUTBOUND(NULL),
	   NUTAG_M_DISPLAY("C"),
	   NUTAG_M_USERNAME("c"),
	   NUTAG_M_PARAMS("c=1"),
	   NUTAG_M_FEATURES("q=0.987;expires=5"),
	   NUTAG_CALLEE_CAPS(1),
	   SIPTAG_EXPIRES_STR("5"), /* Test 423 negotiation */
	   TAG_END());
  run_abc_until(ctx, -1, save_events, -1, save_events,
		-1, save_until_final_response);

  TEST_1(e = c->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 401);
  TEST(sip->sip_status->st_status, 401);
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  AUTHENTICATE(c, c_reg, c_reg->nh,
	       NUTAG_AUTH("Digest:\"test-proxy-0\":charlie:secret"), TAG_END());
  run_abc_until(ctx, -1, save_events, -1, save_events,
		-1, save_until_final_response);

  TEST_1(e = c->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 423);
  TEST_1(e = e->next);
  if (e->data->e_status == 100 && e->data->e_event == nua_r_register) {
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST(sip->sip_status->st_status, 401);
    TEST_1(e = e->next);
  }
  TEST(e->data->e_status, 200); TEST_E(e->data->e_event, nua_r_register);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "C");
  TEST_S(sip->sip_contact->m_url->url_user, "c");
  TEST_1(strstr(sip->sip_contact->m_url->url_params, "c=1"));
  TEST_S(sip->sip_contact->m_q, "0.987");
  TEST_1(msg_header_find_param(sip->sip_contact->m_common, "methods="));
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  if (print_headings)
    printf("TEST NUA-2.3.3: PASSED\n");

  END();
}

int test_register_refresh(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *x;
  struct event *e;
  sip_t const *sip;
  int seen_401;

  if (print_headings)
    printf("TEST NUA-2.3.4: refresh REGISTER\n");

  if (!ctx->p) {
    free_events_in_list(ctx, a->events);
    free_events_in_list(ctx, b->events);
    return 0;
  }

  /* Wait for A and B to refresh their registrations */

  /*
   * Avoid race condition: if X has already refreshed registration
   * with expiration time of 3600 seconds, do not wait for new refresh
   */
  a->next_condition = save_until_final_response;
  b->next_condition = save_until_final_response;

  for (x = a; x; x = x == a ? b : NULL) {
    for (e = x->events->head; e; e = e->next) {
      if (e->data->e_event == nua_r_register &&
	  e->data->e_status == 200 &&
	  (sip = sip_object(e->data->e_msg)) &&
	  sip->sip_contact &&
	  sip->sip_contact->m_expires &&
	  strcmp(sip->sip_contact->m_expires, "3600") == 0) {
	x->next_condition = NULL;
	break;
      }
    }
  }

  run_ab_until(ctx, -1, a->next_condition, -1, b->next_condition);

  for (e = a->events->head; e; e = e->next) {
    TEST_E(e->data->e_event, nua_r_register);
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_contact);
    if (!e->next)
      break;
  }
  TEST_1(e);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_S(sip->sip_contact->m_expires, "3600");
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  seen_401 = 0;

  for (e = b->events->head; e; e = e->next) {
    TEST_E(e->data->e_event, nua_r_register);
    TEST_1(sip = sip_object(e->data->e_msg));

    if (e->data->e_status == 200) {
      TEST(e->data->e_status, 200);
      TEST_1(seen_401);
      TEST_1(sip->sip_contact);
    }
    else if (sip->sip_status && sip->sip_status->st_status == 401) {
      seen_401 = 1;
    }

    if (!e->next)
      break;
  }
  TEST_1(e);
  TEST_S(sip->sip_contact->m_expires, "3600");
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-2.3.4: PASSED\n");

  if (!ctx->p)
    return 0;

  if (print_headings)
    printf("TEST NUA-2.3.5: re-REGISTER when TCP connection is closed\n");

  test_proxy_close_tports(ctx->p);

  run_b_until(ctx, -1, save_until_final_response);

  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  if (e->data->e_status == 100)
    TEST_1(e = e->next);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_expires, "3600");
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-2.3.5: PASSED\n");

  END();
}

int registrar_299(CONDITION_PARAMS)
{
  msg_t *request = nua_current_request(nua);

  save_event_in_list(ctx, event, ep, ep->call);

  if (event == nua_i_register) {
    RESPOND(ep, call, nh, 299, "YES", NUTAG_WITH(request), TAG_END());
    return 1;
  }

  return 0;
}

int test_register_to_c(struct context *ctx)
{
  BEGIN();

  struct endpoint *b = &ctx->b, *c = &ctx->c;
  struct call *b_call = b->call, *c_call = c->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-2.6.1: REGISTER b to c\n");

  nua_set_params(ctx->c.nua,
		 NUTAG_ALLOW("REGISTER"),
		 TAG_END());
  run_c_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(b_call->nh = nua_handle(b->nua, b_call, TAG_END()));

  REGISTER(b, b_call, b_call->nh,
	   NUTAG_REGISTRAR((url_string_t *)c->contact->m_url),
	   SIPTAG_TO(b->to),
	   NUTAG_OUTBOUND(NULL),
	   SIPTAG_CONTACT_STR(NULL),
	   TAG_END());
  run_bc_until(ctx, -1, save_until_final_response, -1, registrar_299);

  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 299);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(!sip->sip_contact);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  TEST_1(e = c->events->head);
  TEST_E(e->data->e_event, nua_i_register);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(!sip->sip_contact);

  free_events_in_list(ctx, c->events);
  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-2.6.1: PASSED\n");

  END();
}


int test_register(struct context *ctx)
{
  if (test_register_to_c(ctx))
    return 1;

  if (ctx->proxy_tests)
    if (test_register_to_proxy(ctx)) return 1;

  return 0;
}


int test_connectivity(struct context *ctx)
{
  if (!ctx->proxy_tests)
    return 0;			/* No proxy */

  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct call *a_call = a->call, *b_call = b->call, *c_call = c->call;
  struct event *e;
  sip_t const *sip;

  /* Connectivity test using OPTIONS */

  if (print_headings)
    printf("TEST NUA-2.4.1: OPTIONS from A to B\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  OPTIONS(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  NUTAG_ALLOW("OPTIONS"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, save_until_received);

  /* Client events: nua_options(), nua_r_options */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_options);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_allow); TEST_1(sip->sip_accept); TEST_1(sip->sip_supported);
  /* TEST_1(sip->sip_content_type); */
  /* TEST_1(sip->sip_payload); */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /* Server events: nua_i_options */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_options);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-2.4.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.4.2: OPTIONS from B to C\n");

  TEST_1(b_call->nh = nua_handle(b->nua, b_call, SIPTAG_TO(c->to), TAG_END()));

  OPTIONS(b, b_call, b_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(c->contact->m_url)),
	  TAG_END());

  run_abc_until(ctx, -1, NULL,
		-1, save_until_final_response,
		-1, save_until_received);

  /* Client events: nua_options(), nua_r_options */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_options);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_allow); TEST_1(sip->sip_accept); TEST_1(sip->sip_supported);
  /* TEST_1(sip->sip_content_type); */
  /* TEST_1(sip->sip_payload); */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  /* Server events: nua_i_options */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_options);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, c->events);
  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-2.4.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.4.3: OPTIONS from C to A\n");

  TEST_1(c_call->nh = nua_handle(c->nua, c_call, SIPTAG_TO(a->to), TAG_END()));

  OPTIONS(c, c_call, c_call->nh,
	  SIPTAG_FROM(c->to),
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(a->contact->m_url)),
	  TAG_END());

  if (ctx->proxy_tests) {
    run_abc_until(ctx, -1, NULL, -1, NULL, -1, save_until_final_response);

    /* Client events: nua_options(), nua_r_options */
    TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_r_options);
    TEST(e->data->e_status, 407);
    TEST_1(!e->next);

    free_events_in_list(ctx, c->events);

    /* Sneakily change the realm */

    TEST(test_proxy_domain_set_authorize(ctx->c.domain, "test-proxy"), 0);

    AUTHENTICATE(c, c_call, c_call->nh,
		 NUTAG_AUTH("Digest:\"test-proxy-0\":charlie:secret"),
		 TAG_END());

    run_abc_until(ctx, -1, NULL, -1, NULL, -1, save_until_final_response);

    /* Client events: nua_options(), nua_r_options */
    TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_r_options);
    TEST(e->data->e_status, 407);
    TEST_1(!e->next);

    free_events_in_list(ctx, c->events);

    AUTHENTICATE(c, c_call, c_call->nh,
		 NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"),
		 TAG_END());
  }

  run_abc_until(ctx, -1, save_until_received,
		-1, NULL,
		-1, save_until_final_response);

  /* Client events: nua_options(), nua_r_options */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_r_options);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_allow); TEST_1(sip->sip_accept); TEST_1(sip->sip_supported);
  /* TEST_1(sip->sip_content_type); */
  /* TEST_1(sip->sip_payload); */
  TEST_1(!e->next);

  free_events_in_list(ctx, c->events);
  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  /* Server events: nua_i_options */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_options);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-2.4.3: PASSED\n");

  END();
}

int test_nat_timeout(struct context *ctx)
{
  if (!ctx->proxy_tests || !ctx->nat)
    return 0;			/* No proxy */

  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  /* Test what happens when NAT bindings go away */

  if (print_headings)
    printf("TEST NUA-2.5.1: NAT binding change\n");

  free_events_in_list(ctx, a->specials);

  test_nat_flush(ctx->nat);	/* Break our connections */

  /* Run until we get final response to REGISTER */
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->specials->head);
  TEST_E(e->data->e_event, nua_i_outbound);
  TEST(e->data->e_status, 102);
  TEST_S(e->data->e_phrase, "NAT binding changed");
  TEST_1(!e->next);

  free_events_in_list(ctx, a->specials);

  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-2.5.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-2.5.2: OPTIONS from B to A\n");

  TEST_1(b_call->nh = nua_handle(b->nua, b_call, SIPTAG_TO(a->to), TAG_END()));

  OPTIONS(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_received,
	       -1, save_until_final_response);

  /* Client events: nua_options(), nua_r_options */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_options);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_allow); TEST_1(sip->sip_accept); TEST_1(sip->sip_supported);
  /* TEST_1(sip->sip_content_type); */
  /* TEST_1(sip->sip_payload); */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  /* Server events: nua_i_options */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_options);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-2.5.2: PASSED\n");

  END();
}

int test_unregister(struct context *ctx)
{
  if (!ctx->proxy_tests)
    return 0;			/* No proxy */

  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct event *e;
  sip_t const *sip;

/* un-REGISTER test

   A			B
   |----un-REGISTER---->|
   |<-------200---------|
   |			|

*/
  if (print_headings)
    printf("TEST NUA-13.1: un-REGISTER a\n");

  if (a->reg->nh) {
    free_events_in_list(ctx, a->events);
    UNREGISTER(a, NULL, a->reg->nh, TAG_END());
    run_a_until(ctx, -1, save_until_final_response);
    TEST_1(e = a->events->head);
    TEST_E(e->data->e_event, nua_r_unregister);
    if (e->data->e_status == 100) {
      TEST_1(e = e->next);
      TEST_E(e->data->e_event, nua_r_unregister);
    }
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(!sip->sip_contact);
    TEST_1(!e->next);
    free_events_in_list(ctx, a->events);
    nua_handle_destroy(a->reg->nh), a->reg->nh = NULL;
  }

  if (print_headings)
    printf("TEST NUA-13.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-13.2: un-REGISTER b\n");

  if (b->reg->nh) {
    free_events_in_list(ctx, b->events);
    UNREGISTER(b, NULL, b->reg->nh, TAG_END());
    run_b_until(ctx, -1, save_until_final_response);
    TEST_1(e = b->events->head);
    TEST_E(e->data->e_event, nua_r_unregister);
    if (e->data->e_status == 100) {
      TEST_1(e = e->next);
      TEST_E(e->data->e_event, nua_r_unregister);
    }
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(!sip->sip_contact);
    TEST_1(!e->next);
    free_events_in_list(ctx, b->events);
    nua_handle_destroy(b->reg->nh), b->reg->nh = NULL;
  }
  if (print_headings)
    printf("TEST NUA-13.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-13.3: un-REGISTER c\n");

  /* Unregister using another handle */
  free_events_in_list(ctx, c->events);
  TEST_1(c->call->nh = nua_handle(c->nua, c->call, TAG_END()));
  UNREGISTER(c, c->call, c->call->nh, SIPTAG_TO(c->to), SIPTAG_FROM(c->to),
	     NUTAG_M_DISPLAY("C"),
	     NUTAG_M_USERNAME("c"),
	     NUTAG_M_PARAMS("c=1"),
	     TAG_END());
  run_c_until(ctx, -1, save_until_final_response);

  TEST_1(e = c->events->head);
  TEST_E(e->data->e_event, nua_r_unregister);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 401);
  TEST(sip->sip_status->st_status, 401);
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  AUTHENTICATE(c, c->call, c->call->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"), TAG_END());
  run_c_until(ctx, -1, save_until_final_response);

  TEST_1(e = c->events->head);
  TEST_E(e->data->e_event, nua_r_unregister);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(!sip->sip_contact);
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);
  nua_handle_destroy(c->call->nh), c->call->nh = NULL;

  if (c->reg->nh) {
    UNREGISTER(c, NULL, c->reg->nh, TAG_END());
    run_c_until(ctx, -1, save_until_final_response);
    TEST_1(e = c->events->head);
    TEST_E(e->data->e_event, nua_r_unregister);
    if (e->data->e_status == 100) {
      TEST_1(e = e->next);
      TEST_E(e->data->e_event, nua_r_unregister);
    }
    if (e->data->e_status == 401) {
      TEST_1(!e->next);
      free_events_in_list(ctx, c->events);
      AUTHENTICATE(c, NULL, c->reg->nh,
		   NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"), TAG_END());
      run_c_until(ctx, -1, save_until_final_response);
      TEST_1(e = c->events->head);
      TEST_E(e->data->e_event, nua_r_unregister);
    }
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_from->a_url->url_user);
    TEST_1(!sip->sip_contact);
    TEST_1(!e->next);
    free_events_in_list(ctx, c->events);
    nua_handle_destroy(c->reg->nh), c->reg->nh = NULL;
  }

  if (print_headings)
    printf("TEST NUA-13.3: PASSED\n");

  END();
}
