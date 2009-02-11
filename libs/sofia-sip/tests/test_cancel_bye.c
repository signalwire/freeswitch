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

/**@CFILE test_cancel_bye.c
 * @brief Test CANCEL, weird BYE and handle destroy
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
#define __func__ "test_cancel_bye"
#endif

/* ======================================================================== */

/* Cancel cases:


   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |------CANCEL------->|
   |<------200 OK-------|
   |			|
   |<-------487---------|
   |--------ACK-------->|
   |			|
   |			|

   Client transitions:
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED

   Server transitions:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |------CANCEL------->|
   |<------200 OK-------|
   |			|
   |<-------487---------|
   |--------ACK-------->|
   |			|
   |			|

   Client transitions:
   INIT -(C1)-> CALLING -(C2)-> PROCEEDING -(C6b)-> TERMINATED

   Server transitions:
   INIT -(S1)-> RECEIVED -(S2a)-> EARLY -(S6b)-> TERMINATED

*/

int cancel_when_calling(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_calling:
    CANCEL(ep, call, nh,
	   /* sf.net bug #173323 */
	   SIPTAG_CALL_ID_STR("non-existing-call-id"),
	   TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}

int bye_when_calling(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_calling:
    BYE(ep, call, nh, TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}


int cancel_when_ringing(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_proceeding:
    CANCEL(ep, call, nh, TAG_END());
    return 0;
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}


int alert_call(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}

int accept_after_183(CONDITION_PARAMS);

int test_call_cancel(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  if (print_headings)
    printf("TEST NUA-5.1.1: cancel call\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, cancel_when_calling, -1, until_terminated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state, nua_cancel()
     CALLING -(C6a)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 487);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S6a)--> TERMINATED: nua_i_cancel, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.1.1: PASSED\n");

  /* ------------------------------------------------------------------------ */

  if (print_headings)
    printf("TEST NUA-5.1.2: cancel call (with nua_bye())\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("TEST NUA-5.1.2"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, bye_when_calling, -1, until_terminated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state, nua_cancel()
     CALLING -(C6a)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 487);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S6a)--> TERMINATED: nua_i_cancel, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.1.2: PASSED\n");

 /* ----------------------------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-5.2.1: cancel call when ringing\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 /*SIPTAG_REJECT_CONTACT_STR("*;audio=FALSE"),*/
	 TAG_END());

  run_ab_until(ctx, -1, cancel_when_ringing, -1, alert_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite(180, nua_i_state, nua_cancel()
     PROCEEDING -(C6b)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 487);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(180), nua_i_state
   EARLY -(S6b)--> TERMINATED: nua_i_cancel, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST(e->data->e_status, 200);
  /* Check for bug #1326727 */
  TEST_1(e->data->e_msg);
#if 0
  TEST_1(sip_object(e->data->e_msg)->sip_reject_contact);
  TEST_1(sip_object(e->data->e_msg)->sip_reject_contact->cp_params &&
	 sip_object(e->data->e_msg)->sip_reject_contact->cp_params[0]);
  TEST_S(sip_object(e->data->e_msg)->sip_reject_contact->cp_params[0],
	 "audio=FALSE");
#endif
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.2.1: PASSED\n");

  /* ------------------------------------------------------------------------ */
  if (print_headings)
    printf("TEST NUA-5.2.2: CANCEL call when server waits for PRACK\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_APPL_METHOD("PRACK"),
	 /*SIPTAG_REJECT_CONTACT_STR("*;audio=FALSE"),*/
	 TAG_END());

  run_ab_until(ctx, -1, cancel_when_ringing, -1, accept_after_183);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.2.2: PASSED\n");


  END();
}

int accept_after_183(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_183_SESSION_PROGRESS, TAG_END());
    RESPOND(ep, call, nh, SIP_200_OK, TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}


/* ======================================================================== */
/* Destroy call handle */

int destroy_when_calling(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_calling:
    DESTROY(ep, call, nh);
    return 1;
  default:
    return 0;
  }
}

int destroy_when_completing(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completing:
    DESTROY(ep, call, nh);
    return 1;
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

int test_call_destroy_1(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-5.3: destroy when calling\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, destroy_when_calling, -1, until_terminated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), ...
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S6a)--> TERMINATED: nua_i_cancel, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.3: PASSED\n");

  END();
}

int accept_until_terminated(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_completed:
  case nua_callstate_ready:
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_call_destroy_2(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-5.4: destroy when completing\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_AUTOACK(0),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, destroy_when_completing, -1, accept_until_terminated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), ...
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.4: PASSED\n");

  END();
}

int destroy_when_early(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    if (call)
      DESTROY(ep, call, nh), call->nh = NULL;
    return 1;
  case nua_callstate_completed:
  case nua_callstate_ready:
  case nua_callstate_terminated:
    if (call)
      DESTROY(ep, call, nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_call_destroy_3(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-5.5: destroy when early\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_AUTOACK(0),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, destroy_when_early);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), ...
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 480);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state ... DESTROY
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.5: PASSED\n");

  END();
}

int destroy_when_completed(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
    return 0;
  case nua_callstate_completed:
  case nua_callstate_ready:
  case nua_callstate_terminated:
    if (call)
      DESTROY(ep, call, nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_call_destroy_4(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-5.6: destroy when completed\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_AUTOACK(0),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, destroy_when_completed);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), ...
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); if (e->data->e_event == nua_r_invite) {
    TEST_E(e->data->e_event, nua_r_invite);
    TEST(e->data->e_status, 180);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
    TEST_1(e = e->next);
  }
  if (e->data->e_event == nua_r_invite) {
    TEST_E(e->data->e_event, nua_r_invite);
    TEST(e->data->e_status, 200);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
    TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
    TEST_1(is_answer_recv(e->data->e_tags));
    TEST_1(e = e->next);
  }
  TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state ... DESTROY
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
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.6: PASSED\n");

  END();
}

/* Destroy when one INVITE is queued. */
int test_call_destroy_5(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-5.7: destroy when re-INVITE is queued\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_AUTOACK(0),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  INVITE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, destroy_when_completed);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), ...
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 481);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(), nua_i_state
   EARLY -(S3b)-> COMPLETED: nua_respond(), nua_i_state ... DESTROY
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
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-5.7: PASSED\n");

  END();
}

int test_call_destroy(struct context *ctx)
{
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  return
    test_call_destroy_1(ctx) ||
    test_call_destroy_2(ctx) ||
    test_call_destroy_3(ctx) ||
    test_call_destroy_4(ctx) ||
    test_call_destroy_5(ctx);
}

/* ======================================================================== */

/* Early BYE

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |--------BYE-------->|
   |<------200 OK-------|
   |			|
   |<-------487---------|
   |--------ACK-------->|
   |			|
   |			|

   Client transitions:
   INIT -(C1)-> CALLING -(C2)-> PROCEEDING -(8)-> TERMINATING -> TERMINATED

   Server transitions:
   INIT -(S1)-> RECEIVED -(S2a)-> EARLY -(S8)-> TERMINATED

*/

int bye_when_ringing(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_proceeding:
    BYE(ep, call, nh, TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}

int bye_when_completing(CONDITION_PARAMS);

static int ack_sent = 0;

size_t count_acks(void *arg, void *message, size_t len)
{
  (void)arg;

  if (su_casenmatch(message, "ACK sip:", 8))
    ack_sent++;

  return len;
}

int test_bye_before_200(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  if (print_headings)
    printf("TEST NUA-6.1: BYE call when ringing\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, bye_when_ringing, -1, alert_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite(180, nua_i_state, nua_cancel()
     PROCEEDING -(C6b)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next);
  if (e->data->e_event == nua_r_bye) {
    /* We might receive this before or after response to INVITE */
    /* If afterwards, it will come after nua_i_state and we just ignore it */
    TEST_E(e->data->e_event, nua_r_bye); TEST(e->data->e_status, 200);
    TEST_1(e->data->e_msg);
    /* Forking has not been enabled, so this should be actually a CANCEL */
    TEST(sip_object(e->data->e_msg)->sip_cseq->cs_method, sip_method_cancel);
    TEST_1(e = e->next);
  }
  TEST_E(e->data->e_event, nua_r_invite); TEST(e->data->e_status, 487);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(180), nua_i_state
   EARLY -(S6b)--> TERMINATED: nua_i_cancel, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  /* Forking has not been enabled, so this should be actually a CANCEL */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_cancel);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.1: PASSED\n");

  END();
}


int bye_when_completing(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completing:
    ack_sent = 0;
    BYE(ep, call, nh, TAG_END());
    return 0;
  case nua_callstate_terminated:
    return 1;
  default:
    return 0;
  }
}


int test_bye_before_ack(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  struct nat_filter *f = NULL;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

/* Early BYE 2

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |			|
   |--------BYE-------->|
   |<------200 OK-------|
   |--------ACK-------->|
   |			|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.2: BYE call when completing\n");

  if (ctx->nat)
    TEST_1(f = test_nat_add_filter(ctx->nat, count_acks, NULL, nat_outbound));
  ack_sent = 0;

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOACK(0),
	 TAG_END());

  run_ab_until(ctx, -1, bye_when_completing, -1, accept_until_terminated);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite(180, nua_i_state, nua_cancel()
     PROCEEDING -(C6b)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling);
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing);
  TEST_1(e = e->next);

  TEST_E(e->data->e_event, nua_r_bye); TEST(e->data->e_status, 200);
  TEST_1(e->data->e_msg);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  if (ctx->nat) {
    while (ack_sent == 0)
      su_root_step(ctx->root, 100);
    TEST_1(ack_sent > 0);
    TEST_1(test_nat_remove_filter(ctx->nat, f) == 0);
  }

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(180), nua_i_state
   EARLY -(S6b)--> TERMINATED: nua_i_bye, nua_i_state
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.2: PASSED\n");

  END();
}

int reject_reinvite_401(CONDITION_PARAMS);

int test_bye_after_receiving_401(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

/* BYE after receiving 401

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |--------ACK-------->|
   |			|
   |<------INVITE-------|
   |--------401-------->|
   |<--------ACK--------|
   |			|
   |<--------BYE--------|
   |------200 OK------->|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.3: BYE after receiving 401\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-6.3"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOANSWER(0),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  free_events_in_list(ctx, a->events);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  free_events_in_list(ctx, b->events);

  /* re-INVITE A. */
  INVITE(b, b_call, b_call->nh,
	 SIPTAG_SUBJECT_STR("NUA-6.3 re-INVITE"),
	 TAG_END());
  run_ab_until(ctx, -1, reject_reinvite_401, -1, save_until_final_response);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(b_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  BYE(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.3: PASSED\n");

  END();
}

int test_bye_after_sending_401(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

/* BYE after sending 401

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |--------ACK-------->|
   |			|
   |<------INVITE-------|
   |--------401-------->|
   |<--------ACK--------|
   |			|
   |--------BYE-------->|
   |<------200 OK-------|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.4.1: BYE after sending 401\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-6.4.1"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOANSWER(0),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  free_events_in_list(ctx, a->events);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  free_events_in_list(ctx, b->events);

  /* re-INVITE A. */
  INVITE(b, b_call, b_call->nh,
	 SIPTAG_SUBJECT_STR("NUA-6.4.1 re-INVITE"),
	 TAG_END());
  run_ab_until(ctx, -1, reject_reinvite_401, -1, save_until_final_response);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(b_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  BYE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.4.1: PASSED\n");

  END();
}

int test_bye_after_receiving_401_to_update(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

/* BYE after receiving 401

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |--------ACK-------->|
   |			|
   |<------UPDATE-------|
   |--------401-------->|
   |			|
   |<--------BYE--------|
   |------200 OK------->|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.4.2: BYE after receiving 401 to UPDATE\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-6.4.2"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOANSWER(0),
	 NUTAG_APPL_METHOD("UPDATE"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  free_events_in_list(ctx, a->events);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  free_events_in_list(ctx, b->events);

  /* UPDATE A. */
  UPDATE(b, b_call, b_call->nh,
	 SIPTAG_SUBJECT_STR("NUA-6.4.2 UPDATE"),
	 TAG_END());
  BYE(b, b_call, b_call->nh, TAG_END()); /* Queued until nua_authenticate */
  run_ab_until(ctx, -1, reject_reinvite_401, -1, save_until_final_response);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(b_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  AUTHENTICATE(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, until_terminated);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.4.2: PASSED\n");

  END();
}

int reject_reinvite_401(CONDITION_PARAMS)
{
  void *request = nua_current_request(nua);

  save_event_in_list(ctx, event, ep, call);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  if (status < 200 && (event == nua_i_invite || event == nua_i_update)) {
    RESPOND(ep, call, nh, SIP_401_UNAUTHORIZED,
	    NUTAG_WITH(request),
	    SIPTAG_WWW_AUTHENTICATE_STR("Digest realm=\"test_nua\", "
					"nonce=\"nsdhfuds\", algorithm=MD5, "
					"qop=\"auth\""),
	    TAG_END());
    return 0;
  }

  if (event == nua_i_state) switch (callstate(tags)) {
  case nua_callstate_ready:
    return 1;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }

  return 0;
}

int test_bye_with_407(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *c = &ctx->c;
  struct call *a_call = a->call, *c_call = c->call;
  struct event *e;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  c_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  if (!ctx->proxy_tests)
    return 0;

/* BYE after receiving 401

   A			C
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |--------ACK-------->|
   |			|
   |       |<----BYE----|
   |       |-----407--->|
   |<-------BYE---------|
   |--------200-------->|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.4.5: BYE with 407\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(c->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(c->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-6.4.2"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOANSWER(0),
	 NUTAG_APPL_METHOD("UPDATE"),
	 TAG_END());

  run_abc_until(ctx, -1, until_ready, -1, NULL, -1, accept_call);

  free_events_in_list(ctx, a->events);

  TEST_1(e = c->events->head); TEST_E(e->data->e_event, nua_i_invite);
  free_events_in_list(ctx, c->events);

  BYE(c, c_call, c_call->nh,
      TAG_END());
  run_c_until(ctx, -1, save_until_final_response);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(c_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, c->events);

  AUTHENTICATE(c, c_call, c_call->nh,
	       NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"), TAG_END());

  run_abc_until(ctx, -1, until_terminated, -1, NULL, -1, until_terminated);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, c->events);
  nua_handle_destroy(c_call->nh), c_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.4.5: PASSED\n");

  END();
}

int test_bye_to_invalid_contact(struct context *ctx)
{
  BEGIN();
  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t *sip = NULL;

  int seen_401;

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

/* Bad Contact URI

   A			B
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |<-------200---------|
   |			|
   |--------ACK-------->|
   |			|
   |<-------BYE---------|
   |--------400-------->|
   |			|
   |--------BYE-------->|
   |<------200 OK-------|
   |			|
*/
  if (print_headings)
    printf("TEST NUA-6.4.3: Test dialog with bad Contact info\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_CONTACT(NULL),
	 SIPTAG_HEADER_STR("Contact: <<sip:xyzzy@com.invalid>"),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  /* Client transitions:
     INIT -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C2)-> PROCEEDING: nua_r_invite(180, nua_i_state, nua_cancel()
     PROCEEDING -(C6b)-> TERMINATED: nua_r_invite(487), nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling);
  TEST_1(is_offer_sent(e->data->e_tags));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready);
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED: nua_i_invite, nua_i_state
   RECEIVED -(S2a)-> EARLY: nua_respond(180), nua_i_state
   EARLY -(S6b)--> TERMINATED: nua_i_cancel, nua_i_state
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  BYE(b, b_call, b_call->nh, TAG_END());

  run_b_until(ctx, -1, until_terminated);

  /* B transitions:
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head);  TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */

  TEST_1(!nua_handle_has_active_call(b_call->nh));
  TEST_1(nua_handle_has_active_call(a_call->nh));

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-6.4.3: PASSED\n");

  if (!ctx->p) {
    free_events_in_list(ctx, b->events);
    return 0;
  }

  if (print_headings)
    printf("TEST NUA-6.4.4: Wait for re-REGISTER after connection has been closed\n");

  if (!e->next || (!e->next->next || !e->next->data->e_status != 200))
    /* B is supposed to re-register pretty soon, wait for re-registration */
    run_b_until(ctx, -1, save_until_final_response);

  seen_401 = 0;

  for (e = e->next; e; e = e->next) {
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
    printf("TEST NUA-6.4.4: PASSED\n");

  END();
}

int test_early_bye(struct context *ctx)
{
  return
    test_bye_with_407(ctx) ||
    test_bye_before_200(ctx) ||
    test_bye_before_ack(ctx) ||
    test_bye_after_receiving_401(ctx) ||
    test_bye_after_sending_401(ctx) ||
    test_bye_after_receiving_401_to_update(ctx) ||
    test_bye_to_invalid_contact(ctx) ||
    0;
}
