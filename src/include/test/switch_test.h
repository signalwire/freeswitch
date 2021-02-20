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
static char *fst_getenv_default(const char *env, char *default_value, switch_bool_t required)
{
	char *val = getenv(env);
	if (!val) {
		if (required) {
			fprintf(stderr, "Failed to start test: environment variable \"%s\" is not set!\n", env);
			exit(1);
		}
		return default_value;
	}
	return val;
}

/**
 * Get environment variable and save to var
 */
#define fst_getenv(env, default_value) \
	char *env = fst_getenv_default(#env, (char *)default_value, SWITCH_FALSE);

/**
 * Get mandatory environment variable and save to var.  Exit with error if missing.
 */
#define fst_getenv_required(env) \
	char *env = fst_getenv_default(#env, NULL, SWITCH_TRUE);

/**
 * initialize FS core from optional configuration dir
 */
static switch_status_t fst_init_core_and_modload(const char *confdir, const char *basedir, int minimal, switch_core_flag_t flags)
{
	switch_status_t status;
	const char *err;
	unsigned long pid = switch_getpid();
	// Let FreeSWITCH core pick these
	//SWITCH_GLOBAL_dirs.base_dir = strdup("/usr/local/freeswitch");
	//SWITCH_GLOBAL_dirs.mod_dir = strdup("/usr/local/freeswitch/mod");
	//SWITCH_GLOBAL_dirs.lib_dir = strdup("/usr/local/freeswitch/lib");
	//SWITCH_GLOBAL_dirs.temp_dir = strdup("/tmp");

#ifdef SWITCH_TEST_BASE_DIR_OVERRIDE
	basedir = SWITCH_TEST_BASE_DIR_OVERRIDE;
#else
#define SWITCH_TEST_BASE_DIR_OVERRIDE "."
#endif

	if (zstr(basedir)) {
		basedir = ".";
	}

	// Allow test to define the runtime dir
	if (!zstr(confdir)) {
#ifdef SWITCH_TEST_BASE_DIR_FOR_CONF
		SWITCH_GLOBAL_dirs.conf_dir = switch_mprintf("%s%s%s", SWITCH_TEST_BASE_DIR_FOR_CONF, SWITCH_PATH_SEPARATOR, confdir);
#else
		if (confdir[0] != '/') {
			SWITCH_GLOBAL_dirs.conf_dir = switch_mprintf(".%s%s", SWITCH_PATH_SEPARATOR, confdir);
		} else {
			SWITCH_GLOBAL_dirs.conf_dir = strdup(confdir);
		}
#endif
	} else {
		SWITCH_GLOBAL_dirs.conf_dir = switch_mprintf("%s%sconf", basedir, SWITCH_PATH_SEPARATOR);
	}

	SWITCH_GLOBAL_dirs.log_dir = switch_mprintf("%s%s%lu%s", basedir, SWITCH_PATH_SEPARATOR, pid, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.run_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.recordings_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.sounds_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.cache_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.db_dir = switch_mprintf("%s%s%lu%s", basedir, SWITCH_PATH_SEPARATOR, pid, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.script_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.htdocs_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.grammar_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.fonts_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.images_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.storage_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.data_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);
	SWITCH_GLOBAL_dirs.localstate_dir = switch_mprintf("%s%s", basedir, SWITCH_PATH_SEPARATOR);

	switch_core_set_globals();

	if (!minimal) {
		status = switch_core_init_and_modload(flags, SWITCH_TRUE, &err);
		switch_sleep(1 * 1000000);
		switch_core_set_variable("sound_prefix", "." SWITCH_PATH_SEPARATOR);
		if (status != SWITCH_STATUS_SUCCESS && err) {
			fprintf(stderr, "%s", err);
		}
		return status;
	}
	status = switch_core_init(SCF_MINIMAL, SWITCH_TRUE, &err);
	if (status != SWITCH_STATUS_SUCCESS && err) {
		fprintf(stderr, "%s", err);
	}
	return status;
}

/**
 * Park FreeSWITCH session.  This is handy when wanting to use switch_core_session_execute_async() on the test session.
 * @param session to park
 */
#define fst_session_park(session) \
	switch_ivr_park_session(session); \
	switch_channel_wait_for_flag(switch_core_session_get_channel(session), CF_PARK, SWITCH_TRUE, 10000, NULL);

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
 * test string for equality - continue test execution on failure
 */
#define fst_check_string_equals fct_chk_eq_str

/**
 * test string for inequality - continue test execution on failure
 */
#define fst_check_string_not_equals fct_chk_neq_str

/**
 * Test string for matching prefix
*/
#define fst_check_string_starts_with fct_chk_startswith_str

/**
 * Test string for matching suffix
*/
#define fst_check_string_ends_with fct_chk_endswith_str

/**
 * Test string for substring
 */
#define fst_check_string_has fct_chk_incl_str

/**
 * Test string for exclusion of substring
 */
#define fst_check_string_does_not_have fct_chk_excl_str

/**
 * Mark reference for time measure
 */
#define fst_time_mark() \
	fst_time_start = switch_time_now();

/**
 * Check a test /w error message
 */
#define fst_xcheck(expr, error_msg) \
	fct_xchk(expr, "%s", error_msg);

/**
 * Fail a test
 */
#define fst_fail(error_msg) \
	fct_xchk(0, "%s", error_msg);

/**
 * Check duration relative to test start, last marked time, or last check.
 */
#define fst_check_duration(duration_ms, precision_ms) \
	{ \
		int actual_duration_ms = (int)((switch_time_now() - fst_time_start) / 1000); \
		fct_xchk( \
			abs((actual_duration_ms - duration_ms)) <= precision_ms, \
			"fst_check_duration: %d != %d +/- %d", \
			(actual_duration_ms), \
			(duration_ms), \
			(precision_ms) \
		); \
	}

/**
 * Check if integer is in range
 */
#define fst_check_int_range(actual, expected, precision) \
	fct_xchk( \
		abs((actual - expected)) <= precision, \
		"fst_check_int_range: %d != %d +/- %d", \
		(actual), \
		(expected), \
		(precision) \
	);

/**
 * Check if double-precision number is in range
 */
#define fst_check_double_range(actual, expected, precision) \
	fct_xchk( \
		fabs((actual - expected)) <= precision, \
		"fst_check_double_range: %f != %f +/- %f", \
		(actual), \
		(expected), \
		(precision) \
	);

/**
 * Run test without loading FS core
 */
#define FST_BEGIN() \
	FCT_BGN() \
	{ \
		int fst_core = 0; \
		switch_time_t fst_time_start = 0; \
		switch_timer_t fst_timer = { 0 }; \
		switch_memory_pool_t *fst_pool = NULL; \
		int fst_timer_started = 0; \
		fst_getenv_default("FST_SUPPRESS_UNUSED_STATIC_WARNING", NULL, SWITCH_FALSE); \
		if (fst_core) { \
			fst_init_core_and_modload(NULL, NULL, 0, 0); /* shuts up compiler */ \
		} \
		{ \


#define FST_END() \
		} \
		if (fst_time_start) { \
			/* shut up compiler */ \
			fst_time_start = 0; \
		} \
	} \
	FCT_END()

/**
 * Define the beginning of a freeswitch core test driver.  Only one per test application allowed.
 * @param confdir directory containing freeswitch.xml configuration
 */
#define FST_CORE_EX_BEGIN(confdir, flags) \
	FCT_BGN() \
	{ \
		int fst_core = 0; \
		switch_time_t fst_time_start = 0; \
		switch_timer_t fst_timer = { 0 }; \
		switch_memory_pool_t *fst_pool = NULL; \
		int fst_timer_started = 0; \
		fst_getenv_default("FST_SUPPRESS_UNUSED_STATIC_WARNING", NULL, SWITCH_FALSE); \
		if (fst_init_core_and_modload(confdir, confdir, 0, flags | SCF_LOG_DISABLE) == SWITCH_STATUS_SUCCESS) { \
			fst_core = 2; \
		} else { \
			fprintf(stderr, "Failed to load FS core\n"); \
			exit(1); \
		} \
		{


/**
 * Define the end of a freeswitch core test driver.
 */
#define FST_CORE_END() \
		switch_core_destroy(); \
		} \
		if (fst_time_start) { \
			/* shut up compiler */ \
			fst_time_start = 0; \
		} \
	} \
	FCT_END()

#define FST_CORE_BEGIN(confdir) FST_CORE_EX_BEGIN(confdir, 0)
#define FST_CORE_DB_BEGIN(confdir) FST_CORE_EX_BEGIN(confdir, SCF_USE_SQL)

/**
 * Minimal FS core load
 */
#define FST_MINCORE_BEGIN(confdir) \
	FCT_BGN() \
	{ \
		int fst_core = 0; \
		switch_time_t fst_time_start = 0; \
		switch_timer_t fst_timer = { 0 }; \
		switch_memory_pool_t *fst_pool = NULL; \
		int fst_timer_started = 0; \
		fst_getenv_default("FST_SUPPRESS_UNUSED_STATIC_WARNING", NULL, SWITCH_FALSE); \
		if (fst_init_core_and_modload(confdir, NULL, 1, 0 | SCF_LOG_DISABLE) == SWITCH_STATUS_SUCCESS) { /* minimal load */ \
			fst_core = 1; \
		} else { \
			fprintf(stderr, "Failed to load FS core\n"); \
			exit(1); \
		} \
		{

#define FST_MINCORE_END FST_CORE_END

/**
 * Define the beginning of a FreeSWITCH module test suite.  Loads the module for test.
 * @param modname name of module to load.
 * @param suite the name of this test suite
 */
#ifdef WIN32
#define FST_MODULE_BEGIN(modname,suite) \
	{ \
		const char *fst_test_module = #modname; \
		if (fst_core && !zstr(fst_test_module)) { \
			const char *err; \
			switch_loadable_module_load_module((char *)"./mod", (char *)fst_test_module, SWITCH_TRUE, &err); \
		} \
		FCT_FIXTURE_SUITE_BGN(suite);
#else
#define FST_MODULE_BEGIN(modname,suite) \
	{ \
		const char *fst_test_module = #modname; \
		if (fst_core && !zstr(fst_test_module)) { \
			const char *err; \
			char path[1024]; \
			sprintf(path, "%s%s%s", SWITCH_TEST_BASE_DIR_OVERRIDE, SWITCH_PATH_SEPARATOR, "../.libs/"); \
			switch_loadable_module_load_module((char *)path, (char *)fst_test_module, SWITCH_TRUE, &err); \
		} \
		FCT_FIXTURE_SUITE_BGN(suite);
#endif

/**
 * Define the end of a FreeSWITCH module test suite.
 */
#ifdef WIN32
#define FST_MODULE_END() \
		FCT_FIXTURE_SUITE_END(); \
		if (!zstr(fst_test_module) && switch_loadable_module_exists(fst_test_module) == SWITCH_STATUS_SUCCESS) { \
			const char *err; \
			switch_loadable_module_unload_module((char *)"./mod", (char *)fst_test_module, SWITCH_FALSE, &err); \
		} \
	}
#else
#define FST_MODULE_END() \
		FCT_FIXTURE_SUITE_END(); \
		if (!zstr(fst_test_module) && switch_loadable_module_exists(fst_test_module) == SWITCH_STATUS_SUCCESS) { \
			const char *err; \
			char path[1024]; \
			sprintf(path, "%s%s%s", SWITCH_TEST_BASE_DIR_OVERRIDE, SWITCH_PATH_SEPARATOR, "../.libs/"); \
			switch_loadable_module_unload_module((char*)path, (char *)fst_test_module, SWITCH_FALSE, &err); \
		} \
	}
#endif

/**
 * Define the beginning of a test suite not associated with a module. 
 * @param suite the name of this test suite
 */
#define FST_SUITE_BEGIN(suite) \
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
		if (fst_core) { \
			switch_core_new_memory_pool(&fst_pool); \
			if (fst_core > 1) { \
				fst_timer_started = (switch_core_timer_init(&fst_timer, "soft", 20, 160, fst_pool) == SWITCH_STATUS_SUCCESS); \
			} \
		}

/**
 * Define the end of test suite setup.
 */
#define FST_SETUP_END FCT_SETUP_END


/**
 * Define the test suite teardown.  This is run after each test or session test.
 */
#define FST_TEARDOWN_BEGIN() \
	FCT_TEARDOWN_BGN() \
		if (fst_core) { \
			if (fst_pool) switch_core_destroy_memory_pool(&fst_pool); \
			if (fst_core > 1) { \
				if (fst_timer_started) switch_core_timer_destroy(&fst_timer); \
			} \
		}

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
		if (fst_core) { \
			switch_log_level_t level = SWITCH_LOG_DEBUG; \
			switch_core_session_ctl(SCSC_LOGLEVEL, &level); \
			fst_requires(fst_pool != NULL); \
			if (fst_core > 1) { \
				fst_requires(fst_timer_started); \
			} \
			fst_time_mark(); \
		} \
		if (fst_test_module) { \
			fst_requires_module(fst_test_module); \
		}

#define FST_TEST_END \
	if (fst_core) { \
		switch_log_level_t level = SWITCH_LOG_DISABLE; \
		switch_core_session_ctl(SCSC_LOGLEVEL, &level); \
	} \
	FCT_TEST_END


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
 * @param rate the rate of the channel
 */

#define FST_SESSION_BEGIN_RATE(name, rate) \
	FCT_TEST_BGN(name) \
	{ \
		if (fst_core) { \
			switch_log_level_t level = SWITCH_LOG_DEBUG; \
			switch_core_session_ctl(SCSC_LOGLEVEL, &level); \
			fst_requires(fst_pool != NULL); \
			if (fst_core > 1) { \
				fst_requires(fst_timer_started); \
			} \
			fst_time_mark(); \
		} \
	} \
	{ \
		switch_core_session_t *fst_session = NULL; \
		switch_event_t *fst_originate_vars = NULL; \
		switch_call_cause_t fst_cause = SWITCH_CAUSE_NORMAL_CLEARING; \
		fst_requires(fst_core); \
		if (fst_test_module) { \
			fst_requires_module(fst_test_module); \
		} \
		fst_requires_module("mod_loopback"); \
		fst_requires_module("mod_sndfile"); \
		fst_requires(switch_core_running()); \
		fst_requires(switch_event_create_plain(&fst_originate_vars, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS); \
		switch_event_add_header_string(fst_originate_vars, SWITCH_STACK_BOTTOM, "origination_caller_id_number", "+15551112222"); \
		switch_event_add_header(fst_originate_vars, SWITCH_STACK_BOTTOM, "rate", "%d", rate); \
		if (switch_ivr_originate(NULL, &fst_session, &fst_cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, fst_originate_vars, SOF_NONE, NULL, NULL) == SWITCH_STATUS_SUCCESS && fst_session) { \
			switch_memory_pool_t *fst_session_pool = switch_core_session_get_pool(fst_session); \
			switch_channel_t *fst_channel = switch_core_session_get_channel(fst_session); \
			switch_channel_set_state(fst_channel, CS_SOFT_EXECUTE); \
			switch_channel_wait_for_state(fst_channel, NULL, CS_SOFT_EXECUTE); \
			switch_channel_set_variable(fst_channel, "send_silence_when_idle", "-1"); \
			switch_channel_set_variable(fst_channel, "RECORD_STEREO", "true"); \
			switch_ivr_record_session(fst_session, (char *)"/tmp/"#name".wav", 0, NULL); \
			for(;;) {

/**
 * Define a session test in a test suite.  This can be used to test IVR functions.
 * See FST_SESSION_BEGIN_RATE
 */

#define FST_SESSION_BEGIN(name) FST_SESSION_BEGIN_RATE(name, 8000)

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
			if (fst_session_pool) { \
				fst_session_pool = NULL; \
			} \
			switch_core_session_rwunlock(fst_session); \
			if (fst_core) { \
				switch_log_level_t level = SWITCH_LOG_DISABLE; \
				switch_core_session_ctl(SCSC_LOGLEVEL, &level); \
			} \
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
	fst_requires(fst_core > 1); \
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
	/* feed file into ASR */ \
	switch_status_t result; \
	switch_file_handle_t file_handle = { 0 }; \
	uint8_t *buf; \
	size_t len = 160; \
	int got_result = 0; \
	fst_asr_result = NULL; \
	file_handle.channels = 1; \
	file_handle.native_rate = 8000; \
	fst_requires(fst_core > 1); \
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test recognizer: input = %s\n", input_filename); \
	fst_requires(switch_core_asr_load_grammar(&ah, grammar, "") == SWITCH_STATUS_SUCCESS); \
	fst_requires(switch_core_file_open(&file_handle, input_filename, file_handle.channels, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS); \
	buf = (uint8_t *)switch_core_alloc(fst_pool, sizeof(uint8_t) * 160 * sizeof(uint16_t) * file_handle.channels); \
	switch_core_timer_sync(&fst_timer); \
	while ((result = switch_core_file_read(&file_handle, buf, &len)) == SWITCH_STATUS_SUCCESS) { \
		fst_requires(switch_core_asr_feed(&ah, buf, len * sizeof(int16_t), &flags) == SWITCH_STATUS_SUCCESS); \
		switch_core_timer_next(&fst_timer); \
		if (switch_core_asr_check_results(&ah, &flags) == SWITCH_STATUS_SUCCESS) { \
			char *xmlstr = NULL; \
			switch_event_t *headers = NULL; \
			flags = SWITCH_ASR_FLAG_NONE; \
			/* switch_ivr_detect_speech.. checks one in media bug then again in speech_thread  */ \
			fst_requires(switch_core_asr_check_results(&ah, &flags) == SWITCH_STATUS_SUCCESS); \
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
	fst_requires(fst_core > 1); \
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
	fst_requires(fst_core > 1); \
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
	fst_requires(fst_core > 1); \
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
	const char *fst_asr_result = NULL; \
	fst_requires(fst_core > 1);

/**
 * Use play_and_detect_speech APP to test recognizer
 *
 * Test Requires:
 *   switch_ivr_displace_session(input_filename) == SWITCH_STATUS_SUCCESS
 *   switch_core_session_execute_application(play_and_detect_speech) == SWITCH_STATUS_SUCCESS
 *   mod_dptools is loaded
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
#define fst_play_and_detect_speech_app_test(recognizer, grammar, prompt_filename, input_filename) \
{ \
	char *args = NULL; \
	fst_requires(fst_core > 1); \
	fst_requires_module("mod_dptools"); \
	switch_channel_set_variable(fst_channel, "detect_speech_result", ""); \
	fst_requires(switch_ivr_displace_session(fst_session, input_filename, 0, "mrf") == SWITCH_STATUS_SUCCESS); \
	args = switch_core_session_sprintf(fst_session, "%s detect:%s %s", prompt_filename, recognizer, grammar); \
	fst_requires(switch_core_session_execute_application(fst_session, "play_and_detect_speech", args) == SWITCH_STATUS_SUCCESS); \
	fst_asr_result = switch_channel_get_variable(fst_channel, "detect_speech_result"); \
	fst_check(fst_asr_result != NULL); \
}

/**
 * Use play_and_detect_speech core function to test recognizer
 *
 * Test Requires:
 *   switch_ivr_displace_session(input_filename) == SWITCH_STATUS_SUCCESS
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
 * @param input_args input callback args
 */
#define fst_play_and_detect_speech_test(recognizer, grammar, prompt_filename, input_filename, input_args) \
{ \
	char *args = NULL; \
	fst_asr_result = NULL; \
	fst_requires(fst_core > 1); \
	fst_requires(switch_ivr_displace_session(fst_session, input_filename, 0, "mrf") == SWITCH_STATUS_SUCCESS); \
	switch_status_t status = switch_ivr_play_and_detect_speech(fst_session, prompt_filename, recognizer, grammar, (char **)&fst_asr_result, 0, input_args); \
	fst_check(fst_asr_result != NULL); \
}

/**
 * Define end of play_and_detect_speech recognizer test
 */
#define fst_play_and_detect_speech_test_end() \
}


/**
 * Compare extension dialplan apps and args with expected apps and args
 * @param expected NULL terminated string array of app arg and names. 
 *     const char *expected[] = { "playback", "https://example.com/foo.wav", "park", "", NULL };
 * @param extension the switch_caller_extension_t to check
 */
#define fst_check_extension_apps(expected, extension) \
	{ \
		fst_xcheck(extension != NULL, "Missing extension\n"); \
		if (extension) { \
			int i; \
			switch_caller_application_t *cur_app = extension->applications; \
			for (i = 0; ; i += 2, cur_app = cur_app->next) { \
				int cur_app_num = i / 2 + 1; \
				if (!expected[i]) { \
					if (cur_app != NULL) { \
						fst_fail(switch_core_sprintf(fst_pool, "Unexpected application #%d \"%s\"\n", cur_app_num, cur_app->application_name)); \
					} \
					break; \
				} \
				fst_xcheck(cur_app != NULL, switch_core_sprintf(fst_pool, "Extension application #%d \"%s\" is missing", cur_app_num, expected[i])); \
				if (!cur_app) { \
					break; \
				} \
				fst_xcheck(cur_app->application_name && !strcmp(expected[i], cur_app->application_name), switch_core_sprintf(fst_pool, "Expected application #%d name is \"%s\", but is \"%s\"\n", cur_app_num, expected[i], cur_app->application_name)); \
				fst_xcheck(cur_app->application_data && !strcmp(expected[i + 1], cur_app->application_data), switch_core_sprintf(fst_pool, "Expected application #%d %s data is \"%s\", but is \"%s\"\n", cur_app_num, expected[i], expected[i + 1], cur_app->application_data)); \
			} \
		} \
	}


/**
 * Inject DTMF into the session to be detected.
 *
 * Test Requires:
 *   switch_api_execute(sched_api) == SWITCH_STATUS_SUCCESS
 *   mod_commands is loaded
 *
 * @param when string describing when to send dtmf
 * @param digits to send
 */
#define fst_sched_recv_dtmf(when, digits) \
{ \
	switch_status_t api_result; \
	switch_stream_handle_t stream = { 0 }; \
	SWITCH_STANDARD_STREAM(stream); \
	fst_requires(fst_core > 1); \
	fst_requires_module("mod_commands"); \
	api_result = switch_api_execute("sched_api", switch_core_session_sprintf(fst_session, "%s none uuid_recv_dtmf %s %s", when, switch_core_session_get_uuid(fst_session), digits), NULL, &stream); \
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_INFO, "Injecting DTMF %s at %s\n", digits, when); \
	fst_requires(api_result == SWITCH_STATUS_SUCCESS); \
	switch_safe_free(stream.data); \
}

#define fst_xml_start() \
	switch_stream_handle_t fst_xml_stream = { 0 }; \
	SWITCH_STANDARD_STREAM(fst_xml_stream);
	int fst_tag_children = 0;
	int fst_tag_body = 0;

#define fst_xml_open_tag(tag_name) \
	fst_xml_stream.write_function(&fst_xml_stream, "<%s", #tag_name); \
	fst_tag_children++;

#define fst_xml_attr(attr) \
	if (!zstr(attr)) fst_xml_stream.write_function(&fst_xml_stream, " %s=\"%s\"", #attr, attr);

#define fst_xml_close_tag(tag_name) \
	--fst_tag_children; \
	if (fst_tag_children > 0 || fst_tag_body) { \
		fst_xml_stream.write_function(&fst_xml_stream, "</%s>", #tag_name); \
	} else { \
		fst_xml_stream.write_function(&fst_xml_stream, "/>"); \
	} \
	fst_tag_body = 0;

#define fst_xml_body(body) \
	if (fst_tag_body) { \
		fst_xml_stream.write_function(&fst_xml_stream, "%s", body); \
	} else { \
		fst_tag_body = 1; \
		fst_xml_stream.write_function(&fst_xml_stream, ">%s", body); \
	}

#define fst_xml_end() \
	switch_xml_parse_str_dynamic((char *)fst_xml_stream.data, SWITCH_FALSE);


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
	switch_zmalloc(buf, s.st_size + 1); \
	fst_requires(buf); \
	size = read(fd, buf, s.st_size); \
	fst_requires(size == s.st_size); \
	close(fd); \
	varname = cJSON_Parse(buf); \
	free(buf); \
	fst_requires(varname); \
}

#endif
