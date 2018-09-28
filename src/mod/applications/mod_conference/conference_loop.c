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

struct _mapping control_mappings[] = {
	{"mute", conference_loop_mute_toggle},
	{"mute on", conference_loop_mute_on},
	{"mute off", conference_loop_mute_off},
	{"moh toggle", conference_loop_moh_toggle},
	{"border", conference_loop_border},
	{"vmute", conference_loop_vmute_toggle},
	{"vmute on", conference_loop_vmute_on},
	{"vmute off", conference_loop_vmute_off},
	{"vmute snap", conference_loop_conference_video_vmute_snap},
	{"vmute snapoff", conference_loop_conference_video_vmute_snapoff},
	{"deaf mute", conference_loop_deafmute_toggle},
	{"energy up", conference_loop_energy_up},
	{"energy equ", conference_loop_energy_equ_conf},
	{"energy dn", conference_loop_energy_dn},
	{"vol talk up", conference_loop_volume_talk_up},
	{"vol talk zero", conference_loop_volume_talk_zero},
	{"vol talk dn", conference_loop_volume_talk_dn},
	{"vol listen up", conference_loop_volume_listen_up},
	{"vol listen zero", conference_loop_volume_listen_zero},
	{"vol listen dn", conference_loop_volume_listen_dn},
	{"hangup", conference_loop_hangup},
	{"event", conference_loop_event},
	{"lock", conference_loop_lock_toggle},
	{"transfer", conference_loop_transfer},
	{"execute_application", conference_loop_exec_app},
	{"floor", conference_loop_floor_toggle},
	{"vid-floor", conference_loop_vid_floor_toggle},
	{"vid-floor-force", conference_loop_vid_floor_force},
	{"deaf", conference_loop_deaf_toggle},
	{"deaf on", conference_loop_deaf_on},
	{"deaf off", conference_loop_deaf_off}
};

int conference_loop_mapping_len()
{
	return (sizeof(control_mappings)/sizeof(control_mappings[0]));
}

switch_status_t conference_loop_dmachine_dispatcher(switch_ivr_dmachine_match_t *match)
{
	key_binding_t *binding = match->user_data;
	switch_channel_t *channel;

	if (!binding) return SWITCH_STATUS_FALSE;

	channel = switch_core_session_get_channel(binding->member->session);
	switch_channel_set_variable(channel, "conference_last_matching_digits", match->match_digits);

	if (binding->action.data) {
		binding->action.expanded_data = switch_channel_expand_variables(channel, binding->action.data);
	}

	binding->handler(binding->member, &binding->action);

	if (binding->action.expanded_data != binding->action.data) {
		free(binding->action.expanded_data);
		binding->action.expanded_data = NULL;
	}

	conference_utils_member_set_flag_locked(binding->member, MFLAG_FLUSH_BUFFER);

	return SWITCH_STATUS_SUCCESS;
}

void conference_loop_floor_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conference_api_sub_floor(member, NULL, NULL);
}

void conference_loop_vid_floor_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conference_api_sub_vid_floor(member, NULL, NULL);
}

void conference_loop_vid_floor_force(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conference_api_sub_vid_floor(member, NULL, "force");
}

void conference_loop_mute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conference_api_sub_mute(member, NULL, NULL);
	} else {
		conference_api_sub_unmute(member, NULL, NULL);
		if (!conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
			conference_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

void conference_loop_mute_on(conference_member_t *member, caller_control_action_t *action)
{
	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conference_api_sub_mute(member, NULL, NULL);
	}
}

void conference_loop_mute_off(conference_member_t *member, caller_control_action_t *action)
{
	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) return;

	if (!conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conference_api_sub_unmute(member, NULL, NULL);
		if (!conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
			conference_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

void conference_loop_conference_video_vmute_snap(conference_member_t *member, caller_control_action_t *action)
{
	conference_video_vmute_snap(member, SWITCH_FALSE);
}

void conference_loop_conference_video_vmute_snapoff(conference_member_t *member, caller_control_action_t *action)
{
	conference_video_vmute_snap(member, SWITCH_TRUE);
}

void conference_loop_moh_toggle(conference_member_t *member, caller_control_action_t *action)
{
	conference_api_set_moh(member->conference, "toggle");
}

void conference_loop_border(conference_member_t *member, caller_control_action_t *action)
{
	conference_api_sub_vid_border(member, NULL, action->expanded_data);
}

void conference_loop_vmute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conference_api_sub_vmute(member, NULL, NULL);
	} else {
		conference_api_sub_unvmute(member, NULL, NULL);
	}
}

void conference_loop_vmute_on(conference_member_t *member, caller_control_action_t *action)
{
	if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conference_api_sub_vmute(member, NULL, NULL);
	}
}

void conference_loop_vmute_off(conference_member_t *member, caller_control_action_t *action)
{
	if (!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conference_api_sub_unvmute(member, NULL, NULL);
	}
}

void conference_loop_lock_toggle(conference_member_t *member, caller_control_action_t *action)
{
	switch_event_t *event;

	if (member == NULL)
		return;

	if (conference_utils_test_flag(member->conference, CFLAG_WAIT_MOD) && !conference_utils_member_test_flag(member, MFLAG_MOD) )
		return;

	if (!conference_utils_test_flag(member->conference, CFLAG_LOCKED)) {
		if (member->conference->is_locked_sound) {
			conference_file_play(member->conference, member->conference->is_locked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		conference_utils_set_flag_locked(member->conference, CFLAG_LOCKED);
		if (test_eflag(member->conference, EFLAG_LOCK) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(member->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "lock");
			switch_event_fire(&event);
		}
	} else {
		if (member->conference->is_unlocked_sound) {
			conference_file_play(member->conference, member->conference->is_unlocked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		conference_utils_clear_flag_locked(member->conference, CFLAG_LOCKED);
		if (test_eflag(member->conference, EFLAG_UNLOCK) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(member->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unlock");
			switch_event_fire(&event);
		}
	}

}

void conference_loop_deaf_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
		conference_api_sub_deaf(member, NULL, NULL);
	} else {
		conference_api_sub_undeaf(member, NULL, NULL);
	}
}

void conference_loop_deaf_on(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
		conference_api_sub_deaf(member, NULL, NULL);
	}
}

void conference_loop_deaf_off(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (!conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
		conference_api_sub_undeaf(member, NULL, NULL);
	}
}

void conference_loop_deafmute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) return;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conference_api_sub_mute(member, NULL, NULL);
		if (conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
			conference_api_sub_deaf(member, NULL, NULL);
		}
	} else {
		conference_api_sub_unmute(member, NULL, NULL);
		if (!conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
			conference_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

void conference_loop_energy_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "";
	switch_event_t *event;
	char *p;

	if (member == NULL)
		return;


	member->energy_level += 200;
	if (member->energy_level > 1800) {
		member->energy_level = 1800;
	}

	if (member->auto_energy_level && member->energy_level > member->auto_energy_level) {
		member->auto_energy_level = 0;
	}

	if (member->max_energy_level && member->energy_level > member->max_energy_level) {
		member->max_energy_level = 0;
	}


	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}




}

void conference_loop_energy_equ_conf(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "", *p;
	switch_event_t *event;

	if (member == NULL)
		return;

	member->energy_level = member->conference->energy_level;


	if (member->auto_energy_level && member->energy_level > member->auto_energy_level) {
		member->auto_energy_level = 0;
	}

	if (member->max_energy_level && member->energy_level > member->max_energy_level) {
		member->max_energy_level = 0;
	}

	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

}

void conference_loop_energy_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "", *p;
	switch_event_t *event;

	if (member == NULL)
		return;

	member->energy_level -= 200;
	if (member->energy_level < 0) {
		member->energy_level = 0;
	}
	
	if (member->auto_energy_level && member->energy_level > member->auto_energy_level) {
		member->auto_energy_level = 0;
	}

	if (member->max_energy_level && member->energy_level > member->max_energy_level) {
		member->max_energy_level = 0;
	}

	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

}

void conference_loop_volume_talk_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level++;
	switch_normalize_volume(member->volume_out_level);

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);

}

void conference_loop_volume_talk_zero(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level = 0;

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);


	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

void conference_loop_volume_talk_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level--;
	switch_normalize_volume(member->volume_out_level);

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

void conference_loop_volume_listen_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level++;
	switch_normalize_volume(member->volume_in_level);

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);

}

void conference_loop_volume_listen_zero(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level = 0;

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);

}

void conference_loop_volume_listen_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level--;
	switch_normalize_volume(member->volume_in_level);

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

void conference_loop_event(conference_member_t *member, caller_control_action_t *action)
{
	switch_event_t *event;
	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "dtmf");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "DTMF-Key", action->binded_dtmf);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Data", action->expanded_data);
		switch_event_fire(&event);
	}
}

void conference_loop_transfer(conference_member_t *member, caller_control_action_t *action)
{
	char *exten = NULL;
	char *dialplan = "XML";
	char *context = "default";

	char *argv[3] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_event_t *event;

	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "transfer");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Dialplan", action->expanded_data);
		switch_event_fire(&event);
	}
	conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);

	if ((mydata = switch_core_session_strdup(member->session, action->expanded_data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			if (argc > 0) {
				exten = argv[0];
			}
			if (argc > 1) {
				dialplan = argv[1];
			}
			if (argc > 2) {
				context = argv[2];
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Empty transfer string [%s]\n", (char *) action->expanded_data);
			goto done;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Unable to allocate memory to duplicate transfer data.\n");
		goto done;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_INFO, "Transfering to: %s, %s, %s\n", exten, dialplan, context);

	switch_ivr_session_transfer(member->session, exten, dialplan, context);

 done:
	return;
}

void conference_loop_exec_app(conference_member_t *member, caller_control_action_t *action)
{
	char *app = NULL;
	char *arg = "";

	char *argv[2] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_event_t *event = NULL;
	switch_channel_t *channel = NULL;

	if (!action->expanded_data) return;

	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "execute_app");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", action->expanded_data);
		switch_event_fire(&event);
	}

	mydata = strdup(action->expanded_data);
	switch_assert(mydata);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc > 0) {
			app = argv[0];
		}
		if (argc > 1) {
			arg = argv[1];
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Empty execute app string [%s]\n",
						  (char *) action->expanded_data);
		goto done;
	}

	if (!app) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Unable to find application.\n");
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_INFO, "Execute app: %s, %s\n", app, arg);

	channel = switch_core_session_get_channel(member->session);

	switch_channel_set_app_flag(channel, CF_APP_TAGGED);
	switch_core_session_set_read_codec(member->session, NULL);
	switch_core_session_execute_application(member->session, app, arg);
	switch_core_session_set_read_codec(member->session, &member->read_codec);
	switch_channel_clear_app_flag(channel, CF_APP_TAGGED);

 done:

	switch_safe_free(mydata);

	return;
}

void conference_loop_hangup(conference_member_t *member, caller_control_action_t *action)
{
	conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);
}

static void stop_talking_handler(conference_member_t *member)
{
	switch_event_t *event;
	double avg = 0, avg2 = 0, gcp = 0, ngcp = 0, pct = 0;

	member->auto_energy_track = 0;

	if (member->score_count && member->talking_count) {		
		int duration_ms = member->talking_count * member->conference->interval;
		avg = (double)member->score_delta_accum / member->score_count;
		avg2 = (double)member->score_accum / member->score_count;


		if (!member->nogate_count) member->nogate_count = 1;
		if (!member->gate_count) member->gate_count = 1;

		pct = ((float)member->nogate_count / (float)member->gate_count) * 100;
		gcp = ((double)member->gate_count / member->talking_count) * 100;
		ngcp = ((double)member->nogate_count / member->talking_count) * 100;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "SCORE AVG %f/%f %d GC %d NGC %d GC %% %f NGC %% %f DIFF %f EL %d MS %d PCT %f\n", 
			   avg2, avg, member->score_count, member->gate_count, 
			   member->nogate_count, 
			   gcp, ngcp, gcp - ngcp, member->energy_level, duration_ms, pct);


		if (member->auto_energy_level) {
			if (duration_ms > 2000 && pct > 1) {
				int new_level = (int)(avg2 *.75);
				if (new_level > member->auto_energy_level) {
					new_level = member->auto_energy_level;
				}
				member->energy_level = new_level;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "SET ENERGY %d\n", new_level);
			}
		}

	}
	
	member->gate_open = 0;
	member->nogate_count = 0;
	member->gate_count = 0;

	if (test_eflag(member->conference, EFLAG_STOP_TALKING) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		if (avg) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-gate-hits", "%u", member->score_count);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-total-packets", "%u", member->talking_count);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-duration-ms", "%u", member->talking_count * member->conference->interval);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-average-energy", "%f", avg2);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-delta-average", "%f", avg);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-hit-on-percent", "%f", gcp);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-non-hit-ratio", "%f", pct);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-hit-off-percent", "%f", ngcp);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talking-hit-off-differential", "%f", gcp - ngcp);
		}
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
		switch_event_fire(&event);
	}

	
}

/* marshall frames from the call leg to the conference thread for muxing to other call legs */
void *SWITCH_THREAD_FUNC conference_loop_input(switch_thread_t *thread, void *obj)
{
	switch_event_t *event;
	conference_member_t *member = obj;
	switch_channel_t *channel;
	switch_status_t status;
	switch_frame_t *read_frame = NULL;
	uint32_t hangover = 40, hangunder = 5, hangover_hits = 0, hangunder_hits = 0, diff_level = 400;
	switch_core_session_t *session = member->session;
	uint32_t flush_len;
	switch_frame_t tmp_frame = { 0 };

	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	switch_assert(member != NULL);

	conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);

	channel = switch_core_session_get_channel(session);

	switch_core_session_get_read_impl(session, &member->read_impl);

	switch_channel_audio_sync(channel);

	flush_len = switch_samples_per_packet(member->conference->rate, member->conference->interval) * 2 * member->conference->channels * (500 / member->conference->interval);

	/* As long as we have a valid read, feed that data into an input buffer where the conference thread will take it
	   and mux it with any audio from other channels. */

	while (conference_utils_member_test_flag(member, MFLAG_RUNNING) && switch_channel_ready(channel)) {

		if (switch_channel_ready(channel) && switch_channel_test_app_flag(channel, CF_APP_TAGGED)) {
			switch_yield(100000);
			continue;
		}

		/* Read a frame. */
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		switch_mutex_lock(member->read_mutex);

		/* end the loop, if appropriate */
		if (!SWITCH_READ_ACCEPTABLE(status) || !conference_utils_member_test_flag(member, MFLAG_RUNNING)) {
			switch_mutex_unlock(member->read_mutex);
			break;
		}

		if (switch_channel_test_flag(channel, CF_VIDEO) && !conference_utils_member_test_flag(member, MFLAG_ACK_VIDEO)) {
			conference_utils_member_set_flag_locked(member, MFLAG_ACK_VIDEO);
			switch_mutex_lock(member->flag_mutex);
			switch_img_free(&member->avatar_png_img);
			switch_mutex_unlock(member->flag_mutex);
			conference_video_check_avatar(member, SWITCH_FALSE);
			switch_core_session_video_reinit(member->session);
			conference_video_set_floor_holder(member->conference, member, SWITCH_FALSE);
			conference_video_check_flush(member, SWITCH_TRUE);
			switch_core_session_request_video_refresh(member->session);
		} else if (conference_utils_member_test_flag(member, MFLAG_ACK_VIDEO) && !switch_channel_test_flag(channel, CF_VIDEO)) {
			conference_video_check_avatar(member, SWITCH_FALSE);
		}

		/* if we have caller digits, feed them to the parser to find an action */
		if (switch_channel_has_dtmf(channel)) {
			char dtmf[128] = "";

			switch_channel_dequeue_dtmf_string(channel, dtmf, sizeof(dtmf));

			if (conference_utils_member_test_flag(member, MFLAG_DIST_DTMF)) {
				conference_member_send_all_dtmf(member, member->conference, dtmf);
			} else if (member->dmachine) {
				char *p;
				char str[2] = "";
				for (p = dtmf; p && *p; p++) {
					str[0] = *p;
					switch_ivr_dmachine_feed(member->dmachine, str, NULL);
				}
			}
		} else if (member->dmachine) {
			switch_ivr_dmachine_ping(member->dmachine, NULL);
		}

		if (switch_queue_size(member->dtmf_queue)) {
			switch_dtmf_t *dt;
			void *pop;

			if (switch_queue_trypop(member->dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				dt = (switch_dtmf_t *) pop;
				switch_core_session_send_dtmf(member->session, dt);
				free(dt);
			}
		}

		if (switch_channel_test_flag(member->channel, CF_CONFERENCE_RESET_MEDIA)) {
			member->reset_media = 10;
			switch_channel_audio_sync(member->channel);
			switch_channel_clear_flag(member->channel, CF_CONFERENCE_RESET_MEDIA);
		}

		if (member->reset_media) {
			if (--member->reset_media > 0) {
				goto do_continue;
			}

			if (conference_member_setup_media(member, member->conference)) {
				switch_mutex_unlock(member->read_mutex);
				break;
			}

			member->loop_loop = 1;

			goto do_continue;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			if (hangunder_hits) {
				hangunder_hits--;
			}
			if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
				if (++hangover_hits >= hangover) {
					hangover_hits = hangunder_hits = 0;
					if (member->nogate_count < hangover) {
						member->nogate_count = 0;
					} else {
						member->nogate_count -= hangover;
					}
					conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);
					conference_member_update_status_field(member);
					conference_member_set_score_iir(member, 0);
					member->floor_packets = 0;
					stop_talking_handler(member);
				}
			}

			goto do_continue;
		}

		if (!switch_channel_test_app_flag(channel, CF_AUDIO)) {
			goto do_continue;
		}
		
		/* if the member can speak, compute the audio energy level and */
		/* generate events when the level crosses the threshold        */
		if (((conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) && !conference_utils_member_test_flag(member, MFLAG_HOLD)) ||
			 conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT))) {
			uint32_t energy = 0, i = 0, samples = 0, j = 0;
			int16_t *data;
			int gate_check = 0;
			int score_iir = 0;
			
			data = read_frame->data;
			member->score = 0;

			if (member->volume_in_level) {
				switch_change_sln_volume(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, member->volume_in_level);
			}

			if ((samples = read_frame->datalen / sizeof(*data))) {
				for (i = 0; i < samples; i++) {
					energy += abs(data[j]);
					j++;
				}

				member->score = energy / samples;
			}

			if (member->vol_period) {
				member->vol_period--;
			}

			gate_check = conference_member_noise_gate_check(member);

			if (gate_check && member->agc) {
				switch_agc_feed(member->agc, (int16_t *)read_frame->data, (read_frame->datalen / 2) * member->conference->channels, 1);
			}

			score_iir = (int) (((1.0 - SCORE_DECAY) * (float) member->score) + (SCORE_DECAY * (float) member->score_iir));

			if (score_iir > SCORE_MAX_IIR) {
				score_iir = SCORE_MAX_IIR;
			}

			conference_member_set_score_iir(member, score_iir);
			
			if (member->auto_energy_level && !conference_utils_member_test_flag(member, MFLAG_TALKING)) {
				if (++member->auto_energy_track >= (1000 / member->conference->interval * member->conference->auto_energy_sec)) {
					if (member->energy_level > member->conference->energy_level) {
						int new_level = member->energy_level - 100;
						
						if (new_level < member->conference->energy_level) {
							new_level = member->conference->energy_level;
						}
						member->energy_level = new_level;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "ENERGY DOWN %d\n", member->energy_level);
					}
					member->auto_energy_track = 0;
				}
			}

			gate_check = conference_member_noise_gate_check(member);

			if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) && !conference_utils_member_test_flag(member, MFLAG_HOLD)) {
				if (member->max_energy_level) {
					if (member->score > member->max_energy_level && ++member->max_energy_hits > member->max_energy_hit_trigger) {
						member->mute_counter = member->burst_mute_count;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "MAX ENERGY HIT!\n");
					} else if (!member->mute_counter && member->score > (int)((double)member->max_energy_level * .75)) {
						int dec = 1;

						if (member->score_count > 3) {
							dec = 2;
						} else if (member->score_count > 6) {
							dec = 3;
						} else if (member->score_count > 9) {
							dec = 4;
						}

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "MAX ENERGY THRESHOLD! -%d\n", dec);
						switch_change_sln_volume(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, -1 * dec);
					} 
				}

				if (member->mute_counter > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "MAX ENERGY DECAY %d\n", member->mute_counter);
					member->mute_counter--;
					switch_generate_sln_silence(read_frame->data, (read_frame->datalen / 2), member->conference->channels, 1400 * (member->conference->rate / 8000));
					if (member->mute_counter == 0) {
						member->max_energy_hits = 0;
					}
				}
				
				if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
					member->talking_count++;

					if (gate_check) {
						int gate_count = 0, nogate_count = 0;
						double pct;
						member->score_accum += member->score;
						member->score_delta_accum += abs(member->score - member->last_score);
						member->score_count++;
						member->score_avg = member->score_accum / member->score_count;

						member->gate_count++;
						member->gate_open = 1;

						gate_count = member->gate_count;
						nogate_count = member->nogate_count;

						if (!gate_count) {
							pct = 0;
						} else {
							pct = ((float)nogate_count / (float)gate_count) * 100;
						}
					
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "TRACK %d %d %d/%d %f\n", 
										  member->score, 
										  member->score_avg,
										  gate_count, nogate_count, pct);
						

					} else {
						member->nogate_count++;
						member->gate_open = 0;
					}
				
				}
			
				if (conference_utils_member_test_flag(member, MFLAG_TALK_DATA_EVENTS)) {
					if (++member->talk_track >= (1000 / member->conference->interval * 10)) {
						uint32_t diff = 0; 
						double avg = 0;
						switch_event_t *event;

						if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							conference_member_add_event_data(member, event);
					

							if (member->first_talk_detect) {

								if (!member->talk_detects) {
									member->talk_detects = 1;
								}
					
								diff = (uint32_t) (switch_micro_time_now() - member->first_talk_detect) / 1000;
								avg = (double)diff / member->talk_detects;
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talk-detects", "%d", member->talk_detects);
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talk-detect-duration", "%d", diff);
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talk-detect-avg", "%f", avg);
							} else {
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "talk-detects", "%d", 0);
							}

					
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "talk-report");
							switch_event_fire(&event);
						}

						if (!conference_utils_member_test_flag(member, MFLAG_TALKING)) {
							member->first_talk_detect = 0;
							member->talk_detects = 0;
						} else {
							member->talk_detects = 1;
						}

						member->talk_track = 0;
					}
				}
			}


			if (gate_check) {
				uint32_t diff = member->score - member->energy_level;
				if (hangover_hits) {
					hangover_hits--;
				}

				if (member->id == member->conference->floor_holder) {
					member->floor_packets++;
				}

				if (diff >= diff_level || ++hangunder_hits >= hangunder) {

					hangover_hits = hangunder_hits = 0;
					member->last_talking = switch_epoch_time_now(NULL);

					if (!conference_utils_member_test_flag(member, MFLAG_TALKING)) {
						conference_utils_member_set_flag_locked(member, MFLAG_TALKING);
						conference_member_update_status_field(member);
						member->floor_packets = 0;

						
						if (!member->first_talk_detect) {
							member->first_talk_detect = switch_micro_time_now();
						}
						
						member->talk_detects++;
						member->score_delta_accum = 0;
						member->score_accum = 0;
						member->score_count = 0;
						member->talking_count = 0;
						
						if (test_eflag(member->conference, EFLAG_START_TALKING) && conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) &&
							!conference_utils_member_test_flag(member, MFLAG_HOLD) &&
							switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							conference_member_add_event_data(member, event);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-talking");
							switch_event_fire(&event);
						}

						if (conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT) && !conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {

							if (!zstr(member->conference->mute_detect_sound)) {
								conference_utils_member_set_flag(member, MFLAG_INDICATE_MUTE_DETECT);
							}

							if (test_eflag(member->conference, EFLAG_MUTE_DETECT) &&
								switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
								conference_member_add_event_data(member, event);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "mute-detect");
								switch_event_fire(&event);
							}
						}
					}
				}
			} else {
				if (hangunder_hits) {
					hangunder_hits--;
				}

				if (conference_utils_member_test_flag(member, MFLAG_TALKING) && conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) &&
					!conference_utils_member_test_flag(member, MFLAG_HOLD)) {
					if (++hangover_hits >= hangover) {
						hangover_hits = hangunder_hits = 0;

						if (member->nogate_count < hangover) {
							member->nogate_count = 0;
						} else {
							member->nogate_count -= hangover;
						}

						conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);
						conference_member_update_status_field(member);

						stop_talking_handler(member);						
					}
				}
			}


			member->last_score = member->score;

			if (member->id == member->conference->floor_holder) {
				if (member->id != member->conference->video_floor_holder &&
					(member->floor_packets > member->conference->video_floor_packets || member->energy_level == 0)) {
					conference_video_set_floor_holder(member->conference, member, SWITCH_FALSE);
				}
			}
		}

		/* skip frames that are not actual media or when we are muted or silent */
		if ((conference_utils_member_test_flag(member, MFLAG_TALKING) || member->energy_level == 0 || conference_utils_test_flag(member->conference, CFLAG_AUDIO_ALWAYS))
			&& conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) && !conference_utils_test_flag(member->conference, CFLAG_WAIT_MOD)
			&& !conference_utils_member_test_flag(member, MFLAG_HOLD)
			&& (member->conference->count > 1 || (member->conference->record_count && member->conference->count >= member->conference->min_recording_participants))) {
			switch_audio_resampler_t *read_resampler = member->read_resampler;
			void *data;
			uint32_t datalen;

			if (read_resampler) {
				int16_t *bptr = (int16_t *) read_frame->data;
				int len = (int) read_frame->datalen;

				switch_resample_process(read_resampler, bptr, len / 2 / member->read_impl.number_of_channels);
				memcpy(member->resample_out, read_resampler->to, read_resampler->to_len * 2 * member->read_impl.number_of_channels);
				len = read_resampler->to_len * 2 * member->read_impl.number_of_channels;
				datalen = len;
				data = member->resample_out;
			} else {
				data = read_frame->data;
				datalen = read_frame->datalen;
			}

			tmp_frame.data = data;
			tmp_frame.datalen = datalen;
			tmp_frame.rate = member->conference->rate;
			conference_member_check_channels(&tmp_frame, member, SWITCH_TRUE);


			if (datalen) {
				switch_size_t ok = 1;
				
				/* Write the audio into the input buffer */
				switch_mutex_lock(member->audio_in_mutex);
				if (switch_buffer_inuse(member->audio_buffer) > flush_len) {
					switch_buffer_toss(member->audio_buffer, tmp_frame.datalen);
				}
				ok = switch_buffer_write(member->audio_buffer, tmp_frame.data, tmp_frame.datalen);
				switch_mutex_unlock(member->audio_in_mutex);
				if (!ok) {
					switch_mutex_unlock(member->read_mutex);
					break;
				}
			}
		}

	do_continue:

		switch_mutex_unlock(member->read_mutex);

	}

	if (switch_queue_size(member->dtmf_queue)) {
		switch_dtmf_t *dt;
		void *pop;

		while (switch_queue_trypop(member->dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			dt = (switch_dtmf_t *) pop;
			free(dt);
		}
	}


	switch_resample_destroy(&member->read_resampler);
	switch_core_session_rwunlock(session);

 end:

	conference_utils_member_clear_flag_locked(member, MFLAG_ITHREAD);

	return NULL;
}


/* launch an input thread for the call leg */
void conference_loop_launch_input(conference_member_t *member, switch_memory_pool_t *pool)
{
	switch_threadattr_t *thd_attr = NULL;

	if (member == NULL || member->input_thread)
		return;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	conference_utils_member_set_flag_locked(member, MFLAG_ITHREAD);
	if (switch_thread_create(&member->input_thread, thd_attr, conference_loop_input, member, pool) != SWITCH_STATUS_SUCCESS) {
		conference_utils_member_clear_flag_locked(member, MFLAG_ITHREAD);
	}
}

/* marshall frames from the conference (or file or tts output) to the call leg */
/* NB. this starts the input thread after some initial setup for the call leg */
void conference_loop_output(conference_member_t *member)
{
	switch_channel_t *channel;
	switch_frame_t write_frame = { 0 };
	uint8_t *data = NULL;
	switch_timer_t timer = { 0 };
	uint32_t interval;
	uint32_t samples;
	//uint32_t csamples;
	uint32_t tsamples;
	uint32_t flush_len;
	uint32_t low_count, bytes;
	call_list_t *call_list, *cp;
	switch_codec_implementation_t read_impl = { 0 }, real_read_impl = { 0 };
	int sanity;
	switch_status_t st;

	switch_core_session_get_read_impl(member->session, &read_impl);
	switch_core_session_get_real_read_impl(member->session, &real_read_impl);


	channel = switch_core_session_get_channel(member->session);
	interval = read_impl.microseconds_per_packet / 1000;
	samples = switch_samples_per_packet(member->conference->rate, interval);
	//csamples = samples;
	tsamples = real_read_impl.samples_per_packet;
	low_count = 0;
	bytes = samples * 2 * member->conference->channels;
	call_list = NULL;
	cp = NULL;

	member->loop_loop = 0;

	switch_assert(member->conference != NULL);

	flush_len = switch_samples_per_packet(member->conference->rate, member->conference->interval) * 2 * member->conference->channels * (500 / member->conference->interval);

	if (switch_core_timer_init(&timer, member->conference->timer_name, interval, tsamples, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Setup timer %s success interval: %u  samples: %u from codec %s\n",
					  member->conference->timer_name, interval, tsamples, real_read_impl.iananame);


	write_frame.data = data = switch_core_session_alloc(member->session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;


	write_frame.codec = &member->write_codec;

	/* Start the input thread */
	conference_loop_launch_input(member, switch_core_session_get_pool(member->session));

	if ((call_list = switch_channel_get_private(channel, "_conference_autocall_list_"))) {
		const char *cid_name = switch_channel_get_variable(channel, "conference_auto_outcall_caller_id_name");
		const char *cid_num = switch_channel_get_variable(channel, "conference_auto_outcall_caller_id_number");
		const char *toval = switch_channel_get_variable(channel, "conference_auto_outcall_timeout");
		const char *flags = switch_channel_get_variable(channel, "conference_utils_auto_outcall_flags");
		const char *profile = switch_channel_get_variable(channel, "conference_auto_outcall_profile");
		const char *ann = switch_channel_get_variable(channel, "conference_auto_outcall_announce");
		const char *prefix = switch_channel_get_variable(channel, "conference_auto_outcall_prefix");
		const char *maxwait = switch_channel_get_variable(channel, "conference_auto_outcall_maxwait");
		const char *delimiter_val = switch_channel_get_variable(channel, "conference_auto_outcall_delimiter");
		const char *skip_member_beep = switch_channel_get_variable(channel, "conference_auto_outcall_skip_member_beep");
		int to = 60;
		int wait_sec = 2;
		int loops = 0;
		switch_event_t *var_event;

		switch_event_create(&var_event, SWITCH_EVENT_CHANNEL_DATA);
		switch_channel_process_export(channel, NULL, var_event, "conference_auto_outcall_export_vars");

		if (ann && !switch_channel_test_app_flag_key("conference_silent", channel, CONF_SILENT_REQ)) {
			member->conference->special_announce = switch_core_strdup(member->conference->pool, ann);
		}

		switch_channel_set_private(channel, "_conference_autocall_list_", NULL);

		conference_utils_set_flag(member->conference, CFLAG_OUTCALL);

		if (toval) {
			to = atoi(toval);
			if (to < 10 || to > 500) {
				to = 60;
			}
		}

		for (cp = call_list; cp; cp = cp->next) {
			int argc;
			char *argv[512] = { 0 };
			char *cpstr = strdup(cp->string);
			int x = 0;

			switch_assert(cpstr);
			if (!zstr(delimiter_val) && strlen(delimiter_val) == 1) {
				char delimiter = *delimiter_val;
				argc = switch_separate_string(cpstr, delimiter, argv, (sizeof(argv) / sizeof(argv[0])));
			} else {
				argc = switch_separate_string(cpstr, ',', argv, (sizeof(argv) / sizeof(argv[0])));
			}
			for (x = 0; x < argc; x++) {
				char *dial_str = switch_mprintf("%s%s", switch_str_nil(prefix), argv[x]);
				switch_event_t *event = NULL;
				switch_event_dup(&event, var_event);
				switch_assert(dial_str);
				conference_outcall_bg(member->conference, NULL, NULL, dial_str, to, switch_str_nil(flags), cid_name, cid_num, NULL,
									  profile, &member->conference->cancel_cause, &event);
				switch_safe_free(dial_str);
			}
			switch_safe_free(cpstr);
		}

		if (maxwait) {
			int tmp = atoi(maxwait);
			if (tmp > 0) {
				wait_sec = tmp;
			}
		}


		loops = wait_sec * 10;

		switch_channel_set_app_flag(channel, CF_APP_TAGGED);
		do {
			switch_ivr_sleep(member->session, 100, SWITCH_TRUE, NULL);
		} while(switch_channel_up(channel) && (member->conference->originating && --loops));
		switch_channel_clear_app_flag(channel, CF_APP_TAGGED);

		if (!switch_channel_ready(channel)) {
			member->conference->cancel_cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
			goto end;
		}

		if (!skip_member_beep || !switch_true(skip_member_beep))
			conference_member_play_file(member, "tone_stream://%(500,0,640)", 0, SWITCH_TRUE);
	}

	if (!conference_utils_test_flag(member->conference, CFLAG_ANSWERED)) {
		switch_channel_answer(channel);
	}


	sanity = 2000;
	while(!conference_utils_member_test_flag(member, MFLAG_ITHREAD) && sanity > 0) {
		switch_cond_next();
		sanity--;
	}

	/* Fair WARNING, If you expect the caller to hear anything or for digit handling to be processed,      */
	/* you better not block this thread loop for more than the duration of member->conference->timer_name!  */
	while (!member->loop_loop && conference_utils_member_test_flag(member, MFLAG_RUNNING) && conference_utils_member_test_flag(member, MFLAG_ITHREAD)
		   && switch_channel_ready(channel)) {
		switch_event_t *event;
		int use_timer = 0;
		switch_buffer_t *use_buffer = NULL;
		uint32_t mux_used = 0;


		//if (member->reset_media || switch_channel_test_flag(member->channel, CF_CONFERENCE_RESET_MEDIA)) {
		//	switch_cond_next();
		//	continue;
		//}

		switch_mutex_lock(member->write_mutex);


		if (switch_channel_test_flag(member->channel, CF_CONFERENCE_ADV)) {
			if (member->conference->la) {
				conference_event_adv_la(member->conference, member, SWITCH_TRUE);
			}
			switch_channel_clear_flag(member->channel, CF_CONFERENCE_ADV);
		}


		if (switch_core_session_dequeue_event(member->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			if (event->event_id == SWITCH_EVENT_MESSAGE) {
				char *from = switch_event_get_header(event, "from");
				char *to = switch_event_get_header(event, "to");
				char *body = switch_event_get_body(event);

				if (to && from && body) {
					if (strchr(to, '+') && strncmp(to, CONF_CHAT_PROTO, strlen(CONF_CHAT_PROTO))) {
						switch_event_del_header(event, "to");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM,
												"to", "%s+%s@%s", CONF_CHAT_PROTO, member->conference->name, member->conference->domain);
					} else {
						switch_event_del_header(event, "to");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "to", "%s", member->conference->name);
					}
					chat_send(event);
				}
			}
			switch_event_destroy(&event);
		}

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			/* test to see if outbound channel has answered */
			if (switch_channel_test_flag(channel, CF_ANSWERED) && !conference_utils_test_flag(member->conference, CFLAG_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
								  "Outbound conference channel answered, setting CFLAG_ANSWERED\n");
				conference_utils_set_flag(member->conference, CFLAG_ANSWERED);
			}
		} else {
			if (conference_utils_test_flag(member->conference, CFLAG_ANSWERED) && !switch_channel_test_flag(channel, CF_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "CLFAG_ANSWERED set, answering inbound channel\n");
				switch_channel_answer(channel);
			}
		}

		use_buffer = NULL;
		mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);

		use_timer = 1;

		if (mux_used) {
			if (mux_used < bytes) {
				if (++low_count >= 5) {
					/* partial frame sitting around this long is useless and builds delay */
					conference_utils_member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
				}
			} else if (mux_used > flush_len) {
				/* getting behind, clear the buffer */
				conference_utils_member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			}
		}

		if (switch_channel_test_app_flag(channel, CF_APP_TAGGED)) {
			conference_utils_member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
		} else if (mux_used >= bytes) {
			/* Flush the output buffer and write all the data (presumably muxed) back to the channel */
			switch_mutex_lock(member->audio_out_mutex);
			write_frame.data = data;
			use_buffer = member->mux_buffer;
			low_count = 0;

			if ((write_frame.datalen = (uint32_t) switch_buffer_read(use_buffer, write_frame.data, bytes))) {
				if (write_frame.datalen) {
					write_frame.samples = write_frame.datalen / 2 / member->conference->channels;

					if( !conference_utils_member_test_flag(member, MFLAG_CAN_HEAR)) {
						memset(write_frame.data, 255, write_frame.datalen);
					} else if (member->volume_out_level) { /* Check for output volume adjustments */
						switch_change_sln_volume(write_frame.data, write_frame.samples * member->conference->channels, member->volume_out_level);
					}

					//write_frame.timestamp = timer.samplecount;

					if (member->fnode) {
						conference_member_add_file_data(member, write_frame.data, write_frame.datalen);
					}

					conference_member_check_channels(&write_frame, member, SWITCH_FALSE);

					if (switch_core_session_write_frame(member->session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
						switch_mutex_unlock(member->audio_out_mutex);
						switch_mutex_unlock(member->write_mutex);
						break;
					}
				}
			}

			switch_mutex_unlock(member->audio_out_mutex);
		}

		if (conference_utils_member_test_flag(member, MFLAG_FLUSH_BUFFER)) {
			if (switch_buffer_inuse(member->mux_buffer)) {
				switch_mutex_lock(member->audio_out_mutex);
				switch_buffer_zero(member->mux_buffer);
				switch_mutex_unlock(member->audio_out_mutex);
			}
			conference_utils_member_clear_flag_locked(member, MFLAG_FLUSH_BUFFER);
		}

		switch_mutex_unlock(member->write_mutex);


		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_MUTE)) {
			if (!zstr(member->conference->muted_sound)) {
				conference_member_play_file(member, member->conference->muted_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];

				switch_snprintf(msg, sizeof(msg), "Muted");
				conference_member_say(member, msg, 0);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_MUTE);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_MUTE_DETECT)) {
			if (!zstr(member->conference->mute_detect_sound)) {
				conference_member_play_file(member, member->conference->mute_detect_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];

				switch_snprintf(msg, sizeof(msg), "Currently Muted");
				conference_member_say(member, msg, 0);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_MUTE_DETECT);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_UNMUTE)) {
			if (!zstr(member->conference->unmuted_sound)) {
				conference_member_play_file(member, member->conference->unmuted_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];

				switch_snprintf(msg, sizeof(msg), "Un-Muted");
				conference_member_say(member, msg, 0);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_UNMUTE);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_DEAF)) {
			if (!zstr(member->conference->deaf_sound)) {
				conference_member_play_file(member, member->conference->deaf_sound, 0, SWITCH_TRUE);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_DEAF);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_UNDEAF)) {
			if (!zstr(member->conference->undeaf_sound)) {
				conference_member_play_file(member, member->conference->undeaf_sound, 0, SWITCH_TRUE);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_UNDEAF);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_BLIND)) {
			if (!zstr(member->conference->deaf_sound)) {
				conference_member_play_file(member, member->conference->deaf_sound, 0, SWITCH_TRUE);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_BLIND);
		}

		if (conference_utils_member_test_flag(member, MFLAG_INDICATE_UNBLIND)) {
			if (!zstr(member->conference->undeaf_sound)) {
				conference_member_play_file(member, member->conference->undeaf_sound, 0, SWITCH_TRUE);
			}
			conference_utils_member_clear_flag(member, MFLAG_INDICATE_UNBLIND);
		}

		if (switch_core_session_private_event_count(member->session)) {
			switch_channel_set_app_flag(channel, CF_APP_TAGGED);
			switch_ivr_parse_all_events(member->session);
			switch_channel_clear_app_flag(channel, CF_APP_TAGGED);
			conference_utils_member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			switch_core_session_set_read_codec(member->session, &member->read_codec);
		} else {
			switch_ivr_parse_all_messages(member->session);
		}

		if (use_timer) {
			switch_core_timer_next(&timer);
		} else {
			switch_cond_next();
		}

	} /* Rinse ... Repeat */

 end:

	if (!member->loop_loop) {
		conference_utils_member_clear_flag_locked(member, MFLAG_RUNNING);

		/* Wait for the input thread to end */
		if (member->input_thread) {
			switch_thread_join(&st, member->input_thread);
			member->input_thread = NULL;
		}
	}

	switch_core_timer_destroy(&timer);

	if (member->loop_loop) {
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_INFO, "Channel leaving conference, cause: %s\n",
					  switch_channel_cause2str(switch_channel_get_cause(channel)));

	/* if it's an outbound channel, store the release cause in the conference struct, we might need it */
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		member->conference->bridge_hangup_cause = switch_channel_get_cause(channel);
	}
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
