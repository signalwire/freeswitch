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
 *
 *
 * switch_ivr_play_say.c -- IVR tests
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

static void on_record_start(switch_event_t *event)
{
	char *str = NULL;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	switch_event_serialize(event, &str, SWITCH_FALSE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s", str);
	switch_safe_free(str);
	if (uuid) {
		switch_core_session_t *session = switch_core_session_locate(uuid);
		if (session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *recording_id = switch_event_get_header_nil(event, "Recording-Variable-ID");
			if (!strcmp(recording_id, "foo")) {
				switch_channel_set_variable(channel, "record_start_event_test_pass", "true");
			}
			switch_core_session_rwunlock(session);
		}
	}
}

static void on_record_stop(switch_event_t *event)
{
	char *str = NULL;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	switch_event_serialize(event, &str, SWITCH_FALSE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s", str);
	switch_safe_free(str);
	if (uuid) {
		switch_core_session_t *session = switch_core_session_locate(uuid);
		if (session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *recording_id = switch_event_get_header_nil(event, "Recording-Variable-ID");
			if (!strcmp(recording_id, "foo")) {
				switch_channel_set_variable(channel, "record_stop_event_test_pass", "true");
			}
			switch_core_session_rwunlock(session);
		}
	}
}

static switch_status_t partial_play_and_collect_input_callback(switch_core_session_t *session, void *input, switch_input_type_t input_type, void *data, __attribute__((unused))unsigned int len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int *count = (int *)data;

	if (input_type == SWITCH_INPUT_TYPE_EVENT) {
		switch_event_t *event = (switch_event_t *)input;

		if (event->event_id == SWITCH_EVENT_DETECTED_SPEECH) {
			const char *speech_type = switch_event_get_header(event, "Speech-Type");
			char *body = switch_event_get_body(event);

			if (zstr(speech_type) || strcmp(speech_type, "detected-partial-speech")) {
				return status;
			}

			(*count)++;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "partial events count: %d\n", *count);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "body=[%s]\n", body);
		}
	} else if (input_type == SWITCH_INPUT_TYPE_DTMF) {
		// never mind
	}

	return status;
}

FST_CORE_BEGIN("./conf_playsay")
{
	FST_SUITE_BEGIN(switch_ivr_play_say)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_tone_stream");
			fst_requires_module("mod_sndfile");
			fst_requires_module("mod_dptools");
			fst_requires_module("mod_test");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_SESSION_BEGIN(play_and_collect_input_failure)
		{
			char terminator_collected = 0;
			char *digits_collected = NULL;
			cJSON *recognition_result = NULL;

			// args
			const char *play_files = "silence_stream://2000";
			const char *speech_engine = "test";
			const char *terminators = "#";
			int min_digits = 1;
			int max_digits = 3;
			int digit_timeout = 15000;
			int no_input_timeout = digit_timeout;
			int speech_complete_timeout = digit_timeout;
			int speech_recognition_timeout = digit_timeout;
			char *speech_grammar_args = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=%d,vad-silence-ms=%d,speech-timeout=%d,language=en-US}default",
													  no_input_timeout, speech_complete_timeout, speech_recognition_timeout);

			switch_status_t status;

			// collect input - 1#
			fst_sched_recv_dtmf("+1", "1");
			fst_sched_recv_dtmf("+2", "2");
			fst_sched_recv_dtmf("+3", "3");
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "123");
			fst_check(terminator_collected == 0);
			cJSON_Delete(recognition_result);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(play_and_collect_input_success)
		{
			char terminator_collected = 0;
			char *digits_collected = NULL;
			cJSON *recognition_result = NULL;

			// args
			const char *play_files = "silence_stream://1000";
			const char *speech_engine = "test";
			const char *terminators = "#";
			int min_digits = 1;
			int max_digits = 99;
			int digit_timeout = 5000;
			int no_input_timeout = digit_timeout;
			int speech_complete_timeout = digit_timeout;
			int speech_recognition_timeout = 60000;
			char *speech_grammar_args = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=%d,vad-silence-ms=%d,speech-timeout=%d,language=en-US}default",
													  no_input_timeout, speech_complete_timeout, speech_recognition_timeout);

			switch_status_t status;

			// collect input - 1#
			fst_sched_recv_dtmf("+2", "1#");
			terminator_collected = 0;
			digits_collected = NULL;
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check_duration(2500, 1000); // should return immediately when term digit is received
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "1");
			fst_check(terminator_collected == '#');

			// collect input - 1# again, same session
			fst_sched_recv_dtmf("+2", "1#");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);

			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // should return immediately when term digit is received
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "1");
			fst_check(terminator_collected == '#');

			// collect input - 1
			fst_sched_recv_dtmf("+2", "1");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(7000, 1000); // should return after timeout when prompt finishes playing
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "1");
			fst_check(terminator_collected == 0);

			// collect input - 12#
			fst_sched_recv_dtmf("+2", "12#");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // should return after timeout when prompt finishes playing
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "12");
			fst_check(terminator_collected == '#');

			// collect input - 12# - long spacing
			fst_sched_recv_dtmf("+2", "1");
			fst_sched_recv_dtmf("+4", "2");
			fst_sched_recv_dtmf("+6", "3");
			fst_sched_recv_dtmf("+8", "4");
			fst_sched_recv_dtmf("+10", "#");

			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(10000, 1000); // should return when dtmf terminator is pressed
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "1234");
			fst_check(terminator_collected == '#');

			// collect input - make an utterance
			speech_complete_timeout = 500; // 'auto' mode...
			speech_grammar_args = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=%d,vad-silence-ms=%d,speech-timeout=%d,language=en-US}default",
													  no_input_timeout, speech_complete_timeout, speech_recognition_timeout);
			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_requires(recognition_result);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // returns when utterance is done
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), "agent");
			fst_check_string_equals(digits_collected, NULL);
			fst_check(terminator_collected == 0);

			// single digit test
			fst_sched_recv_dtmf("+2", "2");
			max_digits = 1;
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // returns when single digit is pressed
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "2");
			fst_check(terminator_collected == 0);

			// three digit test
			fst_sched_recv_dtmf("+2", "259");
			min_digits = 1;
			max_digits = 3;
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_check(recognition_result == NULL);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2000, 1000); // returns when single digit is pressed
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "259");
			fst_check(terminator_collected == 0);

			// min digit test
			fst_sched_recv_dtmf("+2", "25");
			min_digits = 3;
			max_digits = 3;
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_requires(recognition_result);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(7000, 1000); // inter-digit timeout after 2nd digit pressed
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), "");
			fst_check_string_equals(digits_collected, NULL);
			fst_check(terminator_collected == 0);
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(play_and_collect_input_partial)
		{
			char terminator_collected = 0;
			char *digits_collected = NULL;
			cJSON *recognition_result = NULL;

			// args
			const char *play_files = "silence_stream://1000";
			const char *speech_engine = "test";
			const char *terminators = "#";
			int min_digits = 1;
			int max_digits = 99;
			int digit_timeout = 500;
			int no_input_timeout = digit_timeout;
			int speech_complete_timeout = digit_timeout;
			int speech_recognition_timeout = 60000;
			char *speech_grammar_args = switch_core_session_sprintf(fst_session, "{start-input-timers=false,no-input-timeout=%d,vad-silence-ms=%d,speech-timeout=%d,language=en-US,partial=true}default",
											no_input_timeout, speech_complete_timeout, speech_recognition_timeout);
			switch_status_t status;
			switch_input_args_t collect_input_args = { 0 };
			switch_input_args_t *args = NULL;
			int count = 0;

			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;
			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, NULL);
			fst_requires(recognition_result);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // returns when utterance is done
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), "agent");
			fst_check_string_equals(digits_collected, NULL);
			fst_check(terminator_collected == 0);


			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			terminator_collected = 0;
			digits_collected = NULL;
			if (recognition_result) cJSON_Delete(recognition_result);
			recognition_result = NULL;

			args = &collect_input_args;
			args->input_callback = partial_play_and_collect_input_callback;
			args->buf = &count;
			args->buflen = sizeof(int);

			fst_time_mark();
			status = switch_ivr_play_and_collect_input(fst_session, play_files, speech_engine, speech_grammar_args, min_digits, max_digits, terminators, digit_timeout, &recognition_result, &digits_collected, &terminator_collected, args);
			fst_requires(recognition_result);
			// check results
			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_duration(2500, 1000); // returns when utterance is done
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), "agent");
			fst_check_string_equals(digits_collected, NULL);
			fst_check(terminator_collected == 0);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "xxx count = %d\n", count);
			fst_check(count == 3); // 3 partial results
			cJSON_Delete(recognition_result);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(record_file_event_vars)
		{
			const char *record_filename = switch_core_session_sprintf(fst_session, "%s" SWITCH_PATH_SEPARATOR "record_file_event_vars-tmp-%s.wav", SWITCH_GLOBAL_dirs.temp_dir, switch_core_session_get_uuid(fst_session));
			switch_event_t *rec_vars = NULL;
			switch_status_t status;
			switch_event_create_subclass(&rec_vars, SWITCH_EVENT_CLONE, SWITCH_EVENT_SUBCLASS_ANY);
			fst_requires(rec_vars);
			switch_event_bind("record_file_event", SWITCH_EVENT_RECORD_START, SWITCH_EVENT_SUBCLASS_ANY, on_record_start, NULL);
			switch_event_bind("record_file_event", SWITCH_EVENT_RECORD_STOP, SWITCH_EVENT_SUBCLASS_ANY, on_record_stop, NULL);
			switch_event_add_header_string(rec_vars, SWITCH_STACK_BOTTOM, "execute_on_record_start", "set record_start_test_pass=true");
			switch_event_add_header_string(rec_vars, SWITCH_STACK_BOTTOM, "execute_on_record_stop", "set record_stop_test_pass=true");
			switch_event_add_header_string(rec_vars, SWITCH_STACK_BOTTOM, "ID", "foo");
			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			status = switch_ivr_record_file_event(fst_session, NULL, record_filename, NULL, 4, rec_vars);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_xcheck(switch_channel_var_true(fst_channel, "record_start_test_pass"), "Expect record_start_test_pass channel variable set to true");
			fst_xcheck(switch_channel_var_true(fst_channel, "record_stop_test_pass"), "Expect record_stop_test_pass channel variable set to true");
			switch_sleep(1000 * 1000);
			fst_xcheck(switch_channel_var_true(fst_channel, "record_start_event_test_pass"), "Expect RECORD_START event received with Recording-Variable-ID set");
			fst_xcheck(switch_channel_var_true(fst_channel, "record_stop_event_test_pass"), "Expect RECORD_STOP event received with Recording-Variable-ID set");
			switch_event_unbind_callback(on_record_start);
			switch_event_unbind_callback(on_record_stop);
			switch_event_destroy(&rec_vars);
			fst_xcheck(switch_file_exists(record_filename, fst_pool) == SWITCH_STATUS_SUCCESS, "Expect recording file to exist");
			unlink(record_filename);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(record_file_event_chan_vars)
		{
			const char *record_filename = switch_core_session_sprintf(fst_session, "%s" SWITCH_PATH_SEPARATOR "record_file_event_chan_vars-tmp-%s.wav", SWITCH_GLOBAL_dirs.temp_dir, switch_core_session_get_uuid(fst_session));
			switch_event_t *rec_vars = NULL;
			switch_status_t status;
			switch_event_create_subclass(&rec_vars, SWITCH_EVENT_CLONE, SWITCH_EVENT_SUBCLASS_ANY);
			fst_requires(rec_vars);
			switch_event_bind("record_file_event", SWITCH_EVENT_RECORD_START, SWITCH_EVENT_SUBCLASS_ANY, on_record_start, NULL);
			switch_event_bind("record_file_event", SWITCH_EVENT_RECORD_STOP, SWITCH_EVENT_SUBCLASS_ANY, on_record_stop, NULL);
			switch_channel_set_variable(fst_channel, "execute_on_record_start_1", "set record_start_test_pass=true");
			switch_channel_set_variable(fst_channel, "execute_on_record_stop_1", "set record_stop_test_pass=true");
			switch_event_add_header_string(rec_vars, SWITCH_STACK_BOTTOM, "ID", "foo");
			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			status = switch_ivr_record_file_event(fst_session, NULL, record_filename, NULL, 4, rec_vars);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_xcheck(switch_channel_var_true(fst_channel, "record_start_test_pass"), "Expect record_start_test_pass channel variable set to true");
			fst_xcheck(switch_channel_var_true(fst_channel, "record_stop_test_pass"), "Expect record_stop_test_pass channel variable set to true");
			switch_sleep(1000 * 1000);
			fst_xcheck(switch_channel_var_true(fst_channel, "record_start_event_test_pass"), "Expect RECORD_START event received with Recording-Variable-ID set");
			fst_xcheck(switch_channel_var_true(fst_channel, "record_stop_event_test_pass"), "Expect RECORD_STOP event received with Recording-Variable-ID set");
			switch_event_unbind_callback(on_record_start);
			switch_event_unbind_callback(on_record_stop);
			switch_event_destroy(&rec_vars);
			fst_xcheck(switch_file_exists(record_filename, fst_pool) == SWITCH_STATUS_SUCCESS, "Expect recording file to exist");
			unlink(record_filename);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(record_file_event_chan_vars_only)
		{
			const char *record_filename = switch_core_session_sprintf(fst_session, "%s" SWITCH_PATH_SEPARATOR "record_file_event_chan_vars-tmp-%s.wav", SWITCH_GLOBAL_dirs.temp_dir, switch_core_session_get_uuid(fst_session));
			switch_status_t status;
			switch_channel_set_variable(fst_channel, "execute_on_record_start_1", "set record_start_test_pass=true");
			switch_channel_set_variable(fst_channel, "execute_on_record_stop_1", "set record_stop_test_pass=true");
			switch_ivr_displace_session(fst_session, "file_string://silence_stream://500,0!tone_stream://%%(2000,0,350,440)", 0, "r");
			status = switch_ivr_record_file_event(fst_session, NULL, record_filename, NULL, 4, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_xcheck(switch_channel_var_true(fst_channel, "record_start_test_pass"), "Expect record_start_test_pass channel variable set to true");
			fst_xcheck(switch_channel_var_true(fst_channel, "record_stop_test_pass"), "Expect record_stop_test_pass channel variable set to true");
			switch_sleep(1000 * 1000);
			fst_xcheck(switch_file_exists(record_filename, fst_pool) == SWITCH_STATUS_SUCCESS, "Expect recording file to exist");
			unlink(record_filename);
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

