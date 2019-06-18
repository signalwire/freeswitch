/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_ivr_originate.c -- tests originate
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

int reporting = 0;
int destroy = 0;

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_assert(session);
	reporting++;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "session reporting %d\n", reporting);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t my_on_destroy(switch_core_session_t *session)
{
	switch_assert(session);
	destroy++;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "session destroy %d\n", destroy);

	return SWITCH_STATUS_SUCCESS;
}

static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ my_on_reporting,
    /*.on_destroy */ my_on_destroy,
    SSH_FLAG_STICKY
};

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_ivr_originate)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(originate_test_early_state_handler)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_add_state_handler(channel, &state_handlers);
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(!switch_channel_ready(channel));

			switch_core_session_rwunlock(session);

			switch_sleep(1000000);
			fst_check(reporting == 1);
			fst_check(destroy == 1);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_late_state_handler)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_sleep(1000000);
			switch_channel_add_state_handler(channel, &state_handlers);

			switch_core_session_rwunlock(session);

			switch_sleep(1000000);
			fst_check(reporting == 1);
			fst_check(destroy == 2);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(dial_handle_create_json)
		{
			const char *dh_str = "{\n"
			"    \"vars\": {\n"
			"        \"foo\": \"bar\",\n"
			"        \"absolute_codec_string\": \"opus,pcmu,pcma\",\n"
			"        \"ignore_early_media\": \"true\"\n"
			"    },\n"
			"    \"leg_lists\": [\n"
			"        { \"legs\": [\n"
			"            { \n"
			"                \"dial_string\": \"loopback/dest\", \n"
			"                \"vars\": {\n"
			"                    \"bar\": \"bar\"\n"
			"                }\n"
			"            },\n"
			"            { \n"
			"                \"dial_string\": \"sofia/gateway/gw/12345\"\n"
			"            }\n"
			"        ] },\n"
			"        { \"legs\": [\n"
			"            {\n"
			"                \"dial_string\": \"sofia/external/foo@example.com^5551231234\",\n"
			"                \"vars\": {\n"
			"                    \"sip_h_X-Custom\": \"my val\"\n"
			"                }\n"
			"            }\n"
			"        ] },\n"
			"        { \"legs\": [\n"
			"            {\n"
			"                \"dial_string\": \"group/my_group\"\n"
			"            }\n"
			"        ] }\n"
			"    ]\n"
			"}";

			// create dial handle from json string, convert back to json and compare
			switch_dial_handle_t *dh = NULL;
			char *dh_str2 = NULL;
			char *dh_str3 = NULL;
			cJSON *dh_json = NULL;
			fst_requires(switch_dial_handle_create_json(&dh, dh_str) == SWITCH_STATUS_SUCCESS);
			fst_requires(dh != NULL);
			fst_requires(switch_dial_handle_serialize_json_obj(dh, &dh_json) == SWITCH_STATUS_SUCCESS);
			fst_requires(dh_json != NULL);
			fst_requires(switch_dial_handle_serialize_json(dh, &dh_str2) == SWITCH_STATUS_SUCCESS);
			fst_requires(dh_str2 != NULL);
			fst_check_string_equals(dh_str2, "{\"vars\":{\"foo\":\"bar\",\"absolute_codec_string\":\"opus,pcmu,pcma\",\"ignore_early_media\":\"true\"},\"leg_lists\":[{\"legs\":[{\"dial_string\":\"loopback/dest\",\"vars\":{\"bar\":\"bar\"}},{\"dial_string\":\"sofia/gateway/gw/12345\"}]},{\"legs\":[{\"dial_string\":\"sofia/external/foo@example.com^5551231234\",\"vars\":{\"sip_h_X-Custom\":\"my val\"}}]},{\"legs\":[{\"dial_string\":\"group/my_group\"}]}]}");

			dh_str3 = cJSON_PrintUnformatted(dh_json);
			fst_requires(dh_str3);
			fst_check_string_equals(dh_str2, dh_str3);

			switch_safe_free(dh_str2);
			switch_safe_free(dh_str3);
			cJSON_Delete(dh_json);
			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END();
	}
	FST_SUITE_END()
}
FST_CORE_END()
