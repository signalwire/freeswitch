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
 *
 *
 * test_asr.c -- tests for mock test asr interface
 *
 */
#include <switch.h>
#include <test/switch_test.h>
#include <stdlib.h>


static const char *get_query_result_text(switch_memory_pool_t *pool, const char *result)
{
	const char *result_text = NULL;
	cJSON *result_json = cJSON_Parse(result);
	if (result_json) {
		const char *text = cJSON_GetObjectCstr(result_json, "text");
		if (!zstr(text)) {
			result_text = switch_core_strdup(pool, text);
		} else {
			text = cJSON_GetObjectCstr(result_json, "error");
			if (!zstr(text)) {
				result_text = switch_core_strdup(pool, text);
			}
		}
		cJSON_Delete(result_json);
	}
	return result_text;
}


FST_CORE_BEGIN(".")

FST_MODULE_BEGIN(mod_test, test_asr)

FST_SETUP_BEGIN()
{
	fst_requires_module("mod_tone_stream");
	fst_requires_module("mod_sndfile");
	fst_requires_module("mod_dptools");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()


FST_TEST_BEGIN(core_asr)
{
	char path[1024];
	const char* session_id = "123435";
	char *grammar = switch_core_sprintf(fst_pool, "{start-input-timers=true,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default", session_id);
	fst_test_core_asr_open("test");
	sprintf(path, "%s%s%s%s", "file_string://silence_stream://3000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_resume();
	fst_test_core_asr(
		grammar,
		"silence_stream://30000,0");
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "no_input");
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_resume();
	fst_test_core_asr(
		grammar,
		"silence_stream://30000,0");
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "no_input");
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_pause();
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_close();
	fst_test_core_asr_open("test");
	sprintf(path, "%s%s%s%s", "file_string://silence_stream://1000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_close();
}
FST_TEST_END()

FST_TEST_BEGIN(core_asr_auto_resume)
{
	char path[1024];
	const char* session_id = "123435";
	char *grammar = switch_core_sprintf(fst_pool, "{start-input-timers=true,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default", session_id);
	fst_test_core_asr_open("test");
	switch_set_flag(&ah, SWITCH_ASR_FLAG_AUTO_RESUME);
	sprintf(path, "%s%s%s%s", "file_string://silence_stream://3000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr(
		grammar,
		"silence_stream://30000,0");
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "no_input");
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr(
		grammar,
		"silence_stream://30000,0");
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "no_input");
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://silence_stream://1000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_close();
}
FST_TEST_END()

FST_TEST_BEGIN(core_asr_abuse)
{
	char path[1024];
	const char* session_id = "5351514";
	char *grammar = switch_core_sprintf(fst_pool, "{start-input-timers=true,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default", session_id);
	fst_test_core_asr_open("test");
	sprintf(path, "%s%s%s%s", "file_string://silence_stream://3000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_resume();
	fst_test_core_asr_resume();
	fst_test_core_asr_resume();
	fst_test_core_asr_pause();
	fst_test_core_asr_resume();
	sprintf(path, "%s%s%s%s", "file_string://", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav!silence_stream://3000,0");
	fst_test_core_asr(
		grammar,
		path);
	fst_check_string_equals(get_query_result_text(fst_pool, fst_asr_result), "agent");
	fst_test_core_asr_resume();

	// Tested double-close, but FS core will crash... 
	fst_test_core_asr_close();
}
FST_TEST_END()


FST_SESSION_BEGIN(play_and_detect_1)
{
	char path[1024];
	char path2[1024];
	const char *result_text = NULL;
	char *grammar = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default", switch_core_session_get_uuid(fst_session));
	fst_play_and_detect_speech_test_begin();

	/* initial welcome and request */
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	sprintf(path2, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav");
	fst_play_and_detect_speech_app_test("test",
		grammar,
		path,
		path2);
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "agent");

	/* follow up request */
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	sprintf(path2, "%s%s%s%s", "file_string://1000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav");
	fst_play_and_detect_speech_app_test("test",
		grammar,
		path,
		path2);
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "agent");

	fst_play_and_detect_speech_test_end();
}
FST_SESSION_END()

FST_SESSION_BEGIN(play_and_detect_no_input_follow_up)
{
	char path[1024];
	char path2[1024];
	const char *result_text = NULL;
	char *grammar = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}", switch_core_session_get_uuid(fst_session));

	switch_ivr_schedule_hangup(switch_epoch_time_now(NULL) + 60, switch_core_session_get_uuid(fst_session), SWITCH_CAUSE_NORMAL_CLEARING, SWITCH_FALSE);
	fst_play_and_detect_speech_test_begin();
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	sprintf(path2, "%s%s%s%s", "file_string://silence_stream://4000,0!", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/agent.wav");
	fst_play_and_detect_speech_app_test("test",
		grammar,
		path,
		path2);
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "agent");

	/* follow up request - no input */
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	fst_play_and_detect_speech_app_test("test",
		grammar,
		path,
		"silence_stream://10000,0");
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "no_input");

	fst_play_and_detect_speech_test_end();
}
FST_SESSION_END()

FST_SESSION_BEGIN(play_and_detect_no_input)
{
	char path[1024];
	const char *result_text = NULL;

	switch_ivr_schedule_hangup(switch_epoch_time_now(NULL) + 60, switch_core_session_get_uuid(fst_session), SWITCH_CAUSE_NORMAL_CLEARING, SWITCH_FALSE);
	fst_play_and_detect_speech_test_begin();
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	fst_play_and_detect_speech_app_test("test",
		switch_core_session_sprintf(fst_session,
			"{start-input-timers=false,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default",
			switch_core_session_get_uuid(fst_session)),
		path,
		"silence_stream://10000,0");
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "no_input");

	fst_play_and_detect_speech_test_end();
}
FST_SESSION_END()

FST_SESSION_BEGIN(play_and_detect_start_input_timers)
{
	char path[1024];
	const char *result_text = NULL;

	switch_ivr_schedule_hangup(switch_epoch_time_now(NULL) + 60, switch_core_session_get_uuid(fst_session), SWITCH_CAUSE_NORMAL_CLEARING, SWITCH_FALSE);
	fst_play_and_detect_speech_test_begin();
	sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "sounds/ivr-please_state_your_name_and_reason_for_calling.wav");
	fst_play_and_detect_speech_app_test("test",
		switch_core_session_sprintf(fst_session, 
			"{start-input-timers=true,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default",
			switch_core_session_get_uuid(fst_session)),
		path,
		"silence_stream://10000,0");
	result_text = get_query_result_text(fst_pool, fst_asr_result);
	fst_requires(result_text != NULL);
	fst_check_string_equals(result_text, "no_input");

	fst_play_and_detect_speech_test_end();

	fst_check_duration(5000, 500);
}
FST_SESSION_END()

FST_TEST_BEGIN(unload_test)
{
	const char *err = NULL;
	switch_sleep(1000000);
	fst_check(switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, (char *)"mod_test", SWITCH_FALSE, &err) == SWITCH_STATUS_SUCCESS);
}
FST_TEST_END()


FST_MODULE_END()

FST_CORE_END()
