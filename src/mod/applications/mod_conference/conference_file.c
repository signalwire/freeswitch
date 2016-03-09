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
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <mod_conference.h>


switch_status_t conference_file_close(conference_obj_t *conference, conference_file_node_t *node)
{
	switch_event_t *event;
	conference_member_t *member = NULL;

	if (test_eflag(conference, EFLAG_PLAY_FILE_DONE) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {

		conference_event_add_data(conference, event);

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "seconds", "%ld", (long) node->fh.samples_in / node->fh.native_rate);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "milliseconds", "%ld", (long) node->fh.samples_in / (node->fh.native_rate / 1000));
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "samples", "%ld", (long) node->fh.samples_in);

		if (node->layer_id && node->layer_id > -1) {
			if (node->canvas_id < 0) node->canvas_id = 0;
			conference_video_canvas_del_fnode_layer(conference, node);
		}

		if (node->fh.params) {
			switch_event_merge(event, node->fh.params);
		}

		if (node->member_id) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-member-done");

			if ((member = conference_member_get(conference, node->member_id))) {
				conference_member_add_event_data(member, event);
				switch_thread_rwlock_unlock(member->rwlock);
			}

		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-done");
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", node->file);

		if (node->async) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", "true");
		}

		switch_event_fire(&event);
	}

#ifdef OPENAL_POSITIONING
	if (node->al && node->al->device) {
		conference_al_close(node->al);
	}
#endif
	if (conference->playing_video_file && switch_core_file_has_video(&node->fh, SWITCH_FALSE) && conference->canvases[0] && node->canvas_id > -1) {
		if (conference->canvases[node->canvas_id]->timer.timer_interface) {
			conference->canvases[node->canvas_id]->timer.interval = conference->video_fps.ms;
			conference->canvases[node->canvas_id]->timer.samples = conference->video_fps.samples;
			switch_core_timer_sync(&conference->canvases[node->canvas_id]->timer);
			conference->canvases[node->canvas_id]->send_keyframe = 1;
		}
		conference->playing_video_file = 0;
	}
	return switch_core_file_close(&node->fh);
}


/* Make files stop playing in a conference either the current one or all of them */
uint32_t conference_file_stop(conference_obj_t *conference, file_stop_t stop)
{
	uint32_t count = 0;
	conference_file_node_t *nptr;

	switch_assert(conference != NULL);

	switch_mutex_lock(conference->mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = conference->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}

		if (conference->async_fnode) {
			conference->async_fnode->done++;
			count++;
		}
	} else if (stop == FILE_STOP_ASYNC) {
		if (conference->async_fnode) {
			conference->async_fnode->done++;
			count++;
		}
	} else {
		if (conference->fnode) {
			conference->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(conference->mutex);

	return count;
}

/* Play a file in the conference room */
switch_status_t conference_file_play(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel, uint8_t async)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	conference_file_node_t *fnode, *nptr = NULL;
	switch_memory_pool_t *pool;
	uint32_t count;
	char *dfile = NULL, *expanded = NULL;
	int say = 0;
	uint8_t channels = (uint8_t) conference->channels;
	int bad_params = 0;
	int flags = 0;

	switch_assert(conference != NULL);

	if (zstr(file)) {
		return SWITCH_STATUS_NOTFOUND;
	}

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

	if (channel) {
		if ((expanded = switch_channel_expand_variables(channel, file)) != file) {
			file = expanded;
		} else {
			expanded = NULL;
		}
	}

	if (!strncasecmp(file, "say:", 4)) {
		say = 1;
	}

	if (!async && say) {
		status = conference_say(conference, file + 4, leadin);
		goto done;
	}

	if (!switch_is_file_path(file)) {
		if (!say && conference->sound_prefix) {
			char *params_portion = NULL;
			char *file_portion = NULL;
			switch_separate_file_params(file, &file_portion, &params_portion);

			if (params_portion) {
				dfile = switch_mprintf("%s%s%s%s", params_portion, conference->sound_prefix, SWITCH_PATH_SEPARATOR, file_portion);
			} else {
				dfile = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, file_portion);
			}

			file = dfile;
			switch_safe_free(file_portion);
			switch_safe_free(params_portion);

		} else if (!async) {
			status = conference_say(conference, file, leadin);
			goto done;
		} else {
			goto done;
		}
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	fnode->conference = conference;
	fnode->layer_id = -1;
	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;

	if (switch_stristr("position=", file)) {
		/* positional requires mono input */
		fnode->fh.channels = channels = 1;
	}

 retry:

	flags = SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT;

	if (conference->members_with_video && conference_utils_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		flags |= SWITCH_FILE_FLAG_VIDEO;
	}

	/* Open the file */
	fnode->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;
	fnode->fh.mm.fps = conference->video_fps.fps;

	if (switch_core_file_open(&fnode->fh, file, channels, conference->rate, flags, pool) != SWITCH_STATUS_SUCCESS) {
		switch_event_t *event;

		if (test_eflag(conference, EFLAG_PLAY_FILE) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);

			if (fnode->fh.params) {
				switch_event_merge(event, conference->fnode->fh.params);
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", file);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", async ? "true" : "false");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Error", "File could not be played");
			switch_event_fire(&event);
		}

		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_NOTFOUND;
		goto done;
	}

	if (fnode->fh.params) {
		const char *vol = switch_event_get_header(fnode->fh.params, "vol");
		const char *position = switch_event_get_header(fnode->fh.params, "position");
		const char *canvasstr = switch_event_get_header(fnode->fh.params, "canvas");
		int canvas_id = -1;

		if (canvasstr) {
			canvas_id = atoi(canvasstr) - 1;
		}

		if (canvas_id > -1 && canvas_id < MAX_CANVASES) {
			fnode->canvas_id = canvas_id;
		}

		if (!zstr(vol)) {
			fnode->fh.vol = atoi(vol);
		}

		if (!bad_params && !zstr(position) && conference->channels == 2) {
			fnode->al = conference_al_create(pool);
			if (conference_al_parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&fnode->fh);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Position Data.\n");
				fnode->al = NULL;
				channels = (uint8_t)conference->channels;
				bad_params = 1;
				goto retry;
			}
		}
	}

	fnode->pool = pool;
	fnode->async = async;
	fnode->file = switch_core_strdup(fnode->pool, file);

	if (!conference->fnode || (async && !conference->async_fnode)) {
		conference_video_fnode_check(fnode, -1);
	}

	/* Queue the node */
	switch_mutex_lock(conference->mutex);

	if (async) {
		if (conference->async_fnode) {
			nptr = conference->async_fnode;
		}
		conference->async_fnode = fnode;

		if (nptr) {
			switch_memory_pool_t *tmppool;
			conference_file_close(conference, nptr);
			tmppool = nptr->pool;
			switch_core_destroy_memory_pool(&tmppool);
		}

	} else {
		for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

		if (nptr) {
			nptr->next = fnode;
		} else {
			conference->fnode = fnode;
		}
	}

	switch_mutex_unlock(conference->mutex);

 done:

	switch_safe_free(expanded);
	switch_safe_free(dfile);

	return status;
}

/* Play a file */
switch_status_t conference_file_local_play(conference_obj_t *conference, switch_core_session_t *session, char *path, uint32_t leadin, void *buf,
										   uint32_t buflen)
{
	uint32_t x = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel;
	char *expanded = NULL;
	switch_input_args_t args = { 0 }, *ap = NULL;

	if (buf) {
		args.buf = buf;
		args.buflen = buflen;
		ap = &args;
	}

	/* generate some space infront of the file to be played */
	for (x = 0; x < leadin; x++) {
		switch_frame_t *read_frame;
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
	}

	/* if all is well, really play the file */
	if (status == SWITCH_STATUS_SUCCESS) {
		char *dpath = NULL;

		channel = switch_core_session_get_channel(session);
		if ((expanded = switch_channel_expand_variables(channel, path)) != path) {
			path = expanded;
		} else {
			expanded = NULL;
		}

		if (!strncasecmp(path, "say:", 4)) {
			if (!(conference->tts_engine && conference->tts_voice)) {
				status = SWITCH_STATUS_FALSE;
			} else {
				status = switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, path + 4, ap);
			}
			goto done;
		}

		if (!switch_is_file_path(path) && conference->sound_prefix) {
			if (!(dpath = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, path))) {
				status = SWITCH_STATUS_MEMERR;
				goto done;
			}
			path = dpath;
		}

		status = switch_ivr_play_file(session, NULL, path, ap);
		switch_safe_free(dpath);
	}

 done:
	switch_safe_free(expanded);

	return status;
}

switch_status_t conference_close_open_files(conference_obj_t *conference)
{
	int x = 0;

	switch_mutex_lock(conference->mutex);
	/* Close Unused Handles */
	if (conference->fnode) {
		conference_file_node_t *fnode, *cur;
		switch_memory_pool_t *pool;

		fnode = conference->fnode;
		while (fnode) {
			cur = fnode;
			fnode = fnode->next;

			if (cur->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, cur);
			}

			pool = cur->pool;
			switch_core_destroy_memory_pool(&pool);
			x++;
		}
		conference->fnode = NULL;
	}

	if (conference->async_fnode) {
		switch_memory_pool_t *pool;
		conference_file_close(conference, conference->async_fnode);
		pool = conference->async_fnode->pool;
		conference->async_fnode = NULL;
		switch_core_destroy_memory_pool(&pool);
		x++;
	}
	switch_mutex_unlock(conference->mutex);

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
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
