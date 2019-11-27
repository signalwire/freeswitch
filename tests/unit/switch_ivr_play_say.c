/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2019, Anthony Minessale II <anthm@freeswitch.org>
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

			fst_check(status == SWITCH_STATUS_SUCCESS); // might be break?
			fst_check_string_equals(cJSON_GetObjectCstr(recognition_result, "text"), NULL);
			fst_check_string_equals(digits_collected, "123");
			fst_check(terminator_collected == 0);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(play_and_collect_input)
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
	}
	FST_SUITE_END()
}
FST_CORE_END()

