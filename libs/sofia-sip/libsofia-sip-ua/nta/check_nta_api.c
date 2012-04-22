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

/**@CFILE check_nta_api.c
 *
 * @brief Check-driven tester for NTA API
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2009 Nokia Corporation.
 */

#include "config.h"

#include "check_nta.h"
#include "s2base.h"
#include "nta_internal.h"

#include <sofia-sip/nta.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_io.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NONE ((void *)-1)

static void
api_setup(void)
{
  s2_nta_setup("NTA", NULL, TAG_END());
}

static void
api_teardown(void)
{
  mark_point();
  s2_nta_teardown();
}

START_TEST(api_1_0_0)
{
  su_root_t *root = s2base->root;
  nta_agent_t *nta;
  su_home_t home[1];
  su_nanotime_t nano;
  nta_agent_magic_t *amagic = (void *)(home + 1);
  nta_outgoing_magic_t *omagic = (void *)(home + 2);
  msg_t *msg;

  memset(home, 0, sizeof home); home->suh_size = (sizeof home);
  su_home_init(home);

  fail_unless(nta_agent_create(NULL,
			       (url_string_t *)"sip:*:*",
			       NULL,
			       NULL,
			       TAG_END()) == NULL);

  fail_unless(nta_agent_create(root,
			  (url_string_t *)"http://localhost:*/invalid/bind/url",
			  NULL,
			  NULL,
			  TAG_END()) == NULL);

  fail_unless(nta_agent_create(root,
			       (url_string_t *)"sip:*:*;transport=XXX",
			       NULL,
			       NULL,
			       TAG_END()) == NULL);
  mark_point();
  nta = nta_agent_create(root,
			 (url_string_t *)"sip:*:*",
			 NULL,
			 NULL,
			 TAG_END());
  fail_unless(nta != NULL);

  mark_point();
  nta_agent_destroy(NULL);
  mark_point();
  nta_agent_destroy(nta);

  mark_point();
  nta = nta_agent_create(root,
			 (url_string_t *)"sip:*:*",
			 s2_nta_msg_callback,
			 amagic,
			 TAG_END());
  fail_unless(nta != NULL);

  fail_unless(nta_agent_contact(NULL) == NULL);
  fail_unless(nta_agent_via(NULL) == NULL);
  fail_if(strcmp(nta_agent_version(nta), nta_agent_version(NULL)));
  fail_unless(nta_agent_magic(NULL) == NULL);
  fail_unless(nta_agent_magic(nta) == amagic);
  fail_unless(nta_agent_add_tport(NULL, NULL, TAG_END()) == -1);
  fail_unless(nta_agent_newtag(home, "tag=%s", NULL) == NULL);
  fail_unless(nta_agent_newtag(home, "tag=%s", nta) != NULL);

  fail_unless(nta_msg_create(NULL, 0) == NULL);
  fail_unless(nta_msg_complete(NULL) == -1);

  fail_unless((msg = nta_msg_create(nta, 0)) != NULL);
  fail_unless(nta_msg_complete(msg) == -1);
  fail_unless(
    nta_msg_request_complete(msg, NULL, SIP_METHOD(FOO), NULL) == -1);
  fail_unless(nta_is_internal_msg(NULL) == 0);
  fail_unless(nta_is_internal_msg(msg) == 0);
  fail_unless(msg_set_flags(msg, NTA_INTERNAL_MSG));
  fail_unless(nta_is_internal_msg(msg) == 1);
  mark_point(); msg_destroy(msg); mark_point();

  fail_unless(nta_leg_tcreate(NULL, NULL, NULL, TAG_END()) == NULL);
  mark_point(); nta_leg_destroy(NULL); mark_point();
  fail_unless(nta_leg_magic(NULL, NULL) == NULL);
  mark_point(); nta_leg_bind(NULL, NULL, NULL); mark_point();
  fail_unless(nta_leg_tag(NULL, "fidsafsa") == NULL);
  fail_unless(nta_leg_rtag(NULL, "fidsafsa") == NULL);
  fail_unless(nta_leg_get_tag(NULL) == NULL);
  fail_unless(nta_leg_client_route(NULL, NULL, NULL) == -1);
  fail_unless(nta_leg_client_reroute(NULL, NULL, NULL, 0) == -1);
  fail_unless(nta_leg_server_route(NULL, NULL, NULL) == -1);
  fail_unless(nta_leg_by_uri(NULL, NULL) == NULL);
  fail_unless(
    nta_leg_by_dialog(NULL,  NULL, NULL, NULL, NULL, NULL, NULL) == NULL);
  fail_unless(
    nta_leg_by_dialog(nta, NULL, NULL, NULL, NULL, NULL, NULL) == NULL);

  fail_unless(nta_leg_make_replaces(NULL, NULL, 1) == NULL);
  fail_unless(nta_leg_by_replaces(NULL, NULL) == NULL);

  fail_unless(nta_incoming_create(NULL, NULL, NULL, NULL, TAG_END()) == NULL);
  fail_unless(nta_incoming_create(nta, NULL, NULL, NULL, TAG_END()) == NULL);

  mark_point(); nta_incoming_bind(NULL, NULL, NULL); mark_point();
  fail_unless(nta_incoming_magic(NULL, NULL) == NULL);

  fail_unless(nta_incoming_find(NULL, NULL, NULL) == NULL);
  fail_unless(nta_incoming_find(nta, NULL, NULL) == NULL);

  fail_unless(nta_incoming_tag(NULL, NULL) == NULL);
  fail_unless(nta_incoming_gettag(NULL) == NULL);

  fail_unless(nta_incoming_status(NULL) == 400);
  fail_unless(nta_incoming_method(NULL) == sip_method_invalid);
  fail_unless(nta_incoming_method_name(NULL) == NULL);
  fail_unless(nta_incoming_url(NULL) == NULL);
  fail_unless(nta_incoming_cseq(NULL) == 0);
  fail_unless(nta_incoming_received(NULL, &nano) == 0);
  fail_unless(nano == 0);

  fail_unless(nta_incoming_set_params(NULL, TAG_END()) == -1);

  fail_unless(nta_incoming_getrequest(NULL) == NULL);
  fail_unless(nta_incoming_getrequest_ackcancel(NULL) == NULL);
  fail_unless(nta_incoming_getresponse(NULL) == NULL);

  fail_unless(
    nta_incoming_complete_response(NULL, NULL, 800, "foo", TAG_END()) == -1);

  fail_unless(nta_incoming_treply(NULL, SIP_200_OK, TAG_END()) == -1);
  fail_unless(nta_incoming_mreply(NULL, NULL) == -1);

  mark_point(); nta_incoming_destroy(NULL); mark_point();

  fail_unless(
    nta_outgoing_tcreate(NULL, s2_nta_orq_callback, omagic,
			 URL_STRING_MAKE("sip:localhost"),
			 SIP_METHOD_MESSAGE,
			 URL_STRING_MAKE("sip:localhost"),
			 TAG_END()) == NULL);

  fail_unless(
    nta_outgoing_mcreate(NULL, s2_nta_orq_callback, omagic,
			 URL_STRING_MAKE("sip:localhost"),
			 NULL,
			 TAG_END()) == NULL);

  fail_unless(nta_outgoing_default(NULL, NULL, NULL) == NULL);

  fail_unless(nta_outgoing_bind(NULL, NULL, NULL) == -1);
  fail_unless(nta_outgoing_magic(NULL, NULL) == NULL);

  fail_unless(nta_outgoing_status(NULL) == 500);
  fail_unless(nta_outgoing_method(NULL) == sip_method_invalid);
  fail_unless(nta_outgoing_method_name(NULL) == NULL);
  fail_unless(nta_outgoing_cseq(NULL) == 0);

  fail_unless(nta_outgoing_delay(NULL) == UINT_MAX);
  fail_unless(nta_outgoing_request_uri(NULL) == NULL);
  fail_unless(nta_outgoing_route_uri(NULL) == NULL);

  fail_unless(nta_outgoing_getresponse(NULL) == NULL);
  fail_unless(nta_outgoing_getrequest(NULL) == NULL);

  fail_unless(nta_outgoing_tagged(NULL, NULL, NULL, NULL, NULL) == NULL);
  fail_unless(nta_outgoing_cancel(NULL) == -1);
  fail_unless(nta_outgoing_tcancel(NULL, NULL, NULL, TAG_END()) == NULL);
  mark_point(); nta_outgoing_destroy(NULL); mark_point();

  fail_unless(nta_outgoing_find(NULL, NULL, NULL, NULL) == NULL);
  fail_unless(nta_outgoing_find(nta, NULL, NULL, NULL) == NULL);

  fail_unless(nta_outgoing_status(NONE) == 500);
  fail_unless(nta_outgoing_method(NONE) == sip_method_invalid);
  fail_unless(nta_outgoing_method_name(NONE) == NULL);
  fail_unless(nta_outgoing_cseq(NONE) == 0);

  fail_unless(nta_outgoing_delay(NONE) == UINT_MAX);
  fail_unless(nta_outgoing_request_uri(NONE) == NULL);
  fail_unless(nta_outgoing_route_uri(NONE) == NULL);

  fail_unless(nta_outgoing_getresponse(NONE) == NULL);
  fail_unless(nta_outgoing_getrequest(NONE) == NULL);

  fail_unless(nta_outgoing_tagged(NONE, NULL, NULL, NULL, NULL) == NULL);
  fail_unless(nta_outgoing_cancel(NONE) == -1);
  fail_unless(nta_outgoing_tcancel(NONE, NULL, NULL, TAG_END()) == NULL);
  mark_point(); nta_outgoing_destroy(NONE); mark_point();

  fail_unless(nta_reliable_treply(NULL, NULL, NULL, 0, NULL, TAG_END()) == NULL);
  fail_unless(nta_reliable_mreply(NULL, NULL, NULL, NULL) == NULL);
  mark_point(); nta_reliable_destroy(NULL); mark_point();

  mark_point(); nta_agent_destroy(nta); mark_point();
  mark_point(); su_home_deinit(home); mark_point();
}
END_TEST

/* ---------------------------------------------------------------------- */

TCase *check_nta_api_1_0(void)
{
  TCase *tc = tcase_create("NTA 1 - API");

  tcase_add_checked_fixture(tc, api_setup, api_teardown);

  tcase_set_timeout(tc, 10);

  tcase_add_test(tc, api_1_0_0);

  return tc;
}
