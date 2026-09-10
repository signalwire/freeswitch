/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 *
 * test_mod_logfile.c -- End-to-end tests for structured logfile prefixes
 *
 */

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <test/switch_test.h>

static char *read_test_log(void)
{
	char *path;
	FILE *file;
	char *data = NULL;
	long length;

	path = switch_mprintf("%s%sfreeswitch.log", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	if (!path) return NULL;
	file = fopen(path, "rb");
	free(path);
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	data = malloc((size_t) length + 1);
	if (!data) {
		fclose(file);
		return NULL;
	}
	if (length && fread(data, 1, (size_t) length, file) != (size_t) length) {
		free(data);
		fclose(file);
		return NULL;
	}
	data[length] = '\0';
	fclose(file);
	return data;
}

static char *wait_for_test_log(const char *marker)
{
	int attempt;

	for (attempt = 0; attempt < 100; attempt++) {
		char *data = read_test_log();
		if (data && strstr(data, marker)) return data;
		switch_safe_free(data);
		switch_yield(10000);
	}
	return NULL;
}

FST_CORE_BEGIN("./conf_logfile")
{
	FST_SUITE_BEGIN(mod_logfile)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_SESSION_BEGIN(writes_structured_prefixes)
		{
			const char *uuid = switch_core_session_get_uuid(fst_session);
			char *expected = NULL;
			char *marker = NULL;
			char *long_payload = NULL;
			char *contents = NULL;
			char *marker_position = NULL;
			char *first_line = NULL;
			char *second_line = NULL;
			switch_size_t marker_length = 0;
			switch_size_t expected_length = 0;

			fst_check(uuid != NULL);
			if (uuid) {
				expected = switch_mprintf("%s tenant:acme callid:call-42 ", uuid);
				marker = switch_mprintf("mod_logfile integration marker %s", uuid);
				if (expected && marker) {
					marker_length = strlen(marker);
					expected_length = strlen(expected);
					long_payload = malloc(6001);
					if (long_payload && marker_length <= 6000) {
						memcpy(long_payload, marker, marker_length);
						memset(long_payload + marker_length, 'x', 6000 - marker_length);
						long_payload[6000] = '\0';
					}
				}
			}
			fst_check(expected != NULL);
			fst_check(marker != NULL);
			fst_check(long_payload != NULL);
			if (expected && marker && long_payload) {
				fst_requires_module("mod_dptools");
				fst_requires_module("mod_logfile");
				switch_core_session_execute_application(fst_session, "set_log_tag", "tenant=acme");
				switch_channel_set_variable(fst_channel, "sip_call_id", "call-42");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_INFO,
							  "%s\nsecond physical line\n", long_payload);

				contents = wait_for_test_log(marker);
				fst_check(contents != NULL);
				if (contents) {
					switch_size_t available;
					switch_size_t required;

					marker_position = strstr(contents, marker);
					fst_check(marker_position != NULL);
					if (marker_position) {
						first_line = marker_position;
						while (first_line > contents && first_line[-1] != '\n') first_line--;
						available = strlen(marker_position);
						required = 6001 + expected_length + 21;
						fst_check(strlen(first_line) >= expected_length);
						fst_check(available >= required);
						if (strlen(first_line) >= expected_length && available >= required) {
							fst_check(!strncmp(first_line, expected, expected_length));
							fst_check(!strncmp(marker_position, long_payload, 6000));
							fst_check(marker_position[6000] == '\n');
							second_line = marker_position + 6001;
							fst_check(!strncmp(second_line, expected, expected_length));
							fst_check(!strncmp(second_line + expected_length, "second physical line\n", 21));
						}
					}
				}
			}

			switch_safe_free(contents);
			free(long_payload);
			free(marker);
			free(expected);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(recovers_after_transient_reopen_failure)
		{
			switch_memory_pool_t *pool = switch_core_session_get_pool(fst_session);
			const char *uuid = switch_core_session_get_uuid(fst_session);
			switch_event_t *event = NULL;
			char *log_path = switch_mprintf("%s%sfreeswitch.log", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
			char *backup_path = switch_mprintf("%s.reopen-test", switch_str_nil(log_path));
			char *marker = switch_mprintf("mod_logfile reopen recovery marker %s", switch_str_nil(uuid));
			char *contents = NULL;
			switch_bool_t renamed = SWITCH_FALSE;
			switch_bool_t directory_created = SWITCH_FALSE;

			fst_check(pool != NULL);
			fst_check(uuid != NULL);
			fst_check(log_path != NULL);
			fst_check(backup_path != NULL);
			fst_check(marker != NULL);
			if (pool && uuid && log_path && backup_path && marker) {
				fst_check(switch_file_rename(log_path, backup_path, pool) == SWITCH_STATUS_SUCCESS);
				renamed = switch_file_exists(backup_path, pool) == SWITCH_STATUS_SUCCESS;
				if (renamed) {
					fst_check(switch_dir_make(log_path, SWITCH_FPROT_OS_DEFAULT, pool) == SWITCH_STATUS_SUCCESS);
					directory_created = switch_file_exists(log_path, pool) == SWITCH_STATUS_SUCCESS;
				}

				if (directory_created) {
					fst_check(switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS);
					if (event) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Trapped-Signal", "HUP");
						switch_event_deliver(&event);
					}
					if (remove(log_path) == 0) {
						directory_created = SWITCH_FALSE;
					} else {
						fst_check(0);
					}
					if (!directory_created) {
						if (switch_file_rename(backup_path, log_path, pool) == SWITCH_STATUS_SUCCESS) {
							renamed = SWITCH_FALSE;
						} else {
							fst_check(0);
						}
					}

					if (!renamed) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fst_session), SWITCH_LOG_INFO, "%s\n", marker);
						contents = wait_for_test_log(marker);
						fst_check(contents != NULL);
					}
				}
			}

			if (event) switch_event_destroy(&event);
			if (directory_created && log_path) remove(log_path);
			if (renamed && pool && log_path && backup_path) switch_file_rename(backup_path, log_path, pool);
			switch_safe_free(contents);
			free(marker);
			free(backup_path);
			free(log_path);
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
