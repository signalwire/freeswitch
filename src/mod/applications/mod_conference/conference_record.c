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

void conference_record_launch_thread(conference_obj_t *conference, char *path, int canvas_id, switch_bool_t autorec)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	conference_record_t *rec;

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
	}

	/* Create a node object */
	if (!(rec = switch_core_alloc(pool, sizeof(*rec)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return;
	}

	rec->conference = conference;
	rec->path = switch_core_strdup(pool, path);
	rec->pool = pool;
	rec->autorec = autorec;

	if (canvas_id > -1) {
		rec->canvas_id = canvas_id;
	}

	switch_mutex_lock(conference->flag_mutex);
	rec->next = conference->rec_node_head;
	conference->rec_node_head = rec;
	switch_mutex_unlock(conference->flag_mutex);

	switch_threadattr_create(&thd_attr, rec->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, conference_record_thread_run, rec, rec->pool);
}


/* stop the specified recording */
switch_status_t conference_record_stop(conference_obj_t *conference, switch_stream_handle_t *stream, char *path)
{
	conference_member_t *member = NULL;
	int count = 0;

	switch_assert(conference != NULL);
	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL) && (!path || !strcmp(path, member->rec_path))) {
			conference->record_count--;
			if (!conference_utils_test_flag(conference, CFLAG_CONF_RESTART_AUTO_RECORD) && member->rec && member->rec->autorec) {
				stream->write_function(stream, "Stopped AUTO recording file %s (Auto Recording Now Disabled)\n", member->rec_path);
				conference->auto_record = 0;
			} else {
				stream->write_function(stream, "Stopped recording file %s\n", member->rec_path);
			}

			conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);

			count++;

		}
	}


	switch_mutex_unlock(conference->member_mutex);
	return count;
}
/* stop/pause/resume the specified recording */
switch_status_t conference_record_action(conference_obj_t *conference, char *path, recording_action_type_t action)
{
	conference_member_t *member = NULL;
	int count = 0;
	//switch_file_handle_t *fh = NULL;

	switch_assert(conference != NULL);
	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next)
		{
			if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL) && (!path || !strcmp(path, member->rec_path)))
				{
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,	"Action: %d\n", action);
					switch (action)
						{
						case REC_ACTION_STOP:
							conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);
							count++;
							break;
						case REC_ACTION_PAUSE:
							conference_utils_member_set_flag_locked(member, MFLAG_PAUSE_RECORDING);
							count = 1;
							break;
						case REC_ACTION_RESUME:
							conference_utils_member_clear_flag_locked(member, MFLAG_PAUSE_RECORDING);
							count = 1;
							break;
						}
				}
		}
	switch_mutex_unlock(conference->member_mutex);
	return count;
}


/* Sub-Routine called by a record entity inside a conference */
void *SWITCH_THREAD_FUNC conference_record_thread_run(switch_thread_t *thread, void *obj)
{
	int16_t *data_buf;
	conference_member_t smember = { 0 }, *member;
	conference_record_t *rp, *last = NULL, *rec = (conference_record_t *) obj;
	conference_obj_t *conference = rec->conference;
	uint32_t samples = switch_samples_per_packet(conference->rate, conference->interval);
	uint32_t mux_used;
	char *vval;
	switch_timer_t timer = { 0 };
	uint32_t rlen;
	switch_size_t data_buf_len;
	switch_event_t *event;
	switch_size_t len = 0;
	int flags = 0;
	mcu_canvas_t *canvas = NULL;

	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return NULL;
	}

	data_buf_len = samples * sizeof(int16_t) * conference->channels;
	switch_zmalloc(data_buf, data_buf_len);

	switch_mutex_lock(conference_globals.hash_mutex);
	conference_globals.threads++;
	switch_mutex_unlock(conference_globals.hash_mutex);

	member = &smember;

	member->flags[MFLAG_CAN_HEAR] = member->flags[MFLAG_NOCHANNEL] = member->flags[MFLAG_RUNNING] = 1;

	member->conference = conference;
	member->native_rate = conference->rate;
	member->rec = rec;
	member->rec_path = rec->path;
	member->rec_time = switch_epoch_time_now(NULL);
	member->rec->fh.channels = 1;
	member->rec->fh.samplerate = conference->rate;
	member->id = next_member_id();
	member->pool = rec->pool;
	member->frame_size = SWITCH_RECOMMENDED_BUFFER_SIZE;
	member->frame = switch_core_alloc(member->pool, member->frame_size);
	member->mux_frame = switch_core_alloc(member->pool, member->frame_size);
	
	if (conference->canvases[0]) {
		member->canvas_id = rec->canvas_id;
		canvas = conference->canvases[member->canvas_id];
		canvas->recording++;
		canvas->send_keyframe = 1;
	}

	switch_mutex_init(&member->write_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->flag_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->fnode_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_in_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_out_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->read_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_thread_rwlock_create(&member->rwlock, rec->pool);

	/* Setup an audio buffer for the incoming audio */
	if (switch_buffer_create_dynamic(&member->audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (switch_buffer_create_dynamic(&member->mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	member->rec->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;

	flags = SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT;

	if (conference->members_with_video && conference_utils_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		flags |= SWITCH_FILE_FLAG_VIDEO;
		if (canvas) {
			char *orig_path = rec->path;
			rec->path = switch_core_sprintf(rec->pool, "{channels=%d,samplerate=%d,vw=%d,vh=%d,fps=%0.2f}%s",
											conference->channels,
											conference->rate,
											canvas->width,
											canvas->height,
											conference->video_fps.fps,
											orig_path);
		}
	}

	if (switch_core_file_open(&member->rec->fh, rec->path, (uint8_t) conference->channels, conference->rate, flags, rec->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening File [%s]\n", rec->path);

		if (test_eflag(conference, EFLAG_RECORD) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-recording");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Error", "File could not be opened for recording");
			switch_event_fire(&event);
		}

		goto end;
	}

	switch_mutex_lock(conference->mutex);
	if (conference->video_floor_holder) {
		conference_member_t *member;
		if ((member = conference_member_get(conference, conference->video_floor_holder))) {
			if (member->session) {
				switch_core_session_video_reinit(member->session);
			}
			switch_thread_rwlock_unlock(member->rwlock);
		}
	}
	switch_mutex_unlock(conference->mutex);

	if (switch_core_timer_init(&timer, conference->timer_name, conference->interval, samples, rec->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup timer success interval: %u  samples: %u\n", conference->interval, samples);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		goto end;
	}

	if ((vval = switch_mprintf("Conference %s", conference->name))) {
		switch_core_file_set_string(&member->rec->fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_safe_free(vval);
	}

	switch_core_file_set_string(&member->rec->fh, SWITCH_AUDIO_COL_STR_ARTIST, "FreeSWITCH mod_conference Software Conference Module");

	if (test_eflag(conference, EFLAG_RECORD) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-recording");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
		switch_event_fire(&event);
	}

	if (conference_member_add(conference, member) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Joining Conference\n");
		goto end;
	}

	while (conference_utils_member_test_flag(member, MFLAG_RUNNING) && conference_utils_test_flag(conference, CFLAG_RUNNING) && (conference->count + conference->count_ghosts)) {

		len = 0;

		mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);

		if (conference_utils_member_test_flag(member, MFLAG_FLUSH_BUFFER)) {
			if (mux_used) {
				switch_mutex_lock(member->audio_out_mutex);
				switch_buffer_zero(member->mux_buffer);
				switch_mutex_unlock(member->audio_out_mutex);
				mux_used = 0;
			}
			conference_utils_member_clear_flag_locked(member, MFLAG_FLUSH_BUFFER);
		}

	again:

		if (switch_test_flag((&member->rec->fh), SWITCH_FILE_PAUSE)) {
			conference_utils_member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			goto loop;
		}

		if (mux_used >= data_buf_len) {
			/* Flush the output buffer and write all the data (presumably muxed) to the file */
			switch_mutex_lock(member->audio_out_mutex);
			//low_count = 0;

			if ((rlen = (uint32_t) switch_buffer_read(member->mux_buffer, data_buf, data_buf_len))) {
				len = (switch_size_t) rlen / sizeof(int16_t) / conference->channels;
			}
			switch_mutex_unlock(member->audio_out_mutex);
		}

		if (len == 0) {
			mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);

			if (mux_used >= data_buf_len) {
				goto again;
			}

			memset(data_buf, 255, (switch_size_t) data_buf_len);
			len = (switch_size_t) samples;
		}

		if (!conference_utils_member_test_flag(member, MFLAG_PAUSE_RECORDING)) {
			if (!len || switch_core_file_write(&member->rec->fh, data_buf, &len) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Failed\n");
				conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);
			}
		}

	loop:

		switch_core_timer_next(&timer);
	}							/* Rinse ... Repeat */

 end:

	for(;;) {
		switch_mutex_lock(member->audio_out_mutex);
		rlen = (uint32_t) switch_buffer_read(member->mux_buffer, data_buf, data_buf_len);
		switch_mutex_unlock(member->audio_out_mutex);

		if (rlen > 0) {
			len = (switch_size_t) rlen / sizeof(int16_t)/ conference->channels;
			switch_core_file_write(&member->rec->fh, data_buf, &len);
		} else {
			break;
		}
	}

	switch_safe_free(data_buf);
	switch_core_timer_destroy(&timer);
	conference_member_del(conference, member);

	if (canvas) {
		canvas->send_keyframe = 1;
	}

	switch_buffer_destroy(&member->audio_buffer);
	switch_buffer_destroy(&member->mux_buffer);
	conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);
	if (switch_test_flag((&member->rec->fh), SWITCH_FILE_OPEN)) {
		switch_mutex_lock(conference->mutex);
		switch_mutex_unlock(conference->mutex);
		switch_core_file_close(&member->rec->fh);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Recording of %s Stopped\n", rec->path);
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-recording");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Other-Recordings", conference->record_count ? "true" : "false");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Samples-Out", "%ld", (long) member->rec->fh.samples_out);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Samplerate", "%ld", (long) member->rec->fh.samplerate);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Milliseconds-Elapsed", "%ld", (long) member->rec->fh.samples_out / (member->rec->fh.samplerate / 1000));
		switch_event_fire(&event);
	}

	if (rec->autorec && conference->auto_recording) {
		conference->auto_recording--;
	}

	if (canvas) {
		canvas->recording--;
	}

	switch_mutex_lock(conference->flag_mutex);
	for (rp = conference->rec_node_head; rp; rp = rp->next) {
		if (rec == rp) {
			if (last) {
				last->next = rp->next;
			} else {
				conference->rec_node_head = rp->next;
			}
		}
	}
	switch_mutex_unlock(conference->flag_mutex);


	if (rec->pool) {
		switch_memory_pool_t *pool = rec->pool;
		rec = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(conference_globals.hash_mutex);
	conference_globals.threads--;
	switch_mutex_unlock(conference_globals.hash_mutex);

	switch_thread_rwlock_unlock(conference->rwlock);
	return NULL;
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
