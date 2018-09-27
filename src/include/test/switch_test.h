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
 *
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <seven@signalwire.com>
 *
 * switch_test.h -- FreeSWITCH test macros
 */
#ifndef SWITCH_FST_H
#define SWITCH_FST_H

#include <switch.h>

#include <test/switch_fct.h>


/**
 * Get environment variable and save to var
 */
#define fst_getenv(env, default_value) \
	char *env = getenv(#env); \
	if (!env) { \
		env = (char *)default_value; \
	}

/**
 * Get mandatory environment variable and save to var.  Exit with error if missing.
 */
#define fst_getenv_required(env) \
	char *env = getenv(#env); \
	if (!env) { \
		fprintf(stderr, "Failed to start test: environment variable \"%s\" is not set!\n", #env); \
		exit(1); \
	}


/**
 * initialize FS core from optional configuration dir
 */
static void fst_init_core_and_modload(const char *confdir)
{
	const char *err;
	if (!zstr(confdir)) {
		SWITCH_GLOBAL_dirs.conf_dir = strdup(confdir);
	}
	SWITCH_GLOBAL_dirs.sounds_dir = strdup("./");
	switch_core_init_and_modload(0, SWITCH_TRUE, &err);
	switch_sleep(1 * 1000000);
	switch_core_set_variable("sound_prefix", "");
}

/**
 * Park FreeSWITCH session.  This is handy when wanting to use switch_core_session_execute_async() on the test session.
 * @param session to park
 */
static void fst_session_park(switch_core_session_t *session)
{
	switch_ivr_park_session(session);
	switch_channel_wait_for_state(switch_core_session_get_channel(session), NULL, CS_PARK);
}


/**
 * check for test requirement - execute teardown on failure
 */
#define fst_requires fct_req

/**
 * check for required module - execute teardown on failure
 */
#define fst_requires_module(modname) fct_req(switch_loadable_module_exists(modname) == SWITCH_STATUS_SUCCESS)

/**
 * test boolean expression - continue test execution on failure
 */
#define fst_check fct_chk

/**
 * test integers for equality - continue test execution on failure
 */
#define fst_check_int_equals fct_chk_eq_int

/**
 * test strings for equality - continue test execution on failure
 */
#define fst_check_string_equals fct_chk_eq_str

/**
 * test strings for inequality - continue test execution on failure
 */
#define fst_check_string_not_equals fct_chk_neq_str


/**
 * Define the beginning of a freeswitch core test driver.  Only one per test application allowed.
 * @param confdir directory containing freeswitch.xml configuration
 */
#define FST_CORE_BEGIN(confdir) \
	FCT_BGN() \
	{ \
		switch_timer_t fst_timer = { 0 }; \
		switch_memory_pool_t *fst_pool = NULL; \
		fst_init_core_and_modload(confdir);

/**
 * Define the end of a freeswitch core test driver.
 */
#define FST_CORE_END() \
		/*switch_core_destroy();*/ \
	} \
	FCT_END()


/**
 * Define the beginning of a FreeSWITCH module test suite.  Loads the module for test.
 * @param modname name of module to load.
 * @param suite the name of this test suite
 */
#define FST_MODULE_BEGIN(modname,suite) \
	const char *fst_test_module = #modname; \
	if (!zstr(fst_test_module)) { \
		const char *err; \
		switch_loadable_module_load_module((char *)"../.libs", (char *)fst_test_module, SWITCH_FALSE, &err); \
	} \
	FCT_FIXTURE_SUITE_BGN(suite)

/**
 * Define the end of a FreeSWITCH module test suite.
 */
#define FST_MODULE_END FCT_FIXTURE_SUITE_END


/**
 * Define the beginning of a test suite not associated with a module. 
 * @param suite the name of this test suite
 */
#define FST_SUITE_BEGIN(suite) FCT_FIXTURE_SUITE_BGN \
	const char *fst_test_module = NULL; \
	FCT_FIXTURE_SUITE_BGN(suite)

/**
 * Define the end of a test suite.
 */
#define FST_SUITE_END FCT_FIXTURE_SUITE_END


/**
 * Define the test suite setup.  This is run before each test or session test.
 */
#define FST_SETUP_BEGIN() \
	FCT_SETUP_BGN() \
		switch_core_new_memory_pool(&fst_pool); \
		fst_requires(fst_pool != NULL); \
		fst_requires(switch_core_timer_init(&fst_timer, "soft", 20, 160, fst_pool) == SWITCH_STATUS_SUCCESS);

/**
 * Define the end of test suite setup.
 */
#define FST_SETUP_END FCT_SETUP_END


/**
 * Define the test suite teardown.  This is run after each test or session test.
 */
#define FST_TEARDOWN_BEGIN() \
	FCT_TEARDOWN_BGN() \
		switch_core_destroy_memory_pool(&fst_pool); \
		switch_core_timer_destroy(&fst_timer);

/**
 * Define the test suite teardown end.
 */
#define FST_TEARDOWN_END FCT_TEARDOWN_END

/**
 * Define a test in a test suite.
 * Defined vars:
 *   switch_memory_pool_t *fst_pool;   A memory pool that is torn down on test completion
 *   switch_core_timer_t *fst_timer;   A 8kHz, 20ms soft timer (160 samples per frame)
 * @param name the name of this test
 */
#define FST_TEST_BEGIN(name) \
	FCT_TEST_BGN(name) \
		if (fst_test_module) { \
			fst_requires_module(fst_test_module); \
		}

#define FST_TEST_END FCT_TEST_END


/**
 * Define a session test in a test suite.  This can be used to test IVR functions.
 *
 * Records session audio to /tmp/name.wav where name is the name of the test.
 *
 * Required modules:
 *   mod_loopback - for null endpoint
 *   mod_sndfile  - for wav file support
 *
 * Defined vars:
 *   switch_memory_pool_t *fst_pool;          A memory pool that is torn down on test completion
 *   switch_core_timer_t *fst_timer;          A 8kHz, 20ms soft timer (160 samples per frame)
 *   switch_core_session_t *fst_session;      The outbound null session.  L16, 1 channel, 8kHz.
 *   switch_core_session_t *fst_session_pool; The outbound null session's pool.
 *   switch_channel_t *fst_channel;           The outbound null session's channel.
 *
 * @param name the name of this test
 */
#define FST_SESSION_BEGIN(name) \
	FCT_TEST_BGN(name) \
	{ \
		switch_core_session_t *fst_session = NULL; \
		switch_event_t *fst_originate_vars = NULL; \
		switch_call_cause_t fst_cause = SWITCH_CAUSE_NORMAL_CLEARING; \
		if (fst_test_module) { \
			fst_requires_module(fst_test_module); \
		} \
		fst_requires_module("mod_loopback"); \
		fst_requires_module("mod_sndfile"); \
		fst_requires(switch_core_running()); \
		fst_requires(switch_event_create_plain(&fst_originate_vars, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS); \
		switch_event_add_header_string(fst_originate_vars, SWITCH_STACK_BOTTOM, "origination_caller_id_number", "+15551112222"); \
		if (switch_ivr_originate(NULL, &fst_session, &fst_cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, fst_originate_vars, SOF_NONE, NULL, NULL) == SWITCH_STATUS_SUCCESS && fst_session) { \
			switch_memory_pool_t *fst_session_pool = switch_core_session_get_pool(fst_session); \
			switch_channel_t *fst_channel = switch_core_session_get_channel(fst_session); \
			switch_channel_set_state(fst_channel, CS_SOFT_EXECUTE); \
			switch_channel_wait_for_state(fst_channel, NULL, CS_SOFT_EXECUTE); \
			switch_channel_set_variable(fst_channel, "send_silence_when_idle", "-1"); \
			switch_channel_set_variable(fst_channel, "RECORD_STEREO", "true"); \
			switch_ivr_record_session(fst_session, (char *)"/tmp/"#name".wav", 0, NULL); \
			for(;;) {

/* BODY OF TEST CASE HERE */

/**
 * Define the end of a session test in a test suite. 
 * Hangs up session under test.
 */
#define FST_SESSION_END() \
				break; \
			} \
			if (switch_channel_ready(fst_channel)) { \
				switch_channel_hangup(fst_channel, SWITCH_CAUSE_NORMAL_CLEARING); \
			} \
			if (fst_originate_vars) { \
				switch_event_destroy(&fst_originate_vars); \
			} \
			switch_core_session_rwunlock(fst_session); \
			switch_sleep(1000000); \
		} \
	} \
	FCT_TEST_END()


/* CORE ASR TEST MACROS */

/**
 * Open core ASR for a recognizer module.  Opens for L16, 1 channel, 8KHz. 
 *
 * Test Requires:
 *    switch_core_asr_open() == SWITCH_STATUS_SUCCESS
 *
 * Defined vars:
 *   switch_asr_handle_t ah;              Core ASR handle
 *   switch_asr_flag_t flags;             Core ASR flags used to open recognizer.
 *   char *fst_asr_result;                Result of last recognition.  Allocated from test memory pool.
 *
 * @param recognizer name of recognizer to open (like gcloud_dialogflow)
 * 
 */
#define fst_test_core_asr_open(recognizer) \
{\
	char *fst_asr_result = NULL; \
	switch_asr_handle_t ah = { 0 }; \
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE; \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Open recognizer: %s\n", recognizer); \
	/* open ASR interface and feed recorded audio into it and collect result */ \
	fst_requires(switch_core_asr_open(&ah, recognizer, "L16", 8000, "", &flags, fst_pool) == SWITCH_STATUS_SUCCESS); \

/**
 * Execute test on opened recognizer.  Reads audio from input file and passes it to the recognizer.
 * 
 * Test Requires:
 *   switch_core_asr_load_grammar(grammar) == SWITCH_STATUS_SUCCESS
 *   switch_core_file_open(input_filename) == SWITCH_STATUS_SUCCESS
 *   switch_core_file_close() == SWITCH_STATUS_SUCCESS
 *
 * Test Checks:
 *   Got result from recognizer.
 *
 * Test Output:
 *   fst_asr_result has the xmlstr from switch_core_file_get_results()
 *
 * @param grammar recognizer grammar
 * @param input_filename name of file containing audio to send to recognizer.
 */
#define fst_test_core_asr(grammar, input_filename) \
{ \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test recognizer: input = %s\n", input_filename); \
	fst_asr_result = NULL; \
	fst_requires(switch_core_asr_load_grammar(&ah, grammar, "") == SWITCH_STATUS_SUCCESS); \
	/* feed file into ASR */ \
	switch_status_t result; \
	switch_file_handle_t file_handle = { 0 }; \
	file_handle.channels = 1; \
	file_handle.native_rate = 8000; \
	fst_requires(switch_core_file_open(&file_handle, input_filename, file_handle.channels, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS); \
	uint8_t *buf = (uint8_t *)switch_core_alloc(fst_pool, sizeof(uint8_t) * 160 * sizeof(uint16_t) * file_handle.channels); \
	size_t len = 160; \
	int got_result = 0; \
	switch_core_timer_sync(&fst_timer); \
	while ((result = switch_core_file_read(&file_handle, buf, &len)) == SWITCH_STATUS_SUCCESS) { \
		fst_requires(switch_core_asr_feed(&ah, buf, len * sizeof(int16_t), &flags) == SWITCH_STATUS_SUCCESS); \
		switch_core_timer_next(&fst_timer); \
		if (switch_core_asr_check_results(&ah, &flags) == SWITCH_STATUS_SUCCESS) { \
			flags = SWITCH_ASR_FLAG_NONE; \
			/* switch_ivr_detect_speech.. checks one in media bug then again in speech_thread  */ \
			fst_requires(switch_core_asr_check_results(&ah, &flags) == SWITCH_STATUS_SUCCESS); \
			char *xmlstr = NULL; \
			switch_event_t *headers = NULL; \
			result = switch_core_asr_get_results(&ah, &xmlstr, &flags); \
			if (result == SWITCH_STATUS_SUCCESS) { \
				got_result++; \
				switch_core_asr_get_result_headers(&ah, &headers, &flags); \
				if (headers) { \
					switch_event_destroy(&headers); \
				} \
				fst_check(xmlstr != NULL); \
				if (xmlstr != NULL) { \
					fst_asr_result = switch_core_strdup(fst_pool, xmlstr);\
				} \
				switch_safe_free(xmlstr); \
				break; \
			} \
		} \
		len = 160; \
	} \
	fst_check(got_result == 1); \
	fst_requires(switch_core_file_close(&file_handle) == SWITCH_STATUS_SUCCESS); \
}

/**
 * Pause an open recognizer.
 *
 * Test Requires:
 *    switch_core_asr_pause(&ah) == SWITCH_STATUS_SUCCESS
 */
#define fst_test_core_asr_pause() \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Pause recognizer\n"); \
	flags = SWITCH_ASR_FLAG_NONE; \
	fst_requires(switch_core_asr_pause(&ah) == SWITCH_STATUS_SUCCESS);

/**
 * Resumes an open recognizer
 *
 * Test Requires:
 *    switch_core_asr_resume(&ah) == SWITCH_STATUS_SUCCESS
 */
#define fst_test_core_asr_resume() \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Resume recognizer\n"); \
	flags = SWITCH_ASR_FLAG_NONE; \
	fst_requires(switch_core_asr_resume(&ah) == SWITCH_STATUS_SUCCESS);	

/**
 * Close an open recognizer
 *
 * Test Requires:
 *   switch_core_asr_close(&ah, flags) == SWITCH_STATUS_SUCCESS
 */
#define fst_test_core_asr_close() \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Close recognizer\n"); \
	flags = SWITCH_ASR_FLAG_NONE; \
	fst_requires(switch_core_asr_close(&ah, &flags) == SWITCH_STATUS_SUCCESS); \
}



/* PLAY AND DETECT SPEECH TEST MACROS - requires FST_SESSION */

/**
 * Define beginning of play_and_detect_speech recognizer test
 *
 * Defined vars:
 *   const char *fst_asr_result;   Result of last recognition.  Allocated from test memory pool.
 */
#define fst_play_and_detect_speech_test_begin() \
{ \
	const char *fst_asr_result = NULL;

/**
 * Use play_and_detect_speech APP to test recognizer
 *
 * Test Requires:
 *   switch_ivr_displace_session(input_filename) == SWITCH_STATUS_SUCCESS
 *   switch_core_session_execute_application(play_and_detect_speech) == SWITCH_STATUS_SUCCESS
 *
 * Test Checks:
 *   fst_asr_result != NULL after recognition completes
 *
 * Test Output:
 *   fst_asr_result has the result from detect_speech_result channel variable.
 *
 * @param recognizer name of recognizer
 * @param grammar recognizer grammar
 * @param prompt_filename name of prompt to play
 * @param input_filename name of file containing input audio for the recognizer
 */
#define fst_play_and_detect_speech_test(recognizer, grammar, prompt_filename, input_filename) \
{ \
	char *args = NULL; \
	switch_channel_set_variable(fst_channel, "detect_speech_result", ""); \
	fst_requires(switch_ivr_displace_session(fst_session, input_filename, 0, "r") == SWITCH_STATUS_SUCCESS); \
	args = switch_core_session_sprintf(fst_session, "%s detect:%s %s", prompt_filename, recognizer, grammar); \
	fst_requires(switch_core_session_execute_application(fst_session, "play_and_detect_speech", args) == SWITCH_STATUS_SUCCESS); \
	fst_asr_result = switch_channel_get_variable(fst_channel, "detect_speech_result"); \
	fst_check(fst_asr_result != NULL); \
}

/**
 * Define end of play_and_detect_speech recognizer test
 */
#define fst_play_and_detect_speech_test_end() \
}

/**
 * Parse JSON file and save to varname
 *
 * Test Requires:
 *   JSON file can be opened and parsed
 *
 * Test Output:
 *   varname points at cJSON object
 *
 * @param varname name of var to store the resulting cJSON object
 * @param file name of file to parse
 */
#define fst_parse_json_file(varname, file) \
cJSON *varname = NULL; \
{ \
	char *buf; \
	struct stat s; \
	int size; \
	int fd = open(file, O_RDONLY); \
	fst_requires(fd >= 0); \
	fstat(fd, &s); \
	buf = malloc(s.st_size + 1); \
	fst_requires(buf); \
	size = read(fd, buf, s.st_size); \
	fst_requires(size == s.st_size); \
	close(fd); \
	varname = cJSON_Parse(buf); \
	free(buf); \
	fst_requires(varname); \
}

#endif
