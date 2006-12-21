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

int accept_call_immediately(CONDITION_PARAMS);

/* ======================================================================== */
/* NUA-9 tests: REFER */

int test_refer0(struct context *ctx, int refer_with_id, char const *tests);
int test_refer1(struct context *ctx, int refer_with_id, char const *tests);

int test_refer(struct context *ctx)
{
  /* test twice, once without id and once with id */
  return
    test_refer0(ctx, 0, "NUA-9.1") ||
    test_refer0(ctx, 1, "NUA-9.2") ||
    test_refer1(ctx, 0, "NUA-9.3");
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

int test_refer0(struct context *ctx, int refer_with_id, char const *tests)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct call *a_call = a->call, *b_call = b->call, *c_call = c->call;
  struct call *a_c2;
  struct event *e;
  sip_t const *sip;
  sip_event_t const *a_event, *b_event;
  sip_refer_to_t const *refer_to;
  sip_referred_by_t const *referred_by;

  sip_refer_to_t r0[1];
  sip_to_t to[1];

  su_home_t tmphome[SU_HOME_AUTO_SIZE(16384)];

  su_home_auto(tmphome, sizeof(tmphome));

  if (print_headings)
    printf("TEST %s: REFER: refer A to C\n", tests);

  if (print_headings)
    printf("TEST %s.1: REFER: make a call between A and B\n", tests);

  /* Do (not) include id with first implicit Event: refer */
  nua_set_params(ctx->a.nua, NUTAG_REFER_WITH_ID(refer_with_id), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

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
  /* REFER (initial NOTIFY is no more sent)
   A                    B
   |<------REFER--------|
   |-------200 OK------>|
  [|-------NOTIFY------>|]			|
  [|<------200 OK-------|]			|
   */

  if (print_headings)
    printf("TEST %s.2: refer A to C\n", tests);

  *sip_refer_to_init(r0)->r_url = *c->contact->m_url;
  r0->r_url->url_headers = "subject=referred";
  r0->r_display = "C";

  REFER(b, b_call, b_call->nh, SIPTAG_REFER_TO(r0), TAG_END());
  run_ab_until(ctx, -1, save_until_received,
	       -1, save_until_final_response);

  /*
    Events in A:
    nua_i_refer
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_refer);
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
  if (e->next) {
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(!e->next);
  }
  free_events_in_list(ctx, a->events);

  /*
     Events in B after nua_refer():
     nua_r_refer
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 100);
  TEST(tl_gets(e->data->e_tags,
	       NUTAG_REFER_EVENT_REF(b_event),
	       TAG_END()), 1);
  TEST_1(b_event); TEST_1(b_event->o_id);
  TEST_1(b_event = sip_event_dup(tmphome, b_event));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 202);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_SIZE(strtoul(b_event->o_id, NULL, 10), sip->sip_cseq->cs_seq);
#if 0
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
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_S(sip->sip_payload->pl_data, "SIP/2.0 100 Trying\r\n");
  TEST_1(!e->next);
#endif
  free_events_in_list(ctx, b->events);

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
  TEST_S(sip->sip_payload->pl_data, "SIP/2.0 100 Trying\r\n");
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

  a->call->next = a_c2;

  TEST_1(a_c2->nh = nua_handle(a->nua, a_c2, SIPTAG_TO(to), TAG_END()));

  INVITE(a, a_c2, a_c2->nh, /* NUTAG_URL(refer_to->r_url), */
	 NUTAG_REFER_EVENT(a_event),
	 NUTAG_NOTIFY_REFER(a_call->nh),
	 SOATAG_USER_SDP_STR(a_c2->sdp),
	 SIPTAG_REFERRED_BY(referred_by),
	 TAG_END());

  run_abc_until(ctx,
		-1, until_ready,
		-1, save_until_received,
		-1, accept_call_immediately);
  /* XXX - we should use accept_call instead of accept_call_immediately but
     nua has a problem with automatically generated NOTIFYs:
     3rd NOTIFY is not sent because 2nd is still in progress
  */

  /* Client A transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2a+C4)-> READY: nua_r_invite, nua_i_state
     nua_i_notify

     XXX should be:
     CALLING -(C2+C4)-> PROCEEDING: nua_r_invite, nua_i_state
     optional: nua_i_notify
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
     nua_i_notify
     optional: nua_i_notify
  */
  TEST_1(e = a_c2->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, a_c2->events);

  if (a->events->head == NULL)
    run_a_until(ctx, -1, save_until_received);
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /*
     Events in B after nua_refer():
     nua_i_notify
  */
  if (b->events->head == NULL)
    run_b_until(ctx, -1, save_until_received);
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_S(sip->sip_payload->pl_data, "SIP/2.0 200 OK\r\n");
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /*
   C transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
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

  BYE(a, a_call, a_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

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

  nua_handle_destroy(a_c2->nh), a_c2->nh = NULL;
  a->call->next = NULL; free(a_c2);

  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  if (print_headings)
    printf("TEST %s: PASSED\n", tests);

  su_home_deinit(tmphome);

  END();
}


/*
 accept_call_immediately
                      X
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
*/
int accept_call_immediately(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
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

    pl = sip_payload_format(NULL, "SIP/2.0 %u %s\r\n", 
			    st->st_status, st->st_phrase);

    NOTIFY(ep, ep->call, ep->call->nh,
	   SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
	   SIPTAG_PAYLOAD(pl),
	   TAG_IF(st->st_status >= 200,
		  NUTAG_SUBSTATE(nua_substate_terminated)),
	   TAG_END());

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

/* Referred call - NOTIFY and BYE are overlapped

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
   |			|			|
   |<------------------200----------------------|
   |-------NOTIFY------>|			|
   |--------BYE-------->|			|
   |-------------------ACK--------------------->|
   |<------200 OK-------|			|
   |<------200 OK-------|			|
   |			X			|
   |			 			|
   |-------------------BYE--------------------->|
   |<------------------200----------------------|
   |						|

*/

int test_refer1(struct context *ctx, int refer_with_id, char const *tests)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b, *c = &ctx->c;
  struct call *a_call = a->call, *b_call = b->call, *c_call = c->call;
  struct call *a_c2;
  struct event *e;
  sip_t const *sip;
  sip_event_t const *a_event, *b_event;
  sip_refer_to_t const *refer_to;
  sip_referred_by_t const *referred_by;

  sip_refer_to_t r0[1];
  sip_to_t to[1];

  su_home_t tmphome[SU_HOME_AUTO_SIZE(16384)];

  su_home_auto(tmphome, sizeof(tmphome));

  if (print_headings)
    printf("TEST %s: REFER: refer A to C\n", tests);

  if (print_headings)
    printf("TEST %s.1: REFER: make a call between A and B\n", tests);

  /* Do (not) include id with first implicit Event: refer */
  nua_set_params(ctx->a.nua, NUTAG_REFER_WITH_ID(refer_with_id), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

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
  /* REFER (initial NOTIFY is no more sent)
   A                    B
   |<------REFER--------|
   |-------200 OK------>|
  [|-------NOTIFY------>|]			|
  [|<------200 OK-------|]			|
   */

  if (print_headings)
    printf("TEST %s.2: refer A to C\n", tests);

  *sip_refer_to_init(r0)->r_url = *c->contact->m_url;
  r0->r_url->url_headers = "subject=referred";
  r0->r_display = "C";

  REFER(b, b_call, b_call->nh, SIPTAG_REFER_TO(r0), TAG_END());
  run_ab_until(ctx, -1, save_until_received,
	       -1, save_until_final_response);

  /*
    Events in A:
    nua_i_refer
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_refer);
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
  if (e->next) {
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(!e->next);
  }
  free_events_in_list(ctx, a->events);

  /*
     Events in B after nua_refer():
     nua_r_refer
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 100);
  TEST(tl_gets(e->data->e_tags,
	       NUTAG_REFER_EVENT_REF(b_event),
	       TAG_END()), 1);
  TEST_1(b_event); TEST_1(b_event->o_id);
  TEST_1(b_event = sip_event_dup(tmphome, b_event));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_refer);
  TEST(e->data->e_status, 202);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_SIZE(strtoul(b_event->o_id, NULL, 10), sip->sip_cseq->cs_seq);
#if 0
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
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_S(sip->sip_payload->pl_data, "SIP/2.0 100 Trying\r\n");
  TEST_1(!e->next);
#endif
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST %s.2: PASSED\n", tests);


  /* ---------------------------------------------------------------------- */
  /*
   A                    B                       C
   |			|			|
   |-----------------INVITE-------------------->|
   |			|			|
  XXX			|			|
   |			|			|
   |<------------------200----------------------|
   |-------NOTIFY------>|			|
   |---------BYE------->|			|
   |-------------------ACK--------------------->|
   |<--------200--------|			|
   |<------200 OK-------|			|
   |                    X
     */

  if (print_headings)
    printf("TEST %s.4: A invites C\n", tests);

  *sip_to_init(to)->a_url = *refer_to->r_url;
  to->a_display = refer_to->r_display;

  a->call->next = a_c2;

  TEST_1(a_c2->nh = nua_handle(a->nua, a_c2, SIPTAG_TO(to), TAG_END()));

  INVITE(a, a_c2, a_c2->nh, /* NUTAG_URL(refer_to->r_url), */
	 NUTAG_REFER_EVENT(a_event),
	 /* NUTAG_NOTIFY_REFER(a_call->nh), */
	 SOATAG_USER_SDP_STR(a_c2->sdp),
	 SIPTAG_REFERRED_BY(referred_by),
	 TAG_END());

  run_abc_until(ctx,
		-1, notify_until_terminated,
		-1, until_terminated,
		-1, accept_call_immediately);

  /* XXX - we should use accept_call instead of accept_call_immediately but
     nua has a problem with automatically generated NOTIFYs:
     3rd NOTIFY is not sent because 2nd is still in progress
  */

  /* Client A transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2a+C4)-> READY: nua_r_invite, nua_i_state
     nua_r_notify

     Transitions of first call:
     READY --(T2)--> TERMINATING: nua_bye()
     TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state

     XXX should be:
     CALLING -(C2+C4)-> PROCEEDING: nua_r_invite, nua_i_state
     optional: nua_i_notify
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
     nua_i_notify
     optional: nua_i_notify
  */
  TEST_1(e = a_c2->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, a_c2->events);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /*
     Events in B after nua_refer():
     nua_i_notify
     READY -(T1)-> TERMINATED: nua_i_bye, nua_i_state
  */
  if (b->events->head == NULL)
    run_b_until(ctx, -1, save_until_received);
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_S(sip->sip_payload->pl_data, "SIP/2.0 200 OK\r\n");
  TEST_1(sip->sip_event);
  if (refer_with_id)
    TEST_S(sip->sip_event->o_id, b_event->o_id);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  /*
   C transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S3b)-> COMPLETED: nua_respond(), nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
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

  nua_handle_destroy(a_c2->nh), a_c2->nh = NULL;
  a->call->next = NULL; free(a_c2);

  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  if (print_headings)
    printf("TEST %s: PASSED\n", tests);

  su_home_deinit(tmphome);

  END();
}
