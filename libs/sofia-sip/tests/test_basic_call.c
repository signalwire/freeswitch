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

  static int once = 0;
  sip_time_t se, min_se;

  if (print_headings)
    printf("TEST NUA-3.1: Basic call\n");

  /* Disable session timer from proxy */
  test_proxy_get_session_timer(ctx->p, &se, &min_se);
  test_proxy_set_session_timer(ctx->p, 0, 0);

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
  if (!once) {
    /* The session expiration is not used by default. */
    TEST_1(sip->sip_session_expires == NULL);
  }
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
  if (!once++) {
    /* The Session-Expires header is not used by default. */
    TEST_1(sip->sip_session_expires == NULL);
  }
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

  test_proxy_set_session_timer(ctx->p, se, min_se);

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
#if nomore
  /* Test that B does not use application-specific contact */
  TEST_1(!sip->sip_contact->m_url->url_user);
#else
  /* sf.net bug #1816647: Outbound contact does not make it to dialogs */
  /* Now we use first registered contact if aor does not match */
  if (ctx->proxy_tests)
    TEST_S(sip->sip_contact->m_url->url_user, "b");
#endif
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
  TEST_1(e->data->e_status >= 900 || e->data->e_status == 481 ||
	 /* INFO received outside dialog where it is Allow:ed */
	 e->data->e_status == 405);
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

/* ====================================================================== */

/* Basic calls with soa disabled */

int accept_call_no_media(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    NUTAG_MEDIA_ENABLE(0),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
	    TAG_IF(call->sdp, SIPTAG_PAYLOAD_STR(call->sdp)),
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

int test_basic_call_3(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-3.3: Basic call with media disabled\n");

  a_call->sdp = "v=0\r\n"
    "o=- 1 1 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 5008 RTP/AVP 8\r\n";

  b_call->sdp =
    "v=0\r\n"
    "o=- 2 1 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 5010 RTP/AVP 0 8\r\n";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call,
				 SIPTAG_TO_STR("<sip:b@x.org>"),
				 TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 NUTAG_MEDIA_ENABLE(0),
	 NUTAG_URL(b->contact->m_url),
	 SIPTAG_CONTENT_TYPE_STR("application/sdp"),
	 SIPTAG_PAYLOAD_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call_no_media);

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
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/sdp");
  TEST_1(sip->sip_payload);	/* there is sdp in 200 OK */
  TEST_1(sip->sip_contact);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
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
  TEST_1(!is_answer_sent(e->data->e_tags));
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

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  /* A transitions:
     nua_i_info
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.3: PASSED\n");

  END();
}

int ack_when_completing_no_media(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completing:
    ACK(ep, call, nh,
	TAG_IF(call->sdp, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
	TAG_IF(call->sdp, SIPTAG_PAYLOAD_STR(call->sdp)),
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

int accept_call_no_media2(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    NUTAG_MEDIA_ENABLE(0),
	    TAG_IF(call->sdp, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
	    TAG_IF(call->sdp, SIPTAG_PAYLOAD_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
	    TAG_IF(call->sdp, SIPTAG_PAYLOAD_STR(call->sdp)),
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

/* Media disabled, offer/answer in 200 OK/ACK */
int test_basic_call_4(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-3.4: 3pcc call with media disabled\n");

  a_call->sdp = "v=0\r\n"
    "o=- 1 1 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 5008 RTP/AVP 8\r\n";

  b_call->sdp =
    "v=0\r\n"
    "o=- 2 1 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 5010 RTP/AVP 0 8\r\n";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call,
				 SIPTAG_TO_STR("<sip:b@x.org>"),
				 TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 NUTAG_MEDIA_ENABLE(0),
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_AUTOACK(0),
	 TAG_END());

  run_ab_until(ctx, -1, ack_when_completing_no_media,
	       -1, accept_call_no_media2);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(!is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(!is_answer_recv(e->data->e_tags));
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/sdp");
  TEST_1(sip->sip_payload);	/* there is sdp in 200 OK */
  TEST_1(sip->sip_contact);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_sent(e->data->e_tags));
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
  TEST_1(!is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!is_answer_sent(e->data->e_tags));
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

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

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  /* A transitions:
     nua_i_info
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.4: PASSED\n");

  END();
}

int change_uri_in_ack(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completing:
    ACK(ep, call, nh,
	SIPTAG_FROM_STR("sip:anonymous@org.invalid"),
	SIPTAG_TO_STR("sip:anonymous@net.invalid"),
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

/* Test changing from/to within dialog */
/* Test that a proper Contact gets selected in response
 * regardless of the To URI.
 */
int test_basic_call_5(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-3.5: test changing From/To URL in ACK\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call,
				 SIPTAG_TO_STR("<sips:b@x.org>"),
				 TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOACK(0),
	 TAG_END());

  run_ab_until(ctx, -1, change_uri_in_ack, -1, accept_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  if (ctx->proxy_tests)		/* Use Contact from registration? */
    TEST_S(sip->sip_contact->m_url->url_user, "b");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_S(sip->sip_to->a_url->url_user, "anonymous");
  TEST_S(sip->sip_from->a_url->url_user, "anonymous");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

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

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  /* A transitions:
     nua_i_info
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.5: PASSED\n");

  END();
}

/* ======================================================================== */

/* Call with media upgrade:

   A			B
   |-------INVITE------>|
   |   with audio only	|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<------INVITE-------|
   |	with video	|
   |-----100 Trying---->|
   |			|
   |-------200 OK------>|
   |	with video	|
   |<-------ACK---------|
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
int accept_upgrade(CONDITION_PARAMS);

int test_video_call_1(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t *sip;
  sdp_session_t *b_sdp;
  sdp_media_t *m, b_video[1];
  sdp_rtpmap_t *rm, b_h261[1];

  sip_time_t se, min_se;

  if (print_headings)
    printf("TEST NUA-3.6: Basic call\n");

  /* Disable session timer from proxy */
  test_proxy_get_session_timer(ctx->p, &se, &min_se);
  test_proxy_set_session_timer(ctx->p, 0, 0);

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOANSWER(0),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call_with_early_sdp);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

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
  TEST_1(sip->sip_payload);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(tl_find(e->data->e_tags, soatag_local_sdp));
  TEST_1(b_sdp = sdp_session_dup(nua_handle_home(a_call->nh),
				 (sdp_session_t *)
				 tl_find(e->data->e_tags, soatag_local_sdp)
				 ->t_value));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  a_call->sdp =
    "m=audio 5008 RTP/AVP 8\n"
    "m=video 5014 RTP/AVP 34 31\n";

  m = memset(b_video, 0, sizeof b_video);
  m->m_size = sizeof *m;
  m->m_session = b_sdp;
  m->m_type = sdp_media_video, m->m_type_name = "video";
  m->m_port = 5016;
  m->m_proto = sdp_proto_rtp; m->m_proto_name = "RTP/AVP";
  m->m_rtpmaps = memset(rm = b_h261, 0, sizeof b_h261);
  rm->rm_size = sizeof *rm;
  rm->rm_pt = 31; rm->rm_encoding = "h261"; rm->rm_rate = 90000;

  b_sdp->sdp_media->m_next = m;

  INVITE(b, b_call, b_call->nh,
	 SOATAG_USER_SDP(b_sdp),
	 TAG_END());
  run_ab_until(ctx, -1, accept_upgrade, -1, until_ready);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(tl_find(e->data->e_tags, soatag_local_sdp_str));
  TEST_1(strstr((char *)
		tl_find(e->data->e_tags, soatag_local_sdp_str)->t_value,
		"m=video"));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(tl_find(e->data->e_tags, soatag_local_sdp_str));
  TEST_1(strstr((char *)
		tl_find(e->data->e_tags, soatag_local_sdp_str)->t_value,
		"m=video"));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

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

  test_proxy_set_session_timer(ctx->p, se, min_se);

  if (print_headings)
    printf("TEST NUA-3.6: PASSED\n");

  END();
}

int accept_upgrade(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_invite && status < 200) {
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
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

/* Basic call and re-INVITE with user-specified Contact:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |-----re-INVITE----->|
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

   Both client and server save Contact from nua_invite() and nua_respond(),
   respectively.

   INIT -(C1)-> CALLING -(C3a+C4)-> READY
   INIT -(S3c)-> COMPLETED -(S4)-> READY

   Both client and server use saved Contact.

   B sends BYE:
   READY -(T2)-> TERMINATING -(T3)-> TERMINATED
   A receives BYE:
   READY -(T1)-> TERMINATED

   See @page nua_call_model in nua.docs for more information
*/

static sip_contact_t *contact_for_b;

int accept_call_with_contact(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    SIPTAG_CONTACT(contact_for_b),
	    TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    SIPTAG_CONTACT(contact_for_b),
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

int test_basic_call_6(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_route_t *r, rb[1];
  sip_t *sip;

  sip_contact_t ma[1], mb[1];

  if (print_headings)
    printf("TEST NUA-3.6: Basic call\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  *ma = *a->contact;
  ma->m_display = "Alice B.";
  ma->m_url->url_user = "a++a";

  *mb = *b->contact;
  mb->m_display = "Bob A.";
  mb->m_url->url_user = "b++b";

  contact_for_b = mb;

  sip_route_init(rb)->r_url[0] = b->contact->m_url[0];
  rb->r_url->url_user = "bob+0";
  url_param_add(nua_handle_home(a_call->nh), rb->r_url, "lr");

  INVITE(a, a_call, a_call->nh,
	 NUTAG_URL("sip:bob@example.org"), /* Expanded by proxy */
	 SIPTAG_ROUTE_STR("B2 <sip:bob+2@example.org>;bar=foo"), /* Last in list */
	 NUTAG_INITIAL_ROUTE(ctx->lr), /* Removed by proxy (if any) */
	 NUTAG_INITIAL_ROUTE(rb), /* Used to route request to b (not removed)  */
	 NUTAG_INITIAL_ROUTE_STR("B1 <sip:bob+1@example.org;lr>;foo=bar"), /* Next in list */
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_CONTACT(ma),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call_with_contact);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

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
  TEST_1(sip->sip_payload);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_payload);
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Bob A.");
  TEST_S(sip->sip_contact->m_url->url_user, "b++b");
  /* Test that B uses application-specific contact */
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
  TEST_S(sip->sip_contact->m_display, "Alice B.");
  TEST_S(sip->sip_contact->m_url->url_user, "a++a");
  TEST_1(r = sip->sip_route);
  TEST_S(r->r_url->url_user, "bob+0");
  TEST_1(r = r->r_next);
  TEST_S(r->r_url->url_user, "bob+1");
  TEST_1(r = r->r_next);
  TEST_S(r->r_url->url_user, "bob+2");
  TEST_1(!r->r_next);

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

  /* re-INVITE */
  INVITE(a, a_call, a_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Bob A.");
  TEST_S(sip->sip_contact->m_url->url_user, "b++b");
  /* Test that B uses application-specific contact */
  TEST_1(sip->sip_contact->m_url->url_user);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact);
  TEST_S(sip->sip_contact->m_display, "Alice B.");
  TEST_S(sip->sip_contact->m_url->url_user, "a++a");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
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
    printf("TEST NUA-3.6: PASSED\n");

  END();
}

/* Terminate call with 408:

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |--------INFO------->|
   |<--------408--------|
   |			|
   |<-------BYE---------|
   |--------487-------->|

   Client transitions:
   INIT -(C1)-> CALLING -(C2a)-> PROCEEDING -(C3+C4)-> READY
   Server transitions:
   INIT -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S4)-> READY

   A sends UPDATE:
   READY -(T1)-> TERMINATED

   B sends BYE:
   READY -(T2)-> TERMINATING -(T3)-> TERMINATED

   See @page nua_call_model in nua.docs for more information
*/
int reject_method(CONDITION_PARAMS);
int reject_update(CONDITION_PARAMS);

int test_basic_call_7(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t *sip;
  sip_replaces_t *repa, *repb;
  nua_handle_t *nh;

  if (print_headings)
    printf("TEST NUA-3.7.1: Release dialog with error response (RFC 5057)\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
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

  /* Make B to process HUMPPA at application level */
  nua_set_hparams(b_call->nh, NUTAG_APPL_METHOD("HUMPPA"),
		  NUTAG_ALLOW("HUMPPA"),
		  TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  METHOD(a, a_call, a_call->nh, NUTAG_METHOD("HUMPPA"), TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, reject_method);

  TEST_1(e = a->events->head);  TEST_E(e->data->e_event, nua_r_method);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(!nua_handle_has_active_call(a_call->nh));

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_method);
  if (e->next == NULL)
    run_b_until(ctx, nua_i_state, NULL);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.7.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-3.7.2: Release dialog usage with error response (RFC 5057)\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(!nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
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

  /* Let A allow UPDATE  */
  nua_set_params(a->nua, NUTAG_ALLOW("UPDATE"), TAG_END());
  /* Make B to process UPDATE at application level */
  nua_set_hparams(b_call->nh, NUTAG_APPL_METHOD("UPDATE"),
		  NUTAG_ALLOW("UPDATE"),
		  TAG_END());
  run_ab_until(ctx, nua_r_set_params, NULL, nua_r_set_params, NULL);

  UPDATE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, reject_update);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(!nua_handle_has_active_call(a_call->nh));

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_update);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

#if nomore
  /* Used to use INFO instead of UPDATE and INFO was asymmetric */
  TEST_1(!nua_handle_has_active_call(b_call->nh));

  INFO(b, b_call, b_call->nh, TAG_END());
  run_b_until(ctx, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head);  TEST_E(e->data->e_event, nua_r_info);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);
#endif

  TEST_1(!nua_handle_has_active_call(b_call->nh));

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-3.7.2: PASSED\n");

  END();
}

int reject_method(CONDITION_PARAMS)
{
  msg_t *current = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_method) {
    RESPOND(ep, call, nh,
	    SIP_604_DOES_NOT_EXIST_ANYWHERE,
	    NUTAG_WITH(current),
	    TAG_END());
    return 1;
  }
  return 0;
}

int reject_update(CONDITION_PARAMS)
{
  msg_t *current = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_update) {
    RESPOND(ep, call, nh,
	    SIP_480_TEMPORARILY_UNAVAILABLE,
	    NUTAG_WITH(current),
	    TAG_END());
    return 1;
  }
  return 0;
}


int test_basic_call(struct context *ctx)
{
  return 0
    || test_basic_call_1(ctx)
    || test_basic_call_2(ctx)
    || test_basic_call_3(ctx)
    || test_basic_call_4(ctx)
    || test_basic_call_5(ctx)
    || test_basic_call_6(ctx)
    || test_basic_call_7(ctx)
    || test_video_call_1(ctx)
    ;
}
