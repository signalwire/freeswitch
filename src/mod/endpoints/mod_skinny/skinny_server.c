/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * skinny_server.c -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#include <switch.h>
#include "mod_skinny.h"
#include "skinny_protocol.h"
#include "skinny_tables.h"
#include "skinny_server.h"

uint32_t soft_key_template_default_textids[] = {
	SKINNY_TEXTID_REDIAL,
	SKINNY_TEXTID_NEWCALL,
	SKINNY_TEXTID_HOLD,
	SKINNY_TEXTID_TRANSFER,
	SKINNY_TEXTID_CFWDALL,
	SKINNY_TEXTID_CFWDBUSY,
	SKINNY_TEXTID_CFWDNOANSWER,
	SKINNY_TEXTID_BACKSPACE,
	SKINNY_TEXTID_ENDCALL,
	SKINNY_TEXTID_RESUME,
	SKINNY_TEXTID_ANSWER,
	SKINNY_TEXTID_INFO,
	SKINNY_TEXTID_CONF,
	SKINNY_TEXTID_PARK,
	SKINNY_TEXTID_JOIN,
	SKINNY_TEXTID_MEETME,
	SKINNY_TEXTID_CALLPICKUP,
	SKINNY_TEXTID_GRPCALLPICKUP,
	SKINNY_TEXTID_DND,
	SKINNY_TEXTID_IDIVERT
};

#define TEXT_ID_LEN 20

uint32_t soft_key_template_default_events[] = {
	SOFTKEY_REDIAL,
	SOFTKEY_NEWCALL,
	SOFTKEY_HOLD,
	SOFTKEY_TRANSFER,
	SOFTKEY_CFWDALL,
	SOFTKEY_CFWDBUSY,
	SOFTKEY_CFWDNOANSWER,
	SOFTKEY_BACKSPACE,
	SOFTKEY_ENDCALL,
	SOFTKEY_RESUME,
	SOFTKEY_ANSWER,
	SOFTKEY_INFO,
	SOFTKEY_CONF,
	SOFTKEY_PARK,
	SOFTKEY_JOIN,
	SOFTKEY_MEETMECONF,
	SOFTKEY_CALLPICKUP,
	SOFTKEY_GRPCALLPICKUP,
	SOFTKEY_DND,
	SOFTKEY_IDIVERT
};

/*****************************************************************************/
/* SESSION FUNCTIONS */
/*****************************************************************************/
switch_status_t skinny_create_incoming_session(listener_t *listener, uint32_t *line_instance_p, switch_core_session_t **session)
{
	uint32_t line_instance;
	switch_core_session_t *nsession;
	switch_channel_t *channel;
	private_t *tech_pvt;
	char name[128];
	char *sql;
	struct line_stat_res_message *button = NULL;

	line_instance = *line_instance_p;
	if((nsession = skinny_profile_find_session(listener->profile, listener, line_instance_p, 0))) {
		if(skinny_line_get_state(listener, *line_instance_p, 0) == SKINNY_OFF_HOOK) {
			/* Reuse existing session */
			*session = nsession;
			return SWITCH_STATUS_SUCCESS;
		}
		switch_core_session_rwunlock(nsession);
	}
	*line_instance_p = line_instance;
	if(*line_instance_p == 0) {
		*line_instance_p = 1;
	}

	skinny_hold_active_calls(listener);

	skinny_line_get(listener, *line_instance_p, &button);

	if (!button || !button->shortname) {
		skinny_log_l(listener, SWITCH_LOG_CRIT, "Line %d not found on device\n", *line_instance_p);
		goto error;
	}

	if (!(nsession = switch_core_session_request(skinny_get_endpoint_interface(),
					SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
		skinny_log_l_msg(listener, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		skinny_log_ls_msg(listener, nsession, SWITCH_LOG_CRIT, "Error Creating Session private object\n");
		goto error;
	}

	switch_core_session_add_stream(nsession, NULL);

	tech_init(tech_pvt, listener->profile, nsession);

	channel = switch_core_session_get_channel(nsession);

	snprintf(name, sizeof(name), "SKINNY/%s/%s:%d/%d", listener->profile->name, 
			listener->device_name, listener->device_instance, *line_instance_p);
	switch_channel_set_name(channel, name);

	if (switch_core_session_thread_launch(nsession) != SWITCH_STATUS_SUCCESS) {
		skinny_log_ls_msg(listener, nsession, SWITCH_LOG_CRIT, "Error Creating Session thread\n");
		goto error;
	}
	if (switch_core_session_read_lock(nsession) != SWITCH_STATUS_SUCCESS) {
		skinny_log_ls_msg(listener, nsession, SWITCH_LOG_CRIT, "Error Locking Session\n");
		goto error;
	}
	/* First create the caller profile in the patterns Dialplan */
	if (!(tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(nsession),
					NULL, listener->profile->patterns_dialplan, 
					button->displayname, button->shortname, 
					listener->remote_ip, NULL, NULL, NULL,
					"skinny" /* modname */,
					listener->profile->patterns_context, 
					"")) != 0) {
		skinny_log_ls_msg(listener, nsession, SWITCH_LOG_CRIT, "Error Creating Session caller profile\n");
		goto error;
	}

	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	if ((sql = switch_mprintf(
					"INSERT INTO skinny_active_lines "
					"(device_name, device_instance, line_instance, channel_uuid, call_id, call_state) "
					"SELECT device_name, device_instance, line_instance, '%s', %d, %d "
					"FROM skinny_lines "
					"WHERE value='%s'",
					switch_core_session_get_uuid(nsession), tech_pvt->call_id, SKINNY_ON_HOOK, button->shortname
				 ))) {
		skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
		switch_safe_free(sql);
	}
	skinny_session_set_variables(nsession, listener, *line_instance_p);

	send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, tech_pvt->call_id);
	send_set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	send_set_lamp(listener, SKINNY_BUTTON_LINE, *line_instance_p, SKINNY_LAMP_ON);
	skinny_line_set_state(listener, *line_instance_p, tech_pvt->call_id, SKINNY_OFF_HOOK);
	send_select_soft_keys(listener, *line_instance_p, tech_pvt->call_id, SKINNY_KEY_SET_OFF_HOOK, 0xffff);

	send_display_prompt_status_textid(listener, 0, SKINNY_TEXTID_ENTER_NUMBER, *line_instance_p, tech_pvt->call_id);

	send_activate_call_plane(listener, *line_instance_p);
	if (switch_channel_get_state(channel) == CS_NEW) {
		switch_channel_set_state(channel, CS_HIBERNATE);
	} else {
		skinny_log_ls_msg(listener, nsession, SWITCH_LOG_CRIT, 
			"Wow! this channel should be in CS_NEW state, but it is not!\n");
	}

	goto done;
error:
	if (nsession) {
		switch_core_session_destroy(&nsession);
	}

	listener->profile->ib_failed_calls++;
	return SWITCH_STATUS_FALSE;

done:
	*session = nsession;
	listener->profile->ib_calls++;
	return SWITCH_STATUS_SUCCESS;
}

skinny_action_t skinny_session_dest_match_pattern(switch_core_session_t *session, char **data)
{
	skinny_action_t action = SKINNY_ACTION_DROP;
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	/* this part of the code is similar to switch_core_standard_on_routing() */
	if (!zstr(tech_pvt->profile->patterns_dialplan)) {
		switch_dialplan_interface_t *dialplan_interface = NULL;
		switch_caller_extension_t *extension = NULL;
		char *expanded = NULL;
		char *dpstr = NULL;
		char *dp[25];
		int argc, x;

		if ((dpstr = switch_core_session_strdup(session, tech_pvt->profile->patterns_dialplan))) {
			expanded = switch_channel_expand_variables(channel, dpstr);
			argc = switch_separate_string(expanded, ',', dp, (sizeof(dp) / sizeof(dp[0])));
			for (x = 0; x < argc; x++) {
				char *dpname = dp[x];
				char *dparg = NULL;

				if (dpname) {
					if ((dparg = strchr(dpname, ':'))) {
						*dparg++ = '\0';
					}
				} else {
					continue;
				}
				if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
					continue;
				}

				extension = dialplan_interface->hunt_function(session, dparg, NULL);
				UNPROTECT_INTERFACE(dialplan_interface);

				if (extension) {
					goto found;
				}
			}
		}
found:
		while (extension && extension->current_application) {
			switch_caller_application_t *current_application = extension->current_application;

			extension->current_application = extension->current_application->next;

			if (!strcmp(current_application->application_name, "skinny-route") || !strcmp(current_application->application_name, "skinny-process")) {
				action = SKINNY_ACTION_PROCESS;
			} else if (!strcmp(current_application->application_name, "skinny-drop")) {
				action = SKINNY_ACTION_DROP;
			} else if (!strcmp(current_application->application_name, "skinny-wait")) {
				action = SKINNY_ACTION_WAIT;
				*data = switch_core_session_strdup(session, current_application->application_data);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
						"Unknown skinny dialplan application %s\n", current_application->application_name);
			}
		}
	}
	return action;
}


switch_status_t skinny_session_process_dest(switch_core_session_t *session, listener_t *listener, uint32_t line_instance, char *dest, char append_dest, uint32_t backspace)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	if (!dest) {
		if (strlen(tech_pvt->caller_profile->destination_number) == 0) {/* no digit yet */
			send_start_tone(listener, SKINNY_TONE_DIALTONE, 0, line_instance, tech_pvt->call_id);
		}
		if (backspace && strlen(tech_pvt->caller_profile->destination_number)) { /* backspace */
			tech_pvt->caller_profile->destination_number[strlen(tech_pvt->caller_profile->destination_number)-1] = '\0';
			if (strlen(tech_pvt->caller_profile->destination_number) == 0) {
				send_select_soft_keys(listener, line_instance, tech_pvt->call_id, SKINNY_KEY_SET_OFF_HOOK, 0xffff);
			}
			send_back_space_request(listener, line_instance, tech_pvt->call_id);
		}
		if (append_dest != '\0') {/* append digit */
			tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool,
					"%s%c", tech_pvt->caller_profile->destination_number, append_dest);
		}
		if (strlen(tech_pvt->caller_profile->destination_number) == 1) {/* first digit */
			if(!backspace) {
				send_stop_tone(listener, line_instance, tech_pvt->call_id);
			}
			send_select_soft_keys(listener, line_instance, tech_pvt->call_id,
					SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT, 0xffff);
		}
	} else {
		tech_pvt->caller_profile->destination_number = switch_core_strdup(tech_pvt->caller_profile->pool, dest);
		switch_set_flag_locked(tech_pvt, TFLAG_FORCE_ROUTE);
	}

	switch_channel_set_state(channel, CS_ROUTING);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_send_call_info(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt;
	switch_channel_t *channel;

	const char *caller_party_name;
	const char *caller_party_number;
	const char *called_party_name;
	const char *called_party_number;
	uint32_t call_type = 0;

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	switch_assert(tech_pvt->caller_profile != NULL);

	/* Calling party */
	if (zstr((caller_party_name = switch_channel_get_variable(channel, "effective_caller_id_name"))) &&
			zstr((caller_party_name = switch_channel_get_variable(channel, "caller_id_name"))) &&
			zstr((caller_party_name = switch_channel_get_variable_partner(channel, "effective_caller_id_name"))) &&
			zstr((caller_party_name = switch_channel_get_variable_partner(channel, "caller_id_name")))) {
		caller_party_name = SWITCH_DEFAULT_CLID_NAME;
	}
	if (zstr((caller_party_number = switch_channel_get_variable(channel, "effective_caller_id_number"))) &&
			zstr((caller_party_number = switch_channel_get_variable(channel, "caller_id_number"))) &&
			zstr((caller_party_number = switch_channel_get_variable_partner(channel, "effective_caller_id_number"))) &&
			zstr((caller_party_number = switch_channel_get_variable_partner(channel, "caller_id_number")))) {
		caller_party_number = SWITCH_DEFAULT_CLID_NUMBER;
	}
	/* Called party */
	if (zstr((called_party_name = switch_channel_get_variable(channel, "effective_callee_id_name"))) &&
			zstr((called_party_name = switch_channel_get_variable(channel, "callee_id_name"))) &&
			zstr((called_party_name = switch_channel_get_variable_partner(channel, "effective_callee_id_name"))) &&
			zstr((called_party_name = switch_channel_get_variable_partner(channel, "callee_id_name")))) {
		called_party_name = SWITCH_DEFAULT_CLID_NAME;
	}
	if (zstr((called_party_number = switch_channel_get_variable(channel, "effective_callee_id_number"))) &&
			zstr((called_party_number = switch_channel_get_variable(channel, "callee_id_number"))) &&
			zstr((called_party_number = switch_channel_get_variable_partner(channel, "effective_callee_id_number"))) &&
			zstr((called_party_number = switch_channel_get_variable_partner(channel, "callee_id_number"))) &&
			zstr((called_party_number = switch_channel_get_variable(channel, "destination_number")))) {
		called_party_number = SWITCH_DEFAULT_CLID_NUMBER;
	}
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		call_type = SKINNY_INBOUND_CALL;
	} else {
		call_type = SKINNY_OUTBOUND_CALL;
	}
	send_call_info(listener,
			caller_party_name, /* char calling_party_name[40], */
			caller_party_number, /* char calling_party[24], */
			called_party_name, /* char called_party_name[40], */
			called_party_number, /* char called_party[24], */
			line_instance, /* uint32_t line_instance, */
			tech_pvt->call_id, /* uint32_t call_id, */
			call_type, /* uint32_t call_type, */
			"", /* TODO char original_called_party_name[40], */
			"", /* TODO char original_called_party[24], */
			"", /* TODO char last_redirecting_party_name[40], */
			"", /* TODO char last_redirecting_party[24], */
			0, /* TODO uint32_t original_called_party_redirect_reason, */
			0, /* TODO uint32_t last_redirecting_reason, */
			"", /* TODO char calling_party_voice_mailbox[24], */
			"", /* TODO char called_party_voice_mailbox[24], */
			"", /* TODO char original_called_party_voice_mailbox[24], */
			"", /* TODO char last_redirecting_voice_mailbox[24], */
			1, /* uint32_t call_instance, */
			1, /* uint32_t call_security_status, */
			0 /* uint32_t party_pi_restriction_bits */
				);
	return SWITCH_STATUS_SUCCESS;
}

struct skinny_session_send_call_info_all_helper {
	private_t *tech_pvt;
};

int skinny_session_send_call_info_all_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	struct skinny_session_send_call_info_all_helper *helper = pArg;
	listener_t *listener = NULL;

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, 
			device_name, device_instance, &listener);
	if(listener) {
		skinny_session_send_call_info(helper->tech_pvt->session, listener, line_instance);
	}
	return 0;
}

switch_status_t skinny_session_send_call_info_all(switch_core_session_t *session)
{
	struct skinny_session_send_call_info_all_helper helper = {0};
	private_t *tech_pvt = switch_core_session_get_private(session);

	helper.tech_pvt = tech_pvt;
	return skinny_session_walk_lines(tech_pvt->profile,
			switch_core_session_get_uuid(tech_pvt->session), skinny_session_send_call_info_all_callback, &helper);
}

struct skinny_session_set_variables_helper {
	private_t *tech_pvt;
	switch_channel_t *channel;
	uint32_t count;
};

int skinny_session_set_variables_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	uint32_t position = atoi(argv[2]);
	uint32_t line_instance = atoi(argv[3]);
	char *label = argv[4];
	char *value = argv[5];
	char *caller_name = argv[6];
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	struct skinny_session_set_variables_helper *helper = pArg;
	char *tmp;

	helper->count++;
	switch_channel_set_variable_name_printf(helper->channel, device_name, "skinny_device_name_%d", helper->count);
	if ((tmp = switch_mprintf("%d", device_instance))) {
		switch_channel_set_variable_name_printf(helper->channel, tmp, "skinny_device_instance_%d", helper->count);
		switch_safe_free(tmp);
	}
	if ((tmp = switch_mprintf("%d", position))) {
		switch_channel_set_variable_name_printf(helper->channel, tmp, "skinny_line_position_%d", helper->count);
		switch_safe_free(tmp);
	}
	if ((tmp = switch_mprintf("%d", line_instance))) {
		switch_channel_set_variable_name_printf(helper->channel, tmp, "skinny_line_instance_%d", helper->count);
		switch_safe_free(tmp);
	}
	switch_channel_set_variable_name_printf(helper->channel, label, "skinny_line_label_%d", helper->count);
	switch_channel_set_variable_name_printf(helper->channel, value, "skinny_line_value_%d", helper->count);
	switch_channel_set_variable_name_printf(helper->channel, caller_name, "skinny_line_caller_name_%d", helper->count);

	return 0;
}

switch_status_t skinny_session_set_variables(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_status_t status;
	struct skinny_session_set_variables_helper helper = {0};

	helper.tech_pvt = switch_core_session_get_private(session);
	helper.channel = switch_core_session_get_channel(session);
	helper.count = 0;

	switch_channel_set_variable(helper.channel, "skinny_profile_name", helper.tech_pvt->profile->name);
	if (listener) {
		switch_channel_set_variable(helper.channel, "skinny_device_name", listener->device_name);
		switch_channel_set_variable_printf(helper.channel, "skinny_device_instance", "%d", listener->device_instance);
		switch_channel_set_variable_printf(helper.channel, "skinny_line_instance", "%d", line_instance);
	}
	status = skinny_session_walk_lines(helper.tech_pvt->profile,
			switch_core_session_get_uuid(helper.tech_pvt->session), skinny_session_set_variables_callback, &helper);

	switch_channel_set_variable_printf(helper.channel, "skinny_lines_count", "%d", helper.count);
	return status;
}

struct skinny_ring_lines_helper {
	private_t *tech_pvt;
	switch_core_session_t *remote_session;
	uint32_t lines_count;
};

int skinny_ring_lines_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_ring_lines_helper *helper = pArg;
	char *tmp;
	char *label;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	char *value = argv[5];
	char *caller_name = argv[6];
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	listener_t *listener = NULL;

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, 
			device_name, device_instance, &listener);
	if(listener && helper->tech_pvt->session && helper->remote_session) {
		switch_channel_t *channel = switch_core_session_get_channel(helper->tech_pvt->session);
		switch_channel_t *remchannel = switch_core_session_get_channel(helper->remote_session);
		switch_channel_set_state(channel, CS_ROUTING);
		helper->lines_count++;
		switch_channel_set_variable(channel, "effective_callee_id_number", value);
		switch_channel_set_variable(channel, "effective_callee_id_name", caller_name);

		skinny_log_l(listener, SWITCH_LOG_DEBUG, "Ring Lines Callback with Callee Number (%s), Caller Name (%s), Dest Number (%s)\n",
			value, caller_name, helper->tech_pvt->caller_profile->destination_number);

		if (helper->remote_session) {
			switch_core_session_message_t msg = { 0 };
			msg.message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
			msg.string_array_arg[0] = switch_core_session_strdup(helper->remote_session, caller_name);
			msg.string_array_arg[1] = switch_core_session_strdup(helper->remote_session, value);
			msg.from = __FILE__;

			if (switch_core_session_receive_message(helper->remote_session, &msg) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(helper->tech_pvt->session), SWITCH_LOG_WARNING, 
						"Unable to send SWITCH_MESSAGE_INDICATE_DISPLAY message to channel %s\n",
						switch_core_session_get_uuid(helper->remote_session));
			}
		}

		skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_RING_IN);
		send_select_soft_keys(listener, line_instance, helper->tech_pvt->call_id, SKINNY_KEY_SET_RING_IN, 0xffff);

		label = skinny_textid2raw(SKINNY_TEXTID_FROM);
		if ((tmp = switch_mprintf("%s%s", label, helper->tech_pvt->caller_profile->destination_number))) {
			send_display_prompt_status(listener, 0, tmp, line_instance, helper->tech_pvt->call_id);
			switch_safe_free(tmp);
		}
		switch_safe_free(label);

		if ((tmp = switch_mprintf("\005\000\000\000%s", helper->tech_pvt->caller_profile->destination_number))) {
			send_display_pri_notify(listener, 10 /* message_timeout */, 5 /* priority */, tmp);
			switch_safe_free(tmp);
		}
		skinny_session_send_call_info(helper->tech_pvt->session, listener, line_instance);
		send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_BLINK);
		send_set_ringer(listener, SKINNY_RING_INSIDE, SKINNY_RING_FOREVER, 0, helper->tech_pvt->call_id);
		switch_channel_ring_ready(remchannel);
	}
	return 0;
}

switch_call_cause_t skinny_ring_lines(private_t *tech_pvt, switch_core_session_t *remote_session)
{
	switch_status_t status;
	struct skinny_ring_lines_helper helper = {0};

	switch_assert(tech_pvt);
	switch_assert(tech_pvt->profile);
	switch_assert(tech_pvt->session);

	helper.tech_pvt = tech_pvt;
	helper.remote_session = remote_session;
	helper.lines_count = 0;

	status = skinny_session_walk_lines(tech_pvt->profile,
			switch_core_session_get_uuid(tech_pvt->session), skinny_ring_lines_callback, &helper);
	skinny_session_set_variables(tech_pvt->session, NULL, 0);

	if (status != SWITCH_STATUS_SUCCESS) {
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	} else if (helper.lines_count == 0) {
		return SWITCH_CAUSE_UNALLOCATED_NUMBER;
	} else {
		return SWITCH_CAUSE_SUCCESS;
	}
}

switch_status_t skinny_session_ring_out(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	tech_pvt = switch_core_session_get_private(session);

	send_start_tone(listener, SKINNY_TONE_ALERT, 0, line_instance, tech_pvt->call_id);
	skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_RING_OUT);
	send_select_soft_keys(listener, line_instance, tech_pvt->call_id, SKINNY_KEY_SET_RING_OUT, 0xffff);

	send_display_prompt_status_textid(listener, 0, SKINNY_TEXTID_RING_OUT, line_instance, tech_pvt->call_id);

	skinny_session_send_call_info(session, listener, line_instance);

	return SWITCH_STATUS_SUCCESS;
}


struct skinny_session_answer_helper {
	private_t *tech_pvt;
	listener_t *listener;
	uint32_t line_instance;
};

int skinny_session_answer_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_session_answer_helper *helper = pArg;
	listener_t *listener = NULL;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, device_name, device_instance, &listener);
	if(listener) {
		if(!strcmp(device_name, helper->listener->device_name) 
				&& (device_instance == helper->listener->device_instance)
				&& (line_instance == helper->line_instance)) {/* the answering line */
			/* nothing */
			skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Session Answer Callback - matched helper\n");
		} else {
			skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Session Answer Callback\n");

			send_define_current_time_date(listener);
			send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_ON);
			skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_IN_USE_REMOTELY);
			send_select_soft_keys(listener, line_instance, helper->tech_pvt->call_id, 10, 0x0002);

			send_display_prompt_status_textid(listener, 0, SKINNY_TEXTID_IN_USE_REMOTE, line_instance, helper->tech_pvt->call_id);

			send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, helper->tech_pvt->call_id);
		}
	}
	return 0;
}

switch_status_t skinny_session_answer(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	struct skinny_session_answer_helper helper = {0};
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, tech_pvt->call_id);
	send_set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_ON);
	skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_OFF_HOOK);
	send_activate_call_plane(listener, line_instance);

	helper.tech_pvt = tech_pvt;
	helper.listener = listener;
	helper.line_instance = line_instance;

	skinny_session_walk_lines(tech_pvt->profile, switch_core_session_get_uuid(session), skinny_session_answer_callback, &helper);

	if (switch_channel_get_state(channel) == CS_INIT) {
		switch_channel_set_state(channel, CS_ROUTING);
	}
	skinny_session_start_media(session, listener, line_instance);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_start_media(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	if (!switch_channel_test_flag(channel, CF_EARLY_MEDIA)) {
		send_stop_tone(listener, line_instance, tech_pvt->call_id);
		send_open_receive_channel(listener,
				tech_pvt->call_id, /* uint32_t conference_id, */
				tech_pvt->call_id, /* uint32_t pass_thru_party_id, */
				20, /* uint32_t ms_per_packet, */
				SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
				0, /* uint32_t echo_cancel_type, */
				0, /* uint32_t g723_bitrate, */
				0, /* uint32_t conference_id2, */
				0 /* uint32_t reserved[10] */
				);
	}
	if (!switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
		skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_CONNECTED);
		send_select_soft_keys(listener, line_instance, tech_pvt->call_id,
				SKINNY_KEY_SET_CONNECTED, 0xffff);

		send_display_prompt_status_textid(listener, 0, SKINNY_TEXTID_CONNECTED, line_instance, tech_pvt->call_id);
	}
	skinny_session_send_call_info(session, listener, line_instance);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_hold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	tech_pvt = switch_core_session_get_private(session);

	skinny_session_stop_media(session, listener, line_instance);
	switch_ivr_hold(session, NULL, 1);

	send_define_current_time_date(listener);
	send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_WINK);
	skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_HOLD);
	send_select_soft_keys(listener, line_instance, tech_pvt->call_id, SKINNY_KEY_SET_ON_HOLD, 0xffff);

	send_display_prompt_status_textid(listener, 0, SKINNY_TEXTID_HOLD, line_instance, tech_pvt->call_id);

	skinny_session_send_call_info(tech_pvt->session, listener, line_instance);
	send_set_speaker_mode(listener, SKINNY_SPEAKER_OFF);
	send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, tech_pvt->call_id);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_unhold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	tech_pvt = switch_core_session_get_private(session);

	skinny_hold_active_calls(listener);
	send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, tech_pvt->call_id);
	send_set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	send_select_soft_keys(listener, line_instance, tech_pvt->call_id, SKINNY_KEY_SET_RING_OUT, 0xffff);
	skinny_session_start_media(session, listener, line_instance);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_transfer(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	private_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	const char *remote_uuid = NULL;
	switch_core_session_t *session2 = NULL;
	private_t *tech_pvt2 = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	tech_pvt = switch_core_session_get_private(session);
	channel = switch_core_session_get_channel(session);
	remote_uuid = switch_channel_get_partner_uuid(channel);

	if (tech_pvt->transfer_from_call_id) {
		if((session2 = skinny_profile_find_session(listener->profile, listener, &line_instance, tech_pvt->transfer_from_call_id))) {
			switch_channel_t *channel2 = switch_core_session_get_channel(session2);
			const char *remote_uuid2 = switch_channel_get_partner_uuid(channel2);
			if (switch_ivr_uuid_bridge(remote_uuid, remote_uuid2) == SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				switch_channel_hangup(channel2, SWITCH_CAUSE_NORMAL_CLEARING);
			} else {
				/* TODO: How to inform the user that the bridge is not possible? */
			}
			switch_core_session_rwunlock(session2);
		}
	} else {
		if(remote_uuid) {
			/* TODO CallSelectStat */
			status = skinny_create_incoming_session(listener, &line_instance, &session2);
			tech_pvt2 = switch_core_session_get_private(session2);
			tech_pvt2->transfer_from_call_id = tech_pvt->call_id;
			tech_pvt->transfer_to_call_id = tech_pvt2->call_id;
			skinny_session_process_dest(session2, listener, line_instance, NULL, '\0', 0);
			switch_core_session_rwunlock(session2);
		} else {
			/* TODO: How to inform the user that the bridge is not possible? */
		}
	}
	return status;
}

switch_status_t skinny_session_stop_media(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);

	tech_pvt = switch_core_session_get_private(session);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);

	send_close_receive_channel(listener,
			tech_pvt->call_id, /* uint32_t conference_id, */
			tech_pvt->party_id, /* uint32_t pass_thru_party_id, */
			tech_pvt->call_id /* uint32_t conference_id2, */
			);
	send_stop_media_transmission(listener,
			tech_pvt->call_id, /* uint32_t conference_id, */
			tech_pvt->party_id, /* uint32_t pass_thru_party_id, */
			tech_pvt->call_id /* uint32_t conference_id2, */
			);

	return SWITCH_STATUS_SUCCESS;
}

struct skinny_hold_active_calls_helper {
	listener_t *listener;
};

int skinny_hold_active_calls_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_hold_active_calls_helper *helper = pArg;
	switch_core_session_t *session;

	/* char *device_name = argv[0]; */
	/* uint32_t device_instance = atoi(argv[1]); */
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	uint32_t call_id = atoi(argv[15]);
	/* uint32_t call_state = atoi(argv[16]); */

	session = skinny_profile_find_session(helper->listener->profile, helper->listener, &line_instance, call_id);

	if(session) {
		skinny_session_hold_line(session, helper->listener, line_instance);
		switch_core_session_rwunlock(session);
	}

	return 0;
}

switch_status_t skinny_hold_active_calls(listener_t *listener)
{
	struct skinny_hold_active_calls_helper helper = {0};
	char *sql;

	helper.listener = listener;

	if ((sql = switch_mprintf(
					"SELECT skinny_lines.*, channel_uuid, call_id, call_state "
					"FROM skinny_active_lines "
					"INNER JOIN skinny_lines "
					"ON skinny_active_lines.device_name = skinny_lines.device_name "
					"AND skinny_active_lines.device_instance = skinny_lines.device_instance "
					"AND skinny_active_lines.line_instance = skinny_lines.line_instance "
					"WHERE skinny_lines.device_name='%s' AND skinny_lines.device_instance=%d AND (call_state=%d OR call_state=%d)",
					listener->device_name, listener->device_instance, SKINNY_PROCEED, SKINNY_CONNECTED))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_hold_active_calls_callback, &helper);
		switch_safe_free(sql);
	}

	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************/
/* SKINNY MESSAGE HANDLERS */
/*****************************************************************************/
switch_status_t skinny_handle_keep_alive_message(listener_t *listener, skinny_message_t *request)
{
	keepalive_listener(listener, NULL);

	send_keep_alive_ack(listener);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_register(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	skinny_profile_t *profile;
	switch_event_t *event = NULL;
	switch_event_t *params = NULL;
	switch_xml_t xroot, xdomain, xgroup, xuser, xskinny, xparams, xparam, xbuttons, xbutton;
	listener_t *listener2 = NULL;
	char *sql;
	assert(listener->profile);
	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.reg));

	if (!zstr(listener->device_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"A device is already registred on this listener.\n");
		send_register_reject(listener, "A device is already registred on this listener");
		return SWITCH_STATUS_FALSE;
	}

	/* Check directory */
	skinny_device_event(listener, &params, SWITCH_EVENT_REQUEST_PARAMS, SWITCH_EVENT_SUBCLASS_ANY);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "skinny-auth");

	if (switch_xml_locate_user("id", request->data.reg.device_name, profile->domain, "", &xroot, &xdomain, &xuser, &xgroup, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find device [%s@%s]\n"
				"You must define a domain called '%s' in your directory and add a user with id=\"%s\".\n"
				, request->data.reg.device_name, profile->domain, profile->domain, request->data.reg.device_name);
		send_register_reject(listener, "Device not found");
		status =  SWITCH_STATUS_FALSE;
		goto end;
	}

	skinny_profile_find_listener_by_device_name_and_instance(listener->profile,
			request->data.reg.device_name, request->data.reg.instance, &listener2);
	if (listener2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"Device %s:%d is already registered on another listener.\n",
				request->data.reg.device_name, request->data.reg.instance);
		send_register_reject(listener, "Device is already registered on another listener");
		status =  SWITCH_STATUS_FALSE;
		goto end;
	}

	if ((sql = switch_mprintf(
					"INSERT INTO skinny_devices "
					"(name, user_id, instance, ip, type, max_streams, codec_string) "
					"VALUES ('%s','%d','%d', '%s', '%d', '%d', '%s')",
					request->data.reg.device_name,
					request->data.reg.user_id,
					request->data.reg.instance,
					inet_ntoa(request->data.reg.ip),
					request->data.reg.device_type,
					request->data.reg.max_streams,
					"" /* codec_string */
				 ))) {
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}


	strncpy(listener->device_name, request->data.reg.device_name, 16);
	listener->device_instance = request->data.reg.instance;
	listener->device_type = request->data.reg.device_type;

	xskinny = switch_xml_child(xuser, "skinny");
	if (xskinny) {
		if ((xparams = switch_xml_child(xskinny, "params"))) {
			for (xparam = switch_xml_child(xparams, "param"); xparam; xparam = xparam->next) {
				const char *name = switch_xml_attr_soft(xparam, "name");
				const char *value = switch_xml_attr_soft(xparam, "value");
				if (!strcasecmp(name, "skinny-firmware-version")) {
					strncpy(listener->firmware_version, value, 16);
				} else if (!strcasecmp(name, "skinny-soft-key-set-set")) {
					listener->soft_key_set_set = switch_core_strdup(profile->pool, value);
				}
			}
		}
		if ((xbuttons = switch_xml_child(xskinny, "buttons"))) {
			uint32_t line_instance = 1;
			char *network_ip = inet_ntoa(request->data.reg.ip);
			int network_port = 0;
			char network_port_c[6];
			snprintf(network_port_c, sizeof(network_port_c), "%d", network_port);
			for (xbutton = switch_xml_child(xbuttons, "button"); xbutton; xbutton = xbutton->next) {
				uint32_t position = atoi(switch_xml_attr_soft(xbutton, "position"));
				uint32_t type = skinny_str2button(switch_xml_attr_soft(xbutton, "type"));
				const char *label = switch_xml_attr_soft(xbutton, "label");
				const char *value = switch_xml_attr_soft(xbutton, "value");
				if(type ==  SKINNY_BUTTON_LINE) {
					const char *caller_name = switch_xml_attr_soft(xbutton, "caller-name");
					const char *reg_metadata = switch_xml_attr_soft(xbutton, "registration-metadata");
					uint32_t ring_on_idle = atoi(switch_xml_attr_soft(xbutton, "ring-on-idle"));
					uint32_t ring_on_active = atoi(switch_xml_attr_soft(xbutton, "ring-on-active"));
					uint32_t busy_trigger = atoi(switch_xml_attr_soft(xbutton, "busy-trigger"));
					const char *forward_all = switch_xml_attr_soft(xbutton, "forward-all");
					const char *forward_busy = switch_xml_attr_soft(xbutton, "forward-busy");
					const char *forward_noanswer = switch_xml_attr_soft(xbutton, "forward-noanswer");
					uint32_t noanswer_duration = atoi(switch_xml_attr_soft(xbutton, "noanswer-duration"));
					if ((sql = switch_mprintf(
									"INSERT INTO skinny_lines "
									"(device_name, device_instance, position, line_instance, "
									"label, value, caller_name, "
									"ring_on_idle, ring_on_active, busy_trigger, "
									"forward_all, forward_busy, forward_noanswer, noanswer_duration) "
									"VALUES('%s', %d, %d, %d, '%s', '%s', '%s', %d, %d, %d, '%s', '%s', '%s', %d)",
									request->data.reg.device_name, request->data.reg.instance, position, line_instance,
									label, value, caller_name,
									ring_on_idle, ring_on_active, busy_trigger,
									forward_all, forward_busy, forward_noanswer, noanswer_duration))) {
						char *token, *url;
						skinny_execute_sql(profile, sql, profile->sql_mutex);
						switch_safe_free(sql);
						token = switch_mprintf("skinny/%q/%q/%q:%d", profile->name, value, request->data.reg.device_name, request->data.reg.instance);
						url = switch_mprintf("skinny/%q/%q", profile->name, value);
						switch_core_add_registration(value, profile->domain, token, url, 0, network_ip, network_port_c, "tcp", reg_metadata);
						switch_safe_free(token);
						switch_safe_free(url);
					}
					if (line_instance == 1) {
						switch_event_t *message_query_event = NULL;
						if (switch_event_create(&message_query_event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header(message_query_event, SWITCH_STACK_BOTTOM, "Message-Account", "skinny:%s@%s", value, profile->domain);
							switch_event_add_header_string(message_query_event, SWITCH_STACK_BOTTOM, "VM-Skinny-Profile", profile->name);
							switch_event_fire(&message_query_event);
						}
					}
					line_instance++;
				} else {
					const char *settings = switch_xml_attr_soft(xbutton, "settings");
					if ((sql = switch_mprintf(
									"INSERT INTO skinny_buttons "
									"(device_name, device_instance, position, type, label, value, settings) "
									"VALUES('%s', %d, %d, %d, '%s', '%s', '%s')",
									request->data.reg.device_name,
									request->data.reg.instance,
									position,
									type,
									label,
									value,
									settings))) {
						skinny_execute_sql(profile, sql, profile->sql_mutex);
						switch_safe_free(sql);
					}
				}
			}
		}
	}
	if (xroot) {
		switch_xml_free(xroot);
	}

	status = SWITCH_STATUS_SUCCESS;

	/* Reply with RegisterAckMessage */
	send_register_ack(listener, profile->keep_alive, profile->date_format, "", profile->keep_alive, "");

	/* Send CapabilitiesReqMessage */
	send_capabilities_req(listener);

	/* skinny::register event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_REGISTER);
	switch_event_fire(&event);

	keepalive_listener(listener, NULL);

end:
	if(params) {
		switch_event_destroy(&params);
	}

	return status;
}

switch_status_t skinny_handle_port_message(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.as_uint16));

	if ((sql = switch_mprintf(
					"UPDATE skinny_devices SET port=%d WHERE name='%s' and instance=%d",
					request->data.port.port,
					listener->device_name,
					listener->device_instance
				 ))) {
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_keypad_button_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line_instance = 1;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;

	skinny_check_data_length(request, sizeof(request->data.keypad_button.button));

	if(skinny_check_data_length_soft(request, sizeof(request->data.keypad_button))) {
		if (request->data.keypad_button.line_instance > 0) {
			line_instance = request->data.keypad_button.line_instance;
		}
		call_id = request->data.keypad_button.call_id;
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
	if ( !session )
	{
		line_instance = 0;
		session = skinny_profile_find_session(listener->profile, listener, &line_instance, 0);
	}

	if(session) {
		switch_channel_t *channel = NULL;
		private_t *tech_pvt = NULL;
		char digit = '\0';

		channel = switch_core_session_get_channel(session);
		tech_pvt = switch_core_session_get_private(session);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SEND DTMF ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);

		if (request->data.keypad_button.button == 14) {
			digit = '*';
		} else if (request->data.keypad_button.button == 15) {
			digit = '#';
		} else if (request->data.keypad_button.button >= 0 && request->data.keypad_button.button <= 9) {
			digit = '0' + request->data.keypad_button.button;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "UNKNOW DTMF RECEIVED ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);
		}

		/* TODO check call_id and line */

		if((skinny_line_get_state(listener, line_instance, tech_pvt->call_id) == SKINNY_OFF_HOOK)) {

			skinny_session_process_dest(session, listener, line_instance, NULL, digit, 0);
		} else {
			if(digit != '\0') {
				switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0)};
				dtmf.digit = digit;
				switch_channel_queue_dtmf(channel, &dtmf);
			}
		}
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_enbloc_call_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line_instance = 1;
	switch_core_session_t *session = NULL;

	skinny_check_data_length(request, sizeof(request->data.enbloc_call.called_party));

	if(skinny_check_data_length_soft(request, sizeof(request->data.enbloc_call))) {
		if (request->data.enbloc_call.line_instance > 0) {
			line_instance = request->data.enbloc_call.line_instance;
		}
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, 0);

	if(session) {
		skinny_session_process_dest(session, listener, line_instance, request->data.enbloc_call.called_party, '\0', 0);
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_stimulus_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint32_t line_instance = 0;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;
	struct speed_dial_stat_res_message *button_speed_dial = NULL;
	struct line_stat_res_message *button_line = NULL;
	uint32_t line_state;

	switch_channel_t *channel = NULL;

	skinny_check_data_length(request, sizeof(request->data.stimulus)-sizeof(request->data.stimulus.call_id));

	if(skinny_check_data_length_soft(request, sizeof(request->data.stimulus))) {
		call_id = request->data.stimulus.call_id;
	}

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Received stimulus message of type (%s)\n",
		skinny_button2str(request->data.stimulus.instance_type));

	switch(request->data.stimulus.instance_type) {
		case SKINNY_BUTTON_LAST_NUMBER_REDIAL:
			skinny_create_incoming_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, listener->profile->ext_redial, '\0', 0);
			break;
		case SKINNY_BUTTON_SPEED_DIAL:
			skinny_speed_dial_get(listener, request->data.stimulus.instance, &button_speed_dial);

			session = skinny_profile_find_session(listener->profile, listener, &line_instance, 0);
			if(strlen(button_speed_dial->line) > 0) {
				if (!session) {
					skinny_create_incoming_session(listener, &line_instance, &session);
				}
				skinny_session_process_dest(session, listener, line_instance, button_speed_dial->line, '\0', 0);
			}
			break;
		case SKINNY_BUTTON_HOLD:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

			if(session) {
				status = skinny_session_hold_line(session, listener, line_instance);
			}
			break;
		case SKINNY_BUTTON_TRANSFER:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

			if(session) {
				status = skinny_session_transfer(session, listener, line_instance);
			}			
			break;
		case SKINNY_BUTTON_VOICEMAIL:
			skinny_create_incoming_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, listener->profile->ext_voicemail, '\0', 0);
			break;

		case SKINNY_BUTTON_LINE:
			// Get the button data
			skinny_line_get(listener, request->data.stimulus.instance, &button_line);

			// Set the button and try to open the incoming session with this
			line_instance = button_line->number;
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

			// If session and line match, answer the call
			if ( session && line_instance == button_line->number ) {
				line_state = skinny_line_get_state(listener, line_instance, call_id);

				if(line_state == SKINNY_OFF_HOOK) {
					channel = switch_core_session_get_channel(session);
					if (switch_channel_test_flag(channel, CF_HOLD)) {
						switch_ivr_unhold(session);
					}

					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				} 
				else {
					status = skinny_session_answer(session, listener, line_instance);
				}
			}
			else {
				if(skinny_check_data_length_soft(request, sizeof(request->data.soft_key_event))) {
					line_instance = request->data.soft_key_event.line_instance;
				}

				skinny_create_incoming_session(listener, &line_instance, &session);
				skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 0);
			}
			break;

		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown Stimulus Type Received [%d]\n", request->data.stimulus.instance_type);
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return status;
}

switch_status_t skinny_handle_off_hook_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line_instance = 1;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;
	private_t *tech_pvt = NULL;
	uint32_t line_state;

	if(skinny_check_data_length_soft(request, sizeof(request->data.off_hook))) {
		if (request->data.off_hook.line_instance > 0) {
			line_instance = request->data.off_hook.line_instance;
		}
		call_id = request->data.off_hook.call_id;
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

	line_state = skinny_line_get_state(listener, line_instance, call_id);

	if(session && line_state != SKINNY_OFF_HOOK ) { /*answering a call */
		skinny_session_answer(session, listener, line_instance);
	} else { /* start a new call */
		skinny_create_incoming_session(listener, &line_instance, &session);
		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);

		skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 0);
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_on_hook_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint32_t line_instance = 0;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;

	if(skinny_check_data_length_soft(request, sizeof(request->data.on_hook))) {
		line_instance = request->data.on_hook.line_instance;
		call_id = request->data.on_hook.call_id;
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

	if(session) {
		switch_channel_t *channel = NULL;
		private_t *tech_pvt = NULL;

		channel = switch_core_session_get_channel(session);
		tech_pvt = switch_core_session_get_private(session);

		if (tech_pvt->transfer_from_call_id) { /* blind transfer */
			status = skinny_session_transfer(session, listener, line_instance);
		} else {
			if (skinny_line_get_state(listener, line_instance, call_id) != SKINNY_IN_USE_REMOTELY) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			}
		}
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return status;
}
switch_status_t skinny_handle_forward_stat_req_message(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;

	skinny_check_data_length(request, sizeof(request->data.forward_stat_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.forward_stat));
	message->type = FORWARD_STAT_MESSAGE;
	message->length = 4 + sizeof(message->data.forward_stat);

	message->data.forward_stat.line_instance = request->data.forward_stat_req.line_instance;

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Handle Forward Stat Req Message with Line Instance (%d)\n", 
		request->data.forward_stat_req.line_instance);
	skinny_send_reply_quiet(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_speed_dial_stat_request(listener_t *listener, skinny_message_t *request)
{
	struct speed_dial_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.speed_dial_req));

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Handle Speed Dial Stat Request for Number (%d)\n", request->data.speed_dial_req.number);

	skinny_speed_dial_get(listener, request->data.speed_dial_req.number, &button);

	send_speed_dial_stat_res(listener, request->data.speed_dial_req.number, button->line, button->label);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_line_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct line_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.line_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.line_res));
	message->type = LINE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.line_res);

	skinny_line_get(listener, request->data.line_req.number, &button);

	memcpy(&message->data.line_res, button, sizeof(struct line_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

int skinny_config_stat_res_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;
	char *device_name = argv[0];
	int user_id = atoi(argv[1]);
	int instance = atoi(argv[2]);
	char *user_name = argv[3];
	char *server_name = argv[4];
	int number_lines = atoi(argv[5]);
	int number_speed_dials = atoi(argv[6]);

	strncpy(message->data.config_res.device_name, device_name, 16);
	message->data.config_res.user_id = user_id;
	message->data.config_res.instance = instance;
	strncpy(message->data.config_res.user_name, user_name, 40);
	strncpy(message->data.config_res.server_name, server_name, 40);
	message->data.config_res.number_lines = number_lines;
	message->data.config_res.number_speed_dials = number_speed_dials;

	return 0;
}

switch_status_t skinny_handle_config_stat_request(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.config_res));
	message->type = CONFIG_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.config_res);

	if ((sql = switch_mprintf(
					"SELECT name, user_id, instance, '' AS user_name, '' AS server_name, "
					"(SELECT COUNT(*) FROM skinny_lines WHERE device_name='%s' AND device_instance=%d) AS number_lines, "
					"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND device_instance=%d AND type=%d) AS number_speed_dials "
					"FROM skinny_devices WHERE name='%s' ",
					listener->device_name,
					listener->device_instance,
					listener->device_name,
					listener->device_instance,
					SKINNY_BUTTON_SPEED_DIAL,
					listener->device_name
				 ))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_config_stat_res_callback, message);
		switch_safe_free(sql);
	}
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_time_date_request(listener_t *listener, skinny_message_t *request)
{
	return send_define_current_time_date(listener);
}

struct button_template_helper {
	skinny_message_t *message;
	int count[SKINNY_BUTTON_UNDEFINED+1];
	int max_position;
};

int skinny_handle_button_template_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct button_template_helper *helper = pArg;
	skinny_message_t *message = helper->message;
	/* char *device_name = argv[0]; */
	/* uint32_t device_instance = argv[1]; */
	int position = atoi(argv[2]);
	uint32_t type = atoi(argv[3]);
	/* int relative_position = atoi(argv[4]); */

	message->data.button_template.btn[position-1].instance_number = ++helper->count[type];
	message->data.button_template.btn[position-1].button_definition = type;

	message->data.button_template.button_count++;
	message->data.button_template.total_button_count++;
	if(position > helper->max_position) {
		helper->max_position = position;
	}

	return 0;
}

switch_status_t skinny_handle_button_template_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct button_template_helper helper = {0};
	skinny_profile_t *profile;
	char *sql;
	int i;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.button_template));
	message->type = BUTTON_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.button_template);

	message->data.button_template.button_offset = 0;
	message->data.button_template.button_count = 0;
	message->data.button_template.total_button_count = 0;

	helper.message = message;

	/* Add buttons */
	if ((sql = switch_mprintf(
					"SELECT device_name, device_instance, position, type "
					"FROM skinny_buttons "
					"WHERE device_name='%s' AND device_instance=%d "
					"ORDER BY position",
					listener->device_name, listener->device_instance
				 ))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}

	/* Add lines */
	if ((sql = switch_mprintf(
					"SELECT device_name, device_instance, position, %d AS type "
					"FROM skinny_lines "
					"WHERE device_name='%s' AND device_instance=%d "
					"ORDER BY position",
					SKINNY_BUTTON_LINE,
					listener->device_name, listener->device_instance
				 ))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}

	/* Fill remaining buttons with Undefined */
	for(i = 0; i+1 < helper.max_position; i++) {
		if(message->data.button_template.btn[i].button_definition == SKINNY_BUTTON_UNKNOWN) {
			message->data.button_template.btn[i].instance_number = ++helper.count[SKINNY_BUTTON_UNDEFINED];
			message->data.button_template.btn[i].button_definition = SKINNY_BUTTON_UNDEFINED;
			message->data.button_template.button_count++;
			message->data.button_template.total_button_count++;
		}
	}



	return skinny_send_reply(listener, message);;
}

switch_status_t skinny_handle_version_request(listener_t *listener, skinny_message_t *request)
{
	if (zstr(listener->firmware_version)) {
		char *id_str;
		skinny_device_type_params_t *params;
		id_str = switch_mprintf("%d", listener->device_type);
		params = (skinny_device_type_params_t *) switch_core_hash_find(listener->profile->device_type_params_hash, id_str);
		if (params) {
			if (!zstr(params->firmware_version)) {
				strncpy(listener->firmware_version, params->firmware_version, 16);
			}
		}
	}

	if (!zstr(listener->firmware_version)) {
		return send_version(listener, listener->firmware_version);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
				"Device %s:%d is requesting for firmware version, but none is set.\n",
				listener->device_name, listener->device_instance);
		return SWITCH_STATUS_SUCCESS;
	}
}

switch_status_t skinny_handle_capabilities_response(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	uint32_t i = 0;
	uint32_t n = 0;
	char *codec_order[SWITCH_MAX_CODECS];
	char *codec_string;

	size_t string_len, string_pos, pos;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count));

	n = request->data.cap_res.count;
	if (n > SWITCH_MAX_CODECS) {
		n = SWITCH_MAX_CODECS;
	}
	string_len = -1;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count) + n * sizeof(request->data.cap_res.caps[0]));

	for (i = 0; i < n; i++) {
		char *codec = skinny_codec2string(request->data.cap_res.caps[i].codec);
		codec_order[i] = codec;
		string_len += strlen(codec)+1;
	}
	i = 0;
	pos = 0;
	codec_string = switch_core_alloc(listener->pool, string_len+1);
	for (string_pos = 0; string_pos < string_len; string_pos++) {
		char *codec = codec_order[i];
		switch_assert(i < n);
		if(pos == strlen(codec)) {
			codec_string[string_pos] = ',';
			i++;
			pos = 0;
		} else {
			codec_string[string_pos] = codec[pos++];
		}
	}
	codec_string[string_len] = '\0';
	if ((sql = switch_mprintf(
					"UPDATE skinny_devices SET codec_string='%s' WHERE name='%s'",
					codec_string,
					listener->device_name
				 ))) {
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}
	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Codecs %s supported.\n", codec_string);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_alarm(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;

	skinny_check_data_length(request, sizeof(request->data.alarm));

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Received alarm: Severity=%d, DisplayMessage=%s, Param1=%d, Param2=%d.\n",
		request->data.alarm.alarm_severity, request->data.alarm.display_message,
		request->data.alarm.alarm_param1, request->data.alarm.alarm_param2);

	/* skinny::alarm event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_ALARM);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Severity", "%d", request->data.alarm.alarm_severity);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-DisplayMessage", "%s", request->data.alarm.display_message);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param1", "%d", request->data.alarm.alarm_param1);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param2", "%d", request->data.alarm.alarm_param2);
	switch_event_fire(&event);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_open_receive_channel_ack_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint32_t line_instance = 0;
	switch_core_session_t *session;

	skinny_check_data_length(request, sizeof(request->data.open_receive_channel_ack));

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.open_receive_channel_ack.pass_thru_party_id);

	if(session) {
		const char *err = NULL;
		private_t *tech_pvt = NULL;
		switch_channel_t *channel = NULL;
		struct in_addr addr;

		flags[SWITCH_RTP_FLAG_DATAWAIT]++;
		flags[SWITCH_RTP_FLAG_AUTOADJ]++;
		flags[SWITCH_RTP_FLAG_RAW_WRITE]++;

		tech_pvt = switch_core_session_get_private(session);
		channel = switch_core_session_get_channel(session);

		/* Codec */
		tech_pvt->iananame = "PCMU"; /* TODO */
		tech_pvt->codec_ms = 20; /* TODO */
		tech_pvt->rm_rate = 8000; /* TODO */
		tech_pvt->rm_fmtp = NULL; /* TODO */
		tech_pvt->agreed_pt = (switch_payload_t) 0; /* TODO */
		tech_pvt->rm_encoding = switch_core_strdup(switch_core_session_get_pool(session), "");
		skinny_tech_set_codec(tech_pvt, 0);
		if ((status = skinny_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		tech_pvt->local_sdp_audio_ip = listener->local_ip;
		/* Request a local port from the core's allocator */
		if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(tech_pvt->local_sdp_audio_ip))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
			return SWITCH_STATUS_FALSE;
		}

		tech_pvt->remote_sdp_audio_ip = inet_ntoa(request->data.open_receive_channel_ack.ip);
		tech_pvt->remote_sdp_audio_port = request->data.open_receive_channel_ack.port;

		tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
				tech_pvt->local_sdp_audio_port,
				tech_pvt->remote_sdp_audio_ip,
				tech_pvt->remote_sdp_audio_port,
				tech_pvt->agreed_pt,
				tech_pvt->read_impl.samples_per_packet,
				tech_pvt->codec_ms * 1000,
				(switch_rtp_flag_t) 0, "soft", &err,
				switch_core_session_get_pool(session));
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
				"AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
				switch_channel_get_name(channel),
				tech_pvt->local_sdp_audio_ip,
				tech_pvt->local_sdp_audio_port,
				tech_pvt->remote_sdp_audio_ip,
				tech_pvt->remote_sdp_audio_port,
				tech_pvt->agreed_pt,
				tech_pvt->read_impl.microseconds_per_packet / 1000,
				switch_rtp_ready(tech_pvt->rtp_session) ? "SUCCESS" : err);
#ifdef WIN32
		addr.s_addr = inet_addr((uint16_t) tech_pvt->local_sdp_audio_ip);
#else
		inet_aton(tech_pvt->local_sdp_audio_ip, &addr);
#endif
		send_start_media_transmission(listener,
				tech_pvt->call_id, /* uint32_t conference_id, */
				tech_pvt->party_id, /* uint32_t pass_thru_party_id, */
				addr.s_addr, /* uint32_t remote_ip, */
				tech_pvt->local_sdp_audio_port, /* uint32_t remote_port, */
				20, /* uint32_t ms_per_packet, */
				SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
				184, /* uint32_t precedence, */
				0, /* uint32_t silence_suppression, */
				0, /* uint16_t max_frames_per_packet, */
				0 /* uint32_t g723_bitrate */
				);

		switch_set_flag_locked(tech_pvt, TFLAG_IO);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_mark_answered(channel);
		}
		if (switch_channel_test_flag(channel, CF_HOLD)) {
			switch_ivr_unhold(session);
			send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_ON);
		}
	} else {
		skinny_log_l(listener, SWITCH_LOG_WARNING, "Unable to find session for call id=%d.\n", 
				request->data.open_receive_channel_ack.pass_thru_party_id);
	}
end:
	if(session) {
		switch_core_session_rwunlock(session);
	}
	return status;
}

switch_status_t skinny_handle_soft_key_set_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message = NULL;

	if (listener->soft_key_set_set) {
		message = switch_core_hash_find(listener->profile->soft_key_set_sets_hash, listener->soft_key_set_set);
		skinny_log_l(listener, SWITCH_LOG_DEBUG, "Handle Soft Key Set Request with Set (%s)\n", listener->soft_key_set_set);
	}
	if (!message) {
		message = switch_core_hash_find(listener->profile->soft_key_set_sets_hash, "default");
		skinny_log_l(listener, SWITCH_LOG_DEBUG, "Handle Soft Key Set Request with Set (%s)\n", "default");
	}
	if (message) {
		skinny_send_reply(listener, message);
	} else {
		skinny_log_l(listener, SWITCH_LOG_ERROR, "Profile %s doesn't have a default <soft-key-set-set>.\n", 
			listener->profile->name);
	}

	/* Init the states */
	send_select_soft_keys(listener, 0, 0, SKINNY_KEY_SET_ON_HOOK, 0xffff);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_soft_key_event_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint32_t line_instance = 0;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	skinny_check_data_length(request, sizeof(request->data.soft_key_event.event));

	if(skinny_check_data_length_soft(request, sizeof(request->data.soft_key_event))) {
		line_instance = request->data.soft_key_event.line_instance;
		call_id = request->data.soft_key_event.call_id;
	}

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Soft Key Event (%s) with Line Instance (%d), Call ID (%d)\n",
		skinny_soft_key_event2str(request->data.soft_key_event.event), line_instance, call_id);

	switch(request->data.soft_key_event.event) {
		case SOFTKEY_REDIAL:
			status = skinny_create_incoming_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, listener->profile->ext_redial, '\0', 0);
			break;
		case SOFTKEY_NEWCALL:
			status = skinny_create_incoming_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 0);
			break;
		case SOFTKEY_HOLD:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				status = skinny_session_hold_line(session, listener, line_instance);
			}
			break;
		case SOFTKEY_TRANSFER:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);

			if(session) {
				status = skinny_session_transfer(session, listener, line_instance);
			}
			break;
		case SOFTKEY_BACKSPACE:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 1);
			}
			break;
		case SOFTKEY_ENDCALL:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				channel = switch_core_session_get_channel(session);
				if (switch_channel_test_flag(channel, CF_HOLD)) {
					switch_ivr_unhold(session);
				}
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			}
			break;
		case SOFTKEY_RESUME:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				status = skinny_session_unhold_line(session, listener, line_instance);
			}
			break;
		case SOFTKEY_ANSWER:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				status = skinny_session_answer(session, listener, line_instance);
			}
			break;
		case SOFTKEY_IDIVERT:
			session = skinny_profile_find_session(listener->profile, listener, &line_instance, call_id);
			if(session) {
				switch_channel_t *channel = NULL;
				channel = switch_core_session_get_channel(session);

				if (channel) {
					switch_channel_hangup(channel, SWITCH_CAUSE_NO_ANSWER);
				}
			}
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
					"Unknown SoftKeyEvent type: %d.\n", request->data.soft_key_event.event);
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return status;
}

switch_status_t skinny_handle_unregister(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	skinny_message_t *message;

	/* skinny::unregister event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_UNREGISTER);
	switch_event_fire(&event);

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.unregister_ack));
	message->type = UNREGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.unregister_ack);
	message->data.unregister_ack.unregister_status = 0; /* OK */

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Handle Unregister with Status (%d)\n", message->data.unregister_ack.unregister_status);
	
	skinny_send_reply_quiet(listener, message);

	/* Close socket */
	switch_clear_flag_locked(listener, LFLAG_RUNNING);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_soft_key_template_request(listener_t *listener, skinny_message_t *request)
{
	size_t i;
	skinny_message_t *message;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.soft_key_template));
	message->type = SOFT_KEY_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.soft_key_template);

	message->data.soft_key_template.soft_key_offset = 0;
	message->data.soft_key_template.soft_key_count = 21;
	message->data.soft_key_template.total_soft_key_count = 21;

	memset(message->data.soft_key_template.soft_key, 0, sizeof(message->data.soft_key_template));
	for (i=0; i< TEXT_ID_LEN; i++) {
		char *label = skinny_textid2raw(soft_key_template_default_textids[i]);
		strcpy(message->data.soft_key_template.soft_key[i].soft_key_label, skinny_textid2raw(soft_key_template_default_textids[i]));
		switch_safe_free(label);

		message->data.soft_key_template.soft_key[i].soft_key_event = soft_key_template_default_events[i];
	}
		
	skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Handle Soft Key Template Request with Default Template\n");

	skinny_send_reply_quiet(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_headset_status_message(listener_t *listener, skinny_message_t *request)
{
	char *sql;

	skinny_check_data_length(request, sizeof(request->data.headset_status));

	if ((sql = switch_mprintf(
					"UPDATE skinny_devices SET headset=%d WHERE name='%s' and instance=%d",
					(request->data.headset_status.mode==1) ? SKINNY_ACCESSORY_STATE_OFFHOOK : SKINNY_ACCESSORY_STATE_ONHOOK,
					listener->device_name,
					listener->device_instance
				 ))) {
		skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
		switch_safe_free(sql);
	}

	skinny_log_l(listener, SWITCH_LOG_DEBUG, "Update headset accessory status (%s)\n", 
		skinny_accessory_state2str(request->data.headset_status.mode));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_register_available_lines_message(listener_t *listener, skinny_message_t *request)
{
	skinny_check_data_length(request, sizeof(request->data.reg_lines));

	skinny_log_l_msg(listener, SWITCH_LOG_DEBUG, "Handle Register Available Lines\n");

	/* Do nothing */
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_data_message(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	char *tmp = NULL;
	skinny_check_data_length(request, sizeof(request->data.data));
	skinny_check_data_length(request, sizeof(request->data.data) + request->data.data.data_length - 1);

	/* skinny::device_to_user event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_DEVICE_TO_USER);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Message-Id", "%d", request->type);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Message-Id-String", "%s", skinny_message_type2str(request->type));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Application-Id", "%d", request->data.data.application_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Line-Instance", "%d", request->data.data.line_instance);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Call-Id", "%d", request->data.data.call_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Transaction-Id", "%d", request->data.data.transaction_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Data-Length", "%d", request->data.data.data_length);
	/* Ensure that the body is null-terminated */
	tmp = malloc(request->data.data.data_length + 1);
	memcpy(tmp, request->data.data.data, request->data.data.data_length);
	tmp[request->data.data.data_length] = '\0';
	switch_event_add_body(event, "%s", tmp);
	switch_safe_free(tmp);
	switch_event_fire(&event);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_service_url_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct service_url_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.service_url_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.service_url_res));
	message->type = SERVICE_URL_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.service_url_res);

	skinny_service_url_get(listener, request->data.service_url_req.service_url_index, &button);

	memcpy(&message->data.service_url_res, button, sizeof(struct service_url_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_feature_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct feature_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.feature_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.feature_res));
	message->type = FEATURE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.feature_res);

	skinny_feature_get(listener, request->data.feature_req.feature_index, &button);

	memcpy(&message->data.feature_res, button, sizeof(struct feature_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_extended_data_message(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	char *tmp = NULL;
	skinny_check_data_length(request, sizeof(request->data.extended_data));
	skinny_check_data_length(request, sizeof(request->data.extended_data)+request->data.extended_data.data_length-1);

	/* skinny::device_to_user event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_DEVICE_TO_USER);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Message-Id", "%d", request->type);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Message-Id-String", "%s", skinny_message_type2str(request->type));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Application-Id", "%d", request->data.extended_data.application_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Line-Instance", "%d", request->data.extended_data.line_instance);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Call-Id", "%d", request->data.extended_data.call_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Transaction-Id", "%d", request->data.extended_data.transaction_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Data-Length", "%d", request->data.extended_data.data_length);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Sequence-Flag", "%d", request->data.extended_data.sequence_flag);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Display-Priority", "%d", request->data.extended_data.display_priority);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Conference-Id", "%d", request->data.extended_data.conference_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-App-Instance-Id", "%d", request->data.extended_data.app_instance_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-DeviceToUser-Routing-Id", "%d", request->data.extended_data.routing_id);
	/* Ensure that the body is null-terminated */
	tmp = malloc(request->data.data.data_length + 1);
	memcpy(tmp, request->data.data.data, request->data.data.data_length);
	tmp[request->data.data.data_length] = '\0';
	switch_event_add_body(event, "%s", tmp);
	switch_safe_free(tmp);
	switch_event_fire(&event);

	return SWITCH_STATUS_SUCCESS;
}
switch_status_t skinny_handle_dialed_phone_book_message(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;

	skinny_check_data_length(request, sizeof(request->data.dialed_phone_book));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.dialed_phone_book_ack));
	message->type = DIALED_PHONE_BOOK_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.dialed_phone_book_ack);
	message->data.dialed_phone_book_ack.number_index = request->data.dialed_phone_book.number_index;
	message->data.dialed_phone_book_ack.line_instance = request->data.dialed_phone_book.line_instance;
	message->data.dialed_phone_book_ack.unknown = request->data.dialed_phone_book.unknown;
	message->data.dialed_phone_book_ack.unknown2 = 0;

	return SWITCH_STATUS_SUCCESS;
}
switch_status_t skinny_handle_accessory_status_message(listener_t *listener, skinny_message_t *request)
{
	char *sql;

	skinny_check_data_length(request, sizeof(request->data.accessory_status));

	switch(request->data.accessory_status.accessory_id) {
		case SKINNY_ACCESSORY_HEADSET:
			if ((sql = switch_mprintf(
							"UPDATE skinny_devices SET headset=%d WHERE name='%s' and instance=%d",
							request->data.accessory_status.accessory_status,
							listener->device_name,
							listener->device_instance
						 ))) {
				skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
				switch_safe_free(sql);
			}
			break;
		case SKINNY_ACCESSORY_HANDSET:
			if ((sql = switch_mprintf(
							"UPDATE skinny_devices SET handset=%d WHERE name='%s' and instance=%d",
							request->data.accessory_status.accessory_status,
							listener->device_name,
							listener->device_instance
						 ))) {
				skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
				switch_safe_free(sql);
			}
			break;
		case SKINNY_ACCESSORY_SPEAKER:
			if ((sql = switch_mprintf(
							"UPDATE skinny_devices SET speaker=%d WHERE name='%s' and instance=%d",
							request->data.accessory_status.accessory_status,
							listener->device_name,
							listener->device_instance
						 ))) {
				skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
				switch_safe_free(sql);
			}
			break;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_xml_alarm(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	char *tmp = NULL;

	skinny_log_l(listener, SWITCH_LOG_INFO, "Received XML alarm (length=%d).\n", request->length);
	/* skinny::xml_alarm event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_XML_ALARM);
	/* Ensure that the body is null-terminated */
	tmp = malloc(request->length - 4 + 1);
	memcpy(tmp, request->data.as_char, request->length - 4);
	tmp[request->length - 4] = '\0';
	switch_event_add_body(event, "%s", tmp);
	switch_safe_free(tmp);
	switch_event_fire(&event);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request)
{
	if (listener->profile->debug >= 10 || request->type != KEEP_ALIVE_MESSAGE) {
		skinny_log_l(listener, SWITCH_LOG_DEBUG, "Received %s (type=%x,length=%d).\n",
			skinny_message_type2str(request->type), request->type, request->length);
	}
	if(zstr(listener->device_name) && request->type != REGISTER_MESSAGE && request->type != ALARM_MESSAGE && request->type != XML_ALARM_MESSAGE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
				"Device should send a register message first. Received %s (type=%x,length=%d).\n", skinny_message_type2str(request->type), request->type, request->length);
		return SWITCH_STATUS_FALSE;
	}
	switch(request->type) {
		case KEEP_ALIVE_MESSAGE:
			return skinny_handle_keep_alive_message(listener, request);
		case REGISTER_MESSAGE:
			return skinny_handle_register(listener, request);
		case PORT_MESSAGE:
			return skinny_handle_port_message(listener, request);
		case KEYPAD_BUTTON_MESSAGE:
			return skinny_handle_keypad_button_message(listener, request);
		case ENBLOC_CALL_MESSAGE:
			return skinny_handle_enbloc_call_message(listener, request);
		case STIMULUS_MESSAGE:
			return skinny_handle_stimulus_message(listener, request);
		case OFF_HOOK_MESSAGE:
			return skinny_handle_off_hook_message(listener, request);
		case ON_HOOK_MESSAGE:
			return skinny_handle_on_hook_message(listener, request);
		case FORWARD_STAT_REQ_MESSAGE:
			return skinny_handle_forward_stat_req_message(listener, request);
		case SPEED_DIAL_STAT_REQ_MESSAGE:
			return skinny_handle_speed_dial_stat_request(listener, request);
		case LINE_STAT_REQ_MESSAGE:
			return skinny_handle_line_stat_request(listener, request);
		case CONFIG_STAT_REQ_MESSAGE:
			return skinny_handle_config_stat_request(listener, request);
		case TIME_DATE_REQ_MESSAGE:
			return skinny_handle_time_date_request(listener, request);
		case BUTTON_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_button_template_request(listener, request);
		case VERSION_REQ_MESSAGE:
			return skinny_handle_version_request(listener, request);
		case CAPABILITIES_RES_MESSAGE:
			return skinny_handle_capabilities_response(listener, request);
		case ALARM_MESSAGE:
			return skinny_handle_alarm(listener, request);
		case OPEN_RECEIVE_CHANNEL_ACK_MESSAGE:
			return skinny_handle_open_receive_channel_ack_message(listener, request);
		case SOFT_KEY_SET_REQ_MESSAGE:
			return skinny_handle_soft_key_set_request(listener, request);
		case SOFT_KEY_EVENT_MESSAGE:
			return skinny_handle_soft_key_event_message(listener, request);
		case UNREGISTER_MESSAGE:
			return skinny_handle_unregister(listener, request);
		case SOFT_KEY_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_soft_key_template_request(listener, request);
		case HEADSET_STATUS_MESSAGE:
			return skinny_headset_status_message(listener, request);
		case REGISTER_AVAILABLE_LINES_MESSAGE:
			return skinny_handle_register_available_lines_message(listener, request);
		case DEVICE_TO_USER_DATA_MESSAGE:
			return skinny_handle_data_message(listener, request);
		case DEVICE_TO_USER_DATA_RESPONSE_MESSAGE:
			return skinny_handle_data_message(listener, request);
		case SERVICE_URL_STAT_REQ_MESSAGE:
			return skinny_handle_service_url_stat_request(listener, request);
		case FEATURE_STAT_REQ_MESSAGE:
			return skinny_handle_feature_stat_request(listener, request);
		case DEVICE_TO_USER_DATA_VERSION1_MESSAGE:
			return skinny_handle_extended_data_message(listener, request);
		case DEVICE_TO_USER_DATA_RESPONSE_VERSION1_MESSAGE:
			return skinny_handle_extended_data_message(listener, request);
		case DIALED_PHONE_BOOK_MESSAGE:
			return skinny_handle_dialed_phone_book_message(listener, request);
		case ACCESSORY_STATUS_MESSAGE:
			return skinny_handle_accessory_status_message(listener, request);
		case XML_ALARM_MESSAGE:
			return skinny_handle_xml_alarm(listener, request);
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
					"Unhandled %s (type=%x,length=%d).\n", skinny_message_type2str(request->type), request->type, request->length);
			return SWITCH_STATUS_SUCCESS;
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

