/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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

/**@CFILE check_events.c
 *
 * @brief NUA module tests for SIP events
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2008 Nokia Corporation.
 */

#include "config.h"

#undef NDEBUG

#include "check_nua.h"

#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/soa.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/su_tag_io.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* define XXX as 1 in order to see all failing test cases */
#ifndef XXX
#define XXX (0)
#endif

/* ====================================================================== */

static nua_t *nua;
static struct dialog *dialog = NULL;

#define CRLF "\r\n"

void s2_dialog_setup(void)
{
  nua = s2_nua_setup("simple",
		     SIPTAG_ORGANIZATION_STR("Pussy Galore's Flying Circus"),
                     NUTAG_OUTBOUND("no-options-keepalive, no-validate"),
		     TAG_END());

  dialog = su_home_new(sizeof *dialog); fail_if(!dialog);

  s2_register_setup();
}

void s2_dialog_teardown(void)
{
  s2_teardown_started("simple");

  s2_register_teardown();

  nua_shutdown(nua);

  fail_unless_event(nua_r_shutdown, 200);

  s2_nua_teardown();
}

static void simple_thread_setup(void)
{
  s2_nua_thread = 1;
  s2_dialog_setup();
}

static void simple_threadless_setup(void)
{
  s2_nua_thread = 0;
  s2_dialog_setup();
}

static void simple_teardown(void)
{
  s2_dialog_teardown();
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

static char const *event_type = "presence";
static char const *event_mime_type = "application/pidf+xml";
static char const *event_state = presence_open;
static char const *subscription_state = "active;expires=600";

static struct event *
respond_to_subscribe(struct message *subscribe,
		     nua_event_t expect_event,
		     enum nua_substate expect_substate,
		     int status, char const *phrase,
		     tag_type_t tag, tag_value_t value, ...)
{
  struct event *event;
  ta_list ta;

  ta_start(ta, tag, value);
  s2_sip_respond_to(subscribe, dialog, status, phrase,
		ta_tags(ta));
  ta_end(ta);

  event = s2_wait_for_event(expect_event, status); fail_if(!event);
  fail_unless(s2_check_substate(event, expect_substate));
  return event;
}

static struct event *
notify_to_nua(enum nua_substate expect_substate,
	      tag_type_t tag, tag_value_t value, ...)
{
  struct event *event;
  struct message *response;
  ta_list ta;

  ta_start(ta, tag, value);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_NOTIFY, NULL,
			SIPTAG_CONTENT_TYPE_STR(event_mime_type),
			SIPTAG_PAYLOAD_STR(event_state),
			ta_tags(ta)));
  ta_end(ta);

  response = s2_sip_wait_for_response(200, SIP_METHOD_NOTIFY);
  fail_if(!response);
  s2_sip_free_message(response);

  event = s2_wait_for_event(nua_i_notify, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, expect_substate));

  return event;
}

static int send_notify_before_response = 0;

static struct event *
subscription_by_nua(nua_handle_t *nh,
		    enum nua_substate current,
		    tag_type_t tag, tag_value_t value, ...)
{
  struct message *subscribe;
  struct event *notify, *event;
  ta_list ta;
  enum nua_substate substate = nua_substate_active;
  char const *substate_str = subscription_state;
  char const *expires = "600";

  subscribe = s2_sip_wait_for_request(SIP_METHOD_SUBSCRIBE);
  if (event_type)
    fail_if(!subscribe->sip->sip_event ||
	    strcmp(event_type, subscribe->sip->sip_event->o_type));

  if (subscribe->sip->sip_expires && subscribe->sip->sip_expires->ex_delta == 0) {
    substate = nua_substate_terminated;
    substate_str = "terminated;reason=timeout";
    expires = "0";
  }

  ta_start(ta, tag, value);

  if (send_notify_before_response) {
    s2_sip_save_uas_dialog(dialog, subscribe->sip);
    notify = notify_to_nua(substate,
			   SIPTAG_EVENT(subscribe->sip->sip_event),
			   SIPTAG_SUBSCRIPTION_STATE_STR(substate_str),
			   ta_tags(ta));
    event = respond_to_subscribe(subscribe, nua_r_subscribe, substate,
				 SIP_200_OK,
				 SIPTAG_EXPIRES_STR(expires),
				 TAG_END());
    s2_free_event(event);
  }
  else {
    event = respond_to_subscribe(subscribe, nua_r_subscribe, current,
				 SIP_202_ACCEPTED,
				 SIPTAG_EXPIRES_STR(expires),
				 TAG_END());
    s2_free_event(event);
    notify = notify_to_nua(substate,
			   SIPTAG_EVENT(subscribe->sip->sip_event),
			   SIPTAG_SUBSCRIPTION_STATE_STR(substate_str),
			   ta_tags(ta));
  }

  s2_sip_free_message(subscribe);

  return notify;
}

static void
unsubscribe_by_nua(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  struct message *subscribe, *response;
  struct event *event;

  nua_unsubscribe(nh, TAG_END());
  subscribe = s2_sip_wait_for_request(SIP_METHOD_SUBSCRIBE);

  s2_sip_respond_to(subscribe, dialog, SIP_200_OK, SIPTAG_EXPIRES_STR("0"), TAG_END());

  event = s2_wait_for_event(nua_r_unsubscribe, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_active));
  s2_free_event(event);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_NOTIFY, NULL,
			SIPTAG_EVENT(subscribe->sip->sip_event),
			SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=tiemout"),
			SIPTAG_CONTENT_TYPE_STR(event_mime_type),
			SIPTAG_PAYLOAD_STR(event_state),
			TAG_END()));

  event = s2_wait_for_event(nua_i_notify, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_terminated));
  s2_free_event(event);

  response = s2_sip_wait_for_response(200, SIP_METHOD_NOTIFY);
  fail_if(!response);
  s2_sip_free_message(response); s2_sip_free_message(subscribe);
}

/* ====================================================================== */
/* 6 - Subscribe/notify */

START_TEST(subscribe_6_1_1)
{
  nua_handle_t *nh;
  struct event *notify;
  S2_CASE("6.1.1", "Basic subscription",
	  "NUA sends SUBSCRIBE, waits for NOTIFY, sends un-SUBSCRIBE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_free_event(notify);
  unsubscribe_by_nua(nh, TAG_END());
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(subscribe_6_1_2)
{
  nua_handle_t *nh;
  struct message *subscribe, *response;
  struct event *notify, *event;

  S2_CASE("6.1.2", "Basic subscription with refresh",
	  "NUA sends SUBSCRIBE, waits for NOTIFY, "
	  "sends re-SUBSCRIBE, waits for NOTIFY, "
	  "sends un-SUBSCRIBE");

  send_notify_before_response = 1;

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_free_event(notify);

  /* Wait for refresh */
  s2_nua_fast_forward(600, s2base->root);
  subscribe = s2_sip_wait_for_request(SIP_METHOD_SUBSCRIBE);
  s2_sip_respond_to(subscribe, dialog, SIP_200_OK,
		SIPTAG_EXPIRES_STR("600"),
		TAG_END());

  event = s2_wait_for_event(nua_r_subscribe, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_active));
  s2_free_event(event);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_NOTIFY, NULL,
			SIPTAG_EVENT(subscribe->sip->sip_event),
			SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=600"),
			SIPTAG_CONTENT_TYPE_STR(event_mime_type),
			SIPTAG_PAYLOAD_STR(event_state),
			TAG_END()));
  event = s2_wait_for_event(nua_i_notify, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_active));
  s2_free_event(event);
  response = s2_sip_wait_for_response(200, SIP_METHOD_NOTIFY);
  fail_if(!response);
  s2_sip_free_message(response);

  unsubscribe_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(subscribe_6_1_3)
{
  nua_handle_t *nh;
  struct message *response;
  struct event *notify, *event;

  S2_CASE("6.1.3", "Subscription terminated by notifier",
	  "NUA sends SUBSCRIBE, waits for NOTIFY, "
	  "gets NOTIFY terminating the subscription,");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_free_event(notify);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_NOTIFY, NULL,
			SIPTAG_EVENT_STR(event_type),
			SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
			TAG_END()));
  event = s2_wait_for_event(nua_i_notify, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_terminated));
  s2_free_event(event);
  response = s2_sip_wait_for_response(200, SIP_METHOD_NOTIFY);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(subscribe_6_1_4)
{
  nua_handle_t *nh;
  struct message *response;
  struct event *notify, *event;

  S2_CASE("6.1.4", "Subscription terminated by notifier, re-established",
	  "NUA sends SUBSCRIBE, waits for NOTIFY, "
	  "gets NOTIFY terminating the subscription,");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_free_event(notify);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_NOTIFY, NULL,
			SIPTAG_EVENT_STR(event_type),
			SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=deactivated"),
			TAG_END()));
  event = s2_wait_for_event(nua_i_notify, 200); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_embryonic));
  s2_free_event(event);
  response = s2_sip_wait_for_response(200, SIP_METHOD_NOTIFY);
  fail_if(!response);
  s2_sip_free_message(response);

  su_home_unref((void *)dialog), dialog = su_home_new(sizeof *dialog); fail_if(!dialog);

  s2_nua_fast_forward(5, s2base->root);
  /* nua re-establishes the subscription */
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_free_event(notify);

  /* Unsubscribe with nua_subscribe() Expires: 0 */
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), SIPTAG_EXPIRES_STR("0"), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_active, TAG_END());
  s2_free_event(notify);

  nua_handle_destroy(nh);
}
END_TEST

TCase *subscribe_tcase(int threading)
{
  TCase *tc = tcase_create("6.1 - Basic SUBSCRIBE_");
  void (*simple_setup)(void);

  simple_setup = threading ? simple_thread_setup : simple_threadless_setup;
  tcase_add_checked_fixture(tc, simple_setup, simple_teardown);

  {
    tcase_add_test(tc, subscribe_6_1_1);
    tcase_add_test(tc, subscribe_6_1_2);
    tcase_add_test(tc, subscribe_6_1_3);
    tcase_add_test(tc, subscribe_6_1_4);
  }
  return tc;
}

START_TEST(fetch_6_2_1)
{
  nua_handle_t *nh;
  struct event *notify;

  S2_CASE("6.2.1", "Event fetch - NOTIFY after 202",
	  "NUA sends SUBSCRIBE with Expires 0, waits for NOTIFY");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), SIPTAG_EXPIRES_STR("0"), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_check_substate(notify, nua_substate_terminated);
  s2_free_event(notify);
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(fetch_6_2_2)
{
  nua_handle_t *nh;
  struct event *notify;

  S2_CASE("6.2.2", "Event fetch - NOTIFY before 200",
	  "NUA sends SUBSCRIBE with Expires 0, waits for NOTIFY");

  send_notify_before_response = 1;

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), SIPTAG_EXPIRES_STR("0"), TAG_END());
  notify = subscription_by_nua(nh, nua_substate_embryonic, TAG_END());
  s2_check_substate(notify, nua_substate_terminated);
  s2_free_event(notify);
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(fetch_6_2_3)
{
  nua_handle_t *nh;
  struct message *subscribe;
  struct event *event;

  S2_CASE("6.2.3", "Event fetch - no NOTIFY",
	  "NUA sends SUBSCRIBE with Expires 0, waits for NOTIFY, times out");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  nua_subscribe(nh, SIPTAG_EVENT_STR(event_type), SIPTAG_EXPIRES_STR("0"), TAG_END());
  subscribe = s2_sip_wait_for_request(SIP_METHOD_SUBSCRIBE);
  s2_sip_respond_to(subscribe, dialog, SIP_202_ACCEPTED,
		SIPTAG_EXPIRES_STR("0"), TAG_END());
  s2_sip_free_message(subscribe);

  event = s2_wait_for_event(nua_r_subscribe, 202); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_embryonic));
  s2_free_event(event);

  s2_nua_fast_forward(600, s2base->root);

  event = s2_wait_for_event(nua_i_notify, 408); fail_if(!event);
  fail_unless(s2_check_substate(event, nua_substate_terminated));
  s2_free_event(event);

  nua_handle_destroy(nh);
}
END_TEST


TCase *fetch_tcase(int threading)
{
  TCase *tc = tcase_create("6.2 - Event fetch");
  void (*simple_setup)(void);

  simple_setup = threading ? simple_thread_setup : simple_threadless_setup;
  tcase_add_checked_fixture(tc, simple_setup, simple_teardown);

  {
    tcase_add_test(tc, fetch_6_2_1);
    tcase_add_test(tc, fetch_6_2_2);
    tcase_add_test(tc, fetch_6_2_3);
  }
  return tc;
}

nua_handle_t *
subscribe_to_nua(char const *event,
		 tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct event *subscribe;
  struct message *response;
  nua_handle_t *nh;

  nua_set_params(nua, NUTAG_APPL_METHOD("SUBSCRIBE"),
		 SIPTAG_ALLOW_EVENTS_STR(event),
		 TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  ta_start(ta, tag, value);
  s2_sip_request_to(dialog, SIP_METHOD_SUBSCRIBE, NULL,
		SIPTAG_EVENT_STR(event),
		ta_tags(ta));
  ta_end(ta);

  subscribe = s2_wait_for_event(nua_i_subscribe, 100);
  nh = subscribe->nh;
  nua_respond(nh, SIP_202_ACCEPTED,
	      NUTAG_WITH_SAVED(subscribe->event),
	      TAG_END());
  s2_free_event(subscribe);

  response = s2_sip_wait_for_response(202, SIP_METHOD_SUBSCRIBE);
  s2_sip_update_dialog(dialog, response);
  fail_unless(response->sip->sip_expires != NULL);
  s2_sip_free_message(response);

  return nh;
}

START_TEST(notify_6_3_1)
{
  nua_handle_t *nh;
  struct event *subscribe;
  struct message *notify, *response;
  sip_t *sip;

  S2_CASE("6.3.1", "Basic NOTIFY server",
	  "NUA receives SUBSCRIBE, sends 202 and NOTIFY. "
	  "First NOTIFY terminates subscription. ");

  s2_sip_request_to(dialog, SIP_METHOD_SUBSCRIBE, NULL,
		    SIPTAG_EVENT_STR("presence"),
		    TAG_END());
  /* 489 Bad Event by default */
  s2_sip_check_response(489, SIP_METHOD_SUBSCRIBE);

  nua_set_params(nua, NUTAG_APPL_METHOD("SUBSCRIBE"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_sip_request_to(dialog, SIP_METHOD_SUBSCRIBE, NULL,
		SIPTAG_EVENT_STR("presence"),
		TAG_END());
  s2_sip_check_response(489, SIP_METHOD_SUBSCRIBE);

  nua_set_params(nua, SIPTAG_ALLOW_EVENTS_STR("presence"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_sip_request_to(dialog, SIP_METHOD_SUBSCRIBE, NULL,
		SIPTAG_EVENT_STR("presence"),
		TAG_END());
  subscribe = s2_wait_for_event(nua_i_subscribe, 100);
  nh = subscribe->nh;
  nua_respond(nh, SIP_403_FORBIDDEN,
	      NUTAG_WITH_SAVED(subscribe->event),
	      TAG_END());
  s2_free_event(subscribe);

  s2_sip_check_response(403, SIP_METHOD_SUBSCRIBE);

  nua_handle_destroy(nh);

  s2_sip_request_to(dialog, SIP_METHOD_SUBSCRIBE, NULL,
		SIPTAG_EVENT_STR("presence"),
		TAG_END());
  subscribe = s2_wait_for_event(nua_i_subscribe, 100);
  nh = subscribe->nh;
  nua_respond(nh, SIP_202_ACCEPTED,
	      NUTAG_WITH_SAVED(subscribe->event),
	      TAG_END());
  s2_free_event(subscribe);

  response = s2_sip_wait_for_response(202, SIP_METHOD_SUBSCRIBE);
  s2_sip_update_dialog(dialog, response);
  fail_unless(response->sip->sip_expires != NULL);
  s2_sip_free_message(response);

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_terminated),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "terminated"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());

  fail_unless_event(nua_r_notify, 200);
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(notify_6_3_2)
{
  nua_handle_t *nh;
  struct message *notify;
  sip_t *sip;

  S2_CASE("6.3.2", "NOTIFY server - automatic subscription termination",
	  "NUA receives SUBSCRIBE, sends 202 and NOTIFY. "
	  "The subscription terminates with timeout. ");

  nh = subscribe_to_nua("presence", SIPTAG_EXPIRES_STR("300"), TAG_END());

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_notify, 200);

  s2_nua_fast_forward(300, s2base->root);

  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "terminated"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_notify, 200);

  nua_handle_destroy(nh);
}
END_TEST

static int
s2_event_substate(struct event *event)
{
  if (event) {
    tagi_t const *t = tl_find(event->data->e_tags, nutag_substate);
    if (t)
      return t->t_value;
  }
  return -1;
}

START_TEST(notify_6_3_3)
{
  nua_handle_t *nh;
  struct message *notify;
  struct event *response;
  sip_t *sip;

  S2_CASE("6.3.3", "NOTIFY server - terminate with error response to NOTIFY",
	  "NUA receives SUBSCRIBE, sends 202 and NOTIFY. "
	  "The subscription terminates when watcher "
	  "returns 481 to second NOTIFY.");

  nh = subscribe_to_nua("presence", SIPTAG_EXPIRES_STR("300"), TAG_END());

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_notify, 200);

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  response = s2_wait_for_event(nua_r_notify, 481);
  fail_unless(s2_event_substate(response) == nua_substate_terminated);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(notify_6_3_4)
{
  nua_handle_t *nh;
  struct message *notify;
  struct event *response;
  sip_t *sip;

  S2_CASE("6.3.4", "NOTIFY server - terminate with error response to NOTIFY",
	  "NUA receives SUBSCRIBE, sends 202 and NOTIFY. "
	  "The subscription terminates when watcher "
	  "returns 481 to second NOTIFY. The queued 3rd NOTIFY gets "
	  "responded by stack.");

  nh = subscribe_to_nua("presence", SIPTAG_EXPIRES_STR("300"), TAG_END());

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_notify, 200);

  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_open),
	     TAG_END());
  nua_notify(nh,
	     NUTAG_SUBSTATE(nua_substate_active),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  response = s2_wait_for_event(nua_r_notify, 481);
  fail_unless(s2_event_substate(response) == nua_substate_terminated);
  response = s2_wait_for_event(nua_r_notify, 481);
  fail_unless(s2_event_substate(response) == nua_substate_terminated);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(notify_6_3_5)
{
  nua_handle_t *nh;
  struct message *notify;
  struct event *response;
  sip_t *sip;

  S2_CASE("6.3.4", "NOTIFY server - terminate with error response to NOTIFY",
	  "NUA receives SUBSCRIBE, sends 202 and NOTIFY. "
	  "The subscription terminates when watcher "
	  "returns 481 to NOTIFY.");

  nh = subscribe_to_nua("presence", SIPTAG_EXPIRES_STR("300"), TAG_END());

  nua_notify(nh,
	     SIPTAG_SUBSCRIPTION_STATE_STR("active"),
	     SIPTAG_PAYLOAD_STR(presence_closed),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_notify, 200);

  nua_notify(nh,
	     SIPTAG_SUBSCRIPTION_STATE_STR("active"),
	     SIPTAG_PAYLOAD_STR(presence_open),
	     TAG_END());
  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless(notify != NULL);
  sip = notify->sip;
  fail_unless(sip->sip_subscription_state != NULL);
  fail_unless(su_strmatch(sip->sip_subscription_state->ss_substate,
			  "active"));

  nua_notify(nh,
	     NUTAG_NEWSUB(1),
	     SIPTAG_SUBSCRIPTION_STATE_STR("active"),
	     SIPTAG_PAYLOAD_STR(presence_open),
	     TAG_END());

  s2_sip_respond_to(notify, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  response = s2_wait_for_event(nua_r_notify, 481);
  fail_unless(s2_event_substate(response) == nua_substate_terminated);

  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  s2_sip_respond_to(notify, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  response = s2_wait_for_event(nua_r_notify, 481);
  fail_unless(s2_event_substate(response) == nua_substate_terminated);

  nua_handle_destroy(nh);
}
END_TEST

TCase *notifier_tcase(int threading)
{
  TCase *tc = tcase_create("6.3 - Basic event server with NOTIFY ");
  void (*simple_setup)(void);

  simple_setup = threading ? simple_thread_setup : simple_threadless_setup;
  tcase_add_checked_fixture(tc, simple_setup, simple_teardown);

  {
    tcase_add_test(tc, notify_6_3_1);
    tcase_add_test(tc, notify_6_3_2);
    tcase_add_test(tc, notify_6_3_3);
    tcase_add_test(tc, notify_6_3_4);
    tcase_add_test(tc, notify_6_3_5);
  }
  return tc;
}

/* ====================================================================== */

/* Test case template */

START_TEST(empty)
{
  S2_CASE("0.0.0", "Empty test case",
	  "Detailed explanation for empty test case.");

  tport_set_params(s2sip->master, TPTAG_LOG(1), TAG_END());
  s2_setup_logs(7);
  s2_setup_logs(0);
  tport_set_params(s2sip->master, TPTAG_LOG(0), TAG_END());
}

END_TEST

static TCase *empty_tcase(int threading)
{
  TCase *tc = tcase_create("0 - Empty");
  void (*simple_setup)(void);

  simple_setup = threading ? simple_thread_setup : simple_threadless_setup;
  tcase_add_checked_fixture(tc, simple_setup, simple_teardown);

  tcase_add_test(tc, empty);

  return tc;
}

/* ====================================================================== */

void check_simple_cases(Suite *suite, int threading)
{
  suite_add_tcase(suite, subscribe_tcase(threading));
  suite_add_tcase(suite, fetch_tcase(threading));
  suite_add_tcase(suite, notifier_tcase(threading));

  if (0)			/* Template */
    suite_add_tcase(suite, empty_tcase(threading));
}

