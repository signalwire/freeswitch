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

/**@CFILE test_session_timer.c
 * @brief NUA-8 tests: Session timer, UPDATE
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
#define __func__ "test_session_timer"
#endif

/* ======================================================================== */

static size_t remove_update(void *a, void *message, size_t len);

int test_session_timer(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

/* Session timer test:

   A	      P		B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |------------------->|
   |			|
   |			|
   |--INVITE->| 	|
   |<--422----|		|
   |---ACK--->|		|

*/

  if (print_headings)
    printf("TEST NUA-8.1.1: Session timers\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  /* We negotiate session timer of 6 second */
  /* Disable session timer from proxy */
  test_proxy_set_session_timer(ctx->p, 0, 0);

  nua_set_params(ctx->b.nua,
		 NUTAG_SESSION_REFRESHER(nua_any_refresher),
		 NUTAG_MIN_SE(1),
		 NUTAG_SESSION_TIMER(6),
		 NTATAG_SIP_T1X64(8000),
		 TAG_END());

  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_i_state
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
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);
  TEST_S(sip->sip_session_expires->x_refresher, "uas");
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
    printf("TEST NUA-8.1.1: PASSED\n");

  if (ctx->expensive) {
    nua_set_hparams(a_call->nh,
		    NUTAG_SUPPORTED("timer"),
		    NUTAG_MIN_SE(1),
		    NUTAG_SESSION_TIMER(5),
		    TAG_END());
    run_a_until(ctx, nua_r_set_params, until_final_response);

    if (print_headings)
      printf("TEST NUA-8.1.2: Wait for refresh using INVITE\n");

    run_ab_until(ctx, -1, until_ready, -1, until_ready);

    free_events_in_list(ctx, a->events);
    free_events_in_list(ctx, b->events);

    if (print_headings)
      printf("TEST NUA-8.1.2: PASSED\n");

    nua_set_hparams(b_call->nh,
		    NUTAG_UPDATE_REFRESH(1),
		    TAG_END());
    run_b_until(ctx, nua_r_set_params, until_final_response);

    if (print_headings)
      printf("TEST NUA-8.1.3: Wait for refresh using UPDATE\n");

    run_ab_until(ctx, -1, until_ready, -1, until_ready);

    TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_update);

    free_events_in_list(ctx, a->events);
    free_events_in_list(ctx, b->events);

    if (print_headings)
      printf("TEST NUA-8.1.3: PASSED\n");

    if (ctx->nat) {
      struct nat_filter *f;

      if (print_headings)
	printf("TEST NUA-8.1.4: filter UPDATE, wait until session expires\n");

      f = test_nat_add_filter(ctx->nat, remove_update, NULL, nat_inbound);
      TEST_1(f);

      run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

      TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_bye);

      free_events_in_list(ctx, a->events);
      free_events_in_list(ctx, b->events);

      nua_handle_destroy(a_call->nh), a_call->nh = NULL;
      nua_handle_destroy(b_call->nh), b_call->nh = NULL;

      test_nat_remove_filter(ctx->nat, f);

      if (print_headings)
	printf("TEST NUA-8.1.4: PASSED\n");
    }
  }

  if (b_call->nh) {
    if (print_headings)
      printf("TEST NUA-8.1.9: Terminate first session timer call\n");

    BYE(b, b_call, b_call->nh, TAG_END());
    run_ab_until(ctx, -1, until_terminated, -1, until_terminated);
    free_events_in_list(ctx, a->events);
    free_events_in_list(ctx, b->events);

    nua_handle_destroy(a_call->nh), a_call->nh = NULL;
    nua_handle_destroy(b_call->nh), b_call->nh = NULL;

    if (print_headings)
      printf("TEST NUA-8.1.9: PASSED\n");
  }

  /*
   |			|
   |-------INVITE------>|
   |<-------422---------|
   |--------ACK-------->|
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |-------200 OK-------|
   |			|
  */

  if (print_headings)
    printf("TEST NUA-8.2: Session timers negotiation\n");

  test_proxy_set_session_timer(ctx->p, 180, 90);

  nua_set_params(a->nua,
		 NUTAG_SUPPORTED("timer"),
		 TAG_END());

  run_a_until(ctx, nua_r_set_params, until_final_response);

  nua_set_params(b->nua,
		 NUTAG_AUTOANSWER(0),
		 NUTAG_MIN_SE(120),
		 NTATAG_SIP_T1X64(2000),
		 TAG_END());

  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to),
				 TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_SUPPORTED_STR("100rel, timer"),
	 NUTAG_SESSION_TIMER(4),
	 NUTAG_MIN_SE(3),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C6a)-> (TERMINATED/INIT): nua_r_invite
     when testing with proxy, second 422 when call reaches UAS:
       (INIT) -(C1)-> CALLING: nua_i_state
       CALLING -(C6a)-> (TERMINATED/INIT): nua_r_invite
     (INIT) -(C1)-> CALLING: nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
     PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 422);

  if (ctx->proxy_tests) {
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
    TEST_1(is_offer_sent(e->data->e_tags));
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
    TEST(e->data->e_status, 100);
    TEST(sip_object(e->data->e_msg)->sip_status->st_status, 422);
  }

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);
  TEST(sip->sip_session_expires->x_delta, 120);
  TEST_S(sip->sip_session_expires->x_refresher, "uac");
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
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);
  TEST_1(!sip->sip_session_expires->x_refresher);
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
    printf("TEST NUA-8.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-8.3: UPDATE with session timer headers\n");

  UPDATE(b, b_call, b_call->nh, TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  /* Events from B (who sent UPDATE) */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  if (!e->next)
    run_b_until(ctx, -1, until_ready);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_update);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  /* Events from A (who received UPDATE) */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_update);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_session_expires);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

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
    printf("TEST NUA-8.3: PASSED\n");

  END();
}

static
size_t remove_update(void *a, void *message, size_t len)
{
  (void)a;

  if (strncmp("UPDATE ", message, 7) == 0) {
    return 0;
  }

  return len;
}

