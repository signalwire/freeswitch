/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "session destroy %d\n", destroy);

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

static int application_hit = 0;

static void loopback_group_confirm_event_handler(switch_event_t *event) // general event handler
{
	if (event->event_id == SWITCH_EVENT_CHANNEL_APPLICATION) {
		application_hit++;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "application_hit = %d\n", application_hit);
	}
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_ivr_originate)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			application_hit = 0;
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

		FST_TEST_BEGIN(originate_test_empty_dial_string)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			/* Dial string is NULL */
			switch_dial_leg_list_add_leg(ll, &leg, NULL);

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_check(status == SWITCH_STATUS_FALSE);

			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_one_leg)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_no_pre_answer)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "null_enable_auto_answer", "1");
			switch_dial_handle_add_global_var(dh, "null_auto_answer_delay", "2000");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);

			fst_check_duration(3000, 500);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_exec_in_pre_answer)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "null_enable_auto_answer", "1");
			switch_dial_handle_add_global_var(dh, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_global_var(dh, "null_pre_answer", "true");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);

			fst_check_duration(1000, 500);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_exec_after_answer_early_ok)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "null_enable_auto_answer", "1");
			switch_dial_handle_add_global_var(dh, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_global_var(dh, "null_pre_answer", "true");
			switch_dial_handle_add_global_var(dh, "group_confirm_early_ok", "false");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);

			fst_check_duration(3000, 500);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_exec_after_answer_ignore_early_media)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "null_enable_auto_answer", "1");
			switch_dial_handle_add_global_var(dh, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_global_var(dh, "null_pre_answer", "true");
			switch_dial_handle_add_global_var(dh, "ignore_early_media", "true");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);

			fst_check_duration(3000, 500);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_2_legs)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test1");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://1000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");

			switch_dial_leg_list_add_leg(ll, &leg, "null/test2");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://500");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			const char *name = switch_core_session_get_name(session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channel %s\n", name);
			fst_check_string_equals(name, "null/test2");
			switch_core_session_rwunlock(session);

			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_global_var)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			const char *dialstring = "{group_confirm_file='playback silence_stream://1000',group_confirm_key=exec}null/test";

			status = switch_ivr_originate(NULL, &session, &cause, dialstring, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			const char *name = switch_core_session_get_name(session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channel %s\n", name);
			fst_check_string_equals(name, "null/test");
			switch_core_session_rwunlock(session);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_local_var)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			const char *dialstring = "[group_confirm_file='playback silence_stream://1000',group_confirm_key=exec]null/test1,"
				"[group_confirm_file='playback silence_stream://500',group_confirm_key=exec]null/test2";

			status = switch_ivr_originate(NULL, &session, &cause, dialstring, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			const char *name = switch_core_session_get_name(session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "channel %s\n", name);
			fst_check_string_equals(name, "null/test2");
			switch_core_session_rwunlock(session);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_loopback_endpoint_originate)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			const char *dialstring = "[group_confirm_key=exec,group_confirm_file='event a=1']loopback/loopback";

			switch_event_bind("test", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, loopback_group_confirm_event_handler, NULL);
			status = switch_ivr_originate(NULL, &session, &cause, dialstring, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			switch_yield(2000000);
			switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
			switch_core_session_rwunlock(session);
			switch_event_unbind_callback(loopback_group_confirm_event_handler);
			fst_check(application_hit == 1);
		}
		FST_TEST_END()

		FST_SESSION_BEGIN(originate_test_group_confirm_loopback_endpoint_bridge)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			const char *dialstring = "[group_confirm_key=exec,group_confirm_file='event a=1']loopback/loopback";

			switch_event_bind("test", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, loopback_group_confirm_event_handler, NULL);

			switch_core_session_execute_application(fst_session, "bridge", dialstring);
			switch_yield(2000000);
			switch_channel_hangup(fst_channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(application_hit == 1);
		}
		FST_SESSION_END()

		FST_TEST_BEGIN(originate_test_group_confirm_leg_timeout_not_finished)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "null_enable_auto_answer", "1");
			switch_dial_handle_add_leg_var(leg, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_leg_var(leg, "leg_timeout", "4");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://6000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_FALSE);
			fst_check(session == NULL);
			fst_check_duration(4500, 600); // (>= 3.9 sec, <= 5.1 sec)
			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_leg_timeout_finished)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "null_enable_auto_answer", "1");
			switch_dial_handle_add_leg_var(leg, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_leg_var(leg, "leg_timeout", "6");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://2000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			fst_xcheck(switch_channel_test_flag(switch_core_session_get_channel(session), CF_WINNER), "Expect session is group confirm winner");
			switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
			switch_core_session_rwunlock(session);
			switch_dial_handle_destroy(&dh);
			fst_check_duration(4000, 500);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_timeout_leg)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "leg_timeout", "15");
			switch_dial_handle_add_leg_var(leg, "null_enable_auto_answer", "1");
			switch_dial_handle_add_leg_var(leg, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://10000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_leg_var(leg, "group_confirm_timeout", "3");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_FALSE);
			fst_check(session == NULL);
			switch_dial_handle_destroy(&dh);
			fst_check_duration(5500, 600); // (> 4.9 sec < 6.1 sec) only 1 second resolution with these timeouts
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_timeout_global)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "leg_timeout", "15");
			switch_dial_handle_add_leg_var(leg, "null_enable_auto_answer", "1");
			switch_dial_handle_add_leg_var(leg, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://10000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "group_confirm_timeout", "3");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_FALSE);
			fst_check(session == NULL);
			fst_check_duration(5500, 600); // (>= 4.9 sec, <= 6.1 sec) only 1 second resolution with these timeouts
			switch_dial_handle_destroy(&dh);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(originate_test_group_confirm_cancel_timeout_global)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_dial_handle_t *dh;
			switch_dial_leg_list_t *ll;
			switch_dial_leg_t *leg = NULL;

			switch_dial_handle_create(&dh);
			switch_dial_handle_add_leg_list(dh, &ll);

			switch_dial_leg_list_add_leg(ll, &leg, "null/test");
			switch_dial_handle_add_leg_var(leg, "leg_timeout", "3");
			switch_dial_handle_add_leg_var(leg, "null_enable_auto_answer", "1");
			switch_dial_handle_add_leg_var(leg, "null_auto_answer_delay", "2000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_file", "playback silence_stream://2000");
			switch_dial_handle_add_leg_var(leg, "group_confirm_key", "exec");
			switch_dial_handle_add_global_var(dh, "group_confirm_cancel_timeout", "true");

			status = switch_ivr_originate(NULL, &session, &cause, NULL, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, dh);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(session);
			fst_xcheck(switch_channel_test_flag(switch_core_session_get_channel(session), CF_WINNER), "Expect session is group confirm winner");
			switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
			switch_core_session_rwunlock(session);
			switch_dial_handle_destroy(&dh);
			fst_check_duration(4500, 600); // (>= 3.9 sec, <= 5.1 sec)
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
