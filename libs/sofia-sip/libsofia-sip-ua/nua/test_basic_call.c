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

/**@CFILE test_nua_basic_call.c
 * @brief Test basic call.
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
#define __func__ "test_basic_call"
#endif

/* ======================================================================== */

int until_terminated(CONDITION_PARAMS)
{
  if (!check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  return event == nua_i_state && callstate(tags) == nua_callstate_terminated;
}

/*
 X     accept_call    ep
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
*/
int accept_call(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, 
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
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


/*
 X     accept_call    ep
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
*/
int accept_call_with_early_sdp(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, 
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    NUTAG_M_DISPLAY("Bob"),
	    NUTAG_M_USERNAME("b+b"),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
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


/*
 X      INVITE
 |                    |
 |-------INVITE------>|
 |<--------200--------|
 |---------ACK------->|
*/
int until_ready(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

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

/* ======================================================================== */

/* Basic call:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK------>|
   |			|

   Client transitions:
   INIT -(C1)-> CALLING -(C2a)-> PROCEEDING -(C3+C4)-> READY
   Server transitions:
   INIT -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S4)-> READY

   B sends BYE:
   READY -(T2)-> TERMINATING -(T3)-> TERMINATED
   A receives BYE:
   READY -(T1)-> TERMINATED

   See @page nua_call_model in nua.docs for more information
*/

int test_basic_call_1(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t *sip;
  sip_replaces_t *repa, *repb;
  nua_handle_t *nh;

  if (print_headings)
    printf("TEST NUA-3.1: Basic call\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_M_USERNAME("a+a"),
	 NUTAG_M_DISPLAY("Alice"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call_with_early_sdp);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

  TEST_1(repa = nua_handle_make_replaces(a_call->nh, nua_handle_home(a_call->nh), 0));
  TEST_1(repb = nua_handle_make_replaces(b_call->nh, nua_handle_home(b_call->nh), 0));

  TEST_S(repa->rp_call_id, repb->rp_call_id);

  TEST_1(!nua_handle_by_replaces(a->nua, repa));
  TEST_1(!nua_handle_by_replaces(b->nua, repb));

  TEST_1(nh = nua_handle_by_replaces(a->nua, repb));
  TEST_P(nh, a_call->nh);
  nua_handle_unref(nh);

  TEST_1(nh = nua_handle_by_replaces(b->nua, repa));
  TEST_P(nh, b_call->nh);
  nua_handle_unref(nh);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_S(sip->sip_call_id->i_id, repb->rp_call_id);
  TEST_S(sip->sip_from->a_tag, repb->rp_to_tag);
  TEST_S(sip->sip_to->a_tag, repb->rp_from_tag);
  TEST_1(sip->sip_payload);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_payload);
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Bob");
  TEST_S(sip->sip_contact->m_url->url_user, "b+b");
  /* Test that B uses application-specific contact */
  if (ctx->proxy_tests)
    TEST_1(sip->sip_contact->m_url->url_user);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
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
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Alice");
  TEST_S(sip->sip_contact->m_url->url_user, "a+a");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_S(sip->sip_call_id->i_id, repa->rp_call_id);
  TEST_S(sip->sip_from->a_tag, repa->rp_from_tag);
  TEST_S(sip->sip_to->a_tag, repa->rp_to_tag);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  BYE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head);  TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  /* A transitions:
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(!nua_handle_has_active_call(a_call->nh));

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.1: PASSED\n");

  END();
}

/*
  accept_early_answer
 X                    ep
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
*/
int accept_early_answer(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    NUTAG_EARLY_ANSWER(1),
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
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

int test_basic_call_2(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-3.2: Basic call with SDP in 180\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, 
				 SIPTAG_TO_STR("<sip:b@x.org>"),
				 TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_ALLOW("INFO"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_early_answer);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_content_type); 
  TEST_S(sip->sip_content_type->c_type, "application/sdp");
  TEST_1(sip->sip_payload);	/* there is sdp in 200 OK */
  TEST_1(sip->sip_contact);
  /* Test that B does not use application-specific contact */
  TEST_1(!sip->sip_contact->m_url->url_user);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!is_answer_recv(e->data->e_tags)); /* but it is ignored */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

  /* Send a NOTIFY from B to A */
  if (print_headings)
    printf("TEST NUA-3.2.2: send a NOTIFY within a dialog\n");

  /* Make A to accept NOTIFY */
  nua_set_params(a->nua, NUTAG_APPL_METHOD("NOTIFY"), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  NOTIFY(b, b_call, b_call->nh,
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBJECT_STR("NUA-3.2.2"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: no"),
	 TAG_END());

  run_ab_until(ctx, -1, accept_notify, -1, save_until_final_response);

  /* Notifier events: nua_r_notify */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value,
       nua_substate_terminated);

  /* watcher events: nua_i_notify */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, 
       nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-3.2.2: PASSED\n");

  INFO(b, b_call, b_call->nh, TAG_END());
  BYE(b, b_call, b_call->nh, TAG_END());
  INFO(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  while (!b->events->head || /* r_info */
	 !b->events->head->next || /* r_bye */
	 !b->events->head->next->next || /* i_state */
	 !b->events->head->next->next->next) /* r_info */
    run_ab_until(ctx, -1, save_events, -1, save_until_final_response);

  /* B transitions:
   nua_info()
   READY --(T2)--> TERMINATING: nua_bye()
   nua_r_info with 200
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
   nua_r_info with 481/900
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_info);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_info);
  TEST_1(e->data->e_status >= 900 || e->data->e_status == 481);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  /* A transitions:
     nua_i_info
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_info);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  BYE(a, a_call, a_call->nh, TAG_END());
  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e->data->e_status >= 900);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.2: PASSED\n");

  END();
}


int test_basic_call(struct context *ctx)
{
  return test_basic_call_1(ctx) || test_basic_call_2(ctx);
}
