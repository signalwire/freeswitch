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
	{"vid-floor-force", conference_loop_vid_floor_force}
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
	if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conference_api_sub_mute(member, NULL, NULL);
	}
}

void conference_loop_mute_off(conference_member_t *member, caller_control_action_t *action)
{
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

void conference_loop_deafmute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

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
			conference_video_check_avatar(member, SWITCH_FALSE);
			switch_core_session_video_reinit(member->session);
			conference_video_set_floor_holder(member->conference, member, SWITCH_FALSE);
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
			switch_channel_clear_flag(member->channel, CF_CONFERENCE_RESET_MEDIA);
			member->loop_loop = 1;
				
			if (conference_member_setup_media(member, member->conference)) {
				switch_mutex_unlock(member->read_mutex);
				break;
			}
			
			goto do_continue;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			if (member->conference->agc_level) {
				member->nt_tally++;
			}

			if (hangunder_hits) {
				hangunder_hits--;
			}
			if (conference_utils_member_test_flag(member, MFLAG_TALKING)) {
				if (++hangover_hits >= hangover) {
					hangover_hits = hangunder_hits = 0;
					conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);
					conference_member_update_status_field(member);
					conference_member_check_agc_levels(member);
					conference_member_clear_avg(member);
					member->score_iir = 0;
					member->floor_packets = 0;

					if (test_eflag(member->conference, EFLAG_STOP_TALKING) &&
						switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
						conference_member_add_event_data(member, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
						switch_event_fire(&event);
					}
				}
			}

			goto do_continue;
		}

		if (member->nt_tally > (int32_t)(member->read_impl.actual_samples_per_second / member->read_impl.samples_per_packet) * 3) {
			member->agc_volume_in_level = 0;
			conference_member_clear_avg(member);
		}

		/* Check for input volume adjustments */
		if (!member->conference->agc_level) {
			member->conference->agc_level = 0;
			conference_member_clear_avg(member);
		}


		/* if the member can speak, compute the audio energy level and */
		/* generate events when the level crosses the threshold        */
		if ((conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) || conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT))) {
			uint32_t energy = 0, i = 0, samples = 0, j = 0;
			int16_t *data;
			int agc_period = (member->read_impl.actual_samples_per_second / member->read_impl.samples_per_packet) / 4;


			data = read_frame->data;
			member->score = 0;

			if (member->volume_in_level) {
				switch_change_sln_volume(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, member->volume_in_level);
			}

			if (member->agc_volume_in_level) {
				switch_change_sln_volume_granular(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, member->agc_volume_in_level);
			}

			if ((samples = read_frame->datalen / sizeof(*data) / member->read_impl.number_of_channels)) {
				for (i = 0; i < samples; i++) {
					energy += abs(data[j]);
					j += member->read_impl.number_of_channels;
				}

				member->score = energy / samples;
			}

			if (member->vol_period) {
				member->vol_period--;
			}

			if (member->conference->agc_level && member->score &&
				conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) &&
				conference_member_noise_gate_check(member)
				) {
				int last_shift = abs((int)(member->last_score - member->score));

				if (member->score && member->last_score && last_shift > 900) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
									  "AGC %s:%d drop anomalous shift of %d\n",
									  member->conference->name,
									  member->id, last_shift);

				} else {
					member->avg_tally += member->score;
					member->avg_itt++;
					if (!member->avg_itt) member->avg_itt++;
					member->avg_score = member->avg_tally / member->avg_itt;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
								  "AGC %s:%d diff:%d level:%d cur:%d avg:%d vol:%d\n",
								  member->conference->name,
								  member->id, member->conference->agc_level - member->avg_score, member->conference->agc_level,
								  member->score, member->avg_score, member->agc_volume_in_level);

				if (++member->agc_concur >= agc_period) {
					if (!member->vol_period) {
						conference_member_check_agc_levels(member);
					}
					member->agc_concur = 0;
				}
			} else {
				member->nt_tally++;
			}

			member->score_iir = (int) (((1.0 - SCORE_DECAY) * (float) member->score) + (SCORE_DECAY * (float) member->score_iir));

			if (member->score_iir > SCORE_MAX_IIR) {
				member->score_iir = SCORE_MAX_IIR;
			}

			if (conference_member_noise_gate_check(member)) {
				uint32_t diff = member->score - member->energy_level;
				if (hangover_hits) {
					hangover_hits--;
				}

				if (member->conference->agc_level) {
					member->nt_tally = 0;
				}

				if (member == member->conference->floor_holder) {
					member->floor_packets++;
				}

				if (diff >= diff_level || ++hangunder_hits >= hangunder) {

					hangover_hits = hangunder_hits = 0;
					member->last_talking = switch_epoch_time_now(NULL);

					if (!conference_utils_member_test_flag(member, MFLAG_TALKING)) {
						conference_utils_member_set_flag_locked(member, MFLAG_TALKING);
						conference_member_update_status_field(member);
						member->floor_packets = 0;

						if (test_eflag(member->conference, EFLAG_START_TALKING) && conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) &&
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

				if (member->conference->agc_level) {
					member->nt_tally++;
				}

				if (conference_utils_member_test_flag(member, MFLAG_TALKING) && conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
					switch_event_t *event;
					if (++hangover_hits >= hangover) {
						hangover_hits = hangunder_hits = 0;
						conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);
						conference_member_update_status_field(member);
						conference_member_check_agc_levels(member);
						conference_member_clear_avg(member);

						if (test_eflag(member->conference, EFLAG_STOP_TALKING) &&
							switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							conference_member_add_event_data(member, event);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
							switch_event_fire(&event);
						}
					}
				}
			}


			member->last_score = member->score;

			if (member == member->conference->floor_holder) {
				if (member->id != member->conference->video_floor_holder &&
					(member->floor_packets > member->conference->video_floor_packets || member->energy_level == 0)) {
					conference_video_set_floor_holder(member->conference, member, SWITCH_FALSE);
				}
			}
		}
		
		/* skip frames that are not actual media or when we are muted or silent */
		if ((conference_utils_member_test_flag(member, MFLAG_TALKING) || member->energy_level == 0 || conference_utils_test_flag(member->conference, CFLAG_AUDIO_ALWAYS))
			&& conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK) &&	!conference_utils_test_flag(member->conference, CFLAG_WAIT_MOD)
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
	switch_codec_implementation_t read_impl = { 0 };
	int sanity;
	switch_status_t st;

	switch_core_session_get_read_impl(member->session, &read_impl);


	channel = switch_core_session_get_channel(member->session);
	interval = read_impl.microseconds_per_packet / 1000;
	samples = switch_samples_per_packet(member->conference->rate, interval);
	//csamples = samples;
	tsamples = member->orig_read_impl.samples_per_packet;
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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Setup timer %s success interval: %u  samples: %u\n",
					  member->conference->timer_name, interval, tsamples);


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
		int to = 60;
		int wait_sec = 2;
		int loops = 0;

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
				switch_assert(dial_str);
				conference_outcall_bg(member->conference, NULL, NULL, dial_str, to, switch_str_nil(flags), cid_name, cid_num, NULL,
									  profile, &member->conference->cancel_cause, NULL);
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


		if (switch_channel_test_flag(member->channel, CF_CONFERENCE_RESET_MEDIA)) {
			switch_cond_next();
			continue;
		}

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

					write_frame.timestamp = timer.samplecount;

					if (member->fnode) {
						conference_member_add_file_data(member, write_frame.data, write_frame.datalen);
					}

					conference_member_check_channels(&write_frame, member, SWITCH_FALSE);

					if (switch_core_session_write_frame(member->session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
						switch_mutex_unlock(member->audio_out_mutex);
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
