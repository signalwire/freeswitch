/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_png -- play a png as video, optionally with audio
 *
 */

#include <switch.h>

#ifdef _MSC_VER
// Disable MSVC warnings that suggest making code non-portable.
#pragma warning(disable : 4996)
#endif

switch_loadable_module_interface_t *MODULE_INTERFACE;

SWITCH_MODULE_LOAD_FUNCTION(mod_png_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_png_shutdown);
SWITCH_MODULE_DEFINITION(mod_png, mod_png_load, mod_png_shutdown, NULL);

struct png_file_context {
	switch_memory_pool_t *pool;
	switch_image_t *img;
	int reads;
	int sent;
	int max;
	int samples;
	switch_file_handle_t *audio_fh;
};

typedef struct png_file_context png_file_context_t;

static switch_status_t png_file_open(switch_file_handle_t *handle, const char *path)
{
	png_file_context_t *context;
	char *ext;
	unsigned int flags = 0;

	if ((ext = strrchr((char *)path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;
	
	if ((context = (png_file_context_t *)switch_core_alloc(handle->memory_pool, sizeof(png_file_context_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Video only\n");
		return SWITCH_STATUS_GENERR; 
	}

	memset(context, 0, sizeof(png_file_context_t));

	if (!(context->img = switch_img_read_png(path, SWITCH_IMG_FMT_I420))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		return SWITCH_STATUS_GENERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		flags |= SWITCH_FOPEN_READ;
	}

	context->max = 10000;

	if (handle->params) {
		const char *audio_file = switch_event_get_header(handle->params, "audio_file");
		const char *max = switch_event_get_header(handle->params, "png_ms");
		int tmp;

		if (max) {
			tmp = atol(max);
			context->max = tmp;
		}

		if (audio_file) {
			context->audio_fh = switch_core_alloc(handle->memory_pool, sizeof(*context->audio_fh));
			switch_assert(context->audio_fh);

			if (switch_core_file_open(context->audio_fh,
									  audio_file,
									  handle->channels,
									  handle->samplerate,
									  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
									  NULL) != SWITCH_STATUS_SUCCESS) {
									  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to open audio file %s\n", audio_file);
									  context->audio_fh = NULL;
			}
		}
	}

	if (context->max) {
		context->samples = (handle->samplerate / 1000) * context->max;
	}
	
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->pool = handle->memory_pool;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s]\n", path);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t png_file_close(switch_file_handle_t *handle)
{
	png_file_context_t *context = (png_file_context_t *)handle->private_info;

	switch_img_free(&context->img);
	
	if (context->audio_fh) switch_core_file_close(context->audio_fh);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t png_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{

	png_file_context_t *context = (png_file_context_t *)handle->private_info;

	if (context->audio_fh) {
		return switch_core_file_read(context->audio_fh, data, len);
	}

	if (!context->img || !context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->samples > 0) {
		if (*len >= context->samples) {
			*len = context->samples;
		}

		context->samples -= *len;
	}

	if (!context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	memset(data, 0, *len * 2 * handle->channels);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t png_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	png_file_context_t *context = (png_file_context_t *)handle->private_info;
	switch_image_t *dup = NULL;

	if ((flags & SVR_CHECK)) {
		return SWITCH_STATUS_BREAK;
	}

	if (!context->img || !context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	if ((context->reads++ % 20) == 0) {
		switch_img_copy(context->img, &dup);
		frame->img = dup;
		context->sent++;
	} else {
		if ((flags && SVR_BLOCK)) {
			switch_yield(5000);
		}
		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t write_png_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *path = (char *) user_data;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
			switch_channel_set_flag_recursive(channel, CF_VIDEO_DECODED_READ);
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
			switch_thread_rwlock_unlock(MODULE_INTERFACE->rwlock);
			switch_channel_clear_flag_recursive(channel, CF_VIDEO_DECODED_READ);
		}
		break;
	case SWITCH_ABC_TYPE_READ_VIDEO_PING:
		{
			switch_frame_t *frame = switch_core_media_bug_get_video_ping_frame(bug);
			if (!frame || !frame->img) break;
			switch_img_write_png(frame->img, path);
			return SWITCH_FALSE;
		}
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_STANDARD_API(uuid_write_png_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *mydata = NULL;
	char *uuid;
	char *path;
	switch_media_bug_t *bug;
	switch_media_bug_flag_t flags = SMBF_READ_VIDEO_PING;
	switch_core_session_t *session_;

	if (!zstr(cmd) && (mydata = strdup(cmd))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 2) {
		stream->write_function(stream, "-USAGE: <uuid> <path> \n");	
		goto end;
	}
	
	uuid = argv[0];
	if (!(session_ = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Could not locate session.\n");
		goto end;
	}

	if (!switch_channel_test_flag(switch_core_session_get_channel(session_), CF_VIDEO)) {
		stream->write_function(stream, "-ERR Session does not have video.\n");
		switch_core_session_rwunlock(session_);
		goto end;
	}

    if (!switch_is_file_path(argv[1])) {
        const char *prefix = SWITCH_GLOBAL_dirs.images_dir;
        path = switch_core_session_sprintf(session_, "%s%s%s", prefix, SWITCH_PATH_SEPARATOR, argv[1]);
    } else {
        path = switch_core_session_strdup(session_, argv[1]);
    }

	switch_thread_rwlock_rdlock(MODULE_INTERFACE->rwlock);

	if (switch_core_media_bug_add(session_, NULL, NULL, write_png_bug_callback, path, 0, flags, &bug) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR Could not attach bug.\n");
		switch_thread_rwlock_unlock(MODULE_INTERFACE->rwlock);
	} else {
		stream->write_function(stream, "+OK\n");
	}

	switch_core_session_rwunlock(session_);

  end:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

static char *supported_formats[2] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_png_load)
{
	switch_api_interface_t *api_interface;
	switch_file_interface_t *file_interface;

	supported_formats[0] = (char *)"png";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	MODULE_INTERFACE = *module_interface;

	file_interface = (switch_file_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = png_file_open;
	file_interface->file_close = png_file_close;
	file_interface->file_read = png_file_read;
	file_interface->file_read_video = png_file_read_video;

	SWITCH_ADD_API(api_interface, "uuid_write_png", "grab an image from a call",uuid_write_png_function, "");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_png_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
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
