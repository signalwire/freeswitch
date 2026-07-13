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
 * test_mod_logfile_prefix.c -- Tests for structured logfile prefix formatting
 *
 */

#include <switch.h>
#include <stdlib.h>
#include <test/switch_test.h>

#include "../../src/mod/loggers/mod_logfile/mod_logfile_prefix.h"

#define fst_check_string_equals_safe(actual, expected) do { \
	const char *fst_actual__ = (actual); \
	const char *fst_expected__ = (expected); \
	fst_check(fst_actual__ != NULL); \
	fst_check(fst_expected__ != NULL); \
	if (fst_actual__ && fst_expected__) { \
		fst_check_string_equals(fst_actual__, fst_expected__); \
	} \
} while (0)

static int event_header_count(const switch_event_t *event)
{
	const switch_event_header_t *header;
	int count = 0;

	for (header = event ? event->headers : NULL; header; header = header->next) {
		count++;
	}

	return count;
}

typedef struct {
	switch_status_t status;
	switch_size_t bytes;
} write_step_t;

typedef struct {
	write_step_t steps[4];
	int step_count;
	int write_calls;
	int reopen_calls;
	switch_status_t reopen_status;
	char sink[64];
	switch_size_t sink_length;
} write_script_t;

static switch_status_t scripted_write(void *context, const char *data, switch_size_t *length)
{
	write_script_t *script = context;
	write_step_t step;
	switch_size_t bytes;

	if (script->write_calls >= script->step_count) {
		*length = 0;
		return SWITCH_STATUS_FALSE;
	}

	step = script->steps[script->write_calls++];
	bytes = step.bytes < *length ? step.bytes : *length;
	memcpy(script->sink + script->sink_length, data, bytes);
	script->sink_length += bytes;
	script->sink[script->sink_length] = '\0';
	*length = bytes;
	return step.status;
}

static switch_status_t scripted_reopen(void *context)
{
	write_script_t *script = context;

	script->reopen_calls++;
	return script->reopen_status;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(mod_logfile_prefix)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
			mod_logfile_prefix_test_reset_failures();
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(complete_write_finishes_successful_short_writes)
		{
			write_script_t script = { { { SWITCH_STATUS_SUCCESS, 2 }, { SWITCH_STATUS_SUCCESS, 3 } }, 2, 0, 0,
				SWITCH_STATUS_SUCCESS, "", 0 };
			switch_size_t committed = 0;
			switch_status_t status;

			status = mod_logfile_complete_write(&script, "abcde", 5, scripted_write, scripted_reopen, &committed);
			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_int_equals(script.write_calls, 2);
			fst_check_int_equals(script.reopen_calls, 0);
			fst_check_int_equals((int) committed, 5);
			fst_check_string_equals(script.sink, "abcde");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(complete_write_recovers_partial_error_without_duplication)
		{
			write_script_t script = { { { SWITCH_STATUS_FALSE, 2 }, { SWITCH_STATUS_SUCCESS, 3 } }, 2, 0, 0,
				SWITCH_STATUS_SUCCESS, "", 0 };
			switch_size_t committed = 0;
			switch_status_t status;

			status = mod_logfile_complete_write(&script, "abcde", 5, scripted_write, scripted_reopen, &committed);
			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_int_equals(script.write_calls, 2);
			fst_check_int_equals(script.reopen_calls, 1);
			fst_check_int_equals((int) committed, 5);
			fst_check_string_equals(script.sink, "abcde");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(complete_write_stops_when_recovery_fails)
		{
			write_script_t script = { { { SWITCH_STATUS_SUCCESS, 0 } }, 1, 0, 0, SWITCH_STATUS_FALSE, "", 0 };
			switch_size_t committed = 99;
			switch_status_t status;

			status = mod_logfile_complete_write(&script, "abc", 3, scripted_write, scripted_reopen, &committed);
			fst_check_int_equals(status, SWITCH_STATUS_FALSE);
			fst_check_int_equals(script.write_calls, 1);
			fst_check_int_equals(script.reopen_calls, 1);
			fst_check_int_equals((int) committed, 0);
			fst_check_string_equals(script.sink, "");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(complete_write_stops_after_second_zero_progress)
		{
			write_script_t script = { { { SWITCH_STATUS_SUCCESS, 0 }, { SWITCH_STATUS_SUCCESS, 0 } }, 2, 0, 0,
				SWITCH_STATUS_SUCCESS, "", 0 };
			switch_size_t committed = 99;
			switch_status_t status;

			status = mod_logfile_complete_write(&script, "abc", 3, scripted_write, scripted_reopen, &committed);
			fst_check_int_equals(status, SWITCH_STATUS_FALSE);
			fst_check_int_equals(script.write_calls, 2);
			fst_check_int_equals(script.reopen_calls, 1);
			fst_check_int_equals((int) committed, 0);
			fst_check_string_equals(script.sink, "");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_initial_allocation_failure_returns_null)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			char *prefix;

			mod_logfile_prefix_config_init(&config);
			config.log_uuid = SWITCH_TRUE;
			node.userdata = "uuid-123";
			mod_logfile_prefix_test_fail_allocation_after(0);
			prefix = mod_logfile_prefix_build(&config, &node);
			mod_logfile_prefix_test_reset_failures();

			fst_check(prefix == NULL);
			switch_safe_free(prefix);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_mid_growth_failure_discards_fragment)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			char long_value[201];
			char *prefix;

			memset(long_value, 'v', sizeof(long_value) - 1);
			long_value[sizeof(long_value) - 1] = '\0';
			mod_logfile_prefix_config_init(&config);
			config.log_uuid = SWITCH_TRUE;
			config.log_tags = SWITCH_TRUE;
			node.userdata = "uuid-123";
			fst_check_int_equals(switch_event_create_plain(&node.tags, SWITCH_EVENT_CHANNEL_DATA), SWITCH_STATUS_SUCCESS);
			if (node.tags) {
				switch_event_add_header_string(node.tags, SWITCH_STACK_BOTTOM, "tenant", long_value);
			}
			mod_logfile_prefix_test_fail_allocation_after(1);
			prefix = mod_logfile_prefix_build(&config, &node);
			mod_logfile_prefix_test_reset_failures();

			fst_check(prefix == NULL);
			switch_safe_free(prefix);
			switch_event_destroy(&node.tags);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(multiline_mid_growth_failure_discards_fragment)
		{
			char data[260];
			char *formatted;

			memset(data, 'a', 60);
			data[60] = '\n';
			memset(data + 61, 'b', sizeof(data) - 62);
			data[sizeof(data) - 1] = '\0';
			mod_logfile_prefix_test_fail_allocation_after(1);
			formatted = mod_logfile_prefix_lines("tag:value ", data);
			mod_logfile_prefix_test_reset_failures();

			fst_check(formatted == NULL);
			switch_safe_free(formatted);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(channel_var_parser_supports_shorthand_aliases_and_replacement)
		{
			mod_logfile_prefix_config_t config;

			mod_logfile_prefix_config_init(&config);
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config,
				" shorthand , tenant = account_id , =missing, empty= , duplicate = first "), SWITCH_STATUS_SUCCESS);
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config, " duplicate = second "), SWITCH_STATUS_SUCCESS);
			fst_check(config.channel_vars != NULL);
			if (config.channel_vars) {
				fst_check_string_equals_safe(switch_event_get_header(config.channel_vars, "shorthand"), "shorthand");
				fst_check_string_equals_safe(switch_event_get_header(config.channel_vars, "tenant"), "account_id");
				fst_check_string_equals_safe(switch_event_get_header(config.channel_vars, "duplicate"), "second");
				fst_check(switch_event_get_header(config.channel_vars, "empty") == NULL);
				fst_check_int_equals(event_header_count(config.channel_vars), 3);
			}
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config, " =missing, empty= "), SWITCH_STATUS_FALSE);

			mod_logfile_prefix_config_destroy(&config);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(tags_are_ordered_and_sanitized)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			char *prefix;

			mod_logfile_prefix_config_init(&config);
			config.log_uuid = SWITCH_TRUE;
			config.log_tags = SWITCH_TRUE;
			node.userdata = "uuid-123";
			switch_event_create_plain(&node.tags, SWITCH_EVENT_CHANNEL_DATA);
			switch_event_add_header_string(node.tags, SWITCH_STACK_BOTTOM, "tenant name", "acme]\nwest");

			prefix = mod_logfile_prefix_build(&config, &node);
			fst_check_string_equals_safe(prefix, "uuid-123 tenant_name:acme__west ");

			switch_safe_free(prefix);
			switch_event_destroy(&node.tags);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(unsafe_uuid_is_sanitized)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			char *prefix;

			mod_logfile_prefix_config_init(&config);
			config.log_uuid = SWITCH_TRUE;
			node.userdata = "unsafe uuid]\nvalue";

			prefix = mod_logfile_prefix_build(&config, &node);
			fst_check_string_equals_safe(prefix, "unsafe_uuid__value ");

			switch_safe_free(prefix);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_TEST_END()

		FST_SESSION_BEGIN(combined_prefix_orders_uuid_tags_and_live_vars)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			char *expected = NULL;
			char *prefix = NULL;

			mod_logfile_prefix_config_init(&config);
			config.log_uuid = SWITCH_TRUE;
			config.log_tags = SWITCH_TRUE;
			node.userdata = (char *) switch_core_session_get_uuid(fst_session);
			fst_check_int_equals(switch_event_create_plain(&node.tags, SWITCH_EVENT_CHANNEL_DATA), SWITCH_STATUS_SUCCESS);
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config,
				" call = sip_call_id , origin = source_var "), SWITCH_STATUS_SUCCESS);
			fst_check(channel != NULL);
			fst_check(node.userdata != NULL);
			if (node.tags) {
				switch_event_add_header_string(node.tags, SWITCH_STACK_BOTTOM, "tenant name", "acme]\nwest");
				switch_event_add_header_string(node.tags, SWITCH_STACK_BOTTOM, "trace.id", "trace value");
			}
			if (channel && node.userdata) {
				switch_channel_set_variable(channel, "sip_call_id", "call id]\nwest");
				switch_channel_set_variable(channel, "source_var", "live[ value");
				expected = switch_mprintf("%s tenant_name:acme__west trace.id:trace_value call:call_id__west origin:live__value ",
					node.userdata);
				prefix = mod_logfile_prefix_build(&config, &node);
				fst_check_string_equals_safe(prefix, expected);
			}

			switch_safe_free(expected);
			switch_safe_free(prefix);
			switch_event_destroy(&node.tags);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_SESSION_END()

		FST_TEST_BEGIN(prefix_is_applied_without_truncation)
		{
			char *long_line = malloc(5002);
			char *formatted;

			fst_check(long_line != NULL);
			if (long_line) {
				memset(long_line, 'x', 5000);
				long_line[5000] = '\n';
				long_line[5001] = '\0';
				formatted = mod_logfile_prefix_lines("tenant:acme ", long_line);

				fst_check(formatted != NULL);
				if (formatted) {
					fst_check_int_equals((int) strlen(formatted), 5013);
					fst_check_string_starts_with(formatted, "tenant:acme ");
					fst_check_string_ends_with(formatted, "x\n");
				}

				switch_safe_free(formatted);
			}
			free(long_line);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_preserves_final_newline_shape)
		{
			char *formatted = mod_logfile_prefix_lines("tag:v ", "one\ntwo\n");

			fst_check_string_equals_safe(formatted, "tag:v one\ntag:v two\n");
			switch_safe_free(formatted);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_handles_single_blank_line)
		{
			char *formatted = mod_logfile_prefix_lines("tag:v ", "\n");

			fst_check_string_equals_safe(formatted, "tag:v \n");
			switch_safe_free(formatted);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_preserves_intermediate_blank_lines)
		{
			char *formatted = mod_logfile_prefix_lines("tag:v ", "a\n\nb");

			fst_check_string_equals_safe(formatted, "tag:v a\ntag:v \ntag:v b");
			switch_safe_free(formatted);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(prefix_handles_multiline_data_without_final_newline)
		{
			char *formatted = mod_logfile_prefix_lines("tag:v ", "one\ntwo");

			fst_check_string_equals_safe(formatted, "tag:v one\ntag:v two");
			switch_safe_free(formatted);
		}
		FST_TEST_END()

		FST_SESSION_BEGIN(channel_vars_are_live)
		{
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			char *prefix;

			mod_logfile_prefix_config_init(&config);
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config, "callid=sip_call_id"), SWITCH_STATUS_SUCCESS);
			node.userdata = (char *) switch_core_session_get_uuid(fst_session);
			fst_check(channel != NULL);
			fst_check(node.userdata != NULL);

			if (channel && node.userdata) {
				switch_channel_set_variable(channel, "sip_call_id", "first");
				prefix = mod_logfile_prefix_build(&config, &node);
				fst_check_string_equals_safe(prefix, "callid:first ");
				switch_safe_free(prefix);

				switch_channel_set_variable(channel, "sip_call_id", "second");
				prefix = mod_logfile_prefix_build(&config, &node);
				fst_check_string_equals_safe(prefix, "callid:second ");
				switch_safe_free(prefix);
			}

			mod_logfile_prefix_config_destroy(&config);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(c1_controls_are_sanitized_without_corrupting_utf8)
		{
			static const char captured_value[] = "raw" "\x85" "\x9b" "utf8" "\xc2\x85" "\xc2\x9b" "han" "\xe4\xb8\x80";
			static const char live_value[] = "raw" "\x85" "\x9b" "utf8" "\xc2\x85" "\xc2\x9b" "han" "\xe4\xb8\x80";
			mod_logfile_prefix_config_t config;
			switch_log_node_t node = { 0 };
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			char *prefix = NULL;

			mod_logfile_prefix_config_init(&config);
			config.log_tags = SWITCH_TRUE;
			fst_check_int_equals(mod_logfile_prefix_add_channel_vars(&config, "live=live_c1"), SWITCH_STATUS_SUCCESS);
			node.userdata = (char *) switch_core_session_get_uuid(fst_session);
			fst_check(channel != NULL);
			fst_check(node.userdata != NULL);
			fst_check_int_equals(switch_event_create_plain(&node.tags, SWITCH_EVENT_CHANNEL_DATA), SWITCH_STATUS_SUCCESS);

			if (node.tags) {
				switch_event_add_header_string(node.tags, SWITCH_STACK_BOTTOM, "captured", captured_value);
			}
			if (channel && node.userdata && node.tags) {
				switch_channel_set_variable(channel, "live_c1", live_value);
				prefix = mod_logfile_prefix_build(&config, &node);
				fst_check_string_equals_safe(prefix,
					"captured:raw__utf8__han" "\xe4\xb8\x80" " live:raw__utf8__han" "\xe4\xb8\x80" " ");
			}

			switch_safe_free(prefix);
			switch_event_destroy(&node.tags);
			mod_logfile_prefix_config_destroy(&config);
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
