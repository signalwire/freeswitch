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

/**@CFILE test_nua_100rel.c
 * @brief NUA-10 tests: early session, PRACK, UPDATE, precondition.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"
#include <sofia-sip/auth_common.h>
#include <sofia-sip/su_tag_class.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_call_hold"
#endif

/* ======================================================================== */

/*
 X  accept_pracked    ep
 |-------INVITE------>|
 |        (sdp)       |
 |                    |
 |<----100 Trying-----|
 |                    |
 |<-------180---------|
 |       (sdp)        |
 |-------PRACK------->|
 |<-------200---------|
 |                    |
 |<------200 OK-------|
 |--------ACK-------->|
 |                    |
*/
int accept_pracked(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_prack:
    if (200 <= status && status < 300) {
      RESPOND(ep, call, nh, SIP_200_OK, TAG_END());
      ep->next_condition = until_ready;
    }
  default:
    break;
  }

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int accept_pracked2(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    RESPOND(ep, call, nh, SIP_200_OK,
	    NUTAG_INCLUDE_EXTRA_SDP(1),
	    TAG_END());
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}


int test_180rel(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ep, *ei;
  sip_t *sip;

  if (print_headings)
    printf("TEST NUA-10.1.1: Call with 100rel and 180\n");

/* Test for 100rel:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_SESSION_TIMER(180),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_pracked2);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/sdp");
  TEST_1(sip->sip_payload);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Respond with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.1.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.1.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.1.2: PASSED\n");

  END();
}

/*
 X      INVITE
 |                    |
 |-------INVITE------>|
 |<--------200--------|
 |---------ACK------->|
*/
int authenticate_until_ready(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (status == 401 || status == 407) {
    AUTHENTICATE(ep, call, nh,
		 NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"),
		 TAG_END());
  }

  switch (callstate(tags)) {
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

/** Test authentication for PRACK */
int test_prack_auth(struct context *ctx)
{
  if (!ctx->proxy_tests)
    return 0;

  BEGIN();

  struct endpoint *c = &ctx->c,  *b = &ctx->b;
  struct call *c_call = c->call, *b_call = b->call;
  struct event *e, *ep, *ei;
  sip_t *sip;
  sip_proxy_authenticate_t *au;

  if (print_headings)
    printf("TEST NUA-10.1.3: Call with 100rel, PRACK is challenged\n");

/* Test for authentication during 100rel

   C			B
   |-------INVITE--\    |
   |<-------407----/    |
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK---\    |
   |<-------407----/    |
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

*/

  c_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->c.nua,
		 NUTAG_EARLY_MEDIA(1),
		 TAG_END());
  run_c_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(c_call->nh = nua_handle(c->nua, c_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(c, c_call, c_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_FROM(c->to),
	 SOATAG_USER_SDP_STR(c_call->sdp),
	 TAG_END());

  run_bc_until(ctx, -1, accept_pracked, -1, authenticate_until_ready);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 407);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(au = sip->sip_proxy_authenticate);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  if (e->data->e_status == 100 || e->data->e_status == 407) {
    /* The final response to PRACK may be received after ACK is sent */
    if (!event_by_type(e->next, nua_r_prack))
      run_bc_until(ctx, -1, save_events, -1, save_until_final_response);
    TEST_1(e = ep = event_by_type(e->next, nua_r_prack));
  }
  TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_from->a_url->url_user);

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, c->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Respond with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.1.3: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.1.4: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_bc_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* C: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  nua_handle_destroy(c_call->nh), c_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.1.4: PASSED\n");

  END();
}

/*
 X  ringing_pracked    ep
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<-------183---------|
 |-------PRACK------->|
 |<-------200---------|
 |                    |
 |<-------180---------|
 |-------PRACK------->|
 |<-------200---------|
 |                    |
 |<------200 OK-------|
 |--------ACK-------->|
 |                    |
*/
int ringing_pracked(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_prack:
    if (200 <= status && status < 300) {
      RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
      ep->next_condition = accept_pracked;
    }
  default:
    break;
  }

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int respond_483_to_prack(CONDITION_PARAMS);
static int prack_until_terminated(CONDITION_PARAMS);

int test_183rel(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ei, *ep;

  if (print_headings)
    printf("TEST NUA-10.2.1: Call with 100rel, 183 and 180\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, ringing_pracked);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next);

  ep = e->data->e_event == nua_r_prack ? e : NULL;

  if (ep) {
    TEST(ep->data->e_status, 200); TEST_1(e = e->next);
  }

  TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  if (!ep) {
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
    TEST(e->data->e_status, 200);
  }

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 183);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 183 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 180);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Respond with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 200);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.2.1: PASSED\n");

  /* Test for graceful termination by client because 483 sent to PRACK */
  if (print_headings)
    printf("TEST NUA-10.2.2: graceful termination because PRACK fails\n");

  nua_set_hparams(a_call->nh, NUTAG_APPL_METHOD("PRACK"), TAG_END());
  nua_set_hparams(b_call->nh, NUTAG_APPL_METHOD("PRACK"),
		  NUTAG_AUTOANSWER(0), TAG_END());
  run_ab_until(ctx, nua_r_set_params, NULL, nua_r_set_params, NULL);

  INVITE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, prack_until_terminated, -1, respond_483_to_prack);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> TERMINATING: nua_r_invite, nua_i_state,
                                  nua_r_prack, nua_i_state
     TERMINATING -(T1)-> TERMINATED: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 483);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating);

  {
    int bye = 1, cancel = 1, invite = 1, state = 1;

    while (bye || cancel || invite || state) {
      TEST_1(e = e->next);
      if (e->data->e_event == nua_r_bye) {
	TEST_E(e->data->e_event, nua_r_bye);
	TEST(e->data->e_status, 200);
	bye = 0;
	break;
      }
      else if (e->data->e_event == nua_r_invite) {
	TEST_E(e->data->e_event, nua_r_invite);
	TEST(e->data->e_status, 487);
	invite = 0;
      }
      else if (e->data->e_event == nua_r_cancel) {
	TEST_E(e->data->e_event, nua_r_cancel);
	TEST_1(e->data->e_status == 200 || e->data->e_status == 481);
	cancel = 0;
      }
      else if (e->data->e_event == nua_i_state) {
	TEST_E(e->data->e_event, nua_i_state);
	TEST(callstate(e->data->e_tags), nua_callstate_terminated);
	state  = 0;
      }
    }
    if (e->next) {
      /* 2nd terminated? */
      TEST_1(e = e->next);
      TEST_E(e->data->e_event, nua_i_state);
      TEST(callstate(e->data->e_tags), nua_callstate_terminated);
    }
  }

  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state, nua_respond(to PRACK)
   EARLY -(S3b)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 183);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 183 is PRACKed, PRACK is responded with 483 */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);

  /* Client terminates the call
     - we may (it is received before BYE) or may not (received after BYE)
       get CANCEL request
  */
  TEST_1(e = e->next);
  if (e->data->e_event == nua_i_cancel) {
    TEST_E(e->data->e_event, nua_i_cancel);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_ready);
    TEST_1(e = e->next);
  }

  TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.2.2: PASSED\n");

  END();
}

static int prack_until_terminated(CONDITION_PARAMS)
{
  static int terminated, bye_responded, invite_responded, cancel_responded;

  if (!check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite && 100 < status && status < 200 &&
      sip_has_feature(sip->sip_require, "100rel")) {
    sip_rack_t rack[1];

    sip_rack_init(rack);
    rack->ra_response = sip->sip_rseq->rs_response;
    rack->ra_cseq = sip->sip_cseq->cs_seq;
    rack->ra_method = sip->sip_cseq->cs_method;
    rack->ra_method_name = sip->sip_cseq->cs_method_name;

    nua_prack(nh, SIPTAG_RACK(rack), TAG_END());
  }

  if (event == nua_i_state && callstate(tags) == nua_callstate_terminated)
    terminated = 1;

  if (event == nua_r_bye && status >= 200)
    bye_responded = 1;

  if (event == nua_r_invite && status >= 200)
    invite_responded = 1;

  if (event == nua_r_cancel && status >= 200)
    cancel_responded = 1;

  return terminated && bye_responded && invite_responded && cancel_responded;
}

int respond_483_to_prack(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  switch (event) {
  case nua_i_prack:
    if (status <= 200) {
      RESPOND(ep, call, nh, 483, "Foo",
	      NUTAG_WITH_THIS(nua),
	      TAG_END());
    }
  default:
    break;
  }

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

/*
 X  ringing_updated   ep
 |-------INVITE------>|
 |       (sdp)        |
 |<----100 Trying-----|
 |                    |
 |<-------183---------|
 |       (sdp)        |
 |-------PRACK------->|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
 |-------UPDATE------>|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
<using  acccept_pracked>
 |                    |
 |<-------180---------|
 |-------PRACK------->|
 |<-------200---------|
 |                    |
 |<------200 OK-------|
 |--------ACK-------->|
 |                    |
*/
int ringing_updated(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_update:
    if (200 <= status && status < 300) {
      RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
      ep->next_condition = accept_pracked;
    }
    return 0;
  default:
    break;
  }

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS,
	    SIPTAG_REQUIRE_STR("100rel"),
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_preconditions(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ep, *ei;
  sip_t *sip;

  if (print_headings)
    printf("TEST NUA-10.3.1: Call with 100rel and preconditions\n");

/* Test for precondition:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------183---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |-------UPDATE------>|
   |<-------200---------|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition"),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(0),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel"),
	 SIPTAG_REQUIRE_STR("precondition"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, ringing_updated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING --> PROCEEDING: nua_r_prack, nua_i_state
     PROCEEDING --> PROCEEDING: nua_r_update, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_update);

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_update);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  if (e == ep)			/* invite was responded before update */
    e = ep->next->next;

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);
  /* Does not have effect on call state */

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  if (ctx->proxy_tests) {
    TEST_1(sip->sip_session_expires);
    TEST_S(sip->sip_session_expires->x_refresher, "uas");
    TEST_1(!sip_has_supported(sip->sip_require, "timer"));
  }

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY --> EARLY: nua_i_prack, nua_i_state
   EARLY --> EARLY: nua_i_update, nua_i_state
   EARLY --> EARLY: nua_r_update, nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 183);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_sent(e->data->e_tags));
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(!is_answer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 180);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* 180 PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Responded with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 200);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.3.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.3.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.3.2: PASSED\n");

  END();
}

/*
 X  accept_updated    ep
 |-------INVITE------>|
 |       (sdp)        |
 |<----100 Trying-----|
 |                    |
 |<-------183---------|
 |       (sdp)        |
 |-------PRACK------->|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
 |-------UPDATE------>|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
 |                    |
 |<-------180---------|
 |                    |
 |<------200 OK-------|
 |--------ACK-------->|
 |                    |
*/
int accept_updated(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_update:
    if (200 <= status && status < 300) {
      RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    }
    return 0;
  default:
    break;
  }

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS,
	    SIPTAG_REQUIRE_STR("100rel"),
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    if (status == 180)
      RESPOND(ep, call, nh, SIP_200_OK, TAG_END());
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}


int test_preconditions2(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *eu, *ei;
  enum nua_callstate ustate, istate;

  if (print_headings)
    printf("TEST NUA-10.4.1: Call with preconditions and non-100rel 180\n");

/* Test 100rel and preconditions with NUTAG_ONLY183_100REL(1):

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------183---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |-------UPDATE------>|
 +------------------------+
 | |<-------200---------| |
 | |			| |
 | |<-------180---------| |
 | |			| |
 | |<------200 OK-------| |
 +------------------------+
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

   Note that the boxed responses above can be re-ordered
   (180 or 200 OK to INVITE is received before 200 OK to UPDATE).
   ACK, however, is sent only after 200 OK to both UPDATE and INVITE.
*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel"),
	 SIPTAG_REQUIRE_STR("precondition"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_updated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  /* Offer is sent in PRACK */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  /* Send UPDATE */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  /* The final response to the UPDATE and INVITE can be received in any order */
  eu = event_by_type(e->next, nua_r_update);
  ei = event_by_type(e->next, nua_r_invite);

  TEST_1(e = eu); TEST_E(e->data->e_event, nua_r_update);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  ustate = callstate(e->data->e_tags);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* Final response to INVITE  */
  TEST_1(ei = event_by_type(ei->next, nua_r_invite));

  TEST_E(ei->data->e_event, nua_r_invite); TEST(ei->data->e_status, 200);
  TEST_1(e = ei->next); TEST_E(e->data->e_event, nua_i_state);
  istate = callstate(e->data->e_tags);
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  if (eu == e->next) {
    /* 200 OK to UPDATE is received after 200 OK to INVITE */
    TEST(ustate, nua_callstate_ready);
    TEST(istate, nua_callstate_completing);
  }
  else {
    /* 200 OK to UPDATE is received before 200 OK to INVITE */
    TEST(ustate, nua_callstate_proceeding);
    TEST(istate, nua_callstate_ready);
  }

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* Responded with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.4.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.4.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.4.2: PASSED\n");

  END();
}

/*
 X  ringing_updated2  ep
 |-------INVITE------>|
 |       (sdp)        |
 |<----100 Trying-----|
 |                    |
 |<-------183---------|
 |       (sdp)        |
 |-------PRACK------->|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
 |-------UPDATE------>|
 |       (sdp)        |
 |<-------200---------|
 |       (sdp)        |
 |                    |
 |<------UPDATE-------|
 |       (sdp)        |
 |--------200-------->|
 |       (sdp)        |
 |                    |
<using  acccept_pracked>
 |                    |
 |<-------180---------|
 |-------PRACK------->|
 |<-------200---------|
 |                    |
 |<------200 OK-------|
 |--------ACK-------->|
 |                    |
*/
int ringing_updated2(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_update:
    if (200 <= status && status < 300) {
      UPDATE(ep, call, nh, TAG_END());
    }
    return 0;
  case nua_r_update:
    if (200 <= status && status < 300) {
      RESPOND(ep, call, nh, SIP_180_RINGING,
	      SIPTAG_REQUIRE_STR("100rel"),
	      TAG_END());
      ep->next_condition = accept_pracked;
    }
    else if (300 <= status) {
      RESPOND(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
    }
    return 0;
  default:
    break;
  }

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS,
	    SIPTAG_REQUIRE_STR("100rel"),
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_update_by_uas(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ep, *ei;
  sip_t *sip;

  /* -------------------------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-10.5.1: Call with dual UPDATE\n");

/* Test for update by UAS.

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------183---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |-------UPDATE------>|
 +------------------------+
 | |<-------200---------| |
 | |			| |
 | |<------UPDATE-------| |
 +------------------------+
   |--------200-------->|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

 Note that the 200 OK to UPDATE from A and UPDATE from B may be re-ordered
 In that case, A will respond with 500/Retry-After and B will retry UPDATE.
 See do {} while () loop below.
*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition"),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(0),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel"),
	 SIPTAG_REQUIRE_STR("precondition"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, ringing_updated2);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING: nua_r_prack, nua_i_state
     PROCEEDING: nua_r_update, nua_i_state
     PROCEEDING: nua_i_update, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_update);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_update);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);
  /* Does not have effect on call state */

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  if (ctx->proxy_tests) {
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_session_expires);
    TEST_S(sip->sip_session_expires->x_refresher, "uas");
    TEST_1(!sip_has_supported(sip->sip_require, "timer"));
  }

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(183), nua_i_state
   EARLY --> EARLY: nua_i_prack, nua_i_state
   EARLY --> EARLY: nua_i_update, nua_i_state
   EARLY --> EARLY: nua_update(), nua_i_state
   EARLY --> EARLY: nua_r_update, nua_i_state
   EARLY --> EARLY: nua_respond(180), nua_i_state
   EARLY --> EARLY: nua_i_prack, nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(200), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 183);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  /* sent UPDATE */
  do {
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
    TEST_1(is_offer_sent(e->data->e_tags));
    TEST_1(!is_offer_recv(e->data->e_tags));
    TEST_1(!is_answer_sent(e->data->e_tags));
    TEST_1(!is_answer_recv(e->data->e_tags));

    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_update);
    if (e->data->e_status == 100) {
      TEST_1(sip = sip_object(e->data->e_msg));
      TEST(sip->sip_status->st_status, 500); TEST_1(sip->sip_retry_after);
    }
  } while (e->data->e_status == 100);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_sent(e->data->e_tags)); /* XXX */
  TEST_1(!is_offer_recv(e->data->e_tags));
  TEST_1(!is_answer_sent(e->data->e_tags));
  TEST_1(is_answer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 180);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* 180 PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Responded with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(e->data->e_status, 200);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.5.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.5.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.5.2: PASSED\n");

  END();
}

int test_update_failure(struct context *ctx)
{
  BEGIN();
#if 0
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *eu, *ei;
  enum nua_callstate ustate, istate;

  if (print_headings)
    printf("TEST NUA-10.4.1: UPDATE failure terminating session\n");

/* Test UPDATE failing:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------183---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<-------180---------|
   |			|
 +------------------------+
   |<------200 OK-------|
   |-------UPDATE------>|
   |			|
   |<-------481---------|
   |			|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(1),
		 SIPTAG_SUPPORTED_STR("100rel, precondition, timer"),
		 NUTAG_APPL_METHOD("UPDATE"),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel"),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, until_pracked);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 183);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  /* Offer is sent in PRACK */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  /* Send UPDATE */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));

  /* The final response to the UPDATE and INVITE can be received in any order */
  eu = event_by_type(e->next, nua_r_update);
  ei = event_by_type(e->next, nua_r_invite);

  TEST_1(e = eu); TEST_E(e->data->e_event, nua_r_update);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  ustate = callstate(e->data->e_tags);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* Final response to INVITE  */
  TEST_1(ei = event_by_type(ei->next, nua_r_invite));

  TEST_E(ei->data->e_event, nua_r_invite); TEST(ei->data->e_status, 200);
  TEST_1(e = ei->next); TEST_E(e->data->e_event, nua_i_state);
  istate = callstate(e->data->e_tags);
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  if (eu == e->next) {
    /* 200 OK to UPDATE is received after 200 OK to INVITE */
    TEST(ustate, nua_callstate_ready);
    TEST(istate, nua_callstate_completing);
  }
  else {
    /* 200 OK to UPDATE is received before 200 OK to INVITE */
    TEST(ustate, nua_callstate_proceeding);
    TEST(istate, nua_callstate_ready);
  }

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */

  /* Responded with 183 Session Progress */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  /* Responded with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.4.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.4.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.4.2: PASSED\n");
#endif
  END();
}

int cancel_when_pracked(CONDITION_PARAMS);
int alert_call(CONDITION_PARAMS);

int test_180rel_cancel1(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-10.6: CANCEL after PRACK\n");

/* Test for 100rel:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |------CANCEL------->|
   |<------200 OK-------|
   |			|
   |<-------487---------|
   |--------ACK-------->|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(0),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(0),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, cancel_when_pracked, -1, alert_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> TERMINATED: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_prack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_cancel);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 487);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   Option A:
   EARLY -(S10)-> TERMINATED: nua_i_cancel, nua_i_state
   Option B:
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
   READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.6: PASSED\n");

  END();
}

int cancel_when_pracked(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_prack)
    CANCEL(ep, call, nh, TAG_END());

  switch (callstate(tags)) {
  case nua_callstate_proceeding:
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}

int test_180rel_cancel2(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ep, *ec;

  if (print_headings)
    printf("TEST NUA-10.7: CANCEL after 100rel 180\n");

/* Test for 100rel:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |------CANCEL------->|
   |<------200 OK-------|
   |			|
   |<-------487---------|
   |--------ACK-------->|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(0),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_ONLY183_100REL(0),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, cancel_when_ringing, -1, accept_pracked2);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> TERMINATED: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

#define NEXT_SKIP(x) \
  do { TEST_1(e = e->next); } \
  while (x);

  NEXT_SKIP(e->data->e_event == nua_r_prack ||
	    e->data->e_event == nua_r_cancel ||
	    e->data->e_event == nua_i_state);

  TEST_E(e->data->e_event, nua_r_invite);
  if (e->data->e_status == 487) {
    TEST(e->data->e_status, 487);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_terminated);
    if (e->next)
      NEXT_SKIP(e->data->e_event == nua_r_prack || e->data->e_event == nua_r_cancel);
    TEST_1(!e->next);
  }
  else {
    TEST(e->data->e_status, 200);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_ready);

    BYE(a, a_call, a_call->nh, TAG_END());
    run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

    NEXT_SKIP(e->data->e_event == nua_r_prack || e->data->e_event == nua_r_cancel);
    TEST_E(e->data->e_event, nua_r_bye);
    TEST(e->data->e_status, 200);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
    TEST_1(!e->next);
  }

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   Option A:
   EARLY -(S10)-> TERMINATED: nua_i_cancel, nua_i_state
   Option B:
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
   READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  ec = event_by_type(e->next, nua_i_cancel);

  if (ec) {
    TEST_1(e = ec); TEST_E(e->data->e_event, nua_i_cancel);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  }
  else {
    /* 180 is PRACKed, PRACK does not have effect on call state */
    ep = event_by_type(e->next, nua_i_prack);
    if (ep) e = ep;
    /* Responded with 200 OK */
    TEST_1(e = event_by_type(e->next, nua_i_state));
    TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
    TEST_1(!is_offer_answer_done(e->data->e_tags));
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
    TEST_1(!is_offer_answer_done(e->data->e_tags));
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  }

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.7: PASSED\n");

  END();
}

int redirect_pracked(CONDITION_PARAMS);

int test_180rel_redirected(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *ep, *ei;
  sip_t *sip;

  if (print_headings)
    printf("TEST NUA-10.8.1: Call with 100rel and 180\n");

/* Test for 100rel:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<----302 Moved------|
   |--------ACK-------->|
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<-------180---------|
   |-------PRACK------->|
   |<-------200---------|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|

*/

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(1),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_SESSION_TIMER(180),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, redirect_pracked);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING->(redirected)->CALLING: nua_r_invite, nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state, nua_r_prack
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite); /* 302 */
  ep = event_by_type(e->next, nua_r_prack); /* 200 */
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 302);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!is_offer_sent(e->data->e_tags));

  ei = event_by_type(e->next, nua_r_invite);
  ep = event_by_type(e->next, nua_r_prack);
  if (!ep) {
    run_a_until(ctx, -1, save_until_final_response);
    ep = event_by_type(e->next, nua_r_prack);
  }

  TEST_1(e = ep); TEST_E(e->data->e_event, nua_r_prack);
  TEST(e->data->e_status, 200);

  TEST_1(e = ei); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(!e->next || !ep->next);
  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* 302 terminates call */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* Terminated */

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));

  /* Responded with 180 Ringing */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));

  /* 180 is PRACKed */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_prack);
  /* Does not have effect on call state */

  /* Respond with 200 OK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(!is_offer_answer_done(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_offer_answer_done(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-10.8.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-10.8.2: terminate call\n");

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* A: READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-10.8.2: PASSED\n");

  END();
}

int redirect_pracked(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_prack) {
    if (!ep->flags.bit0) {
      sip_contact_t m[1];

      ep->flags.bit0 = 1;

      *m = *ep->contact;
      m->m_url->url_user = "302";
      RESPOND(ep, call, nh, SIP_302_MOVED_TEMPORARILY, SIPTAG_CONTACT(m), TAG_END());
      return 0;
    }
    else {
      RESPOND(ep, call, nh, SIP_200_OK,
	      TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	      TAG_END());
    }
  }


  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 0;
  default:
    return 0;
  }
}

int test_100rel(struct context *ctx)
{
  int retval = 0;

  retval = test_180rel(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_prack_auth(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_183rel(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_preconditions(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_preconditions2(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_update_by_uas(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_update_failure(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_180rel_cancel1(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_180rel_cancel2(ctx); RETURN_ON_SINGLE_FAILURE(retval);
  retval = test_180rel_redirected(ctx); RETURN_ON_SINGLE_FAILURE(retval);

  nua_set_params(ctx->a.nua,
		 NUTAG_EARLY_MEDIA(0),
		 SIPTAG_SUPPORTED(ctx->a.supported),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->b.nua,
		 NUTAG_EARLY_MEDIA(0),
		 NUTAG_ONLY183_100REL(0),
		 SIPTAG_SUPPORTED(ctx->b.supported),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(ctx->c.nua,
		 NUTAG_EARLY_MEDIA(0),
		 NUTAG_ONLY183_100REL(0),
		 SIPTAG_SUPPORTED(ctx->c.supported),
		 TAG_END());
  run_c_until(ctx, nua_r_set_params, until_final_response);

  return retval;
}
