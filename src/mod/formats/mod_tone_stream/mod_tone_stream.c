/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_tone_stream.c -- Tone Generation Stream
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_tone_stream_load);
SWITCH_MODULE_DEFINITION(mod_tone_stream, mod_tone_stream_load, NULL, NULL);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_tone_stream_shutdown);

struct silence_handle {
	int32_t samples;
	int silence;
	int forever;
};

static switch_status_t silence_stream_file_open(switch_file_handle_t *handle, const char *path)
{

	struct silence_handle *sh;
	int ms;
	char *p;

	sh = switch_core_alloc(handle->memory_pool, sizeof(*sh));

	ms = atoi(path);

	if (ms > 0) {
		sh->samples = (handle->samplerate / 1000) * ms;
	} else {
		sh->samples = 0;
		sh->forever = 1;
	}

	if ((p = strchr(path, ','))) {
		p++;
		ms = atoi(p);
		if (ms > 0) {
			sh->silence = ms;
		}
	}

	handle->private_info = sh;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t silence_stream_file_close(switch_file_handle_t *handle)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t silence_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	struct silence_handle *sh = handle->private_info;

	if (!sh->forever) {
		if (sh->samples <= 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (*len > (size_t) sh->samples) {
			*len = sh->samples;
		}

		sh->samples -= *len;
	}

	if (sh->silence) {
		switch_generate_sln_silence((int16_t *) data, *len, sh->silence);
	} else {
		memset(data, 0, *len * 2);
	}

	return SWITCH_STATUS_SUCCESS;
}


static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	switch_buffer_t *audio_buffer = ts->user_data;
	int wrote;

	if (!audio_buffer) {
		return -1;
	}

	wrote = teletone_mux_tones(ts, map);
	switch_buffer_write(audio_buffer, ts->buffer, wrote * 2);

	return 0;
}

static switch_status_t tone_stream_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_buffer_t *audio_buffer = NULL;
	teletone_generation_session_t ts;
	char *tonespec;
	int loops = 0;
	char *tmp;
	int fd = -1;
	char buf[1024] = "";
	size_t len;

	memset(&ts, 0, sizeof(ts));

	tonespec = switch_core_strdup(handle->memory_pool, path);

	switch_buffer_create_dynamic(&audio_buffer, 1024, 1024, 0);
	switch_assert(audio_buffer);

	if ((tmp = (char *)switch_stristr(";loops=", tonespec))) {
		*tmp = '\0';
		tmp += 7;
		if (tmp) {
			loops = atoi(tmp);
			switch_buffer_set_loops(audio_buffer, loops);
		}
	}

	if (handle->params) {
		if ((tmp = switch_event_get_header(handle->params, "loops"))) {
			loops = atoi(tmp);
			switch_buffer_set_loops(audio_buffer, loops);		
		}
	}

	if (!handle->samplerate) {
		handle->samplerate = 8000;
	}

	teletone_init_session(&ts, 0, teletone_handler, audio_buffer);
	ts.rate = handle->samplerate;
	ts.channels = 1;

	if (!strncasecmp(tonespec, "path=", 5)) {
		tmp = tonespec + 5;
		if ((fd = open(tmp, O_RDONLY)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to open [%s]\n", tmp);
			return SWITCH_STATUS_FALSE;
		}

		while ((len = switch_fd_read_line(fd, buf, sizeof(buf)))) {
			teletone_run(&ts, buf);
		}
		close(fd);
		fd = -1;
	} else {
		teletone_run(&ts, tonespec);
	}

	teletone_destroy_session(&ts);

	handle->private_info = audio_buffer;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tone_stream_file_close(switch_file_handle_t *handle)
{
	switch_buffer_t *audio_buffer = handle->private_info;

	switch_buffer_destroy(&audio_buffer);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tone_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_buffer_t *audio_buffer = handle->private_info;
	switch_size_t bytes;

	if ((bytes = switch_buffer_read_loop(audio_buffer, data, *len * 2)) <= 0) {
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

	*len = bytes / 2;
	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };
static char *silence_supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_tone_stream_load)
{
	switch_file_interface_t *file_interface;
	supported_formats[0] = "tone_stream";
	silence_supported_formats[0] = "silence_stream";

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = tone_stream_file_open;
	file_interface->file_close = tone_stream_file_close;
	file_interface->file_read = tone_stream_file_read;

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = silence_supported_formats;
	file_interface->file_open = silence_stream_file_open;
	file_interface->file_close = silence_stream_file_close;
	file_interface->file_read = silence_stream_file_read;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_tone_stream_shutdown)
{
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
