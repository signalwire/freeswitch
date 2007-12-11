/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_file.c -- Main Core Library (File I/O Functions)
 *
 */
#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_file_open(switch_file_handle_t *fh,
													  const char *file_path, uint8_t channels, uint32_t rate, unsigned int flags, switch_memory_pool_t *pool)
{
	char *ext;
	switch_status_t status;
	char stream_name[128] = "";
	char *rhs = NULL;

	if ((rhs = strstr(file_path, SWITCH_URL_SEPARATOR))) {
		switch_copy_string(stream_name, file_path, (rhs + 1) - file_path);
		ext = stream_name;
		file_path = rhs + 3;
	} else {
		if ((ext = strrchr(file_path, '.')) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
			return SWITCH_STATUS_FALSE;
		}
		ext++;
	}

	if ((fh->file_interface = switch_loadable_module_get_file_interface(ext)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid file format [%s] for [%s]!\n", ext, file_path);
		return SWITCH_STATUS_GENERR;
	}

	fh->flags = flags;
	if (pool) {
		fh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(fh, SWITCH_FILE_FLAG_FREE_POOL);
	}

	if (rhs) {
		fh->handler = switch_core_strdup(fh->memory_pool, rhs);
	}

	if (rate) {
		fh->samplerate = rate;
	} else {
		rate = 8000;
	}

	if (channels) {
		fh->channels = channels;
	} else {
		fh->channels = 1;
	}

	if ((status = fh->file_interface->file_open(fh, file_path)) == SWITCH_STATUS_SUCCESS) {
		switch_set_flag(fh, SWITCH_FILE_OPEN);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	return fh->file_interface->file_read(fh, data, len);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_write(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	return fh->file_interface->file_write(fh, data, len);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples, int whence)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	switch_set_flag(fh, SWITCH_FILE_SEEK);
	status = fh->file_interface->file_seek(fh, cur_pos, samples, whence);
	if (samples) {
		fh->offset_pos = *cur_pos;
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_set_string(switch_file_handle_t *fh, switch_audio_col_t col, const char *string)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	return fh->file_interface->file_set_string(fh, col, string);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_get_string(switch_file_handle_t *fh, switch_audio_col_t col, const char **string)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	return fh->file_interface->file_get_string(fh, col, string);

}


SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	switch_clear_flag(fh, SWITCH_FILE_OPEN);
	return fh->file_interface->file_close(fh);

}
