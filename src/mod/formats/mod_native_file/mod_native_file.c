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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_native_file.c -- Native Files
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_native_file_load);
SWITCH_MODULE_DEFINITION(mod_native_file, mod_native_file_load, NULL, NULL);

struct native_file_context {
	switch_file_t *fd;
};

typedef struct native_file_context native_file_context;

static switch_status_t native_file_file_open(switch_file_handle_t *handle, const char *path)
{
	native_file_context *context;
	char *ext;
	unsigned int flags = 0;

	if ((ext = strrchr(path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		flags |= SWITCH_FOPEN_WRITE | SWITCH_FOPEN_CREATE;
		if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND) || switch_test_flag(handle, SWITCH_FILE_WRITE_OVER)) {
			flags |= SWITCH_FOPEN_READ;
		} else {
			flags |= SWITCH_FOPEN_TRUNCATE;
		}

	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		flags |= SWITCH_FOPEN_READ;
	}

	if (switch_file_open(&context->fd, path, flags, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, handle->memory_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
		int64_t samples = 0;
		switch_file_seek(context->fd, SEEK_END, &samples);
		handle->pos = samples;
	}

	handle->samples = 0;
	handle->samplerate = 8000;

	if (ext) {
		if (!strcasecmp(ext, "G722")) {
			handle->samplerate = 16000;
		}
	}

	handle->channels = 1;
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 1;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	handle->flags |= SWITCH_FILE_NATIVE;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s] %dhz\n", path, handle->samplerate);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t native_file_file_truncate(switch_file_handle_t *handle, int64_t offset)
{
	native_file_context *context = handle->private_info;
	switch_status_t status;

	if ((status = switch_file_trunc(context->fd, offset)) == SWITCH_STATUS_SUCCESS) {
		handle->pos = 0;
	}

	return status;

}

static switch_status_t native_file_file_close(switch_file_handle_t *handle)
{
	native_file_context *context = handle->private_info;

	if (context->fd) {
		switch_file_close(context->fd);
		context->fd = NULL;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t native_file_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_status_t status;

	native_file_context *context = handle->private_info;

	status = switch_file_seek(context->fd, whence, &samples);
	if (status == SWITCH_STATUS_SUCCESS) {
		handle->pos += samples;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t native_file_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_status_t status;

	native_file_context *context = handle->private_info;

	status = switch_file_read(context->fd, data, len);
	if (status == SWITCH_STATUS_SUCCESS) {
		handle->pos += *len;
	}
	return status;
}

static switch_status_t native_file_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	native_file_context *context = handle->private_info;

	return switch_file_write(context->fd, data, len);
}

static switch_status_t native_file_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t native_file_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS + 1] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_native_file_load)
{
	switch_file_interface_t *file_interface;

	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	uint32_t num_codecs = switch_loadable_module_get_codecs(codecs, sizeof(codecs) / sizeof(codecs[0]));
	uint32_t x;

	for (x = 0; x < num_codecs; x++) {
		supported_formats[x] = switch_core_strdup(pool, codecs[x]->iananame);
	}

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = native_file_file_open;
	file_interface->file_close = native_file_file_close;
	file_interface->file_truncate = native_file_file_truncate;
	file_interface->file_read = native_file_file_read;
	file_interface->file_write = native_file_file_write;
	file_interface->file_seek = native_file_file_seek;
	file_interface->file_set_string = native_file_file_set_string;
	file_interface->file_get_string = native_file_file_get_string;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
