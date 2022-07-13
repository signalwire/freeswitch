/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2021, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_log.c -- tests core logging
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

switch_memory_pool_t *pool = NULL;
static switch_mutex_t *mutex = NULL;
switch_thread_cond_t *cond = NULL;
static cJSON *last_alert_log = NULL;

static switch_log_json_format_t json_format = {
	{ NULL, NULL }, // version
	{ NULL, NULL }, // host
	{ NULL, NULL }, // timestamp
	{ "level", NULL }, // level
	{ NULL, NULL }, // ident
	{ NULL, NULL }, // pid
	{ NULL, NULL }, // uuid
	{ NULL, NULL }, // file
	{ NULL, NULL }, // line
	{ NULL, NULL }, // function
	{ "message", NULL }, // full_message
	{ NULL, NULL }, // short_message
	NULL, // custom_field_prefix
	0.0, // timestamp_divisor
	{ NULL, NULL } // sequence
};

static switch_status_t test_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	switch_mutex_lock(mutex);
	if (level == SWITCH_LOG_ALERT && !last_alert_log && node->content && strstr(node->content, "switch_log test: ")) {
		last_alert_log = switch_log_node_to_json(node, level, &json_format, NULL);
		switch_thread_cond_signal(cond);
	}
	switch_mutex_unlock(mutex);
	return SWITCH_STATUS_SUCCESS;
}

static char *wait_for_log(switch_interval_time_t timeout_ms)
{
	char *log_str = NULL;
	cJSON *log = NULL;
	switch_time_t now = switch_time_now();
	switch_time_t expiration = now + (timeout_ms * 1000);
	switch_mutex_lock(mutex);
	while (!last_alert_log && (now = switch_time_now()) < expiration) {
		switch_interval_time_t timeout = expiration - now;
		switch_thread_cond_timedwait(cond, mutex, timeout);
	}
	log = last_alert_log;
	last_alert_log = NULL;
	switch_mutex_unlock(mutex);
	if (log) {
		log_str = cJSON_PrintUnformatted(log);
		cJSON_Delete(log);
	}
	return log_str;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_log)
	{
		switch_core_new_memory_pool(&pool);
		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, pool);
		switch_thread_cond_create(&cond, pool);

		FST_SETUP_BEGIN()
		{
			json_format.custom_field_prefix = NULL;
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_SESSION_BEGIN(switch_log_meta_printf)
		{
			cJSON *item = NULL;
			cJSON *meta = NULL;
			char *log = NULL;

			switch_log_bind_logger(test_logger, SWITCH_LOG_ALERT, SWITCH_FALSE);

			switch_log_meta_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, NULL, "switch_log test: Plain channel log %d\n", 0);
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"level\":1,\"message\":\"switch_log test: Plain channel log 0\\n\"}");
			switch_safe_free(log);

			switch_log_meta_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_ALERT, NULL, "switch_log test: Plain session log %d\n", 1);
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"level\":1,\"message\":\"switch_log test: Plain session log 1\\n\"}");
			switch_safe_free(log);

			switch_log_meta_printf(SWITCH_CHANNEL_UUID_LOG(switch_core_session_get_uuid(fst_session)), SWITCH_LOG_ALERT, NULL, "switch_log test: Plain uuid log %d\n", 2);
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"level\":1,\"message\":\"switch_log test: Plain uuid log 2\\n\"}");
			switch_safe_free(log);

			meta = cJSON_CreateObject();
			cJSON_AddStringToObject(meta, "foo", "bar");
			cJSON_AddNumberToObject(meta, "measure", 3.14159);
			switch_log_meta_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, &meta, "switch_log test: channel log with metadata %d\n", 3);
			fst_xcheck(meta == NULL, "Expect logging meta data to be consumed by switch_log_meta_printf()");
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"foo\":\"bar\",\"measure\":3.14159,\"level\":1,\"message\":\"switch_log test: channel log with metadata 3\\n\"}");
			switch_safe_free(log);

			meta = cJSON_CreateObject();
			cJSON_AddStringToObject(meta, "foo", "bar");
			cJSON_AddNumberToObject(meta, "measure", 3.14159);
			switch_log_meta_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_ALERT, &meta, "switch_log test: Session log with metadata %d\n", 4);
			fst_xcheck(meta == NULL, "Expect logging meta data to be consumed by switch_log_meta_printf()");
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"foo\":\"bar\",\"measure\":3.14159,\"level\":1,\"message\":\"switch_log test: Session log with metadata 4\\n\"}");
			switch_safe_free(log);

			meta = cJSON_CreateObject();
			cJSON_AddStringToObject(meta, "foo", "bar");
			cJSON_AddNumberToObject(meta, "measure", 3.14159);
			switch_log_meta_printf(SWITCH_CHANNEL_UUID_LOG(switch_core_session_get_uuid(fst_session)), SWITCH_LOG_ALERT, &meta, "switch_log test: uuid log with metadata %d\n", 5);
			fst_xcheck(meta == NULL, "Expect logging meta data to be consumed by switch_log_meta_printf()");
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"foo\":\"bar\",\"measure\":3.14159,\"level\":1,\"message\":\"switch_log test: uuid log with metadata 5\\n\"}");
			switch_safe_free(log);

			meta = cJSON_CreateObject();
			item = cJSON_AddObjectToObject(meta, "nested");
			cJSON_AddStringToObject(item, "stringval", "1234");
			item = cJSON_AddArrayToObject(item, "array");
			cJSON_AddItemToArray(item, cJSON_CreateString("12"));
			item = cJSON_AddArrayToObject(meta, "array2");
			cJSON_AddItemToArray(item, cJSON_CreateString("34"));
			switch_log_meta_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_ALERT, &meta, "switch_log test: session log with complex metadata %d\n", 6);
			fst_xcheck(meta == NULL, "Expect logging meta data to be consumed by switch_log_meta_printf()");
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"nested\":{\"stringval\":\"1234\",\"array\":[\"12\"]},\"array2\":[\"34\"],\"level\":1,\"message\":\"switch_log test: session log with complex metadata 6\\n\"}");
			switch_safe_free(log);

			meta = cJSON_CreateObject();
			item = cJSON_AddObjectToObject(meta, "nested");
			cJSON_AddStringToObject(item, "stringval", "1234");
			item = cJSON_AddArrayToObject(item, "array");
			cJSON_AddItemToArray(item, cJSON_CreateString("12"));
			item = cJSON_AddArrayToObject(meta, "array2");
			cJSON_AddItemToArray(item, cJSON_CreateString("34"));
			json_format.custom_field_prefix = "prefix.";
			switch_log_meta_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_ALERT, &meta, "switch_log test: session log with prefixed complex metadata %d\n", 7);
			fst_xcheck(meta == NULL, "Expect logging meta data to be consumed by switch_log_meta_printf()");
			log = wait_for_log(1000);
			fst_check_string_equals(log, "{\"prefix.nested\":{\"stringval\":\"1234\",\"array\":[\"12\"]},\"prefix.array2\":[\"34\"],\"level\":1,\"message\":\"switch_log test: session log with prefixed complex metadata 7\\n\"}");
			switch_safe_free(log);

			cJSON_Delete(last_alert_log);
			last_alert_log = NULL;
			switch_log_unbind_logger(test_logger);
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()

	switch_core_destroy_memory_pool(&pool);
}
FST_CORE_END()
