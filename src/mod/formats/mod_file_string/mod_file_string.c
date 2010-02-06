/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Cesar Cepeda <cesar@auronix.com>
 *
 *
 * mod_file_string.c -- Local Streaming Audio
 *
 */
#include <switch.h>
/* for apr_pstrcat */
#define DEFAULT_PREBUFFER_SIZE 1024 * 64

SWITCH_MODULE_LOAD_FUNCTION(mod_file_string_load);

SWITCH_MODULE_DEFINITION(mod_file_string, mod_file_string_load, NULL, NULL);


struct file_string_source;

static struct {
	switch_mutex_t *mutex;
	switch_hash_t *source_hash;
} globals;

struct file_string_context {
	char *argv[128];
	int argc;
	int index;
	int samples;
	switch_file_handle_t fh;
};

typedef struct file_string_context file_string_context_t;


static int next_file(switch_file_handle_t *handle)
{
	file_string_context_t *context = handle->private_info;
	char *file;
	const char *prefix = handle->prefix;

  top:

	context->index++;

	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&context->fh);
	}

	if (context->index == context->argc) {
		return 0;
	}


	if (!prefix) {
		if (!(prefix = switch_core_get_variable("sound_prefix"))) {
			prefix = SWITCH_GLOBAL_dirs.sounds_dir;
		}
	}

	if (!prefix || switch_is_file_path(context->argv[context->index])) {
		file = context->argv[context->index];
	} else {
		file = switch_core_sprintf(handle->memory_pool, "%s%s%s", prefix, SWITCH_PATH_SEPARATOR, context->argv[context->index]);
	}

	if (switch_core_file_open(&context->fh,
							  file, handle->channels, handle->samplerate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		goto top;
	}

	handle->samples = context->fh.samples;
	handle->samplerate = context->fh.samplerate;
	handle->channels = context->fh.channels;
	handle->format = context->fh.format;
	handle->sections = context->fh.sections;
	handle->seekable = context->fh.seekable;
	handle->speed = context->fh.speed;
	handle->interval = context->fh.interval;
	handle->flags = context->fh.flags;

	if (context->index == 0) {
		context->samples = (handle->samplerate / 1000) * 250;
	}

	return 1;
}

static switch_status_t file_string_file_open(switch_file_handle_t *handle, const char *path)
{
	file_string_context_t *context;
	char *file_dup;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This format does not support writing!\n");
		return SWITCH_STATUS_FALSE;
	}

	context = switch_core_alloc(handle->memory_pool, sizeof(*context));

	file_dup = switch_core_strdup(handle->memory_pool, path);
	context->argc = switch_separate_string(file_dup, '!', context->argv, (sizeof(context->argv) / sizeof(context->argv[0])));
	context->index = -1;

	handle->private_info = context;

	return next_file(handle) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t file_string_file_close(switch_file_handle_t *handle)
{
	file_string_context_t *context = handle->private_info;

	switch_core_file_close(&context->fh);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t file_string_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	file_string_context_t *context = handle->private_info;
	switch_status_t status;
	size_t llen = *len;

	if (context->samples > 0) {
		if (*len > (size_t) context->samples) {
			*len = context->samples;
		}

		context->samples -= *len;
		switch_generate_sln_silence((int16_t *) data, *len, 400);
		status = SWITCH_STATUS_SUCCESS;
	} else {
		status = switch_core_file_read(&context->fh, data, len);
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		if (!next_file(handle)) {
			return SWITCH_STATUS_FALSE;
		}
		*len = llen;
		status = switch_core_file_read(&context->fh, data, len);
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_file_string_load)
{
	switch_file_interface_t *file_interface;
	supported_formats[0] = "file_string";

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = file_string_file_open;
	file_interface->file_close = file_string_file_close;
	file_interface->file_read = file_string_file_read;

	memset(&globals, 0, sizeof(globals));
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
