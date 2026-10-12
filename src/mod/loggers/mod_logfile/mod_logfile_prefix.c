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
 * mod_logfile_prefix.c -- Structured logfile prefix formatting
 *
 */

#include "mod_logfile_prefix.h"

#define LOGFILE_TOKEN_NAME_MAX 128
#define LOGFILE_TOKEN_VALUE_MAX 512
#define LOGFILE_CHANNEL_VAR_MAX 128
#define LOGFILE_BUFFER_INITIAL_SIZE 64

typedef struct {
	char *data;
	switch_size_t length;
	switch_size_t capacity;
} logfile_buffer_t;

#ifdef MOD_LOGFILE_PREFIX_TEST
static int test_allocation_fail_after = -1;
static int test_allocation_count = 0;

void mod_logfile_prefix_test_fail_allocation_after(int successful_allocations)
{
	test_allocation_fail_after = successful_allocations;
	test_allocation_count = 0;
}

void mod_logfile_prefix_test_reset_failures(void)
{
	test_allocation_fail_after = -1;
	test_allocation_count = 0;
}
#endif

static void *logfile_buffer_realloc(void *data, switch_size_t size)
{
#ifdef MOD_LOGFILE_PREFIX_TEST
	if (test_allocation_fail_after >= 0 && test_allocation_count >= test_allocation_fail_after) {
		return NULL;
	}
	test_allocation_count++;
#endif
	return realloc(data, size);
}

static switch_status_t logfile_buffer_reserve(logfile_buffer_t *buffer, switch_size_t additional)
{
	char *data;
	switch_size_t capacity;
	switch_size_t required;

	if (!buffer || additional > (switch_size_t) -1 - buffer->length - 1) return SWITCH_STATUS_MEMERR;
	required = buffer->length + additional + 1;
	if (required <= buffer->capacity) return SWITCH_STATUS_SUCCESS;

	capacity = buffer->capacity ? buffer->capacity : LOGFILE_BUFFER_INITIAL_SIZE;
	while (capacity < required) {
		if (capacity > (switch_size_t) -1 / 2) {
			capacity = required;
			break;
		}
		capacity *= 2;
	}

	data = logfile_buffer_realloc(buffer->data, capacity);
	if (!data) return SWITCH_STATUS_MEMERR;
	buffer->data = data;
	buffer->capacity = capacity;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t logfile_buffer_append(logfile_buffer_t *buffer, const char *data, switch_size_t length)
{
	if (!length) return SWITCH_STATUS_SUCCESS;
	if (!buffer || !data || logfile_buffer_reserve(buffer, length) != SWITCH_STATUS_SUCCESS) return SWITCH_STATUS_MEMERR;
	memcpy(buffer->data + buffer->length, data, length);
	buffer->length += length;
	buffer->data[buffer->length] = '\0';
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t logfile_buffer_append_string(logfile_buffer_t *buffer, const char *data)
{
	return logfile_buffer_append(buffer, data, data ? strlen(data) : 0);
}

static void logfile_buffer_destroy(logfile_buffer_t *buffer)
{
	if (!buffer) return;
	switch_safe_free(buffer->data);
	buffer->length = 0;
	buffer->capacity = 0;
}

static void sanitize_name(char *dst, switch_size_t size, const char *src)
{
	switch_size_t i = 0;

	if (!dst || !size) return;
	for (; src && *src && i + 1 < size; src++) {
		unsigned char c = (unsigned char) *src;
		switch_bool_t safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
		dst[i++] = (char) (safe ? c : '_');
	}
	dst[i] = '\0';
}

static switch_size_t utf8_sequence_length(const unsigned char *src)
{
	unsigned char first;

	if (!src) return 0;
	first = src[0];
	if (first >= 0xc2 && first <= 0xdf) {
		return src[1] && src[1] >= 0x80 && src[1] <= 0xbf ? 2 : 0;
	}
	if (first == 0xe0) {
		return src[1] >= 0xa0 && src[1] <= 0xbf && src[2] &&
			src[2] >= 0x80 && src[2] <= 0xbf ? 3 : 0;
	}
	if ((first >= 0xe1 && first <= 0xec) || (first >= 0xee && first <= 0xef)) {
		return src[1] >= 0x80 && src[1] <= 0xbf && src[2] &&
			src[2] >= 0x80 && src[2] <= 0xbf ? 3 : 0;
	}
	if (first == 0xed) {
		return src[1] >= 0x80 && src[1] <= 0x9f && src[2] &&
			src[2] >= 0x80 && src[2] <= 0xbf ? 3 : 0;
	}
	if (first == 0xf0) {
		return src[1] >= 0x90 && src[1] <= 0xbf && src[2] && src[3] &&
			src[2] >= 0x80 && src[2] <= 0xbf && src[3] >= 0x80 && src[3] <= 0xbf ? 4 : 0;
	}
	if (first >= 0xf1 && first <= 0xf3) {
		return src[1] >= 0x80 && src[1] <= 0xbf && src[2] && src[3] &&
			src[2] >= 0x80 && src[2] <= 0xbf && src[3] >= 0x80 && src[3] <= 0xbf ? 4 : 0;
	}
	if (first == 0xf4) {
		return src[1] >= 0x80 && src[1] <= 0x8f && src[2] && src[3] &&
			src[2] >= 0x80 && src[2] <= 0xbf && src[3] >= 0x80 && src[3] <= 0xbf ? 4 : 0;
	}

	return 0;
}

static void sanitize_value(char *dst, switch_size_t size, const char *src)
{
	switch_size_t i = 0;
	const unsigned char *cursor = (const unsigned char *) src;

	if (!dst || !size) return;
	while (cursor && *cursor && i + 1 < size) {
		unsigned char c = *cursor;
		switch_size_t length = utf8_sequence_length(cursor);

		if (length) {
			if (length == 2 && cursor[0] == 0xc2 && cursor[1] >= 0x80 && cursor[1] <= 0x9f) {
				dst[i++] = '_';
			} else if (i + length < size) {
				memcpy(dst + i, cursor, length);
				i += length;
			} else {
				break;
			}
			cursor += length;
		} else {
			dst[i++] = (char) (c <= 32 || c == 127 || c == '[' || c == ']' || switch_iscntrl(c) ? '_' : c);
			cursor++;
		}
	}
	dst[i] = '\0';
}

static switch_status_t append_pair(logfile_buffer_t *buffer, const char *name, const char *value)
{
	char safe_name[LOGFILE_TOKEN_NAME_MAX + 1] = "";
	char safe_value[LOGFILE_TOKEN_VALUE_MAX + 1] = "";

	if (!buffer) return SWITCH_STATUS_FALSE;
	sanitize_name(safe_name, sizeof(safe_name), name);
	sanitize_value(safe_value, sizeof(safe_value), value);
	if (!zstr(safe_name) && !zstr(safe_value)) {
		if (logfile_buffer_append_string(buffer, safe_name) != SWITCH_STATUS_SUCCESS ||
			logfile_buffer_append(buffer, ":", 1) != SWITCH_STATUS_SUCCESS ||
			logfile_buffer_append_string(buffer, safe_value) != SWITCH_STATUS_SUCCESS ||
			logfile_buffer_append(buffer, " ", 1) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_MEMERR;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

void mod_logfile_prefix_config_init(mod_logfile_prefix_config_t *config)
{
	if (!config) return;
	memset(config, 0, sizeof(*config));
}

void mod_logfile_prefix_config_destroy(mod_logfile_prefix_config_t *config)
{
	if (config && config->channel_vars) {
		switch_event_destroy(&config->channel_vars);
	}
}

switch_status_t mod_logfile_prefix_add_channel_vars(mod_logfile_prefix_config_t *config, const char *data)
{
	char *argv[LOGFILE_CHANNEL_VAR_MAX] = { 0 };
	char *dup;
	int argc;
	int i;
	int added = 0;

	if (!config || zstr(data)) return SWITCH_STATUS_FALSE;
	dup = strdup(data);
	if (!dup) return SWITCH_STATUS_MEMERR;
	argc = switch_separate_string(dup, ',', argv, LOGFILE_CHANNEL_VAR_MAX);

	if (!config->channel_vars &&
		switch_event_create_plain(&config->channel_vars, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		free(dup);
		return SWITCH_STATUS_MEMERR;
	}

	for (i = 0; i < argc; i++) {
		char *item = switch_strip_spaces(argv[i], SWITCH_FALSE);
		char *name = item;
		char *variable = strchr(item, '=');

		if (variable) {
			*variable++ = '\0';
			name = switch_strip_spaces(name, SWITCH_FALSE);
			variable = switch_strip_spaces(variable, SWITCH_FALSE);
		} else {
			variable = name;
		}

		if (!zstr(name) && !zstr(variable)) {
			switch_event_del_header(config->channel_vars, name);
			switch_event_add_header_string(config->channel_vars, SWITCH_STACK_BOTTOM, name, variable);
			added++;
		}
	}

	free(dup);
	return added ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

char *mod_logfile_prefix_build(const mod_logfile_prefix_config_t *config, const switch_log_node_t *node)
{
	logfile_buffer_t buffer = { 0 };
	switch_event_header_t *header;
	switch_core_session_t *session = NULL;

	if (!config || !node) return NULL;

	if (config->log_uuid && !zstr(node->userdata)) {
		char uuid[LOGFILE_TOKEN_VALUE_MAX + 1] = "";
		sanitize_value(uuid, sizeof(uuid), node->userdata);
		if (!zstr(uuid) && (logfile_buffer_append_string(&buffer, uuid) != SWITCH_STATUS_SUCCESS ||
			logfile_buffer_append(&buffer, " ", 1) != SWITCH_STATUS_SUCCESS)) {
			goto fail;
		}
	}

	if (config->log_tags && node->tags) {
		for (header = node->tags->headers; header; header = header->next) {
			if (append_pair(&buffer, header->name, header->value) != SWITCH_STATUS_SUCCESS) goto fail;
		}
	}

	if (!zstr(node->userdata) && config->channel_vars && config->channel_vars->headers &&
		(session = switch_core_session_locate(node->userdata))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		for (header = config->channel_vars->headers; header; header = header->next) {
			const char *value = switch_channel_get_variable(channel, header->value);
			if (append_pair(&buffer, header->name, value) != SWITCH_STATUS_SUCCESS) {
				switch_core_session_rwunlock(session);
				session = NULL;
				goto fail;
			}
		}
		switch_core_session_rwunlock(session);
		session = NULL;
	}

	if (!buffer.length) logfile_buffer_destroy(&buffer);
	return buffer.data;

  fail:
	if (session) switch_core_session_rwunlock(session);
	logfile_buffer_destroy(&buffer);
	return NULL;
}

char *mod_logfile_prefix_lines(const char *prefix, const char *data)
{
	logfile_buffer_t buffer = { 0 };
	const char *cursor;
	switch_size_t prefix_length;

	if (zstr(data)) return NULL;
	if (zstr(prefix)) return strdup(data);

	prefix_length = strlen(prefix);
	cursor = data;
	while (*cursor) {
		const char *newline = strchr(cursor, '\n');
		switch_size_t length = newline ? (switch_size_t) (newline - cursor) : strlen(cursor);

		if (logfile_buffer_append(&buffer, prefix, prefix_length) != SWITCH_STATUS_SUCCESS ||
			(length && logfile_buffer_append(&buffer, cursor, length) != SWITCH_STATUS_SUCCESS)) goto fail;
		if (!newline) break;
		if (logfile_buffer_append(&buffer, "\n", 1) != SWITCH_STATUS_SUCCESS) goto fail;
		cursor = newline + 1;
	}

	return buffer.data;

  fail:
	logfile_buffer_destroy(&buffer);
	return NULL;
}

switch_status_t mod_logfile_complete_write(void *context, const char *data, switch_size_t length,
	mod_logfile_write_callback_t write_callback, mod_logfile_reopen_callback_t reopen_callback,
	switch_size_t *committed)
{
	switch_size_t offset = 0;
	switch_bool_t recovered = SWITCH_FALSE;

	if (committed) *committed = 0;
	if (!data || !length || !write_callback) return SWITCH_STATUS_FALSE;

	while (offset < length) {
		switch_size_t written = length - offset;
		switch_status_t status = write_callback(context, data + offset, &written);

		if (written > length - offset) written = length - offset;
		offset += written;
		if (committed) *committed = offset;
		if (offset == length) return SWITCH_STATUS_SUCCESS;
		if (status == SWITCH_STATUS_SUCCESS && written) continue;

		if (recovered || !reopen_callback || reopen_callback(context) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		recovered = SWITCH_TRUE;
	}

	return SWITCH_STATUS_SUCCESS;
}
