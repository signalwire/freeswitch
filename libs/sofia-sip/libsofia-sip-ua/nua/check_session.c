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
static soa_session_t *soa = NULL;
static struct dialog *dialog = NULL;

#define CRLF "\r\n"

static void call_setup(void)
{
  nua = s2_nua_setup("call",
		     SIPTAG_ORGANIZATION_STR("Pussy Galore's Flying Circus"),
                     NUTAG_OUTBOUND("no-options-keepalive, no-validate"),
		     TAG_END());

  soa = soa_create(NULL, s2base->root, NULL);

  fail_if(!soa);

  soa_set_params(soa,
		 SOATAG_USER_SDP_STR("m=audio 5008 RTP/AVP 8 0" CRLF
				     "m=video 5010 RTP/AVP 34" CRLF),
		 TAG_END());

  dialog = su_home_new(sizeof *dialog); fail_if(!dialog);

  s2_register_setup();
}

static void call_thread_setup(void)
{
  s2_nua_thread = 1;
  call_setup();
}

static void call_teardown(void)
{
  s2_teardown_started("call");

  mark_point();

  s2_register_teardown();

  if (s2->shutdown == 0) {
    mark_point();
    nua_shutdown(nua);
    fail_unless_event(nua_r_shutdown, 200);
  }

  mark_point();

  s2_nua_teardown();
}

static void
add_call_fixtures(TCase *tc, int threading)
{
  if (threading)
    tcase_add_checked_fixture(tc, call_thread_setup, call_teardown);
  else
    tcase_add_checked_fixture(tc, call_setup, call_teardown);
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
  s2_sip_respond_to(request, dialog, status, phrase,
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
    s2_sip_request_to(dialog, method, name, tport,
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

  return s2_sip_wait_for_request(SIP_METHOD_INVITE);
}

static uint32_t s2_rseq;

static struct message *
respond_with_100rel(struct message *invite,
		    struct dialog *d,
		    int with_sdp,
		    int status, char const *phrase,
		    tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  sip_rseq_t rs[1];

  assert(100 < status && status < 200);

  sip_rseq_init(rs);
  rs->rs_response = ++s2_rseq;

  ta_start(ta, tag, value);

  if (with_sdp) {
    respond_with_sdp(
      invite, dialog, status, phrase,
      SIPTAG_REQUIRE_STR("100rel"),
      SIPTAG_RSEQ(rs),
      ta_tags(ta));
  }
  else {
    s2_sip_respond_to(
      invite, dialog, status, phrase,
      SIPTAG_REQUIRE_STR("100rel"),
      SIPTAG_RSEQ(rs),
      ta_tags(ta));
  }
  ta_end(ta);

  fail_unless_event(nua_r_invite, status);

  return s2_sip_wait_for_request(SIP_METHOD_PRACK);
}

static void
invite_by_nua(nua_handle_t *nh,
	      tag_type_t tag, tag_value_t value, ...)
{
  struct message *invite;
  ta_list ta;

  ta_start(ta, tag, value);
  invite = invite_sent_by_nua(nh, ta_tags(ta));
  ta_end(ta);

  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());

  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));
}

static nua_handle_t *
invite_to_nua(tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  struct event *invite;
  struct message *response;
  nua_handle_t *nh;

  soa_generate_offer(soa, 1, NULL);

  ta_start(ta, tag, value);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, ta_tags(ta));
  ta_end(ta);

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh;
  fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));

  fail_unless_event(nua_i_ack, 200);
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

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
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

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 407);

  nua_authenticate(nh, NUTAG_AUTH("Digest:\"s2test\":abc:abc"), TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
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

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);
  fail_unless_event(nua_r_cancel, 200);

  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  fail_unless_event(nua_r_invite, 487);
}

static void
bye_to_nua(nua_handle_t *nh,
	   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  ta_start(ta, tag, value);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, ta_tags(ta)));
  ta_end(ta);

  fail_unless_event(nua_i_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));
}

/* ====================================================================== */
/* 2 - Call cases */

/* 2.1 - Basic call cases */

START_TEST(call_2_1_1)
{
  nua_handle_t *nh;

  S2_CASE("2.1.1", "Basic call",
	  "NUA sends INVITE, NUA sends BYE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite_by_nua(nh, TAG_END());

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_2_1)
{
  nua_handle_t *nh;

  S2_CASE("2.1.2.1", "Basic call",
	  "NUA sends INVITE, NUA receives BYE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite_by_nua(nh, TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_1_2_2)
{
  nua_handle_t *nh;

  S2_CASE("2.1.2.2", "Basic call over TCP",
	  "NUA sends INVITE, NUA receives BYE");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  TAG_END());

  invite_by_nua(nh,
		NUTAG_PROXY(s2sip->tcp.contact->m_url),
		TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_3_1)
{
  nua_handle_t *nh;

  S2_CASE("2.1.3.1", "Incoming call",
	  "NUA receives INVITE and BYE");

  nh = invite_to_nua(TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_3_2)
{
  nua_handle_t *nh;

  S2_CASE("2.1.3.2", "Incoming call over TCP",
	  "NUA receives INVITE and BYE");

  dialog->tport = s2sip->tcp.tport;

  nh = invite_to_nua(TAG_END());

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_4)
{
  nua_handle_t *nh;

  S2_CASE("2.1.4", "Incoming call",
	  "NUA receives INVITE and sends BYE");

  nh = invite_to_nua(TAG_END());

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_5)
{
  nua_handle_t *nh;

  S2_CASE("2.1.5", "Incoming call",
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

  S2_CASE("2.1.6", "Basic call",
	  "NUA received INVITE, "
	  "NUA responds (and saves proxy for dialog), "
	  "NUA sends BYE");

  soa_generate_offer(soa, 1, NULL);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh;
  fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      /* Dialog-specific proxy is saved */
	      NUTAG_PROXY(s2sip->tcp.contact->m_url),
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));

  fail_unless_event(nua_i_ack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  nua_bye(nh, TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  /* Check that NUA used dialog-specific proxy with BYE */
  fail_unless(tport_is_tcp(bye->tport));
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_2_1_7)
{
  nua_handle_t *nh, *nh2;
  sip_replaces_t *replaces;

  S2_CASE("2.1.7", "Call lookup",
	  "Test dialog and call-id lookup");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

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


START_TEST(call_2_1_8)
{
  nua_handle_t *nh;
  struct message *invite, *ack;

  S2_CASE("2.1.8", "Call using NUTAG_PROXY()",
	  "Test handle-specific NUTAG_PROXY().");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    NUTAG_PROXY(s2sip->tcp.contact->m_url), TAG_END());

  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());

  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_unless(ack && tport_is_tcp(ack->tport));

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


TCase *invite_tcase(int threading)
{
  TCase *tc = tcase_create("2.1 - Basic INVITE");
  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_2_1_1);
    tcase_add_test(tc, call_2_1_2_1);
    tcase_add_test(tc, call_2_1_2_2);
    tcase_add_test(tc, call_2_1_3_1);
    tcase_add_test(tc, call_2_1_3_2);
    tcase_add_test(tc, call_2_1_4);
    tcase_add_test(tc, call_2_1_5);
    tcase_add_test(tc, call_2_1_6);
    tcase_add_test(tc, call_2_1_7);
    tcase_add_test(tc, call_2_1_8);
  }
  return tc;
}

/* ---------------------------------------------------------------------- */
/* 2.2 - Call CANCEL cases */

START_TEST(cancel_2_2_1)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("2.2.1", "Cancel call",
	  "NUA is caller, NUA sends CANCEL immediately");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));
  nua_cancel(nh, TAG_END());

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  fail_if(s2sip->received != NULL);
  s2_sip_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  fail_unless_event(nua_r_invite, 487);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless_event(nua_r_cancel, 200);
  fail_if(s2->events != NULL);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_2)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("2.2.2", "Canceled call",
	  "NUA is caller, NUA sends CANCEL after receiving 100");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());

  cancel_by_nua(nh, invite, dialog, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_3)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("2.2.3", "Canceled call",
	  "NUA is caller, NUA sends CANCEL after receiving 180");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  cancel_by_nua(nh, invite, dialog, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_4)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("2.2.4", "Cancel and 200 OK glare",
	  "NUA is caller, NUA sends CANCEL after receiving 180 "
	  "but UAS already sent 200 OK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_cancel(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());

  s2_sip_respond_to(cancel, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  s2_sip_free_message(cancel);
  fail_unless_event(nua_r_cancel, 481);

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_5)
{
  nua_handle_t *nh;
  struct message *invite, *cancel, *bye;

  S2_CASE(
    "2.2.5", "Cancel and 200 OK glare",
    "NUA is caller, "
    "NUA uses nua_bye() to send CANCEL after receiving 180\n"
    "but UAS already sent 200 OK.\n"
    "Test case checks that NUA really sends BYE after nua_bye() is called\n");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     NUTAG_AUTOACK(0),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_bye(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());

  s2_sip_respond_to(cancel, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  s2_sip_free_message(cancel);
  fail_unless_event(nua_r_cancel, 481);

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  fail_unless(s2_check_callstate(nua_callstate_terminating));

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_6)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response;

  S2_CASE("2.2.6", "Cancel call",
	  "NUA is callee, sends 100, 180, INVITE gets canceled");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_CANCEL, NULL, TAG_END()));
  fail_unless_event(nua_i_cancel, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  response = s2_sip_wait_for_response(200, SIP_METHOD_CANCEL);
  fail_if(!response);
  s2_sip_free_message(response);

  response = s2_sip_wait_for_response(487, SIP_METHOD_INVITE);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_7)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response;
  char const *via = "SIP/2.0/UDP host.in.invalid;rport";

  S2_CASE("2.2.7", "Call gets canceled",
	  "NUA is callee, sends 100, 180, INVITE gets canceled. "
	  "Using RFC 2543 dialog and transaction matching.");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL,
		   SIPTAG_VIA_STR(via),
		   TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_CANCEL, NULL, TAG_END()));
  fail_unless_event(nua_i_cancel, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  response = s2_sip_wait_for_response(200, SIP_METHOD_CANCEL);
  fail_if(!response);
  s2_sip_free_message(response);

  response = s2_sip_wait_for_response(487, SIP_METHOD_INVITE);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(cancel_2_2_8)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;
  int timeout;

  S2_CASE("2.2.8", "CANCEL and INVITE times out",
	  "NUA is caller, NUA sends CANCEL after receiving 180 "
	  "but UAS never responds.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_cancel(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  s2_sip_free_message(cancel);
  fail_if(!cancel);

  /* Now, time out both CANCEL and INVITE */
  for (timeout = 0; timeout < 34; timeout++) {
    s2_nua_fast_forward(1, s2base->root);
    cancel = s2_sip_next_request(SIP_METHOD_CANCEL);
    if (cancel)
      s2_sip_free_message(cancel);
  }

  fail_unless_event(nua_r_cancel, 408);
  fail_unless_event(nua_r_invite, 408);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(cancel_2_2_9)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;
  int timeout;

  S2_CASE("2.2.9", "CANCEL a RFC2543 UA",
	  "NUA is caller, NUA sends CANCEL after receiving 180, "
	  "UAS sends 200 OK to CANCEL but no response to INVITE.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_cancel(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);

  /* Time out INVITE */
  for (timeout = 0; timeout < 34; timeout++) {
    s2_nua_fast_forward(1, s2base->root);
  }

  fail_unless_event(nua_r_invite, 408);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  nua_handle_destroy(nh);
}
END_TEST

START_TEST(cancel_2_2_10)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;
  struct event *event;
  int timeout;

  S2_CASE("2.2.10", "CANCEL and INVITE times out",
	  "NUA is caller, NUA sends CANCEL after receiving 180 "
	  "but UAS never responds.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_cancel(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  s2_sip_free_message(cancel);
  fail_if(!cancel);

  nua_cancel(nh, TAG_END());
  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);

  /* emulate network gone bad below
     zap registration handle here
     so that s2_register_teardown() does not hang
  */
  s2->registration->nh = NULL;

  nua_set_params(s2->nua, NUTAG_SHUTDOWN_EVENTS(1), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  nua_shutdown(s2->nua);
  event = s2_wait_for_event(nua_r_shutdown, 100);
  fail_unless(event != NULL);
  s2_free_event(event);

  /* Time out */
  for (timeout = 0; timeout < 34; timeout++) {
    s2_nua_fast_forward(5, s2base->root);
    nua_shutdown(s2->nua);
    event = s2_wait_for_event(nua_r_shutdown, 0);
    fail_unless(event != NULL);
    if (event->data->e_status >= 200)
      break;
  }

  s2->shutdown = 200;
}
END_TEST



TCase *cancel_tcase(int threading)
{
  TCase *tc = tcase_create("2.2 - CANCEL");
  add_call_fixtures(tc, threading);

  tcase_add_test(tc, cancel_2_2_1);
  tcase_add_test(tc, cancel_2_2_2);
  tcase_add_test(tc, cancel_2_2_3);
  tcase_add_test(tc, cancel_2_2_4);
  if (XXX) tcase_add_test(tc, cancel_2_2_5);
  tcase_add_test(tc, cancel_2_2_6);
  tcase_add_test(tc, cancel_2_2_7);
  tcase_add_test(tc, cancel_2_2_8);
  tcase_add_test(tc, cancel_2_2_9);
  tcase_add_test(tc, cancel_2_2_10);

  return tc;
}


/* ---------------------------------------------------------------------- */
/* 2.3 - Session timers */

/* Wait for invite from NUA */
static struct message *
invite_timer_round(nua_handle_t *nh,
		   char const *session_expires,
		   sip_record_route_t *rr)
{
  struct message *invite, *ack;

  fail_unless(s2_check_callstate(nua_callstate_calling));
  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);
  /* Check that INVITE contains Session-Expires header with refresher=uac */
  fail_unless(invite->sip->sip_session_expires != NULL);
  fail_unless(su_casematch(invite->sip->sip_session_expires->x_refresher, "uac"));
  respond_with_sdp(
    invite, dialog, SIP_200_OK,
    SIPTAG_SESSION_EXPIRES_STR(session_expires),
    SIPTAG_REQUIRE_STR("timer"),
    SIPTAG_RECORD_ROUTE(rr),
    TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  if (rr == NULL)
    s2_sip_free_message(ack);
  return ack;
}

START_TEST(call_2_3_1)
{
  nua_handle_t *nh;
  sip_record_route_t rr[1];
  struct message *ack;

  sip_record_route_init(rr);
  *rr->r_url = *s2sip->contact->m_url;
  rr->r_url->url_user = "record";
  rr->r_url->url_params = "lr";

  S2_CASE("2.3.1", "Incoming call with call timers",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends re-INVITE twice, "
	  "sends BYE.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_nua_fast_forward(300, s2base->root);
  ack = invite_timer_round(nh, "300;refresher=uac", rr);
  fail_if(ack->sip->sip_route &&
	  su_strmatch(ack->sip->sip_route->r_url->url_user, "record"));
  s2_nua_fast_forward(300, s2base->root);
  invite_timer_round(nh, "300;refresher=uac", NULL);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_3_2)
{
  nua_handle_t *nh;

  S2_CASE("2.3.2", "Incoming call with call timers",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends re-INVITE, "
	  "sends BYE.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_nua_fast_forward(300, s2base->root);
  invite_timer_round(nh, "300;refresher=uac", NULL);
  s2_nua_fast_forward(300, s2base->root);
  invite_timer_round(nh, "300;refresher=uac", NULL);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST



TCase *session_timer_tcase(int threading)
{
  TCase *tc = tcase_create("2.3 - Session timers");
  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_2_3_1);
    tcase_add_test(tc, call_2_3_2);
  }
  return tc;
}

/* ====================================================================== */
/* 2.4 - 100rel */

START_TEST(call_2_4_1)
{
  nua_handle_t *nh;
  struct message *invite, *prack, *ack;
  int with_sdp;
  sip_record_route_t rr[1];

  S2_CASE("2.4.1", "Call with 100rel",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "receives 200, send ACK.");

  sip_record_route_init(rr);
  *rr->r_url = *s2sip->contact->m_url;
  rr->r_url->url_user = "record";
  rr->r_url->url_params = "lr";

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    TAG_END());
  process_offer(invite);

  prack = respond_with_100rel(invite, dialog, with_sdp = 1,
			      SIP_183_SESSION_PROGRESS,
			      SIPTAG_RECORD_ROUTE(rr),
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_180_RINGING,
			      TAG_END());
  fail_unless(prack->sip->sip_route != NULL);
  fail_unless(su_strmatch(prack->sip->sip_route->r_url->url_user, "record"));

  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  /* Change the record-route */
  rr->r_url->url_user = "record2";
  s2_sip_respond_to(invite, dialog, SIP_200_OK,
		SIPTAG_RECORD_ROUTE(rr),
		TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  fail_unless(su_strmatch(ack->sip->sip_route->r_url->url_user, "record2"));
  s2_sip_free_message(ack);

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_4_2)
{
  nua_handle_t *nh;
  struct message *invite, *prack;
  int with_sdp;

  S2_CASE("2.4.2", "Call with 100rel",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "receives 200, send ACK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

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

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_183_SESSION_PROGRESS,
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_180_RINGING,
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  s2_sip_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_4_3)
{
  struct message *response;

  S2_CASE("2.4.3", "Call without 100rel",
	  "NUA receives INVITE with Required: 100rel, "
	  "rejects it with 420");

  nua_set_params(s2->nua,
		 SIPTAG_SUPPORTED(SIP_NONE),
		 SIPTAG_SUPPORTED_STR("timer"),
		 TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL,
		   SIPTAG_REQUIRE_STR("100rel"),
		   TAG_END());

  response = s2_sip_wait_for_response(420, SIP_METHOD_INVITE);
  fail_if(!response);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));
}
END_TEST

START_TEST(call_2_4_4)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response;

  S2_CASE("2.4.3", "Call without 100rel",
	  "NUA receives INVITE with Supported: 100rel, "
	  "proceeds normally");

  nua_set_params(s2->nua,
		 SIPTAG_SUPPORTED(SIP_NONE),
		 SIPTAG_SUPPORTED_STR("timer"),
		 TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL,
		   SIPTAG_SUPPORTED_STR("100rel"),
		   TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_respond(nh, SIP_183_SESSION_PROGRESS,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_early));

  response = s2_sip_wait_for_response(183, SIP_METHOD_INVITE);
  fail_if(!response);
  fail_if(response->sip->sip_require);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));

  fail_unless_event(nua_i_ack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_4_5)
{
  nua_handle_t *nh;
  struct message *invite, *prack, *cancel;
  int i;
  int with_sdp;
  sip_from_t *branch1, *branch2;

  /* Testcase for bug FSCORE-338 -
     forked transactions getting canceled and terminated properly. */

  S2_CASE("2.4.5", "Destroy proceeding call with 100rel",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "handle is destroyed.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh,
    NUTAG_PROXY(s2sip->tcp.contact->m_url),
    NUTAG_MEDIA_ENABLE(0),
    NUTAG_AUTOACK(0),
    SIPTAG_CONTENT_TYPE_STR("application/sdp"),
    SIPTAG_PAYLOAD_STR(
      "v=0" CRLF
      "o=- 6805647540234172778 5821668777690722690 IN IP4 127.0.0.1" CRLF
      "s=-" CRLF
      "c=IN IP4 127.0.0.1" CRLF
      "m=audio 5004 RTP/AVP 0 8" CRLF),
    TAG_END());

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_183_SESSION_PROGRESS,
			      SIPTAG_CONTACT(s2sip->tcp.contact),
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_180_RINGING,
			      SIPTAG_CONTACT(s2sip->tcp.contact),
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  branch1 = dialog->local;
  branch2 = dialog->local = sip_from_dup(dialog->home, invite->sip->sip_to);
  sip_from_tag(dialog->home, dialog->local, s2_sip_generate_tag(dialog->home));

  nua_handle_destroy(nh), nh = NULL;

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());

  for (i = 1; i < 4; i++) {
    s2_nua_fast_forward(1, s2base->root);
  }

  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  /* Time out requests */
  for (i = 1; i < 128; i++) {
    s2_nua_fast_forward(1, s2base->root);
  }
}
END_TEST

TCase *invite_100rel_tcase(int threading)
{
  TCase *tc = tcase_create("2.4 - INVITE with 100rel");
  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_2_4_1);
    tcase_add_test(tc, call_2_4_2);
    tcase_add_test(tc, call_2_4_3);
    tcase_add_test(tc, call_2_4_4);
    tcase_add_test(tc, call_2_4_5);
  }
  return tc;
}

/* ====================================================================== */
/* 2.5 - Call with preconditions */

START_TEST(call_2_5_1)
{
  nua_handle_t *nh;
  struct message *invite, *prack, *update;
  int with_sdp;

  S2_CASE("2.5.1", "Call with preconditions",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 for it, "
	  "sends UPDATE, receives 200 for it, "
	  "receives 180, sends PRACK, receives 200 for it, "
          "receives 200, send ACK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    SIPTAG_REQUIRE_STR("precondition"),
    TAG_END());
  process_offer(invite);

  prack = respond_with_100rel(invite, dialog, with_sdp = 1,
			      SIP_183_SESSION_PROGRESS,
			      TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  process_offer(prack);
  respond_with_sdp(
    prack, dialog, SIP_200_OK,
    SIPTAG_REQUIRE_STR("100rel"),
    TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless_event(nua_r_prack, 200);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  update = s2_sip_wait_for_request(SIP_METHOD_UPDATE);
  /* UPDATE sent by stack, stack sends event for it */
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  process_offer(update);
  respond_with_sdp(
    update, dialog, SIP_200_OK,
    TAG_END());
  s2_sip_free_message(update), update = NULL;

  fail_unless_event(nua_r_update, 200);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  prack = respond_with_100rel(invite, dialog, with_sdp = 0,
			      SIP_180_RINGING,
			      TAG_END());
  s2_sip_respond_to(prack, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless(s2_check_callstate(nua_callstate_proceeding));
  fail_unless_event(nua_r_prack, 200);

  s2_sip_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_5_2)
{
  nua_handle_t *nh;
  struct message *invite, *prack, *update;
  sip_rseq_t rs[1];
  sip_rack_t rack[1];

  S2_CASE("2.5.2", "Call with preconditions - send 200 w/ ongoing PRACK ",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, "
          "receives 200 to INVITE, "
	  "receives 200 to PRACK, "
	  "sends ACK, "
	  "sends UPDATE, "
	  "receives 200 to UPDATE.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh,
    SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    SIPTAG_REQUIRE_STR("precondition"),
    NUTAG_APPL_METHOD("PRACK"),
    TAG_END());
  process_offer(invite);

  sip_rseq_init(rs)->rs_response = ++s2_rseq;
  respond_with_sdp(
    invite, dialog, SIP_183_SESSION_PROGRESS,
    SIPTAG_REQUIRE_STR("100rel"),
    SIPTAG_RSEQ(rs),
    TAG_END());
  fail_unless_event(nua_r_invite, 183);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  sip_rack_init(rack)->ra_response = s2_rseq;
  rack->ra_cseq = invite->sip->sip_cseq->cs_seq;
  rack->ra_method = invite->sip->sip_cseq->cs_method;
  rack->ra_method_name = invite->sip->sip_cseq->cs_method_name;

  nua_prack(nh, SIPTAG_RACK(rack), TAG_END());
  prack = s2_sip_wait_for_request(SIP_METHOD_PRACK);
  process_offer(prack);

  s2_sip_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  respond_with_sdp(
    prack, dialog, SIP_200_OK,
    TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless_event(nua_r_prack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  update = s2_sip_wait_for_request(SIP_METHOD_UPDATE);
  /* UPDATE sent by stack, stack sends event for it */
  fail_unless(s2_check_callstate(nua_callstate_calling));

  process_offer(update);
  respond_with_sdp(
    update, dialog, SIP_200_OK,
    TAG_END());
  s2_sip_free_message(update), update = NULL;

  fail_unless_event(nua_r_update, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_5_3)
{
  nua_handle_t *nh;
  struct message *invite, *prack, *update;
  sip_rseq_t rs[1];
  sip_rack_t rack[1];

  S2_CASE("2.5.3", "Call with preconditions - send 200 w/ ongoing UPDATE ",
	  "NUA sends INVITE, "
	  "receives 183, sends PRACK, receives 200 to PRACK, "
	  "sends UPDATE, "
          "receives 200 to INVITE, "
	  "receives 200 to UPDATE, "
	  "sends ACK.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(
    nh,
    SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
    SIPTAG_REQUIRE_STR("precondition"),
    NUTAG_APPL_METHOD("PRACK"),
    TAG_END());
  process_offer(invite);

  sip_rseq_init(rs)->rs_response = ++s2_rseq;
  respond_with_sdp(
    invite, dialog, SIP_183_SESSION_PROGRESS,
    SIPTAG_REQUIRE_STR("100rel"),
    SIPTAG_RSEQ(rs),
    TAG_END());
  fail_unless_event(nua_r_invite, 183);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  sip_rack_init(rack)->ra_response = s2_rseq;
  rack->ra_cseq = invite->sip->sip_cseq->cs_seq;
  rack->ra_method = invite->sip->sip_cseq->cs_method;
  rack->ra_method_name = invite->sip->sip_cseq->cs_method_name;

  nua_prack(nh, SIPTAG_RACK(rack), TAG_END());
  prack = s2_sip_wait_for_request(SIP_METHOD_PRACK);
  process_offer(prack);
  respond_with_sdp(
    prack, dialog, SIP_200_OK,
    TAG_END());
  s2_sip_free_message(prack), prack = NULL;
  fail_unless_event(nua_r_prack, 200);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  update = s2_sip_wait_for_request(SIP_METHOD_UPDATE);
  /* UPDATE sent by stack, stack sends event for it */
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  s2_sip_respond_to(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  process_offer(update);
  respond_with_sdp(
    update, dialog, SIP_200_OK,
    TAG_END());
  s2_sip_free_message(update), update = NULL;

  fail_unless_event(nua_r_update, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  bye_to_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

TCase *invite_precondition_tcase(int threading)
{
  TCase *tc = tcase_create("2.5 - Call with preconditions");
  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_2_5_1);
    tcase_add_test(tc, call_2_5_2);
    tcase_add_test(tc, call_2_5_3);
  }
  return tc;
}

/* ====================================================================== */
/* 2.6 - Re-INVITEs */

START_TEST(call_2_6_1)
{
  nua_handle_t *nh;
  struct message *invite, *ack;
  int i;

  S2_CASE("2.6.1", "Queued re-INVITEs",
	  "NUA receives INVITE, "
	  "sends re-INVITE twice, "
	  "sends BYE.");

  nh = invite_to_nua(TAG_END());

  nua_invite(nh, TAG_END());
  nua_invite(nh, TAG_END());

  for (i = 0; i < 2; i++) {
    fail_unless(s2_check_callstate(nua_callstate_calling));

    invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
    fail_if(!invite);
    process_offer(invite);
    respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
    s2_sip_free_message(invite);

    ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
    fail_if(!ack);
    s2_sip_free_message(ack);

    fail_unless_event(nua_r_invite, 200);
    fail_unless(s2_check_callstate(nua_callstate_ready));
  }

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_6_2)
{
  nua_handle_t *nh;
  struct message *invite, *ack, *response;

  S2_CASE("2.6.2", "Re-INVITE glare",
	  "NUA sends re-INVITE and then receives re-INVITE, "
	  "sends BYE.");

  nh = invite_to_nua(TAG_END());

  nua_invite(nh, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, dialog, SIP_500_INTERNAL_SERVER_ERROR,
		    SIPTAG_RETRY_AFTER_STR("8"),
		    TAG_END());
  s2_sip_free_message(invite);
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  response = s2_sip_wait_for_response(491, SIP_METHOD_INVITE);
  fail_if(!response);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));
  s2_sip_free_message(response);
  fail_if(soa_process_reject(soa, NULL) < 0);

  /* We get nua_r_invite with 100 trying (and 500 in sip->sip_status) */
  fail_unless_event(nua_r_invite, 100);

  s2_nua_fast_forward(10, s2base->root);

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_6_3)
{
  nua_handle_t *nh;
  struct message *response;

  S2_CASE("2.6.3", "Handling re-INVITE without SDP gracefully",
	  "NUA receives INVITE, "
	  "re-INVITE without SDP (w/o NUTAG_REFRESH_WITHOUT_SDP(), "
	  "re-INVITE without SDP (using NUTAG_REFRESH_WITHOUT_SDP(), "
	  "sends BYE.");

  nh = invite_to_nua(
    TAG_END());

  s2_sip_request_to(dialog, SIP_METHOD_INVITE, NULL,
		SIPTAG_USER_AGENT_STR("evil (evil) evil"),
		TAG_END());

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  fail_if(!response->sip->sip_content_type);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));

  fail_unless_event(nua_i_ack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  s2_nua_fast_forward(10, s2base->root);

  nua_set_hparams(nh, NUTAG_REFRESH_WITHOUT_SDP(1), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_sip_request_to(dialog, SIP_METHOD_INVITE, NULL,
		    SIPTAG_USER_AGENT_STR("evil (evil) evil"),
		    TAG_END());

  nua_respond(nh, SIP_200_OK, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);

  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  fail_if(response->sip->sip_content_type);
  s2_sip_free_message(response);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));

  fail_unless_event(nua_i_ack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_6_4)
{
  nua_handle_t *nh;
  struct message *invite, *ack;

  S2_CASE("2.6.4", "re-INVITEs w/o SDP",
	  "NUA sends re-INVITE w/o SDP, "
	  "receives SDP w/ offer, "
	  "sends ACK w/ answer, "
	  "sends BYE.");

  /* Bug reported by Liu Yang 2009-01-11 */
  nh = invite_to_nua(TAG_END());

  nua_invite(nh, SIPTAG_PAYLOAD_STR(""), TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));
  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  fail_if(invite->sip->sip_content_type);
  soa_generate_offer(soa, 1, NULL);
  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  process_answer(ack);
  s2_sip_free_message(ack);

  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_2_6_5)
{
  nua_handle_t *nh;
  struct event *reinvite;
  struct message *invite, *ack, *response;

  /* Test case for FreeSwitch bugs #SFSIP-135, #SFSIP-137 */

  S2_CASE("2.6.5", "Re-INVITE glare and 500 Retry-After",
	  "NUA receives re-INVITE, replies with 200, "
	  "sends re-INVITE, gets 500, gets ACK, retrys INVITE,"
	  "sends BYE.");

  nh = invite_to_nua(TAG_END());

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  reinvite = s2_wait_for_event(nua_i_invite, 200); fail_unless(reinvite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_completed));
  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_update_dialog(dialog, response);
  process_answer(response);
  s2_sip_free_message(response);

  nua_invite(nh, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_calling));
  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, dialog, SIP_500_INTERNAL_SERVER_ERROR,
		    SIPTAG_RETRY_AFTER_STR("7"),
		    TAG_END());
  s2_sip_free_message(invite);
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  /* We get nua_r_invite with 100 trying (and 500 in sip->sip_status) */
  fail_unless_event(nua_r_invite, 100);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL, TAG_END()));
  fail_unless_event(nua_i_ack, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));

  s2_nua_fast_forward(10, s2base->root);

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  process_offer(invite);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

TCase *reinvite_tcase(int threading)
{
  TCase *tc = tcase_create("2.6 - re-INVITEs");

  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_2_6_1);
    tcase_add_test(tc, call_2_6_2);
    tcase_add_test(tc, call_2_6_3);
    tcase_add_test(tc, call_2_6_4);
    tcase_add_test(tc, call_2_6_5);
  }
  return tc;
}


/* ====================================================================== */
/* 3.1 - Call error cases */

START_TEST(call_3_1_1)
{
  nua_handle_t *nh;
  struct message *invite, *ack;

  S2_CASE("3.1.1", "Call failure", "Call fails with 403 response");
  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, NULL, SIP_403_FORBIDDEN,
		SIPTAG_TO_STR("UAS Changed <sip:To@Header.field.invalid>"),
		TAG_END());
  s2_sip_free_message(invite);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  fail_if(strcmp(ack->sip->sip_to->a_display, "UAS Changed"));
  s2_sip_free_message(ack);
  fail_unless_event(nua_r_invite, 403);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_3_1_2)
{
  nua_handle_t *nh;
  struct message *invite;
  int i;

  S2_CASE("3.1.2", "Call fails after too many retries",
	  "Call fails after 4 times 500 Retry-After");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  NUTAG_RETRY_COUNT(3),
		  TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  for (i = 0;; i++) {
    fail_unless(s2_check_callstate(nua_callstate_calling));
    invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
    fail_if(!invite);
    s2_sip_respond_to(invite, NULL, SIP_500_INTERNAL_SERVER_ERROR,
		  SIPTAG_RETRY_AFTER_STR("5"),
		  TAG_END());
    s2_sip_free_message(invite);
    fail_unless(s2_sip_check_request(SIP_METHOD_ACK));
    if (i == 3)
      break;
    fail_unless_event(nua_r_invite, 100);
    s2_nua_fast_forward(5, s2base->root);
  }

  fail_unless_event(nua_r_invite, 500);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(call_3_2_1)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("3.2.1", "Re-INVITE failure", "Re-INVITE fails with 403 response");
  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  TAG_END());

  invite_by_nua(nh, TAG_END());

  nua_invite(nh, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, NULL, SIP_403_FORBIDDEN, TAG_END());
  s2_sip_free_message(invite);

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));
  fail_unless_event(nua_r_invite, 403);
  /* Return to previous state */
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());
}
END_TEST


START_TEST(call_3_2_2)
{
  nua_handle_t *nh;
  struct message *invite, *bye;
  int i;

  S2_CASE("3.2.2", "Re-INVITE fails after too many retries",
	  "Call fails after 4 times 500 Retry-After");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  NUTAG_RETRY_COUNT(3),
		  TAG_END());

  invite_by_nua(nh, TAG_END());

  nua_invite(nh, SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	     TAG_END());

  for (i = 0;; i++) {
    fail_unless(s2_check_callstate(nua_callstate_calling));
    invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
    fail_if(!invite);
    s2_sip_respond_to(invite, NULL, SIP_500_INTERNAL_SERVER_ERROR,
		  SIPTAG_RETRY_AFTER_STR("5"),
		  TAG_END());
    s2_sip_free_message(invite);
    fail_unless(s2_sip_check_request(SIP_METHOD_ACK));
    if (i == 3)
      break;
    fail_unless_event(nua_r_invite, 100);
    s2_nua_fast_forward(5, s2base->root);
  }

  fail_unless_event(nua_r_invite, 500);
  /* Graceful termination */
  fail_unless(s2_check_callstate(nua_callstate_terminating));
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(call_3_2_3)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("3.2.3", "Re-INVITE failure", "Re-INVITE fails with 491 response");
  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor),
		  TAG_END());

  invite_by_nua(nh, TAG_END());

  nua_invite(nh, TAG_END());

  fail_unless(s2_check_callstate(nua_callstate_calling));

  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, NULL, SIP_491_REQUEST_PENDING, TAG_END());
  s2_sip_free_message(invite);
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));
  fail_unless_event(nua_r_invite, 491);
  /* Return to previous state */
  fail_unless(s2_check_callstate(nua_callstate_ready));

  bye_by_nua(nh, TAG_END());
}
END_TEST


TCase *invite_error_tcase(int threading)
{
  TCase *tc = tcase_create("3 - Call Errors");
  add_call_fixtures(tc, threading);
  {
    tcase_add_test(tc, call_3_1_1);
    tcase_add_test(tc, call_3_1_2);
    tcase_add_test(tc, call_3_2_1);
    tcase_add_test(tc, call_3_2_2);
    tcase_add_test(tc, call_3_2_3);
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

  S2_CASE("4.1.1", "Re-INVITE while terminating",
	  "NUA sends BYE, "
	  "BYE is challenged, "
	  "and NUA is re-INVITEd at the same time.");

  nh = invite_to_nua(TAG_END());

  s2_flush_events();

  nua_bye(nh, TAG_END());

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 407);

  soa_generate_offer(soa, 1, NULL);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  do {
    r481 = s2_sip_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r481->sip->sip_status->st_status < 200);

  s2_sip_update_dialog(dialog, r481); /* send ACK */

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_2)
{
  nua_handle_t *nh;
  struct message *bye, *r481;

  S2_CASE("4.1.2", "Re-INVITE while terminating",
	  "NUA sends BYE, and gets re-INVITEd at same time");

  nh = invite_to_nua(TAG_END());

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  do {
    r481 = s2_sip_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r481->sip->sip_status->st_status < 200);

  s2_sip_update_dialog(dialog, r481); /* send ACK */

  fail_unless(s2_check_callstate(nua_callstate_terminated));

  s2_sip_respond_to(bye, dialog, SIP_200_OK,
		TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_1_3)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  S2_CASE("4.1.3", "BYE while terminating",
	  "NUA sends BYE and receives BYE");

  nh = invite_to_nua(TAG_END());

  mark_point();

  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_respond(nh, 200, "OKOK", NUTAG_WITH(i_bye->data->e_msg), TAG_END());

  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_4)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  S2_CASE("4.1.4", "Send BYE after BYE has been received",
	  "NUA receives BYE, tries to send BYE at same time");

  nh = invite_to_nua(TAG_END());

  mark_point();
  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);
  s2_flush_events();

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_bye(nh, TAG_END());

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_respond(nh, 200, "OKOK", NUTAG_WITH(i_bye->data->e_msg), TAG_END());
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_5)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  S2_CASE("4.1.5", "Send BYE after BYE has been received",
	  "NUA receives BYE, tries to send BYE at same time");

  nh = invite_to_nua(TAG_END());

  mark_point();
  nua_set_hparams(nh, NUTAG_APPL_METHOD("BYE"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);
  s2_flush_events();

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 100);
  fail_if(!i_bye);

  nua_bye(nh, TAG_END());

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));

  nua_handle_destroy(nh);
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));
}
END_TEST


START_TEST(bye_4_1_6)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  S2_CASE("4.1.6", "Send BYE after INVITE has been received",
	  "NUA receives INVITE, sends BYE at same time");

  nh = invite_to_nua(TAG_END());

  nua_set_hparams(nh, NUTAG_AUTOANSWER(0), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_sip_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless_event(nua_i_invite, 100);
  fail_unless(s2_check_callstate(nua_callstate_received));

  do {
    r486 = s2_sip_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_sip_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_7)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  S2_CASE("4.1.7", "Send BYE after INVITE has been received",
	  "NUA receives INVITE, sends BYE at same time");

  nh = invite_to_nua(TAG_END());

  nua_set_hparams(nh, NUTAG_AUTOANSWER(0), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_sip_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless_event(nua_i_invite, 100);
  fail_unless(s2_check_callstate(nua_callstate_received));

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  do {
    r486 = s2_sip_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_sip_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_1_8)
{
  nua_handle_t *nh;
  struct message *bye, *r486;

  S2_CASE("4.1.8", "BYE followed by response to INVITE",
	  "NUA receives INVITE, sends BYE at same time");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite_by_nua(nh, NUTAG_AUTOANSWER(0), TAG_END());

  s2_flush_events();

  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());
  fail_unless(s2_sip_check_response(100, SIP_METHOD_INVITE));
  nua_bye(nh, TAG_END());
  fail_unless_event(nua_i_invite, 100);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());

  do {
    r486 = s2_sip_wait_for_response(0, SIP_METHOD_INVITE);
  }
  while (r486->sip->sip_status->st_status < 200);
  s2_sip_update_dialog(dialog, r486); /* send ACK */
  fail_unless(r486->sip->sip_status->st_status == 486);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  nua_handle_destroy(nh);
}
END_TEST


START_TEST(bye_4_1_9)
{
  nua_handle_t *nh;
  struct message *bye;
  struct event *i_bye;

  S2_CASE("4.1.6", "Send BYE, receive BYE, destroy",
	  "NUA sends BYE, receives BYE and handle gets destroyed");

  nh = invite_to_nua(TAG_END());

  mark_point();

  s2_flush_events();

  nua_bye(nh, TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 200);
  fail_if(!i_bye);
  s2_free_event(i_bye), i_bye = NULL;
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));
  nua_handle_destroy(nh);
  mark_point();

  su_root_step(s2base->root, 10);
  su_root_step(s2base->root, 10);
  su_root_step(s2base->root, 10);

  mark_point();
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  mark_point();
  while (su_home_check_alloc((su_home_t *)nua, (void *)nh)) {
    su_root_step(s2base->root, 10);
  }
}
END_TEST


START_TEST(bye_4_1_10)
{
  nua_handle_t *nh;
  struct message *invite, *bye;
  struct event *i_bye;

  S2_CASE("4.1.6", "Send auto-BYE upon receiving 501, receive BYE, destroy",
	  "NUA sends BYE, receives BYE and handle gets destroyed");

  nh = invite_to_nua(TAG_END());

  mark_point();

  s2_flush_events();

  nua_invite(nh, TAG_END());
  invite = s2_sip_wait_for_request(SIP_METHOD_INVITE);
  fail_if(!invite);
  s2_sip_respond_to(invite, dialog, SIP_501_NOT_IMPLEMENTED, TAG_END());
  s2_sip_free_message(invite);

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);

  fail_unless(s2_check_callstate(nua_callstate_calling));
  fail_unless_event(nua_r_invite, 501);
  fail_unless(s2_check_callstate(nua_callstate_terminating));

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 200);
  fail_if(!i_bye);
  s2_free_event(i_bye), i_bye = NULL;
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));
  nua_handle_destroy(nh);

  su_root_step(s2base->root, 10);
  su_root_step(s2base->root, 10);
  su_root_step(s2base->root, 10);

  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  while (su_home_check_alloc((su_home_t *)nua, (void *)nh)) {
    su_root_step(s2base->root, 10);
  }
}
END_TEST

START_TEST(bye_4_1_11)
{
  nua_handle_t *nh;
  struct message *invite, *ack;
  struct event *i_bye;

  S2_CASE("4.1.11", "Receive BYE in completing state",
	  "NUA sends INVITE, receives 200, receives BYE.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, NUTAG_AUTOACK(0), TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  s2_sip_request_to(dialog, SIP_METHOD_BYE, NULL, TAG_END());
  i_bye = s2_wait_for_event(nua_i_bye, 200);
  fail_if(!i_bye);
  s2_free_event(i_bye), i_bye = NULL;
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_sip_check_response(200, SIP_METHOD_BYE));

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_2_1)
{
  nua_handle_t *nh;
  struct message *bye;

  S2_CASE("4.2.1", "BYE in progress while call timer expires",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends BYE, BYE challenged, "
	  "waits until session expires.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_nua_fast_forward(300, s2base->root);
  invite_timer_round(nh, "300", NULL);

  nua_bye(nh, TAG_END());

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 407);

  s2_nua_fast_forward(300, s2base->root);

  nua_authenticate(nh, NUTAG_AUTH("Digest:\"s2test\":abc:abc"), TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_if(s2->events);

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(bye_4_2_2)
{
  nua_handle_t *nh;
  struct message *bye;

  S2_CASE("4.2.2", "BYE in progress while call timer expires",
	  "NUA receives INVITE, "
	  "activates call timers, "
	  "sends BYE, BYE challenged, "
	  "waits until session expires.");

  nh = invite_to_nua(
    SIPTAG_SESSION_EXPIRES_STR("300;refresher=uas"),
    SIPTAG_REQUIRE_STR("timer"),
    TAG_END());

  s2_nua_fast_forward(300, s2base->root);
  invite_timer_round(nh, "300", NULL);

  s2_nua_fast_forward(140, s2base->root);

  nua_bye(nh, TAG_END());

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 407);

  s2_nua_fast_forward(160, s2base->root);

  nua_authenticate(nh, NUTAG_AUTH(s2_auth_credentials), TAG_END());
  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
  fail_unless_event(nua_r_bye, 200);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_if(s2->events);

  nua_handle_destroy(nh);
}
END_TEST

TCase *termination_tcase(int threading)
{
  TCase *tc = tcase_create("4 - Call Termination");
  add_call_fixtures(tc, threading);
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
    tcase_add_test(tc, bye_4_1_11);
    tcase_add_test(tc, bye_4_2_1);
    tcase_add_test(tc, bye_4_2_2);
    tcase_set_timeout(tc, 5);
  }
  return tc;
}

/* ====================================================================== */

START_TEST(destroy_4_3_1)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("4.3.1", "Destroy handle after INVITE sent",
	  "NUA sends INVITE, handle gets destroyed.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);

  nua_handle_destroy(nh);

  s2_sip_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_sip_free_message(invite);

  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);
}
END_TEST


START_TEST(destroy_4_3_2)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("4.3.2", "Destroy handle in calling state",
	  "NUA sends INVITE, receives 180, handle gets destroyed.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_handle_destroy(nh);

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_sip_free_message(invite);

  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);
}
END_TEST

START_TEST(destroy_4_3_3)
{
  nua_handle_t *nh;
  struct message *invite, *ack, *bye;

  S2_CASE("4.3.3", "Destroy handle in completing state",
	  "NUA sends INVITE, receives 200, handle gets destroyed.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, NUTAG_AUTOACK(0), TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  nua_handle_destroy(nh);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  s2_sip_free_message(invite);
}
END_TEST


START_TEST(destroy_4_3_4)
{
  nua_handle_t *nh;
  struct message *invite, *ack, *bye;

  S2_CASE("4.3.3", "Destroy handle in ready state ",
	  "NUA sends INVITE, receives 200, handle gets destroyed.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, NUTAG_AUTOACK(0), TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  nua_ack(nh, TAG_END());
  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  fail_unless(s2_check_callstate(nua_callstate_ready));

  nua_handle_destroy(nh);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  s2_sip_free_message(invite);
}
END_TEST


START_TEST(destroy_4_3_5)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("4.3.5", "Destroy handle in re-INVITE calling state",
	  "NUA sends re-INVITE, handle gets destroyed.");

  nh = invite_to_nua(TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);

  nua_handle_destroy(nh);

  s2_sip_respond_to(invite, dialog, SIP_100_TRYING, TAG_END());

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_sip_free_message(invite);

  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);
}
END_TEST


START_TEST(destroy_4_3_6)
{
  nua_handle_t *nh;
  struct message *invite, *cancel;

  S2_CASE("4.3.6", "Destroy handle in calling state of re-INVITE",
	  "NUA sends re-INVITE, receives 180, handle gets destroyed.");

  nh = invite_to_nua(TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_handle_destroy(nh);

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(invite, dialog, SIP_487_REQUEST_CANCELLED, TAG_END());
  s2_sip_free_message(invite);

  s2_sip_respond_to(cancel, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(cancel);
}
END_TEST


START_TEST(destroy_4_3_7)
{
  nua_handle_t *nh;
  struct message *invite, *ack, *bye;

  S2_CASE("4.3.7", "Destroy handle in completing state of re-INVITE",
	  "NUA sends INVITE, receives 200, handle gets destroyed.");

  nh = invite_to_nua(TAG_END());

  invite = invite_sent_by_nua(nh, NUTAG_AUTOACK(0), TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_completing));

  nua_handle_destroy(nh);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  s2_sip_free_message(invite);
}
END_TEST


START_TEST(destroy_4_3_8)
{
  nua_handle_t *nh;
  struct message *invite, *ack, *bye;

  S2_CASE("4.3.8", "Destroy handle after INVITE sent",
	  "NUA sends INVITE, handle gets destroyed, "
	  "but remote end returns 200 OK. "
	  "Make sure nua tries to release call properly.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);

  nua_handle_destroy(nh);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
}
END_TEST


START_TEST(destroy_4_3_9)
{
  nua_handle_t *nh;
  struct message *invite, *cancel, *ack, *bye;

  S2_CASE("4.3.9", "Destroy handle in calling state",
	  "NUA sends INVITE, receives 180, handle gets destroyed, "
	  "but remote end returns 200 OK. "
	  "Make sure nua tries to release call properly.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());
  process_offer(invite);
  s2_sip_respond_to(invite, dialog, SIP_180_RINGING, TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  nua_handle_destroy(nh);

  cancel = s2_sip_wait_for_request(SIP_METHOD_CANCEL);
  fail_if(!cancel);
  s2_sip_respond_to(cancel, dialog, SIP_481_NO_TRANSACTION, TAG_END());
  s2_sip_free_message(cancel);

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);

  ack = s2_sip_wait_for_request(SIP_METHOD_ACK);
  fail_if(!ack);
  s2_sip_free_message(ack);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
}
END_TEST


START_TEST(destroy_4_4_1)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response;

  S2_CASE("4.4.1", "Destroy handle while call is on-going",
	  "NUA is callee, sends 100, destroys handle");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);

  response = s2_sip_wait_for_response(480, SIP_METHOD_INVITE);
  fail_if(!response);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));
  s2_sip_free_message(response);
}
END_TEST


START_TEST(destroy_4_4_2)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response;

  S2_CASE("4.4.1", "Destroy handle while call is on-going",
	  "NUA is callee, sends 180, destroys handle");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_180_RINGING, TAG_END());

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);

  response = s2_sip_wait_for_response(480, SIP_METHOD_INVITE);
  fail_if(!response);
  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));
  s2_sip_free_message(response);
}
END_TEST


START_TEST(destroy_4_4_3_1)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response, *bye;

  S2_CASE("4.4.3.1", "Destroy handle while call is on-going",
	  "NUA is callee, sends 200, destroys handle");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  fail_unless(s2_check_callstate(nua_callstate_early));

  nua_respond(nh, SIP_200_OK, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_handle_destroy(nh);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));
  s2_sip_free_message(response);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);
}
END_TEST


START_TEST(destroy_4_4_3_2)
{
  nua_handle_t *nh;
  struct event *invite;
  struct message *response, *bye;

  S2_CASE("4.4.3.1", "Destroy handle while call is on-going",
	  "NUA is callee, sends 200, destroys handle");

  soa_generate_offer(soa, 1, NULL);
  request_with_sdp(dialog, SIP_METHOD_INVITE, NULL, TAG_END());

  invite = s2_wait_for_event(nua_i_invite, 100); fail_unless(invite != NULL);
  fail_unless(s2_check_callstate(nua_callstate_received));

  nh = invite->nh; fail_if(!nh);

  s2_free_event(invite);

  response = s2_sip_wait_for_response(100, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_respond(nh, SIP_180_RINGING,
	      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8"),
	      TAG_END());

  response = s2_sip_wait_for_response(180, SIP_METHOD_INVITE);
  fail_if(!response);
  s2_sip_free_message(response);

  fail_unless(s2_check_callstate(nua_callstate_early));

  nua_respond(nh, SIP_200_OK, TAG_END());
  fail_unless(s2_check_callstate(nua_callstate_completed));

  response = s2_sip_wait_for_response(200, SIP_METHOD_INVITE);
  fail_if(!response);

  nua_handle_destroy(nh);

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  fail_if(s2_sip_request_to(dialog, SIP_METHOD_ACK, NULL,
			SIPTAG_VIA(sip_object(dialog->invite)->sip_via),
			TAG_END()));
  s2_sip_free_message(response);
}
END_TEST

static TCase *destroy_tcase(int threading)
{
  TCase *tc = tcase_create("4.3 - Destroying Handle");

  add_call_fixtures(tc, threading);

  {
    tcase_add_test(tc, destroy_4_3_1);
    tcase_add_test(tc, destroy_4_3_2);
    tcase_add_test(tc, destroy_4_3_3);
    tcase_add_test(tc, destroy_4_3_4);
    tcase_add_test(tc, destroy_4_3_5);
    tcase_add_test(tc, destroy_4_3_6);
    tcase_add_test(tc, destroy_4_3_7);
    if (XXX) {
      tcase_add_test(tc, destroy_4_3_8);
      tcase_add_test(tc, destroy_4_3_9);
    }

    tcase_add_test(tc, destroy_4_4_1);
    tcase_add_test(tc, destroy_4_4_2);
    tcase_add_test(tc, destroy_4_4_3_1);
    tcase_add_test(tc, destroy_4_4_3_2);

    tcase_set_timeout(tc, 5);
  }
  return tc;
}

/* ====================================================================== */

static void options_setup(void), options_teardown(void);

START_TEST(options_5_1_1)
{
  struct event *options;
  nua_handle_t *nh;
  struct message *response;

  S2_CASE("5.1.1", "Test nua_respond() API",
	  "Test nua_respond() API with OPTIONS.");

  s2_sip_request_to(dialog, SIP_METHOD_OPTIONS, NULL, TAG_END());

  options = s2_wait_for_event(nua_i_options, 200);
  fail_unless(options != NULL);
  nh = options->nh; fail_if(!nh);

  response = s2_sip_wait_for_response(200, SIP_METHOD_OPTIONS);

  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);

  nua_set_params(nua, NUTAG_APPL_METHOD("OPTIONS"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_sip_request_to(dialog, SIP_METHOD_OPTIONS, NULL, TAG_END());

  options = s2_wait_for_event(nua_i_options, 100);
  fail_unless(options != NULL);
  nh = options->nh; fail_if(!nh);

  nua_respond(nh, 202, "okok", NUTAG_WITH_SAVED(options->event), TAG_END());

  response = s2_sip_wait_for_response(202, SIP_METHOD_OPTIONS);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);
}
END_TEST


#if HAVE_LIBPTHREAD
#include <pthread.h>

void *respond_to_options(void *arg)
{
  struct event *options = (struct event *)arg;

  nua_respond(options->nh, 202, "ok ok",
	      NUTAG_WITH_SAVED(options->event),
	      TAG_END());

  pthread_exit(arg);
  return NULL;
}

START_TEST(options_5_1_2)
{
  struct event *options;
  nua_handle_t *nh;
  struct message *response;
  pthread_t tid;
  void *thread_return = NULL;

  S2_CASE("5.1.2", "Test nua_respond() API with another thread",
	  "Test multithreading nua_respond() API with OPTIONS.");

  nua_set_params(nua, NUTAG_APPL_METHOD("OPTIONS"), TAG_END());
  fail_unless_event(nua_r_set_params, 200);

  s2_sip_request_to(dialog, SIP_METHOD_OPTIONS, NULL, TAG_END());

  options = s2_wait_for_event(nua_i_options, 100);
  fail_unless(options != NULL);
  nh = options->nh; fail_if(!nh);

  fail_if(pthread_create(&tid, NULL, respond_to_options, (void *)options));
  pthread_join(tid, &thread_return);
  fail_unless(thread_return == (void *)options);

  response = s2_sip_wait_for_response(202, SIP_METHOD_OPTIONS);
  fail_if(!response);
  s2_sip_free_message(response);

  nua_handle_destroy(nh);
}
END_TEST
#else
START_TEST(options_5_1_2)
{
}
END_TEST
#endif

TCase *options_tcase(int threading)
{
  TCase *tc = tcase_create("5 - OPTIONS, etc");

  tcase_add_checked_fixture(tc, options_setup, options_teardown);

  tcase_add_test(tc, options_5_1_1);
  tcase_add_test(tc, options_5_1_2);

  return tc;
}

static void options_setup(void)
{
  s2_nua_thread = 1;
  call_setup();
}

static void options_teardown(void)
{
  s2_teardown_started("options");
  call_teardown();
}

/* ====================================================================== */
/* Test cases for REFER */

START_TEST(refer_5_2_1)
{
  nua_handle_t *nh;
  sip_refer_to_t r[1];
  struct event *refer;
  struct message *notify;

  S2_CASE("5.2.1", "Receive REFER",
	  "Make a call, receive REFER.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  invite_by_nua(nh, TAG_END());

  *sip_refer_to_init(r)->r_url = *s2sip->aor->a_url;
  r->r_url->url_user = "bob2";

  s2_sip_request_to(dialog, SIP_METHOD_REFER, NULL,
		SIPTAG_REFER_TO(r),
		TAG_END());
  refer = s2_wait_for_event(nua_i_refer, 202);

  bye_by_nua(nh, TAG_END());

  nua_handle_destroy(nh);

  notify = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  s2_sip_respond_to(notify, dialog, SIP_200_OK, TAG_END());
}
END_TEST


START_TEST(refer_5_2_2)
{
  nua_handle_t *nh, *nh2;
  sip_refer_to_t r[1];
  sip_referred_by_t by[1];
  struct event *refer, *notified;
  sip_t const *sip;
  sip_event_t const *refer_event = NULL;
  sip_subscription_state_t const *ss;
  struct message *invite;
  struct message *notify0, *notify1, *notify2;
  struct dialog *dialog1, *dialog2;

  S2_CASE("5.2.2", "Receive REFER",
	  "Make a call, receive REFER, "
	  "make another call with automatic NOTIFYs");

  dialog2 = su_home_new(sizeof *dialog2); fail_unless(dialog2 != NULL);

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());
  invite_by_nua(nh, TAG_END());

  *sip_refer_to_init(r)->r_url = *s2sip->aor->a_url;
  r->r_url->url_user = "bob2";

  s2_sip_request_to(dialog, SIP_METHOD_REFER, NULL,
		SIPTAG_REFER_TO(r),
		TAG_END());
  refer = s2_wait_for_event(nua_i_refer, 202);
  sip = sip_object(refer->data->e_msg);
  fail_unless(sip && sip->sip_refer_to);

  bye_by_nua(nh, TAG_END());

  dialog1 = dialog, dialog = dialog2;

  *sip_referred_by_init(by)->b_url =
    *sip->sip_from->a_url;

  fail_unless(tl_gets(refer->data->e_tags,
		      NUTAG_REFER_EVENT_REF(refer_event),
		      TAG_END()) == 1);

  nua_notify(nh,
	     SIPTAG_EVENT(refer_event),
	     SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
	     SIPTAG_PAYLOAD_STR("SIP/2.0 100 Trying\r\n"),
	     NUTAG_SUBSTATE(nua_substate_active),
	     TAG_END());
  notify0 = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  fail_unless((ss = notify0->sip->sip_subscription_state) != NULL);
  fail_unless(su_casematch("active", ss->ss_substate));
  s2_sip_respond_to(notify0, dialog1, SIP_200_OK, TAG_END());
  notified = s2_wait_for_event(nua_r_notify, 200);

  nh2 = nua_handle(nua, NULL, NUTAG_URL(r->r_url), TAG_END());

  invite = invite_sent_by_nua(nh2,
			      NUTAG_REFER_EVENT(refer_event),
			      NUTAG_NOTIFY_REFER(nh),
			      SIPTAG_REFERRED_BY(by),
			      TAG_END());
  process_offer(invite);

  respond_with_sdp(
    invite, dialog, SIP_180_RINGING,
    SIPTAG_CONTENT_DISPOSITION_STR("session;handling=optional"),
    TAG_END());
  fail_unless_event(nua_r_invite, 180);
  fail_unless(s2_check_callstate(nua_callstate_proceeding));

  notify1 = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  s2_sip_respond_to(notify1, dialog1, SIP_200_OK, TAG_END());

  respond_with_sdp(invite, dialog, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);
  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  notify2 = s2_sip_wait_for_request(SIP_METHOD_NOTIFY);
  s2_sip_respond_to(notify2, dialog1, SIP_200_OK, TAG_END());
  fail_unless((ss = notify2->sip->sip_subscription_state) != NULL);
  fail_unless(su_casematch("terminated", ss->ss_substate));

  nua_handle_destroy(nh);
  bye_by_nua(nh2, TAG_END());
  nua_handle_destroy(nh2);
}
END_TEST

TCase *refer_tcase(int threading)
{
  TCase *tc = tcase_create("5.2 - Call Transfer");

  add_call_fixtures(tc, threading);

  tcase_add_test(tc, refer_5_2_1);
  tcase_add_test(tc, refer_5_2_2);

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

TCase *empty_tcase(int threading)
{
  TCase *tc = tcase_create("0 - Empty");

  add_call_fixtures(tc, threading);

  tcase_add_test(tc, empty);

  return tc;
}

/* ====================================================================== */

void check_session_cases(Suite *suite, int threading)
{
  suite_add_tcase(suite, invite_tcase(threading));
  suite_add_tcase(suite, cancel_tcase(threading));
  suite_add_tcase(suite, session_timer_tcase(threading));
  suite_add_tcase(suite, invite_100rel_tcase(threading));
  suite_add_tcase(suite, invite_precondition_tcase(threading));
  suite_add_tcase(suite, reinvite_tcase(threading));
  suite_add_tcase(suite, invite_error_tcase(threading));
  suite_add_tcase(suite, termination_tcase(threading));
  suite_add_tcase(suite, destroy_tcase(threading));
  suite_add_tcase(suite, options_tcase(threading));
  suite_add_tcase(suite, refer_tcase(threading));

  if (0)			/* Template */
    suite_add_tcase(suite, empty_tcase(threading));
}
