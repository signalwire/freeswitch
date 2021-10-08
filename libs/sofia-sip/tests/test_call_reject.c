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

/**@CFILE test_call_reject.c
 * @brief NUA-4 tests: call reject cases
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
#define __func__ "test_reject"
#endif

/* ======================================================================== */
/*
 A      reject-1      B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<--------486--------|
 |---------ACK------->|
*/
int reject_1(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_486_BUSY_HERE, TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}


int test_reject_a(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-4.1: reject before ringing\n");

  /*
   A      reject-1      B
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<--------486--------|
   |---------ACK------->|
  */

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-1"),
	 SIPTAG_CONTACT_STR("sip:a@127.0.0.1"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, reject_1);

  /*
   Client transitions in reject-1:
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 486);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions in reject-1:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_contact); TEST_1(!sip->sip_contact->m_next);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.1: PASSED\n");

  END();
}

/*
 A      reject-2      B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------602--------|
 |---------ACK------->|
*/
int reject_2(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, 602, "Rejected 2", TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_reject_b(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;

  /* ------------------------------------------------------------------------ */
  /*
   A      reject-2      B
   |			|
   |-------INVITE------>|
   |<----100 Trying-----|
   |			|
   |<----180 Ringing----|
   |			|
   |<--------602--------|
   |---------ACK------->|
  */

  if (print_headings)
    printf("TEST NUA-4.2: reject after ringing\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  /* Make call reject-2 */
  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-2"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, reject_2);

  /*
   Client transitions in reject-2:
   INIT -(C1)-> CALLING -(C2)-> PROCEEDING -(C6b)-> TERMINATED
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 602);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions in reject-2:
   INIT -(S1)-> RECEIVED -(S2)-> EARLY -(S6a)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.2: PASSED\n");

  END();
}

/* ------------------------------------------------------------------------ */

int reject_302(CONDITION_PARAMS), reject_305(CONDITION_PARAMS);
int reject_500_retry_after(CONDITION_PARAMS);
int redirect_always(CONDITION_PARAMS);
int reject_604(CONDITION_PARAMS);

/*
 A     reject-302     B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<-----302 Other-----|
 |--------ACK-------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<--305 Use Proxy----|
 |--------ACK-------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<----500 Retry------|
 |--------ACK-------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<---604 Nowhere-----|
 |--------ACK-------->|
*/

int reject_302(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    {
      sip_contact_t m[1];
      *m = *ep->contact;
      m->m_url->url_user = "302";
      RESPOND(ep, call, nh, SIP_302_MOVED_TEMPORARILY,
	      SIPTAG_CONTACT(m), TAG_END());
    }
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = reject_305;
    return 0;
  default:
    return 0;
  }
}

int reject_305(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    {
      sip_contact_t m[1];
      *m = *ep->contact;
      m->m_url->url_user = "305";
      m->m_url->url_params = "lr=1";
      RESPOND(ep, call, nh, SIP_305_USE_PROXY, SIPTAG_CONTACT(m), TAG_END());
    }
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = reject_500_retry_after;
    return 0;
  default:
    return 0;
  }
}

int reject_500_retry_after(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_invite) {
    sip_retry_after_t af[1];
    sip_retry_after_init(af)->af_delta = 1;
    RESPOND(ep, call, nh, 500, "Retry After", SIPTAG_RETRY_AFTER(af), TAG_END());
  }
  else if (event == nua_i_state) switch (callstate(tags)) {
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = reject_604;
    break;
  default:
    break;
  }

  return 0;
}

int reject_604(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_604_DOES_NOT_EXIST_ANYWHERE, TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int redirect_always(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  if (event == nua_i_invite) {
    char user[30];
    sip_contact_t m[1];
    *m = *ep->contact;
    snprintf(user, sizeof user, "user-%u", ep->flags.n++);
    m->m_url->url_user = user;
    RESPOND(ep, call, nh, SIP_302_MOVED_TEMPORARILY,
	    SIPTAG_CONTACT(m), TAG_END());
    nua_handle_destroy(nh);
    call->nh = NULL;
    return 1;
  }

  return 0;
}


int test_reject_302(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  /* Make call reject-3 */
  if (print_headings)
    printf("TEST NUA-4.3: redirect then reject\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-3"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, reject_302);

  /*
   A      reject-3      B
   |                    |
   |-------INVITE------>|
   |<----100 Trying-----|
   |                    |
   |<-----302 Other-----|
   |--------ACK-------->|
   |                    |
   |-------INVITE------>|
   |<----100 Trying-----|
   |<---305 Use Proxy---|
   |--------ACK-------->|
   |                    |
   |-------INVITE------>|
   |<----100 Trying-----|
   |<-----500 Retry-----|
   |--------ACK-------->|
   |                    |
   |                    |
   |-------INVITE------>|
   |<----100 Trying-----|
   |                    |
   |<----180 Ringing----|
   |                    |
   |<---604 Nowhere-----|
   |--------ACK-------->|
  */

  /*
   Client transitions in reject-3:
   INIT -(C1)-> PROCEEDING -(C6a)-> TERMINATED/INIT
   INIT -(C1)-> CALLING -(C2)-> PROCEEDING -(C6b)-> TERMINATED
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 302);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 305);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 100);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 500);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 604);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED/INIT
   INIT -(S1)-> RECEIVED -(S2)-> EARLY -(S6b)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_request);
  TEST_S(sip->sip_request->rq_url->url_user, "302");
  TEST_1(sip->sip_route);
  TEST_S(sip->sip_route->r_url->url_user, "305");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.3: PASSED\n");

  if (print_headings)
    printf("TEST NUA-4.3.1: redirect until retry count is exceeded\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("redirect always"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, redirect_always);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.3: PASSED\n");

  END();
}

/* ------------------------------------------------------------------------ */

/* Reject call with 407, then 401 */

int reject_407(CONDITION_PARAMS);
int reject_401(CONDITION_PARAMS);
int authenticate_call(CONDITION_PARAMS);
int reject_403(CONDITION_PARAMS);

/*
 A     reject-401     B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<--------407--------|
 |---------ACK------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------401--------|
 |---------ACK------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<-------403---------|
 |--------ACK-------->|
*/

int reject_407(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_407_PROXY_AUTH_REQUIRED,
	    SIPTAG_PROXY_AUTHENTICATE_STR("Digest realm=\"test_nua\", "
					  "nonce=\"nsdhfuds\", algorithm=MD5, "
					  "qop=\"auth-int\""),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = reject_401;
    return 0;
  default:
    return 0;
  }
}

int reject_401(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_180_RINGING, TAG_END());
    return 0;
  case nua_callstate_early:
    RESPOND(ep, call, nh, SIP_401_UNAUTHORIZED,
	    SIPTAG_WWW_AUTHENTICATE_STR("Digest realm=\"test_nua\", "
					"nonce=\"nsdhfuds\", algorithm=MD5, "
					"qop=\"auth\""),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = reject_403;
    return 0;
  default:
    return 0;
  }
}

int reject_403(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_403_FORBIDDEN, TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    ep->next_condition = NULL;
    return 1;
  default:
    return 0;
  }
}

int authenticate_call(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite && status == 401) {
    AUTHENTICATE(ep, call, nh, NUTAG_AUTH("Digest:\"test_nua\":jaska:secret"),
		 SIPTAG_SUBJECT_STR("Got 401"),
		 TAG_END());
    return 0;
  }

  if (event == nua_r_invite && status == 407) {
    AUTHENTICATE(ep, call, nh, NUTAG_AUTH("Digest:\"test_nua\":erkki:secret"),
		 SIPTAG_SUBJECT_STR("Got 407"),
		 TAG_END());
    return 0;
  }

  switch (callstate(tags)) {
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}

int test_reject_401(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event const *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-4.4: challenge then reject\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-401"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, authenticate_call, -1, reject_407);

  /*
   Client transitions in reject-3:
   INIT -(C1)-> CALLING -(C2)-> PROCEEDING -(C6b)-> TERMINATED/INIT
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 407);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 401);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 401);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 403);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED/INIT
   INIT -(S1)-> RECEIVED -(S2)-> EARLY -(S6b)-> TERMINATED/INIT
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subject);
  TEST_S(sip->sip_subject->g_value, "reject-401");
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_proxy_authorization);
  /* Ensure that nua_authenticate() tags get added to the request */
  TEST_1(sip->sip_subject);
  TEST_S(sip->sip_subject->g_value, "Got 407");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_early); /* EARLY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subject);
  TEST_S(sip->sip_subject->g_value, "Got 401");
  TEST_1(sip->sip_authorization);
  TEST_1(sip->sip_proxy_authorization);
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.4: PASSED\n");

  END();
}

/* ------------------------------------------------------------------------ */

/* Reject call with 401 and bad challenge */

/*
 A   reject-401-aka   B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<--------401--------|
 |---------ACK------->|
*/

int reject_401_aka(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_401_UNAUTHORIZED,
	    /* Send a challenge that we do not grok */
	    SIPTAG_WWW_AUTHENTICATE_STR("Digest realm=\"test_nua\", "
					"nonce=\"nsdhfuds\", algorithm=SHA0-AKAv6, "
					"qop=\"auth\""),
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

int test_reject_401_aka(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event const *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-4.6.1: invalid challenge \n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-401-aka"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());

  run_ab_until(ctx, -1, until_terminated, -1, reject_401_aka);

  /*
   Client transitions
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED/INIT
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 401);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.6.1: PASSED\n");

  END();
}


/* ------------------------------------------------------------------------ */

/* Reject call with 401, twice */

/*
 A   reject-401-bad   B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<--------401--------|
 |---------ACK------->|
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |<--------401--------|
 |---------ACK------->|
*/

int reject_401_bad(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (callstate(tags)) {
  case nua_callstate_received:
    RESPOND(ep, call, nh, SIP_401_UNAUTHORIZED,
	    /* Send a challenge that we do not grok */
	    SIPTAG_WWW_AUTHENTICATE_STR("Digest realm=\"No hope\", "
					"nonce=\"goO541ftNrw327aWpu2\", "
					"algorithm=MD5, "
					"qop=\"auth\""),
	    TAG_END());
    return 0;
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    if (ep->flags.bit0)		/* Terminate 2 calls */
      return 1;
    ep->flags.bit0 = 1;
    return 0;
  default:
    return 0;
  }
}

int authenticate_bad(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite && status == 401) {
    AUTHENTICATE(ep, call, nh, NUTAG_AUTH("Digest:\"No hope\":jaska:secret"),
		 SIPTAG_SUBJECT_STR("Bad password"),
		 TAG_END());
    return 0;
  }

  switch (callstate(tags)) {
  case nua_callstate_terminated:
    if (call)
      nua_handle_destroy(call->nh), call->nh = NULL;
    return 1;
  default:
    return 0;
  }
}


int test_reject_401_bad(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event const *e;
  sip_t const *sip;

  if (print_headings)
    printf("TEST NUA-4.6.2: bad username/password\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));
  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-401-bad"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());


  run_ab_until(ctx, -1, authenticate_bad, -1, reject_401_bad);

  /*
   Client transitions
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED/INIT
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 401);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(sip_object(e->data->e_msg)->sip_status->st_status, 401);
  /* nua_authenticate() fails and INVITE returns an internal error response */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 904);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server transitions:
   INIT -(S1)-> RECEIVED -(S6a)-> TERMINATED
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_invite);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_invite);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(e->data->e_status, 100);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_received); /* RECEIVED */
  TEST_1(is_offer_recv(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.6.2: PASSED\n");

  END();
}


/* ---------------------------------------------------------------------- */

int test_mime_negotiation(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;

  /* Make call reject-3 */
  if (print_headings)
    printf("TEST NUA-4.5: check for rejections of invalid requests\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  if (print_headings)
    printf("TEST NUA-4.5.1: invalid Content-Type\n");

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-3"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_CONTENT_TYPE_STR("application/xyzzy+xml"),
	 SIPTAG_CONTENT_DISPOSITION_STR("session;required"),
	 SIPTAG_PAYLOAD_STR("m=audio 5008 RTP/AVP 8\n"),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, NULL);

  /*
   A    reject-5.1      B
   |			|
   |-------INVITE------>|
   |<-------415---------|
   |--------ACK-------->|
  */

  /*
   Client transitions in reject-3:
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 415);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 415);
  TEST_1(sip->sip_accept);
  TEST_S(sip->sip_accept->ac_type, "application/sdp");
  TEST_1(sip->sip_accept_encoding);
  /* No content-encoding is supported */
  TEST_S(sip->sip_accept_encoding->aa_value, "");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* CALLING */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-4.5.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-4.5.2: invalid Content-Encoding\n");

  /*
   A    reject-5.2      B
   |			|
   |-------INVITE------>|
   |<-------415---------|
   |--------ACK-------->|
  */

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-5"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_CONTENT_ENCODING_STR("zyxxy"),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, NULL);

  /*
   Client transitions in reject-3:
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 415);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 415);
  TEST_1(sip->sip_accept);
  TEST_S(sip->sip_accept->ac_type, "application/sdp");
  TEST_1(sip->sip_accept_encoding);
  /* No content-encoding is supported */
  TEST_S(sip->sip_accept_encoding->aa_value, "");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-4.5.2: PASSED\n");

  if (print_headings)
    printf("TEST NUA-4.5.3: invalid Accept\n");

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("reject-3"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 SIPTAG_ACCEPT_STR("application/xyzzy+xml"),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, NULL);


  /*
   A    reject-5.3      B
   |			|
   |-------INVITE------>|
   |<-------406---------|
   |--------ACK-------->|
  */

  /*
   Client transitions in reject-3:
   INIT -(C1)-> CALLING -(C6a)-> TERMINATED
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(is_offer_sent(e->data->e_tags));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 406);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST(sip->sip_status->st_status, 406);
  TEST_1(sip->sip_accept);
  TEST_S(sip->sip_accept->ac_type, "application/sdp");
  TEST_1(sip->sip_accept_encoding);
  /* No content-encoding is supported */
  TEST_S(sip->sip_accept_encoding->aa_value, "");
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-4.5.3: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-4.5: PASSED\n");

  END();
}

/* ---------------------------------------------------------------------- */

size_t filter_200_OK(void *arg, void *message, size_t len)
{
  (void)arg;

  if (len >= 11 && su_casenmatch(message, "SIP/2.0 200", 11))
    return 0;
  return len;
}

size_t filter_ACK(void *arg, void *message, size_t len)
{
  (void)arg;

  if (len >= 7 && su_casenmatch(message, "ACK sip", 7))
    return 0;
  return len;
}

int call_with_bad_ack(CONDITION_PARAMS);
int accept_call_with_bad_contact(CONDITION_PARAMS);

int test_call_timeouts(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  struct nat_filter *f, *f2;

  if (print_headings)
    printf("TEST NUA-4.7: check for error and timeout handling\n");

  a_call->sdp = "m=audio 5008 RTP/AVP 8";
  b_call->sdp = "m=audio 5010 RTP/AVP 0 8";

  if (!ctx->nat)
    goto completed_4_7_1;

  if (print_headings)
    printf("TEST NUA-4.7.1: ACK timeout (200 OK filtered)\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(f = test_nat_add_filter(ctx->nat, filter_200_OK, NULL, nat_inbound));
  TEST_1(f2 = test_nat_add_filter(ctx->nat, filter_200_OK,
				  NULL, nat_outbound));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-4.7.1"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, accept_call);

  /*
 A     accept_call    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |   X-----200--------|
 |   X-----200--------|
 |   X-----200--------|
 |                    |
 |<--------BYE--------|
 |--------200 OK---X  |

  */

  /*
    Client transitions:
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding); /* PROCEEDING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  /*
   Server transitions:
   -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S5)-> TERMINATING
   -(S10)-> TERMINATED -X
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_error);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating); /* TERMINATING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST(e->data->e_status, 408);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  TEST_1(test_nat_remove_filter(ctx->nat, f) == 0);
  TEST_1(test_nat_remove_filter(ctx->nat, f2) == 0);

  if (print_headings)
    printf("TEST NUA-4.7.1: PASSED\n");

 completed_4_7_1:

  if (!ctx->nat)
    goto completed_4_7_2;

  if (print_headings)
    printf("TEST NUA-4.7.2: ACK timeout (ACK filtered)\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(f = test_nat_add_filter(ctx->nat, filter_ACK, NULL, nat_outbound));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-4.7.2"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, accept_call);

  /*
 A     accept_call    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |--------ACK-----X   |
 |                    |
 |<--------200--------|
 |--------ACK-----X   |
 |                    |
 |<--------200--------|
 |--------ACK-----X   |
 |                    |
 |<--------BYE--------|
 |--------200 OK----->|

  */

  /*
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated);
  TEST_1(!e->next);

  /*
   Server transitions:
   -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S5)-> TERMINATING
   -(S10)-> TERMINATED -X
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_error);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating); /* TERMINATING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  TEST_1(test_nat_remove_filter(ctx->nat, f) == 0);

  if (print_headings)
    printf("TEST NUA-4.7.2: PASSED\n");

 completed_4_7_2:

  if (print_headings)
    printf("TEST NUA-4.7.3: sending ACK fails\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-4.7.3"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 NUTAG_AUTOACK(0),
	 TAG_END());
  run_ab_until(ctx, -1, call_with_bad_ack, -1, accept_call);

  /*
 A     accept_call    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------|
 |--ACK-X             |
 |                    |
 |<--------BYE--------|
 |--------200 OK----->|

  */

  /*
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_completing); /* COMPLETING */
  /* try to send ACK */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated);
  TEST_1(!e->next);

  /*
   Server transitions:
   -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S5)-> TERMINATING
   -(S10)-> TERMINATED -X
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-4.7.3: PASSED\n");

  if (!ctx->nat)
    goto completed_4_7_4;

  if (print_headings)
    printf("TEST NUA-4.7.4: 200 OK timeout after client has timed out\n");

  if (ctx->expensive)
    nua_set_params(b->nua, NTATAG_SIP_T1X64(34000), TAG_END());
  else
    nua_set_params(b->nua, NTATAG_SIP_T1X64(4000), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TEST_1(f = test_nat_add_filter(ctx->nat, filter_ACK, NULL, nat_outbound));

  INVITE(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 SIPTAG_SUBJECT_STR("NUA-4.7.4"),
	 SOATAG_USER_SDP_STR(a_call->sdp),
	 TAG_END());
  run_ab_until(ctx, -1, until_terminated, -1, accept_call);

  /*
 A     accept_call    B
 |                    |
 |-------INVITE------>|
 |<----100 Trying-----|
 |                    |
 |<----180 Ringing----|
 |                    |
 |<--------200--------| Timer H'
 |--------ACK-----X   X--+
 |                    |  |
 |<--------200--------|  |
 |--------ACK-----X   |  |
 |                    |  |
 |<--------200--------|  |
 |                    |  |
 |<--------200--------|  |
 |                    |  |
 |                    |<-+
 |<--------BYE--------|
 |--------200 OK----->|

  */

  /*
  */

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_calling); /* CALLING */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 180);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_proceeding);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_invite);
  TEST(e->data->e_status, 200);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_ready); /* READY */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated);
  TEST_1(!e->next);

  /*
   Server transitions:
   -(S1)-> RECEIVED -(S2a)-> EARLY -(S3b)-> COMPLETED -(S5)-> TERMINATING
   -(S10)-> TERMINATED -X
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
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_error);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminating);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_bye);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_state);
  TEST(callstate(e->data->e_tags), nua_callstate_terminated); /* TERMINATED */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  TEST_1(test_nat_remove_filter(ctx->nat, f) == 0);

  if (print_headings)
    printf("TEST NUA-4.7.4: PASSED\n");

  nua_set_params(b->nua, NTATAG_SIP_T1X64(2000), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

 completed_4_7_4:

  /* XXX - PRACK timeout, PRACK failing, media failing, re-INVITEs */

  if (print_headings)
    printf("TEST NUA-4.7: PASSED\n");

  END();
}

int call_with_bad_ack(CONDITION_PARAMS)
{
  if (!check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_invite && 200 <= status && status < 300) {
    ACK(ep, call, nh,
	/* Syntax error - sending ACK fails, we send BYE */
	SIPTAG_MAX_FORWARDS_STR("blue"),
	TAG_END());
  }

  return event == nua_i_state && callstate(tags) == nua_callstate_terminated;
}

/* ---------------------------------------------------------------------- */

int test_rejects(struct context *ctx)
{
  return
    test_reject_401_bad(ctx) ||
    test_reject_a(ctx) ||
    test_reject_b(ctx) ||
    test_reject_302(ctx) ||
    test_reject_401(ctx) ||
    test_mime_negotiation(ctx) ||
    test_reject_401_aka(ctx) ||
    test_call_timeouts(ctx) ||
    0;
}
