/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2017, Seven Du <dujinfang@gmail.com>
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
 * Anthony Minessale <anthm@freeswitch.org>
 *
 * mod_video_filter -- FS Video Codec / File Format using libav.org
 *
 */

#include <switch.h>

switch_loadable_module_interface_t *MODULE_INTERFACE;

SWITCH_MODULE_LOAD_FUNCTION(mod_video_filter_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_video_filter_shutdown);
SWITCH_MODULE_DEFINITION(mod_video_filter, mod_video_filter_load, mod_video_filter_shutdown, NULL);

typedef struct chromakey_context_s {
	int threshold;
	switch_image_t *bgimg;
	switch_rgb_color_t bgcolor;
	switch_rgb_color_t mask;
	switch_core_session_t *session;
} chromakey_context_t;

static void init_context(chromakey_context_t *context)
{
	switch_color_set_rgb(&context->bgcolor, "#000000");
	switch_color_set_rgb(&context->mask, "#FFFFFF");
	context->threshold = 300;
}

static void uninit_context(chromakey_context_t *context)
{
	switch_img_free(&context->bgimg);
}

static void parse_params(chromakey_context_t *context, int start, int argc, char **argv, const char **function, switch_media_bug_flag_t *flags)
{
	int n = argc - start;
	int i = start;

	if (n > 0 && argv[i]) { // color
		switch_color_set_rgb(&context->mask, argv[i]);
	}

	i++;

	if (n > 1 && argv[i]) { // thresh
		int thresh = atoi(argv[i]);

		if (thresh > 0) context->threshold = thresh;
	}

	i++;

	if (n > 2 && argv[i]) {
		if (argv[i][0] == '#') { // bgcolor
			switch_color_set_rgb(&context->bgcolor, argv[i]);
		} else {
			if (!context->bgimg) {
				context->bgimg = switch_img_read_png(argv[i], SWITCH_IMG_FMT_ARGB);
			}
		}
	}

	if (n > 3 && argv[i]) {
		if (!strcasecmp(argv[i], "patch")) {
			*function = "patch:video";
			*flags = SMBF_VIDEO_PATCH;
		}
	}

	i++;
}

static switch_status_t video_thread_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	chromakey_context_t *context = (chromakey_context_t *)user_data;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_image_t *img = NULL;
	void *data = NULL;

	if (!switch_channel_ready(channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!frame->img) {
		return SWITCH_STATUS_SUCCESS;
	}

	data = malloc(frame->img->d_w * frame->img->d_h * 4);
	switch_assert(data);

	switch_img_to_raw(frame->img, data, frame->img->d_w * 4, SWITCH_IMG_FMT_ARGB);
	img = switch_img_wrap(NULL, SWITCH_IMG_FMT_ARGB, frame->img->d_w, frame->img->d_h, 1, data);
	switch_assert(img);
	switch_img_chromakey(img, &context->mask, context->threshold);

	if (context->bgimg) {
		switch_img_patch(frame->img, context->bgimg, 0, 0);
	} else {
		switch_img_fill(frame->img, 0, 0, img->d_w, img->d_h, &context->bgcolor);
	}

	switch_img_patch(frame->img, img, 0, 0);
	switch_img_free(&img);
	free(data);

	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t chromakey_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	chromakey_context_t *context = (chromakey_context_t *)user_data;

	switch_channel_t *channel = switch_core_session_get_channel(context->session);

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
			uninit_context(context);
		}
		break;
	case SWITCH_ABC_TYPE_READ_VIDEO_PING:
	case SWITCH_ABC_TYPE_VIDEO_PATCH:
		{
			switch_frame_t *frame = switch_core_media_bug_get_video_ping_frame(bug);
			video_thread_callback(context->session, frame, context);
		}
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

#define CHROMAKEY_APP_SYNTAX "<#mask_color> [threshold] [#bg_color|path/to/image.png]"
SWITCH_STANDARD_APP(chromakey_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *argv[4] = { 0 };
	int argc;
	char *lbuf;
	switch_media_bug_flag_t flags = SMBF_READ_VIDEO_PING | SMBF_READ_VIDEO_PATCH;
	const char *function = "chromakey";
	chromakey_context_t *context;

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_chromakey_bug_"))) {
		if (!zstr(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, "_chromakey_bug_", NULL);
			switch_core_media_bug_remove(session, &bug);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 chromakey at once on the same channel!\n");
		}
		return;
	}

	switch_channel_wait_for_flag(channel, CF_VIDEO_READY, SWITCH_TRUE, 10000, NULL);

	context = (chromakey_context_t *) switch_core_session_alloc(session, sizeof(*context));
	switch_assert(context != NULL);
	memset(context, 0, sizeof(*context));
	init_context(context);
	context->session = session;

    if (data && (lbuf = switch_core_session_strdup(session, data))
        && (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
        parse_params(context, 1, argc, argv, &function, &flags);
    }

	switch_thread_rwlock_rdlock(MODULE_INTERFACE->rwlock);

	if ((status = switch_core_media_bug_add(session, function, NULL, chromakey_bug_callback, context, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure!\n");
		switch_thread_rwlock_unlock(MODULE_INTERFACE->rwlock);
		return;
	}

	switch_channel_set_private(channel, "_chromakey_bug_", bug);
}

/* API Interface Function */
#define CHROMAKEY_API_SYNTAX "<uuid> [start|stop] " CHROMAKEY_APP_SYNTAX
SWITCH_STANDARD_API(chromakey_api_function)
{
	switch_core_session_t *rsession = NULL;
	switch_channel_t *channel = NULL;
	switch_media_bug_t *bug;
	switch_status_t status;
	chromakey_context_t *context;
	char *mycmd = NULL;
	int argc = 0;
	char *argv[25] = { 0 };
	char *uuid = NULL;
	char *action = NULL;
	switch_media_bug_flag_t flags = SMBF_READ_VIDEO_PING | SMBF_READ_VIDEO_PATCH;
	const char *function = "chromakey";

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto usage;
	}

	uuid = argv[0];
	action = argv[1];

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		goto done;
	}

	channel = switch_core_session_get_channel(rsession);

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_chromakey_bug_"))) {
		if (!zstr(action)) {
			if (!strcasecmp(action, "stop")) {
				switch_channel_set_private(channel, "_chromakey_bug_", NULL);
				switch_core_media_bug_remove(rsession, &bug);
				stream->write_function(stream, "+OK Success\n");
			} else if (!strcasecmp(action, "start")) {
				context = (chromakey_context_t *) switch_core_media_bug_get_user_data(bug);
				switch_assert(context);
				parse_params(context, 2, argc, argv, &function, &flags);
				stream->write_function(stream, "+OK Success\n");
			}
		} else {
			stream->write_function(stream, "-ERR Invalid action\n");
		}
		goto done;
	}

	if (!zstr(action) && strcasecmp(action, "start")) {
		goto usage;
	}

	context = (chromakey_context_t *) switch_core_session_alloc(rsession, sizeof(*context));
	switch_assert(context != NULL);
	context->session = rsession;

	init_context(context);
	parse_params(context, 2, argc, argv, &function, &flags);

	switch_thread_rwlock_rdlock(MODULE_INTERFACE->rwlock);

	if ((status = switch_core_media_bug_add(rsession, function, NULL,
											chromakey_bug_callback, context, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR Failure!\n");
		switch_thread_rwlock_unlock(MODULE_INTERFACE->rwlock);
		goto done;
	} else {
		switch_channel_set_private(channel, "_chromakey_bug_", bug);
		stream->write_function(stream, "+OK Success\n");
		goto done;
	}

 usage:
	stream->write_function(stream, "-USAGE: %s\n", CHROMAKEY_API_SYNTAX);

 done:
	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_video_filter_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_video_filter_load)
{
	switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	MODULE_INTERFACE = *module_interface;

	SWITCH_ADD_APP(app_interface, "chromakey", "chromakey", "chromakey bug",
				   chromakey_start_function, CHROMAKEY_APP_SYNTAX, SAF_NONE);

    SWITCH_ADD_API(api_interface, "chromakey", "chromakey", chromakey_api_function, CHROMAKEY_API_SYNTAX);

    switch_console_set_complete("add chromakey ::console::list_uuid ::[start:stop");

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
