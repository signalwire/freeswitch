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

/**@CFILE check_session.c
 *
 * @brief NUA module tests for SIP session handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2008 Nokia Corporation.
 */

#include "config.h"

#include "check_nua.h"

#include "test_s2.h"

#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/soa.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_io.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ====================================================================== */
/* Call cases */

static nua_t *nua;
static soa_session_t *soa = NULL;
static struct dialog *dialog = NULL;

#define CRLF "\r\n"

static void call_setup(void)
{
  s2_case("0.1.1", "Setup for Call Tests", "");

  nua = s2_nua_setup(TAG_END());

  soa = soa_create(NULL, s2->root, NULL);

  fail_if(!soa);

  soa_set_params(soa,
		 SOATAG_USER_SDP_STR("m=audio 5008 RTP/AVP 8 0" CRLF
				     "m=video 5010 RTP/AVP 34" CRLF),
		 TAG_END());

  dialog = su_home_new(sizeof *dialog); fail_if(!dialog);

  s2_register_setup();
}

static void call_teardown(void)
{
  s2_case("0.1.2", "Teardown Call Test Setup", "");

  mark_point();

  s2_register_teardown();

  nua_shutdown(nua);
  fail_unless(s2_check_event(nua_r_shutdown, 200));

  s2_nua_teardown();
}

static void save_sdp_to_soa(struct message *message)
{
  sip_payload_t *pl;
  char const *body;
  isize_t bodylen;

  fail_if(!message);

  fail_if(!message->sip->sip_content_length);
  fail_if(!message->sip->sip_content_type);
  fail_if(strcmp(message->sip->sip_content_type->c_type,
		 "application/sdp"));

  fail_if(!message->sip->sip_payload);
  pl = message->sip->sip_payload;
  body = pl->pl_data, bodylen = pl->pl_len;

  fail_if(soa_set_remote_sdp(soa, NULL, body, (issize_t)bodylen) < 0);
}

static void process_offer(struct message *message)
{
  save_sdp_to_soa(message);
  fail_if(soa_generate_answer(soa, NULL) < 0);
}

static void process_answer(struct message *message)
{
  save_sdp_to_soa(message);
  fail_if(soa_process_answer(soa, NULL) < 0);
}

static void
respond_with_sdp(struct message *request,
		 struct dialog *dialog,
		 int status, char const *phrase,
		 tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  char const *body;
  isize_t bodylen;

  fail_if(soa_get_local_sdp(soa, NULL, &body, &bodylen) != 1);

  ta_start(ta, tag, value);
  s2_respond_to(request, dialog, status, phrase,
		SIPTAG_CONTENT_TYPE_STR("application/sdp"),
		SIPTAG_PAYLOAD_STR(body),
		SIPTAG_CONTENT_DISPOSITION_STR("session"),
		ta_tags(ta));
  ta_end(ta);
}

static void
request_with_sdp(struct dialog *dialog,
		 sip_method_t method, char const *name,
		 tport_t *tport,
		 tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  char const *body;
  isize_t bodylen;

  fail_if(soa_get_local_sdp(soa, NULL, &body, &bodylen) != 1);

  ta_start(ta, tag, value);
  fail_if(
    s2_request_to(dialog, method, name, tport,
		  SIPTAG_CONTENT_TYPE_STR("application/sdp"),
		  SIPTAG_PAYLOAD_STR(body),
		  ta_tags(ta)));
  ta_end(ta);
}

static struct message *
invite_sent_by_nua(nua_handle_t *nh,
		   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);
  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     ta_tags(ta));
  ta_end(ta);

  fail_unless(s2_check_callstate(nua_callstate_calling));

  return s2_wait_for_request(SIP_METHOD_INVITE);
}

static struct message *
respond_with_100rel(struct message *invite,
		    struct dialog *d,
		    int sdp,
		    int status, char const *phrase,
		    tag_type_t tag, tag_value_t value, ...)
{
  struct message *prack;
  ta_list ta;
  static uint32_t rseq;
  sip_rseq_t rs[1];

  assert(100 < status && status < 200);

  sip_rseq_init(rs);
  rs->rs_response = ++rseq;

  ta_start(ta, tag, value);

  if (sdp) {
    respond_with_sdp(
      invite, dialog, status, phrase,
      SIPTAG_REQUIRE_STR("100rel"),
      SIPTAG_RSEQ(rs),
      ta_tags(ta));
  }
  else {
    s2_respond_to(
      invite, dialog, status, phrase,
      SIPTAG_REQUIRE_STR("100rel"),
      SIPTAG_RSEQ(rs),
      ta_tags(ta));
  }
  ta_end(ta);

  fail_unless(s2_check_event(nua_r_invite, status));

  prack = s2_wait_for_request(SIP_METHOD_PRACK);
  /* Assumes auto-prack, so there is no offer in prack */
  s2_respond_to(prack, dialog, SIP_200_OK, TAG_END());

  return prack;
}

static void
invite_by_nua(nua_handle_t *nh,
	      tag_type_t tag, tag_value_t value, ...)
{
  struct message *invite;
  ta_list ta;

  ta_start(ta, tag, value);
  invite = invite_sent_by_nua(
    nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    ta_tags(ta));
  ta_end(ta);

  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());

  fail_unless(s2_check_event(nua_r_invite, 180));
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_free_message(invite);
  fail_unless(s2_check_event(nua_r_invite, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_check_request(SIP_METHOD_ACK));
}

static nua_handle_t *
invite_to_nua(tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct event *invite;
  struct message *response;
  nua_handle_t *nh;
  sip_cseq_t cseq[1];

  soa_generate_offer(soa, 1, NULL);

  ta_start(ta, tag, value);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, ta_tags(ta));
  ta_end(ta);

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh;
  fail_if(!nh);

  sip_cseq_init(cseq);
  cseq->cs_method = sip_method_ack;
  cseq->cs_method_name = "ACK";
  cseq->cs_seq = sip_object(invite->data->e_msg)->sip_cseq->cs_seq;

  s2_free_event(invite);

  response = s2_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_update_dialog(dialog, response);
  process_answer(response);
  s2_free_message(response);

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_update_dialog(dialog, response);
  s2_free_message(response);

  fail_if(s2_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_CSEQ(cseq), TAG_END()));

  fail_unless(s2_check_event(nua_i_ack, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));

  return nh;
}

static void
bye_by_nua(nua_handle_t *nh,
	   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct message *bye;

  ta_start(ta, tag, value);
  nua_bye(nh, ta_tags(ta));
  ta_end(ta);

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
}

static void 
bye_by_nua_challenged(nua_handle_t *nh,
		      tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct message *bye;

  s2_flush_events();

  ta_start(ta, tag, value);
  nua_bye(nh, ta_tags(ta));
  ta_end(ta);

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 407));

  nua_authenticate(nh, NUTAG_AUTH("Digest:\"s2test\":abc:abc"), TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_if(s2->events);
}


static void 
cancel_by_nua(nua_handle_t *nh,
	      struct message *invite,
	      struct dialog *dialog,
	      tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct message *cancel;

  ta_start(ta, tag, value);
  nua_cancel(nh, ta_tags(ta));
  ta_end(ta);

  cancel = s2_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_free_message(cancel);
  fail_unless(s2_check_event(nua_r_cancel, 200));

  s2_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  fail_unless(s2_check_request(SIP_METHOD_ACK));

  fail_unless(s2_check_event(nua_r_invite, 487));
}

static void 
bye_to_nua(nua_handle_t *nh,
	   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  ta_start(ta, tag, value);
  fail_if(s2_request_to(dialog, SIP_METHOD_BYE, NULL, ta_tags(ta)));
  ta_end(ta);

  fail_unless(s2_check_event(nua_i_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_check_response(200, SIP_METHOD_BYE));
}

/* ====================================================================== */
/* 2 - Call cases */

/* 2.1 - Basic call cases */

START_TEST(call_2_1_1)
{
  nua_handle_t *nh;

  s2_case("2.1.1", "Basic call",
	  "NUA sends INVITE, NUA sends BYE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite_by_nua(nh, TAG_END());

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_2)
{
  nua_handle_t *nh;

  s2_case("2.1.2", "Basic call",
	  "NUA sends INVITE, NUA receives BYE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite_by_nua(nh, TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_3)
{
  nua_handle_t *nh;

  s2_case("2.1.3", "Incoming call",
	  "NUA receives INVITE and BYE");

  nh = invite_to_nua(TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_4)
{
  nua_handle_t *nh;

  s2_case("2.1.4", "Incoming call",
	  "NUA receives INVITE and sends BYE");

  nh = invite_to_nua(TAG_END());

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_5)
{
  nua_handle_t *nh;

  s2_case("2.1.5", "Incoming call",
	  "NUA receives INVITE and sends BYE, BYE is challenged");

  nh = invite_to_nua(TAG_END());

  bye_by_nua_challenged(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_6)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *invite;
  struct message *response;
  sip_cseq_t cseq[1];

  s2_case("2.1.6", "Basic call",
	  "NUA received INVITE, "
	  "NUA responds (and saves proxy for dialog), "
	  "NUA sends BYE");

  soa_generate_offer(soa, 1, NULL);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh;
  fail_if(!nh);

  sip_cseq_init(cseq);
  cseq->cs_method = sip_method_ack;
  cseq->cs_method_name = "ACK";
  cseq->cs_seq = sip_object(invite->data->e_msg)->sip_cseq->cs_seq;

  s2_free_event(invite);

  response = s2_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      /* Dialog-specific proxy is saved */
	      NUTAG_PROXY(s2->tcp.contact->m_url),
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_update_dialog(dialog, response);
  process_answer(response);
  s2_free_message(response);

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_update_dialog(dialog, response);
  s2_free_message(response);

  fail_if(s2_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_CSEQ(cseq), TAG_END()));

  fail_unless(s2_check_event(nua_i_ack, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));

  nua_bye(nh, TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  /* Check that NUA used dialog-specific proxy with BYE */
  fail_unless(tport_is_tcp(bye->tport));
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_7)
{
  nua_handle_t *nh, *nh2;
  sip_replaces_t *replaces;

  s2_case("2.1.7", "Call lookup",
	  "Test dialog and call-id lookup");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite_by_nua(nh, TAG_END());

  nh2 = nua_handle_by_call_id(nua, dialog->call_id->i_id);
  fail_if(!nh2); fail_if(nh != nh2); nua_handle_unref(nh2);

  replaces = sip_replaces_format(NULL, "%s;from-tag=%s;to-tag=%s",
				 dialog->call_id->i_id,
				 dialog->local->a_tag,
				 dialog->remote->a_tag);
  fail_if(!replaces);

  nh2 = nua_handle_by_replaces(nua, replaces);
  fail_if(!nh2); fail_if(nh != nh2); nua_handle_unref(nh2);

  msg_header_free_all(NULL, (msg_header_t *)replaces);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


TCase *invite_tcase(void)
{
  TCase *tc = tcase_create("2.1 - Basic INVITE");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  {
    tcase_add_test(tc, call_2_1_1);
    tcase_add_test(tc, call_2_1_2);
    tcase_add_test(tc, call_2_1_3);
    tcase_add_test(tc, call_2_1_4);
    tcase_add_test(tc, call_2_1_5);
    tcase_add_test(tc, call_2_1_6);
    tcase_add_test(tc, call_2_1_7);
  }
  return tc;
}

/* ---------------------------------------------------------------------- */
/* 2.2 - Call CANCEL cases */

START_TEST(cancel_outgoing)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  s2_case("2.2.1", "Cancel call",
	  "NUA is callee, NUA sends CANCEL immediately");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));
  nua_cancel(nh, TAG_END());

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  fail_if(s2->received != NULL);
  s2_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());
  cancel = s2_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_respond_to(cancel, dialog, SIP_200_OK, TAG_END());

  fail_unless(s2_check_request(SIP_METHOD_ACK));

  fail_unless(s2_check_event(nua_r_invite, 487));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_check_event(nua_r_cancel, 200));
  fail_if(s2->events != NULL);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_outgoing_after_100)
{
  nua_handle_t *nh;
  struct message *invite;

  s2_case("2.2.2", "Canceled call",
	  "NUA is callee, NUA sends CANCEL after receiving 100");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  s2_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());

  cancel_by_nua(nh, invite, dialog, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_outgoing_after_180)
{
  nua_handle_t *nh;
  struct message *invite;

  s2_case("2.2.3", "Canceled call",
	  "NUA is callee, NUA sends CANCEL after receiving 180");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless(s2_check_event(nua_r_invite, 180));
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  cancel_by_nua(nh, invite, dialog, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_outgoing_glare)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  s2_case("2.2.4", "Cancel and 200 OK glare",
	  "NUA is callee, NUA sends CANCEL after receiving 180 "
	  "but UAS already sent 200 OK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless(s2_check_event(nua_r_invite, 180));
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_cancel(nh, TAG_END());
  cancel = s2_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());

  s2_respond_to(cancel, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  s2_free_message(cancel);
  fail_unless(s2_check_event(nua_r_cancel, 481));

  fail_unless(s2_check_request(SIP_METHOD_ACK));

  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


TCase *cancel_tcase(void)
{
  TCase *tc = tcase_create("2.2 - CANCEL");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);

  tcase_add_test(tc, cancel_outgoing);
  tcase_add_test(tc, cancel_outgoing_after_100);
  tcase_add_test(tc, cancel_outgoing_after_180);
  tcase_add_test(tc, cancel_outgoing_glare);

  return tc;
}


/* ---------------------------------------------------------------------- */
/* 2.3 - Session timers */

static void invite_timer_round(nua_handle_t *nh,
			       char const *session_expires)
{
  struct message *invite, *ack;

  fail_unless(s2_check_callstate(nua_callstate_calling));
  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_200_OK,
    SIPTAG_SESSION_EXPIRES_STR(session_expires),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());
  s2_free_message(invite);
  fail_unless(s2_check_event(nua_r_invite, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_wait_for_request(SIP_METHOD_ACK);
  s2_free_message(ack);
}

START_TEST(call_to_nua_with_timer)
{
  nua_handle_t *nh;

  s2_case("2.3.1", "Incoming call with call timers",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends re-INVITE twice, "
	  "sends BYE.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_fast_forward(300);
  invite_timer_round(nh, "300;refresher=uac");
  s2_fast_forward(300);
  invite_timer_round(nh, "300;refresher=uac");

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_to_nua_with_timer_2)
{
  nua_handle_t *nh;

  s2_case("2.3.2", "Incoming call with call timers",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends re-INVITE, "
	  "sends BYE.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_fast_forward(300);
  invite_timer_round(nh, "300");
  s2_fast_forward(300);
  invite_timer_round(nh, "300");

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


TCase *session_timer_tcase(void)
{
  TCase *tc = tcase_create("2.3 - Session timers");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  {
    tcase_add_test(tc, call_to_nua_with_timer);
    tcase_add_test(tc, call_to_nua_with_timer_2);
  }
  return tc;
}

/* ====================================================================== */
/* 2.4 - 100rel */

START_TEST(call_with_prack_by_nua)
{
  nua_handle_t *nh;
  struct message *invite, *prack;

  s2_case("2.4.1", "Call with 100rel",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "receives 200, send ACK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite = invite_sent_by_nua(
    nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    TAG_END());
  process_offer(invite);

  prack = respond_with_100rel(invite, dialog, 1,
			      SIP_183_SESSION_PROGRESS,
			      TAG_END());
  s2_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless(s2_check_event(nua_r_prack, 200));

  prack = respond_with_100rel(invite, dialog, 0,
			      SIP_180_RINGING,
			      TAG_END());
  s2_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless(s2_check_event(nua_r_prack, 200));

  s2_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_free_message(invite);
  fail_unless(s2_check_event(nua_r_invite, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_check_request(SIP_METHOD_ACK));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_with_prack_sans_soa)
{
  nua_handle_t *nh;
  struct message *invite, *prack;

  s2_case("2.4.1", "Call with 100rel",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "receives 200, send ACK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite = invite_sent_by_nua(
    nh,
    NUTAG_MEDIA_ENABLE(0),
    SIPTAG_CONTENT_TYPE_STR("application/sdp"),
    SIPTAG_PAYLOAD_STR(
      "v=0" CRLF
      "o=- 6805647540234172778 5821668777690722690 IN IP4 127.0.0.1" CRLF
      "s=-" CRLF
      "c=IN IP4 127.0.0.1" CRLF
      "m=audio 5004 RTP/AVP 0 8" CRLF),
    TAG_END());

  prack = respond_with_100rel(invite, dialog, 0,
			      SIP_183_SESSION_PROGRESS,
			      TAG_END());
  s2_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless(s2_check_event(nua_r_prack, 200));

  prack = respond_with_100rel(invite, dialog, 0,
			      SIP_180_RINGING,
			      TAG_END());
  s2_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless(s2_check_event(nua_r_prack, 200));

  s2_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_free_message(invite);
  fail_unless(s2_check_event(nua_r_invite, 200));
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_check_request(SIP_METHOD_ACK));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

TCase *invite_100rel_tcase(void)
{
  TCase *tc = tcase_create("2.4 - INVITE with 100rel");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  {
    tcase_add_test(tc, call_with_prack_by_nua);
    tcase_add_test(tc, call_with_prack_sans_soa);
  }
  return tc;
}

/* ====================================================================== */
/* 3.1 - Call error cases */

START_TEST(call_forbidden)
{
  nua_handle_t *nh;
  struct message *invite;

  s2_case("3.1.1", "Call failure", "Call fails with 403 response");
  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local),
		  TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_respond_to(invite, NULL, SIP_403_FORBIDDEN, TAG_END());
  s2_free_message(invite);

  fail_unless(s2_check_request(SIP_METHOD_ACK));
  fail_unless(s2_check_event(nua_r_invite, 403));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(too_many_retrys)
{
  nua_handle_t *nh;
  struct message *invite;
  int i;

  s2_case("3.1.2", "Call fails after too many retries",
	  "Call fails after 4 times 500 Retry-After");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local),
		  NUTAG_RETRY_COUNT(3),
		  TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  for (i = 0;; i++) {
    fail_unless(s2_check_callstate(nua_callstate_calling));
    invite = s2_wait_for_request(SIP_METHOD_INVITE);
    fail_if(!invite);
    s2_respond_to(invite, NULL, SIP_500_INTERNAL_SERVER_ERROR,
		  SIPTAG_RETRY_AFTER_STR("5"),
		  TAG_END());
    s2_free_message(invite);
    fail_unless(s2_check_request(SIP_METHOD_ACK));
    if (i == 3)
      break;
    fail_unless(s2_check_event(nua_r_invite, 100));
    s2_fast_forward(5);
  }

  fail_unless(s2_check_event(nua_r_invite, 500));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(reinvite_forbidden)
{
  nua_handle_t *nh;
  struct message *invite;

  s2_case("3.2.1", "Re-INVITE failure", "Re-INVITE fails with 403 response");
  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local),
		  TAG_END());

  invite_by_nua(nh, TAG_END());

  nua_invite(nh, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_respond_to(invite, NULL, SIP_403_FORBIDDEN, TAG_END());
  s2_free_message(invite);

  fail_unless(s2_check_request(SIP_METHOD_ACK));
  fail_unless(s2_check_event(nua_r_invite, 403));
  /* Return to previous state */
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());
}
END_TEST


START_TEST(reinvite_too_many_retrys)
{
  nua_handle_t *nh;
  struct message *invite, *bye;
  int i;

  s2_case("3.2.2", "Re-INVITE fails after too many retries",
	  "Call fails after 4 times 500 Retry-After");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local),
		  NUTAG_RETRY_COUNT(3),
		  TAG_END());

  invite_by_nua(nh, TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  for (i = 0;; i++) {
    fail_unless(s2_check_callstate(nua_callstate_calling));
    invite = s2_wait_for_request(SIP_METHOD_INVITE);
    fail_if(!invite);
    s2_respond_to(invite, NULL, SIP_500_INTERNAL_SERVER_ERROR,
		  SIPTAG_RETRY_AFTER_STR("5"),
		  TAG_END());
    s2_free_message(invite);
    fail_unless(s2_check_request(SIP_METHOD_ACK));
    if (i == 3)
      break;
    fail_unless(s2_check_event(nua_r_invite, 100));
    s2_fast_forward(5);
  }

  fail_unless(s2_check_event(nua_r_invite, 500));
  /* Graceful termination */
  fail_unless(s2_check_callstate(nua_callstate_terminating));
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


TCase *invite_error_tcase(void)
{
  TCase *tc = tcase_create("3 - Call Errors");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  {
    tcase_add_test(tc, call_forbidden);
    tcase_add_test(tc, too_many_retrys);
    tcase_add_test(tc, reinvite_forbidden);
    tcase_add_test(tc, reinvite_too_many_retrys);
    tcase_set_timeout(tc, 5);
  }
  return tc;
}


/* ====================================================================== */
/* Weird call termination cases */

START_TEST(bye_4_1_1)
{
  nua_handle_t *nh;
  struct message *bye, *r481;

  s2_case("4.1.1", "Re-INVITE while terminating",
	  "NUA sends BYE, "
	  "BYE is challenged, "
	  "and NUA is re-INVITEd at the same time.");

  nh = invite_to_nua(TAG_END());

  s2_flush_events();

  nua_bye(nh, TAG_END());

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 407));

  soa_generate_offer(soa, 1, NULL);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  do {
    r481 = s2_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r481->sip->sip_status->st_status < 200);

  s2_update_dialog(dialog, r481); /* send ACK */

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_2)
{
  nua_handle_t *nh;
  struct message *bye, *r481;

  s2_case("4.1.2", "Re-INVITE while terminating",
	  "NUA sends BYE, and gets re-INVITEd at same time");

  nh = invite_to_nua(TAG_END());

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  do {
    r481 = s2_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r481->sip->sip_status->st_status < 200);

  s2_update_dialog(dialog, r481); /* send ACK */

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  s2_respond_to(bye, dialog, SIP_200_OK,
		TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_1_3)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  s2_case("4.1.3", "BYE while terminating",
	  "NUA sends BYE and receives BYE");

  nh = invite_to_nua(TAG_END());

  mark_point();

  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless(s2_check_event(nua_r_set_params, 200));

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_respond(nh, 200, "OKOK", NUTAG_WITH(i_bye->data->e_msg), TAG_END());

  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  fail_unless(s2_check_response(200, SIP_METHOD_BYE));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_4)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  s2_case("4.1.4", "Send BYE after BYE has been received",
	  "NUA receives BYE, tries to send BYE at same time");

  nh = invite_to_nua(TAG_END());

  mark_point();
  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless(s2_check_event(nua_r_set_params, 200));
  s2_flush_events();

  s2_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_bye(nh, TAG_END());

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_respond(nh, 200, "OKOK", NUTAG_WITH(i_bye->data->e_msg), TAG_END());
  fail_unless(s2_check_response(200, SIP_METHOD_BYE));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_5)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  s2_case("4.1.5", "Send BYE after BYE has been received",
	  "NUA receives BYE, tries to send BYE at same time");

  nh = invite_to_nua(TAG_END());

  mark_point();
  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless(s2_check_event(nua_r_set_params, 200));
  s2_flush_events();

  s2_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_bye(nh, TAG_END());

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
  fail_unless(s2_check_response(500, SIP_METHOD_BYE));
}
END_TEST


START_TEST(bye_4_1_6)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  s2_case("4.1.6", "Send BYE after INVITE has been received",
	  "NUA receives INVITE, sends BYE at same time");

  nh = invite_to_nua(TAG_END());

  nua_set_hparams(nh, NUTAG_AUTOANSWER(0), TAG_END());
  fail_unless(s2_check_event(nua_r_set_params, 200));

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless(s2_check_event(nua_i_invite, 100));
  fail_unless(s2_check_callstate(nua_callstate_received));

  do {
    r486 = s2_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_7)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  s2_case("4.1.7", "Send BYE after INVITE has been received",
	  "NUA receives INVITE, sends BYE at same time");

  nh = invite_to_nua(TAG_END());

  nua_set_hparams(nh, NUTAG_AUTOANSWER(0), TAG_END());
  fail_unless(s2_check_event(nua_r_set_params, 200));

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless(s2_check_event(nua_i_invite, 100));
  fail_unless(s2_check_callstate(nua_callstate_received));

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  do {
    r486 = s2_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_1_8)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  s2_case("4.1.8", "BYE followed by response to INVITE",
	  "NUA receives INVITE, sends BYE at same time");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2->local), TAG_END());

  invite_by_nua(nh, NUTAG_AUTOANSWER(0), TAG_END());

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless(s2_check_event(nua_i_invite, 100));
  fail_unless(s2_check_callstate(nua_callstate_received));

  nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());

  do {
    r486 = s2_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_9)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  s2_case("4.1.6", "Send BYE, receive BYE, destroy",
	  "NUA sends BYE, receives BYE and handle gets destroyed");

  nh = invite_to_nua(TAG_END());

  mark_point();

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 200);
  fail_if(!i_bye);
  s2_free_event(i_bye), i_bye = NULL;
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_check_response(200, SIP_METHOD_BYE));
  nua_handle_destroy(nh);
  mark_point();

  su_root_step(s2->root, 10);
  su_root_step(s2->root, 10);
  su_root_step(s2->root, 10);

  mark_point();
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  mark_point();
  while (su_home_check_alloc((su_home_t *)nua, (void *)nh)) {
    su_root_step(s2->root, 10);
  }
}
END_TEST


START_TEST(bye_4_1_10)
{
  nua_handle_t *nh;
  struct message *invite, *bye;
  struct event *i_bye;

  s2_case("4.1.6", "Send auto-BYE upon receiving 501, receive BYE, destroy",
	  "NUA sends BYE, receives BYE and handle gets destroyed");

  nh = invite_to_nua(TAG_END());

  mark_point();

  s2_flush_events();

  nua_invite(nh, TAG_END());
  invite = s2_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_respond_to(invite, dialog, SIP_501_NOT_IMPLEMENTED, TAG_END());
  s2_free_message(invite);

  fail_unless(s2_check_request(SIP_METHOD_ACK));

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  fail_unless(s2_check_callstate(nua_callstate_calling));
  fail_unless(s2_check_event(nua_r_invite, 501));
  fail_unless(s2_check_callstate(nua_callstate_terminating));

  s2_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 200);
  fail_if(!i_bye);
  s2_free_event(i_bye), i_bye = NULL;
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_check_response(200, SIP_METHOD_BYE));
  nua_handle_destroy(nh);

  su_root_step(s2->root, 10);
  su_root_step(s2->root, 10);
  su_root_step(s2->root, 10);

  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);

  while (su_home_check_alloc((su_home_t *)nua, (void *)nh)) {
    su_root_step(s2->root, 10);
  }
}
END_TEST

START_TEST(bye_4_2_1)
{
  nua_handle_t *nh;
  struct message *bye;

  s2_case("4.2.1", "BYE in progress while call timer expires",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends BYE, BYE challenged, "
	  "waits until session expires.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_fast_forward(300);
  invite_timer_round(nh, "300");

  nua_bye(nh, TAG_END());

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 407));

  s2_fast_forward(300);

  nua_authenticate(nh, NUTAG_AUTH("Digest:\"s2test\":abc:abc"), TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_if(s2->events);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_2_2)
{
  nua_handle_t *nh;
  struct message *bye;

  s2_case("4.2.2", "BYE in progress while call timer expires",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends BYE, BYE challenged, "
	  "waits until session expires.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_fast_forward(300);
  invite_timer_round(nh, "300");

  s2_fast_forward(300);

  nua_bye(nh, TAG_END());

  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 407));

  s2_fast_forward(300);

  nua_authenticate(nh, NUTAG_AUTH(s2_auth_credentials), TAG_END());
  bye = s2_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_free_message(bye);
  fail_unless(s2_check_event(nua_r_bye, 200));
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_if(s2->events);

  nua_handle_destroy(nh);
}
END_TEST

TCase *termination_tcase(void)
{
  TCase *tc = tcase_create("4 - Call Termination");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  {
    tcase_add_test(tc, bye_4_1_1);
    tcase_add_test(tc, bye_4_1_2);
    tcase_add_test(tc, bye_4_1_3);
    tcase_add_test(tc, bye_4_1_4);
    tcase_add_test(tc, bye_4_1_5);
    tcase_add_test(tc, bye_4_1_6);
    tcase_add_test(tc, bye_4_1_7);
    tcase_add_test(tc, bye_4_1_8);
    tcase_add_test(tc, bye_4_1_9);
    tcase_add_test(tc, bye_4_1_10);
    tcase_add_test(tc, bye_4_2_1);
    tcase_add_test(tc, bye_4_2_2);
    tcase_set_timeout(tc, 5);
  }
  return tc;
}

/* ====================================================================== */

/* Test case template */

START_TEST(empty)
{
  s2_case("0.0.0", "Empty test case",
	  "Detailed explanation for empty test case.");

  tport_set_params(s2->master, TPTAG_LOG(1), TAG_END());
  s2_setup_logs(7);
  s2_setup_logs(0);
  tport_set_params(s2->master, TPTAG_LOG(0), TAG_END());
}

END_TEST

TCase *empty_tcase(void)
{
  TCase *tc = tcase_create("0 - Empty");
  tcase_add_checked_fixture(tc, call_setup, call_teardown);
  tcase_add_test(tc, empty);

  return tc;
}

/* ====================================================================== */

void check_session_cases(Suite *suite)
{
  suite_add_tcase(suite, invite_tcase());
  suite_add_tcase(suite, cancel_tcase());
  suite_add_tcase(suite, session_timer_tcase());
  suite_add_tcase(suite, invite_100rel_tcase());
  suite_add_tcase(suite, invite_error_tcase());
  suite_add_tcase(suite, termination_tcase());

  if (0)			/* Template */
    suite_add_tcase(suite, empty_tcase());
}
