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

SWITCH_MODULE_LOAD_FUNCTION(mod_conference_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_conference_shutdown);
SWITCH_MODULE_DEFINITION(mod_conference, mod_conference_load, mod_conference_shutdown, NULL);
SWITCH_STANDARD_APP(conference_function);

const char *mod_conference_app_name = "conference";
char *mod_conference_cf_name = "conference.conf";
conference_globals_t conference_globals = {0};
int EC = 0;
char *api_syntax = NULL;

SWITCH_STANDARD_API(conference_api_main){
	return conference_api_main_real(cmd, session, stream);
}

/* Return a Distinct ID # */
uint32_t next_member_id(void)
{
	uint32_t id;

	switch_mutex_lock(conference_globals.id_mutex);
	id = ++conference_globals.id_pool;
	switch_mutex_unlock(conference_globals.id_mutex);

	return id;
}

void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	switch_assert(delim != NULL);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		char *uuid;
		char *name;
		uint32_t count = 0;

		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		uuid = switch_core_session_get_uuid(member->session);
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);
		name = switch_channel_get_name(channel);

		stream->write_function(stream, "%u%s%s%s%s%s%s%s%s%s",
							   member->id, delim, name, delim, uuid, delim, profile->caller_id_name, delim, profile->caller_id_number, delim);

		if (conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
			stream->write_function(stream, "hear");
			count++;
		}

		if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "speak");
			count++;
		}

		if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "talking");
			count++;
		}

		if (switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "video");
			count++;
		}

		if (member == member->conference->floor_holder) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "floor");
			count++;
		}

		if (member->id == member->conference->video_floor_holder) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "vid-floor");
			count++;
		}

		if (conference_utils_member_test_flag(member, MFLAG_MOD)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "moderator");
			count++;
		}

		if (conference_utils_member_test_flag(member, MFLAG_GHOST)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "ghost");
			count++;
		}

		stream->write_function(stream, "%s%d%s%d%s%d%s%d\n", delim,
							   member->volume_in_level,
							   delim,
							   member->agc_volume_in_level,
							   delim, member->volume_out_level, delim, member->energy_level);
	}

	switch_mutex_unlock(conference->member_mutex);
}

void conference_send_notify(conference_obj_t *conference, const char *status, const char *call_id, switch_bool_t final)
{
	switch_event_t *event;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;

	if (!conference_utils_test_flag(conference, CFLAG_RFC4579)) {
		return;
	}

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
		event->flags |= EF_UNIQ_HEADERS;

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-event", "refer");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", call_id);

		if (final) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "final", "true");
		}


		switch_event_add_body(event, "%s", status);
		switch_event_fire(&event);
	}

	switch_safe_free(dup_domain);

}


/* Main monitor thread (1 per distinct conference room) */
void *SWITCH_THREAD_FUNC conference_thread_run(switch_thread_t *thread, void *obj)
{
	conference_obj_t *conference = (conference_obj_t *) obj;
	conference_member_t *imember, *omember;
	uint32_t samples = switch_samples_per_packet(conference->rate, conference->interval);
	uint32_t bytes = samples * 2 * conference->channels;
	uint8_t ready = 0, total = 0;
	switch_timer_t timer = { 0 };
	switch_event_t *event;
	uint8_t *file_frame;
	uint8_t *async_file_frame;
	int16_t *bptr;
	uint32_t x = 0;
	int32_t z = 0;
	int member_score_sum = 0;
	int divisor = 0;
	conference_cdr_node_t *np;

	if (!(divisor = conference->rate / 8000)) {
		divisor = 1;
	}

	file_frame = switch_core_alloc(conference->pool, SWITCH_RECOMMENDED_BUFFER_SIZE);
	async_file_frame = switch_core_alloc(conference->pool, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (switch_core_timer_init(&timer, conference->timer_name, conference->interval, samples, conference->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup timer success interval: %u  samples: %u\n", conference->interval, samples);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		return NULL;
	}

	switch_mutex_lock(conference_globals.hash_mutex);
	conference_globals.threads++;
	switch_mutex_unlock(conference_globals.hash_mutex);

	conference->auto_recording = 0;
	conference->record_count = 0;

	while (conference_globals.running && !conference_utils_test_flag(conference, CFLAG_DESTRUCT)) {
		switch_size_t file_sample_len = samples;
		switch_size_t file_data_len = samples * 2 * conference->channels;
		int has_file_data = 0, members_with_video = 0, members_with_avatar = 0;
		uint32_t conference_energy = 0;
		int nomoh = 0;
		conference_member_t *floor_holder;

		/* Sync the conference to a single timing source */
		if (switch_core_timer_next(&timer) != SWITCH_STATUS_SUCCESS) {
			conference_utils_set_flag(conference, CFLAG_DESTRUCT);
			break;
		}

		switch_mutex_lock(conference->mutex);
		has_file_data = ready = total = 0;

		floor_holder = conference->floor_holder;

		/* Read one frame of audio from each member channel and save it for redistribution */
		for (imember = conference->members; imember; imember = imember->next) {
			uint32_t buf_read = 0;
			total++;
			imember->read = 0;

			if (conference_utils_member_test_flag(imember, MFLAG_RUNNING) && imember->session) {
				switch_channel_t *channel = switch_core_session_get_channel(imember->session);
				switch_media_flow_t video_media_flow;
				
				if ((!floor_holder || (imember->score_iir > SCORE_IIR_SPEAKING_MAX && (floor_holder->score_iir < SCORE_IIR_SPEAKING_MIN)))) {// &&
					//(!conference_utils_test_flag(conference, CFLAG_VID_FLOOR) || switch_channel_test_flag(channel, CF_VIDEO))) {
					floor_holder = imember;
				}

				video_media_flow = switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO);

				if (video_media_flow != imember->video_media_flow) {
					imember->video_media_flow = video_media_flow;

					if (imember->video_media_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
						conference_utils_member_clear_flag(imember, MFLAG_CAN_BE_SEEN);
						conference_video_find_floor(imember, SWITCH_FALSE);
					} else {
						conference_utils_member_set_flag(imember, MFLAG_CAN_BE_SEEN);
						conference_video_find_floor(imember, SWITCH_TRUE);
						switch_core_session_request_video_refresh(imember->session);
					}
				}

				if (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_VIDEO_READY) && imember->video_media_flow != SWITCH_MEDIA_FLOW_SENDONLY && (!conference_utils_test_flag(conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) || conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN))) {
					members_with_video++;
				}

				if (imember->avatar_png_img && !switch_channel_test_flag(channel, CF_VIDEO)) {
					members_with_avatar++;
				}

				if (conference_utils_member_test_flag(imember, MFLAG_NOMOH)) {
					nomoh++;
				}
			}

			conference_utils_member_clear_flag_locked(imember, MFLAG_HAS_AUDIO);
			switch_mutex_lock(imember->audio_in_mutex);

			if (switch_buffer_inuse(imember->audio_buffer) >= bytes
				&& (buf_read = (uint32_t) switch_buffer_read(imember->audio_buffer, imember->frame, bytes))) {
				imember->read = buf_read;
				conference_utils_member_set_flag_locked(imember, MFLAG_HAS_AUDIO);
				ready++;
			}
			switch_mutex_unlock(imember->audio_in_mutex);
		}

		conference->members_with_video = members_with_video;
		conference->members_with_avatar = members_with_avatar;

		if (floor_holder != conference->floor_holder) {
			conference_member_set_floor_holder(conference, floor_holder);
		}

		if (conference->perpetual_sound && !conference->async_fnode) {
			conference_file_play(conference, conference->perpetual_sound, CONF_DEFAULT_LEADIN, NULL, 1);
		} else if (conference->moh_sound && ((nomoh == 0 && conference->count == 1)
											 || conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) && !conference->async_fnode && !conference->fnode) {
			conference_file_play(conference, conference->moh_sound, CONF_DEFAULT_LEADIN, NULL, 1);
		}


		/* Find if no one talked for more than x number of second */
		if (conference->terminate_on_silence && conference->count > 1) {
			int is_talking = 0;

			for (imember = conference->members; imember; imember = imember->next) {
				if (switch_epoch_time_now(NULL) - imember->join_time <= conference->terminate_on_silence) {
					is_talking++;
				} else if (imember->last_talking != 0 && switch_epoch_time_now(NULL) - imember->last_talking <= conference->terminate_on_silence) {
					is_talking++;
				}
			}
			if (is_talking == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Conference has been idle for over %d seconds, terminating\n", conference->terminate_on_silence);
				conference_utils_set_flag(conference, CFLAG_DESTRUCT);
			}
		}

		/* Start auto recording if there's the minimum number of required participants. */
		if (conference->auto_record && !conference->auto_recording && (conference->count >= conference->min_recording_participants)) {
			conference->auto_recording++;
			conference->record_count++;
			imember = conference->members;
			if (imember) {
				switch_channel_t *channel = switch_core_session_get_channel(imember->session);
				char *rfile = switch_channel_expand_variables(channel, conference->auto_record);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Auto recording file: %s\n", rfile);
				conference_record_launch_thread(conference, rfile, -1, SWITCH_TRUE);

				if (rfile != conference->auto_record) {
					conference->record_filename = switch_core_strdup(conference->pool, rfile);
					switch_safe_free(rfile);
				} else {
					conference->record_filename = switch_core_strdup(conference->pool, conference->auto_record);
				}

				/* Set the conference recording variable for each member */
				for (omember = conference->members; omember; omember = omember->next) {
					if (!omember->session) continue;
					channel = switch_core_session_get_channel(omember->session);
					switch_channel_set_variable(channel, "conference_recording", conference->record_filename);
					switch_channel_set_variable_printf(channel, "conference_recording_canvas", "%d", conference->auto_record_canvas + 1);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Auto Record Failed.  No members in conference.\n");
			}
		}

		/* If a file or speech event is being played */
		if (conference->fnode && !switch_test_flag(conference->fnode, NFLAG_PAUSE)) {
			/* Lead in time */
			if (conference->fnode->leadin) {
				conference->fnode->leadin--;
			} else if (!conference->fnode->done) {
				file_sample_len = samples;

				if (conference->fnode->type == NODE_TYPE_SPEECH) {
					switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
					switch_size_t speech_len = file_data_len;

					if (conference->fnode->al) {
						speech_len /= 2;
					}

					if (switch_core_speech_read_tts(conference->fnode->sh, file_frame, &speech_len, &flags) == SWITCH_STATUS_SUCCESS) {

						if (conference->fnode->al) {
							conference_al_process(conference->fnode->al, file_frame, speech_len, conference->rate);
						}

						file_sample_len = file_data_len / 2 / conference->fnode->sh->channels;


					} else {
						file_sample_len = file_data_len = 0;
					}
				} else if (conference->fnode->type == NODE_TYPE_FILE) {
					switch_core_file_read(&conference->fnode->fh, file_frame, &file_sample_len);
					if (conference->fnode->fh.vol) {
						switch_change_sln_volume_granular((void *)file_frame, (uint32_t)file_sample_len * conference->fnode->fh.channels,
														  conference->fnode->fh.vol);
					}
					if (conference->fnode->al) {
						conference_al_process(conference->fnode->al, file_frame, file_sample_len * 2, conference->fnode->fh.samplerate);
					}
				}

				if (file_sample_len <= 0) {
					conference->fnode->done++;
				} else {
					has_file_data = 1;
				}
			}
		}

		if (conference->async_fnode) {
			/* Lead in time */
			if (conference->async_fnode->leadin) {
				conference->async_fnode->leadin--;
			} else if (!conference->async_fnode->done) {
				file_sample_len = samples;
				switch_core_file_read(&conference->async_fnode->fh, async_file_frame, &file_sample_len);
				if (conference->async_fnode->al) {
					conference_al_process(conference->async_fnode->al, file_frame, file_sample_len * 2, conference->async_fnode->fh.samplerate);
				}
				if (file_sample_len <= 0) {
					conference->async_fnode->done++;
				} else {
					if (has_file_data) {
						switch_size_t x;
						for (x = 0; x < file_sample_len * conference->channels; x++) {
							int32_t z;
							int16_t *muxed;

							muxed = (int16_t *) file_frame;
							bptr = (int16_t *) async_file_frame;
							z = muxed[x] + bptr[x];
							switch_normalize_to_16bit(z);
							muxed[x] = (int16_t) z;
						}
					} else {
						memcpy(file_frame, async_file_frame, file_sample_len * 2 * conference->channels);
						has_file_data = 1;
					}
				}
			}
		}

		if (ready || has_file_data) {
			/* Use more bits in the main_frame to preserve the exact sum of the audio samples. */
			int main_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			int16_t write_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };


			/* Init the main frame with file data if there is any. */
			bptr = (int16_t *) file_frame;
			if (has_file_data && file_sample_len) {

				for (x = 0; x < bytes / 2; x++) {
					if (x <= file_sample_len * conference->channels) {
						main_frame[x] = (int32_t) bptr[x];
					} else {
						memset(&main_frame[x], 255, sizeof(main_frame[x]));
					}
				}
			}

			member_score_sum = 0;
			conference->mux_loop_count = 0;
			conference->member_loop_count = 0;


			/* Copy audio from every member known to be producing audio into the main frame. */
			for (omember = conference->members; omember; omember = omember->next) {
				conference->member_loop_count++;

				if (!(conference_utils_member_test_flag(omember, MFLAG_RUNNING) && conference_utils_member_test_flag(omember, MFLAG_HAS_AUDIO))) {
					continue;
				}

				if (conference->agc_level) {
					if (conference_utils_member_test_flag(omember, MFLAG_TALKING) && conference_utils_member_test_flag(omember, MFLAG_CAN_SPEAK)) {
						member_score_sum += omember->score;
						conference->mux_loop_count++;
					}
				}

				bptr = (int16_t *) omember->frame;
				for (x = 0; x < omember->read / 2; x++) {
					main_frame[x] += (int32_t) bptr[x];
				}
			}

			if (conference->agc_level && conference->member_loop_count) {
				conference_energy = 0;

				for (x = 0; x < bytes / 2; x++) {
					z = abs(main_frame[x]);
					switch_normalize_to_16bit(z);
					conference_energy += (int16_t) z;
				}

				conference->score = conference_energy / ((bytes / 2) / divisor) / conference->member_loop_count;
				conference->avg_tally += conference->score;
				conference->avg_score = conference->avg_tally / ++conference->avg_itt;
				if (!conference->avg_itt) conference->avg_tally = conference->score;
			}

			/* Create write frame once per member who is not deaf for each sample in the main frame
			   check if our audio is involved and if so, subtract it from the sample so we don't hear ourselves.
			   Since main frame was 32 bit int, we did not lose any detail, now that we have to convert to 16 bit we can
			   cut it off at the min and max range if need be and write the frame to the output buffer.
			*/
			for (omember = conference->members; omember; omember = omember->next) {
				switch_size_t ok = 1;

				if (!conference_utils_member_test_flag(omember, MFLAG_RUNNING)) {
					continue;
				}

				if (!conference_utils_member_test_flag(omember, MFLAG_CAN_HEAR)) {
					switch_mutex_lock(omember->audio_out_mutex);
					memset(write_frame, 255, bytes);
					ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
					switch_mutex_unlock(omember->audio_out_mutex);
					continue;
				}

				bptr = (int16_t *) omember->frame;

				for (x = 0; x < bytes / 2 ; x++) {
					z = main_frame[x];

					/* bptr[x] represents my own contribution to this audio sample */
					if (conference_utils_member_test_flag(omember, MFLAG_HAS_AUDIO) && x <= omember->read / 2) {
						z -= (int32_t) bptr[x];
					}

					/* when there are relationships, we have to do more work by scouring all the members to see if there are any
					   reasons why we should not be hearing a paticular member, and if not, delete their samples as well.
					*/
					if (conference->relationship_total) {
						for (imember = conference->members; imember; imember = imember->next) {
							if (imember != omember && conference_utils_member_test_flag(imember, MFLAG_HAS_AUDIO)) {
								conference_relationship_t *rel;
								switch_size_t found = 0;
								int16_t *rptr = (int16_t *) imember->frame;
								for (rel = imember->relationships; rel; rel = rel->next) {
									if ((rel->id == omember->id || rel->id == 0) && !switch_test_flag(rel, RFLAG_CAN_SPEAK)) {
										z -= (int32_t) rptr[x];
										found = 1;
										break;
									}
								}
								if (!found) {
									for (rel = omember->relationships; rel; rel = rel->next) {
										if ((rel->id == imember->id || rel->id == 0) && !switch_test_flag(rel, RFLAG_CAN_HEAR)) {
											z -= (int32_t) rptr[x];
											break;
										}
									}
								}

							}
						}
					}

					/* Now we can convert to 16 bit. */
					switch_normalize_to_16bit(z);
					write_frame[x] = (int16_t) z;
				}

				switch_mutex_lock(omember->audio_out_mutex);
				ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
				switch_mutex_unlock(omember->audio_out_mutex);

				if (!ok) {
					switch_mutex_unlock(conference->mutex);
					goto end;
				}
			}
		} else { /* There is no source audio.  Push silence into all of the buffers */
			int16_t write_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };

			if (conference->comfort_noise_level) {
				switch_generate_sln_silence(write_frame, samples, conference->channels, conference->comfort_noise_level);
			} else {
				memset(write_frame, 255, bytes);
			}

			for (omember = conference->members; omember; omember = omember->next) {
				switch_size_t ok = 1;

				if (!conference_utils_member_test_flag(omember, MFLAG_RUNNING)) {
					continue;
				}

				switch_mutex_lock(omember->audio_out_mutex);
				ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
				switch_mutex_unlock(omember->audio_out_mutex);

				if (!ok) {
					switch_mutex_unlock(conference->mutex);
					goto end;
				}
			}
		}

		if (conference->async_fnode && conference->async_fnode->done) {
			switch_memory_pool_t *pool;

			if (conference->canvases[0] && conference->async_fnode->layer_id > -1 ) {
				conference_video_canvas_del_fnode_layer(conference, conference->async_fnode);
			}

			conference_file_close(conference, conference->async_fnode);
			pool = conference->async_fnode->pool;
			conference->async_fnode = NULL;
			switch_core_destroy_memory_pool(&pool);
		}

		if (conference->fnode && conference->fnode->done) {
			conference_file_node_t *fnode;
			switch_memory_pool_t *pool;

			if (conference->canvases[0] && conference->fnode->layer_id > -1 ) {
				conference_video_canvas_del_fnode_layer(conference, conference->fnode);
			}

			if (conference->fnode->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, conference->fnode);
			}

			fnode = conference->fnode;
			conference->fnode = conference->fnode->next;

			if (conference->fnode) {
				conference_video_fnode_check(conference->fnode, -1);
			}


			pool = fnode->pool;
			fnode = NULL;
			switch_core_destroy_memory_pool(&pool);
		}

		if (!conference->end_count && conference->endconference_time &&
			switch_epoch_time_now(NULL) - conference->endconference_time > conference->endconference_grace_time) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Conference %s: endconf grace time exceeded (%u)\n",
							  conference->name, conference->endconference_grace_time);
			conference_utils_set_flag(conference, CFLAG_DESTRUCT);
			conference_utils_set_flag(conference, CFLAG_ENDCONF_FORCED);
		}

		switch_mutex_unlock(conference->mutex);
	}
	/* Rinse ... Repeat */
 end:

	if (conference_utils_test_flag(conference, CFLAG_OUTCALL)) {
		conference->cancel_cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Ending pending outcall channels for Conference: '%s'\n", conference->name);
		while(conference->originating) {
			switch_yield(200000);
		}
	}

	conference_send_presence(conference);

	switch_mutex_lock(conference->mutex);
	conference_file_stop(conference, FILE_STOP_ASYNC);
	conference_file_stop(conference, FILE_STOP_ALL);

	for (np = conference->cdr_nodes; np; np = np->next) {
		if (np->var_event) {
			switch_event_destroy(&np->var_event);
		}
	}


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
		}
		conference->fnode = NULL;
	}

	if (conference->async_fnode) {
		switch_memory_pool_t *pool;
		conference_file_close(conference, conference->async_fnode);
		pool = conference->async_fnode->pool;
		conference->async_fnode = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		switch_channel_t *channel;

		if (!conference_utils_member_test_flag(imember, MFLAG_NOCHANNEL)) {
			channel = switch_core_session_get_channel(imember->session);

			if (!switch_false(switch_channel_get_variable(channel, "hangup_after_conference"))) {
				/* add this little bit to preserve the bridge cause code in case of an early media call that */
				/* never answers */
				if (conference_utils_test_flag(conference, CFLAG_ANSWERED)) {
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				} else {
					/* put actual cause code from outbound channel hangup here */
					switch_channel_hangup(channel, conference->bridge_hangup_cause);
				}
			}
		}

		conference_utils_member_clear_flag_locked(imember, MFLAG_RUNNING);
	}
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (conference->vh[0].up == 1) {
		conference->vh[0].up = -1;
	}

	if (conference->vh[1].up == 1) {
		conference->vh[1].up = -1;
	}

	while (conference->vh[0].up || conference->vh[1].up) {
		switch_cond_next();
	}

	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
	conference_event_add_data(conference, event);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "conference-destroy");
	switch_event_fire(&event);

	switch_core_timer_destroy(&timer);
	switch_mutex_lock(conference_globals.hash_mutex);
	if (conference_utils_test_flag(conference, CFLAG_INHASH)) {
		switch_core_hash_delete(conference_globals.conference_hash, conference->name);
	}
	switch_mutex_unlock(conference_globals.hash_mutex);


	conference_utils_clear_flag(conference, CFLAG_VIDEO_MUXING);

	for (x = 0; x <= conference->canvas_count; x++) {
		if (conference->canvases[x] && conference->canvases[x]->video_muxing_thread) {
			switch_status_t st = 0;
			switch_thread_join(&st, conference->canvases[x]->video_muxing_thread);
			conference->canvases[x]->video_muxing_thread = NULL;
		}
	}

	/* Wait till everybody is out */
	conference_utils_clear_flag_locked(conference, CFLAG_RUNNING);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock ON\n");
	switch_thread_rwlock_wrlock(conference->rwlock);
	switch_thread_rwlock_unlock(conference->rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock OFF\n");

	if (conference->la) {
		switch_live_array_destroy(&conference->la);
	}

	if (conference->sh) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&conference->lsh, &flags);
		conference->sh = NULL;
	}

	conference->end_time = switch_epoch_time_now(NULL);
	conference_cdr_render(conference);

	switch_mutex_lock(conference_globals.setup_mutex);
	if (conference->layout_hash) {
		switch_core_hash_destroy(&conference->layout_hash);
	}
	switch_mutex_unlock(conference_globals.setup_mutex);

	if (conference->layout_group_hash) {
		switch_core_hash_destroy(&conference->layout_group_hash);
	}


	if (conference->pool) {
		switch_memory_pool_t *pool = conference->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(conference_globals.hash_mutex);
	conference_globals.threads--;
	switch_mutex_unlock(conference_globals.hash_mutex);

	return NULL;
}



/* Say some thing with TTS in the conference room */
switch_status_t conference_say(conference_obj_t *conference, const char *text, uint32_t leadin)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t count;
	switch_event_t *params = NULL;
	char *fp = NULL;
	int channels;
	const char *position = NULL;

	switch_assert(conference != NULL);

	channels = conference->channels;

	if (zstr(text)) {
		return SWITCH_STATUS_GENERR;
	}


	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	if (!(conference->tts_engine && conference->tts_voice)) {
		count = 0;
	}
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->conference = conference;
	fnode->layer_id = -1;

	if (*text == '{') {
		char *new_fp;

		fp = switch_core_strdup(pool, text);
		switch_assert(fp);

		if (switch_event_create_brackets(fp, '{', '}', ',', &params, &new_fp, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
			new_fp = fp;
		}

		text = new_fp;
	}


	fnode->type = NODE_TYPE_SPEECH;
	fnode->leadin = leadin;

	if (params && (position = switch_event_get_header(params, "position"))) {
		if (conference->channels != 2) {
			position = NULL;
		} else {
			channels = 1;
			fnode->al = conference_al_create(pool);
			if (conference_al_parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				fnode->al = NULL;
				channels = conference->channels;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Position Data.\n");
			}
		}
	}

	if (conference->sh && conference->last_speech_channels != channels) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&conference->lsh, &flags);
		conference->sh = NULL;
	}

	if (!conference->sh) {
		memset(&conference->lsh, 0, sizeof(conference->lsh));
		if (switch_core_speech_open(&conference->lsh, conference->tts_engine, conference->tts_voice,
									conference->rate, conference->interval, channels, &flags, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
		conference->last_speech_channels = channels;
		conference->sh = &conference->lsh;
	}

	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(conference->mutex);
	for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		conference->fnode = fnode;
	}

	fnode->sh = conference->sh;
	if (*text == '#') {
		char *tmp = (char *) text + 1;
		char *vp = tmp, voice[128] = "";
		if ((tmp = strchr(tmp, '#'))) {
			text = tmp + 1;
			switch_copy_string(voice, vp, (tmp - vp) + 1);
			switch_core_speech_text_param_tts(fnode->sh, "voice", voice);
		}
	} else {
		switch_core_speech_text_param_tts(fnode->sh, "voice", conference->tts_voice);
	}

	/* Begin Generation */
	switch_sleep(200000);
	switch_core_speech_feed_tts(fnode->sh, (char *) text, &flags);
	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

 end:

	if (params) {
		switch_event_destroy(&params);
	}

	return status;
}


switch_status_t conference_list_conferences(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;

	switch_mutex_lock(conference_globals.hash_mutex);
	for (hi = switch_core_hash_first(conference_globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);
	}
	switch_mutex_unlock(conference_globals.hash_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

void conference_list_pretty(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;

		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);

		stream->write_function(stream, "%u) %s (%s)\n", member->id, profile->caller_id_name, profile->caller_id_number);
	}

	switch_mutex_unlock(conference->member_mutex);
}


void conference_list_count_only(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	stream->write_function(stream, "%d", conference->count);
}


switch_xml_t add_x_tag(switch_xml_t x_member, const char *name, const char *value, int off)
{
	switch_size_t dlen;
	char *data;
	switch_xml_t x_tag;

	if (!value) {
		return 0;
	}

	dlen = strlen(value) * 3 + 1;

	x_tag = switch_xml_add_child_d(x_member, name, off);
	switch_assert(x_tag);

	switch_zmalloc(data, dlen);

	switch_url_encode(value, data, dlen);
	switch_xml_set_txt_d(x_tag, data);
	free(data);

	return x_tag;
}

void conference_xlist(conference_obj_t *conference, switch_xml_t x_conference, int off)
{
	conference_member_t *member = NULL;
	switch_xml_t x_member = NULL, x_members = NULL, x_flags;
	int moff = 0;
	char i[30] = "";
	char *ival = i;
	switch_assert(conference != NULL);
	switch_assert(x_conference != NULL);

	switch_xml_set_attr_d(x_conference, "name", conference->name);
	switch_snprintf(i, sizeof(i), "%d", conference->count);
	switch_xml_set_attr_d(x_conference, "member-count", ival);
	switch_snprintf(i, sizeof(i), "%d", conference->count_ghosts);
	switch_xml_set_attr_d(x_conference, "ghost-count", ival);
	switch_snprintf(i, sizeof(i), "%u", conference->rate);
	switch_xml_set_attr_d(x_conference, "rate", ival);
	switch_xml_set_attr_d(x_conference, "uuid", conference->uuid_str);

	if (conference_utils_test_flag(conference, CFLAG_LOCKED)) {
		switch_xml_set_attr_d(x_conference, "locked", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_DESTRUCT)) {
		switch_xml_set_attr_d(x_conference, "destruct", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) {
		switch_xml_set_attr_d(x_conference, "wait_mod", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_AUDIO_ALWAYS)) {
		switch_xml_set_attr_d(x_conference, "audio_always", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_RUNNING)) {
		switch_xml_set_attr_d(x_conference, "running", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_ANSWERED)) {
		switch_xml_set_attr_d(x_conference, "answered", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_ENFORCE_MIN)) {
		switch_xml_set_attr_d(x_conference, "enforce_min", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_BRIDGE_TO)) {
		switch_xml_set_attr_d(x_conference, "bridge_to", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_DYNAMIC)) {
		switch_xml_set_attr_d(x_conference, "dynamic", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_EXIT_SOUND)) {
		switch_xml_set_attr_d(x_conference, "exit_sound", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_ENTER_SOUND)) {
		switch_xml_set_attr_d(x_conference, "enter_sound", "true");
	}

	if (conference->max_members > 0) {
		switch_snprintf(i, sizeof(i), "%d", conference->max_members);
		switch_xml_set_attr_d(x_conference, "max_members", ival);
	}

	if (conference->record_count > 0) {
		switch_xml_set_attr_d(x_conference, "recording", "true");
	}

	if (conference->endconference_grace_time > 0) {
		switch_snprintf(i, sizeof(i), "%u", conference->endconference_grace_time);
		switch_xml_set_attr_d(x_conference, "endconference_grace_time", ival);
	}

	if (conference_utils_test_flag(conference, CFLAG_VID_FLOOR)) {
		switch_xml_set_attr_d(x_conference, "video_floor_only", "true");
	}

	if (conference_utils_test_flag(conference, CFLAG_RFC4579)) {
		switch_xml_set_attr_d(x_conference, "video_rfc4579", "true");
	}

	switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - conference->run_time);
	switch_xml_set_attr_d(x_conference, "run_time", ival);

	if (conference->agc_level) {
		char tmp[30] = "";
		switch_snprintf(tmp, sizeof(tmp), "%d", conference->agc_level);
		switch_xml_set_attr_d_buf(x_conference, "agc", tmp);
	}

	x_members = switch_xml_add_child_d(x_conference, "members", 0);
	switch_assert(x_members);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		char *uuid;
		//char *name;
		uint32_t count = 0;
		switch_xml_t x_tag;
		int toff = 0;
		char tmp[50] = "";

		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			if (member->rec_path) {
				x_member = switch_xml_add_child_d(x_members, "member", moff++);
				switch_assert(x_member);
				switch_xml_set_attr_d(x_member, "type", "recording_node");
				/* or:
				   x_member = switch_xml_add_child_d(x_members, "recording_node", moff++);
				*/

				x_tag = switch_xml_add_child_d(x_member, "record_path", count++);
				if (conference_utils_member_test_flag(member, MFLAG_PAUSE_RECORDING)) {
					switch_xml_set_attr_d(x_tag, "status", "paused");
				}
				switch_xml_set_txt_d(x_tag, member->rec_path);

				x_tag = switch_xml_add_child_d(x_member, "join_time", count++);
				switch_xml_set_attr_d(x_tag, "type", "UNIX-epoch");
				switch_snprintf(i, sizeof(i), "%d", member->rec_time);
				switch_xml_set_txt_d(x_tag, i);
			}
			continue;
		}

		uuid = switch_core_session_get_uuid(member->session);
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);
		//name = switch_channel_get_name(channel);


		x_member = switch_xml_add_child_d(x_members, "member", moff++);
		switch_assert(x_member);
		switch_xml_set_attr_d(x_member, "type", "caller");

		switch_snprintf(i, sizeof(i), "%d", member->id);

		add_x_tag(x_member, "id", i, toff++);
		add_x_tag(x_member, "uuid", uuid, toff++);
		add_x_tag(x_member, "caller_id_name", profile->caller_id_name, toff++);
		add_x_tag(x_member, "caller_id_number", profile->caller_id_number, toff++);


		switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - member->join_time);
		add_x_tag(x_member, "join_time", i, toff++);

		switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - member->last_talking);
		add_x_tag(x_member, "last_talking", member->last_talking ? i : "N/A", toff++);

		switch_snprintf(i, sizeof(i), "%d", member->energy_level);
		add_x_tag(x_member, "energy", i, toff++);

		switch_snprintf(i, sizeof(i), "%d", member->volume_in_level);
		add_x_tag(x_member, "volume_in", i, toff++);

		switch_snprintf(i, sizeof(i), "%d", member->volume_out_level);
		add_x_tag(x_member, "volume_out", i, toff++);

		x_flags = switch_xml_add_child_d(x_member, "flags", count++);
		switch_assert(x_flags);

		x_tag = switch_xml_add_child_d(x_flags, "can_hear", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_CAN_HEAR) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "can_speak", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "mute_detect", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "talking", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_TALKING) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "has_video", count++);
		switch_xml_set_txt_d(x_tag, switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "video_bridge", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_VIDEO_BRIDGE) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "has_floor", count++);
		switch_xml_set_txt_d(x_tag, (member == member->conference->floor_holder) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "is_moderator", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_MOD) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "end_conference", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_ENDCONF) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "is_ghost", count++);
		switch_xml_set_txt_d(x_tag, conference_utils_member_test_flag(member, MFLAG_GHOST) ? "true" : "false");

		switch_snprintf(tmp, sizeof(tmp), "%d", member->volume_out_level);
		add_x_tag(x_member, "output-volume", tmp, toff++);

		switch_snprintf(tmp, sizeof(tmp), "%d", member->agc_volume_in_level ? member->agc_volume_in_level : member->volume_in_level);
		add_x_tag(x_member, "input-volume", tmp, toff++);

		switch_snprintf(tmp, sizeof(tmp), "%d", member->agc_volume_in_level);
		add_x_tag(x_member, "auto-adjusted-input-volume", tmp, toff++);

	}

	switch_mutex_unlock(conference->member_mutex);
}

void conference_fnode_toggle_pause(conference_file_node_t *fnode, switch_stream_handle_t *stream)
{
	if (fnode) {
		if (switch_test_flag(fnode, NFLAG_PAUSE)) {
			stream->write_function(stream, "+OK Resume\n");
			switch_clear_flag(fnode, NFLAG_PAUSE);
		} else {
			stream->write_function(stream, "+OK Pause\n");
			switch_set_flag(fnode, NFLAG_PAUSE);
		}
	}
}

void conference_fnode_check_status(conference_file_node_t *fnode, switch_stream_handle_t *stream)
{
	if (fnode) {
		stream->write_function(stream, "+OK %"SWITCH_INT64_T_FMT "/%" SWITCH_INT64_T_FMT " %s\n",
			fnode->fh.vpos, fnode->fh.duration, fnode->fh.file_path);
	} else {
		stream->write_function(stream, "-ERR Nothing is playing\n");
	}
}

void conference_fnode_seek(conference_file_node_t *fnode, switch_stream_handle_t *stream, char *arg)
{
	if (fnode && fnode->type == NODE_TYPE_FILE) {
		unsigned int samps = 0;
		unsigned int pos = 0;

		if (*arg == '+' || *arg == '-') {
			int step;
			int32_t target;
			if (!(step = atoi(arg))) {
				step = 1000;
			}

			samps = step * (fnode->fh.native_rate / 1000);
			target = (int32_t)fnode->fh.pos + samps;

			if (target < 0) {
				target = 0;
			}

			stream->write_function(stream, "+OK seek to position %d\n", target);
			switch_core_file_seek(&fnode->fh, &pos, target, SEEK_SET);

		} else {
			samps = switch_atoui(arg) * (fnode->fh.native_rate / 1000);
			stream->write_function(stream, "+OK seek to position %d\n", samps);
			switch_core_file_seek(&fnode->fh, &pos, samps, SEEK_SET);
		}
	}
}


/* generate an outbound call from the conference */
switch_status_t conference_outcall(conference_obj_t *conference,
								   char *conference_name,
								   switch_core_session_t *session,
								   char *bridgeto, uint32_t timeout,
								   char *flags, char *cid_name,
								   char *cid_num,
								   char *profile,
								   switch_call_cause_t *cause,
								   switch_call_cause_t *cancel_cause, switch_event_t *var_event)
{
	switch_core_session_t *peer_session = NULL;
	switch_channel_t *peer_channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *caller_channel = NULL;
	char appdata[512];
	int rdlock = 0;
	switch_bool_t have_flags = SWITCH_FALSE;
	const char *outcall_flags;
	int track = 0;
	const char *call_id = NULL;

	if (var_event && switch_true(switch_event_get_header(var_event, "conference_track_status"))) {
		track++;
		call_id = switch_event_get_header(var_event, "conference_track_call_id");
	}

	*cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (conference == NULL) {
		char *dialstr = switch_mprintf("{ignore_early_media=true}%s", bridgeto);
		status = switch_ivr_originate(NULL, &peer_session, cause, dialstr, 60, NULL, cid_name, cid_num, NULL, var_event, SOF_NO_LIMITS, NULL);
		switch_safe_free(dialstr);

		if (status != SWITCH_STATUS_SUCCESS) {
			return status;
		}

		peer_channel = switch_core_session_get_channel(peer_session);
		rdlock = 1;
		goto callup;
	}

	conference_name = conference->name;

	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return SWITCH_STATUS_FALSE;
	}

	if (session != NULL) {
		caller_channel = switch_core_session_get_channel(session);
	}

	if (zstr(cid_name)) {
		cid_name = conference->caller_id_name;
	}

	if (zstr(cid_num)) {
		cid_num = conference->caller_id_number;
	}

	/* establish an outbound call leg */

	switch_mutex_lock(conference->mutex);
	conference->originating++;
	switch_mutex_unlock(conference->mutex);

	if (track) {
		conference_send_notify(conference, "SIP/2.0 100 Trying\r\n", call_id, SWITCH_FALSE);
	}


	status = switch_ivr_originate(session, &peer_session, cause, bridgeto, timeout, NULL, cid_name, cid_num, NULL, var_event, SOF_NO_LIMITS, cancel_cause);
	switch_mutex_lock(conference->mutex);
	conference->originating--;
	switch_mutex_unlock(conference->mutex);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot create outgoing channel, cause: %s\n",
						  switch_channel_cause2str(*cause));
		if (caller_channel) {
			switch_channel_hangup(caller_channel, *cause);
		}

		if (track) {
			conference_send_notify(conference, "SIP/2.0 481 Failure\r\n", call_id, SWITCH_TRUE);
		}

		goto done;
	}

	if (track) {
		conference_send_notify(conference, "SIP/2.0 200 OK\r\n", call_id, SWITCH_TRUE);
	}

	rdlock = 1;
	peer_channel = switch_core_session_get_channel(peer_session);

	/* make sure the conference still exists */
	if (!conference_utils_test_flag(conference, CFLAG_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Conference is gone now, nevermind..\n");
		if (caller_channel) {
			switch_channel_hangup(caller_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		}
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		goto done;
	}

	if (caller_channel && switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

 callup:

	/* if the outbound call leg is ready */
	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
		switch_caller_extension_t *extension = NULL;

		/* build an extension name object */
		if ((extension = switch_caller_extension_new(peer_session, conference_name, conference_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
			status = SWITCH_STATUS_MEMERR;
			goto done;
		}

		if ((outcall_flags = switch_channel_get_variable(peer_channel, "outcall_flags"))) {
			if (!zstr(outcall_flags)) {
				flags = (char *)outcall_flags;
			}
		}

		if (flags && strcasecmp(flags, "none")) {
			have_flags = SWITCH_TRUE;
		}
		/* add them to the conference */

		switch_snprintf(appdata, sizeof(appdata), "%s%s%s%s%s%s", conference_name,
						profile?"@":"", profile?profile:"",
						have_flags?"+flags{":"", have_flags?flags:"", have_flags?"}":"");
		switch_caller_extension_add_application(peer_session, extension, (char *) mod_conference_app_name, appdata);

		switch_channel_set_caller_extension(peer_channel, extension);
		switch_channel_set_state(peer_channel, CS_EXECUTE);

	} else {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

 done:
	if (conference) {
		switch_thread_rwlock_unlock(conference->rwlock);
	}
	if (rdlock && peer_session) {
		switch_core_session_rwunlock(peer_session);
	}

	return status;
}

void *SWITCH_THREAD_FUNC conference_outcall_run(switch_thread_t *thread, void *obj)
{
	struct bg_call *call = (struct bg_call *) obj;

	if (call) {
		switch_call_cause_t cause;
		switch_event_t *event;


		conference_outcall(call->conference, call->conference_name,
						   call->session, call->bridgeto, call->timeout,
						   call->flags, call->cid_name, call->cid_num, call->profile, &cause, call->cancel_cause, call->var_event);

		if (call->conference && test_eflag(call->conference, EFLAG_BGDIAL_RESULT) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(call->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "bgdial-result");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Result", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", call->uuid);
			switch_event_fire(&event);
		}

		if (call->var_event) {
			switch_event_destroy(&call->var_event);
		}

		switch_safe_free(call->bridgeto);
		switch_safe_free(call->flags);
		switch_safe_free(call->cid_name);
		switch_safe_free(call->cid_num);
		switch_safe_free(call->conference_name);
		switch_safe_free(call->uuid);
		switch_safe_free(call->profile);
		if (call->pool) {
			switch_core_destroy_memory_pool(&call->pool);
		}
		switch_safe_free(call);
	}

	return NULL;
}

switch_status_t conference_outcall_bg(conference_obj_t *conference,
									  char *conference_name,
									  switch_core_session_t *session, char *bridgeto, uint32_t timeout, const char *flags, const char *cid_name,
									  const char *cid_num, const char *call_uuid, const char *profile, switch_call_cause_t *cancel_cause, switch_event_t **var_event)
{
	struct bg_call *call = NULL;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = NULL;

	if (!(call = malloc(sizeof(*call))))
		return SWITCH_STATUS_MEMERR;

	memset(call, 0, sizeof(*call));
	call->conference = conference;
	call->session = session;
	call->timeout = timeout;
	call->cancel_cause = cancel_cause;

	if (var_event) {
		call->var_event = *var_event;
		var_event = NULL;
	}

	if (conference) {
		pool = conference->pool;
	} else {
		switch_core_new_memory_pool(&pool);
		call->pool = pool;
	}

	if (bridgeto) {
		call->bridgeto = strdup(bridgeto);
	}
	if (flags) {
		call->flags = strdup(flags);
	}
	if (cid_name) {
		call->cid_name = strdup(cid_name);
	}
	if (cid_num) {
		call->cid_num = strdup(cid_num);
	}

	if (conference_name) {
		call->conference_name = strdup(conference_name);
	}

	if (call_uuid) {
		call->uuid = strdup(call_uuid);
	}

	if (profile) {
		call->profile = strdup(profile);
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, conference_outcall_run, call, pool);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Launching BG Thread for outcall\n");

	return SWITCH_STATUS_SUCCESS;
}



SWITCH_STANDARD_APP(conference_auto_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	call_list_t *call_list, *np;

	call_list = switch_channel_get_private(channel, "_conference_autocall_list_");

	if (zstr(data)) {
		call_list = NULL;
	} else {
		np = switch_core_session_alloc(session, sizeof(*np));
		switch_assert(np != NULL);

		np->string = switch_core_session_strdup(session, data);
		if (call_list) {
			np->next = call_list;
			np->iteration = call_list->iteration + 1;
		} else {
			np->iteration = 1;
		}
		call_list = np;
	}
	switch_channel_set_private(channel, "_conference_autocall_list_", call_list);
}


/* Application interface function that is called from the dialplan to join the channel to a conference */
SWITCH_STANDARD_APP(conference_function)
{
	switch_codec_t *read_codec = NULL;
	//uint32_t flags = 0;
	conference_member_t member = { 0 };
	conference_obj_t *conference = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *mydata = NULL;
	char *conference_name = NULL;
	char *bridge_prefix = "bridge:";
	char *flags_prefix = "+flags{";
	char *bridgeto = NULL;
	char *profile_name = NULL;
	switch_xml_t cxml = NULL, cfg = NULL, profiles = NULL;
	const char *flags_str, *v_flags_str;
	const char *cflags_str, *v_cflags_str;
	member_flag_t mflags[MFLAG_MAX] = { 0 };
	switch_core_session_message_t msg = { 0 };
	uint8_t rl = 0, isbr = 0;
	char *dpin = "";
	const char *mdpin = "";
	conference_xml_cfg_t xml_cfg = { 0 };
	switch_event_t *params = NULL;
	int locked = 0;
	int mpin_matched = 0;
	uint32_t *mid;

	if (!switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_DONE) &&
		(switch_channel_test_flag(channel, CF_RECOVERED) || switch_true(switch_channel_get_variable(channel, "conference_silent_entry")))) {
		switch_channel_set_app_flag_key("conference_silent", channel, CONF_SILENT_REQ);
	}

	switch_core_session_video_reset(session);

	switch_channel_set_flag(channel, CF_CONFERENCE);

	if (switch_channel_answer(channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel answer failed.\n");
		goto end;
	}

	/* Save the original read codec. */
	if (!(read_codec = switch_core_session_get_read_codec(session))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel has no media!\n");
		goto end;
	}


	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Invalid arguments\n");
		goto end;
	}

	mydata = switch_core_session_strdup(session, data);

	if (!mydata) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Pool Failure\n");
		goto end;
	}

	if ((flags_str = strstr(mydata, flags_prefix))) {
		char *p;
		*((char *) flags_str) = '\0';
		flags_str += strlen(flags_prefix);
		if ((p = strchr(flags_str, '}'))) {
			*p = '\0';
		}
	}

	//if ((v_flags_str = switch_channel_get_variable(channel, "conference_member_flags"))) {
	if ((v_flags_str = conference_utils_combine_flag_var(session, "conference_member_flags"))) {
		if (zstr(flags_str)) {
			flags_str = v_flags_str;
		} else {
			flags_str = switch_core_session_sprintf(session, "%s|%s", flags_str, v_flags_str);
		}
	}

	cflags_str = flags_str;

	//if ((v_cflags_str = switch_channel_get_variable(channel, "conference_flags"))) {
	if ((v_cflags_str = conference_utils_combine_flag_var(session, "conference_flags"))) {
		if (zstr(cflags_str)) {
			cflags_str = v_cflags_str;
		} else {
			cflags_str = switch_core_session_sprintf(session, "%s|%s", cflags_str, v_cflags_str);
		}
	}

	/* is this a bridging conference ? */
	if (!strncasecmp(mydata, bridge_prefix, strlen(bridge_prefix))) {
		isbr = 1;
		mydata += strlen(bridge_prefix);
		if ((bridgeto = strchr(mydata, ':'))) {
			*bridgeto++ = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Config Error!\n");
			goto done;
		}
	}

	conference_name = mydata;

	/* eat all leading spaces on conference name, which can cause problems */
	while (*conference_name == ' ') {
		conference_name++;
	}

	/* is there a conference pin ? */
	if ((dpin = strchr(conference_name, '+'))) {
		*dpin++ = '\0';
	} else dpin = "";

	/* is there profile specification ? */
	if ((profile_name = strrchr(conference_name, '@'))) {
		*profile_name++ = '\0';
	} else {
		profile_name = "default";
	}

#if 0
	if (0) {
		member.dtmf_parser = conference->dtmf_parser;
	} else {

	}
#endif

	if (switch_channel_test_flag(channel, CF_RECOVERED)) {
		const char *check = switch_channel_get_variable(channel, "last_transfered_conference");

		if (!zstr(check)) {
			conference_name = (char *) check;
		}
	}

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "conference_name", conference_name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile_name", profile_name);

	/* Open the config from the xml registry */
	if (!(cxml = switch_xml_open_cfg(mod_conference_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of %s failed\n", mod_conference_cf_name);
		goto done;
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		xml_cfg.profile = switch_xml_find_child(profiles, "profile", "name", profile_name);
	}

	/* if this is a bridging call, and it's not a duplicate, build a */
	/* conference object, and skip pin handling, and locked checking */

	if (!locked) {
		switch_mutex_lock(conference_globals.setup_mutex);
		locked = 1;
	}

	if (isbr) {
		char *uuid = switch_core_session_get_uuid(session);

		if (!strcmp(conference_name, "_uuid_")) {
			conference_name = uuid;
		}

		if ((conference = conference_find(conference_name, NULL))) {
			switch_thread_rwlock_unlock(conference->rwlock);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Conference %s already exists!\n", conference_name);
			goto done;
		}

		/* Create the conference object. */
		conference = conference_new(conference_name, xml_cfg, session, NULL);

		if (!conference) {
			goto done;
		}

		conference->flags[CFLAG_JSON_STATUS] = 1;
		conference_utils_set_cflags(cflags_str, conference->flags);

		if (locked) {
			switch_mutex_unlock(conference_globals.setup_mutex);
			locked = 0;
		}

		switch_channel_set_variable(channel, "conference_name", conference->name);

		/* Set the minimum number of members (once you go above it you cannot go below it) */
		conference->min = 2;

		/* Indicate the conference is dynamic */
		conference_utils_set_flag_locked(conference, CFLAG_DYNAMIC);

		/* Indicate the conference has a bridgeto party */
		conference_utils_set_flag_locked(conference, CFLAG_BRIDGE_TO);

		/* Start the conference thread for this conference */
		conference_launch_thread(conference);

	} else {
		int enforce_security =  switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND;
		const char *pvar = switch_channel_get_variable(channel, "conference_enforce_security");

		if (pvar) {
			enforce_security = switch_true(pvar);
		}

		if ((conference = conference_find(conference_name, NULL))) {
			if (locked) {
				switch_mutex_unlock(conference_globals.setup_mutex);
				locked = 0;
			}
		}

		/* if the conference exists, get the pointer to it */
		if (!conference) {
			const char *max_members_str;
			const char *endconference_grace_time_str;
			const char *auto_record_str;

			/* no conference yet, so check for join-only flag */
			if (flags_str) {
				conference_utils_set_mflags(flags_str, mflags);

				if (!(mflags[MFLAG_CAN_SPEAK])) {
					if (!(mflags[MFLAG_MUTE_DETECT])) {
						switch_core_media_hard_mute(session, SWITCH_TRUE);
					}
				}

				if (mflags[MFLAG_JOIN_ONLY]) {
					switch_event_t *event;
					switch_xml_t jos_xml;
					char *val;
					/* send event */
					switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
					switch_channel_event_set_basic_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Profile-Name", profile_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "rejected-join-only");
					switch_event_fire(&event);
					/* check what sound file to play */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Cannot create a conference since join-only flag is set\n");
					jos_xml = switch_xml_find_child(xml_cfg.profile, "param", "name", "join-only-sound");
					if (jos_xml && (val = (char *) switch_xml_attr_soft(jos_xml, "value"))) {
						switch_channel_answer(channel);
						switch_ivr_play_file(session, NULL, val, NULL);
					}
					if (!switch_false(switch_channel_get_variable(channel, "hangup_after_conference"))) {
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					}
					goto done;
				}
			}

			/* couldn't find the conference, create one */
			conference = conference_new(conference_name, xml_cfg, session, NULL);

			if (!conference) {
				goto done;
			}

			conference->flags[CFLAG_JSON_STATUS] = 1;
			conference_utils_set_cflags(cflags_str, conference->flags);

			if (locked) {
				switch_mutex_unlock(conference_globals.setup_mutex);
				locked = 0;
			}

			switch_channel_set_variable(channel, "conference_name", conference->name);

			/* Set MOH from variable if not set */
			if (zstr(conference->moh_sound)) {
				conference->moh_sound = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "conference_moh_sound"));
			}

			/* Set perpetual-sound from variable if not set */
			if (zstr(conference->perpetual_sound)) {
				conference->perpetual_sound = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "conference_perpetual_sound"));
			}

			/* Override auto-record profile parameter from variable */
			if (!zstr(auto_record_str = switch_channel_get_variable(channel, "conference_auto_record"))) {
				conference->auto_record = switch_core_strdup(conference->pool, auto_record_str);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "conference_auto_record set from variable to %s\n", auto_record_str);
			}

			/* Set the minimum number of members (once you go above it you cannot go below it) */
			conference->min = 1;

			/* check for variable used to specify override for max_members */
			if (!zstr(max_members_str = switch_channel_get_variable(channel, "conference_max_members"))) {
				uint32_t max_members_val;
				errno = 0;		/* sanity first */
				max_members_val = strtol(max_members_str, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) max_members_val < 0 || max_members_val == 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "conference_max_members variable %s is invalid, not setting a limit\n", max_members_str);
				} else {
					conference->max_members = max_members_val;
				}
			}

			/* check for variable to override endconference_grace_time profile value */
			if (!zstr(endconference_grace_time_str = switch_channel_get_variable(channel, "conference_endconference_grace_time"))) {
				uint32_t grace_time_val;
				errno = 0;		/* sanity first */
				grace_time_val = strtol(endconference_grace_time_str, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) grace_time_val < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "conference_endconference_grace_time variable %s is invalid, not setting a time limit\n", endconference_grace_time_str);
				} else {
					conference->endconference_grace_time = grace_time_val;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "conference endconference_grace_time set from variable to %d\n", grace_time_val);
				}
			}

			/* Indicate the conference is dynamic */
			conference_utils_set_flag_locked(conference, CFLAG_DYNAMIC);

			/* acquire a read lock on the thread so it can't leave without us */
			if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Read Lock Fail\n");
				goto done;
			}

			rl++;

			/* Start the conference thread for this conference */
			conference_launch_thread(conference);
		} else {				/* setup user variable */
			switch_channel_set_variable(channel, "conference_name", conference->name);
			rl++;
		}

		/* Moderator PIN as a channel variable */
		mdpin = switch_channel_get_variable(channel, "conference_moderator_pin");

		if (zstr(dpin) && conference->pin) {
			dpin = conference->pin;
		}
		if (zstr(mdpin) && conference->mpin) {
			mdpin = conference->mpin;
		}


		/* if this is not an outbound call, deal with conference pins */
		if (enforce_security && (!zstr(dpin) || !zstr(mdpin))) {
			char pin_buf[80] = "";
			char *cf_pin_url_param_name = "X-ConfPin=";
			int pin_retries = conference->pin_retries;
			int pin_valid = 0;
			switch_status_t status = SWITCH_STATUS_SUCCESS;
			char *supplied_pin_value;

			/* Answer the channel */
			switch_channel_answer(channel);

			/* look for PIN in channel variable first.  If not present or invalid revert to prompting user */
			supplied_pin_value = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "supplied_pin"));
			if (!zstr(supplied_pin_value)) {
				char *supplied_pin_value_start;
				int i = 0;
				if ((supplied_pin_value_start = (char *) switch_stristr(cf_pin_url_param_name, supplied_pin_value))) {
					/* pin supplied as a URL parameter, move pointer to start of actual pin value */
					supplied_pin_value = supplied_pin_value_start + strlen(cf_pin_url_param_name);
				}
				while (*supplied_pin_value != 0 && *supplied_pin_value != ';') {
					pin_buf[i++] = *supplied_pin_value++;
				}

				validate_pin(pin_buf, dpin, mdpin);
				memset(pin_buf, 0, sizeof(pin_buf));
			}

			if (!conference->pin_sound) {
				conference->pin_sound = switch_core_strdup(conference->pool, "conference/conf-pin.wav");
			}

			if (!conference->bad_pin_sound) {
				conference->bad_pin_sound = switch_core_strdup(conference->pool, "conference/conf-bad-pin.wav");
			}

			while (!pin_valid && pin_retries && status == SWITCH_STATUS_SUCCESS) {
				size_t dpin_length = dpin ? strlen(dpin) : 0;
				size_t mdpin_length = mdpin ? strlen(mdpin) : 0;
				int maxpin = dpin_length > mdpin_length ? (int)dpin_length : (int)mdpin_length;
				switch_status_t pstatus = SWITCH_STATUS_FALSE;

				/* be friendly */
				if (conference->pin_sound) {
					pstatus = conference_file_local_play(conference, session, conference->pin_sound, 20, pin_buf, sizeof(pin_buf));
				} else if (conference->tts_engine && conference->tts_voice) {
					pstatus =
						switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, "please enter the conference pin number", NULL);
				} else {
					pstatus = switch_ivr_speak_text(session, "flite", "slt", "please enter the conference pin number", NULL);
				}

				if (pstatus != SWITCH_STATUS_SUCCESS && pstatus != SWITCH_STATUS_BREAK) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot ask the user for a pin, ending call\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}

				/* wait for them if neccessary */
				if ((int)strlen(pin_buf) < maxpin) {
					char *buf = pin_buf + strlen(pin_buf);
					char term = '\0';

					status = switch_ivr_collect_digits_count(session,
															 buf,
															 sizeof(pin_buf) - strlen(pin_buf), maxpin - strlen(pin_buf), "#", &term, 10000, 0, 0);
					if (status == SWITCH_STATUS_TIMEOUT) {
						status = SWITCH_STATUS_SUCCESS;
					}
				}

				if (status == SWITCH_STATUS_SUCCESS) {
					validate_pin(pin_buf, dpin, mdpin);
				}

				if (!pin_valid) {
					/* zero the collected pin */
					memset(pin_buf, 0, sizeof(pin_buf));

					/* more friendliness */
					if (conference->bad_pin_sound) {
						conference_file_local_play(conference, session, conference->bad_pin_sound, 20, NULL, 0);
					}
					switch_channel_flush_dtmf(channel);
				}
				pin_retries--;
			}

			if (!pin_valid) {
				conference_cdr_rejected(conference, channel, CDRR_PIN);
				goto done;
			}
		}

		if (conference->special_announce && !switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_REQ)) {
			conference_file_local_play(conference, session, conference->special_announce, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		/* don't allow more callers if the conference is locked, unless we invited them */
		if (conference_utils_test_flag(conference, CFLAG_LOCKED) && enforce_security) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Conference %s is locked.\n", conference_name);
			conference_cdr_rejected(conference, channel, CDRR_LOCKED);
			if (conference->locked_sound) {
				/* Answer the channel */
				switch_channel_answer(channel);
				conference_file_local_play(conference, session, conference->locked_sound, 20, NULL, 0);
			}
			goto done;
		}

		/* dont allow more callers than the max_members allows for -- I explicitly didnt allow outbound calls
		 * someone else can add that (see above) if they feel that outbound calls should be able to violate the
		 * max_members limit
		 */
		if ((conference->max_members > 0) && (conference->count >= conference->max_members)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Conference %s is full.\n", conference_name);
			conference_cdr_rejected(conference, channel, CDRR_MAXMEMBERS);
			if (conference->maxmember_sound) {
				/* Answer the channel */
				switch_channel_answer(channel);
				conference_file_local_play(conference, session, conference->maxmember_sound, 20, NULL, 0);
			}
			goto done;
		}

	}

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	/* if we're using "bridge:" make an outbound call and bridge it in */
	if (!zstr(bridgeto) && strcasecmp(bridgeto, "none")) {
		switch_call_cause_t cause;
		if (conference_outcall(conference, NULL, session, bridgeto, 60, NULL, NULL, NULL, NULL, &cause, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
	} else {
		/* if we're not using "bridge:" set the conference answered flag */
		/* and this isn't an outbound channel, answer the call */
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND)
			conference_utils_set_flag(conference, CFLAG_ANSWERED);
	}

	member.session = session;
	member.channel = switch_core_session_get_channel(session);
	member.pool = switch_core_session_get_pool(session);

	/* Prepare MUTEXS */
	switch_mutex_init(&member.flag_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.write_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.read_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.fnode_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.audio_in_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.audio_out_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_thread_rwlock_create(&member.rwlock, member.pool);

	if (conference_member_setup_media(&member, conference)) {
		//flags = 0;
		goto done;
	}


	if (!(mid = switch_channel_get_private(channel, "__confmid"))) {
		mid = switch_core_session_alloc(session, sizeof(*mid));
		*mid = next_member_id();
		switch_channel_set_private(channel, "__confmid", mid);
	}

	switch_channel_set_variable_printf(channel, "conference_member_id", "%u", *mid);
	member.id = *mid;


	/* Install our Signed Linear codec so we get the audio in that format */
	switch_core_session_set_read_codec(member.session, &member.read_codec);


	memcpy(mflags, conference->mflags, sizeof(mflags));

	conference_utils_set_mflags(flags_str, mflags);
	mflags[MFLAG_RUNNING] = 1;

	if (!(mflags[MFLAG_CAN_SPEAK])) {
		if (!(mflags[MFLAG_MUTE_DETECT])) {
			switch_core_media_hard_mute(member.session, SWITCH_TRUE);
		}
	}

	if (mpin_matched) {
		mflags[MFLAG_MOD] = 1;
	}

	conference_utils_merge_mflags(member.flags, mflags);


	if (mflags[MFLAG_MINTWO]) {
		conference->min = 2;
	}


	if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX) {
		switch_queue_create(&member.video_queue, 200, member.pool);
		switch_queue_create(&member.mux_out_queue, 500, member.pool);
		switch_frame_buffer_create(&member.fb);
	}

	/* Add the caller to the conference */
	if (conference_member_add(conference, &member) != SWITCH_STATUS_SUCCESS) {
		switch_core_codec_destroy(&member.read_codec);
		goto done;
	}

	if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX) {
		conference_video_launch_muxing_write_thread(&member);
	}

	msg.from = __FILE__;

	/* Tell the channel we are going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	switch_core_session_receive_message(session, &msg);

	if (conference_utils_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		switch_channel_set_flag(channel, CF_VIDEO_DECODED_READ);
		switch_core_media_gen_key_frame(session);
	}

	/* Chime in the core video thread */
	switch_core_session_set_video_read_callback(session, conference_video_thread_callback, (void *)&member);

	if (switch_channel_test_flag(channel, CF_VIDEO_ONLY)) {
		while(conference_utils_member_test_flag((&member), MFLAG_RUNNING) && switch_channel_ready(channel)) {
			switch_yield(100000);
		}
	} else {

		/* Run the conference loop */
		do {
			conference_loop_output(&member);
		} while (member.loop_loop);
	}

	switch_core_session_set_video_read_callback(session, NULL, NULL);

	switch_channel_set_private(channel, "_conference_autocall_list_", NULL);

	/* Tell the channel we are no longer going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	switch_core_session_receive_message(session, &msg);

	if (member.video_muxing_write_thread) {
		switch_status_t st = SWITCH_STATUS_SUCCESS;
		switch_queue_push(member.mux_out_queue, NULL);
		switch_thread_join(&st, member.video_muxing_write_thread);
		member.video_muxing_write_thread = NULL;
	}

	/* Remove the caller from the conference */
	conference_member_del(member.conference, &member);

	/* Put the original codec back */
	switch_core_session_set_read_codec(member.session, NULL);

	/* Clean Up. */

 done:

	if (locked) {
		switch_mutex_unlock(conference_globals.setup_mutex);
	}

	if (member.read_resampler) {
		switch_resample_destroy(&member.read_resampler);
	}

	switch_event_destroy(&params);
	switch_buffer_destroy(&member.resample_buffer);
	switch_buffer_destroy(&member.audio_buffer);
	switch_buffer_destroy(&member.mux_buffer);

	if (member.fb) {
		switch_frame_buffer_destroy(&member.fb);
	}

	if (conference) {
		switch_mutex_lock(conference->mutex);
		if (conference_utils_test_flag(conference, CFLAG_DYNAMIC) && conference->count == 0) {
			conference_utils_set_flag_locked(conference, CFLAG_DESTRUCT);
		}
		switch_mutex_unlock(conference->mutex);
	}

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
	}

	if (conference && conference_utils_member_test_flag(&member, MFLAG_KICKED) && conference->kicked_sound) {
		char *toplay = NULL;
		char *dfile = NULL;
		char *expanded = NULL;
		char *src = member.kicked_sound ? member.kicked_sound : conference->kicked_sound;


		if (!strncasecmp(src, "say:", 4)) {
			if (conference->tts_engine && conference->tts_voice) {
				switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, src + 4, NULL);
			}
		} else {
			if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(session), src)) != src) {
				toplay = expanded;
			} else {
				expanded = NULL;
				toplay = src;
			}

			if (!switch_is_file_path(toplay) && conference->sound_prefix) {
				dfile = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, toplay);
				switch_assert(dfile);
				toplay = dfile;
			}

			switch_ivr_play_file(session, NULL, toplay, NULL);
			switch_safe_free(dfile);
			switch_safe_free(expanded);
		}
	}

	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);

	/* release the readlock */
	if (rl) {
		switch_thread_rwlock_unlock(conference->rwlock);
	}

	switch_channel_set_variable(channel, "last_transfered_conference", NULL);

 end:

	switch_channel_clear_flag(channel, CF_CONFERENCE);

	switch_core_session_video_reset(session);
}



/* Create a thread for the conference and launch it */
void conference_launch_thread(conference_obj_t *conference)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	conference_utils_set_flag_locked(conference, CFLAG_RUNNING);
	switch_threadattr_create(&thd_attr, conference->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_mutex_lock(conference_globals.hash_mutex);
	switch_mutex_unlock(conference_globals.hash_mutex);
	switch_thread_create(&thread, thd_attr, conference_thread_run, conference, conference->pool);
}

conference_obj_t *conference_find(char *name, char *domain)
{
	conference_obj_t *conference;

	switch_mutex_lock(conference_globals.hash_mutex);
	if ((conference = switch_core_hash_find(conference_globals.conference_hash, name))) {
		if (conference_utils_test_flag(conference, CFLAG_DESTRUCT)) {
			switch_core_hash_delete(conference_globals.conference_hash, conference->name);
			conference_utils_clear_flag(conference, CFLAG_INHASH);
			conference = NULL;
		} else if (!zstr(domain) && conference->domain && strcasecmp(domain, conference->domain)) {
			conference = NULL;
		}
	}
	if (conference) {
		if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
			conference = NULL;
		}
	}
	switch_mutex_unlock(conference_globals.hash_mutex);

	return conference;
}

/* create a new conferene with a specific profile */
conference_obj_t *conference_new(char *name, conference_xml_cfg_t cfg, switch_core_session_t *session, switch_memory_pool_t *pool)
{
	conference_obj_t *conference;
	switch_xml_t xml_kvp;
	char *timer_name = NULL;
	char *domain = NULL;
	char *desc = NULL;
	char *name_domain = NULL;
	char *tts_engine = NULL;
	char *tts_voice = NULL;
	char *enter_sound = NULL;
	char *sound_prefix = NULL;
	char *exit_sound = NULL;
	char *alone_sound = NULL;
	char *muted_sound = NULL;
	char *mute_detect_sound = NULL;
	char *unmuted_sound = NULL;
	char *locked_sound = NULL;
	char *is_locked_sound = NULL;
	char *is_unlocked_sound = NULL;
	char *kicked_sound = NULL;
	char *join_only_sound = NULL;
	char *pin = NULL;
	char *mpin = NULL;
	char *pin_sound = NULL;
	char *bad_pin_sound = NULL;
	char *energy_level = NULL;
	char *auto_gain_level = NULL;
	char *caller_id_name = NULL;
	char *caller_id_number = NULL;
	char *caller_controls = NULL;
	char *moderator_controls = NULL;
	char *member_flags = NULL;
	char *conference_flags = NULL;
	char *perpetual_sound = NULL;
	char *moh_sound = NULL;
	char *outcall_templ = NULL;
	char *video_layout_name = NULL;
	char *video_layout_group = NULL;
	char *video_canvas_size = NULL;
	char *video_canvas_bgcolor = NULL;
	char *video_border_color = NULL;
	int video_border_size = 0;
	char *video_super_canvas_bgcolor = NULL;
	char *video_letterbox_bgcolor = NULL;
	char *video_codec_bandwidth = NULL;
	char *no_video_avatar = NULL;
	conference_video_mode_t conference_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
	int conference_video_quality = 1;
	int auto_kps_debounce = 30000;
	float fps = 15.0f;
	uint32_t max_members = 0;
	uint32_t announce_count = 0;
	char *maxmember_sound = NULL;
	uint32_t rate = 8000, interval = 20;
	uint32_t channels = 1;
	int broadcast_chat_messages = 1;
	int comfort_noise_level = 0;
	int pin_retries = 3;
	int ivr_dtmf_timeout = 500;
	int ivr_input_timeout = 0;
	int video_canvas_count = 0;
	int video_super_canvas_label_layers = 0;
	int video_super_canvas_show_all_layers = 0;
	char *suppress_events = NULL;
	char *verbose_events = NULL;
	char *auto_record = NULL;
	int auto_record_canvas = 0;
	int min_recording_participants = 1;
	char *conference_log_dir = NULL;
	char *cdr_event_mode = NULL;
	char *terminate_on_silence = NULL;
	char *endconference_grace_time = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH+1];
	switch_uuid_t uuid;
	switch_codec_implementation_t read_impl = { 0 };
	switch_channel_t *channel = NULL;
	const char *force_rate = NULL, *force_interval = NULL, *force_channels = NULL, *presence_id = NULL;
	uint32_t force_rate_i = 0, force_interval_i = 0, force_channels_i = 0, video_auto_floor_msec = 0;
	switch_event_t *event;
	
	/* Validate the conference name */
	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Record! no name.\n");
		return NULL;
	}

	if (session) {
		uint32_t tmp;

		switch_core_session_get_read_impl(session, &read_impl);
		channel = switch_core_session_get_channel(session);

		presence_id = switch_channel_get_variable(channel, "presence_id");

		if ((force_rate = switch_channel_get_variable(channel, "conference_force_rate"))) {
			if (!strcasecmp(force_rate, "auto")) {
				force_rate_i = read_impl.actual_samples_per_second;
			} else {
				tmp = atoi(force_rate);

				if (tmp == 8000 || tmp == 12000 || tmp == 16000 || tmp == 24000 || tmp == 32000 || tmp == 44100 || tmp == 48000) {
					force_rate_i = rate = tmp;
				}
			}
		}

		if ((force_channels = switch_channel_get_variable(channel, "conference_force_channels"))) {
			if (!strcasecmp(force_channels, "auto")) {
				force_rate_i = read_impl.number_of_channels;
			} else {
				tmp = atoi(force_channels);

				if (tmp == 1 || tmp == 2) {
					force_channels_i = channels = tmp;
				}
			}
		}

		if ((force_interval = switch_channel_get_variable(channel, "conference_force_interval"))) {
			if (!strcasecmp(force_interval, "auto")) {
				force_interval_i = read_impl.microseconds_per_packet / 1000;
			} else {
				tmp = atoi(force_interval);

				if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
					force_interval_i = interval = tmp;
				}
			}
		}
	}

	switch_mutex_lock(conference_globals.hash_mutex);

	/* parse the profile tree for param values */
	if (cfg.profile)
		for (xml_kvp = switch_xml_child(cfg.profile, "param"); xml_kvp; xml_kvp = xml_kvp->next) {
			char *var = (char *) switch_xml_attr_soft(xml_kvp, "name");
			char *val = (char *) switch_xml_attr_soft(xml_kvp, "value");
			char buf[128] = "";
			char *p;

			if (strchr(var, '_')) {
				switch_copy_string(buf, var, sizeof(buf));
				for (p = buf; *p; p++) {
					if (*p == '_') {
						*p = '-';
					}
				}
				var = buf;
			}

			if (!force_rate_i && !strcasecmp(var, "rate") && !zstr(val)) {
				uint32_t tmp = atoi(val);
				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						rate = read_impl.actual_samples_per_second;
					}
				} else {
					if (tmp == 8000 || tmp == 12000 || tmp == 16000 || tmp == 24000 || tmp == 32000 || tmp == 44100 || tmp == 48000) {
						rate = tmp;
					}
				}
			} else if (!force_channels_i && !strcasecmp(var, "channels") && !zstr(val)) {
				uint32_t tmp = atoi(val);
				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						channels = read_impl.number_of_channels;
					}
				} else {
					if (tmp == 1 || tmp == 2) {
						channels = tmp;
					}
				}
			} else if (!strcasecmp(var, "domain") && !zstr(val)) {
				domain = val;
			} else if (!strcasecmp(var, "description") && !zstr(val)) {
				desc = val;
			} else if (!force_interval_i && !strcasecmp(var, "interval") && !zstr(val)) {
				uint32_t tmp = atoi(val);

				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						interval = read_impl.microseconds_per_packet / 1000;
					}
				} else {
					if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
						interval = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
										  "Interval must be multipe of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
					}
				}
			} else if (!strcasecmp(var, "timer-name") && !zstr(val)) {
				timer_name = val;
			} else if (!strcasecmp(var, "tts-engine") && !zstr(val)) {
				tts_engine = val;
			} else if (!strcasecmp(var, "tts-voice") && !zstr(val)) {
				tts_voice = val;
			} else if (!strcasecmp(var, "enter-sound") && !zstr(val)) {
				enter_sound = val;
			} else if (!strcasecmp(var, "outcall-templ") && !zstr(val)) {
				outcall_templ = val;
			} else if (!strcasecmp(var, "video-layout-name") && !zstr(val)) {
				video_layout_name = val;
			} else if (!strcasecmp(var, "video-canvas-count") && !zstr(val)) {
				video_canvas_count = atoi(val);
			} else if (!strcasecmp(var, "video-super-canvas-label-layers") && !zstr(val)) {
				video_super_canvas_label_layers = atoi(val);
			} else if (!strcasecmp(var, "video-super-canvas-show-all-layers") && !zstr(val)) {
				video_super_canvas_show_all_layers = atoi(val);
			} else if (!strcasecmp(var, "video-canvas-bgcolor") && !zstr(val)) {
				video_canvas_bgcolor = val;
			} else if (!strcasecmp(var, "video-border-color") && !zstr(val)) {
				video_border_color = val;
			} else if (!strcasecmp(var, "video-border-size") && !zstr(val)) {
				video_border_size = atoi(val);
			} else if (!strcasecmp(var, "video-super-canvas-bgcolor") && !zstr(val)) {
				video_super_canvas_bgcolor= val;
			} else if (!strcasecmp(var, "video-letterbox-bgcolor") && !zstr(val)) {
				video_letterbox_bgcolor= val;
			} else if (!strcasecmp(var, "video-canvas-size") && !zstr(val)) {
				video_canvas_size = val;
			} else if (!strcasecmp(var, "video-fps") && !zstr(val)) {
				fps = (float)atof(val);
			} else if (!strcasecmp(var, "video-codec-bandwidth") && !zstr(val)) {
				video_codec_bandwidth = val;
			} else if (!strcasecmp(var, "video-no-video-avatar") && !zstr(val)) {
				no_video_avatar = val;
			} else if (!strcasecmp(var, "exit-sound") && !zstr(val)) {
				exit_sound = val;
			} else if (!strcasecmp(var, "alone-sound") && !zstr(val)) {
				alone_sound = val;
			} else if (!strcasecmp(var, "perpetual-sound") && !zstr(val)) {
				perpetual_sound = val;
			} else if (!strcasecmp(var, "moh-sound") && !zstr(val)) {
				moh_sound = val;
			} else if (!strcasecmp(var, "muted-sound") && !zstr(val)) {
				muted_sound = val;
			} else if (!strcasecmp(var, "mute-detect-sound") && !zstr(val)) {
				mute_detect_sound = val;
			} else if (!strcasecmp(var, "unmuted-sound") && !zstr(val)) {
				unmuted_sound = val;
			} else if (!strcasecmp(var, "locked-sound") && !zstr(val)) {
				locked_sound = val;
			} else if (!strcasecmp(var, "is-locked-sound") && !zstr(val)) {
				is_locked_sound = val;
			} else if (!strcasecmp(var, "is-unlocked-sound") && !zstr(val)) {
				is_unlocked_sound = val;
			} else if (!strcasecmp(var, "member-flags") && !zstr(val)) {
				member_flags = val;
			} else if (!strcasecmp(var, "conference-flags") && !zstr(val)) {
				conference_flags = val;
			} else if (!strcasecmp(var, "cdr-log-dir") && !zstr(val)) {
				conference_log_dir = val;
			} else if (!strcasecmp(var, "cdr-event-mode") && !zstr(val)) {
				cdr_event_mode = val;
			} else if (!strcasecmp(var, "kicked-sound") && !zstr(val)) {
				kicked_sound = val;
			} else if (!strcasecmp(var, "join-only-sound") && !zstr(val)) {
				join_only_sound = val;
			} else if (!strcasecmp(var, "pin") && !zstr(val)) {
				pin = val;
			} else if (!strcasecmp(var, "moderator-pin") && !zstr(val)) {
				mpin = val;
			} else if (!strcasecmp(var, "pin-retries") && !zstr(val)) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					pin_retries = tmp;
				}
			} else if (!strcasecmp(var, "pin-sound") && !zstr(val)) {
				pin_sound = val;
			} else if (!strcasecmp(var, "bad-pin-sound") && !zstr(val)) {
				bad_pin_sound = val;
			} else if (!strcasecmp(var, "energy-level") && !zstr(val)) {
				energy_level = val;
			} else if (!strcasecmp(var, "auto-gain-level") && !zstr(val)) {
				auto_gain_level = val;
			} else if (!strcasecmp(var, "caller-id-name") && !zstr(val)) {
				caller_id_name = val;
			} else if (!strcasecmp(var, "caller-id-number") && !zstr(val)) {
				caller_id_number = val;
			} else if (!strcasecmp(var, "caller-controls") && !zstr(val)) {
				caller_controls = val;
			} else if (!strcasecmp(var, "ivr-dtmf-timeout") && !zstr(val)) {
				ivr_dtmf_timeout = atoi(val);
				if (ivr_dtmf_timeout < 500) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "not very smart value for ivr-dtmf-timeout found (%d), defaulting to 500ms\n", ivr_dtmf_timeout);
					ivr_dtmf_timeout = 500;
				}
			} else if (!strcasecmp(var, "ivr-input-timeout") && !zstr(val)) {
				ivr_input_timeout = atoi(val);
				if (ivr_input_timeout != 0 && ivr_input_timeout < 500) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "not very smart value for ivr-input-timeout found (%d), defaulting to 500ms\n", ivr_input_timeout);
					ivr_input_timeout = 5000;
				}
			} else if (!strcasecmp(var, "moderator-controls") && !zstr(val)) {
				moderator_controls = val;
			} else if (!strcasecmp(var, "broadcast-chat-messages") && !zstr(val)) {
				broadcast_chat_messages = switch_true(val);
			} else if (!strcasecmp(var, "comfort-noise") && !zstr(val)) {
				int tmp;
				tmp = atoi(val);
				if (tmp > 1 && tmp < 10000) {
					comfort_noise_level = tmp;
				} else if (switch_true(val)) {
					comfort_noise_level = 1400;
				}
			} else if (!strcasecmp(var, "video-auto-floor-msec") && !zstr(val)) {
				int tmp;
				tmp = atoi(val);

				if (tmp > 0) {
					video_auto_floor_msec = tmp;
				}
			} else if (!strcasecmp(var, "sound-prefix") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "override sound-prefix with: %s\n", val);
				sound_prefix = val;
			} else if (!strcasecmp(var, "max-members") && !zstr(val)) {
				errno = 0;		/* sanity first */
				max_members = strtol(val, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) max_members < 0 || max_members == 1) {
					/* a negative wont work well, and its foolish to have a conference limited to 1 person unless the outbound
					 * stuff is added, see comments above
					 */
					max_members = 0;	/* set to 0 to disable max counts */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "max-members %s is invalid, not setting a limit\n", val);
				}
			} else if (!strcasecmp(var, "max-members-sound") && !zstr(val)) {
				maxmember_sound = val;
			} else if (!strcasecmp(var, "announce-count") && !zstr(val)) {
				errno = 0;		/* safety first */
				announce_count = strtol(val, NULL, 0);
				if (errno == ERANGE || errno == EINVAL) {
					announce_count = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "announce-count is invalid, not anouncing member counts\n");
				}
			} else if (!strcasecmp(var, "suppress-events") && !zstr(val)) {
				suppress_events = val;
			} else if (!strcasecmp(var, "verbose-events") && !zstr(val)) {
				verbose_events = val;
			} else if (!strcasecmp(var, "auto-record") && !zstr(val)) {
				auto_record = val;
			} else if (!strcasecmp(var, "auto-record-canvas-id") && !zstr(val)) {
				auto_record_canvas = atoi(val);
				if (auto_record_canvas) {
					auto_record_canvas--;

					if (auto_record_canvas < 1) auto_record_canvas = 0;
				}
			} else if (!strcasecmp(var, "min-required-recording-participants") && !zstr(val)) {
				if (!strcmp(val, "1")) {
					min_recording_participants = 1;
				} else if (!strcmp(val, "2")) {
					min_recording_participants = 2;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "min-required-recording-participants is invalid, leaving set to %d\n", min_recording_participants);
				}
			} else if (!strcasecmp(var, "terminate-on-silence") && !zstr(val)) {
				terminate_on_silence = val;
			} else if (!strcasecmp(var, "endconf-grace-time") && !zstr(val)) {
				endconference_grace_time = val;
			} else if (!strcasecmp(var, "video-quality") && !zstr(val)) {
				int tmp = atoi(val);

				if (tmp > -1 && tmp < 5) {
					conference_video_quality = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Video quality must be between 0 and 4\n");
				}
			} else if (!strcasecmp(var, "video-kps-debounce") && !zstr(val)) {
				int tmp = atoi(val);

				if (tmp >= 0) {
					auto_kps_debounce = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video-kps-debounce must be 0 or higher\n");
				}
				
			} else if (!strcasecmp(var, "video-mode") && !zstr(val)) {
				if (!strcasecmp(val, "passthrough")) {
					conference_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
				} else if (!strcasecmp(val, "transcode")) {
					conference_video_mode = CONF_VIDEO_MODE_TRANSCODE;
				} else if (!strcasecmp(val, "mux")) {
					conference_video_mode = CONF_VIDEO_MODE_MUX;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video-mode invalid, valid settings are 'passthrough', 'transcode' and 'mux'\n");
				}
			}
		}

	/* Set defaults and various paramaters */

	/* Timer module to use */
	if (zstr(timer_name)) {
		timer_name = "soft";
	}

	/* Caller ID Name */
	if (zstr(caller_id_name)) {
		caller_id_name = (char *) mod_conference_app_name;
	}

	/* Caller ID Number */
	if (zstr(caller_id_number)) {
		caller_id_number = SWITCH_DEFAULT_CLID_NUMBER;
	}

	if (!pool) {
		/* Setup a memory pool to use. */
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			conference = NULL;
			goto end;
		}
	}

	/* Create the conference object. */
	if (!(conference = switch_core_alloc(pool, sizeof(*conference)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		conference = NULL;
		goto end;
	}

	conference->start_time = switch_epoch_time_now(NULL);

	/* initialize the conference object with settings from the specified profile */
	conference->pool = pool;
	conference->profile_name = switch_core_strdup(conference->pool, cfg.profile ? switch_xml_attr_soft(cfg.profile, "name") : "none");
	if (timer_name) {
		conference->timer_name = switch_core_strdup(conference->pool, timer_name);
	}
	if (tts_engine) {
		conference->tts_engine = switch_core_strdup(conference->pool, tts_engine);
	}
	if (tts_voice) {
		conference->tts_voice = switch_core_strdup(conference->pool, tts_voice);
	}

	conference->comfort_noise_level = comfort_noise_level;
	conference->pin_retries = pin_retries;
	conference->caller_id_name = switch_core_strdup(conference->pool, caller_id_name);
	conference->caller_id_number = switch_core_strdup(conference->pool, caller_id_number);
	conference->caller_controls = switch_core_strdup(conference->pool, caller_controls);
	conference->moderator_controls = switch_core_strdup(conference->pool, moderator_controls);
	conference->broadcast_chat_messages = broadcast_chat_messages;
	conference->video_quality = conference_video_quality;
	conference->auto_kps_debounce = auto_kps_debounce;

	conference->conference_video_mode = conference_video_mode;

	if (!switch_core_has_video() && (conference->conference_video_mode == CONF_VIDEO_MODE_MUX || conference->conference_video_mode == CONF_VIDEO_MODE_TRANSCODE)) {
		conference->conference_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video-mode invalid, only valid setting is 'passthrough' due to no video capabilities\n");
	}

	if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX) {
		int canvas_w = 0, canvas_h = 0;
		if (video_canvas_size) {
			char *p;

			if ((canvas_w = atoi(video_canvas_size))) {
				if ((p = strchr(video_canvas_size, 'x'))) {
					p++;
					if (*p) {
						canvas_h = atoi(p);
					}
				}
			}
		}

		if (canvas_w < 320 || canvas_h < 180) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s video-canvas-size, falling back to %ux%u\n",
							  video_canvas_size ? "Invalid" : "Unspecified", CONFERENCE_CANVAS_DEFAULT_WIDTH, CONFERENCE_CANVAS_DEFAULT_HIGHT);
			canvas_w = CONFERENCE_CANVAS_DEFAULT_WIDTH;
			canvas_h = CONFERENCE_CANVAS_DEFAULT_HIGHT;
		}

		if (video_border_size) {
			if (video_border_size < 0) video_border_size = 0;
			if (video_border_size > 50) video_border_size = 50;
		}
		conference->video_border_size = video_border_size;

		conference_video_parse_layouts(conference, canvas_w, canvas_h);

		if (!video_canvas_bgcolor) {
			video_canvas_bgcolor = "#333333";
		}

		if (!video_border_color) {
			video_border_color = "#000000";
		}
		
		if (!video_super_canvas_bgcolor) {
			video_super_canvas_bgcolor = "#068df3";
		}

		if (!video_letterbox_bgcolor) {
			video_letterbox_bgcolor = "#000000";
		}

		if (no_video_avatar) {
			conference->no_video_avatar = switch_core_strdup(conference->pool, no_video_avatar);
		}

		
		conference->video_canvas_bgcolor = switch_core_strdup(conference->pool, video_canvas_bgcolor);
		conference->video_border_color = switch_core_strdup(conference->pool, video_border_color);
		conference->video_super_canvas_bgcolor = switch_core_strdup(conference->pool, video_super_canvas_bgcolor);
		conference->video_letterbox_bgcolor = switch_core_strdup(conference->pool, video_letterbox_bgcolor);

		if (fps) {
			conference_video_set_fps(conference, fps);
		}

		if (!conference->video_fps.ms) {
			conference_video_set_fps(conference, 30);
		}

		if (video_codec_bandwidth) {
			if (!strcasecmp(video_codec_bandwidth, "auto")) {
				conference->video_codec_settings.video.bandwidth = switch_calc_bitrate(canvas_w, canvas_h, conference->video_quality, conference->video_fps.fps);
			} else {
				conference->video_codec_settings.video.bandwidth = switch_parse_bandwidth_string(video_codec_bandwidth);
			}
		}

		if (zstr(video_layout_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No video-layout-name specified, using " CONFERENCE_MUX_DEFAULT_LAYOUT "\n");
			video_layout_name = CONFERENCE_MUX_DEFAULT_LAYOUT;
		}

		if (!strncasecmp(video_layout_name, "group:", 6)) {
			video_layout_group = video_layout_name + 6;
		}
		if (video_layout_name) {
			conference->video_layout_name = switch_core_strdup(conference->pool, video_layout_name);
		}

		if (video_layout_group) {
			conference->video_layout_group = switch_core_strdup(conference->pool, video_layout_group);
		}

		if (!conference_video_get_layout(conference, video_layout_name, video_layout_group)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid video-layout-name specified, using " CONFERENCE_MUX_DEFAULT_LAYOUT "\n");
			video_layout_name = CONFERENCE_MUX_DEFAULT_LAYOUT;
			video_layout_group = video_layout_name + 6;
			conference->video_layout_name = switch_core_strdup(conference->pool, video_layout_name);
			conference->video_layout_group = switch_core_strdup(conference->pool, video_layout_group);
		}

		if (!conference_video_get_layout(conference, video_layout_name, video_layout_group)) {
			conference->video_layout_name = conference->video_layout_group = video_layout_group = video_layout_name = NULL;
			conference->conference_video_mode = CONF_VIDEO_MODE_TRANSCODE;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid conference layout settings, falling back to transcode mode\n");
		} else {
			conference->canvas_width = canvas_w;
			conference->canvas_height = canvas_h;
		}
	}

	if (conference->conference_video_mode == CONF_VIDEO_MODE_TRANSCODE || conference->conference_video_mode == CONF_VIDEO_MODE_MUX) {
		conference_utils_set_flag(conference, CFLAG_TRANSCODE_VIDEO);
	}

	if (outcall_templ) {
		conference->outcall_templ = switch_core_strdup(conference->pool, outcall_templ);
	}
	conference->run_time = switch_epoch_time_now(NULL);

	if (!zstr(conference_log_dir)) {
		char *path;

		if (!strcmp(conference_log_dir, "auto")) {
			path = switch_core_sprintf(conference->pool, "%s%sconference_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
		} else if (!switch_is_file_path(conference_log_dir)) {
			path = switch_core_sprintf(conference->pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, conference_log_dir);
		} else {
			path = switch_core_strdup(conference->pool, conference_log_dir);
		}

		switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, conference->pool);
		conference->log_dir = path;

	}

	if (!zstr(cdr_event_mode)) {
		if (!strcmp(cdr_event_mode, "content")) {
			conference->cdr_event_mode = CDRE_AS_CONTENT;
		} else if (!strcmp(cdr_event_mode, "file")) {
			if (!zstr(conference->log_dir)) {
				conference->cdr_event_mode = CDRE_AS_FILE;
			} else {
				conference->cdr_event_mode = CDRE_NONE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "'cdr-log-dir' parameter not set; CDR event mode 'file' ignored");
			}
		} else {
			conference->cdr_event_mode = CDRE_NONE;
		}
	}

	if (!zstr(perpetual_sound)) {
		conference->perpetual_sound = switch_core_strdup(conference->pool, perpetual_sound);
	}

	conference->mflags[MFLAG_CAN_SPEAK] = conference->mflags[MFLAG_CAN_HEAR] = conference->mflags[MFLAG_CAN_BE_SEEN] = 1;

	if (!zstr(moh_sound) && switch_is_moh(moh_sound)) {
		conference->moh_sound = switch_core_strdup(conference->pool, moh_sound);
	}

	if (member_flags) {
		conference_utils_set_mflags(member_flags, conference->mflags);
	}

	if (conference_flags) {
		conference_utils_set_cflags(conference_flags, conference->flags);
	}

	if (!zstr(sound_prefix)) {
		conference->sound_prefix = switch_core_strdup(conference->pool, sound_prefix);
	} else {
		const char *val;
		if ((val = switch_channel_get_variable(channel, "sound_prefix")) && !zstr(val)) {
			/* if no sound_prefix was set, use the channel sound_prefix */
			conference->sound_prefix = switch_core_strdup(conference->pool, val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "using channel sound prefix: %s\n", conference->sound_prefix);
		}
	}

	if (!zstr(enter_sound)) {
		conference->enter_sound = switch_core_strdup(conference->pool, enter_sound);
	}

	if (!zstr(exit_sound)) {
		conference->exit_sound = switch_core_strdup(conference->pool, exit_sound);
	}

	if (!zstr(muted_sound)) {
		conference->muted_sound = switch_core_strdup(conference->pool, muted_sound);
	}

	if (zstr(mute_detect_sound)) {
		if (!zstr(muted_sound)) {
			conference->mute_detect_sound = switch_core_strdup(conference->pool, muted_sound);
		}
	} else {
		conference->mute_detect_sound = switch_core_strdup(conference->pool, mute_detect_sound);
	}

	if (!zstr(unmuted_sound)) {
		conference->unmuted_sound = switch_core_strdup(conference->pool, unmuted_sound);
	}

	if (!zstr(kicked_sound)) {
		conference->kicked_sound = switch_core_strdup(conference->pool, kicked_sound);
	}

	if (!zstr(join_only_sound)) {
		conference->join_only_sound = switch_core_strdup(conference->pool, join_only_sound);
	}

	if (!zstr(pin_sound)) {
		conference->pin_sound = switch_core_strdup(conference->pool, pin_sound);
	}

	if (!zstr(bad_pin_sound)) {
		conference->bad_pin_sound = switch_core_strdup(conference->pool, bad_pin_sound);
	}

	if (!zstr(pin)) {
		conference->pin = switch_core_strdup(conference->pool, pin);
	}

	if (!zstr(mpin)) {
		conference->mpin = switch_core_strdup(conference->pool, mpin);
	}

	if (!zstr(alone_sound)) {
		conference->alone_sound = switch_core_strdup(conference->pool, alone_sound);
	}

	if (!zstr(locked_sound)) {
		conference->locked_sound = switch_core_strdup(conference->pool, locked_sound);
	}

	if (!zstr(is_locked_sound)) {
		conference->is_locked_sound = switch_core_strdup(conference->pool, is_locked_sound);
	}

	if (!zstr(is_unlocked_sound)) {
		conference->is_unlocked_sound = switch_core_strdup(conference->pool, is_unlocked_sound);
	}

	if (!zstr(energy_level)) {
		conference->energy_level = atoi(energy_level);
		if (conference->energy_level < 0) {
			conference->energy_level = 0;
		}
	}

	if (!zstr(auto_gain_level)) {
		int level = 0;

		if (switch_true(auto_gain_level) && !switch_is_number(auto_gain_level)) {
			level = DEFAULT_AGC_LEVEL;
		} else {
			level = atoi(auto_gain_level);
		}

		if (level > 0 && level > conference->energy_level) {
			conference->agc_level = level;
		}
	}

	if (!zstr(maxmember_sound)) {
		conference->maxmember_sound = switch_core_strdup(conference->pool, maxmember_sound);
	}
	/* its going to be 0 by default, set to a value otherwise so this should be safe */
	conference->max_members = max_members;
	conference->announce_count = announce_count;

	conference->name = switch_core_strdup(conference->pool, name);

	if ((name_domain = strchr(conference->name, '@'))) {
		name_domain++;
		conference->domain = switch_core_strdup(conference->pool, name_domain);
	} else if (domain) {
		conference->domain = switch_core_strdup(conference->pool, domain);
	} else if (presence_id && (name_domain = strchr(presence_id, '@'))) {
		name_domain++;
		conference->domain = switch_core_strdup(conference->pool, name_domain);
	} else {
		conference->domain = "cluecon.com";
	}

	conference->chat_id = switch_core_sprintf(conference->pool, "conf+%s@%s", conference->name, conference->domain);

	conference->channels = channels;
	conference->rate = rate;
	conference->interval = interval;
	conference->ivr_dtmf_timeout = ivr_dtmf_timeout;
	conference->ivr_input_timeout = ivr_input_timeout;

	if (video_auto_floor_msec) {
		conference->video_floor_packets = video_auto_floor_msec / conference->interval;
	}

	conference->eflags = 0xFFFFFFFF;

	if (!zstr(suppress_events)) {
		conference_utils_clear_eflags(suppress_events, &conference->eflags);
	}

	if (!zstr(auto_record)) {
		conference->auto_record = switch_core_strdup(conference->pool, auto_record);
	}

	conference->min_recording_participants = min_recording_participants;

	if (!zstr(desc)) {
		conference->desc = switch_core_strdup(conference->pool, desc);
	}

	if (!zstr(terminate_on_silence)) {
		conference->terminate_on_silence = atoi(terminate_on_silence);
	}
	if (!zstr(endconference_grace_time)) {
		conference->endconference_grace_time = atoi(endconference_grace_time);
	}

	if (!zstr(verbose_events) && switch_true(verbose_events)) {
		conference->verbose_events = 1;
	}

	/* Create the conference unique identifier */
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	conference->uuid_str = switch_core_strdup(conference->pool, uuid_str);

	/* Set enter sound and exit sound flags so that default is on */
	conference_utils_set_flag(conference, CFLAG_ENTER_SOUND);
	conference_utils_set_flag(conference, CFLAG_EXIT_SOUND);

	/* Activate the conference mutex for exclusivity */
	switch_mutex_init(&conference->mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->flag_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_thread_rwlock_create(&conference->rwlock, conference->pool);
	switch_mutex_init(&conference->member_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->canvas_mutex, SWITCH_MUTEX_NESTED, conference->pool);

	switch_mutex_lock(conference_globals.hash_mutex);
	conference_utils_set_flag(conference, CFLAG_INHASH);
	switch_core_hash_insert(conference_globals.conference_hash, conference->name, conference);
	switch_mutex_unlock(conference_globals.hash_mutex);

	conference->super_canvas_label_layers = video_super_canvas_label_layers;
	conference->super_canvas_show_all_layers = video_super_canvas_show_all_layers;

	if (video_canvas_count < 1) video_canvas_count = 1;

	if (conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) && video_canvas_count > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Personal Canvas and Multi-Canvas modes are not compatable. 1 canvas will be used.\n");
		video_canvas_count = 1;
	}

	if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX) {
		video_layout_t *vlayout = conference_video_get_layout(conference, conference->video_layout_name, conference->video_layout_group);

		if (!vlayout) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot find layout\n");
			conference->video_layout_name = conference->video_layout_group = NULL;
			conference_utils_clear_flag(conference, CFLAG_VIDEO_MUXING);
		} else {
			int j;

			for (j = 0; j < video_canvas_count; j++) {
				mcu_canvas_t *canvas = NULL;

				switch_mutex_lock(conference->canvas_mutex);
				conference_video_init_canvas(conference, vlayout, &canvas);
				conference_video_attach_canvas(conference, canvas, 0);
				conference_video_launch_muxing_thread(conference, canvas, 0);
				switch_mutex_unlock(conference->canvas_mutex);
			}

			if (conference->canvas_count > 1) {
				video_layout_t *svlayout = conference_video_get_layout(conference, NULL, CONFERENCE_MUX_DEFAULT_SUPER_LAYOUT);
				mcu_canvas_t *canvas = NULL;

				if (svlayout) {
					switch_mutex_lock(conference->canvas_mutex);
					conference_video_init_canvas(conference, svlayout, &canvas);
					conference_video_set_canvas_bgcolor(canvas, conference->video_super_canvas_bgcolor);
					conference_video_attach_canvas(conference, canvas, 1);
					conference_video_launch_muxing_thread(conference, canvas, 1);
					switch_mutex_unlock(conference->canvas_mutex);
				}
			}
		}
	}


	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
	conference_event_add_data(conference, event);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "conference-create");
	switch_event_fire(&event);

	if (conference_utils_test_flag(conference, CFLAG_LIVEARRAY_SYNC)) {
		char *p;

		if (strchr(conference->name, '@')) {
			conference->la_event_channel = switch_core_sprintf(conference->pool, "conference-liveArray.%s", conference->name);
			conference->chat_event_channel = switch_core_sprintf(conference->pool, "conference-chat.%s", conference->name);
			conference->mod_event_channel = switch_core_sprintf(conference->pool, "conference-mod.%s", conference->name);
		} else {
			conference->la_event_channel = switch_core_sprintf(conference->pool, "conference-liveArray.%s@%s", conference->name, conference->domain);
			conference->chat_event_channel = switch_core_sprintf(conference->pool, "conference-chat.%s@%s", conference->name, conference->domain);
			conference->mod_event_channel = switch_core_sprintf(conference->pool, "conference-mod.%s@%s", conference->name, conference->domain);
		}

		conference->la_name = switch_core_strdup(conference->pool, conference->name);
		if ((p = strchr(conference->la_name, '@'))) {
			*p = '\0';
		}

		switch_live_array_create(conference->la_event_channel, conference->la_name, conference_globals.event_channel_id, &conference->la);
		switch_live_array_set_user_data(conference->la, conference);
		switch_live_array_set_command_handler(conference->la, conference_event_la_command_handler);
	}

	
 end:

	switch_mutex_unlock(conference_globals.hash_mutex);

	return conference;
}

void conference_send_presence(conference_obj_t *conference)
{
	switch_event_t *event;

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conference->name);
		if (strchr(conference->name, '@')) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", conference->name);
		} else {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conference->name);

		if (conference->count) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", conference->count == 1 ? "early" : "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "presence-call-direction", conference->count == 1 ? "outbound" : "inbound");
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Inactive");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
		}



		switch_event_fire(&event);
	}

}
#if 0
uint32_t conference_kickall_matching_var(conference_obj_t *conference, const char *var, const char *val)
{
	conference_member_t *member = NULL;
	const char *vval = NULL;
	uint32_t r = 0;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel = NULL;

		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		channel = switch_core_session_get_channel(member->session);
		vval = switch_channel_get_variable(channel, var);

		if (vval && !strcmp(vval, val)) {
			conference_utils_member_set_flag_locked(member, MFLAG_KICKED);
			conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);
			switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);
			r++;
		}

	}

	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	return r;
}
#endif

void send_presence(switch_event_types_t id)
{
	switch_xml_t cxml, cfg, advertise, room;
	switch_event_t *params = NULL;

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "presence", "true");


	/* Open the config from the xml registry */
	if (!(cxml = switch_xml_open_cfg(mod_conference_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", mod_conference_cf_name);
		goto done;
	}

	if ((advertise = switch_xml_child(cfg, "advertise"))) {
		for (room = switch_xml_child(advertise, "room"); room; room = room->next) {
			char *name = (char *) switch_xml_attr_soft(room, "name");
			char *status = (char *) switch_xml_attr_soft(room, "status");
			switch_event_t *event;

			if (name && switch_event_create(&event, id) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", status ? status : "Available");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
				switch_event_fire(&event);
			}
		}
	}

 done:
	switch_event_destroy(&params);

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}
}

/* Called by FreeSWITCH when the module loads */
SWITCH_MODULE_LOAD_FUNCTION(mod_conference_load)
{
	char *p = NULL;
	switch_chat_interface_t *chat_interface;
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&conference_globals, 0, sizeof(conference_globals));

	/* Connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_console_add_complete_func("::conference::conference_list_conferences", conference_list_conferences);


	switch_event_channel_bind("conference", conference_event_channel_handler, &conference_globals.event_channel_id);
	switch_event_channel_bind("conference-liveArray", conference_event_la_channel_handler, &conference_globals.event_channel_id);
	switch_event_channel_bind("conference-mod", conference_event_mod_channel_handler, &conference_globals.event_channel_id);
	switch_event_channel_bind("conference-chat", conference_event_chat_channel_handler, &conference_globals.event_channel_id);

	if ( conference_api_sub_syntax(&api_syntax) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	/* create/register custom event message type */
	if (switch_event_reserve_subclass(CONF_EVENT_MAINT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", CONF_EVENT_MAINT);
		return SWITCH_STATUS_TERM;
	}

	/* Setup the pool */
	conference_globals.conference_pool = pool;

	/* Setup a hash to store conferences by name */
	switch_core_hash_init(&conference_globals.conference_hash);
	switch_mutex_init(&conference_globals.conference_mutex, SWITCH_MUTEX_NESTED, conference_globals.conference_pool);
	switch_mutex_init(&conference_globals.id_mutex, SWITCH_MUTEX_NESTED, conference_globals.conference_pool);
	switch_mutex_init(&conference_globals.hash_mutex, SWITCH_MUTEX_NESTED, conference_globals.conference_pool);
	switch_mutex_init(&conference_globals.setup_mutex, SWITCH_MUTEX_NESTED, conference_globals.conference_pool);

	/* Subscribe to presence request events */
	if (switch_event_bind(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, conference_event_pres_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to presence request events!\n");
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CONFERENCE_DATA_QUERY, SWITCH_EVENT_SUBCLASS_ANY, conference_data_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to conference data query events!\n");
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CALL_SETUP_REQ, SWITCH_EVENT_SUBCLASS_ANY, conference_event_call_setup_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to conference data query events!\n");
	}

	SWITCH_ADD_API(api_interface, "conference", "Conference module commands", conference_api_main, p);
	SWITCH_ADD_APP(app_interface, mod_conference_app_name, mod_conference_app_name, NULL, conference_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "conference_set_auto_outcall", "conference_set_auto_outcall", NULL, conference_auto_function, NULL, SAF_NONE);
	SWITCH_ADD_CHAT(chat_interface, CONF_CHAT_PROTO, chat_send);

	send_presence(SWITCH_EVENT_PRESENCE_IN);

	conference_globals.running = 1;
	/* indicate that the module should continue to be loaded */
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_conference_shutdown)
{
	if (conference_globals.running) {

		/* signal all threads to shutdown */
		conference_globals.running = 0;

		switch_event_channel_unbind(NULL, conference_event_channel_handler);
		switch_event_channel_unbind(NULL, conference_event_la_channel_handler);
		switch_event_channel_unbind(NULL, conference_event_mod_channel_handler);
		switch_event_channel_unbind(NULL, conference_event_chat_channel_handler);

		switch_console_del_complete_func("::conference::conference_list_conferences");

		/* wait for all threads */
		while (conference_globals.threads) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for %d threads\n", conference_globals.threads);
			switch_yield(100000);
		}

		switch_event_unbind_callback(conference_event_pres_handler);
		switch_event_unbind_callback(conference_data_event_handler);
		switch_event_unbind_callback(conference_event_call_setup_handler);
		switch_event_free_subclass(CONF_EVENT_MAINT);

		/* free api interface help ".syntax" field string */
		switch_safe_free(api_syntax);
	}
	switch_core_hash_destroy(&conference_globals.conference_hash);

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
