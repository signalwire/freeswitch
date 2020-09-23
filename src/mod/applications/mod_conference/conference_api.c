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


api_command_t conference_api_sub_commands[] = {
	{"canvas-auto-clear", (void_fn_t) & conference_api_sub_canvas_auto_clear, CONF_API_SUB_ARGS_SPLIT, "canvas-auto-clear", "<canvas_id> <true|false>"},
	{"count", (void_fn_t) & conference_api_sub_count, CONF_API_SUB_ARGS_SPLIT, "count", ""},
	{"list", (void_fn_t) & conference_api_sub_list, CONF_API_SUB_ARGS_SPLIT, "list", "[delim <string>]|[count]"},
	{"xml_list", (void_fn_t) & conference_api_sub_xml_list, CONF_API_SUB_ARGS_SPLIT, "xml_list", ""},
	{"json_list", (void_fn_t) & conference_api_sub_json_list, CONF_API_SUB_ARGS_SPLIT, "json_list", "[compact]"},
	{"energy", (void_fn_t) & conference_api_sub_energy, CONF_API_SUB_MEMBER_TARGET, "energy", "<member_id|all|last|non_moderator> [<newval>]"},
	{"auto-energy", (void_fn_t) & conference_api_sub_auto_energy, CONF_API_SUB_MEMBER_TARGET, "auto-energy", "<member_id|all|last|non_moderator> [<newval>]"},
	{"max-energy", (void_fn_t) & conference_api_sub_max_energy, CONF_API_SUB_MEMBER_TARGET, "max-energy", "<member_id|all|last|non_moderator> [<newval>]"},
	{"agc", (void_fn_t) & conference_api_sub_agc, CONF_API_SUB_MEMBER_TARGET, "agc", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-canvas", (void_fn_t) & conference_api_sub_canvas, CONF_API_SUB_MEMBER_TARGET, "vid-canvas", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-watching-canvas", (void_fn_t) & conference_api_sub_watching_canvas, CONF_API_SUB_MEMBER_TARGET, "vid-watching-canvas", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-layer", (void_fn_t) & conference_api_sub_layer, CONF_API_SUB_MEMBER_TARGET, "vid-layer", "<member_id|all|last|non_moderator> [<newval>]"},
	{"volume_in", (void_fn_t) & conference_api_sub_volume_in, CONF_API_SUB_MEMBER_TARGET, "volume_in", "<member_id|all|last|non_moderator> [<newval>]"},
	{"volume_out", (void_fn_t) & conference_api_sub_volume_out, CONF_API_SUB_MEMBER_TARGET, "volume_out", "<member_id|all|last|non_moderator> [<newval>]"},
	{"position", (void_fn_t) & conference_api_sub_position, CONF_API_SUB_MEMBER_TARGET, "position", "<member_id> <x>:<y>:<z>"},
	{"auto-3d-position", (void_fn_t) & conference_api_sub_auto_position, CONF_API_SUB_ARGS_SPLIT, "auto-3d-position", "[on|off]"},
	{"play", (void_fn_t) & conference_api_sub_play, CONF_API_SUB_ARGS_SPLIT, "play", "<file_path> [async|<member_id> [nomux]]"},
	{"moh", (void_fn_t) & conference_api_sub_moh, CONF_API_SUB_ARGS_SPLIT, "moh", "<file_path>|toggle|[on|off]"},
	{"pause_play", (void_fn_t) & conference_api_sub_pause_play, CONF_API_SUB_ARGS_SPLIT, "pause", "[<member_id>]"},
	{"play_status", (void_fn_t) & conference_api_sub_play_status, CONF_API_SUB_ARGS_SPLIT, "play_status", "[<member_id>]"},
	{"file_seek", (void_fn_t) & conference_api_sub_file_seek, CONF_API_SUB_ARGS_SPLIT, "file_seek", "[+-]<val> [<member_id>]"},
	{"say", (void_fn_t) & conference_api_sub_say, CONF_API_SUB_ARGS_AS_ONE, "say", "<text>"},
	{"saymember", (void_fn_t) & conference_api_sub_saymember, CONF_API_SUB_ARGS_AS_ONE, "saymember", "<member_id> <text>"},
	{"cam", (void_fn_t) & conference_api_sub_cam, CONF_API_SUB_ARGS_SPLIT, "cam", ""},
	{"stop", (void_fn_t) & conference_api_sub_stop, CONF_API_SUB_ARGS_SPLIT, "stop", "<[current|all|async|last]> [<member_id>]"},
	{"dtmf", (void_fn_t) & conference_api_sub_dtmf, CONF_API_SUB_MEMBER_TARGET, "dtmf", "<[member_id|all|last|non_moderator]> <digits>"},
	{"kick", (void_fn_t) & conference_api_sub_kick, CONF_API_SUB_MEMBER_TARGET, "kick", "<[member_id|all|last|non_moderator]> [<optional sound file>]"},
	{"vid-flip", (void_fn_t) & conference_api_sub_vid_flip, CONF_API_SUB_MEMBER_TARGET, "vid-flip", "<[member_id|all|last|non_moderator]>"},
	{"vid-border", (void_fn_t) & conference_api_sub_vid_border, CONF_API_SUB_MEMBER_TARGET, "vid-border", "<[member_id|all|last|non_moderator]>"},
	{"hup", (void_fn_t) & conference_api_sub_hup, CONF_API_SUB_MEMBER_TARGET, "hup", "<[member_id|all|last|non_moderator]>"},
	{"hold", (void_fn_t) & conference_api_sub_hold, CONF_API_SUB_MEMBER_TARGET, "hold", "<[member_id|all]|last|non_moderator> [file]"},
	{"unhold", (void_fn_t) & conference_api_sub_unhold, CONF_API_SUB_MEMBER_TARGET, "unhold", "<[member_id|all]|last|non_moderator>"},
	{"mute", (void_fn_t) & conference_api_sub_mute, CONF_API_SUB_MEMBER_TARGET, "mute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"tmute", (void_fn_t) & conference_api_sub_tmute, CONF_API_SUB_MEMBER_TARGET, "tmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"unmute", (void_fn_t) & conference_api_sub_unmute, CONF_API_SUB_MEMBER_TARGET, "unmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"vmute", (void_fn_t) & conference_api_sub_vmute, CONF_API_SUB_MEMBER_TARGET, "vmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"tvmute", (void_fn_t) & conference_api_sub_tvmute, CONF_API_SUB_MEMBER_TARGET, "tvmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"vmute-snap", (void_fn_t) & conference_api_sub_conference_video_vmute_snap, CONF_API_SUB_MEMBER_TARGET, "vmute-snap", "<[member_id|all]|last|non_moderator>"},
	{"unvmute", (void_fn_t) & conference_api_sub_unvmute, CONF_API_SUB_MEMBER_TARGET, "unvmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"vblind", (void_fn_t) & conference_api_sub_vblind, CONF_API_SUB_MEMBER_TARGET, "vblind", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"tvblind", (void_fn_t) & conference_api_sub_tvblind, CONF_API_SUB_MEMBER_TARGET, "tvblind", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"unvblind", (void_fn_t) & conference_api_sub_unvblind, CONF_API_SUB_MEMBER_TARGET, "unvblind", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"deaf", (void_fn_t) & conference_api_sub_deaf, CONF_API_SUB_MEMBER_TARGET, "deaf", "<[member_id|all]|last|non_moderator>"},
	{"undeaf", (void_fn_t) & conference_api_sub_undeaf, CONF_API_SUB_MEMBER_TARGET, "undeaf", "<[member_id|all]|last|non_moderator>"},
	{"vid-filter", (void_fn_t) & conference_api_sub_video_filter, CONF_API_SUB_MEMBER_TARGET, "vid-filter", "<[member_id|all]|last|non_moderator> <string>"},
	{"relate", (void_fn_t) & conference_api_sub_relate, CONF_API_SUB_ARGS_SPLIT, "relate", "<member_id>[,<member_id>] <other_member_id>[,<other_member_id>] [nospeak|nohear|clear]"},
	{"lock", (void_fn_t) & conference_api_sub_lock, CONF_API_SUB_ARGS_SPLIT, "lock", ""},
	{"unlock", (void_fn_t) & conference_api_sub_unlock, CONF_API_SUB_ARGS_SPLIT, "unlock", ""},
	{"dial", (void_fn_t) & conference_api_sub_dial, CONF_API_SUB_ARGS_SPLIT, "dial", "<endpoint_module_name>/<destination> <callerid number> <callerid name>"},
	{"bgdial", (void_fn_t) & conference_api_sub_bgdial, CONF_API_SUB_ARGS_SPLIT, "bgdial", "<endpoint_module_name>/<destination> <callerid number> <callerid name>"},
	{"transfer", (void_fn_t) & conference_api_sub_transfer, CONF_API_SUB_ARGS_SPLIT, "transfer", "<conference_name> <member id> [...<member id>]"},
	{"record", (void_fn_t) & conference_api_sub_record, CONF_API_SUB_ARGS_SPLIT, "record", "<filename>"},
	{"chkrecord", (void_fn_t) & conference_api_sub_check_record, CONF_API_SUB_ARGS_SPLIT, "chkrecord", "<confname>"},
	{"norecord", (void_fn_t) & conference_api_sub_norecord, CONF_API_SUB_ARGS_SPLIT, "norecord", "<[filename|all]>"},
	{"pause", (void_fn_t) & conference_api_sub_pauserec, CONF_API_SUB_ARGS_SPLIT, "pause", "<filename>"},
	{"resume", (void_fn_t) & conference_api_sub_pauserec, CONF_API_SUB_ARGS_SPLIT, "resume", "<filename>"},
	{"recording", (void_fn_t) & conference_api_sub_recording, CONF_API_SUB_ARGS_SPLIT, "recording", "[start|stop|check|pause|resume] [<filename>|all]"},
	{"exit_sound", (void_fn_t) & conference_api_sub_exit_sound, CONF_API_SUB_ARGS_SPLIT, "exit_sound", "on|off|none|file <filename>"},
	{"enter_sound", (void_fn_t) & conference_api_sub_enter_sound, CONF_API_SUB_ARGS_SPLIT, "enter_sound", "on|off|none|file <filename>"},
	{"pin", (void_fn_t) & conference_api_sub_pin, CONF_API_SUB_ARGS_SPLIT, "pin", "<pin#>"},
	{"nopin", (void_fn_t) & conference_api_sub_pin, CONF_API_SUB_ARGS_SPLIT, "nopin", ""},
	{"get", (void_fn_t) & conference_api_sub_get, CONF_API_SUB_ARGS_SPLIT, "get", "<parameter-name>"},
	{"set", (void_fn_t) & conference_api_sub_set, CONF_API_SUB_ARGS_SPLIT, "set", "<max_members|sound_prefix|caller_id_name|caller_id_number|endconference_grace_time> <value>"},
	{"file-vol", (void_fn_t) & conference_api_sub_file_vol, CONF_API_SUB_ARGS_SPLIT, "file-vol", "<vol#>"},
	{"floor", (void_fn_t) & conference_api_sub_floor, CONF_API_SUB_MEMBER_TARGET, "floor", "<member_id|last>"},
	{"vid-floor", (void_fn_t) & conference_api_sub_vid_floor, CONF_API_SUB_MEMBER_TARGET, "vid-floor", "<member_id|last> [force]"},
	{"vid-banner", (void_fn_t) & conference_api_sub_vid_banner, CONF_API_SUB_MEMBER_TARGET, "vid-banner", "<member_id|last> <text>"},
	{"vid-mute-img", (void_fn_t) & conference_api_sub_vid_mute_img, CONF_API_SUB_MEMBER_TARGET, "vid-mute-img", "<member_id|last> [<path>|clear]"},
	{"vid-logo-img", (void_fn_t) & conference_api_sub_vid_logo_img, CONF_API_SUB_MEMBER_TARGET, "vid-logo-img", "<member_id|last> [<path>|clear]"},
	{"vid-codec-group", (void_fn_t) & conference_api_sub_vid_codec_group, CONF_API_SUB_MEMBER_TARGET, "vid-codec-group", "<member_id|last> [<group>|clear]"},
	{"vid-res-id", (void_fn_t) & conference_api_sub_vid_res_id, CONF_API_SUB_ARGS_SPLIT, "vid-res-id", "<member_id>|all <val>|clear [force]"},
	{"vid-role-id", (void_fn_t) & conference_api_sub_vid_role_id, CONF_API_SUB_MEMBER_TARGET, "vid-role-id", "<member_id|last> <val>|clear"},
	{"get-uuid", (void_fn_t) & conference_api_sub_get_uuid, CONF_API_SUB_MEMBER_TARGET, "get-uuid", "<member_id|last>"},
	{"clear-vid-floor", (void_fn_t) & conference_api_sub_clear_vid_floor, CONF_API_SUB_ARGS_AS_ONE, "clear-vid-floor", ""},
	{"vid-layout", (void_fn_t) & conference_api_sub_vid_layout, CONF_API_SUB_ARGS_SPLIT, "vid-layout", "<layout name>|group <group name> [<canvas id>]"},
	{"vid-write-png", (void_fn_t) & conference_api_sub_write_png, CONF_API_SUB_ARGS_SPLIT, "vid-write-png", "<path>"},
	{"vid-fps", (void_fn_t) & conference_api_sub_vid_fps, CONF_API_SUB_ARGS_SPLIT, "vid-fps", "<fps>"},
	{"vid-res", (void_fn_t) & conference_api_sub_vid_res, CONF_API_SUB_ARGS_SPLIT, "vid-res", "<WxH>"},
	{"vid-fgimg", (void_fn_t) & conference_api_sub_canvas_fgimg, CONF_API_SUB_ARGS_SPLIT, "vid-fgimg", "<file> | clear [<canvas-id>]"},
	{"vid-bgimg", (void_fn_t) & conference_api_sub_canvas_bgimg, CONF_API_SUB_ARGS_SPLIT, "vid-bgimg", "<file> | clear [<canvas-id>]"},
	{"vid-bandwidth", (void_fn_t) & conference_api_sub_vid_bandwidth, CONF_API_SUB_ARGS_SPLIT, "vid-bandwidth", "<BW>"},
	{"vid-personal", (void_fn_t) & conference_api_sub_vid_personal, CONF_API_SUB_ARGS_SPLIT, "vid-personal", "[on|off]"}
};

switch_status_t conference_api_sub_pause_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc == 2) {
		switch_mutex_lock(conference->mutex);
		conference_fnode_toggle_pause(conference->fnode, stream);
		switch_mutex_unlock(conference->mutex);

		return SWITCH_STATUS_SUCCESS;
	}

	if (argc == 3) {
		uint32_t id = atoi(argv[2]);
		conference_member_t *member;

		if ((member = conference_member_get(conference, id))) {
			switch_mutex_lock(member->fnode_mutex);
			conference_fnode_toggle_pause(member->fnode, stream);
			switch_mutex_unlock(member->fnode_mutex);
			switch_thread_rwlock_unlock(member->rwlock);
			return SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "-ERR Member: %u not found.\n", id);
		}
	}

	return SWITCH_STATUS_GENERR;
}

switch_status_t conference_api_sub_play_status(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc == 2) {
		switch_mutex_lock(conference->mutex);
		conference_fnode_check_status(conference->fnode, stream);
		switch_mutex_unlock(conference->mutex);

		return SWITCH_STATUS_SUCCESS;
	}

	if (argc == 3) {
		uint32_t id = atoi(argv[2]);
		conference_member_t *member;

		if ((member = conference_member_get(conference, id))) {
			switch_mutex_lock(member->fnode_mutex);
			conference_fnode_check_status(member->fnode, stream);
			switch_mutex_unlock(member->fnode_mutex);
			switch_thread_rwlock_unlock(member->rwlock);
			return SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "-ERR Member: %u not found.\n", id);
		}
	}

	return SWITCH_STATUS_GENERR;
}

/* _In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream */
switch_status_t conference_api_main_real(const char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char *lbuf = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *http = NULL, *type = NULL;
	int argc;
	char *argv[25] = { 0 };

	if (!cmd) {
		cmd = "help";
	}

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
		type = switch_event_get_header(stream->param_event, "content-type");
	}

	if (http) {
		/* Output must be to a web browser */
		if (type && !strcasecmp(type, "text/html")) {
			stream->write_function(stream, "<pre>\n");
		}
	}

	if (!(lbuf = strdup(cmd))) {
		return status;
	}

	argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	/* try to find a command to execute */
	if (argc && argv[0]) {
		conference_obj_t *conference = NULL;

		if ((conference = conference_find(argv[0], NULL))) {
			if (argc >= 2) {
				conference_api_dispatch(conference, stream, argc, argv, cmd, 1);
			} else {
				stream->write_function(stream, "Conference command, not specified.\nTry 'help'\n");
			}
			switch_thread_rwlock_unlock(conference->rwlock);

		} else if (argv[0]) {
			/* special case the list command, because it doesn't require a conference argument */
			if (strcasecmp(argv[0], "list") == 0) {
				conference_api_sub_list(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "count") == 0) {
				conference_api_sub_count(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "xml_list") == 0) {
				conference_api_sub_xml_list(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "json_list") == 0) {
				conference_api_sub_json_list(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "help") == 0 || strcasecmp(argv[0], "commands") == 0) {
				stream->write_function(stream, "%s\n", api_syntax);
			} else if (argv[1] && strcasecmp(argv[1], "dial") == 0) {
				if (conference_api_sub_dial(NULL, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
					/* command returned error, so show syntax usage */
					stream->write_function(stream, "%s %s", conference_api_sub_commands[CONF_API_COMMAND_DIAL].pcommand,
										   conference_api_sub_commands[CONF_API_COMMAND_DIAL].psyntax);
				}
			} else if (argv[1] && strcasecmp(argv[1], "bgdial") == 0) {
				if (conference_api_sub_bgdial(NULL, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
					/* command returned error, so show syntax usage */
					stream->write_function(stream, "%s %s", conference_api_sub_commands[CONF_API_COMMAND_BGDIAL].pcommand,
										   conference_api_sub_commands[CONF_API_COMMAND_BGDIAL].psyntax);
				}
			} else {
				stream->write_function(stream, "-ERR Conference %s not found\n", argv[0]);
			}
		}

	} else {
		int i;

		for (i = 0; i < CONFFUNCAPISIZE; i++) {
			stream->write_function(stream, "<conf name> %s %s\n", conference_api_sub_commands[i].pcommand, conference_api_sub_commands[i].psyntax);
		}
	}


	switch_safe_free(lbuf);

	return status;
}

switch_status_t conference_api_sub_syntax(char **syntax)
{
	/* build api interface help ".syntax" field string */
	uint32_t i;
	size_t nl = 0, ol = 0;
	char cmd_str[256];
	char *tmp = NULL, *p = strdup("");

	for (i = 0; i < CONFFUNCAPISIZE; i++) {
		nl = strlen(conference_api_sub_commands[i].pcommand) + strlen(conference_api_sub_commands[i].psyntax) + 5;

		switch_snprintf(cmd_str, sizeof(cmd_str), "add conference ::conference::conference_list_conferences %s", conference_api_sub_commands[i].pcommand);
		switch_console_set_complete(cmd_str);

		if (p != NULL) {
			ol = strlen(p);
		}
		tmp = realloc(p, ol + nl);
		if (tmp != NULL) {
			p = tmp;
			strcat(p, "\t\t");
			strcat(p, conference_api_sub_commands[i].pcommand);
			if (!zstr(conference_api_sub_commands[i].psyntax)) {
				strcat(p, " ");
				strcat(p, conference_api_sub_commands[i].psyntax);
			}
			if (i < CONFFUNCAPISIZE - 1) {
				strcat(p, "\n");
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't realloc\n");
			switch_safe_free(p);
			return SWITCH_STATUS_TERM;
		}
	}

	*syntax = p;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_mute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_clear_flag_locked(member, MFLAG_CAN_SPEAK);
	conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);

	if (member->session && !conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT) && !conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		switch_core_media_hard_mute(member->session, SWITCH_TRUE);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_MUTE);
	}
	conference_member_set_score_iir(member, 0);

	if (stream != NULL) {
		stream->write_function(stream, "+OK mute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "mute-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_unhold(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	mcu_layer_t *layer = NULL;
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_clear_flag_locked(member, MFLAG_HOLD);

	if (member->session && !conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT)) {
		switch_core_media_hard_mute(member->session, SWITCH_FALSE);
	}

	conference_member_stop_file(member, FILE_STOP_ALL);

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
		if ((layer = conference_video_get_layer_locked(member))) {
			layer->clear = 1;
			conference_video_release_layer(&layer);
		}

		conference_video_reset_video_bitrate_counters(member);

		if (member->channel) {
			switch_channel_clear_flag(member->channel, CF_VIDEO_PAUSE_READ);
			switch_channel_video_sync(member->channel);
		}
	}
	
	if (stream != NULL) {
		stream->write_function(stream, "+OK unhold %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_HOLD_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unhold-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_hold(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
		conference_video_reset_video_bitrate_counters(member);

		if (member->channel) {
			switch_channel_set_flag(member->channel, CF_VIDEO_PAUSE_READ);
			switch_core_session_request_video_refresh(member->session);
			switch_channel_video_sync(member->channel);
		}
	}

	if (member->session) {
		switch_core_media_hard_mute(member->session, SWITCH_TRUE);
	}

	conference_utils_member_set_flag(member, MFLAG_HOLD);
	
	conference_member_set_score_iir(member, 0);

	if (!zstr(data)) {
		conference_member_play_file(member, data, 0, SWITCH_FALSE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK hold %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_HOLD_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "hold-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_tmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_SPEAK)) {
		return conference_api_sub_mute(member, stream, data);
	}

	return conference_api_sub_unmute(member, stream, data);
}


switch_status_t conference_api_sub_unmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_set_flag_locked(member, MFLAG_CAN_SPEAK);

	if (member->session && !conference_utils_member_test_flag(member, MFLAG_MUTE_DETECT)) {
		switch_core_media_hard_mute(member->session, SWITCH_FALSE);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_UNMUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK unmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unmute-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_conference_video_vmute_snap(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_bool_t clear = SWITCH_FALSE;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!member->conference->canvases[0]) {
		if (stream) stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		if (stream) stream->write_function(stream, "-ERR member %u is on hold\n", member->id);
		return SWITCH_STATUS_SUCCESS;
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK vmute image snapped %u\n", member->id);
	}

	if (data && !strcasecmp((char *)data, "clear")) {
		clear = SWITCH_TRUE;
	}

	conference_video_vmute_snap(member, clear);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_canvas_auto_clear(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int canvas_id_start = 0;
	int canvas_id_end   = 0;
	int i = 0;

	if (argc < 3) {
		stream->write_function(stream, "+OK");

		for (i = 0; i < conference->canvas_count; i++) {
			stream->write_function(stream, " canvas %d auto_clear=%s", i + 1, conference->canvases[i]->disable_auto_clear ? "false" : "true");
		}

		stream->write_function(stream, "\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", argv[1]);

	canvas_id_start = atoi(argv[2]);

	if (canvas_id_start == 0) {
		canvas_id_end = conference->canvas_count - 1;
	} else {
		canvas_id_start--;
		canvas_id_end = canvas_id_start;
	}

	stream->write_function(stream, "+OK");
	switch_mutex_lock(conference->canvas_mutex);

	for (i = canvas_id_start; i<= canvas_id_end; i++) {
		conference->canvases[i]->disable_auto_clear = !switch_true(argv[3]);
		stream->write_function(stream, " canvas %d auto_clear=%s", i + 1, argv[3]);
	}

	switch_mutex_unlock(conference->canvas_mutex);
	stream->write_function(stream, "\n");

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		if (stream) stream->write_function(stream, "-ERR member %u is on hold\n", member->id);
		return SWITCH_STATUS_SUCCESS;
	}

	conference_utils_member_clear_flag_locked(member, MFLAG_CAN_BE_SEEN);
	conference_video_reset_video_bitrate_counters(member);

	if (member->channel) {
		switch_channel_set_flag(member->channel, CF_VIDEO_PAUSE_READ);
		switch_core_session_request_video_refresh(member->session);
		switch_channel_video_sync(member->channel);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_MUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK vmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "vmute-member");
		switch_event_fire(&event);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_tvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		if (stream) stream->write_function(stream, "-ERR member %u is on hold\n", member->id);
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		return conference_api_sub_vmute(member, stream, data);
	}

	return conference_api_sub_unvmute(member, stream, data);
}


switch_status_t conference_api_sub_unvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		if (stream) stream->write_function(stream, "-ERR member %u is on hold\n", member->id);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((layer = conference_video_get_layer_locked(member))) {
		layer->clear = 1;
		conference_video_release_layer(&layer);
	}

	conference_utils_member_set_flag_locked(member, MFLAG_CAN_BE_SEEN);
	conference_video_reset_video_bitrate_counters(member);

	if (member->channel) {
		switch_channel_clear_flag(member->channel, CF_VIDEO_PAUSE_READ);
		switch_channel_video_sync(member->channel);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_UNMUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK unvmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unvmute-member");
		switch_event_fire(&event);
	}


	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vblind(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_clear_flag_locked(member, MFLAG_CAN_SEE);

	if (!conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		switch_core_session_write_blank_video(member->session, 50);
		conference_video_reset_video_bitrate_counters(member);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_BLIND);
	}

	if (stream) stream->write_function(stream, "+OK vblind %u\n", member->id);

	if (test_eflag(member->conference, EFLAG_BLIND_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "vblind-member");
		switch_event_fire(&event);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_tvblind(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (conference_utils_member_test_flag(member, MFLAG_CAN_SEE)) {
		return conference_api_sub_vblind(member, stream, data);
	}

	return conference_api_sub_unvblind(member, stream, data);
}


switch_status_t conference_api_sub_unvblind(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_set_flag_locked(member, MFLAG_CAN_SEE);

	if (!conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		conference_video_reset_video_bitrate_counters(member);
		switch_channel_set_flag(member->channel, CF_VIDEO_REFRESH_REQ);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_UNBLIND);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK unvblind %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_BLIND_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unvblind-member");
		switch_event_fire(&event);
	}


	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_deaf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_clear_flag_locked(member, MFLAG_CAN_HEAR);

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_DEAF);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK deaf %u\n", member->id);
	}
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "deaf-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_video_filter(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *filter_str = (char *) data;

	switch_core_video_parse_filter_string(&member->video_filters, filter_str);

	stream->write_function(stream, "+OK\n");

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_undeaf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	conference_utils_member_set_flag_locked(member, MFLAG_CAN_HEAR);

	if (!(data) || !strstr((char *) data, "quiet")) {
		conference_utils_member_set_flag(member, MFLAG_INDICATE_UNDEAF);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK undeaf %u\n", member->id);
	}
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "undeaf-member");
		switch_event_fire(&event);
	}

	if (conference_utils_test_flag(member->conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(member->conference, NULL);
	}

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_hup(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		if (stream != NULL) {
			stream->write_function(stream, "-ERR Invalid member!\n");
		}
		return SWITCH_STATUS_GENERR;
	}

	conference_utils_member_clear_flag(member, MFLAG_RUNNING);
	if (stream != NULL) {
		stream->write_function(stream, "+OK hup %u\n", member->id);
	}

	if (member->conference && test_eflag(member->conference, EFLAG_HUP_MEMBER)) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_member_add_event_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "hup-member");
			switch_event_fire(&event);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_kick(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	conference_utils_member_clear_flag(member, MFLAG_RUNNING);
	conference_utils_member_set_flag_locked(member, MFLAG_KICKED);
	switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);

	if (data && member->session) {
		member->kicked_sound = switch_core_session_strdup(member->session, (char *) data);
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK kicked %u\n", member->id);
	}

	if (member->conference && test_eflag(member->conference, EFLAG_KICK_MEMBER)) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_member_add_event_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "kick-member");
			switch_event_fire(&event);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_border(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *arg = (char *) data;
	mcu_layer_t *layer = NULL;
	int len = 5;
	
	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(arg)) {
		if (stream) {
			stream->write_function(stream, "-ERR No text supplied\n", switch_channel_get_name(member->channel));
		}
		goto end;
	}
	
	layer = conference_video_get_layer_locked(member);

	if (!layer) {
		if (stream) {
			stream->write_function(stream, "-ERR Channel %s is not in a video layer\n", switch_channel_get_name(member->channel));
		}
		goto end;
	}

	if (!strcasecmp(arg, "toggle")) {
		if (member->video_manual_border) {
			len = 0;
		} else {
			len = 5;
		}
	} else {
		len = atoi(arg);
	}

	if (len < 0 || len > 20) {
		len = 0;
	}
	
	member->video_manual_border = len;
	layer->manual_border = len;

	if (stream) {
		stream->write_function(stream, "+OK\n");
	}
	
 end:

	if (layer) {
		conference_video_release_layer(&layer);
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_vid_flip(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *arg = (char *) data;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if ((conference_utils_member_test_flag(member, MFLAG_FLIP_VIDEO) || conference_utils_member_test_flag(member, MFLAG_MIRROR_VIDEO)) && !arg) {
		conference_utils_member_clear_flag_locked(member, MFLAG_FLIP_VIDEO);
		conference_utils_member_clear_flag_locked(member, MFLAG_ROTATE_VIDEO);
		conference_utils_member_clear_flag_locked(member, MFLAG_MIRROR_VIDEO);
	} else {

		if (arg && !strcasecmp(arg, "mirror")) {
			if (conference_utils_member_test_flag(member, MFLAG_MIRROR_VIDEO)) {
				conference_utils_member_clear_flag_locked(member, MFLAG_MIRROR_VIDEO);
			} else {
				conference_utils_member_set_flag_locked(member, MFLAG_MIRROR_VIDEO);
			}
		} else {
			conference_utils_member_set_flag_locked(member, MFLAG_FLIP_VIDEO);

			if (arg) {
				if (!strcasecmp(arg, "rotate")) {
					conference_utils_member_set_flag_locked(member, MFLAG_ROTATE_VIDEO);
				} else if (switch_is_number(arg)) {
					int num = atoi(arg);
				
					if (num == 0 || num == 90 || num == 180 || num == 270) {
						member->flip = num;
					}
				}
			} else {
				member->flip = 180;
			}
		}
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK flipped %u\n", member->id);
	}


	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_dtmf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;
	char *dtmf = (char *) data;

	if (member == NULL) {
		if (stream != NULL) stream->write_function(stream, "-ERR Invalid member!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(dtmf)) {
		if (stream != NULL) stream->write_function(stream, "-ERR Invalid input!\n");
		return SWITCH_STATUS_GENERR;
	} else {
		char *p;

		for(p = dtmf; p && *p; p++) {
			switch_dtmf_t *dt, digit = { *p, SWITCH_DEFAULT_DTMF_DURATION };

			switch_zmalloc(dt, sizeof(*dt));
			*dt = digit;

			switch_queue_push(member->dtmf_queue, dt);
			switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);
		}
	}

	if (stream != NULL) {
		stream->write_function(stream, "+OK sent %s to %u\n", (char *) data, member->id);
	}

	if (test_eflag(member->conference, EFLAG_DTMF_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "dtmf-member");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Digits", dtmf);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_watching_canvas(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index;
	char *val = (char *) data;

	if (member->conference->canvas_count == 1) {
		stream->write_function(stream, "-ERR Only 1 Canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	index = conference_member_get_canvas_id(member, val, SWITCH_TRUE);

	if (index < 0) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		return SWITCH_STATUS_SUCCESS;
	}

	member->watching_canvas_id = index;
	conference_video_reset_member_codec_index(member);
	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	member->conference->canvases[index]->send_keyframe = 10;
	member->conference->canvases[index]->refresh = 1;
	stream->write_function(stream, "+OK watching canvas %d\n", index + 1);

	conference_member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_canvas(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index;
	char *val = (char *) data;
	//mcu_canvas_t *canvas = NULL;

	if (member->conference->canvas_count == 1) {
		stream->write_function(stream, "-ERR Only 1 Canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(member->conference->canvas_mutex);

	index = conference_member_get_canvas_id(member, val, SWITCH_FALSE);

	if (index < 0) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		switch_mutex_unlock(member->conference->canvas_mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	conference_video_detach_video_layer(member);
	member->canvas_id = index;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;

	//canvas = member->conference->canvases[member->canvas_id];
	//conference_video_attach_video_layer(member, canvas, index);
	conference_video_reset_member_codec_index(member);
	switch_mutex_unlock(member->conference->canvas_mutex);

	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	member->conference->canvases[index]->send_keyframe = 10;
	member->conference->canvases[index]->refresh = 1;
	stream->write_function(stream, "+OK canvas %d\n", member->canvas_id + 1);

	return SWITCH_STATUS_SUCCESS;
}



switch_status_t conference_api_sub_layer(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index = -1;
	mcu_canvas_t *canvas = NULL;
	char *val = (char *) data;

	if (!val) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (member->canvas_id < 0) {
		stream->write_function(stream, "-ERR Invalid Canvas\n");
		return SWITCH_STATUS_FALSE;
	}


	switch_mutex_lock(member->conference->canvas_mutex);

	if (switch_is_number(val)) {
		index = atoi(val) - 1;

		if (index < 0) {
			index = 0;
		}
	} else {
		index = member->video_layer_id;

		if (index < 0) index = 0;

		if (!strcasecmp(val, "next")) {
			index++;
		} else if (!strcasecmp(val, "prev")) {
			index--;
		}
	}

	canvas = member->conference->canvases[member->canvas_id];

	if (index >= canvas->total_layers) {
		index = 0;
	}

	if (index < 0) {
		index = canvas->total_layers - 1;
	}

	conference_video_attach_video_layer(member, canvas, index);
	switch_mutex_unlock(member->conference->canvas_mutex);

	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	canvas->send_keyframe = 10;
	canvas->refresh = 1;
	stream->write_function(stream, "+OK layer %d\n", member->video_layer_id + 1);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_energy(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->energy_level += 200;
			if (member->energy_level > 1800) {
				member->energy_level = 1800;
			}
		} else if (!strcasecmp(data, "down")) {
			member->energy_level -= 200;
			if (member->energy_level < 0) {
				member->energy_level = 0;
			}
		} else {
			member->energy_level = atoi((char *) data);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "Energy %u = %d\n", member->id, member->energy_level);
	}

	if (member->auto_energy_level && member->energy_level > member->auto_energy_level) {
		member->auto_energy_level  = 0;
		if (stream != NULL) {
			stream->write_function(stream, "Auto-Energy level exceeded, Auto-Energy mode disabled\n", SWITCH_VA_NONE);
		}
	}


	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

void conference_api_set_agc(conference_member_t *member, const char *data)
{
	int tmp = 0;
	char *argv[4] = { 0 };
	char *conf;

	if (data) {
		conf = switch_core_strdup(member->pool, data);
		switch_split(conf, ':', argv);
	} else {
		member->agc_level = member->conference->agc_level;
		member->agc_low_energy_level = member->conference->agc_low_energy_level;
		member->agc_change_factor = member->conference->agc_change_factor;
		member->agc_margin = member->conference->agc_margin;
		member->agc_period_len = member->conference->agc_period_len;
	}

	if (argv[0]) {
		tmp = atoi(argv[0]);

		if (tmp > 0) {
			member->agc_level = tmp;
		}
	}

	if (argv[1]) {
		tmp = atoi(argv[1]);

		if (tmp > 0) {
			member->agc_low_energy_level = tmp;
		}
	}


	if (argv[2]) {
		tmp = atoi(argv[2]);

		if (tmp > 0) {
			member->agc_change_factor = tmp;
		}
	}


	if (argv[0]) {
		tmp = atoi(argv[0]);

		if (tmp > 0) {
			member->agc_period_len = (1000 / member->conference->interval) * tmp;
		}
	}


	if (!member->agc) {
		switch_agc_create(&member->agc, member->agc_level, member->agc_low_energy_level, member->agc_margin,
						  member->agc_change_factor, member->agc_period_len);
		switch_agc_set_token(member->agc, switch_channel_get_name(member->channel));
	} else {
		switch_agc_set(member->agc, member->agc_level, member->agc_low_energy_level, member->agc_margin,
					   member->agc_change_factor, member->agc_period_len);
	}

}


switch_status_t conference_api_sub_agc(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->agc_level += 200;
			if (member->agc_level > 1800) {
				member->agc_level = 1800;
			}
		} else if (!strcasecmp(data, "down")) {
			member->agc_level -= 200;
			if (member->agc_level < 0) {
				member->agc_level = 0;
			}
		} else {
			conference_api_set_agc(member, (char *)data);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "Agc %u = %d\n", member->id, member->agc_level);
	}


	//	if (test_eflag(member->conference, EFLAG_AGC_LEVEL_MEMBER) &&
	if (data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "agc-level-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Agc-Level", "%d", member->agc_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_auto_energy(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->auto_energy_level += 200;
			if (member->auto_energy_level > 1800) {
				member->auto_energy_level = 1800;
			}
		} else if (!strcasecmp(data, "down")) {
			member->auto_energy_level -= 200;
			if (member->auto_energy_level < 0) {
				member->auto_energy_level = 0;
			}
		} else {
			member->auto_energy_level = atoi((char *) data);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "%u = Auto-Energy: %d Energy: %d\n", member->id, member->auto_energy_level, member->energy_level);
	}

	if (!member->energy_level) {
		member->energy_level = member->auto_energy_level / 2;
	}


	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "auto-energy-level-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Auto-Energy-Level", "%d", member->auto_energy_level);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_max_energy(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->max_energy_level += 200;
			if (member->max_energy_level > 1800) {
				member->max_energy_level = 1800;
			}
		} else if (!strcasecmp(data, "down")) {
			member->max_energy_level -= 200;
			if (member->max_energy_level < 0) {
				member->max_energy_level = 0;
			}
		} else {
			member->max_energy_level = atoi((char *) data);
		}
		unlock_member(member);
	}

	if (member->max_energy_level && member->max_energy_level < member->energy_level) {
		member->max_energy_level = 0;
		if (stream != NULL) stream->write_function(stream, "-ERR %u Max-Energy cannot exceed energy level.\n", member->id);
	} else if (data) {
		char *p, *q;
		if ((p = strchr(data, ':'))) {
			p++;
			if (*p) {
				int tmp = atoi(p);
				if (tmp >= 0) {
					member->burst_mute_count = tmp / member->conference->interval;
				}

				if ((q = strchr(p, ':'))) {
					q++;
					if (*q) {
						int tmp = atoi(q);
						
						if (tmp >= 0) {
							member->max_energy_hit_trigger = tmp;
						}
					}
				}
			}
		}
	}


	if (stream != NULL) {
		stream->write_function(stream, "%u = Max-Energy: %d Energy: %d Max-Energy-Mute: %dms Max-Energy-Hit-Trigger %d\n", 
							   member->id, member->energy_level, member->max_energy_level, member->burst_mute_count * member->conference->interval, member->max_energy_hit_trigger);
	}


	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "max-energy-level-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Max-Energy-Level", "%d", member->max_energy_level);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Max-Energy-Mute", "%d", member->burst_mute_count * member->conference->interval);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Max-Energy-Hit-Trigger", "%d", member->max_energy_hit_trigger);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->max_energy_level);

		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_auto_position(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
#ifdef OPENAL_POSITIONING
	char *arg = NULL;
	int set = 0;

	if (argc > 2) {
		arg = argv[2];
	}


	if (!zstr(arg)) {
		if (!strcasecmp(arg, "on")) {
			conference_utils_set_flag(conference, CFLAG_POSITIONAL);
			set = 1;
		} else if (!strcasecmp(arg, "off")) {
			conference_utils_clear_flag(conference, CFLAG_POSITIONAL);
		}
	}

	if (set && conference_utils_test_flag(conference, CFLAG_POSITIONAL)) {
		conference_al_gen_arc(conference, stream);
	}

	stream->write_function(stream, "+OK positioning %s\n", conference_utils_test_flag(conference, CFLAG_POSITIONAL) ? "on" : "off");

#else
	stream->write_function(stream, "-ERR not supported\n");

#endif

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_position(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
#ifndef OPENAL_POSITIONING
	if (stream) stream->write_function(stream, "-ERR not supported\n");
#else
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (conference_utils_member_test_flag(member, MFLAG_NO_POSITIONAL)) {
		if (stream) stream->write_function(stream, 
										   "-ERR %s has positional audio blocked.\n", switch_channel_get_name(member->channel));
		return SWITCH_STATUS_SUCCESS;
	}

	if (!member->al) {
		if (!conference_utils_member_test_flag(member, MFLAG_POSITIONAL) && member->conference->channels == 2) {
			conference_utils_member_set_flag(member, MFLAG_POSITIONAL);
			member->al = conference_al_create(member->pool);
		} else {

			if (stream) {
				stream->write_function(stream, "-ERR Positional audio not avalilable %d\n", member->conference->channels);
			}
			return SWITCH_STATUS_FALSE;
		}
	}


	if (data) {
		if (conference_member_parse_position(member, data) != SWITCH_STATUS_SUCCESS) {
			if (stream) {
				stream->write_function(stream, "-ERR invalid input!\n");
			}
			return SWITCH_STATUS_FALSE;
		}
	}


	if (stream != NULL) {
		stream->write_function(stream, "+OK Position %u = %0.2f:%0.2f:%0.2f\n", member->id, member->al->pos_x, member->al->pos_y, member->al->pos_z);
	}

	if (test_eflag(member->conference, EFLAG_SET_POSITION_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "set-position-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Position", "%0.2f:%0.2f:%0.2f", member->al->pos_x, member->al->pos_y, member->al->pos_z);
		switch_event_fire(&event);
	}

#endif

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_volume_in(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->volume_in_level++;
			switch_normalize_volume(member->volume_in_level);
		} else if (!strcasecmp(data, "down")) {
			member->volume_in_level--;
			switch_normalize_volume(member->volume_in_level);
		} else {
			member->volume_in_level = atoi((char *) data);
			switch_normalize_volume(member->volume_in_level);
		}
		unlock_member(member);

	}
	if (stream != NULL) {
		stream->write_function(stream, "+OK Volume IN %u = %d\n", member->id, member->volume_in_level);
	}
	if (test_eflag(member->conference, EFLAG_VOLUME_IN_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-in-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_volume_out(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->volume_out_level++;
			switch_normalize_volume(member->volume_out_level);
		} else if (!strcasecmp(data, "down")) {
			member->volume_out_level--;
			switch_normalize_volume(member->volume_out_level);
		} else {
			member->volume_out_level = atoi((char *) data);
			switch_normalize_volume(member->volume_out_level);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "+OK Volume OUT %u = %d\n", member->id, member->volume_out_level);
	}
	if (test_eflag(member->conference, EFLAG_VOLUME_OUT_MEMBER) && data &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-out-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_personal(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int on = 0;

	if (!conference->canvases[0]) {
		stream->write_function(stream, "-ERR conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (argv[2]) {
		on = switch_true(argv[2]);
		if (on) {
			conference_utils_set_flag(conference, CFLAG_PERSONAL_CANVAS);
			conference->video_layout_group = "grid";
			conference_utils_set_flag(conference, CFLAG_REFRESH_LAYOUT);
		} else {
			conference_utils_clear_flag(conference, CFLAG_PERSONAL_CANVAS);
		}
	}

	stream->write_function(stream, "+OK personal is %s\n", on ? "on" : "off");

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_bandwidth(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint32_t i;
	int32_t video_write_bandwidth;
	int x = 0, id = -1;
	char *group = NULL;
	char *array[4] = {0};
	float sdiv = 0;
	int fdiv = 0;
	int force_w = 0, force_h = 0;
	conference_member_t *imember;
	
	if (!argv[2]) {
		stream->write_function(stream, "-ERR Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}
	
	switch_split(argv[2], ':', array);

	if (array[1]) {
		if (*array[1] == '=') {
			char *p = array[1];
			
			force_w = atoi((p+1));
			if ((p = strchr(p+1, 'x'))) {
				p++;
				if (*p) {
					force_h = atoi(p);
				}
			}

			if (!(force_w > 100 && force_w < 1920 && force_h > 100 && force_h < 1080)) {
				force_w = force_h = 0;
			}
		} else {
			sdiv = atof(array[1]);
			if (sdiv < 1.5 || sdiv > 8.0) {
				sdiv = 0;
			}
		}
	}
	
	if (array[2]) {
		fdiv = atoi(array[2]);
		if (fdiv < 2 || fdiv > 8) {
			fdiv = 0;
		}
	}

	video_write_bandwidth = switch_parse_bandwidth_string(array[0]);
	
	if (argv[3]) {
		group = argv[3];
	}

	if (argv[4]) {
		id = atoi(argv[4]);

		if (id < 1 || id > MAX_CANVASES+1) {
			id = -1;
		}

		if (id < 1 || id > conference->canvas_count) {
			stream->write_function(stream, "-ERR Invalid canvas\n");
			goto end;
		}
	}

	switch_mutex_lock(conference->member_mutex);

	for (imember = conference->members; imember; imember = imember->next) {
		
		if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
			continue;
		}

		switch_core_media_set_outgoing_bitrate(imember->session, SWITCH_MEDIA_TYPE_VIDEO, video_write_bandwidth);
		
		stream->write_function(stream, "+OK Set Bandwidth %d kps for member %s\n", video_write_bandwidth, switch_channel_get_name(imember->channel));
	}

	for (i = 0; i <= conference->canvas_count; i++) {
		if (i != id - 1) {
			continue;
		}

		if (conference->canvases[i]) {
			mcu_canvas_t *canvas = conference->canvases[i];
			int j;

			for (j = 0; j < canvas->write_codecs_count; j++) {
				int w = canvas->width, h = canvas->height;
				
				if ((zstr(group) || !strcmp(group, switch_str_nil(canvas->write_codecs[j]->video_codec_group)))) {
					switch_core_codec_control(&canvas->write_codecs[j]->codec, SCC_VIDEO_BANDWIDTH,
											  SCCT_INT, &video_write_bandwidth, SCCT_NONE, NULL, NULL, NULL);
					
					if (fdiv) {
						canvas->write_codecs[j]->fps_divisor = fdiv;
					}

					if (force_w && force_h) {
						w = force_w;
						h = force_h;
					} else if (sdiv) {
						w = (int)((float) w / sdiv);
						h = (int)((float) h / sdiv);
					}

					if (w && h) {
						switch_img_free(&canvas->write_codecs[j]->scaled_img);
						if (w != canvas->img->d_w || h != canvas->img->d_h) {
							canvas->write_codecs[j]->scaled_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, w, h, 16);
						}
					}

					if (!sdiv && w) {
						sdiv = (float)canvas->img->d_w / w;
					}

					
					stream->write_function(stream, "+OK Set Bandwidth for canvas %d index %d group[%s] to %d sdiv %.2f %dx%d fdiv %d\n", i + 1, j, 
										   switch_str_nil(canvas->write_codecs[j]->video_codec_group), video_write_bandwidth, sdiv, w,h, fdiv);

					
					x++;
				}
			}
		}
	}
	switch_mutex_unlock(conference->member_mutex);

 end:

	if (!x) {
		stream->write_function(stream, "-ERR Bandwidth not set\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_canvas_bgimg(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	mcu_canvas_t *canvas = NULL;
	int idx = 0;
	char *file = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!argv[2]) {
		stream->write_function(stream, "-ERR Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	file = argv[2];

	if (argv[3]) {
		idx = atoi(argv[3]) - 1;
	}

	if (idx < 0 || idx > SUPER_CANVAS_ID || !conference->canvases[idx]) {
		stream->write_function(stream, "-ERR Invalid canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	canvas = conference->canvases[idx];
	switch_mutex_lock(canvas->mutex);
	if (!strcasecmp(file, "clear")) {
		conference_video_reset_image(canvas->img, &canvas->bgcolor);
	} else {
		status = conference_video_set_canvas_bgimg(canvas, file);
	}
	switch_mutex_unlock(canvas->mutex);

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Set Bgimg %s\n", file);
	} else {
		stream->write_function(stream, "-ERR Error Setting Bgimg %s\n", file);
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_canvas_fgimg(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	mcu_canvas_t *canvas = NULL;
	int idx = 0;
	char *file = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!argv[2]) {
		stream->write_function(stream, "-ERR Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	file = argv[2];

	if (argv[3]) {
		idx = atoi(argv[3]) - 1;
	}

	if (idx < 0 || idx > SUPER_CANVAS_ID || !conference->canvases[idx]) {
		stream->write_function(stream, "-ERR Invalid canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if ((canvas = conference->canvases[idx])) {
		switch_mutex_lock(canvas->mutex);
		if (!strcasecmp(file, "clear")) {
			conference_video_reset_image(canvas->img, &canvas->bgcolor);
		} else {
			status = conference_video_set_canvas_fgimg(canvas, file);
		}
		switch_mutex_unlock(canvas->mutex);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Set FGimg %s\n", file);
	} else {
		stream->write_function(stream, "-ERR Error Setting FGimg %s\n", file);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_res(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int canvas_w = 0, canvas_h = 0, id = 0;
	char *video_canvas_size = argv[2];

	
	if (!conference->canvases[0]) {
		stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(video_canvas_size)) {
		stream->write_function(stream, "-ERR Invalid size\n");
		return SWITCH_STATUS_SUCCESS;
	} else {
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
		stream->write_function(stream, "-ERR Invalid size, [%dx%d] is too small\n", canvas_w, canvas_h);
		return SWITCH_STATUS_SUCCESS;
	}

	if (canvas_w > 7680 || canvas_h > 4320) {
		stream->write_function(stream, "-ERR Invalid size, [%dx%d] is too large.\n", canvas_w, canvas_h);
		return SWITCH_STATUS_SUCCESS;
	}


	if (argv[3]) {

		id = atoi(argv[3]);
		
		if (id < 1 || id > MAX_CANVASES+1) {
			id = -1;
		}

		if (id < 1) {
			stream->write_function(stream, "-ERR Invalid canvas\n");
		}

	}

	if (id == 0) id = 1;

	if (id > conference->canvas_count + 1) {
		id = 1;
	}

	if (conference_video_change_res(conference, canvas_w, canvas_h, id - 1) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Resolution set to [%dx%d]\n", canvas_w, canvas_h);
	} else {
		stream->write_function(stream, "-ERR Resolution not set\n");
	}
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_fps(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	float fps = 0;

	if (!conference->canvases[0]) {
		stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!argv[2]) {
		stream->write_function(stream, "+OK Current FPS [%0.2f]\n", conference->video_fps.fps);
		return SWITCH_STATUS_SUCCESS;
	}

	fps = (float)atof(argv[2]);

	if (conference_video_set_fps(conference, fps)) {
		stream->write_function(stream, "+OK FPS set to [%0.2f]\n", conference->video_fps.fps);
	} else {
		stream->write_function(stream, "-ERR Invalid FPS [%s]\n", argv[2]);
	}

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t conference_api_sub_write_png(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	mcu_canvas_t *canvas = NULL;

	if (!argv[2]) {
		stream->write_function(stream, "-ERR Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!conference->canvas_count) {
		stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference->canvas_count > 1) {
		/* pick super canvas */
		canvas = conference->canvases[conference->canvas_count];
	} else {
		canvas = conference->canvases[0];
	}

	switch_mutex_lock(canvas->mutex);
	status = switch_img_write_png(canvas->img, argv[2]);
	switch_mutex_unlock(canvas->mutex);

	stream->write_function(stream, "%s\n", status == SWITCH_STATUS_SUCCESS ? "+OK" : "-ERR");

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_layout(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	video_layout_t *vlayout = NULL;
	char *group_name = NULL;

	int idx = 0;

	if (!argv[2]) {
		stream->write_function(stream, "-ERR Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!conference->canvases[0]) {
		stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[2], "list")) {
		switch_hash_index_t *hi;
		void *val;
		const void *vvar;
		for (hi = switch_core_hash_first(conference->layout_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, &vvar, NULL, &val);
			stream->write_function(stream, "%s\n", (char *)vvar);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strncasecmp(argv[2], "group", 5)) {
		layout_group_t *lg = NULL;
		int xx = 4;

		if ((group_name = strchr(argv[2], ':'))) {
			group_name++;
			xx--;
		} else {
			group_name = argv[3];
		}

		if (!group_name) {
			stream->write_function(stream, "-ERR Group name not specified.\n");
			return SWITCH_STATUS_SUCCESS;
		} else {
			if (((lg = switch_core_hash_find(conference->layout_group_hash, group_name)))) {
				if (conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
					stream->write_function(stream, "-ERR Change personal canvas to layout group [%s]\n", group_name);
					conference->video_layout_group = switch_core_strdup(conference->pool, group_name);
					conference_utils_set_flag(conference, CFLAG_REFRESH_LAYOUT);
					return SWITCH_STATUS_SUCCESS;
				}
			} else {
				group_name = NULL;
			}

			stream->write_function(stream, "+OK Change to layout group [%s]\n", group_name);

			if (argv[xx]) {
				if ((idx = atoi(argv[xx])) > 0) {
					idx--;
				}
			}
		}
	}

	if ((vlayout = switch_core_hash_find(conference->layout_hash, argv[2]))) {
		if (argv[3]) {
			if ((idx = atoi(argv[3]))) {
				idx--;
			}
		}
	}

	if (!vlayout && !group_name) {
		stream->write_function(stream, "-ERR Invalid layout [%s]\n", argv[2]);
		return SWITCH_STATUS_SUCCESS;
	}

	if (idx < 0 || idx > (int)(conference->canvas_count - 1)) idx = 0;


	if (conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
		if (vlayout) {
			stream->write_function(stream, "+OK Change personal canvas set to layout [%s]\n", vlayout->name);
			switch_mutex_lock(conference->member_mutex);
			conference->new_personal_vlayout = vlayout;
			switch_mutex_unlock(conference->member_mutex);
		} else {
			stream->write_function(stream, "-ERR no layout for personal canvas\n");
			return SWITCH_STATUS_SUCCESS;
		}
	} else {

		switch_mutex_lock(conference->canvases[idx]->mutex);
		if (vlayout) {
			stream->write_function(stream, "+OK Change canvas %d to layout [%s]\n", idx + 1, vlayout->name);
			conference->canvases[idx]->new_vlayout = vlayout;
			conference->canvases[idx]->video_layout_group = NULL;
		} else if (group_name) {
			conference->canvases[idx]->video_layout_group = switch_core_strdup(conference->pool, group_name);
			conference_utils_set_flag(conference, CFLAG_REFRESH_LAYOUT);
		}
		switch_mutex_unlock(conference->canvases[idx]->mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_count(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{

	if (conference) {
		conference_list_count_only(conference, stream);
	} else {
		int count = 0;
		switch_hash_index_t *hi;
		switch_mutex_lock(conference_globals.hash_mutex);
		for (hi = switch_core_hash_first(conference_globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			count++;
		}
		switch_mutex_unlock(conference_globals.hash_mutex);
		stream->write_function(stream, "%d", count);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int ret_status = SWITCH_STATUS_GENERR;
	int count = 0;
	switch_hash_index_t *hi;
	void *val;
	char *d = ";";
	int pretty = 0;
	int summary = 0;
	int countonly = 0;
	int argofs = (argc >= 2 && strcasecmp(argv[1], "list") == 0);	/* detect being called from chat vs. api */

	if (argv[1 + argofs]) {
		if (argv[2 + argofs] && !strcasecmp(argv[1 + argofs], "delim")) {
			d = argv[2 + argofs];

			if (*d == '"') {
				if (++d) {
					char *p;
					if ((p = strchr(d, '"'))) {
						*p = '\0';
					}
				} else {
					d = ";";
				}
			}
		} else if (strcasecmp(argv[1 + argofs], "pretty") == 0) {
			pretty = 1;
		} else if (strcasecmp(argv[1 + argofs], "summary") == 0) {
			summary = 1;
		} else if (strcasecmp(argv[1 + argofs], "count") == 0) {
			countonly = 1;
		}
	}

	if (conference == NULL) {
		switch_mutex_lock(conference_globals.hash_mutex);
		for (hi = switch_core_hash_first(conference_globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			int fcount = 0;
			switch_core_hash_this(hi, NULL, NULL, &val);
			conference = (conference_obj_t *) val;

			stream->write_function(stream, "+OK Conference %s (%u member%s rate: %u%s flags: ",
								   conference->name,
								   conference->count,
								   conference->count == 1 ? "" : "s", conference->rate, conference_utils_test_flag(conference, CFLAG_LOCKED) ? " locked" : "");

			if (conference_utils_test_flag(conference, CFLAG_LOCKED)) {
				stream->write_function(stream, "%slocked", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_DESTRUCT)) {
				stream->write_function(stream, "%sdestruct", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_WAIT_MOD)) {
				stream->write_function(stream, "%swait_mod", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_AUDIO_ALWAYS)) {
				stream->write_function(stream, "%saudio_always", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_RUNNING)) {
				stream->write_function(stream, "%srunning", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_ANSWERED)) {
				stream->write_function(stream, "%sanswered", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_ENFORCE_MIN)) {
				stream->write_function(stream, "%senforce_min", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_BRIDGE_TO)) {
				stream->write_function(stream, "%sbridge_to", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_DYNAMIC)) {
				stream->write_function(stream, "%sdynamic", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_EXIT_SOUND)) {
				stream->write_function(stream, "%sexit_sound", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_ENTER_SOUND)) {
				stream->write_function(stream, "%senter_sound", fcount ? "|" : "");
				fcount++;
			}

			if (conference->record_count > 0) {
				stream->write_function(stream, "%srecording", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_VID_FLOOR)) {
				stream->write_function(stream, "%svideo_floor_only", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_RFC4579)) {
				stream->write_function(stream, "%svideo_rfc4579", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_LIVEARRAY_SYNC)) {
				stream->write_function(stream, "%slivearray_sync", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK)) {
				stream->write_function(stream, "%svideo_floor_lock", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
				stream->write_function(stream, "%stranscode_video", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_VIDEO_MUXING)) {
				stream->write_function(stream, "%svideo_muxing", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
				stream->write_function(stream, "%sminimize_video_encoding", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE)) {
				stream->write_function(stream, "%smanage_inbound_bitrate", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_JSON_STATUS)) {
				stream->write_function(stream, "%sjson_status", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
				stream->write_function(stream, "%svideo_bridge_first_two", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
				stream->write_function(stream, "%svideo_required_for_canvas", fcount ? "|" : "");
				fcount++;
			}

			if (conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
				stream->write_function(stream, "%spersonal_canvas", fcount ? "|" : "");
				fcount++;
			}

			if (!fcount) {
				stream->write_function(stream, "none");
			}

			stream->write_function(stream, ")\n");

			count++;
			if (!summary) {
				if (pretty) {
					conference_list_pretty(conference, stream);
				} else {
					conference_list(conference, stream, d);
				}
			}
		}
		switch_mutex_unlock(conference_globals.hash_mutex);
	} else {
		count++;
		if (countonly) {
			conference_list_count_only(conference, stream);
		} else if (pretty) {
			conference_list_pretty(conference, stream);
		} else {
			conference_list(conference, stream, d);
		}
	}

	if (!count) {
		stream->write_function(stream, "+OK No active conferences.\n");
	}

	ret_status = SWITCH_STATUS_SUCCESS;

	return ret_status;
}

switch_status_t conference_api_sub_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (conference_utils_member_test_flag(member, MFLAG_DED_VID_LAYER) && !conference_utils_test_flag(member->conference, CFLAG_DED_VID_LAYER_AUDIO_FLOOR)) {
		if (stream != NULL) {
			stream->write_function(stream, "-ERR cannot set floor on a member in an active video role\n");
		}

		return SWITCH_STATUS_SUCCESS;
	}
	
	if (member->conference->floor_holder == member->id) {
		conference_member_set_floor_holder(member->conference, NULL, 0);
		if (stream != NULL) {
			stream->write_function(stream, "+OK floor none\n");
		}
	} else if (member->conference->floor_holder == 0) {
		conference_member_set_floor_holder(member->conference, member, 0);
		if (stream != NULL) {
			stream->write_function(stream, "+OK floor %u\n", member->id);
		}
	} else {
		if (stream != NULL) {
			stream->write_function(stream, "-ERR floor is held by %u\n", member->conference->floor_holder);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_clear_vid_floor(conference_obj_t *conference, switch_stream_handle_t *stream, void *data)
{

	switch_mutex_lock(conference->mutex);
	conference_utils_clear_flag(conference, CFLAG_VID_FLOOR_LOCK);
	//conference_video_set_floor_holder(conference, NULL);
	switch_mutex_unlock(conference->mutex);

	stream->write_function(stream, "+OK floor Cleared\n", SWITCH_VA_NONE);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_mute_img(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	layer = conference_video_get_layer_locked(member);

	if (!layer) {
		goto end;
	}

	member->video_mute_png = NULL;

	if (text) {
		switch_img_free(&layer->mute_img);
	}

	if (text && strcasecmp(text, "clear")) {
		member->video_mute_png = switch_core_strdup(member->pool, text);
	}

end:

	stream->write_function(stream, "%s\n", member->video_mute_png ? member->video_mute_png : "_undef_");

	conference_video_release_layer(&layer);

	return SWITCH_STATUS_SUCCESS;

}


switch_status_t conference_api_sub_vid_logo_img(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	conference_member_set_logo(member, text);

	layer = conference_video_get_layer_locked(member);

	if (!layer) {
		goto end;
	}

	conference_video_layer_set_logo(member, layer);

 end:

	stream->write_function(stream, "+OK Video logo %s\n", member->video_logo ? "set" : "cleared");

	conference_video_release_layer(&layer);

	return SWITCH_STATUS_SUCCESS;

}


switch_status_t conference_api_sub_vid_codec_group(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (text) {

		if (!strcmp(text, "clear")) {
			member->video_codec_group = NULL;
		} else {
			member->video_codec_group = switch_core_strdup(member->pool, text);
		}

		switch_mutex_lock(member->conference->member_mutex);
		member->video_codec_index = -1;
		switch_mutex_unlock(member->conference->member_mutex);
		stream->write_function(stream, "+OK Video codec group %s %s\n", member->video_codec_group ? "set" : "cleared", switch_str_nil(member->video_codec_group));
	} else {
		stream->write_function(stream, "+OK Video codec group is %s\n", member->video_codec_group);
	}
	

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t conference_api_sub_get_uuid(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	if (member->session) {
		stream->write_function(stream, "%s", switch_core_session_get_uuid(member->session));
	} else {
		stream->write_function(stream, "_undef_");
	}

	return SWITCH_STATUS_SUCCESS;
}

static void clear_res_id(conference_obj_t *conference, conference_member_t *member, const char *id)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (imember == member) {
			continue;
		}

		if (imember->video_reservation_id && !strcasecmp(imember->video_reservation_id, id)) {
			imember->video_reservation_id = NULL;
			conference_video_detach_video_layer(imember);
		}
	}
	switch_mutex_unlock(conference->member_mutex);
}

static void clear_role_id(conference_obj_t *conference, conference_member_t *member, const char *id)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (imember == member) {
			continue;
		}

		if (imember->video_role_id && !strcasecmp(imember->video_role_id, id)) {
			imember->video_role_id = NULL;
			conference_video_detach_video_layer(imember);
		}
	}
	switch_mutex_unlock(conference->member_mutex);
}

switch_status_t conference_api_sub_vid_res_id(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint8_t all = 0, clear = 0, force = 0;
	uint32_t member_id;
	char *res_id = NULL;
	conference_member_t *member;

	if (argc < 3 || argc > 5)
		return SWITCH_STATUS_GENERR;

	res_id = argv[3];

	if (argc > 3) {
		if (!strcasecmp(res_id, "clear")) {
			clear = 1;
		}
	} else {
		clear = 1;
	}

	if (argc > 4)
		force = strcasecmp(argv[4], "force") ? 0 : 1;

	if (!(member_id = atoi(argv[2]))) {
		all = strcasecmp(argv[2], "all") ? 0 : 1;
	}

	if (all && clear) {
		switch_mutex_lock(conference->member_mutex);
		for (member = conference->members; member; member = member->next) {
			if (member->session && !conference_utils_member_test_flag(member, MFLAG_NOCHANNEL)) {
				conference_api_sub_vid_res_id_member(member, stream, res_id, clear, force);
			}
		}
		switch_mutex_unlock(conference->member_mutex);
	} else if (member_id) {
		if (!(member = conference_member_get(conference, member_id)))
			return SWITCH_STATUS_GENERR;
		conference_api_sub_vid_res_id_member(member, stream, res_id, clear, force);
		switch_thread_rwlock_unlock(member->rwlock);
	} else {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;

}


switch_status_t conference_api_sub_vid_res_id_member(conference_member_t *member, switch_stream_handle_t *stream, char *res_id, int clear, int force)
{

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!member->conference->canvases[0]) {
		stream->write_function(stream, "-ERR conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (clear || (!force && member->video_reservation_id && !strcasecmp(res_id, member->video_reservation_id))) {
		member->video_reservation_id = NULL;
		stream->write_function(stream, "+OK reservation_id cleared\n");
		conference_video_detach_video_layer(member);
	} else {
		clear_res_id(member->conference, member, res_id);
		if (!member->video_reservation_id || strcmp(member->video_reservation_id, res_id)) {
			member->video_reservation_id = switch_core_strdup(member->pool, res_id);
		}
		stream->write_function(stream, "+OK reservation_id %s\n", res_id);
		conference_video_detach_video_layer(member);
		conference_video_find_floor(member, SWITCH_FALSE);
	}

	return SWITCH_STATUS_SUCCESS;

}


switch_status_t conference_api_sub_vid_role_id(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	int force = 0;
	
	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!member->conference->canvases[0]) {
		stream->write_function(stream, "-ERR conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!zstr(text) && *text == '=') {
		text++;
		force = 1;
	}
	
	if (zstr(text) || !strcasecmp(text, "clear") || (!force && member->video_role_id && !strcasecmp(text, member->video_role_id))) {
		member->video_role_id = NULL;
		stream->write_function(stream, "+OK role_id cleared\n");
	} else {
		clear_role_id(member->conference, member, text);
		if (!member->video_role_id || strcmp(member->video_role_id, text)) {
			member->video_role_id = switch_core_strdup(member->pool, text);
		}
		stream->write_function(stream, "+OK role_id %s\n", text);
		conference_video_find_floor(member, SWITCH_FALSE);
	}



	return SWITCH_STATUS_SUCCESS;

}

switch_status_t conference_api_sub_vid_banner(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	mcu_layer_t *layer = NULL;
	char *text = (char *) data;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	switch_url_decode(text);

 	if (zstr(text)) goto end;

	member->video_banner_text = switch_core_strdup(member->pool, text);

	layer = conference_video_get_layer_locked(member);

	if (!layer) goto end;

	conference_video_layer_set_banner(member, layer, NULL);

 end:

	stream->write_function(stream, "+OK\n");

	conference_video_release_layer(&layer);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_vid_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int force = 0;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO) && !member->avatar_png_img) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Channel %s does not have video capability!\n", switch_channel_get_name(member->channel));
		return SWITCH_STATUS_FALSE;
	}

	if (conference_utils_member_test_flag(member, MFLAG_DED_VID_LAYER)) {
		if (stream != NULL) {
			stream->write_function(stream, "-ERR cannot set floor on a member in an active video role\n");
		}
		
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (data && switch_stristr("force", (char *) data)) {
		force = 1;
	}

	if (member->conference->video_floor_holder == member->id && conference_utils_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK)) {
		conference_utils_clear_flag(member->conference, CFLAG_VID_FLOOR_LOCK);

		conference_member_set_floor_holder(member->conference, member, 0);
		if (stream == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor auto\n", member->conference->name);
		} else {
			stream->write_function(stream, "+OK floor none\n");
		}

	} else if (force || member->conference->video_floor_holder == 0) {
		conference_utils_set_flag(member->conference, CFLAG_VID_FLOOR_LOCK);
		conference_video_set_floor_holder(member->conference, member, SWITCH_TRUE);
		if (test_eflag(member->conference, EFLAG_FLOOR_CHANGE)) {
			if (stream == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor %d %s\n",
								  member->conference->name, member->id, switch_channel_get_name(member->channel));
			} else {
				stream->write_function(stream, "+OK floor %u\n", member->id);
			}
		}
	} else {
		if (stream == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s floor already held by %d %s\n",
							  member->conference->name, member->id, switch_channel_get_name(member->channel));
		} else {
			stream->write_function(stream, "-ERR floor is held by %u\n", member->conference->video_floor_holder);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}



switch_status_t conference_api_sub_file_seek(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc == 3) {
		switch_mutex_lock(conference->mutex);
		conference_fnode_seek(conference->fnode, stream, argv[2]);
		switch_mutex_unlock(conference->mutex);

		return SWITCH_STATUS_SUCCESS;
	}

	if (argc == 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member = conference_member_get(conference, id);
		if (member == NULL) {
			stream->write_function(stream, "-ERR Member: %u not found.\n", id);
			return SWITCH_STATUS_GENERR;
		}

		switch_mutex_lock(member->fnode_mutex);
		conference_fnode_seek(member->fnode, stream, argv[2]);
		switch_mutex_unlock(member->fnode_mutex);
		switch_thread_rwlock_unlock(member->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

switch_status_t conference_api_set_moh(conference_obj_t *conference, const char *what)
{
	if (!what) {
		return SWITCH_STATUS_FALSE;
	}

	if (!strcasecmp(what, "toggle")) {
		if (conference_utils_test_flag(conference, CFLAG_NO_MOH)) {
			conference_utils_clear_flag(conference, CFLAG_NO_MOH);
		} else {
			conference_utils_set_flag(conference, CFLAG_NO_MOH);
		}
	} else if (!strcasecmp(what, "on")) {
		conference_utils_clear_flag(conference, CFLAG_NO_MOH);
	} else if (!strcasecmp(what, "off")) {
		conference_utils_set_flag(conference, CFLAG_NO_MOH);
	} else if (!strcasecmp(what, "reset")) {
		conference->tmp_moh_sound = NULL;
	} else {
		conference->tmp_moh_sound = switch_core_strdup(conference->pool, what);
	}

	if (conference_utils_test_flag(conference, CFLAG_NO_MOH) || conference->tmp_moh_sound) {
		conference_file_stop(conference, FILE_STOP_ASYNC);
	}
	
	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_api_sub_moh(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{

	if (conference_api_set_moh(conference, argv[2]) == SWITCH_STATUS_SUCCESS) {
		if (stream) {
			stream->write_function(stream, "+OK moh\n");
		}
	} else {
		if (stream) {
			stream->write_function(stream, "-ERR invalid moh param\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int ret_status = SWITCH_STATUS_GENERR;
	switch_event_t *event;
	uint8_t async = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if ((argc == 4 && !strcasecmp(argv[3], "async")) || (argc == 5 && !strcasecmp(argv[4], "async"))) {
		argc--;
		async++;
	}

	if (argc == 3) {
		if (conference_file_play(conference, argv[2], 0, NULL, async) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "+OK (play) Playing file %s\n", argv[2]);
			if (test_eflag(conference, EFLAG_PLAY_FILE) &&
				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_event_add_data(conference, event);

				if (conference->fnode && conference->fnode->fh.params) {
					switch_event_merge(event, conference->fnode->fh.params);
				}

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", async ? "true" : "false");
				switch_event_fire(&event);
			}
		} else {
			stream->write_function(stream, "-ERR (play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
		}
		ret_status = SWITCH_STATUS_SUCCESS;
	} else if (argc >= 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member;
		switch_bool_t mux = SWITCH_TRUE;

		if (argc > 4 && !strcasecmp(argv[4], "nomux")) {
			mux = SWITCH_FALSE;
		}

		if ((member = conference_member_get(conference, id))) {
			if (conference_member_play_file(member, argv[2], 0, mux) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "+OK (play) Playing file %s to member %u\n", argv[2], id);
				if (test_eflag(conference, EFLAG_PLAY_FILE_MEMBER) &&
					switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
					conference_member_add_event_data(member, event);

					if (member->fnode->fh.params) {
						switch_event_merge(event, member->fnode->fh.params);
					}

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-member");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
					switch_event_fire(&event);
				}
			} else {
				stream->write_function(stream, "-ERR (play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
			}
			switch_thread_rwlock_unlock(member->rwlock);
			ret_status = SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "-ERR Member: %u not found.\n", id);
		}
	}

	return ret_status;
}

switch_status_t conference_api_sub_say(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text)
{
	switch_event_t *event;

	if (zstr(text)) {
		stream->write_function(stream, "-ERR (say) Error! No text.\n");
		return SWITCH_STATUS_GENERR;
	}

	if (conference_say(conference, text, 0) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR (say) Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	stream->write_function(stream, "+OK (say) OK\n");
	if (test_eflag(conference, EFLAG_SPEAK_TEXT) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "speak-text");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Text", text);
		switch_event_fire(&event);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_saymember(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text)
{
	int ret_status = SWITCH_STATUS_GENERR;
	char *expanded = NULL;
	char *start_text = NULL;
	char *workspace = NULL;
	uint32_t id = 0;
	conference_member_t *member = NULL;
	switch_event_t *event;

	if (zstr(text)) {
		stream->write_function(stream, "-ERR (saymember) No Text!\n");
		goto done;
	}

	if (!(workspace = strdup(text))) {
		stream->write_function(stream, "-ERR (saymember) Memory Error!\n");
		goto done;
	}

	if ((start_text = strchr(workspace, ' '))) {
		*start_text++ = '\0';
		text = start_text;
	}

	id = atoi(workspace);

	if (!id || zstr(text)) {
		stream->write_function(stream, "-ERR (saymember) No Text!\n");
		goto done;
	}

	if (!(member = conference_member_get(conference, id))) {
		stream->write_function(stream, "-ERR (saymember) Unknown Member %u!\n", id);
		goto done;
	}

	if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(member->session), (char *) text)) != text) {
		text = expanded;
	} else {
		expanded = NULL;
	}

	if (!text || conference_member_say(member, (char *) text, 0) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR (saymember) Error!\n");
		goto done;
	}

	stream->write_function(stream, "+OK (saymember)\n");
	if (test_eflag(member->conference, EFLAG_SPEAK_TEXT_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_member_add_event_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "speak-text-member");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Text", text);
		switch_event_fire(&event);
	}
	ret_status = SWITCH_STATUS_SUCCESS;

 done:

	if (member) {
		switch_thread_rwlock_unlock(member->rwlock);
	}

	switch_safe_free(workspace);
	switch_safe_free(expanded);
	return ret_status;
}

switch_status_t conference_api_sub_cam(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int x;
	int canvas_id = -1;
	int layer_id = -1;
	int ok = 0;
	mcu_canvas_t *canvas = NULL;
	mcu_layer_t *layer = NULL;

	if (!conference->canvases[0]) {
		stream->write_function(stream, "-ERR Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (argc > 4) {
		canvas_id = atoi(argv[2]);
		layer_id = atoi(argv[3]);

		if (canvas_id > -1 && layer_id > -1 && canvas_id < conference->canvas_count) {
			switch_mutex_lock(conference->canvas_mutex);
			canvas = conference->canvases[canvas_id];
			switch_mutex_lock(canvas->mutex);
			if (layer_id < canvas->total_layers) {
				layer = &canvas->layers[layer_id];
				ok = 1;
				for (x = 4; x < argc; x++) {
					char *p = strchr(argv[x], '=');
					int val = -1, isfalse = 0;
					char *str_arg = NULL;
					
					if (p) {
						*p++ = '\0';
						
						if (!strcasecmp(argv[x], "zoom") || !strcasecmp(argv[x], "pan")) {
							str_arg = p;
							if (switch_false(p)) {
								isfalse = 1;
							}
						} else {
							if (switch_is_number(p)) {
								val = atoi(p);
							} else if (switch_true(p)) {
								val = 1;
							} else {
								val = 0;
								isfalse = 1;
							}
						}
					} else if (!strcasecmp(argv[x], "reset")) {
						str_arg = "true";
					}

					if (val < 0 && !str_arg) {
						stream->write_function(stream, "-ERR invalid val for option [%s]\n", argv[x]);
						continue;
					}

					if (!strcasecmp(argv[x], "autozoom")) {
						if ((layer->cam_opts.autozoom = val)) {
							layer->cam_opts.manual_zoom = 0;
						}
						
					} else if (!strcasecmp(argv[x], "autopan")) {
						if ((layer->cam_opts.autopan = val)) {
							layer->cam_opts.manual_pan = 0;
						}
					} else if (!strcasecmp(argv[x], "zoom_factor")) {
						if (val > 0 && val < 5) {
							layer->cam_opts.zoom_factor = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-4\n", argv[x]);
						}
					} else if (!strcasecmp(argv[x], "snap_factor")) {
						if (val > 0 && val < layer->screen_w / 2) {
							layer->cam_opts.snap_factor = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-%d\n", argv[x], layer->screen_w / 2);
						}
					} else if (!strcasecmp(argv[x], "zoom_move_factor")) {
						if (val > 0 && val < layer->screen_w / 2) {
							layer->cam_opts.zoom_move_factor = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-4\n", argv[x], layer->screen_w / 2);
						}
					} else if (!strcasecmp(argv[x], "pan_speed")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.pan_speed = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "pan_accel_speed")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.pan_accel_speed = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "pan_accel_min")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.pan_accel_min = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "zoom_speed")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.zoom_speed = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "zoom_accel_speed")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.zoom_accel_speed = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "zoom_accel_min")) {
						if (val > 0 && val < 100) {
							layer->cam_opts.zoom_accel_min = val;
						} else {
							stream->write_function(stream, "-ERR invalid val for option [%s] must be 1-100\n");
						}
					} else if (!strcasecmp(argv[x], "reset")) {
						conference_video_reset_layer_cam(layer);
					} else if (!strcasecmp(argv[x], "pan")) {
						char *x_val = NULL, *y_val = NULL;
						int x = -1, y = -1;
						int on = 0;

						if (isfalse) {
							layer->pan_geometry.x = 0;
							layer->pan_geometry.y = 0;
							layer->cam_opts.manual_pan = 0;
						} else {
							
							if (str_arg) {
								char *p = strchr(str_arg, ':');
								
								if (p) {
									*p++ = '\0';

									if (*str_arg == 'x') {
										x_val = p;
									} else if (*str_arg == 'y') {
										y_val = p;
									}
								}
							}

							if (!x_val && !y_val) {
								stream->write_function(stream, "-ERR invalid val for pan\n");
							}

							if (x_val) x = atoi(x_val);
							if (y_val) y = atoi(y_val);
							
							if (x_val && strrchr(x_val, 'i')) {
								int nx = (int)layer->pan_geometry.x + x;

								if (nx < 0) nx = 0;
								if (nx + layer->pan_geometry.w > layer->img->d_w) nx = layer->img->d_w - layer->pan_geometry.w;
								
								layer->pan_geometry.x = nx;
								on++;
							} else if (x > -1) {
								layer->pan_geometry.x = x;
								on++;
							}
							
							if (y_val && strrchr(y_val, 'i')) {
								int ny = (int)layer->pan_geometry.y + y;

								if (ny < 0) ny = 0;
								if (ny + layer->pan_geometry.h > layer->img->d_h) ny = layer->img->d_h - layer->pan_geometry.h;
								
								layer->pan_geometry.y = ny;
								on++;
							} else if (y > -1) {
								layer->pan_geometry.y = y;
								on++;
							}	
							

							if (on) {
								layer->cam_opts.manual_pan = 1;
								layer->cam_opts.autopan = 0;
								stream->write_function(stream, "+OK PAN %d,%d\n", layer->pan_geometry.x, layer->pan_geometry.y);
							}
						}

						
					} else if (!strcasecmp(argv[x], "zoom")) {
						if (str_arg && !isfalse) {
							char *array[4] = {0};
							int iray[4] = {0};
							int ac;
							
							if ((ac = switch_split(str_arg, ':', array)) >= 3) {
								int i;

								for (i = 0; i < ac; i++) {
									int tmp = atoi(array[i]);

									if (tmp < 0) break;
									iray[i] = tmp;
								}

								if (i == ac) {
									layer->cam_opts.manual_zoom = 1;
									layer->cam_opts.autozoom = 0;

									layer->zoom_geometry.x = iray[0];
									layer->zoom_geometry.y = iray[1];
									layer->zoom_geometry.w = iray[2];
									if (iray[3]) {
										layer->zoom_geometry.h = iray[3];
									} else {
										layer->zoom_geometry.h = iray[2];
									}
									
									layer->crop_x = iray[0];
									layer->crop_y = iray[1];
									layer->crop_w = iray[2];
									layer->crop_h = iray[2];
									layer->pan_geometry = layer->zoom_geometry;
								} else {
									ok = 0;
								}
							}
						} else {
							layer->zoom_geometry.x = 0;
							layer->zoom_geometry.y = 0;
							layer->zoom_geometry.w = 0;
							layer->zoom_geometry.h = 0;
							layer->cam_opts.manual_zoom = 0;
						}
					} else {
						stream->write_function(stream, "-ERR invalid option [%s]\n", argv[x]);
					}
				}
			}
			switch_mutex_unlock(canvas->mutex);
			switch_mutex_unlock(conference->canvas_mutex);

		}
	}

	if (ok) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR invalid args\n");
	}


	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_stop(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint8_t current = 0, all = 0, async = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc > 2) {
		current = strcasecmp(argv[2], "current") ? 0 : 1;
		all = strcasecmp(argv[2], "all") ? 0 : 1;
		async = strcasecmp(argv[2], "async") ? 0 : 1;
	} else {
		all = 1;
	}

	if (!(current || all || async))
		return SWITCH_STATUS_GENERR;

	if (argc == 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member;

		if ((member = conference_member_get(conference, id))) {
			uint32_t stopped = conference_member_stop_file(member, async ? FILE_STOP_ASYNC : current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
			stream->write_function(stream, "+OK Stopped %u files.\n", stopped);
			switch_thread_rwlock_unlock(member->rwlock);
		} else {
			stream->write_function(stream, "-ERR Member: %u not found.\n", id);
		}
	} else {
		uint32_t stopped = conference_file_stop(conference, async ? FILE_STOP_ASYNC : current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
		stream->write_function(stream, "+OK Stopped %u files.\n", stopped);
	}
	return SWITCH_STATUS_SUCCESS;
}

void _conference_api_sub_relate_show_member_relationships(conference_obj_t *conference, switch_stream_handle_t *stream, uint32_t member_id)
{
	conference_member_t *member;
	for (member = conference->members; member; member = member->next) {
		conference_relationship_t *rel;

		if (member_id > 0 && member->id != member_id) continue;

		for (rel = member->relationships; rel; rel = rel->next) {
			stream->write_function(stream, "%d -> %d %s%s%s\n", member->id, rel->id,
								   (rel->flags & RFLAG_CAN_SPEAK) ? "SPEAK " : "NOSPEAK ",
								   (rel->flags & RFLAG_CAN_HEAR) ? "HEAR " : "NOHEAR ",
								   (rel->flags & RFLAG_CAN_SEND_VIDEO) ? "SENDVIDEO " : "NOSENDVIDEO ");
		}
	}
}

void _conference_api_sub_relate_clear_member_relationship(conference_obj_t *conference, switch_stream_handle_t *stream, uint32_t id, uint32_t oid)
{
	conference_member_t *member = NULL, *other_member = NULL;
	if ((member = conference_member_get(conference, id))) {
		conference_member_del_relationship(member, oid);
		other_member = conference_member_get(conference, oid);

		if (other_member) {
			if (conference_utils_member_test_flag(other_member, MFLAG_RECEIVING_VIDEO)) {
				conference_utils_member_clear_flag(other_member, MFLAG_RECEIVING_VIDEO);
				if (conference->floor_holder) {
					conference_member_t *omember = NULL;
		
					if ((omember = conference_member_get(member->conference, conference->floor_holder))) {
						switch_core_session_request_video_refresh(omember->session);
						switch_thread_rwlock_unlock(omember->rwlock);
					}
				}
			}
			switch_thread_rwlock_unlock(other_member->rwlock);
		}

		stream->write_function(stream, "+OK relationship %u->%u cleared.\n", id, oid);
		switch_thread_rwlock_unlock(member->rwlock);
	} else {
		stream->write_function(stream, "-ERR relationship %u->%u not found.\n", id, oid);
	}
}

void _conference_api_sub_relate_set_member_relationship(conference_obj_t *conference, switch_stream_handle_t *stream, uint32_t id, uint32_t oid, uint8_t nospeak, uint8_t nohear, uint8_t sendvideo, char *action)
{

	conference_member_t *member = NULL, *other_member = NULL;

	if ((member = conference_member_get(conference, id))) {
		other_member = conference_member_get(conference, oid);
	}

	if (member && other_member) {
		conference_relationship_t *rel = NULL;

		if (sendvideo && conference_utils_member_test_flag(other_member, MFLAG_RECEIVING_VIDEO) && (! (nospeak || nohear))) {
			stream->write_function(stream, "-ERR member %d already receiving video", oid);
			goto skip;
		}

		if ((rel = conference_member_get_relationship(member, other_member))) {
			rel->flags = 0;
		} else {
			rel = conference_member_add_relationship(member, oid);
		}

		if (rel) {
			switch_set_flag(rel, RFLAG_CAN_SPEAK | RFLAG_CAN_HEAR);
			if (nospeak) {
				switch_clear_flag(rel, RFLAG_CAN_SPEAK);
				conference_utils_member_clear_flag_locked(member, MFLAG_TALKING);
			}
			if (nohear) {
				switch_clear_flag(rel, RFLAG_CAN_HEAR);
			}
			if (sendvideo) {
				switch_set_flag(rel, RFLAG_CAN_SEND_VIDEO);
				conference_utils_member_set_flag(other_member, MFLAG_RECEIVING_VIDEO);
				switch_core_session_request_video_refresh(member->session);
			}

			stream->write_function(stream, "+OK %u->%u %s set\n", id, oid, action);
		} else {
			stream->write_function(stream, "-ERR error!\n");
		}
	} else {
		stream->write_function(stream, "-ERR relationship %u->%u not found.\n", id, oid);
	}

skip:
	if (member) {
		switch_thread_rwlock_unlock(member->rwlock);
	}

	if (other_member) {
		switch_thread_rwlock_unlock(other_member->rwlock);
	}
}

switch_status_t conference_api_sub_relate(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint8_t nospeak = 0, nohear = 0, sendvideo = 0, clear = 0;
	int members = 0;
	int other_members = 0;
	char *members_array[100] = { 0 };
	char *other_members_array[100] = { 0 };
	char *lbuf_members = NULL, *lbuf_other_members = NULL, *action = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 3) {
		switch_mutex_lock(conference->mutex);

		if (conference->relationship_total) {
			if (argc == 3) {
				char *lbuf = NULL;
				lbuf = strdup(argv[2]);
				members = switch_separate_string(lbuf, ',', members_array, (sizeof(members_array) / sizeof(members_array[0])));
				if (members) {
					int i;
					uint32_t member_id;
					for (i = 0; i < members && members_array[i]; i++) {
						member_id = atoi(members_array[i]);
						_conference_api_sub_relate_show_member_relationships(conference, stream, member_id);
					}
				}
				switch_safe_free(lbuf);
			}

		} else {
			stream->write_function(stream, "+OK No relationships\n");
		}
		switch_mutex_unlock(conference->mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	if (argc <= 4)
		return SWITCH_STATUS_GENERR;

	nospeak = strstr(argv[4], "nospeak") ? 1 : 0;
	nohear = strstr(argv[4], "nohear") ? 1 : 0;
	sendvideo = strstr(argv[4], "sendvideo") ? 1 : 0;

	if (!strcasecmp(argv[4], "clear")) {
		clear = 1;
	}

	if (!(clear || nospeak || nohear || sendvideo)) {
		return SWITCH_STATUS_GENERR;
	}

	lbuf_members = strdup(argv[2]);
	lbuf_other_members = strdup(argv[3]);
	action = strdup(argv[4]);
	members = switch_separate_string(lbuf_members, ',', members_array, (sizeof(members_array) / sizeof(members_array[0])));
	other_members = switch_separate_string(lbuf_other_members, ',', other_members_array, (sizeof(other_members_array) / sizeof(other_members_array[0])));
	if (members && other_members) {
		int i, i2;
		uint32_t member_id, other_member_id;
		for (i = 0; i < members && members_array[i]; i++) {
			member_id = atoi(members_array[i]);
			for (i2 = 0; i2 < other_members && other_members_array[i2]; i2++) {
				other_member_id = atoi(other_members_array[i2]);
				if (clear) {
					_conference_api_sub_relate_clear_member_relationship(conference, stream, member_id, other_member_id);
				}
				if (nospeak || nohear || sendvideo) {
					_conference_api_sub_relate_set_member_relationship(conference, stream, member_id, other_member_id, nospeak, nohear, sendvideo, action);
				}
			}
		}
	}
	switch_safe_free(lbuf_members);
	switch_safe_free(lbuf_other_members);
	switch_safe_free(action);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_lock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (conference->is_locked_sound) {
		conference_file_play(conference, conference->is_locked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
	}

	conference_utils_set_flag_locked(conference, CFLAG_LOCKED);
	stream->write_function(stream, "+OK %s locked\n", argv[0]);
	if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "lock");
		switch_event_fire(&event);
	}

	return 0;
}

switch_status_t conference_api_sub_unlock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (conference->is_unlocked_sound) {
		conference_file_play(conference, conference->is_unlocked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
	}

	conference_utils_clear_flag_locked(conference, CFLAG_LOCKED);
	stream->write_function(stream, "+OK %s unlocked\n", argv[0]);
	if (test_eflag(conference, EFLAG_UNLOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unlock");
		switch_event_fire(&event);
	}

	return 0;
}

switch_status_t conference_api_sub_exit_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "-ERR Not enough args\n");
		return SWITCH_STATUS_GENERR;
	}

	if ( !strcasecmp(argv[2], "on") ) {
		conference_utils_set_flag_locked(conference, CFLAG_EXIT_SOUND);
		stream->write_function(stream, "+OK %s exit sounds on (%s)\n", argv[0], conference->exit_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sounds-on");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "off") || !strcasecmp(argv[2], "none") ) {
		conference_utils_clear_flag_locked(conference, CFLAG_EXIT_SOUND);
		stream->write_function(stream, "+OK %s exit sounds off (%s)\n", argv[0], conference->exit_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sounds-off");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "file") ) {
		if (! argv[3]) {
			stream->write_function(stream, "-ERR No filename specified\n");
		} else {
			/* TODO: if possible, verify file exists before setting it */
			stream->write_function(stream,"+OK Old exit sound: [%s]\n", conference->exit_sound);
			conference->exit_sound = switch_core_strdup(conference->pool, argv[3]);
			stream->write_function(stream, "+OK %s exit sound file set to %s\n", argv[0], conference->exit_sound);
			if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_event_add_data(conference, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sound-file-changed");
				switch_event_fire(&event);
			}
		}
	} else {
		stream->write_function(stream, "-ERR Bad args\n");
		return SWITCH_STATUS_GENERR;
	}

	return 0;
}


switch_status_t conference_api_sub_enter_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "-ERR Not enough args\n");
		return SWITCH_STATUS_GENERR;
	}

	if ( !strcasecmp(argv[2], "on") ) {
		conference_utils_set_flag_locked(conference, CFLAG_ENTER_SOUND);
		stream->write_function(stream, "+OK %s enter sounds on (%s)\n", argv[0], conference->enter_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sounds-on");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "off") || !strcasecmp(argv[2], "none") ) {
		conference_utils_clear_flag_locked(conference, CFLAG_ENTER_SOUND);
		stream->write_function(stream, "+OK %s enter sounds off (%s)\n", argv[0], conference->enter_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_event_add_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sounds-off");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "file") ) {
		if (! argv[3]) {
			stream->write_function(stream, "-ERR No filename specified\n");
		} else {
			/* TODO: verify file exists before setting it */
			conference->enter_sound = switch_core_strdup(conference->pool, argv[3]);
			stream->write_function(stream, "+OK %s enter sound file set to %s\n", argv[0], conference->enter_sound);
			if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_event_add_data(conference, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sound-file-changed");
				switch_event_fire(&event);
			}
		}
	} else {
		stream->write_function(stream, "-ERR Bad args\n");
		return SWITCH_STATUS_GENERR;
	}

	return 0;
}


switch_status_t conference_api_sub_dial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_call_cause_t cause;
	char *tmp;

	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "-ERR Bad Args\n");
		return SWITCH_STATUS_GENERR;
	}

	if (conference && argv[2] && strstr(argv[2], "vlc/")) {
		tmp = switch_core_sprintf(conference->pool, "{vlc_rate=%d,vlc_channels=%d,vlc_interval=%d}%s",
								  conference->rate, conference->channels, conference->interval, argv[2]);
		argv[2] = tmp;
	}

	if (conference) {
		conference_outcall(conference, NULL, NULL, argv[2], 60, NULL, argv[4], argv[3], NULL, &cause, NULL, NULL, NULL);
	} else {
		conference_outcall(NULL, argv[0], NULL, argv[2], 60, NULL, argv[4], argv[3], NULL, &cause, NULL, NULL, NULL);
	}
	stream->write_function(stream, "+OK Call Requested: result: [%s]\n", switch_channel_cause2str(cause));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_bgdial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "-ERR Bad Args\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if (conference) {
		conference_outcall_bg(conference, NULL, NULL, argv[2], 60, NULL, argv[4], argv[3], uuid_str, NULL, NULL, NULL);
	} else {
		conference_outcall_bg(NULL, argv[0], NULL, argv[2], 60, NULL, argv[4], argv[3], uuid_str, NULL, NULL, NULL);
	}

	stream->write_function(stream, "+OK Job-UUID: %s\n", uuid_str);

	return SWITCH_STATUS_SUCCESS;
}



switch_status_t conference_api_sub_transfer(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_status_t ret_status = SWITCH_STATUS_SUCCESS;
	char *conference_name = NULL, *profile_name;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc > 3 && !zstr(argv[2])) {
		int x;

		conference_name = strdup(argv[2]);
		switch_assert(conference_name);

		if ((profile_name = strchr(conference_name, '@'))) {
			*profile_name++ = '\0';
		} else {
			profile_name = "default";
		}

		for (x = 3; x < argc; x++) {
			conference_member_t *member = NULL;
			uint32_t id = atoi(argv[x]);
			switch_channel_t *channel;
			switch_event_t *event;
			char *xdest = NULL;

			if (!id || !(member = conference_member_get(conference, id))) {
				stream->write_function(stream, "-ERR No Member %u in conference %s.\n", id, conference->name);
				continue;
			}

			channel = switch_core_session_get_channel(member->session);
			xdest = switch_core_session_sprintf(member->session, "conference:%s@%s", conference_name, profile_name);
			switch_ivr_session_transfer(member->session, xdest, "inline", NULL);

			switch_channel_set_variable(channel, "last_transfered_conference", conference_name);

			stream->write_function(stream, "+OK Member '%d' sent to conference %s.\n", member->id, argv[2]);

			/* tell them what happened */
			if (test_eflag(conference, EFLAG_TRANSFER) &&
				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_member_add_event_data(member, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-Conference-Name", conference->name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-Conference-Name", argv[2]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "transfer");
				switch_event_fire(&event);
			}

			switch_thread_rwlock_unlock(member->rwlock);
		}
	} else {
		ret_status = SWITCH_STATUS_GENERR;
	}

	switch_safe_free(conference_name);

	return ret_status;
}

switch_status_t conference_api_sub_check_record(conference_obj_t *conference, switch_stream_handle_t *stream, int arc, char **argv)
{
	conference_record_t *rec;
	int x = 0;

	switch_mutex_lock(conference->flag_mutex);
	for (rec = conference->rec_node_head; rec; rec = rec->next) {
		stream->write_function(stream, "+OK Record file %s%s%s\n", rec->path, rec->autorec ? " " : "", rec->autorec ? "(Auto)" : "");
		x++;
	}

	if (!x) {
		stream->write_function(stream, "-ERR Conference is not being recorded.\n");
	}
	switch_mutex_unlock(conference->flag_mutex);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_record(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int id = 0;
	conference_record_t *rec;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2) {
		return SWITCH_STATUS_GENERR;
	}

	if (argv[3]) {

		id = atoi(argv[3]);

		if (id < 1 || id > MAX_CANVASES+1) {
			id = -1;
		}

		if (id < 1) {
			stream->write_function(stream, "-ERR Invalid canvas\n");
		}

	}

	if (id == 0 && conference->canvases[0]) id = 1;

	if (id > conference->canvas_count + 1) {
		id = 1;
	}


	switch_mutex_lock(conference->flag_mutex);
	for (rec = conference->rec_node_head; rec; rec = rec->next) {
		char *path_a, *path_b;
		
		if ((path_a = strrchr(rec->path, '}'))) {
			while(*path_a == ' ' || *path_a == '}') path_a++;
		} else {
			path_a = rec->path;
		}

		if ((path_b = strrchr(argv[2], '}'))) {
			while(*path_b == ' ' || *path_b == '}') path_b++;
		} else {
			path_b = argv[2];
		}
		
		if (!strcmp(path_a, path_b)) {
			stream->write_function(stream, "-ERR file [%s] is already being used for recording.\n", rec->path);
			switch_mutex_unlock(conference->flag_mutex);
			return SWITCH_STATUS_SUCCESS;
		}
	}
	switch_mutex_unlock(conference->flag_mutex);


	if (id > 0) {
		stream->write_function(stream, "+OK Record file %s canvas %d\n", argv[2], id);
	} else {
		stream->write_function(stream, "+OK Record file %s\n", argv[2]);
	}

	conference->record_filename = switch_core_strdup(conference->pool, argv[2]);
	conference->record_count++;
	conference_record_launch_thread(conference, argv[2], id - 1, SWITCH_FALSE);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_norecord(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int all, before = conference->record_count, ttl = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2)
		return SWITCH_STATUS_GENERR;

	all = (strcasecmp(argv[2], "all") == 0);

	if (!conference_record_stop(conference, stream, all ? NULL : argv[2]) && !all) {
		stream->write_function(stream, "-ERR non-existant recording '%s'\n", argv[2]);
	}

	ttl = before - conference->record_count;
	stream->write_function(stream, "+OK Stopped recording %d file%s\n", ttl, ttl == 1 ? "" : "s");

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_pauserec(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;
	recording_action_type_t action;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2)
		return SWITCH_STATUS_GENERR;

	if (strcasecmp(argv[1], "pause") == 0) {
		action = REC_ACTION_PAUSE;
	} else if (strcasecmp(argv[1], "resume") == 0) {
		action = REC_ACTION_RESUME;
	} else {
		return SWITCH_STATUS_GENERR;
	}
	stream->write_function(stream, "+OK %s recording file %s\n",
						   action == REC_ACTION_PAUSE ? "Pause" : "Resume", argv[2]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,	"%s recording file %s\n",
					  action == REC_ACTION_PAUSE ? "Pause" : "Resume", argv[2]);

	if (!conference_record_action(conference, argv[2], action)) {
		stream->write_function(stream, "-ERR non-existant recording '%s'\n", argv[2]);
	} else {
		if (test_eflag(conference, EFLAG_RECORD) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS)
			{
				conference_event_add_data(conference, event);
				if (action == REC_ACTION_PAUSE) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "pause-recording");
				} else {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "resume-recording");
				}
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", argv[2]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Other-Recordings", conference->record_count ? "true" : "false");
				switch_event_fire(&event);
			}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_recording(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc > 2 && argc <= 3) {
		if (strcasecmp(argv[2], "stop") == 0 || strcasecmp(argv[2], "check") == 0) {
			argv[3] = "all";
			argc++;
		}
	}

	if (argc <= 3) {
		/* It means that old syntax is used */
		return conference_api_sub_record(conference,stream,argc,argv);
	} else {
		/* for new syntax call existing functions with fixed parameter list */
		if (strcasecmp(argv[2], "start") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			argv[3] = argv[4];
			return conference_api_sub_record(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "stop") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conference_api_sub_norecord(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "check") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conference_api_sub_check_record(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "pause") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conference_api_sub_pauserec(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "resume") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conference_api_sub_pauserec(conference,stream,4,argv);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}
}

switch_status_t conference_api_sub_file_vol(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc >= 1) {
		conference_file_node_t *fnode;
		int vol = 0;
		int ok = 0;

		if (argc < 3) {
			stream->write_function(stream, "-ERR missing args\n");
			return SWITCH_STATUS_GENERR;
		}

		switch_mutex_lock(conference->mutex);

		fnode = conference->fnode;

		vol = atoi(argv[2]);

		if (argc > 3) {
			if (strcasecmp(argv[3], "async")) {
				fnode = conference->async_fnode;
			}
		}

		if (fnode && fnode->type == NODE_TYPE_FILE) {
			fnode->fh.vol = vol;
			ok = 1;
		}
		switch_mutex_unlock(conference->mutex);


		if (ok) {
			stream->write_function(stream, "+OK volume changed\n");
			return SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "-ERR File not playing\n");
			return SWITCH_STATUS_GENERR;
		}


	} else {
		stream->write_function(stream, "-ERR Invalid parameters:\n");
		return SWITCH_STATUS_GENERR;
	}
}

switch_status_t conference_api_sub_pin(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if ((argc == 4) && (!strcmp(argv[2], "mod"))) {
		conference->mpin = switch_core_strdup(conference->pool, argv[3]);
		stream->write_function(stream, "+OK Moderator Pin for conference %s set: %s\n", argv[0], conference->mpin);
		return SWITCH_STATUS_SUCCESS;
	} else if ((argc == 3) && (!strcmp(argv[1], "pin"))) {
		conference->pin = switch_core_strdup(conference->pool, argv[2]);
		stream->write_function(stream, "+OK Pin for conference %s set: %s\n", argv[0], conference->pin);
		return SWITCH_STATUS_SUCCESS;
	} else if (argc == 2 && (!strcmp(argv[1], "nopin"))) {
		conference->pin = NULL;
		stream->write_function(stream, "+OK Pin for conference %s deleted\n", argv[0]);
		return SWITCH_STATUS_SUCCESS;
	} else {
		stream->write_function(stream, "-ERR Invalid parameters:\n");
		return SWITCH_STATUS_GENERR;
	}
}

switch_status_t conference_api_sub_get(conference_obj_t *conference,
									   switch_stream_handle_t *stream, int argc, char **argv) {
	int ret_status = SWITCH_STATUS_GENERR;

	if (argc != 3) {
		ret_status = SWITCH_STATUS_FALSE;
	} else {
		ret_status = SWITCH_STATUS_SUCCESS;
		if (strcasecmp(argv[2], "run_time") == 0) {
			stream->write_function(stream, "%ld",
								   switch_epoch_time_now(NULL) - conference->run_time);
		} else if (strcasecmp(argv[2], "count") == 0) {
			stream->write_function(stream, "%d",
								   conference->count);
		} else if (strcasecmp(argv[2], "count_ghosts") == 0) {
			stream->write_function(stream, "%d",
								   conference->count_ghosts);
		} else if (strcasecmp(argv[2], "max_members") == 0) {
			stream->write_function(stream, "%d",
								   conference->max_members);
		} else if (strcasecmp(argv[2], "rate") == 0) {
			stream->write_function(stream, "%d",
								   conference->rate);
		} else if (strcasecmp(argv[2], "profile_name") == 0) {
			stream->write_function(stream, "%s",
								   conference->profile_name);
		} else if (strcasecmp(argv[2], "sound_prefix") == 0) {
			stream->write_function(stream, "%s",
								   conference->sound_prefix);
		} else if (strcasecmp(argv[2], "caller_id_name") == 0) {
			stream->write_function(stream, "%s",
								   conference->caller_id_name);
		} else if (strcasecmp(argv[2], "caller_id_number") == 0) {
			stream->write_function(stream, "%s",
								   conference->caller_id_number);
		} else if (strcasecmp(argv[2], "is_locked") == 0) {
			stream->write_function(stream, "%s",
								   conference_utils_test_flag(conference, CFLAG_LOCKED) ? "locked" : "");
		} else if (strcasecmp(argv[2], "endconference_grace_time") == 0) {
			stream->write_function(stream, "%d",
								   conference->endconference_grace_time);
		} else if (strcasecmp(argv[2], "uuid") == 0) {
			stream->write_function(stream, "%s",
								   conference->uuid_str);
		} else if (strcasecmp(argv[2], "wait_mod") == 0) {
			stream->write_function(stream, "%s",
								   conference_utils_test_flag(conference, CFLAG_WAIT_MOD) ? "true" : "");
		} else {
			ret_status = SWITCH_STATUS_FALSE;
		}
	}

	return ret_status;
}

switch_status_t conference_api_sub_set(conference_obj_t *conference,
									   switch_stream_handle_t *stream, int argc, char **argv) {
	int ret_status = SWITCH_STATUS_GENERR;

	if (argc != 4 || zstr(argv[3])) {
		ret_status = SWITCH_STATUS_FALSE;
	} else {
		ret_status = SWITCH_STATUS_SUCCESS;
		if (strcasecmp(argv[2], "max_members") == 0) {
			int new_max = atoi(argv[3]);
			if (new_max >= 0) {
				stream->write_function(stream, "%d", conference->max_members);
				conference->max_members = new_max;
			} else {
				ret_status = SWITCH_STATUS_FALSE;
			}
		} else	if (strcasecmp(argv[2], "sound_prefix") == 0) {
			stream->write_function(stream, "%s",conference->sound_prefix);
			conference->sound_prefix = switch_core_strdup(conference->pool, argv[3]);
		} else	if (strcasecmp(argv[2], "caller_id_name") == 0) {
			stream->write_function(stream, "%s",conference->caller_id_name);
			conference->caller_id_name = switch_core_strdup(conference->pool, argv[3]);
		} else	if (strcasecmp(argv[2], "caller_id_number") == 0) {
			stream->write_function(stream, "%s",conference->caller_id_number);
			conference->caller_id_number = switch_core_strdup(conference->pool, argv[3]);
		} else if (strcasecmp(argv[2], "endconference_grace_time") == 0) {
			int new_gt = atoi(argv[3]);
			if (new_gt >= 0) {
				stream->write_function(stream, "%d", conference->endconference_grace_time);
				conference->endconference_grace_time = new_gt;
			} else {
				ret_status = SWITCH_STATUS_FALSE;
			}
		} else {
			ret_status = SWITCH_STATUS_FALSE;
		}
	}

	return ret_status;
}



switch_status_t conference_api_sub_xml_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int count = 0;
	switch_hash_index_t *hi;
	void *val;
	switch_xml_t x_conference, x_conferences;
	int off = 0;
	char *ebuf;

	x_conferences = switch_xml_new("conferences");
	switch_assert(x_conferences);

	if (conference == NULL) {
		switch_mutex_lock(conference_globals.hash_mutex);
		for (hi = switch_core_hash_first(conference_globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);
			conference = (conference_obj_t *) val;

			x_conference = switch_xml_add_child_d(x_conferences, "conference", off++);
			switch_assert(conference);

			count++;
			conference_xlist(conference, x_conference, off);

		}
		switch_mutex_unlock(conference_globals.hash_mutex);
	} else {
		x_conference = switch_xml_add_child_d(x_conferences, "conference", off++);
		switch_assert(conference);
		count++;
		conference_xlist(conference, x_conference, off);
	}


	ebuf = switch_xml_toxml(x_conferences, SWITCH_TRUE);

	stream->write_function(stream, "%s", ebuf);

	switch_xml_free(x_conferences);
	free(ebuf);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_sub_json_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_hash_index_t *hi;
	void *val;
	char *ebuf;
	cJSON *conferences = cJSON_CreateArray();
	switch_bool_t compact = SWITCH_FALSE;

	switch_assert(conferences);

	if (conference == NULL) {
		switch_mutex_lock(conference_globals.hash_mutex);
		for (hi = switch_core_hash_first(conference_globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);
			conference = (conference_obj_t *) val;
			conference_jlist(conference, conferences);
		}
		switch_mutex_unlock(conference_globals.hash_mutex);
		compact = (argc == 2 && !strcmp(argv[1], "compact"));
	} else {
		conference_jlist(conference, conferences);
		compact = (argc == 3 && !strcmp(argv[2], "compact"));
	}

	ebuf = compact ? cJSON_PrintUnformatted(conferences) : cJSON_Print(conferences);
	switch_assert(ebuf);
	stream->write_function(stream, "%s", ebuf);
	free(ebuf);
	
	cJSON_Delete(conferences);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_api_dispatch(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv, const char *cmdline, int argn)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t i, found = 0;
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	/* loop through the command table to find a match */
	for (i = 0; i < CONFFUNCAPISIZE && !found; i++) {
		if (strcasecmp(argv[argn], conference_api_sub_commands[i].pname) == 0) {
			found = 1;
			switch (conference_api_sub_commands[i].fntype) {

				/* commands that we've broken the command line into arguments for */
			case CONF_API_SUB_ARGS_SPLIT:
				{
					conference_api_args_cmd_t pfn = (conference_api_args_cmd_t) conference_api_sub_commands[i].pfnapicmd;

					if (pfn(conference, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
						/* command returned error, so show syntax usage */
						stream->write_function(stream, "%s %s", conference_api_sub_commands[i].pcommand, conference_api_sub_commands[i].psyntax);
					}
				}
				break;

				/* member specific command that can be iterated */
			case CONF_API_SUB_MEMBER_TARGET:
				{
					uint32_t id = 0;
					uint8_t all = 0;
					uint8_t last = 0;
					uint8_t non_mod = 0;

					if (argv[argn + 1]) {
						if (!(id = atoi(argv[argn + 1]))) {
							all = strcasecmp(argv[argn + 1], "all") ? 0 : 1;
							non_mod = strcasecmp(argv[argn + 1], "non_moderator") ? 0 : 1;
							last = strcasecmp(argv[argn + 1], "last") ? 0 : 1;
						}
					}

					if (all || non_mod) {
						conference_member_itterator(conference, stream, non_mod, (conference_api_member_cmd_t) conference_api_sub_commands[i].pfnapicmd, argv[argn + 2]);
					} else if (last) {
						conference_member_t *member = NULL;
						conference_member_t *last_member = NULL;

						switch_mutex_lock(conference->member_mutex);

						/* find last (oldest) member */
						member = conference->members;
						while (member != NULL) {
							if (last_member == NULL || member->id > last_member->id) {
								last_member = member;
							}
							member = member->next;
						}

						/* exec functio on last (oldest) member */
						if (last_member != NULL && last_member->session && !conference_utils_member_test_flag(last_member, MFLAG_NOCHANNEL)) {
							conference_api_member_cmd_t pfn = (conference_api_member_cmd_t) conference_api_sub_commands[i].pfnapicmd;
							pfn(last_member, stream, argv[argn + 2]);
						}

						switch_mutex_unlock(conference->member_mutex);
					} else if (id) {
						conference_api_member_cmd_t pfn = (conference_api_member_cmd_t) conference_api_sub_commands[i].pfnapicmd;
						conference_member_t *member = conference_member_get(conference, id);

						if (member != NULL) {
							pfn(member, stream, argv[argn + 2]);
							switch_thread_rwlock_unlock(member->rwlock);
						} else {
							stream->write_function(stream, "-ERR Non-Existant ID %u\n", id);
						}
					} else if (!zstr(argv[argn + 1]) && strchr(argv[argn + 1], '=')) {
						conference_api_member_cmd_t pfn = (conference_api_member_cmd_t) conference_api_sub_commands[i].pfnapicmd;
						conference_member_t *member;
						char *var, *val;

						var = strdup(argv[argn + 1]);
						switch_assert(var);

						if ((val = strchr(var, '='))) {
							*val++ = '\0';
						}

						member = conference_member_get_by_var(conference, var, val);

						if (member != NULL) {
							pfn(member, stream, argv[argn + 2]);
							switch_thread_rwlock_unlock(member->rwlock);
						} else {
							stream->write_function(stream, "-ERR Non-Existant member\n");
						}

						switch_safe_free(var);
					} else {
						stream->write_function(stream, "%s %s", conference_api_sub_commands[i].pcommand, conference_api_sub_commands[i].psyntax);
					}
				}
				break;

				/* commands that deals with all text after command */
			case CONF_API_SUB_ARGS_AS_ONE:
				{
					conference_api_text_cmd_t pfn = (conference_api_text_cmd_t) conference_api_sub_commands[i].pfnapicmd;
					char *start_text;
					const char *modified_cmdline = cmdline;
					const char *cmd = conference_api_sub_commands[i].pname;

					if (!zstr(modified_cmdline) && (start_text = strstr(modified_cmdline, cmd))) {
						modified_cmdline = start_text + strlen(cmd);
						while (modified_cmdline && (*modified_cmdline == ' ' || *modified_cmdline == '\t')) {
							modified_cmdline++;
						}
					}

					/* call the command handler */
					if (pfn(conference, stream, modified_cmdline) != SWITCH_STATUS_SUCCESS) {
						/* command returned error, so show syntax usage */
						stream->write_function(stream, "%s %s", conference_api_sub_commands[i].pcommand, conference_api_sub_commands[i].psyntax);
					}
				}
				break;
			}
		}
	}

	if (!found) {
		stream->write_function(stream, "-ERR Conference command '%s' not found.\n", argv[argn]);
	} else {
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
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
