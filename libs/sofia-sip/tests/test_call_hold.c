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

/**@CFILE test_call_hold.c
 * @brief Test re-INVITE, call hold, un-hold.
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

int complete_call(CONDITION_PARAMS);
int until_complete(CONDITION_PARAMS);
int invite_responded(CONDITION_PARAMS);
static size_t remove_first_ack(void *_once, void *message, size_t len);

/* ======================================================================== */
/* test_call_hold message sequence looks like this:

 A                    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
 :                    :
 |--INVITE(sendonly)->|
 |<---200(recvonly)---|
 |---------ACK------->|
 :                    :
 |<-INVITE(inactive)--|
 |----200(inactive)-->|
 |<--------ACK--------|
 :                    :
 |--INVITE(recvonly)->|
 |<---200(sendonly)---|
 |---------ACK------->|
 :                    :
 |<-INVITE(sendrecv)--|
 |----200(sendrecv)-->|
 |<--------ACK--------|
 :                    :
 |--------INFO------->|
 |<--------200--------|
 :                    :
 |---------BYE------->|
 |<--------200--------|
*/

int test_call_hold(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t *sip;
  int zero = 0;
  struct nat_filter *f;

  a_call->sdp =
    "m=audio 5008 RTP/AVP 0 8\n"
    "m=video 6008 RTP/AVP 30\n";
  b_call->sdp =
    "m=audio 5010 RTP/AVP 8\n"
    "a=rtcp:5011\n"
    "m=video 6010 RTP/AVP 30\n"
    "a=rtcp:6011\n";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_call);

  /*
    Client transitions:
    INIT -(C1)-> CALLING: nua_invite(), nua_i_state
    CALLING -(C2)-> PROCEEDING: nua_r_invite, nua_i_state
    PROCEEDING -(C3+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

  free_events_in_list(ctx, b->events);

  /*
 :                    :
 |--INVITE(sendonly)->|
 |<---200(recvonly)---|
 |---------ACK------->|
 :                    :
  */

  if (print_headings)
    printf("TEST NUA-7.1: put B on hold\n");

  /* Put B on hold */
  INVITE(a, a_call, a_call->nh, SOATAG_HOLD("audio"),
	 SIPTAG_SUBJECT_STR("hold b"),
	 TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDONLY);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_call_on_hold(a_call->nh));

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_RECVONLY);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_RECVONLY);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-7.1: PASSED\n");

  /* ------------------------------------------------------------------------ */
  /*
 :                    :
 |<-INVITE(inactive)--|
 |----200(inactive)-->|
 |<--------ACK--------|
 :                    :
  */

  if (print_headings)
    printf("TEST NUA-7.2: put A on hold\n");

  /* Put A on hold, too. */
  INVITE(b, b_call, b_call->nh, SOATAG_HOLD("audio"),
	 SIPTAG_SUBJECT_STR("hold a"),
	 TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(nua_handle_has_call_on_hold(b_call->nh));

  free_events_in_list(ctx, b->events);

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_call_on_hold(a_call->nh));

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-7.2: PASSED\n");

  /* ------------------------------------------------------------------------ */
  /*
 :                    :
 |--INVITE(recvonly)->|
 |<---200(sendonly)---|
 |---------ACK------->|
 :                    :
  */

  if (print_headings)
    printf("TEST NUA-7.3: resume B\n");

  /* Resume B from hold */
  INVITE(a, a_call, a_call->nh, SOATAG_HOLD(NULL),
	 SIPTAG_SUBJECT_STR("resume b"),
	 TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_RECVONLY);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDONLY);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDONLY);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(nua_handle_has_call_on_hold(b_call->nh));

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-7.3: PASSED\n");

  /* ------------------------------------------------------------------------ */
  /*
 :                    :
 |<-INVITE(sendrecv)--|
 |----200(sendrecv)-->|
 |<--------ACK--------|
 :                    :
  */

  if (print_headings)
    printf("TEST NUA-7.4: resume A\n");

  /* Resume A on hold, too. */
  INVITE(b, b_call, b_call->nh, SOATAG_HOLD(""),
	 SIPTAG_SUBJECT_STR("TEST NUA-7.4: resume A"),
	 TAG_END());
  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a+C4)-> READY: nua_r_invite, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(a_call->nh));

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-7.4: PASSED\n");

  /* ---------------------------------------------------------------------- */
  /*
 A                    B
 |--------INFO------->|
 |<--------200--------|
   */
  if (print_headings)
    printf("TEST NUA-7.5: send INFO\n");

  INFO(a, a_call, a_call->nh, TAG_END());
  run_a_until(ctx, -1, save_until_final_response);
  /* XXX - B should get a  nua_i_info event with 405 */

  /* A sent INFO, receives 405 */
  TEST_1(e = a->events->head);  TEST_E(e->data->e_event, nua_r_info);
  TEST(e->data->e_status, 405);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

#if 0				/* XXX */
  /* B received INFO */
  TEST_1(e = b->events->head);  TEST_E(e->data->e_event, nua_i_info);
  TEST(e->data->e_status, 405);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);
#endif

  /* Add INFO to allowed methods */
  nua_set_hparams(b_call->nh, NUTAG_ALLOW("INFO, PUBLISH"), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  INFO(a, a_call, a_call->nh, TAG_END());
  run_ab_until(ctx, -1, save_until_final_response, -1, save_until_received);

  /* A sent INFO, receives 200 */
  TEST_1(e = a->events->head);  TEST_E(e->data->e_event, nua_r_info);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /* B received INFO */
  TEST_1(e = b->events->head);  TEST_E(e->data->e_event, nua_i_info);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-7.5: PASSED\n");

  /* ------------------------------------------------------------------------ */
  /*
 :                    :
 |<------INVITE-------|
 |--------200-------->|
 |<--------ACK--------|
 :                    :
  */

  if (print_headings)
    printf("TEST NUA-7.6.1: re-INVITE without auto-ack\n");

  /* Turn off auto-ack */
  nua_set_hparams(b_call->nh, NUTAG_AUTOACK(0), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  INVITE(b, b_call, b_call->nh, SOATAG_HOLD(""),
	 SIPTAG_SUBJECT_STR("TEST NUA-7.6: re-INVITE without auto-ack"),
	 TAG_END());
  run_ab_until(ctx, -1, until_complete, -1, complete_call);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a)-> COMPLETING: nua_r_invite, nua_i_state
     COMPLETING -(C4)-> READY: nua_ack(), nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  ACK(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, until_ready);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_SENDRECV);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-7.6.1: PASSED\n");

  /* ------------------------------------------------------------------------ */
  /*
 :                    :
 |<------INVITE-------|
 |--------200-------->|
 |<--------ACK--------|
 :                    :
  */

  if (ctx->proxy_tests && ctx->nat) {
  if (print_headings)
    printf("TEST NUA-7.6.2: almost overlapping re-INVITE\n");

  /* Turn off auto-ack */
  nua_set_hparams(b_call->nh, NUTAG_AUTOACK(0), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  INVITE(b, b_call, b_call->nh, SOATAG_HOLD("#"),
	 SIPTAG_SUBJECT_STR("TEST NUA-7.6.2: re-INVITE"),
	 TAG_END());
  run_ab_until(ctx, -1, until_complete, -1, complete_call);

  /* Client transitions:
     READY -(C1)-> CALLING: nua_invite(), nua_i_state
     CALLING -(C3a)-> COMPLETING: nua_r_invite, nua_i_state
     COMPLETING -(C4)-> READY: nua_ack(), nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  TEST_1(is_answer_recv(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  /*
   Server transitions:
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  f = test_nat_add_filter(ctx->nat, remove_first_ack, &zero, nat_inbound);

  ACK(b, b_call, b_call->nh, TAG_END());
  INVITE(b, b_call, b_call->nh, SOATAG_HOLD("*"),
	 SIPTAG_SUBJECT_STR("TEST NUA-7.6.2: almost overlapping re-INVITE"),
	 NUTAG_AUTOACK(1),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, invite_responded);

  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_retry_after);

#if 1
  if (e->next) {
    free_events_in_list(ctx, b->events);
    free_events_in_list(ctx, a->events);
    goto passed;	/* XXX - once in a while B *does* retry */
  }
  TEST_1(!e->next);
#endif
  free_events_in_list(ctx, b->events);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST(video_activity(e->data->e_tags), SOA_ACTIVE_INACTIVE);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  test_nat_remove_filter(ctx->nat, f);

  if (print_headings)
    printf("TEST NUA-7.6.2: PASSED\n");
  }


  /* ---------------------------------------------------------------------- */
  /*
 A                    B
 |---------BYE------->|
 |<--------200--------|
   */

  if (print_headings)
    printf("TEST NUA-7.6.3: terminate call\n");

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
  if (ctx->proxy_tests && ctx->nat) {
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 481);
  }
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);
  free_events_in_list(ctx, b->events);

 passed:

  if (print_headings)
    printf("TEST NUA-7.6.3: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  END();
}

/*
 INVITE without auto-ack
 X
 |                    |
 |-------INVITE------>|
 |<--------200--------|
 |                    |
 |---------ACK------->|
*/
int complete_call(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completing:
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

/*
 X      INVITE
 |                    |
 |-------INVITE------>|
 |<--------200--------|
*/
int until_complete(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_completed:
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

static size_t remove_first_ack(void *_once, void *message, size_t len)
{
  int *once = _once;

  if (*once)
    return len;

  if (strncmp("ACK ", message, 4) == 0) {
    printf("FILTERING %.*s\n", strcspn(message, "\r\n"), (char *)message);
    *once = 1;
    return 0;
  }

  return len;
}

/* ======================================================================== */
/* test_reinvite message sequence looks like this:

 A                    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |---------ACK------->|
 :                    :
 |<----re-INVITE------|
 |<-------BYE---------|
 |--------200-------->|
 |-----487-INVITE---->|
 |<--------ACK--------|
*/

int accept_no_save(CONDITION_PARAMS);
int ringing_until_terminated(CONDITION_PARAMS);
int bye_when_ringing(CONDITION_PARAMS);

int test_reinvite(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;

  if (print_headings)
    printf("TEST NUA-7.7: Test re-INVITE and BYE\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 0 8\n";
  b_call->sdp = "m=audio 5010 RTP/AVP 8\n";

  TEST_1(a_call->nh =
	 nua_handle(a->nua, a_call,
		    SIPTAG_FROM_STR("Alice <sip:alice@example.com>"),
		    SIPTAG_TO(b->to),
		    NUTAG_AUTOANSWER(0),
		    TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, accept_no_save, -1, accept_no_save);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(b_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

/*
 A                    B
 |<----re-INVITE------|
 |<------CANCEL-------|
 |<-------BYE---------|
 |-----200-CANCEL---->|
 |------200-BYE------>|
 |-----487-INVITE---->|
 |<--------ACK--------|
*/

  /* re-INVITE A, send BYE after receiving 180 */
  INVITE(b, b_call, b_call->nh,
	 SIPTAG_SUBJECT_STR("re-INVITE"),
	 TAG_END());
  /* Run until both a and b has terminated their call */
  run_ab_until(ctx, -1, ringing_until_terminated, -1, bye_when_ringing);

#if notyet
  struct event *e;

  /* XXX - check events later - now we are happy that calls get terminated  */
  /* Client events:
   READY -(C1)-> CALLING: nua_invite(), nua_i_state
   CALLING --(C2)--> PROCEEDING: nua_r_invite, nua_i_state, nua_bye()
   PROCEEDING--((C3a+C4)-> READY: nua_r_invite, nua_i_state
   READY --(T2)--> TERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating); /* READY */
  /* Now we can receive events, in any possible order */
  /* XXX */
  TEST_1(e = e->next);

  TEST_1(!nua_handle_has_active_call(a_call->nh));

  /*
   Server transitions:
   READY --(T2)--> CTERMINATING: nua_bye()
   TERMINATING --(T3)--> TERMINATED: nua_r_bye, nua_i_state
   READY -(S3a)-> COMPLETED: nua_i_invite, <auto-answer>, nua_i_state
   COMPLETED -(S4)-> READY: nua_i_ack, nua_i_state
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completed); /* COMPLETED */
  TEST_1(is_answer_sent(e->data->e_tags));
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_RECVONLY);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_ack);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST(audio_activity(e->data->e_tags), SOA_ACTIVE_RECVONLY);
  TEST_1(!e->next);

  TEST_1(nua_handle_has_active_call(b_call->nh));
  TEST_1(!nua_handle_has_call_on_hold(b_call->nh));

#endif

  if (print_headings)
    printf("TEST NUA-7.7: PASSED\n");

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  END();
}

/*
 Accept INVITE
 X
 |                    |
 |-------INVITE------>|
 |<--------200--------|
 |                    |
 |---------ACK------->|
*/
int accept_no_save(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

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

int ringing_until_terminated(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
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

/* ======================================================================== */

int accept_and_attempt_reinvite(CONDITION_PARAMS);
int until_ready2(CONDITION_PARAMS);

/* test_reinvite2 message sequence looks like this:

 A                    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |           /-INVITE-|
 |           \---900->|
 |                    |
 |<--------200--------|
 |---------ACK------->|
 :                    :
 :   queue INVITE     :
 :                    :
 |-----re-INVITE----->|
 |<--------200--------|
 |---------ACK------->|
 |-----re-INVITE----->|
 |<--------200--------|
 |---------ACK------->|
 :                    :
 :        glare       :
 :                    :
 |-----re-INVITE----->|
 |<----re-INVITE------|
 |<--------491--------|
 |---------491------->|
 |---------ACK------->|
 |<--------ACK--------|
 :                    :
 |---------BYE------->|
 |<--------200--------|
*/

int test_reinvite2(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  if (print_headings)
    printf("TEST NUA-7.8.1: Test re-INVITE glare\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 0 8\n";
  b_call->sdp = "m=audio 5010 RTP/AVP 8\n";

  TEST_1(a_call->nh =
	 nua_handle(a->nua, a_call,
		    SIPTAG_FROM_STR("Alice <sip:alice@example.com>"),
		    SIPTAG_TO(b->to),
		    TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_ready, -1, accept_and_attempt_reinvite);

  TEST_1(nua_handle_has_active_call(a_call->nh));
  TEST_1(nua_handle_has_active_call(b_call->nh));

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  /* Check that we can queue INVITEs */
  INVITE(a, a_call, a_call->nh, TAG_END());
  INVITE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, until_ready2, -1, until_ready2);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  /* Check that INVITE glare works */
  INVITE(a, a_call, a_call->nh, TAG_END());
  INVITE(b, b_call, b_call->nh, TAG_END());

  a->flags.n = 0, b->flags.n = 0;
  run_ab_until(ctx, -1, until_ready2, -1, until_ready2);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-7.8.1: PASSED\n");



  /* ---------------------------------------------------------------------- */
  /*
 A                    B
 |---------BYE------->|
 |<--------200--------|
   */

  if (print_headings)
    printf("TEST NUA-7.8.2: terminate call\n");

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
    printf("TEST NUA-7.8.2: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  END();
}

int accept_and_attempt_reinvite(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_prack) {
    INVITE(ep, call, nh, TAG_END());
    RESPOND(ep, call, nh, SIP_200_OK,
	    TAG_IF(call->sdp, SOATAG_USER_SDP_STR(call->sdp)),
	    TAG_END());
  }
  else switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING,
	    SIPTAG_REQUIRE_STR("100rel"),
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
  return 0;
}

/*
 X      INVITE
 |                    |
 |-------INVITE------>|
 |<--------200--------|
 |---------ACK------->|
*/
int until_ready2(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite && status == 491) {
    if (ep == &ctx->a && ++ctx->b.flags.n >= 2) ctx->b.running = 0;
    if (ep == &ctx->b && ++ctx->a.flags.n >= 2) ctx->a.running = 0;
  }

  switch (callstate(tags)) {
  case nua_callstate_ready:
    return ++ep->flags.n >= 2;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int invite_responded(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  return event == nua_r_invite && status >= 100;
}


int test_reinvites(struct context *ctx)
{
  int retval = 0;

  if (print_headings)
    printf("TEST NUA-7: Test call hold and re-INVITEs\n");

  retval = test_call_hold(ctx);

  if (retval == 0)
    retval = test_reinvite(ctx);

  if (retval == 0)
    retval = test_reinvite2(ctx);

  if (print_headings && retval == 0)
    printf("TEST NUA-7: PASSED\n");

  return retval;
}
