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

int conference_member_noise_gate_check(conference_member_t *member)
{
	int r = 0;


	if (member->conference->agc_level && member->agc_volume_in_level != 0) {
		int target_score = 0;

		target_score = (member->energy_level + (25 * member->agc_volume_in_level));

		if (target_score < 0) target_score = 0;

		r = (int)member->score > target_score;

	} else {
		r = (int32_t)member->score > member->energy_level;
	}

	return r;
}

void conference_member_clear_avg(conference_member_t *member)
{

	member->avg_score = 0;
	member->avg_itt = 0;
	member->avg_tally = 0;
	member->agc_concur = 0;
}

void conference_member_do_binding(conference_member_t *member, conference_key_callback_t handler, const char *digits, const char *data)
{
	key_binding_t *binding;

	binding = switch_core_alloc(member->pool, sizeof(*binding));
	binding->member = member;

	binding->action.binded_dtmf = switch_core_strdup(member->pool, digits);

	if (data) {
		binding->action.data = switch_core_strdup(member->pool, data);
	}

	binding->handler = handler;
	switch_ivr_dmachine_bind(member->dmachine, "conf", digits, 0, conference_loop_dmachine_dispatcher, binding);

}

void conference_member_bind_controls(conference_member_t *member, const char *controls)
{
	switch_xml_t cxml, cfg, xgroups, xcontrol;
	switch_event_t *params;
	int i;

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Conf-Name", member->conference->name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Action", "request-controls");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Controls", controls);

	if (!(cxml = switch_xml_open_cfg(mod_conference_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", mod_conference_cf_name);
		goto end;
	}

	if (!(xgroups = switch_xml_child(cfg, "caller-controls"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find caller-controls in %s\n", mod_conference_cf_name);
		goto end;
	}

	if (!(xgroups = switch_xml_find_child(xgroups, "group", "name", controls))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find group '%s' in caller-controls section of %s\n", switch_str_nil(controls), mod_conference_cf_name);
		goto end;
	}


	for (xcontrol = switch_xml_child(xgroups, "control"); xcontrol; xcontrol = xcontrol->next) {
		const char *key = switch_xml_attr(xcontrol, "action");
		const char *digits = switch_xml_attr(xcontrol, "digits");
		const char *data = switch_xml_attr_soft(xcontrol, "data");

		if (zstr(key) || zstr(digits)) continue;

		for(i = 0; i < conference_loop_mapping_len(); i++) {
			if (!strcasecmp(key, control_mappings[i].name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s binding '%s' to '%s'\n",
								  switch_core_session_get_name(member->session), digits, key);

				conference_member_do_binding(member, control_mappings[i].handler, digits, data);
			}
		}
	}

 end:

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	if (params) switch_event_destroy(&params);

}

void conference_member_update_status_field(conference_member_t *member)
{
	char *str, *vstr = "", display[128] = "", *json_display = NULL;
	cJSON *json, *audio, *video;

	if (!member->conference->la || !member->json || !member->status_field || conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
		return;
	}
	
	switch_live_array_lock(member->conference->la);

	if (!conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		str = "MUTE";
	} else if (switch_channel_test_flag(member->channel, CF_HOLD)) {
		str = "HOLD";
	} else if (member == member->conference->floor_holder) {
		if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
			str = "TALKING (FLOOR)";
		} else {
			str = "FLOOR";
		}
	} else if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
		str = "TALKING";
	} else {
		str = "ACTIVE";
	}

	if (switch_channel_test_flag(member->channel, CF_VIDEO)) {
		if (!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
			vstr = " VIDEO (BLIND)";
		} else {
			vstr = " VIDEO";
			if (member && member->id == member->conference->video_floor_holder) {
				vstr = " VIDEO (FLOOR)";
			}
		}
	}

	switch_snprintf(display, sizeof(display), "%s%s", str, vstr);

	if (conference_utils_test_flag(member->conference, CFLAG_JSON_STATUS)) {
		json = cJSON_CreateObject();
		audio = cJSON_CreateObject();
		cJSON_AddItemToObject(audio, "muted", cJSON_CreateBool(!conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)));
		cJSON_AddItemToObject(audio, "deaf", cJSON_CreateBool(!conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)));
		cJSON_AddItemToObject(audio, "onHold", cJSON_CreateBool(switch_channel_test_flag(member->channel, CF_HOLD)));
		cJSON_AddItemToObject(audio, "talking", cJSON_CreateBool(conference_utils_member_test_flag(member, MFLAG_TALKING)));
		cJSON_AddItemToObject(audio, "floor", cJSON_CreateBool(member == member->conference->floor_holder));
		cJSON_AddItemToObject(audio, "energyScore", cJSON_CreateNumber(member->score));
		cJSON_AddItemToObject(json, "audio", audio);

		if (switch_channel_test_flag(member->channel, CF_VIDEO) || member->avatar_png_img) {
			video = cJSON_CreateObject();

			if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) && 
				member->video_layer_id > -1 && switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
				cJSON_AddItemToObject(video, "visible", cJSON_CreateTrue());
			} else {
				cJSON_AddItemToObject(video, "visible", cJSON_CreateFalse());
			}

			cJSON_AddItemToObject(video, "videoOnly", cJSON_CreateBool(switch_channel_test_flag(member->channel, CF_VIDEO_ONLY)));
			if (switch_true(switch_channel_get_variable_dup(member->channel, "video_screen_share", SWITCH_FALSE, -1))) {
				cJSON_AddItemToObject(video, "screenShare", cJSON_CreateTrue());
			}

			cJSON_AddItemToObject(video, "avatarPresented", cJSON_CreateBool(!!member->avatar_png_img));
			cJSON_AddItemToObject(video, "mediaFlow", cJSON_CreateString(switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY ? "sendOnly" : "sendRecv"));
			cJSON_AddItemToObject(video, "muted", cJSON_CreateBool(!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)));
			cJSON_AddItemToObject(video, "floor", cJSON_CreateBool(member && member->id == member->conference->video_floor_holder));
			if (member && member->id == member->conference->video_floor_holder && conference_utils_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK)) {
				cJSON_AddItemToObject(video, "floorLocked", cJSON_CreateTrue());
			}
			cJSON_AddItemToObject(video, "reservationID", member->video_reservation_id ?
								  cJSON_CreateString(member->video_reservation_id) : cJSON_CreateNull());

			cJSON_AddItemToObject(video, "videoLayerID", cJSON_CreateNumber(member->video_layer_id));

			cJSON_AddItemToObject(json, "video", video);
		} else {
			cJSON_AddItemToObject(json, "video", cJSON_CreateFalse());
		}

		if (conference_utils_test_flag(member->conference, CFLAG_JSON_STATUS)) {
			cJSON_AddItemToObject(json, "oldStatus", cJSON_CreateString(display));
		}

		json_display = cJSON_PrintUnformatted(json);
		cJSON_Delete(json);
	}

	switch_safe_free(member->status_field->valuestring);

	if (json_display) {
		member->status_field->valuestring = json_display;
	} else {
		member->status_field->valuestring = strdup(display);
	}

	switch_live_array_add(member->conference->la, switch_core_session_get_uuid(member->session), -1, &member->json, SWITCH_FALSE);
	switch_live_array_unlock(member->conference->la);
}

switch_status_t conference_member_add_event_data(conference_member_t *member, switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!member)
		return status;

	if (member->conference) {
		status = conference_event_add_data(member->conference, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Floor", "%s", (member == member->conference->floor_holder) ? "true" : "false" );
	}

	if (member->session) {
		switch_channel_t *channel = switch_core_session_get_channel(member->session);

		if (member->verbose_events) {
			switch_channel_event_set_data(channel, event);
		} else {
			switch_channel_event_set_basic_data(channel, event);
		}
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Video", "%s",
								switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO) ? "true" : "false" );

	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Hear", "%s", conference_utils_member_test_flag(member, MFLAG_CAN_HEAR) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "See", "%s", conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speak", "%s", conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Talking", "%s", conference_utils_member_test_flag(member, MFLAG_TALKING) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Mute-Detect", "%s", conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-Type", "%s", conference_utils_member_test_flag(member, MFLAG_MOD) ? "moderator" : "member");
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-Ghost", "%s", conference_utils_member_test_flag(member, MFLAG_GHOST) ? "true" : "false");
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Current-Energy", "%d", member->score);

	return status;
}


#ifndef OPENAL_POSITIONING
switch_status_t conference_member_parse_position(conference_member_t *member, const char *data)
{
	return SWITCH_STATUS_FALSE;
}
#else
switch_status_t conference_member_parse_position(conference_member_t *member, const char *data)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (member->al) {
		status = conference_al_parse_position(member->al, data);
	}

	return status;

}
#endif

/* if other_member has a relationship with member, produce it */
conference_relationship_t *conference_member_get_relationship(conference_member_t *member, conference_member_t *other_member)
{
	conference_relationship_t *rel = NULL, *global = NULL;

	if (member == NULL || other_member == NULL || member->relationships == NULL)
		return NULL;

	lock_member(member);
	lock_member(other_member);

	for (rel = member->relationships; rel; rel = rel->next) {
		if (rel->id == other_member->id) {
			break;
		}

		/* 0 matches everyone. (We will still test the others because a real match carries more clout) */
		if (rel->id == 0) {
			global = rel;
		}
	}

	unlock_member(other_member);
	unlock_member(member);

	return rel ? rel : global;
}

/* stop playing a file for the member of the conference */
uint32_t conference_member_stop_file(conference_member_t *member, file_stop_t stop)
{
	conference_file_node_t *nptr;
	uint32_t count = 0;

	if (member == NULL)
		return count;


	switch_mutex_lock(member->fnode_mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = member->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}
	} else {
		if (member->fnode) {
			member->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(member->fnode_mutex);

	return count;
}



/* traverse the conference member list for the specified member id and return it's pointer */
conference_member_t *conference_member_get(conference_obj_t *conference, uint32_t id)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	if (!id) {
		return NULL;
	}

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {

		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		if (member->id == id) {
			break;
		}
	}

	if (member) {
		if (!conference_utils_member_test_flag(member, MFLAG_INTREE) ||
			conference_utils_member_test_flag(member, MFLAG_KICKED) ||
			(member->session && !switch_channel_up(switch_core_session_get_channel(member->session)))) {

			/* member is kicked or hanging up so forget it */
			member = NULL;
		}
	}

	if (member) {
		if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
			/* if you cant readlock it's way to late to do anything */
			member = NULL;
		}
	}

	switch_mutex_unlock(conference->member_mutex);

	return member;
}


/* traverse the conference member list for the specified member with var/val  and return it's pointer */
conference_member_t *conference_member_get_by_var(conference_obj_t *conference, const char *var, const char *val)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	if (!(var && val)) {
		return NULL;
	}

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		const char *check_var;
		
		if (conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		if ((check_var = switch_channel_get_variable_dup(member->channel, var , SWITCH_FALSE, -1)) && !strcmp(check_var, val)) {
			break;
		}
	}

	if (member) {
		if (!conference_utils_member_test_flag(member, MFLAG_INTREE) ||
			conference_utils_member_test_flag(member, MFLAG_KICKED) ||
			(member->session && !switch_channel_up(switch_core_session_get_channel(member->session)))) {

			/* member is kicked or hanging up so forget it */
			member = NULL;
		}
	}

	if (member) {
		if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
			/* if you cant readlock it's way to late to do anything */
			member = NULL;
		}
	}

	switch_mutex_unlock(conference->member_mutex);

	return member;
}

void conference_member_check_agc_levels(conference_member_t *member)
{
	int x = 0;

	if (!member->avg_score) return;

	if ((int)member->avg_score < member->conference->agc_level - 100) {
		member->agc_volume_in_level++;
		switch_normalize_volume_granular(member->agc_volume_in_level);
		x = 1;
	} else if ((int)member->avg_score > member->conference->agc_level + 100) {
		member->agc_volume_in_level--;
		switch_normalize_volume_granular(member->agc_volume_in_level);
		x = -1;
	}

	if (x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
						  "AGC %s:%d diff:%d level:%d cur:%d avg:%d vol:%d %s\n",
						  member->conference->name,
						  member->id, member->conference->agc_level - member->avg_score, member->conference->agc_level,
						  member->score, member->avg_score, member->agc_volume_in_level, x > 0 ? "+++" : "---");

		conference_member_clear_avg(member);
	}
}

void conference_member_check_channels(switch_frame_t *frame, conference_member_t *member, switch_bool_t in)
{
	if (member->conference->channels != member->read_impl.number_of_channels || conference_utils_member_test_flag(member, MFLAG_POSITIONAL)) {
		uint32_t rlen;
		int from, to;

		if (in) {
			to = member->conference->channels;
			from = member->read_impl.number_of_channels;
		} else {
			from = member->conference->channels;
			to = member->read_impl.number_of_channels;
		}

		rlen = frame->datalen / 2 / from;

		if (in && frame->rate == 48000 && ((from == 1 && to == 2) || (from == 2 && to == 2)) && conference_utils_member_test_flag(member, MFLAG_POSITIONAL)) {
			if (from == 2 && to == 2) {
				switch_mux_channels((int16_t *) frame->data, rlen, 2, 1);
				frame->datalen /= 2;
				rlen = frame->datalen / 2;
			}

			conference_al_process(member->al, frame->data, frame->datalen, frame->rate);
		} else {
			switch_mux_channels((int16_t *) frame->data, rlen, from, to);
		}

		frame->datalen = rlen * 2 * to;

	}
}


void conference_member_add_file_data(conference_member_t *member, int16_t *data, switch_size_t file_data_len)
{
	switch_size_t file_sample_len;
	int16_t file_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };


	switch_mutex_lock(member->fnode_mutex);

	if (!member->fnode) {
		goto done;
	}

	file_sample_len = file_data_len / 2 / member->conference->channels;

	/* if we are done, clean it up */
	if (member->fnode->done) {
		conference_file_node_t *fnode;
		switch_memory_pool_t *pool;

		if (member->fnode->type != NODE_TYPE_SPEECH) {
			conference_file_close(member->conference, member->fnode);
		}

		fnode = member->fnode;
		member->fnode = member->fnode->next;

		pool = fnode->pool;
		fnode = NULL;
		switch_core_destroy_memory_pool(&pool);
	} else if(!switch_test_flag(member->fnode, NFLAG_PAUSE)) {
		/* skip this frame until leadin time has expired */
		if (member->fnode->leadin) {
			member->fnode->leadin--;
		} else {
			if (member->fnode->type == NODE_TYPE_SPEECH) {
				switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
				switch_size_t speech_len = file_data_len;

				if (member->fnode->al) {
					speech_len /= 2;
				}

				if (switch_core_speech_read_tts(member->fnode->sh, file_frame, &speech_len, &flags) == SWITCH_STATUS_SUCCESS) {
					file_sample_len = file_data_len / 2 / member->conference->channels;
				} else {
					file_sample_len = file_data_len = 0;
				}
			} else if (member->fnode->type == NODE_TYPE_FILE) {
				switch_core_file_read(&member->fnode->fh, file_frame, &file_sample_len);
				file_data_len = file_sample_len * 2 * member->fnode->fh.channels;
			}

			if (file_sample_len <= 0) {
				member->fnode->done++;
			} else {			/* there is file node data to mix into the frame */
				uint32_t i;
				int32_t sample;

				/* Check for output volume adjustments */
				if (member->volume_out_level) {
					switch_change_sln_volume(file_frame, (uint32_t)file_sample_len * member->conference->channels, member->volume_out_level);
				}

				if (member->fnode->al) {
					conference_al_process(member->fnode->al, file_frame, file_sample_len * 2, member->conference->rate);
				}

				for (i = 0; i < (int)file_sample_len * member->conference->channels; i++) {
					if (member->fnode->mux) {
						sample = data[i] + file_frame[i];
						switch_normalize_to_16bit(sample);
						data[i] = (int16_t)sample;
					} else {
						data[i] = file_frame[i];
					}
				}

			}
		}
	}

 done:

	switch_mutex_unlock(member->fnode_mutex);
}


/* Add a custom relationship to a member */
conference_relationship_t *conference_member_add_relationship(conference_member_t *member, uint32_t id)
{
	conference_relationship_t *rel = NULL;

	if (member == NULL || id == 0 || !(rel = switch_core_alloc(member->pool, sizeof(*rel))))
		return NULL;

	rel->id = id;


	lock_member(member);
	switch_mutex_lock(member->conference->member_mutex);
	member->conference->relationship_total++;
	switch_mutex_unlock(member->conference->member_mutex);
	rel->next = member->relationships;
	member->relationships = rel;
	unlock_member(member);

	return rel;
}

/* Remove a custom relationship from a member */
switch_status_t conference_member_del_relationship(conference_member_t *member, uint32_t id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_relationship_t *rel, *last = NULL;

	if (member == NULL)
		return status;

	lock_member(member);
	for (rel = member->relationships; rel; rel = rel->next) {
		if (id == 0 || rel->id == id) {
			/* we just forget about rel here cos it was allocated by the member's pool
			   it will be freed when the member is */
			conference_member_t *omember;


			status = SWITCH_STATUS_SUCCESS;
			if (last) {
				last->next = rel->next;
			} else {
				member->relationships = rel->next;
			}

			if ((rel->flags & RFLAG_CAN_SEND_VIDEO)) {
				conference_utils_member_clear_flag(member, MFLAG_RECEIVING_VIDEO);
				if ((omember = conference_member_get(member->conference, rel->id))) {
					conference_utils_member_clear_flag(omember, MFLAG_RECEIVING_VIDEO);
					switch_thread_rwlock_unlock(omember->rwlock);
				}
			}

			switch_mutex_lock(member->conference->member_mutex);
			member->conference->relationship_total--;
			switch_mutex_unlock(member->conference->member_mutex);

			continue;
		}

		last = rel;
	}
	unlock_member(member);

	return status;
}



/* Gain exclusive access and add the member to the list */
switch_status_t conference_member_add(conference_obj_t *conference, conference_member_t *member)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event;
	char msg[512];				/* conference count announcement */
	call_list_t *call_list = NULL;
	switch_channel_t *channel;
	const char *controls = NULL, *position = NULL, *var = NULL;
	switch_bool_t has_video = switch_core_has_video();

	switch_assert(conference != NULL);
	switch_assert(member != NULL);

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	lock_member(member);
	switch_mutex_lock(conference->member_mutex);
	
	if (member->rec) {
		conference->recording_members++;
	}

	member->join_time = switch_epoch_time_now(NULL);
	member->conference = conference;
	member->next = conference->members;
	member->energy_level = conference->energy_level;
	member->score_iir = 0;
	member->verbose_events = conference->verbose_events;
	member->video_layer_id = -1;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;

	switch_queue_create(&member->dtmf_queue, 100, member->pool);

	conference->members = member;
	conference_utils_member_set_flag_locked(member, MFLAG_INTREE);
	switch_mutex_unlock(conference->member_mutex);
	conference_cdr_add(member);


	if (!conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {

		if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
			conference_utils_member_clear_flag_locked(member, MFLAG_CAN_BE_SEEN);
		}

		if (conference_utils_member_test_flag(member, MFLAG_GHOST)) {
			conference->count_ghosts++;
		} else {
			conference->count++;
		}

		if (conference_utils_member_test_flag(member, MFLAG_ENDCONF)) {
			if (conference->end_count++) {
				conference->endconference_time = 0;
			}
		}

		conference_send_presence(conference);

		channel = switch_core_session_get_channel(member->session);

		if (switch_true(switch_channel_get_variable_dup(member->channel, "video_second_screen", SWITCH_FALSE, -1))) {
			conference_utils_member_set_flag(member, MFLAG_SECOND_SCREEN);
		}

		conference_video_check_avatar(member, SWITCH_FALSE);

		if ((var = switch_channel_get_variable_dup(member->channel, "conference_join_volume_in", SWITCH_FALSE, -1))) {
			uint32_t id = atoi(var);

			if (id > -5 && id < 5) {
				member->volume_in_level = id;
			}
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "conference_join_volume_out", SWITCH_FALSE, -1))) {
			uint32_t id = atoi(var);

			if (id > -5 && id < 5) {
				member->volume_out_level = id;
			}
		}


		if ((var = switch_channel_get_variable_dup(member->channel, "conference_join_energy_level", SWITCH_FALSE, -1))) {
			uint32_t id = atoi(var);

			if (id > -5 && id < 5) {
				member->energy_level = id;
			}
		}
		
		if ((var = switch_channel_get_variable_dup(member->channel, "video_initial_canvas", SWITCH_FALSE, -1))) {
			uint32_t id = atoi(var) - 1;
			if (id < conference->canvas_count) {
				member->canvas_id = id;
				member->layer_timeout = DEFAULT_LAYER_TIMEOUT;
			}
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "video_initial_watching_canvas", SWITCH_FALSE, -1))) {
			uint32_t id = atoi(var) - 1;
			
			if (id == 0) {
				id = conference->canvas_count;
			}
			
			if (id <= conference->canvas_count && conference->canvases[id]) {
				member->watching_canvas_id = id;
			}
		}
		
		conference_video_reset_member_codec_index(member);

		if (has_video) {
			if ((var = switch_channel_get_variable_dup(member->channel, "video_mute_png", SWITCH_FALSE, -1))) {
				member->video_mute_png = switch_core_strdup(member->pool, var);
				member->video_mute_img = switch_img_read_png(member->video_mute_png, SWITCH_IMG_FMT_I420);
			}

			if ((var = switch_channel_get_variable_dup(member->channel, "video_reservation_id", SWITCH_FALSE, -1))) {
				member->video_reservation_id = switch_core_strdup(member->pool, var);
			}

			if ((var = switch_channel_get_variable(channel, "video_use_dedicated_encoder")) && switch_true(var)) {
				conference_utils_member_set_flag_locked(member, MFLAG_NO_MINIMIZE_ENCODING);
			}

			if ((var = switch_channel_get_variable(member->channel, "rtp_video_max_bandwidth_in"))) {
				member->max_bw_in = switch_parse_bandwidth_string(var);
			}
		
			if ((var = switch_channel_get_variable(member->channel, "rtp_video_max_bandwidth_out"))) {
				member->max_bw_out = switch_parse_bandwidth_string(var);

				if (member->max_bw_out < conference->video_codec_settings.video.bandwidth) {
					conference_utils_member_set_flag_locked(member, MFLAG_NO_MINIMIZE_ENCODING);
					switch_core_media_set_outgoing_bitrate(member->session, SWITCH_MEDIA_TYPE_VIDEO, member->max_bw_out);
				}
			}
		}
		
		switch_channel_set_variable_printf(channel, "conference_member_id", "%d", member->id);
		switch_channel_set_variable_printf(channel, "conference_moderator", "%s", conference_utils_member_test_flag(member, MFLAG_MOD) ? "true" : "false");
		switch_channel_set_variable_printf(channel, "conference_ghost", "%s", conference_utils_member_test_flag(member, MFLAG_GHOST) ? "true" : "false");
		switch_channel_set_variable(channel, "conference_recording", conference->record_filename);
		switch_channel_set_variable(channel, CONFERENCE_UUID_VARIABLE, conference->uuid_str);

		if (switch_channel_test_flag(channel, CF_VIDEO)) {
			/* Tell the channel to request a fresh vid frame */
			switch_core_session_video_reinit(member->session);
		}

		if (!switch_channel_get_variable(channel, "conference_call_key")) {
			char *key = switch_core_session_sprintf(member->session, "conference_%s_%s_%s",
													conference->name, conference->domain, switch_channel_get_variable(channel, "caller_id_number"));
			switch_channel_set_variable(channel, "conference_call_key", key);
		}


		if (conference_utils_test_flag(conference, CFLAG_WAIT_MOD) && conference_utils_member_test_flag(member, MFLAG_MOD)) {
			conference_utils_clear_flag(conference, CFLAG_WAIT_MOD);
		}

		if (conference->count > 1) {
			if ((conference->moh_sound && !conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) ||
				(conference_utils_test_flag(conference, CFLAG_WAIT_MOD) && !switch_true(switch_channel_get_variable(channel, "conference_permanent_wait_mod_moh")))) {
				/* stop MoH if any */
				conference_file_stop(conference, FILE_STOP_ASYNC);
			}

			if (!switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_REQ) && !zstr(conference->enter_sound)) {
				const char * enter_sound = switch_channel_get_variable(channel, "conference_enter_sound");
				if (conference_utils_test_flag(conference, CFLAG_ENTER_SOUND) && !conference_utils_member_test_flag(member, MFLAG_SILENT)) {
					if (!zstr(enter_sound)) {
						conference_file_play(conference, (char *)enter_sound, CONF_DEFAULT_LEADIN,
											 switch_core_session_get_channel(member->session), 0);
					} else {
						conference_file_play(conference, conference->enter_sound, CONF_DEFAULT_LEADIN, switch_core_session_get_channel(member->session), 0);
					}
				}
			}
		}


		call_list = (call_list_t *) switch_channel_get_private(channel, "_conference_autocall_list_");

		if (call_list) {
			char saymsg[1024];
			switch_snprintf(saymsg, sizeof(saymsg), "Auto Calling %d parties", call_list->iteration);
			conference_member_say(member, saymsg, 0);
		} else {

			if (!switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_REQ)) {
				/* announce the total number of members in the conference */
				if (conference->count >= conference->announce_count && conference->announce_count > 1) {
					switch_snprintf(msg, sizeof(msg), "There are %d callers", conference->count);
					conference_member_say(member, msg, CONF_DEFAULT_LEADIN);
				} else if (conference->count == 1 && !conference->perpetual_sound && !conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) {
					/* as long as its not a bridge_to conference, announce if person is alone */
					if (!conference_utils_test_flag(conference, CFLAG_BRIDGE_TO)) {
						if (conference->alone_sound  && !conference_utils_member_test_flag(member, MFLAG_GHOST)) {
							conference_file_stop(conference, FILE_STOP_ASYNC);
							conference_file_play(conference, conference->alone_sound, CONF_DEFAULT_LEADIN,
												 switch_core_session_get_channel(member->session), 0);
						} else {
							switch_snprintf(msg, sizeof(msg), "You are currently the only person in this conference.");
							conference_member_say(member, msg, CONF_DEFAULT_LEADIN);
						}
					}
				}
			}
		}

		if (conference->min && conference->count >= conference->min) {
			conference_utils_set_flag(conference, CFLAG_ENFORCE_MIN);
		}

		if (!switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_REQ) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_member_add_event_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "add-member");
			switch_event_fire(&event);
		}

		switch_channel_clear_app_flag_key("conference_silent", channel, CONF_SILENT_REQ);
		switch_channel_set_app_flag_key("conference_silent", channel, CONF_SILENT_DONE);


		if ((position = switch_channel_get_variable(channel, "conference_position"))) {

			if (conference->channels == 2) {
				if (conference_utils_member_test_flag(member, MFLAG_NO_POSITIONAL)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "%s has positional audio blocked.\n", switch_channel_get_name(channel));
				} else {
					if (conference_member_parse_position(member, position) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,	"%s invalid position data\n", switch_channel_get_name(channel));
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,	"%s position data set\n", switch_channel_get_name(channel));
					}

					conference_utils_member_set_flag(member, MFLAG_POSITIONAL);
					member->al = conference_al_create(member->pool);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,	"%s cannot set position data on mono conference.\n", switch_channel_get_name(channel));
			}
		}



		controls = switch_channel_get_variable(channel, "conference_controls");

		if (zstr(controls)) {
			if (!conference_utils_member_test_flag(member, MFLAG_MOD) || !conference->moderator_controls) {
				controls = conference->caller_controls;
			} else {
				controls = conference->moderator_controls;
			}
		}

		if (zstr(controls)) {
			controls = "default";
		}

		if (strcasecmp(controls, "none")) {
			switch_ivr_dmachine_create(&member->dmachine, "mod_conference", NULL,
									   conference->ivr_dtmf_timeout, conference->ivr_input_timeout, NULL, NULL, NULL);
			conference_member_bind_controls(member, controls);
		}

	}

	unlock_member(member);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);

	if (conference->la && member->channel) {
		if (!conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
			cJSON *dvars;
			switch_event_t *var_event;
			switch_event_header_t *hi;
			
			member->json = cJSON_CreateArray();
			cJSON_AddItemToArray(member->json, cJSON_CreateStringPrintf("%0.4d", member->id));
			cJSON_AddItemToArray(member->json, cJSON_CreateString(switch_channel_get_variable(member->channel, "caller_id_number")));
			cJSON_AddItemToArray(member->json, cJSON_CreateString(switch_channel_get_variable(member->channel, "caller_id_name")));

			cJSON_AddItemToArray(member->json, cJSON_CreateStringPrintf("%s@%s",
																		switch_channel_get_variable(member->channel, "original_read_codec"),
																		switch_channel_get_variable(member->channel, "original_read_rate")
																		));
			member->status_field = cJSON_CreateString("");
			cJSON_AddItemToArray(member->json, member->status_field);

			if (conference_utils_test_flag(member->conference, CFLAG_JSON_STATUS)) {
				switch_channel_get_variables(member->channel, &var_event);

				dvars = cJSON_CreateObject();

				for (hi = var_event->headers; hi; hi = hi->next) {
					if (!strncasecmp(hi->name, "verto_dvar_", 11)) {
						char *var = hi->name + 11;
						
						if (var) {
							cJSON_AddItemToObject(dvars, var, cJSON_CreateString(hi->value));
						}
					}
				}

				cJSON_AddItemToArray(member->json, dvars);

				switch_event_destroy(&var_event);
			}

			cJSON_AddItemToArray(member->json, cJSON_CreateNull());

			conference_member_update_status_field(member);
			//switch_live_array_add_alias(conference->la, switch_core_session_get_uuid(member->session), "conference");
		}

		conference_event_adv_la(conference, member, SWITCH_TRUE);

		if (!conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
			switch_live_array_add(conference->la, switch_core_session_get_uuid(member->session), -1, &member->json, SWITCH_FALSE);
		}

	}


	if (conference_utils_test_flag(conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(conference, NULL);
	}


	conference_event_send_rfc(conference);
	conference_event_send_json(conference);

	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

	conference_video_find_floor(member, SWITCH_TRUE);


	if (conference_utils_member_test_flag(member, MFLAG_JOIN_VID_FLOOR)) {
		conference_video_set_floor_holder(conference, member, SWITCH_TRUE);
		conference_utils_set_flag(member->conference, CFLAG_VID_FLOOR_LOCK);

		if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor %d %s\n",
							  conference->name, member->id, switch_channel_get_name(member->channel));
		}
	}

	return status;
}

void conference_member_set_floor_holder(conference_obj_t *conference, conference_member_t *member)
{
	switch_event_t *event;
	conference_member_t *old_member = NULL;
	int old_id = 0;

	if (conference->floor_holder) {
		if (conference->floor_holder == member) {
			return;
		} else {
			old_member = conference->floor_holder;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Dropping floor %s\n",
							  switch_channel_get_name(old_member->channel));

		}
	}

	switch_mutex_lock(conference->mutex);
	if (member) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding floor %s\n",
						  switch_channel_get_name(member->channel));

		conference->floor_holder = member;
		conference_member_update_status_field(member);
	} else {
		conference->floor_holder = NULL;
	}


	if (old_member) {
		old_id = old_member->id;
		conference_member_update_status_field(old_member);
		old_member->floor_packets = 0;
	}

	conference_utils_set_flag(conference, CFLAG_FLOOR_CHANGE);
	switch_mutex_unlock(conference->mutex);

	if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "floor-change");
		if (old_id) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Old-ID", "%d", old_id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-ID", "none");
		}

		if (conference->floor_holder) {
			conference_member_add_event_data(conference->floor_holder, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-ID", "%d", conference->floor_holder->id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-ID", "none");
		}

		switch_event_fire(&event);
	}

}

/* Gain exclusive access and remove the member from the list */
switch_status_t conference_member_del(conference_obj_t *conference, conference_member_t *member)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_member_t *imember, *last = NULL;
	switch_event_t *event;
	conference_file_node_t *member_fnode;
	switch_speech_handle_t *member_sh;
	const char *exit_sound = NULL;

	switch_assert(conference != NULL);
	switch_assert(member != NULL);

	switch_thread_rwlock_wrlock(member->rwlock);

	if (member->session && (exit_sound = switch_channel_get_variable(switch_core_session_get_channel(member->session), "conference_exit_sound"))) {
		conference_file_play(conference, (char *)exit_sound, CONF_DEFAULT_LEADIN,
							 switch_core_session_get_channel(member->session), 0);
	}


	lock_member(member);

	conference_member_del_relationship(member, 0);

	conference_cdr_del(member);

#ifdef OPENAL_POSITIONING
	if (member->al && member->al->device) {
		conference_al_close(member->al);
	}
#endif

	member_fnode = member->fnode;
	member_sh = member->sh;
	member->fnode = NULL;
	member->sh = NULL;
	unlock_member(member);

	if (member->dmachine) {
		switch_ivr_dmachine_destroy(&member->dmachine);
	}

	member->avatar_patched = 0;
	switch_img_free(&member->avatar_png_img);
	switch_img_free(&member->video_mute_img);
	switch_img_free(&member->pcanvas_img);
	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	lock_member(member);
	conference_utils_member_clear_flag(member, MFLAG_INTREE);

	
	switch_safe_free(member->text_framedata);
	member->text_framesize = 0;
	if (member->text_buffer) {
		switch_buffer_destroy(&member->text_buffer);
	}

	if (member->rec) {
		conference->recording_members--;
	}

	for (imember = conference->members; imember; imember = imember->next) {
		if (imember == member) {
			if (last) {
				last->next = imember->next;
			} else {
				conference->members = imember->next;
			}
			break;
		}
		last = imember;
	}

	switch_thread_rwlock_unlock(member->rwlock);

	/* Close Unused Handles */
	if (member_fnode) {
		conference_file_node_t *fnode, *cur;
		switch_memory_pool_t *pool;

		fnode = member_fnode;
		while (fnode) {
			cur = fnode;
			fnode = fnode->next;

			if (cur->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, cur);
			}

			pool = cur->pool;
			switch_core_destroy_memory_pool(&pool);
		}
	}

	if (member_sh) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&member->lsh, &flags);
	}

	if (member == member->conference->floor_holder) {
		conference_member_set_floor_holder(member->conference, NULL);
	}

	if (member->id == member->conference->video_floor_holder) {
		conference_utils_clear_flag(member->conference, CFLAG_VID_FLOOR_LOCK);
		if (member->conference->last_video_floor_holder) {
			member->conference->video_floor_holder = member->conference->last_video_floor_holder;
			member->conference->last_video_floor_holder = 0;
		}
		member->conference->video_floor_holder = 0;
	}

	if (!conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
		switch_channel_t *channel = switch_core_session_get_channel(member->session);
		if (conference_utils_member_test_flag(member, MFLAG_GHOST)) {
			conference->count_ghosts--;
		} else {
			conference->count--;
		}

		conference_video_check_flush(member, SWITCH_FALSE);

		if (conference_utils_member_test_flag(member, MFLAG_ENDCONF)) {
			if (!--conference->end_count) {
				//conference_utils_set_flag_locked(conference, CFLAG_DESTRUCT);
				conference->endconference_time = switch_epoch_time_now(NULL);
			}
		}

		conference_send_presence(conference);
		switch_channel_set_variable(channel, "conference_call_key", NULL);

		if ((conference->min && conference_utils_test_flag(conference, CFLAG_ENFORCE_MIN) && (conference->count + conference->count_ghosts) < conference->min)
			|| (conference_utils_test_flag(conference, CFLAG_DYNAMIC) && (conference->count + conference->count_ghosts == 0))) {
			conference_utils_set_flag(conference, CFLAG_DESTRUCT);
		} else {
			if (!switch_true(switch_channel_get_variable(channel, "conference_permanent_wait_mod_moh")) && conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) {
				/* Stop MOH if any */
				conference_file_stop(conference, FILE_STOP_ASYNC);
			}
			if (!exit_sound && conference->exit_sound && conference_utils_test_flag(conference, CFLAG_EXIT_SOUND) && !conference_utils_member_test_flag(member, MFLAG_SILENT)) {
				conference_file_play(conference, conference->exit_sound, 0, channel, 0);
			}
			if (conference->count == 1 && conference->alone_sound && !conference_utils_test_flag(conference, CFLAG_WAIT_MOD) && !conference_utils_member_test_flag(member, MFLAG_GHOST)) {
				conference_file_stop(conference, FILE_STOP_ASYNC);
				conference_file_play(conference, conference->alone_sound, 0, channel, 0);
			}
		}

		if (test_eflag(conference, EFLAG_DEL_MEMBER) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_member_add_event_data(member, event);
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "del-member");
			switch_event_fire(&event);
		}
	}

	conference_video_find_floor(member, SWITCH_FALSE);
	conference_video_detach_video_layer(member);

	if (member->canvas) {
		conference_video_destroy_canvas(&member->canvas);
	}

	member->conference = NULL;

	switch_mutex_unlock(conference->member_mutex);
	unlock_member(member);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);


	if (conference->la && member->session) {
		switch_live_array_del(conference->la, switch_core_session_get_uuid(member->session));
		//switch_live_array_clear_alias(conference->la, switch_core_session_get_uuid(member->session), "conference");
		conference_event_adv_la(conference, member, SWITCH_FALSE);
	}

	conference_event_send_rfc(conference);
	conference_event_send_json(conference);

	if (conference_utils_test_flag(conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(conference, NULL);
	}

	if (member->session) {
		switch_core_media_hard_mute(member->session, SWITCH_FALSE);
	}

	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

	return status;
}

void conference_member_send_all_dtmf(conference_member_t *member, conference_obj_t *conference, const char *dtmf)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);

	for (imember = conference->members; imember; imember = imember->next) {
		/* don't send to self */
		if (imember->id == member->id) {
			continue;
		}
		if (imember->session) {
			const char *p;
			for (p = dtmf; p && *p; p++) {
				switch_dtmf_t *dt, digit = { *p, SWITCH_DEFAULT_DTMF_DURATION };

				switch_zmalloc(dt, sizeof(*dt));
				*dt = digit;
				switch_queue_push(imember->dtmf_queue, dt);
				switch_core_session_kill_channel(imember->session, SWITCH_SIG_BREAK);
			}
		}
	}

	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);
}


/* Play a file in the conference room to a member */
switch_status_t conference_member_play_file(conference_member_t *member, char *file, uint32_t leadin, switch_bool_t mux)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *dfile = NULL, *expanded = NULL;
	conference_file_node_t *fnode, *nptr = NULL;
	switch_memory_pool_t *pool;
	int channels = member->conference->channels;
	int bad_params = 0;

	if (member == NULL || file == NULL || conference_utils_member_test_flag(member, MFLAG_KICKED))
		return status;

	if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(member->session), file)) != file) {
		file = expanded;
	} else {
		expanded = NULL;
	}
	if (!strncasecmp(file, "say:", 4)) {
		if (!zstr(file + 4)) {
			status = conference_member_say(member, file + 4, leadin);
		}
		goto done;
	}
	if (!switch_is_file_path(file)) {
		if (member->conference->sound_prefix) {
			if (!(dfile = switch_mprintf("%s%s%s", member->conference->sound_prefix, SWITCH_PATH_SEPARATOR, file))) {
				goto done;
			}
			file = dfile;
		} else if (!zstr(file)) {
			status = conference_member_say(member, file, leadin);
			goto done;
		}
	}
	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Pool Failure\n");
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}
	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	fnode->conference = member->conference;
	fnode->layer_id = -1;
	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;
	fnode->mux = mux;
	fnode->member_id = member->id;

	if (switch_stristr("position=", file)) {
		/* positional requires mono input */
		fnode->fh.channels = channels = 1;
	}

 retry:

	/* Open the file */
	fnode->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;
	if (switch_core_file_open(&fnode->fh,
							  file, (uint8_t) channels, member->conference->rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  pool) != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_NOTFOUND;
		goto done;
	}
	fnode->pool = pool;
	fnode->file = switch_core_strdup(fnode->pool, file);

	if (fnode->fh.params) {
		const char *position = switch_event_get_header(fnode->fh.params, "position");

		if (!bad_params && !zstr(position) && member->conference->channels == 2) {
			fnode->al = conference_al_create(pool);
			if (conference_al_parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&fnode->fh);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Invalid Position Data.\n");
				fnode->al = NULL;
				channels = member->conference->channels;
				bad_params = 1;
				goto retry;
			}
		}
	}

	/* Queue the node */
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Queueing file '%s' for play\n", file);
	switch_mutex_lock(member->fnode_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);
	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}
	switch_mutex_unlock(member->fnode_mutex);
	status = SWITCH_STATUS_SUCCESS;

 done:

	switch_safe_free(expanded);
	switch_safe_free(dfile);

	return status;
}

/* Say some thing with TTS in the conference room */
switch_status_t conference_member_say(conference_member_t *member, char *text, uint32_t leadin)
{
	conference_obj_t *conference = member->conference;
	conference_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *fp = NULL;
	int channels = member->conference->channels;
	switch_event_t *params = NULL;
	const char *position = NULL;

	if (member == NULL || zstr(text))
		return SWITCH_STATUS_FALSE;

	switch_assert(conference != NULL);

	if (!(conference->tts_engine && conference->tts_voice)) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Alloc Failure\n");
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
	fnode->pool = pool;


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


	if (member->sh && member->last_speech_channels != channels) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&member->lsh, &flags);
		member->sh = NULL;
	}

	if (!member->sh) {
		memset(&member->lsh, 0, sizeof(member->lsh));
		if (switch_core_speech_open(&member->lsh, conference->tts_engine, conference->tts_voice,
									conference->rate, conference->interval, channels, &flags, switch_core_session_get_pool(member->session)) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
		member->last_speech_channels = channels;
		member->sh = &member->lsh;
	}

	/* Queue the node */
	switch_mutex_lock(member->fnode_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}

	fnode->sh = member->sh;
	/* Begin Generation */
	switch_sleep(200000);

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

	switch_core_speech_feed_tts(fnode->sh, text, &flags);
	switch_mutex_unlock(member->fnode_mutex);

	status = SWITCH_STATUS_SUCCESS;

 end:

	if (params) {
		switch_event_destroy(&params);
	}

	return status;
}


/* execute a callback for every member of the conference */
void conference_member_itterator(conference_obj_t *conference, switch_stream_handle_t *stream, uint8_t non_mod, conference_api_member_cmd_t pfncallback, void *data)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	switch_assert(pfncallback != NULL);

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (!(non_mod && conference_utils_member_test_flag(member, MFLAG_MOD))) {
			if (member->session && !conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
				pfncallback(member, stream, data);
			}
		} else {
			stream->write_function(stream, "Skipping moderator (member id %d).\n", member->id);
		}
	}
	switch_mutex_unlock(conference->member_mutex);
}


int conference_member_get_canvas_id(conference_member_t *member, const char *val, switch_bool_t watching)
{
	int index = -1;
	int cur;

	if (watching) {
		cur = member->watching_canvas_id;
	} else {
		cur = member->canvas_id;
	}

	if (!val) {
		return -1;
	}

	if (switch_is_number(val)) {
		index = atoi(val) - 1;

		if (index < 0) {
			index = 0;
		}
	} else {
		index = cur;

		if (!strcasecmp(val, "next")) {
			index++;
		} else if (!strcasecmp(val, "prev")) {
			index--;
		}
	}

	if (watching) {
		if (index < 0) {
			index = member->conference->canvas_count;
		} else if ((uint32_t)index > member->conference->canvas_count || !member->conference->canvases[index]) {
			index = 0;
		}
	} else {
		if (index < 0) {
			index = member->conference->canvas_count;
		} else if ((uint32_t)index >= member->conference->canvas_count || !member->conference->canvases[index]) {
			index = 0;
		}
	}

	if (index > MAX_CANVASES || index < 0) {
		return -1;
	}

	if (member->conference->canvas_count > 1) {
		if ((uint32_t)index > member->conference->canvas_count) {
			return -1;
		}
	} else {
		if ((uint32_t)index >= member->conference->canvas_count) {
			return -1;
		}
	}

	return index;
}


int conference_member_setup_media(conference_member_t *member, conference_obj_t *conference)
{
	switch_codec_implementation_t read_impl = { 0 };

	switch_mutex_lock(member->audio_out_mutex);

	switch_core_session_get_read_impl(member->session, &read_impl);

	if (switch_core_codec_ready(&member->read_codec)) {
		switch_core_codec_destroy(&member->read_codec);
		memset(&member->read_codec, 0, sizeof(member->read_codec));
	}

	if (switch_core_codec_ready(&member->write_codec)) {
		switch_core_codec_destroy(&member->write_codec);
		memset(&member->write_codec, 0, sizeof(member->write_codec));
	}

	if (member->read_resampler) {
		switch_resample_destroy(&member->read_resampler);
	}

	switch_core_session_get_real_read_impl(member->session, &member->orig_read_impl);
	member->native_rate = member->orig_read_impl.samples_per_second;

	/* Setup a Signed Linear codec for reading audio. */
	if (switch_core_codec_init(&member->read_codec,
							   "L16",
							   NULL, NULL, read_impl.actual_samples_per_second, read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, member->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
						  "Raw Codec Activation Success L16@%uhz %d channel %dms\n",
						  read_impl.actual_samples_per_second, read_impl.number_of_channels, read_impl.microseconds_per_packet / 1000);

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz %d channel %dms\n",
						  read_impl.actual_samples_per_second, read_impl.number_of_channels, read_impl.microseconds_per_packet / 1000);

		goto done;
	}

	if (!member->frame_size) {
		member->frame_size = SWITCH_RECOMMENDED_BUFFER_SIZE;
		member->frame = switch_core_alloc(member->pool, member->frame_size);
		member->mux_frame = switch_core_alloc(member->pool, member->frame_size);
	}

	if (read_impl.actual_samples_per_second != conference->rate) {
		if (switch_resample_create(&member->read_resampler,
								   read_impl.actual_samples_per_second,
								   conference->rate, member->frame_size, SWITCH_RESAMPLE_QUALITY, read_impl.number_of_channels) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Unable to create resampler!\n");
			goto done;
		}


		member->resample_out = switch_core_alloc(member->pool, member->frame_size);
		member->resample_out_len = member->frame_size;

		/* Setup an audio buffer for the resampled audio */
		if (!member->resample_buffer && switch_buffer_create_dynamic(&member->resample_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX)
			!= SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
			goto done;
		}
	}


	/* Setup a Signed Linear codec for writing audio. */
	if (switch_core_codec_init(&member->write_codec,
							   "L16",
							   NULL,
							   NULL,
							   conference->rate,
							   read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, member->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
						  "Raw Codec Activation Success L16@%uhz %d channel %dms\n",
						  conference->rate, conference->channels, read_impl.microseconds_per_packet / 1000);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz %d channel %dms\n",
						  conference->rate, conference->channels, read_impl.microseconds_per_packet / 1000);
		goto codec_done2;
	}

	/* Setup an audio buffer for the incoming audio */
	if (!member->audio_buffer && switch_buffer_create_dynamic(&member->audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (!member->mux_buffer && switch_buffer_create_dynamic(&member->mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	switch_mutex_unlock(member->audio_out_mutex);

	return 0;

 codec_done1:
	switch_core_codec_destroy(&member->read_codec);
 codec_done2:
	switch_core_codec_destroy(&member->write_codec);
 done:

	switch_mutex_unlock(member->audio_out_mutex);

	return -1;


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
