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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * test_mod_lua -- mod_lua test
 *
 */

#include <test/switch_test.h>

#define CONCURRENT_RUN_THREADS 32

static switch_mutex_t *concurrent_run_mutex = NULL;
static switch_thread_cond_t *concurrent_run_cond = NULL;
static int concurrent_run_ready = 0;
static int concurrent_run_go = 0;

static void *SWITCH_THREAD_FUNC concurrent_run_thread(switch_thread_t *thread, void *obj)
{
	int *ok = (int *)obj;
	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);

	/* Block until every thread is up, so that one broadcast sends them all into lua_init() together */
	switch_mutex_lock(concurrent_run_mutex);
	concurrent_run_ready++;
	switch_thread_cond_broadcast(concurrent_run_cond);
	while (!concurrent_run_go) {
		switch_thread_cond_wait(concurrent_run_cond, concurrent_run_mutex);
	}
	switch_mutex_unlock(concurrent_run_mutex);

	switch_api_execute("lua", "~stream:write('+OK')", NULL, &stream);
	*ok = stream.data && !strcmp((char *)stream.data, "+OK");
	switch_safe_free(stream.data);

	return NULL;
}

FST_CORE_BEGIN("conf")
{
	FST_MODULE_BEGIN(mod_lua, mod_lua_test)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_lua");
		}
		FST_SETUP_END()

		/* Keep this test first: only the very first luaopen_freeswitch() in the process can race on SWIG's type tables */
		FST_TEST_BEGIN(concurrent_first_run_test)
		{
			switch_thread_t *thread[CONCURRENT_RUN_THREADS] = { 0 };
			int ok[CONCURRENT_RUN_THREADS] = { 0 };
			switch_threadattr_t *thd_attr = NULL;
			switch_status_t status;
			int i, started = 0;

			switch_mutex_init(&concurrent_run_mutex, SWITCH_MUTEX_DEFAULT, fst_pool);
			switch_thread_cond_create(&concurrent_run_cond, fst_pool);
			switch_threadattr_create(&thd_attr, fst_pool);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

			for (i = 0; i < CONCURRENT_RUN_THREADS; i++) {
				if (switch_thread_create(&thread[started], thd_attr, concurrent_run_thread, &ok[started], fst_pool) == SWITCH_STATUS_SUCCESS) {
					started++;
				}
			}

			fst_check_int_equals(started, CONCURRENT_RUN_THREADS);

			/* Only wait for the threads that did start, so a failed switch_thread_create() fails the test instead of hanging it */
			switch_mutex_lock(concurrent_run_mutex);
			while (concurrent_run_ready < started) {
				switch_thread_cond_wait(concurrent_run_cond, concurrent_run_mutex);
			}
			concurrent_run_go = 1;
			switch_thread_cond_broadcast(concurrent_run_cond);
			switch_mutex_unlock(concurrent_run_mutex);

			for (i = 0; i < started; i++) {
				switch_thread_join(&status, thread[i]);
				fst_check(ok[i]);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(json_test)
		{
			switch_stream_handle_t stream = { 0 };

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("lua", "test_json.lua", NULL, &stream);

			if (stream.data) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "LUA DATA: %s\n", (char *)stream.data);
				fst_check(strstr(stream.data, "+OK") == stream.data);
				free(stream.data);
			}
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()
