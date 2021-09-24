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

/**@CFILE check_etsi.c
 *
 * @brief ETSI test cases
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Paulo Pizarro
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

/* ====================================================================== */

/* define XXX as 1 in order to see all failing test cases */
#ifndef XXX
#define XXX (0)
#endif

/* ====================================================================== */

static nua_t *nua;
static soa_session_t *soa = NULL;
static struct dialog *d1 = NULL;
static struct dialog *d2 = NULL;

#define CRLF "\r\n"

static void etsi_setup(void)
{
  nua = s2_nua_setup("ETSI",
                     NUTAG_OUTBOUND("no-options-keepalive, no-validate"),
		     TAG_END());

  soa = soa_create(NULL, s2base->root, NULL);

  fail_if(!soa);

  soa_set_params(soa,
		 SOATAG_USER_SDP_STR("m=audio 5008 RTP/AVP 8 0" CRLF
				     "m=video 5010 RTP/AVP 34" CRLF),
		 TAG_END());

  d1 = su_home_new(sizeof *d1); fail_if(!d1);
  d2 = su_home_new(sizeof *d2); fail_if(!d2);
}

static void etsi_thread_setup(void)
{
  s2_nua_thread = 1;
  etsi_setup();
}

static void etsi_threadless_setup(void)
{
  s2_nua_thread = 0;
  etsi_setup();
}

static void etsi_teardown(void)
{
  s2_teardown_started("ETSI");

  mark_point();

  nua_shutdown(nua);
  fail_unless_event(nua_r_shutdown, 200);

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

static void
bye_by_nua(struct dialog *dialog, nua_handle_t *nh,
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

/* ====================================================================== */
/* 6 - ETSI cases */

/* 6.1 Call Control - Originating Endpoint - Call Establishment */

START_TEST(SIP_CC_OE_CE_V_019)
{
  nua_handle_t *nh;
  struct message *invite;
  struct message *bye;

  S2_CASE("6.1.1", "SIP_CC_OE_CE_V_019",
          "Ensure that the IUT when an INVITE client transaction "
          "is in the Calling state, on receipt of Success (200 OK) "
          "responses differing only on the tag in the To header, "
          "sends an ACK request with a To header identical to the "
          "received one for each received Success (200 OK) responses.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());

  process_offer(invite);

  respond_with_sdp(invite, d1, SIP_200_OK, TAG_END());

  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  respond_with_sdp(invite, d2, SIP_200_OK, TAG_END());
  s2_sip_free_message(invite);

  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  bye = s2_sip_wait_for_request(SIP_METHOD_BYE);
  fail_if(!bye);
  s2_sip_respond_to(bye, d2, SIP_200_OK, TAG_END());
  s2_sip_free_message(bye);

  bye_by_nua(d1, nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(SIP_CC_OE_CE_TI_008)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("6.1.2", "SIP_CC_OE_CE_TI_008",
          "If an unreliable transport is used, ensure that "
          "the IUT, when an INVITE client transaction is in "
          "the Completed state, on receipt of final responses "
          "that matches the transaction, still answer with an "
          "ACK request until timer D set to at least 32 second expires.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());

  s2_sip_respond_to(invite, d1, 404, "First not found", TAG_END());
  fail_unless_event(nua_r_invite, 404);
  fail_unless(s2_check_callstate(nua_callstate_terminated));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(5, s2base->root);

  s2_sip_respond_to(invite, d1, 404, "Not found after 5 seconds", TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(5, s2base->root);

  s2_sip_respond_to(invite, d1, 404, "Not found after 10 seconds", TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(21, s2base->root);

  s2_sip_respond_to(invite, d1, 404, "Not found after 31 seconds", TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(5, s2base->root);

  /* Wake up nua thread and let it time out INVITE transaction */
  nua_set_params(s2->nua, TAG_END());
  fail_unless_event(nua_r_set_params, 0);

  s2_sip_respond_to(invite, d1, 404, "Not found after 32 seconds", TAG_END());
  s2_sip_free_message(invite);
  fail_if(s2_sip_check_request_timeout(SIP_METHOD_ACK, 3));

  nua_handle_destroy(nh);
}
END_TEST

START_TEST(SIP_CC_OE_CE_TI_011_012)
{
  nua_handle_t *nh;
  struct message *invite;

  S2_CASE("6.1.2", "SIP_CC_OE_CE_TI_011_012",
          "Ensure that the IUT, when an INVITE client transaction "
          "has been in the Terminated state, on receipt of a "
          "retransmitted Success (200 OK) responses sends an ACK "
          "request until 64*T1 duration expires, after this, "
          "on receipt of a retransmitted Success (200 OK) "
          "responses does not send an ACK request.");

  nh = nua_handle(nua, NULL, SIPTAG_TO(s2sip->aor), TAG_END());

  invite = invite_sent_by_nua(nh, TAG_END());

  process_offer(invite);

  respond_with_sdp(invite, d1, SIP_200_OK, TAG_END());

  fail_unless_event(nua_r_invite, 200);
  fail_unless(s2_check_callstate(nua_callstate_ready));
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(5, s2base->root);
  respond_with_sdp(invite, d1, SIP_200_OK, TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(5, s2base->root);
  respond_with_sdp(invite, d1, SIP_200_OK, TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  s2_nua_fast_forward(20, s2base->root);
  respond_with_sdp(invite, d1, SIP_200_OK, TAG_END());
  fail_unless(s2_sip_check_request(SIP_METHOD_ACK));

  /* Stack times out the INVITE transaction */
  s2_nua_fast_forward(5, s2base->root);

  respond_with_sdp(invite, d1, SIP_200_OK,
		   SIPTAG_SUBJECT_STR("Stray 200 OK"),
		   TAG_END());
  s2_sip_free_message(invite);
  mark_point();
  fail_if(s2_sip_check_request_timeout(SIP_METHOD_ACK, 3));

  bye_by_nua(d1, nh, TAG_END());

  nua_handle_destroy(nh);
}
END_TEST

TCase *sip_cc_oe_ce_tcase(int threading)
{
  TCase *tc = tcase_create("6.1 - ETSI CC OE - Call Establishment");

  void (*setup)(void) = threading ? etsi_thread_setup : etsi_threadless_setup;

  tcase_add_checked_fixture(tc, setup, etsi_teardown);
  {
    tcase_add_test(tc, SIP_CC_OE_CE_V_019);
    tcase_add_test(tc, SIP_CC_OE_CE_TI_008);
    tcase_add_test(tc, SIP_CC_OE_CE_TI_011_012);
  }
  return tc;
}

/* ====================================================================== */

void check_etsi_cases(Suite *suite, int threading)
{
  suite_add_tcase(suite, sip_cc_oe_ce_tcase(threading));
}
