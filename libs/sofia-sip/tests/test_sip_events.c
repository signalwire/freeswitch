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

/**@CFILE test_sip_events.c
 * @brief NUA-12 tests: SUBSCRIBE/NOTIFY.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/nea.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen);
#endif

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_sip_events"
#endif

/* ======================================================================== */
/* Test events methods: SUBSCRIBE/NOTIFY */

/**Terminate until received notify.
 * Save events (except nua_i_active or terminated).
 */
int save_until_notified(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);
  return event == nua_i_notify;
}

int save_until_notified_and_responded(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_notify) ep->flags.bit0 = 1;
  if (event == nua_r_subscribe || event == nua_r_unsubscribe) {
    if (status == 407) {
      AUTHENTICATE(ep, call, nh,
		   NUTAG_AUTH("Digest:\"test-proxy\":charlie:secret"),
		   TAG_END());
    }
    else if (status >= 300)
      return 1;
    else if (status >= 200)
      ep->flags.bit1 = 1;
  }

  return ep->flags.bit0 && ep->flags.bit1;
}


int save_until_subscription(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);
  return event == nua_i_subscription;
}


int test_events(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *en, *es;
  sip_t const *sip;
  tagi_t const *t, *n_tags, *r_tags;
  url_t b_url[1];
  enum nua_substate substate;
  nea_sub_t *sub = NULL;

  char const open[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<presence xmlns='urn:ietf:params:xml:ns:cpim-pidf' \n"
    "   entity='pres:bob@example.org'>\n"
    "  <tuple id='ksac9udshce'>\n"
    "    <status><basic>open</basic></status>\n"
    "    <contact priority='1.0'>sip:bob@example.org</contact>\n"
    "  </tuple>\n"
    "</presence>\n";

  char const closed[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<presence xmlns='urn:ietf:params:xml:ns:cpim-pidf' \n"
    "   entity='pres:bob@example.org'>\n"
    "  <tuple id='ksac9udshce'>\n"
    "    <status><basic>closed</basic></status>\n"
    "  </tuple>\n"
    "</presence>\n";


/* SUBSCRIBE test

   A			B
   |------SUBSCRIBE---->|
   |<--------405--------|
   |			|

*/
  if (print_headings)
    printf("TEST NUA-12.1: SUBSCRIBE without notifier\n");

  nua_set_params(b->nua, SIPTAG_ALLOW_EVENTS(NULL), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_final_response,
	       -1, NULL /* XXX save_until_received */);

  /* Client events:
     nua_subscribe(), nua_r_subscribe
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_subscribe);
  TEST(e->data->e_status, 489);
  TEST_1(!e->next);

#if 0				/* XXX */
  /*
   Server events:
   nua_i_subscribe
  */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST(e->data->e_status, 405);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event);
  TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(!e->next);
#endif

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-12.1: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* SUBSCRIBE test using notifier and establishing subscription

   A			B
   |                    |
   |------SUBSCRIBE---->|
   |<--------202--------|
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/

  if (print_headings)
    printf("TEST NUA-12.2: using notifier and establishing subscription\n");

  TEST_1(b_call->nh = nua_handle(b->nua, b_call, TAG_END()));

  *b_url = *b->contact->m_url;

  NOTIFIER(b, b_call, b_call->nh,
	   NUTAG_URL(b_url),
	   SIPTAG_EVENT_STR("presence"),
	   SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	   SIPTAG_PAYLOAD_STR(closed),
	   NEATAG_THROTTLE(1),
	   TAG_END());
  run_b_until(ctx, nua_r_notifier, until_final_response);

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, save_until_received);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe
  */
  TEST_1(en = event_by_type(a->events->head, nua_i_notify));
  TEST_1(es = event_by_type(a->events->head, nua_r_subscribe));

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  if (es->next == en) {
    TEST_1(200 <= e->data->e_status && e->data->e_status < 300);
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_embryonic);
  }
  else {
    TEST_1(200 <= e->data->e_status && e->data->e_status < 300);
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  }

  TEST_1(e = en); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  n_tags = e->data->e_tags;

  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  TEST_1(sip->sip_subscription_state->ss_expires);
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(!en->next || !es->next);
  free_events_in_list(ctx, a->events);

  /* XXX --- Do not check server side events */
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-12.2: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* NOTIFY with updated content

   A			B
   |                    |
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  if (print_headings)
    printf("TEST NUA-12.3: update notifier\n");

  /* Update presence data */

  NOTIFIER(b, b_call, b_call->nh,
	   SIPTAG_EVENT_STR("presence"),
	   SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	   SIPTAG_PAYLOAD_STR(open),
	   TAG_END());

  run_ab_until(ctx, -1, save_until_notified, -1, save_until_received);

  /* subscriber events:
     nua_i_notify
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  TEST_1(sip->sip_subscription_state->ss_expires);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value,
       nua_substate_active);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /* XXX --- Do not check server side events */
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-12.3: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* un-SUBSCRIBE

   A			B
   |                    |
   |------SUBSCRIBE---->|
   |<--------202--------|
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  if (print_headings)
    printf("TEST NUA-12.5: un-SUBSCRIBE\n");

  UNSUBSCRIBE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_final_response,
	       -1, save_until_subscription);

  /* Client events:
     nua_unsubscribe(), nua_i_notify/nua_r_unsubscribe
  */
  TEST_1(e = a->events->head);
  if (e->data->e_event == nua_i_notify) {
    TEST_E(e->data->e_event, nua_i_notify);
    TEST_1(sip = sip_object(e->data->e_msg));
    n_tags = e->data->e_tags;
    TEST_1(sip->sip_event);
    TEST_1(sip->sip_subscription_state);
    TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
    TEST_1(!sip->sip_subscription_state->ss_expires);
    TEST_1(tl_find(n_tags, nutag_substate));
    TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
    TEST_1(e = e->next);
  }
  TEST_E(e->data->e_event, nua_r_unsubscribe);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value,
       nua_substate_terminated);
  /* Currently, NOTIFY is dropped after successful response to unsubscribe */
  /* But we don't really care.. */
  /* TEST_1(!e->next); */
  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscription with terminated status */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscription);
  TEST(tl_gets(e->data->e_tags,
               NEATAG_SUB_REF(sub),
               NUTAG_SUBSTATE_REF(substate),
               TAG_END()), 2);
  TEST_1(sub);
  TEST(substate, nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-12.5: PASSED\n");

  /* ---------------------------------------------------------------------- */
/* Fetch event, SUBSCRIBE with expires: 0

   A			B
   |                    |
   |------SUBSCRIBE---->|
   |<--------202--------|
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  if (print_headings)
    printf("TEST NUA-12.5.1: event fetch\n");

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_ACCEPT_STR("application/pidf+xml"),
	    SIPTAG_EXPIRES_STR("0"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, save_until_subscription);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe
  */
  TEST_1(en = event_by_type(a->events->head, nua_i_notify));
  TEST_1(es = event_by_type(a->events->head, nua_r_subscribe));

  e = es; TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(t = tl_find(e->data->e_tags, nutag_substate));
  TEST_1(t->t_value == nua_substate_pending ||
	 t->t_value == nua_substate_terminated ||
	 t->t_value == nua_substate_embryonic);

  e = en; TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  n_tags = e->data->e_tags;

  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value,
       nua_substate_terminated);
  TEST_1(!en->next || !es->next);
  free_events_in_list(ctx, a->events);

  /*
   Server events:
   nua_i_subscription
  */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscription);
  TEST(tl_gets(e->data->e_tags, NEATAG_SUB_REF(sub), TAG_END()), 1);
  TEST_1(sub);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-12.4.1: PASSED\n");


  /* ---------------------------------------------------------------------- */
/* 2nd SUBSCRIBE with event id

   A			B
   |                    |
   |------SUBSCRIBE---->|
   |<--------202--------|
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  /* XXX - we should do this before unsubscribing first one */
  if (print_headings)
    printf("TEST NUA-12.4.2: establishing 2nd subscription\n");

   NOTIFIER(b, b_call, b_call->nh,
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_CONTENT_TYPE_STR("application/xpidf+xml"),
	    SIPTAG_PAYLOAD_STR(open),
	    NEATAG_THROTTLE(1),
	    NUTAG_SUBSTATE(nua_substate_pending),
	    TAG_END());
  run_b_until(ctx, nua_r_notifier, until_final_response);

  NOTIFIER(b, b_call, b_call->nh,
	   SIPTAG_EVENT_STR("presence"),
	   SIPTAG_CONTENT_TYPE_STR("application/xpidf+xml"),
	   SIPTAG_PAYLOAD_STR(closed),
	   NEATAG_THROTTLE(1),
	   NEATAG_FAKE(1),
	   NUTAG_SUBSTATE(nua_substate_pending),
	   TAG_END());
  run_b_until(ctx, nua_r_notifier, until_final_response);

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence;id=1"),
	    SIPTAG_ACCEPT_STR("application/xpidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, save_until_subscription);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe
  */
  TEST_1(en = event_by_type(a->events->head, nua_i_notify));
  TEST_1(es = event_by_type(a->events->head, nua_r_subscribe));

  e = es; TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(t = tl_find(e->data->e_tags, nutag_substate));
  TEST_1(t->t_value == nua_substate_pending ||
	 t->t_value == nua_substate_embryonic);

  e = en; TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  n_tags = e->data->e_tags;

  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_S(sip->sip_event->o_id, "1");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/xpidf+xml");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  /* Check that we really got "fake" content */
  TEST_1(memmem(sip->sip_payload->pl_data, sip->sip_payload->pl_len,
		"<basic>closed</basic>", strlen("<basic>closed</basic>")));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(sip->sip_subscription_state->ss_expires);
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value,
       nua_substate_pending);
  TEST_1(!en->next || !es->next);
  free_events_in_list(ctx, a->events);

  /*
   Server events:
   nua_i_subscription
  */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscription);
  TEST(tl_gets(e->data->e_tags, NEATAG_SUB_REF(sub), TAG_END()), 1);
  TEST_1(sub);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  /* Authorize user A */
  AUTHORIZE(b, b_call, b_call->nh,
	    NUTAG_SUBSTATE(nua_substate_active),
	    NEATAG_SUB(sub),
	    NEATAG_FAKE(0),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified,
	       -1, save_until_final_response);

  /* subscriber events:
     nua_i_notify with NUTAG_SUBSTATE(nua_substate_active)
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/xpidf+xml");
  TEST_1(sip->sip_payload && sip->sip_payload->pl_data);
  /* Check that we really got real content */
  TEST_1(memmem(sip->sip_payload->pl_data, sip->sip_payload->pl_len,
		"<basic>open</basic>", strlen("<basic>open</basic>")));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  TEST_1(sip->sip_subscription_state->ss_expires);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /*
   Server events:
   nua_r_authorize
  */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_r_authorize);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-12.4: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* NOTIFY terminating subscription

   A			B
   |                    |
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/

  if (print_headings)
    printf("TEST NUA-12.6: terminate notifier\n");

  TERMINATE(b, b_call, b_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_notified, -1, until_final_response);

  /* Client events:
     nua_i_notify
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_S(sip->sip_event->o_id, "1");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(!sip->sip_subscription_state->ss_expires);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-12.6: PASSED\n");

  /* ---------------------------------------------------------------------- */


  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  END();			/* test_events */
}
