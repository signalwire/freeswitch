/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * utils.c -- VoiceMail IVR / Different utility that might need to go into the core (after cleanup)
 *
 */
#include <switch.h>

#include "utils.h"

switch_status_t vmivr_merge_media_files(const char** inputs, const char *output, int rate) {
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh_output = { 0 };
	int channels = 1;
	int j = 0;

	if (switch_core_file_open(&fh_output, output, channels, rate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open %s\n", output);
		goto end;
	}

	for (j = 0; inputs[j] != NULL && j < 128 && status == SWITCH_STATUS_SUCCESS; j++) {
		switch_file_handle_t fh_input = { 0 };
		char buf[2048];
		switch_size_t len = sizeof(buf) / 2;

		if (switch_core_file_open(&fh_input, inputs[j], channels, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open %s\n", inputs[j]);
			status = SWITCH_STATUS_GENERR;
			break;
		}

		while (switch_core_file_read(&fh_input, buf, &len) == SWITCH_STATUS_SUCCESS) {
			if (switch_core_file_write(&fh_output, buf, &len) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write error\n");
				status = SWITCH_STATUS_GENERR;
				break;
			}
		}

		if (fh_input.file_interface) {
			switch_core_file_close(&fh_input);
		}
	}

	if (fh_output.file_interface) {
		switch_core_file_close(&fh_output);
	}
end:
	return status;
}

void jsonapi_populate_event(switch_core_session_t *session, switch_event_t *apply_event, const char *api, const char *data) {
	switch_event_t *phrases_event = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hp;

	switch_assert(apply_event);
	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(api, data, session, &stream);
	switch_event_create_json(&phrases_event, (char *) stream.data);
	switch_safe_free(stream.data);

	for (hp = phrases_event->headers; hp; hp = hp->next) {
		if (!strncasecmp(hp->name, "VM-", 3)) {
			switch_event_add_header(apply_event, SWITCH_STACK_BOTTOM, hp->name, "%s", hp->value);
		}
	}
	switch_event_destroy(&phrases_event);
	phrases_event = apply_event;

	return;
}

switch_event_t *jsonapi2event(switch_core_session_t *session, const char *api, const char *data) {
	switch_event_t *phrases_event = NULL;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(api, data, session, &stream);
	switch_event_create_json(&phrases_event, (char *) stream.data);
	switch_safe_free(stream.data);

	return phrases_event;
}

char *generate_random_file_name(switch_core_session_t *session, const char *mod_name, const char *file_extension) {
	char rand_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";
	switch_uuid_t srand_uuid;

	switch_uuid_get(&srand_uuid);
	switch_uuid_format(rand_uuid, &srand_uuid);

	return switch_core_session_sprintf(session, "%s%s%s_%s.%s", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, mod_name, rand_uuid, file_extension);

}

switch_status_t vmivr_api_execute(switch_core_session_t *session, const char *apiname, const char *arguments) {
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(apiname, arguments, session, &stream);
	if (!strncasecmp(stream.data, "-ERR", 4)) {
		status = SWITCH_STATUS_GENERR;
	}
	switch_safe_free(stream.data);
	return status;
}

void append_event_message(switch_core_session_t *session, vmivr_profile_t *profile, switch_event_t *phrase_params, switch_event_t *msg_list_event, size_t current_msg) {

	char *varname;
	char *apicmd;
	char *total_msg = NULL;

	if (!msg_list_event || !(total_msg = switch_event_get_header(msg_list_event, "VM-List-Count")) || current_msg > atoi(total_msg)) {
		/* TODO Error MSG */
		return;
	}

	varname = switch_mprintf("VM-List-Message-%" SWITCH_SIZE_T_FMT "-UUID", current_msg);
	apicmd = switch_mprintf("json %s %s %s %s", profile->api_profile, profile->domain, profile->id, switch_event_get_header(msg_list_event, varname));

	switch_safe_free(varname);

	jsonapi_populate_event(session, phrase_params, profile->api_msg_get, apicmd);

	/* TODO Set these 2 header correctly */
	switch_event_add_header(phrase_params, SWITCH_STACK_BOTTOM, "VM-Message-Type", "%s", "new");
	switch_event_add_header(phrase_params, SWITCH_STACK_BOTTOM, "VM-Message-Number", "%"SWITCH_SIZE_T_FMT, current_msg);

	switch_event_add_header_string(phrase_params, SWITCH_STACK_BOTTOM, "VM-Message-Private-Local-Copy", "False");

	switch_safe_free(apicmd);
}

