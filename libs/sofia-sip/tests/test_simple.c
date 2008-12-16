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

/**@CFILE test_nua_simple.c
 * @brief NUA-11: Test SIMPLE methods: MESSAGE, PUBLISH and SUBSCRIBE/NOTIFY.
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
#define __func__ "test_simple"
#endif

extern int accept_request(CONDITION_PARAMS);

int save_until_nth_final_response(CONDITION_PARAMS);
int accept_n_notifys(CONDITION_PARAMS);

int test_message(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;
  url_t url[1];

/* Message test

   A			B
   |-------MESSAGE----->|
   |<-------200---------|
   |			|

*/
  if (print_headings)
    printf("TEST NUA-11.1.1: MESSAGE\n");

  if (ctx->proxy_tests)
    *url = *b->to->a_url;
  else
    *url = *b->contact->m_url;

  /* Test that query part is included in request sent to B */
  url->url_headers = "organization=United%20Testers";

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, TAG_END()));

  MESSAGE(a, a_call, a_call->nh,
	  NUTAG_URL(url),
	  SIPTAG_SUBJECT_STR("NUA-11.1.1"),
	  SIPTAG_CONTENT_TYPE_STR("text/plain"),
	  SIPTAG_PAYLOAD_STR("Hello hellO!\n"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, save_until_received);

  /* Client events:
     nua_message(), nua_r_message
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_message);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  /*
   Server events:
   nua_i_message
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_message);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subject && sip->sip_subject->g_string);
  TEST_S(sip->sip_subject->g_string, "NUA-11.1.1");
  TEST_1(sip->sip_organization);
  TEST_S(sip->sip_organization->g_string, "United Testers");
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.1.1: PASSED\n");

/* MESSAGE as application method

   A			B
   |-------MESSAGE----->|
   |<-------202---------|
   |			|
*/

  if (print_headings)
    printf("TEST NUA-11.1.2: MESSAGE\n");

  nua_set_params(b->nua, NUTAG_APPL_METHOD("MESSAGE"), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, TAG_END()));

  MESSAGE(a, a_call, a_call->nh,
	  NUTAG_URL(url),
	  SIPTAG_SUBJECT_STR("NUA-11.1.2"),
	  SIPTAG_CONTENT_TYPE_STR("text/plain"),
	  SIPTAG_PAYLOAD_STR("Hello hellO!\n"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, accept_request);

  /* Client events:
     nua_message(), nua_r_message
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_message);
  TEST(e->data->e_status, 202);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip_user_agent(sip));
  TEST_S(sip_user_agent(sip)->g_value, "007");
  TEST_1(!e->next);

  /*
   Server events:
   nua_i_message
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_message);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subject && sip->sip_subject->g_string);
  TEST_S(sip->sip_subject->g_string, "NUA-11.1.2");
  TEST_1(sip->sip_organization);
  TEST_S(sip->sip_organization->g_string, "United Testers");
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.1.2: PASSED\n");


/* Message test

   A
   |-------MESSAGE--\
   |<---------------/
   |--------200-----\
   |<---------------/
   |

*/
  if (print_headings)
    printf("TEST NUA-11.2: MESSAGE to myself\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(a->to), TAG_END()));

  MESSAGE(a, a_call, a_call->nh,
	  /* We cannot reach us by using our contact! */
	  NUTAG_URL(!ctx->p && !ctx->proxy_tests ? a->contact->m_url : NULL),
	  SIPTAG_SUBJECT_STR("NUA-11.2"),
	  SIPTAG_CONTENT_TYPE_STR("text/plain"),
	  SIPTAG_PAYLOAD_STR("Hello hellO!\n"),
	  TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Events:
     nua_message(), nua_i_message, nua_r_message
  */
  TEST_1(e = a->specials->head);
  while (e->data->e_event == nua_i_outbound)
    e = e->next;
  TEST_E(e->data->e_event, nua_i_message);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subject && sip->sip_subject->g_string);
  TEST_S(sip->sip_subject->g_string, "NUA-11.2");

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_message);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, a->specials);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.2: PASSED\n");

  END();
}

int accept_request(CONDITION_PARAMS)
{
  msg_t *with = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  if (status < 200) {
    RESPOND(ep, call, nh, SIP_202_ACCEPTED,
	    NUTAG_WITH(with),
	    SIPTAG_USER_AGENT_STR("007"),
	    TAG_END());
    return 1;
  }

  return 0;
}

char const *test_etag = "tagtag";

int respond_with_etag(CONDITION_PARAMS)
{
  msg_t *with = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 1;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
    char const *etag;
  case nua_i_publish:
    etag = sip->sip_if_match ? sip->sip_if_match->g_value : NULL;
    if (sip->sip_if_match && (etag == NULL || strcmp(etag, test_etag))) {
      RESPOND(ep, call, nh, SIP_412_PRECONDITION_FAILED,
	      NUTAG_WITH(with),
	      TAG_END());
    }
    else {
      RESPOND(ep, call, nh, SIP_200_OK,
	      NUTAG_WITH(with),
	      SIPTAG_ETAG_STR(test_etag),
	      SIPTAG_EXPIRES_STR("3600"),
	      SIPTAG_EXPIRES(sip->sip_expires),	/* overrides 3600 */
	      TAG_END());
    }
    return 1;
  default:
    return 0;
  }
}

static int close_handle(CONDITION_PARAMS)
{
  if (call->nh == nh)
    call->nh = NULL;
  nua_handle_destroy(nh);
  return 1;
}

int test_publish(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;


/* PUBLISH test

   A			B
   |-------PUBLISH----->|
   |<-------405---------| (method not allowed by default)
   |			|
   |-------PUBLISH----->|
   |<-------501---------| (no events allowed)
   |			|
   |-------PUBLISH----->|
   |<-------489---------| (event not allowed by default)
   |			|
   |-------PUBLISH----->|
   |<-------200---------| (event allowed, responded)
   |			|
   |-----un-PUBLISH---->|
   |<-------200---------| (event allowed, responded)

*/
  if (print_headings)
    printf("TEST NUA-11.3: PUBLISH\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_EVENT_STR("presence"),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, NULL);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 405);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  nua_set_params(b->nua, NUTAG_ALLOW("PUBLISH"), TAG_END());

  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_EVENT_STR("presence"),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 501);	/* Not implemented */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /* Allow presence event */

  nua_set_params(b->nua, NUTAG_ALLOW_EVENTS("presence"), TAG_END());

  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_EVENT_STR("reg"),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 489);	/* Bad Event */
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_EVENT_STR("presence"),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  SIPTAG_EXPIRES_STR("5"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, respond_with_etag);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_etag);
  TEST_S(sip->sip_etag->g_string, test_etag);
  TEST_1(!e->next);

  /*
   Server events:
   nua_i_publish
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_publish);
  TEST(e->data->e_status, 100);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (!ctx->expensive && 0)
    goto skip_republish;

  run_ab_until(ctx, -1, save_until_final_response, -1, respond_with_etag);

  /* Client events: nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_etag);
  TEST_S(sip->sip_etag->g_string, test_etag);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server events:
   nua_i_publish
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_publish);
  TEST(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_if_match);
  TEST_S(sip->sip_if_match->g_string, "tagtag");
  TEST_1(!sip->sip_content_type);
  TEST_1(!sip->sip_payload);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

 skip_republish:

  UNPUBLISH(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, respond_with_etag);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_unpublish);
  TEST(e->data->e_status, 200);
  TEST_1(!e->next);

  /*
   Server events:
   nua_i_publish
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_publish);
  TEST(e->data->e_status, 100);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  /* Let server close handle without responding to PUBLISH */
  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_EVENT_STR("presence"),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, close_handle);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 500);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  /* No Event header */

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  PUBLISH(a, a_call, a_call->nh,
	  TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	  SIPTAG_CONTENT_TYPE_STR("text/urllist"),
	  SIPTAG_PAYLOAD_STR("sip:example.com\n"),
	  TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, save_events);

  /* Client events:
     nua_publish(), nua_r_publish
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_publish);
  TEST(e->data->e_status, 489);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /*
   Server events: nothing
  */
  TEST_1(!b->events->head);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.3: PASSED\n");

  END();
}

static char const presence_open[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<presence xmlns='urn:ietf:params:xml:ns:cpim-pidf' \n"
    "   entity='pres:bob@example.org'>\n"
    "  <tuple id='ksac9udshce'>\n"
    "    <status><basic>open</basic></status>\n"
    "    <contact priority='1.0'>sip:bob@example.org</contact>\n"
    "  </tuple>\n"
    "</presence>\n";

static char const presence_closed[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<presence xmlns='urn:ietf:params:xml:ns:cpim-pidf' \n"
    "   entity='pres:bob@example.org'>\n"
    "  <tuple id='ksac9udshce'>\n"
    "    <status><basic>closed</basic></status>\n"
    "  </tuple>\n"
    "</presence>\n";


int accept_and_notify_twice(CONDITION_PARAMS)
{
  msg_t *with = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_subscribe:
    if (status < 200) {
      NOTIFY(ep, call, nh,
	     SIPTAG_EVENT(sip->sip_event),
	     SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     NUTAG_SUBSTATE(nua_substate_pending),
	     TAG_END());
      NOTIFY(ep, call, nh,
	     SIPTAG_EVENT(sip->sip_event),
	     SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	     SIPTAG_PAYLOAD_STR(presence_open),
	     NUTAG_SUBSTATE(nua_substate_active),
	     TAG_END());
      RESPOND(ep, call, nh, SIP_202_ACCEPTED,
	      NUTAG_WITH(with),
	      SIPTAG_EXPIRES_STR("360"),
	      TAG_END());
    }
    return 0;

  case nua_r_notify:
    return status >= 200 &&
      tl_find(tags, nutag_substate)->t_value == nua_substate_active;

  default:
    return 0;
  }
}

int save_until_responded_and_notified_twice(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_notify) {
    if (ep->flags.bit0)
      ep->flags.bit1 = 1;
    ep->flags.bit0 = 1;
  }

  if (event == nua_r_subscribe || event == nua_r_unsubscribe) {
    if (status >= 300)
      return 1;
    else if (status >= 200) {
      ep->flags.bit2 = 1;
    }
  }

  return ep->flags.bit0 && ep->flags.bit1 && ep->flags.bit2;
}


int accept_and_notify(CONDITION_PARAMS)
{
  msg_t *with = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_subscribe:
    if (status < 200) {
      RESPOND(ep, call, nh, SIP_202_ACCEPTED,
	      NUTAG_WITH(with),
	      SIPTAG_EXPIRES_STR("360"),
	      SIPTAG_EXPIRES(sip->sip_expires),
	      TAG_END());

      NOTIFY(ep, call, nh,
	     SIPTAG_EVENT(sip->sip_event),
	     SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     NUTAG_SUBSTATE(nua_substate_pending),
	     TAG_END());
    }
    return 0;

  case nua_r_notify:
    return status >= 200;

  default:
    return 0;
  }
}

int save_and_notify(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_subscribe:
    NOTIFY(ep, call, nh,
	   SIPTAG_EVENT(sip->sip_event),
	   SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	   SIPTAG_PAYLOAD_STR(presence_closed),
	   NUTAG_SUBSTATE(nua_substate_active),
	   TAG_END());
    return 0;

  case nua_r_notify:
    return status >= 200;

  default:
    return 0;
  }
}

extern int save_until_notified_and_responded(CONDITION_PARAMS);
extern int save_until_notified(CONDITION_PARAMS);

int test_subscribe_notify(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *en1, *en2, *es;
  sip_t const *sip;
  tagi_t const *n_tags, *r_tags;

  if (print_headings)
    printf("TEST NUA-11.4: notifier server using nua_notify()\n");

  if (print_headings)
    printf("TEST NUA-11.4.1: establishing subscription\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_EXPIRES_STR("333"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_responded_and_notified_twice,
	       -1, accept_and_notify_twice);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe/nua_i_notify
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (en2 == NULL && e->data->e_event == nua_i_notify)
      en2 = e;
    else if (e->data->e_event == nua_r_subscribe)
      es = e;
    else
      TEST_1(!e);
  }

  TEST_1(e = en1);
  TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_pending);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(sip->sip_subscription_state->ss_expires);

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(e->data->e_status == 202 || e->data->e_status == 200);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  r_tags = tl_find(e->data->e_tags, nutag_substate);
  if (es == a->events->head) {
    TEST(r_tags->t_value, nua_substate_embryonic);
  }
  else if (es == a->events->head->next) {
    TEST_1(r_tags->t_value == nua_substate_pending);
  }
  else {
    TEST_1(r_tags->t_value == nua_substate_active);
  }
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_expires);
  TEST_1(sip->sip_expires->ex_delta <= 333);

  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_pending);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.4.1: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* NOTIFY with updated content

   A			B
   |                    |
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  if (print_headings)
    printf("TEST NUA-11.4.2: send NOTIFY\n");

  /* Update presence data */

  NOTIFY(b, b_call, b_call->nh,
	 NUTAG_SUBSTATE(nua_substate_active),
	 SIPTAG_EVENT_STR("presence"),
	 SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	 SIPTAG_PAYLOAD_STR(presence_open),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_notified,
	       -1, save_until_final_response);

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
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  /* Notifier events: nua_r_notify */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.4.2: PASSED\n");

  /* ---------------------------------------------------------------------- */

/* Re-SUBSCRIBE

   A			B
   |                    |
   |<------NOTIFY-------|
   |-------200 OK------>|
   |                    |
*/
  if (print_headings)
    printf("TEST NUA-11.4.3: re-SUBSCRIBE\n");

  /* Set default expiration time */
  nua_set_hparams(b_call->nh, NUTAG_SUB_EXPIRES(365), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_EXPIRES_STR("3600"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, save_until_final_response);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (e->data->e_event == nua_r_subscribe)
      es = e;
    else
      TEST_1(!e);
  }
  TEST_1(e = en1);
  TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  TEST_1(sip->sip_subscription_state->ss_expires);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_expires);
  TEST_1(sip->sip_expires->ex_delta <= 365);

  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.4.3: PASSED\n");

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
    printf("TEST NUA-11.4.4: un-SUBSCRIBE\n");

  UNSUBSCRIBE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_final_response,
	       -1, save_until_final_response);

  /* Client events:
     nua_unsubscribe(), nua_i_notify/nua_r_unsubscribe
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (e->data->e_event == nua_r_unsubscribe)
      es = e;
    else
      TEST_1(!e);
  }
  if (en1) {
    TEST_1(e = en1); TEST_E(e->data->e_event, nua_i_notify);
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_event);
    TEST_1(sip->sip_subscription_state);
    TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
    TEST_1(!sip->sip_subscription_state->ss_expires);
    n_tags = e->data->e_tags;
    TEST_1(tl_find(n_tags, nutag_substate));
    TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  }

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_unsubscribe);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  if (en1 == a->events->head)
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
  else
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_expires);
  TEST_1(sip->sip_expires->ex_delta == 0);

  free_events_in_list(ctx, a->events);

  /* Notifier events: nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value,
       nua_substate_terminated);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(e->data->e_status >= 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.4.4: PASSED\n");

  if (print_headings)
    printf("TEST NUA-11.4: PASSED\n");

  END();
}

/* ---------------------------------------------------------------------- */
/* Subscriber gracefully terminates dialog upon 483 */

static
size_t change_status_to_483(void *a, void *message, size_t len);
int save_until_notified_and_responded_twice(CONDITION_PARAMS);
int save_until_notify_responded_twice(CONDITION_PARAMS);
int accept_subscription_until_terminated(CONDITION_PARAMS);

int test_subscribe_notify_graceful(struct context *ctx)
{
  if (!ctx->nat)
    return 0;

  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *en1, *en2, *es;
  sip_t const *sip;
  tagi_t const *n_tags, *r_tags;
  struct nat_filter *f;

  if (print_headings)
    printf("TEST NUA-11.5.1: establishing subscription\n");

  nua_set_params(b->nua, NUTAG_APPL_METHOD("NOTIFY"),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, accept_and_notify);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (es == NULL && e->data->e_event == nua_r_subscribe)
      es = e;
    else
      TEST_1(!e);
  }
  TEST_1(e = en1); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(sip->sip_subscription_state->ss_expires);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_pending);

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  if (es == a->events->head)
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_embryonic);
  else
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_pending);
  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_pending);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.5.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-11.5.2: terminate gracefully upon 483\n");

  TEST_1(f = test_nat_add_filter(ctx->nat,
				 change_status_to_483, NULL,
				 nat_inbound));

  SUBSCRIBE(a, a_call, a_call->nh, TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded_twice,
	       -1, accept_subscription_until_terminated);

#if 0
  /* Client events:
     nua_unsubscribe(), nua_i_notify/nua_r_unsubscribe
  */
  TEST_1(e = a->events->head);
  if (e->data->e_event == nua_i_notify) {
    TEST_E(e->data->e_event, nua_i_notify);
    n_tags = e->data->e_tags;
    TEST_1(sip = sip_object(e->data->e_msg));
    TEST_1(sip->sip_event);
    TEST_1(sip->sip_subscription_state);
    TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
    TEST_1(!sip->sip_subscription_state->ss_expires);
    TEST_1(tl_find(n_tags, nutag_substate));
    TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
    TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_unsubscribe);
    TEST(e->data->e_status, 200);
    r_tags = e->data->e_tags;
    TEST_1(tl_find(r_tags, nutag_substate));
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
  }
  else {
    TEST_E(e->data->e_event, nua_r_unsubscribe);
    TEST(e->data->e_status, 200);
    r_tags = e->data->e_tags;
    TEST_1(tl_find(r_tags, nutag_substate));
    TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
  }
  TEST_1(!e->next);

  /* Notifier events: nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value,
       nua_substate_terminated);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(e->data->e_status >= 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
#endif

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  test_nat_remove_filter(ctx->nat, f);

  if (print_headings)
    printf("TEST NUA-11.5.2: PASSED\n");

  END();
}

static
size_t change_status_to_483(void *a, void *message, size_t len)
{
  (void)a;

  if (strncmp("SIP/2.0 2", message, 9) == 0) {
    memcpy(message, "SIP/2.0 483", 11);
  }
  return len;
}

int save_until_notified_and_responded_twice(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_notify) {
    if (ep->flags.bit0)
      ep->flags.bit1 = 1;
    ep->flags.bit0 = 1;
  }

  if (event == nua_r_subscribe || event == nua_r_unsubscribe) {
    if (status >= 300)
      return 1;
    else if (status >= 200) {
      if (ep->flags.bit2)
	ep->flags.bit3 = 1;
      ep->flags.bit2 = 1;
    }
  }

  return ep->flags.bit0 && ep->flags.bit1 && ep->flags.bit2 && ep->flags.bit3;
}

int save_until_notify_responded_twice(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (event == nua_r_notify) {
    if (ep->flags.bit0)
      ep->flags.bit1 = 1;
    ep->flags.bit0 = 1;
  }

  return ep->flags.bit0 && ep->flags.bit1;
}

/* ---------------------------------------------------------------------- */

/*
 * When incoming SUBSCRIBE, send NOTIFY,
 * 200 OK SUBSCRIBE when NOTIFY has been responded.
 */
int notify_and_accept(CONDITION_PARAMS)
{
  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_subscribe:
    if (status < 200) {
      NOTIFY(ep, call, nh,
	     SIPTAG_EVENT(sip->sip_event),
	     SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
    }
    return 0;

  case nua_r_notify:
    if (status >= 200) {
      struct event *e;
      for (e = ep->events->head; e; e = e->next) {
	if (e->data->e_event == nua_i_subscribe) {
	  RESPOND(ep, call, nh, SIP_200_OK,
		  NUTAG_WITH(e->data->e_msg),
		  TAG_END());
	  break;
	}
      }
      return 1;
    }

  default:
    return 0;
  }
}

int test_event_fetch(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *en1, *en2, *es;
  sip_t const *sip;
  tagi_t const *r_tags;

  if (print_headings)
    printf("TEST NUA-11.6.1: event fetch using nua_notify()\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

/* Fetch 1:

   A			 B
   |                     |
   |------SUBSCRIBE----->|
   |     Expires: 0      |
   |<---------202--------|
   |                     |
   |<-------NOTIFY-------|
   | S-State: terminated |
   |-------200 OK------->|
   |                     |
*/

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_EXPIRES_STR("0"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, accept_and_notify);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe/nua_i_notify
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (en2 == NULL && e->data->e_event == nua_i_notify)
      en2 = e;
    else if (e->data->e_event == nua_r_subscribe)
      es = e;
    else
      TEST_1(!e);
  }

  TEST_1(e = en1);
  TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(e->data->e_status == 202 || e->data->e_status == 200);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));

  if (es == a->events->head)
    TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_embryonic);
  else
    TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_expires);
  TEST_1(sip->sip_expires->ex_delta == 0);

  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.6.1: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;


  if (print_headings)
    printf("TEST NUA-11.6.2: event fetch, NOTIFY comes before 202\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

/* Fetch 2:

   A			 B
   |                     |
   |------SUBSCRIBE----->|
   |     Expires: 0      |
   |<-------NOTIFY-------|
   | S-State: terminated |
   |-------200 OK------->|
   |                     |
   |<---------202--------|
   |                     |
*/

  SUBSCRIBE(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	    SIPTAG_EVENT_STR("presence"),
	    SIPTAG_EXPIRES_STR("0"),
	    SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	    TAG_END());

  run_ab_until(ctx, -1, save_until_notified_and_responded,
	       -1, notify_and_accept);

  /* Client events:
     nua_subscribe(), nua_i_notify/nua_r_subscribe/nua_i_notify
  */
  for (en1 = en2 = es = NULL, e = a->events->head; e; e = e->next) {
    if (en1 == NULL && e->data->e_event == nua_i_notify)
      en1 = e;
    else if (en2 == NULL && e->data->e_event == nua_i_notify)
      en2 = e;
    else if (e->data->e_event == nua_r_subscribe)
      es = e;
    else
      TEST_1(!e);
  }

  TEST_1(e = en1);
  TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));
  TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_subscribe);
  TEST_1(e->data->e_status == 202 || e->data->e_status == 200);
  TEST_1(tl_find(e->data->e_tags, nutag_substate));

  if (es == a->events->head)
    TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_embryonic);
  else
    TEST(tl_find(e->data->e_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_expires);
  TEST_1(sip->sip_expires->ex_delta == 0);

  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.6.2: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  END();
}

/* ---------------------------------------------------------------------- */
/* Unsolicited NOTIFY */

int accept_notify(CONDITION_PARAMS);

int test_newsub_notify(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;
  sip_call_id_t *i;
  tagi_t const *n_tags, *r_tags;

#if 0

  if (print_headings)
    printf("TEST NUA-11.7.1: rejecting NOTIFY without subscription locally\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  NOTIFY(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	 SIPTAG_SUBJECT_STR("NUA-11.7.1"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Client events:
     nua_notify(), nua_r_notify
  */
  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 481);
  TEST_1(!e->data->e_msg);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-11.7.1: PASSED\n");

  if (print_headings)
    printf("TEST NUA-11.7.2: rejecting NOTIFY without subscription\n");

  TEST_1(i = sip_call_id_create(nua_handle_home(a_call->nh), NULL));

  NOTIFY(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBJECT_STR("NUA-11.7.2 first"),
	 SIPTAG_FROM_STR("<sip:alice@example.com>;tag=nua-11.7.2"),
	 SIPTAG_CALL_ID(i),
	 SIPTAG_CSEQ_STR("1 NOTIFY"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Client events:
     nua_notify(), nua_r_notify
  */
  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 481);
  TEST_1(e->data->e_msg);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /* 2nd NOTIFY using same dialog */
  /* Check that server really discards the dialog */

  NOTIFY(a, a_call, a_call->nh, NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBJECT_STR("NUA-11.7.2 second"),
	 SIPTAG_FROM_STR("<sip:alice@example.com>;tag=nua-11.7.2"),
	 SIPTAG_CALL_ID(i),
	 SIPTAG_CSEQ_STR("2 NOTIFY"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  /* Client events:
     nua_notify(), nua_r_notify
  */
  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 481);
  TEST_1(e->data->e_msg);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  if (print_headings)
    printf("TEST NUA-11.7.2: PASSED\n");

  /* ---------------------------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-11.7.3: accept NOTIFY\n");

  nua_set_params(b->nua, NUTAG_APPL_METHOD("NOTIFY"), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBJECT_STR("NUA-11.7.3"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, accept_notify);

  /* Notifier events: nua_r_notify */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  /* subscriber events:
     nua_i_notify
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.7.3: PASSED\n");

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

#else
  (void)i;
  nua_set_params(b->nua, NUTAG_APPL_METHOD("NOTIFY"), TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);
#endif

  /* ---------------------------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-11.7.4: multiple unsolicited NOTIFYs\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_EXPIRES_STR("10"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.4aa"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: no"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=333"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.4a"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: no"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=3000"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.4b"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("terminated"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.4c"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  a->state.n = 4;
  b->state.n = 4;

  run_ab_until(ctx, -1, save_until_nth_final_response,
	       -1, accept_n_notifys);

  /* Notifier events: nua_r_notify nua_r_notify */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  /* subscriber events:
     nua_i_notify
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.7.4: PASSED\n");

  /* ---------------------------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-11.7.5: multiple unsolicited NOTIFYs\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=333"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.5a"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: no"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=3000"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.5b"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("terminated"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.5c"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  a->state.n = 3;
  b->state.n = 3;

  run_ab_until(ctx, -1, save_until_nth_final_response,
	       -1, accept_n_notifys);

  /* Notifier events: nua_r_notify nua_r_notify */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST(e->data->e_status, 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  /* subscriber events:
     nua_i_notify
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.7.5: PASSED\n");

  /* ---------------------------------------------------------------------- */

#if 0

  if (print_headings)
    printf("TEST NUA-11.7.6: unsolicited NOTIFY handle destroyed\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=333"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.6a"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: no"),
	 TAG_END());

  NOTIFY(a, a_call, a_call->nh,
	 NUTAG_URL(b->contact->m_url),
	 NUTAG_NEWSUB(1),
	 SIPTAG_SUBSCRIPTION_STATE_STR("active; expires=3000"),
	 SIPTAG_SUBJECT_STR("NUA-11.7.6b"),
	 SIPTAG_EVENT_STR("message-summary"),
	 SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
	 SIPTAG_PAYLOAD_STR("Messages-Waiting: yes"),
	 TAG_END());

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  a->state.n = 3;
  b->state.n = 3;

  run_b_until(ctx, -1, accept_n_notifys);

  /* subscriber events:
     nua_i_notify
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "active");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_active);
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(!e->next);

  free_events_in_list(ctx, b->events);

  if (print_headings)
    printf("TEST NUA-11.7.6: PASSED\n");

  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

#endif

  if (print_headings)
    printf("TEST NUA-11.7: PASSED\n");

  END();
}

/**Terminate when received notify.
 * Respond to NOTIFY with 200 OK if it has not been responded.
 * Save events (except nua_i_active or terminated).
 */
int accept_notify(CONDITION_PARAMS)
{
  if (event == nua_i_notify && status < 200)
    RESPOND(ep, call, nh, SIP_200_OK,
	    NUTAG_WITH_THIS(ep->nua),
	    TAG_END());

  save_event_in_list(ctx, event, ep, call);

  return event == nua_i_notify;
}

int save_until_nth_final_response(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (nua_r_set_params <= event && event < nua_i_network_changed
      && status >= 200) {
    if (ep->state.n > 0)
      ep->state.n--;
    return ep->state.n == 0;
  }

  return 0;
}

int accept_n_notifys(CONDITION_PARAMS)
{
  tagi_t const *substate = tl_find(tags, nutag_substate);

  if (event == nua_i_notify && status < 200)
    RESPOND(ep, call, nh, SIP_200_OK,
	    NUTAG_WITH_THIS(ep->nua),
	    TAG_END());

  save_event_in_list(ctx, event, ep, call);

  if (event != nua_i_notify)
    return 0;

  if (ep->state.n > 0)
    ep->state.n--;

  if (ep->state.n == 0)
    return 1;

  if (substate && substate->t_value == nua_substate_terminated) {
    if (call && call->nh == nh) {
      call->nh = NULL;
      nua_handle_destroy(nh);
    }
  }

  return 0;
}

/* ======================================================================== */

int save_until_subscription_terminated(CONDITION_PARAMS);
int accept_subscription_until_terminated(CONDITION_PARAMS);

/* Timeout subscription */
int test_subscription_timeout(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e, *en, *es;
  sip_t const *sip;
  tagi_t const *n_tags, *r_tags;

  if (print_headings)
    printf("TEST NUA-11.8: subscribe and wait until subscription times out\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  METHOD(a, a_call, a_call->nh,
	 NUTAG_METHOD("SUBSCRIBE"),
	 NUTAG_URL(b->contact->m_url),
	 SIPTAG_EVENT_STR("presence"),
	 SIPTAG_ACCEPT_STR("application/xpidf, application/pidf+xml"),
	 SIPTAG_EXPIRES_STR("2"),
	 NUTAG_APPL_METHOD("NOTIFY"),
	 NUTAG_DIALOG(2),
	 TAG_END());

  run_ab_until(ctx,
	       -1, save_until_subscription_terminated,
	       -1, accept_subscription_until_terminated);

  /* Client events:
     nua_method(), nua_i_notify/nua_r_method, nua_i_notify
  */
  TEST_1(en = event_by_type(a->events->head, nua_i_notify));
  TEST_1(es = event_by_type(a->events->head, nua_r_method));

  TEST_1(e = en); TEST_E(e->data->e_event, nua_i_notify);
  TEST_1(sip = sip_object(e->data->e_msg));
  n_tags = e->data->e_tags;

  TEST_1(e = es); TEST_E(e->data->e_event, nua_r_method);
  r_tags = e->data->e_tags;

  TEST_1(sip->sip_event); TEST_S(sip->sip_event->o_type, "presence");
  TEST_1(sip->sip_content_type);
  TEST_S(sip->sip_content_type->c_type, "application/pidf+xml");
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "pending");
  TEST_1(sip->sip_subscription_state->ss_expires);
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_pending);

  if (es->next == en)
    e = en->next;
  else
    e = es->next;

  TEST_1(e); TEST_E(e->data->e_event, nua_i_notify);
  n_tags = e->data->e_tags;
  TEST_1(tl_find(n_tags, nutag_substate));
  TEST(tl_find(n_tags, nutag_substate)->t_value, nua_substate_terminated);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(sip->sip_event);
  TEST_1(sip->sip_subscription_state);
  TEST_S(sip->sip_subscription_state->ss_substate, "terminated");
  TEST_1(!sip->sip_subscription_state->ss_expires);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  /* Server events: nua_i_subscribe, nua_r_notify */
  TEST_1(e = b->events->head);
  TEST_E(e->data->e_event, nua_i_subscribe);
  TEST_E(e->data->e_status, 100);
  TEST_1(sip = sip_object(e->data->e_msg));

  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_pending);

  /* Notifier events: 2nd nua_r_notify */
  TEST_1(e = e->next); TEST_E(e->data->e_event, nua_r_notify);
  TEST_1(e->data->e_status >= 200);
  r_tags = e->data->e_tags;
  TEST_1(tl_find(r_tags, nutag_substate));
  TEST(tl_find(r_tags, nutag_substate)->t_value, nua_substate_terminated);

  free_events_in_list(ctx, b->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-11.8: PASSED\n");

  END();
}

int save_until_subscription_terminated(CONDITION_PARAMS)
{
  void *with = nua_current_request(nua);

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_notify) {
    if (status < 200)
      RESPOND(ep, call, nh, SIP_200_OK, NUTAG_WITH(with), TAG_END());

    tags = tl_find(tags, nutag_substate);
    return tags && tags->t_value == nua_substate_terminated;
  }

  return 0;
}

int accept_subscription_until_terminated(CONDITION_PARAMS)
{
  void *with = nua_current_request(nua);

  save_event_in_list(ctx, event, ep, call);

  if (event == nua_i_subscribe && status < 200) {
    RESPOND(ep, call, nh, SIP_202_ACCEPTED,
	    NUTAG_WITH(with),
	    SIPTAG_EXPIRES_STR("360"),
	    TAG_END());
    NOTIFY(ep, call, nh,
	   SIPTAG_EVENT(sip->sip_event),
	   SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
	   SIPTAG_PAYLOAD_STR(presence_closed),
	   NUTAG_SUBSTATE(nua_substate_pending),
	   TAG_END());
  }
  else if (event == nua_r_notify) {
    tags = tl_find(tags, nutag_substate);
    return tags && tags->t_value == nua_substate_terminated;
  }

  return 0;
}


/* ======================================================================== */
/* Test simple methods: MESSAGE, PUBLISH, SUBSCRIBE/NOTIFY */

int test_simple(struct context *ctx)
{
  return 0
    || test_message(ctx)
    || test_publish(ctx)
    || test_subscribe_notify(ctx)
    || test_event_fetch(ctx)
    || test_newsub_notify(ctx)
    || test_subscribe_notify_graceful(ctx)
    || test_subscription_timeout(ctx)
    ;
}
