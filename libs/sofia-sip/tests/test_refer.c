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

/**@CFILE test_nua_re_invite.c
 * @brief Test re_inviteing, outbound, nat traversal.
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
#define __func__ "test_call_hold"
#endif

/* ======================================================================== */
/* NUA-9 tests: REFER */

int test_refer0(struct context *ctx, char const *tests,
		int refer_with_id, int notify_by_appl);
int notify_until_terminated(CONDITION_PARAMS);
int test_challenge_refer(struct context *ctx);

int test_refer(struct context *ctx)
{
  /* test twice, once without id and once with id */
  return
    test_challenge_refer(ctx) ||
    test_refer0(ctx, "NUA-9.1", 0, 0) ||
    test_refer0(ctx, "NUA-9.2", 1, 0) ||
    test_refer0(ctx, "NUA-9.3", 0, 1) ||
    test_refer0(ctx, "NUA-9.4", 1, 1);
}

/* Referred call:

   A			B
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<------REFER--------|
   |-------200 OK------>|			C
  [|-------NOTIFY------>|]			|
  [|<------200 OK-------|]			|
   |			|			|
   |			|			|
   |<-----SUBSCRIBE-----|                       |
   |-------200 OK------>|			|
   |			|			|
   |			|			|
   |-----------------INVITE-------------------->|
   |			|			|
   |<------------------180----------------------|
   |-------NOTIFY------>|			|
   |<------200 OK-------|			|
   |			|			|
   |<------------------200----------------------|
   |-------NOTIFY------>|			|
   |<------200 OK-------|			|
   |-------------------ACK--------------------->|
   |			|			|
   |--------BYE-------->|			|
   |<------200 OK-------|			|
   |			X			|
   |			 			|
   |-------------------BYE--------------------->|
   |<------------------200----------------------|
   |						|

*/

int test_refer0(struct context *ctx, char const *tests,
		int refer_with_id, int notify_by_appl)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct call *a_call = a->call, *b_call = b->call, *c_call = c->call;
  struct call *a_refer, *a_c2, *b_refer;
  struct eventlist *a_revents, *b_revents;
  struct event *e, *notify_e;
  sip_t const *sip;
  sip_event_t const *a_event, *b_event;
  sip_refer_to_t const *refer_to;
  sip_referred_by_t const *referred_by;

  sip_refer_to_t r0[1];
  sip_to_t to[1];

  su_home_t tmphome[SU_HOME_AUTO_SIZE(16384)];

  su_home_auto(tmphome, sizeof(tmphome));

  if (print_headings)
    printf("TEST %s: REFER: refer A to C%s%s%s\n", tests,
	   refer_with_id ? " with Event id" : "",
	   refer_with_id && !notify_by_appl ? " and" : "",
	   !notify_by_appl ? " nua generating the NOTIFYs" : "");

  if (print_headings)
    printf("TEST %s.1: REFER: make a call between A and B\n", tests);

  /* Do (not) include id with first implicit Event: refer */
  nua_set_params(ctx->a.nua, NUTAG_REFER_WITH_ID(refer_with_id), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  if (refer_with_id) {
    TEST_1(a_refer = calloc(1, (sizeof *a_refer) + (sizeof *a_refer->events)));
    call_init(a_refer);
    a_refer->events = (void *)(a_refer + 1);
    eventlist_init(a_refer->events);

    a_call->next = a_refer;
    a_revents = a_refer->events;

    TEST_1(b_refer = calloc(1, (sizeof *b_refer) + (sizeof *b_refer->events)));
    call_init(b_refer);
    b_refer->events = (void *)(b_refer + 1);
    eventlist_init(b_refer->events);

    b_call->next = b_refer;
    b_revents = b_refer->events;
  }
  else {
    a_refer = a_call, b_refer = b_call;
    a_revents = a->events, b_revents = b->events;
  }

  TEST_1(a_c2 = calloc(1, (sizeof *a_c2) + (sizeof *a_c2->events)));
  call_init(a_c2);
  a_c2->events = (void *)(a_c2 + 1);
  eventlist_init(a_c2->events);

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";
  a_c2->sdp   = "m=audio 5012 RTP/AVP 8";
  c_call->sdp = "m=audio 5014 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST %s.1: PASSED\n", tests);

  /* ---------------------------------------------------------------------- */
  /* REFER (initial NOTIFY is no more sent unless REFER creates a new dialog)
   A                    B
   |<------REFER--------|
   |-------200 OK------>|
  [|-------NOTIFY------>|]			|
  [|<------200 OK-------|]			|
   */

  if (print_headings)
    printf("TEST %s.2: B refers A to C\n", tests);

  if (b_refer != b_call)
    TEST_1(b_refer->nh =
	   nua_handle(b->nua, b_refer, SIPTAG_TO(a->to), TAG_END()));

  *sip_refer_to_init(r0)->r_url = *c->contact->m_url;
  r0->r_url->url_headers = "subject=referred";
  r0->r_display = "C";

  REFER(b, b_refer, b_refer->nh, SIPTAG_REFER_TO(r0),
	TAG_IF(!ctx->proxy_tests && b_refer != b_call,
	       NUTAG_URL(a->contact->m_url)),
	TAG_END());
  run_ab_until(ctx, -1, save_until_received,
	       -1, save_until_final_response);

  /*
    Events in A:
    nua_i_refer
  */
  TEST_1(e = a_revents->head); TEST_E(e->data->e_event, nua_i_refer);
  TEST(e->data->e_status, 202);
  a_event = NULL;
  TEST(tl_gets(e->data->e_tags,
	       NUTAG_REFER_EVENT_REF(a_event),
	       TAG_END()), 1);
  TEST_1(a_event); TEST_1(a_event = sip_event_dup(tmphome, a_event));
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_refer_to);
  TEST_1(refer_to = sip_refer_to_dup(tmphome, sip->sip_refer_to));
  TEST_1(sip->sip_referred_by);
  TEST_1(referred_by = sip_referred_by_dup(tmphome, sip->sip_referred_by));

  /*
     Events in B after nua_refer():
     nua_r_refer
  */
  TEST_1(e = b_revents->head); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 100);
  TEST(tl_gets(e->data->e_tags,
	       NUTAG_REFER_EVENT_REF(b_event),
	       TAG_END()), 1);
  TEST_1(b_event); TEST_1(b_event->o_id);
  TEST_1(b_event = sip_event_dup(tmphome, b_event));

  notify_e = NULL;

  TEST_1(e = e->next);
  if (e->data->e_event == nua_i_notify) {
    notify_e = e;
    TEST_1(e = e->next);
  }
  TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 202);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_SIZE(strtoul(b_event->o_id, NULL, 10), sip->sip_cseq->cs_seq);

  if (a_refer != a_call) {
    while (!notify_e) {
      for (e = b_revents->head; e; e = e->next) {
	if (e->data->e_event == nua_i_notify) {
	  notify_e = e;
	  break;
	}
      }
      if (!notify_e)
	run_ab_until(ctx, -1, save_until_received, nua_i_notify, save_events);
    }

    if (a_revents->head->next == NULL)
      run_a_until(ctx, -1, save_until_received);

    TEST_1(e = a_revents->head->next); TEST_E(e->data->e_event, nua_r_notify);
    TEST_1(!e->next);

    TEST_1(e = notify_e);
    TEST_E(e->data->e_event, nua_i_notify);
    TEST(e->data->e_status, 200);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_event);
    if (refer_with_id)
      TEST_S(sip->sip_event->o_id, b_event->o_id);
    TEST_1(sip->sip_subscription_state);
    TEST_S(sip->sip_subscription_state->ss_substate, "pending");
    TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
    TEST_M(sip->sip_payload->pl_data, "SIP/2.0 100 Trying\r\n",
	   sip->sip_payload->pl_len);
  }

  free_events_in_list(ctx, a_revents);
  free_events_in_list(ctx, b_revents);

  if (print_headings)
    printf("TEST %s.2: PASSED\n", tests);

#if 0
  /* ---------------------------------------------------------------------- */
  /*
   A                    B
   |<-----SUBSCRIBE-----|
   |-------200 OK------>|
   |-------NOTIFY------>|			|
   |<------200 OK-------|			|
   */

  if (print_headings)
    printf("TEST %s.3: extend expiration time for implied subscription\n", tests);

  SUBSCRIBE(b, b_call, b_call->nh,
	    SIPTAG_EVENT(b_event),
	    SIPTAG_EXPIRES_STR("3600"),
	    TAG_END());
  run_ab_until(ctx, -1, save_until_final_response,
	       -1, save_until_final_response);

  /*
    Events in A:
    nua_i_subscribe, nua_r_notify
  */
  TEST_1(e = a->events->head);
  if (e->data->e_event == nua_r_notify)
    TEST_1(e = e->next);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST(e->data->e_status, 202);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /*
     Events in B after nua_subscribe():
     nua_r_subscribe, nua_i_notify
  */
  TEST_1(e = b->events->head);
  if (e->data->e_event == nua_i_notify) {
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_M(sip->sip_payload->pl_data, "SIP/2.0 100 Trying\r\n",
	 sip->sip_payload->pl_len);
  TEST_1(e = e->next);
  }
  TEST_E(e->data->e_event, nua_r_subscribe);
  TEST(e->data->e_status, 202);
  if (!e->next)
    run_b_until(ctx, -1, save_until_received);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST %s.3: PASSED\n", tests);
#endif

  /* ---------------------------------------------------------------------- */
  /*
   A                    B                       C
   |			|			|
   |-----------------INVITE-------------------->|
   |			|			|
  XXX			|			|
   | 			|			|
   |<------------------180----------------------|
   |-------NOTIFY------>|			|
   |<------200 OK-------|			|
   | 			|			|
  XXX			|			|
   |			|			|
   |<------------------200----------------------|
   |-------NOTIFY------>|			|
   |<------200 OK-------|			|
   |-------------------ACK--------------------->|
   */

  if (print_headings)
    printf("TEST %s.4: A invites C\n", tests);

  *sip_to_init(to)->a_url = *refer_to->r_url;
  to->a_display = refer_to->r_display;

  a_refer->next = a_c2;

  TEST_1(a_c2->nh = nua_handle(a->nua, a_c2, SIPTAG_TO(to), TAG_END()));

  INVITE(a, a_c2, a_c2->nh, /* NUTAG_URL(refer_to->r_url), */
	 TAG_IF(!notify_by_appl, NUTAG_REFER_EVENT(a_event)),
	 TAG_IF(!notify_by_appl, NUTAG_NOTIFY_REFER(a_refer->nh)),
	 SOATAG_USER_SDP_STR(a_c2->sdp),
	 SIPTAG_REFERRED_BY(referred_by),
	 TAG_END());

  run_abc_until(ctx,
		-1, notify_by_appl ? notify_until_terminated : until_ready,
		-1, save_until_received,
		-1, accept_call);

  /* Wait until both NOTIFY has been responded */
  while (a_revents->head == NULL || a_revents->head->next == NULL)
    run_ab_until(ctx, -1, save_until_received, -1, save_events);
  while (b_revents->head == NULL || b_revents->head->next == NULL)
    run_ab_until(ctx, -1, save_events, -1, save_until_received);

  /* Client A transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2+C4)-> PROCEEDING: nua_r_invite, nua_i_state
     nua_r_notify
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
     nua_r_notify
  */
  TEST_1(e = a_c2->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, a_c2->events);

  TEST_1(e = a_revents->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  if (a_refer == a_call && notify_by_appl) {
    free_event_in_list(ctx, a_revents, a_revents->head);
    free_event_in_list(ctx, a_revents, a_revents->head);
  }
  else {
    TEST_1(!e->next);
    free_events_in_list(ctx, a_revents);
  }

  /*
     Events in B after nua_refer():
     nua_i_notify
  */
  TEST_1(e = b_revents->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_M(sip->sip_payload->pl_data, "SIP/2.0 180 Ringing\r\n", sip->sip_payload->pl_len);
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_M(sip->sip_payload->pl_data, "SIP/2.0 200 OK\r\n", sip->sip_payload->pl_len);
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  if (b_refer == b_call && notify_by_appl) {
    free_event_in_list(ctx, b_revents, b_revents->head);
    free_event_in_list(ctx, b_revents, b_revents->head);
  }
  else {
    TEST_1(!e->next);
    free_events_in_list(ctx, b_revents);
  }

  /*
   C transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  if (print_headings)
    printf("TEST %s.4: PASSED\n", tests);

  /* ---------------------------------------------------------------------- */
  /*
 A                    B
 |---------BYE------->|
 |<--------200--------|
   */

  if (print_headings)
    printf("TEST %s.5.1: terminate call between A and B\n", tests);

  if (notify_by_appl) {
    if (!a->events->head || !a->events->head->next)
      run_ab_until(ctx, -1, until_terminated, -1, save_events);
    if (!b->events->head || !b->events->head->next)
      run_ab_until(ctx, -1, save_events, -1, until_terminated);
  }
  else {
    BYE(a, a_call, a_call->nh, TAG_END());
    run_ab_until(ctx, -1, until_terminated, -1, until_terminated);
  }

  /*
    Transitions of A:
    READY --(T2)--> TERMINATING: nua_bye()
    TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /* Transitions of B:
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST %s.5.1: PASSED\n", tests);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;


  /* ---------------------------------------------------------------------- */
  /*
   A                                            C
   |-------------------BYE--------------------->|
   |<------------------200----------------------|
   */

  if (print_headings)
    printf("TEST %s.5.2: terminate call between A and C\n", tests);

  BYE(a, a_c2, a_c2->nh, TAG_END());
  run_abc_until(ctx, -1, until_terminated, -1, NULL, -1, until_terminated);

  /*
   Transitions of A:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = a_c2->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, a_c2->events);

  /* Transitions of B:
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, c->events);

  if (print_headings)
    printf("TEST %s.5.2: PASSED\n", tests);

  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  nua_handle_destroy(a_c2->nh), a_c2->nh = NULL;
  a_refer->next = NULL; free(a_c2);

  if (a_refer != a_call) {
    nua_handle_destroy(a_refer->nh), a_refer->nh = NULL;
    a_call->next = NULL; free(a_refer);
  }

  if (b_refer != b_call) {
    nua_handle_destroy(b_refer->nh), b_refer->nh = NULL;
    b_call->next = NULL; free(b_refer);
  }

  if (print_headings)
    printf("TEST %s: PASSED\n", tests);

  su_home_deinit(tmphome);

  END();
}


/*
 X      INVITE
 |                    |
 |-----------------INVITE-------------------->|
 |                    |                       |
 |                    |                       |
 |<------------------200----------------------|
 |-------NOTIFY------>|			      |
 |--------BYE-------->|			      |
 |-------------------ACK--------------------->|

*/
int notify_until_terminated(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite) {
    sip_status_t *st = sip->sip_status;
    sip_payload_t *pl;
    struct call *r_call;

    if (!nua_handle_has_events(ep->call->nh))
      r_call = ep->call->next;
    else
      r_call = ep->call;

    assert(nua_handle_has_events(r_call->nh));

    pl = sip_payload_format(NULL, "SIP/2.0 %u %s\r\n",
			    st->st_status, st->st_phrase);

    NOTIFY(ep, r_call, r_call->nh,
	   SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
	   SIPTAG_PAYLOAD(pl),
	   NUTAG_SUBSTATE(st->st_status >= 200
			  ? nua_substate_terminated
			  : nua_substate_active),
	   TAG_END());

    su_free(NULL, pl);

    if (st->st_status >= 200)
      BYE(ep, ep->call, ep->call->nh, TAG_END());

    return 0;
  }

  if (call != ep->call)
    return 0;

  switch (callstate(tags)) {
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}

int authenticate_refer(CONDITION_PARAMS);
int reject_refer_after_notified(CONDITION_PARAMS);

int test_challenge_refer(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b, *c = &ctx->c;
  struct call *a_call = a->call, *c_call = c->call;
  struct event *e;
  sip_t const *sip;

  sip_refer_to_t r0[1];

  if (!ctx->proxy_tests)
    return 0;

  if (print_headings)
    printf("TEST NUA-9.0.1: challenge REFER\n");

  nua_set_params(ctx->a.nua, NUTAG_APPL_METHOD("REFER"), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  *sip_refer_to_init(r0)->r_url = *b->contact->m_url;
  r0->r_url->url_headers = "subject=referred";
  r0->r_display = "B";

  TEST_1(c_call->nh = nua_handle(c->nua, c_call, SIPTAG_TO(a->to), TAG_END()));

  REFER(c, c_call, c_call->nh,
	SIPTAG_FROM(c->to),
	SIPTAG_REFER_TO(r0),
	TAG_END());

  run_abc_until(ctx, -1, reject_refer_after_notified, -1, NULL, -1, authenticate_refer);

  /*
    Events in A:
    nua_i_refer
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_refer);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_refer_to);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);
  /*
     Events in C after nua_refer():
     nua_r_refer
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 407);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 100);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 480);

  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, c->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  nua_set_params(ctx->a.nua,
		 NUTAG_APPL_METHOD(NULL),
		 NUTAG_APPL_METHOD("INVITE, REGISTER, PUBLISH, SUBSCRIBE"),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  if (print_headings)
    printf("TEST NUA-9.0.1: PASSED\n");

  END();
}

int reject_refer_after_notified(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_refer) {
  }

  if (event == nua_r_notify) {
    /* Respond to refer only after initial notify has been responded */
    struct eventlist *list;
    struct event *e;

    if (call->events)
      list = call->events;
    else
      list = ep->events;

    for (e = list->head; e; e = e->next)
      if (e->data->e_event == nua_i_refer)
	break;

    if (e) {
      RESPOND(ep, call, nh, SIP_480_TEMPORARILY_UNAVAILABLE,
	      NUTAG_WITH(e->data->e_msg),
	      TAG_END());
      return 1;
    }
    return 0;
  }

  return 0;
}

int authenticate_refer(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (status == 401 || status == 407) {
    AUTHENTICATE(ep, call, nh,
		 NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"),
		 TAG_END());
  }

  return status == 480;
}
