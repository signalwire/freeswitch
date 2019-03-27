/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * kazoo_dptools.c -- clones of mod_dptools commands slightly modified for kazoo
 *
 */
#include "mod_kazoo.h"

#define SET_SHORT_DESC "Set a channel variable"
#define SET_LONG_DESC "Set a channel variable for the channel calling the application."
#define SET_SYNTAX "<varname>=<value>"

#define MULTISET_SHORT_DESC "Set many channel variables"
#define MULTISET_LONG_DESC "Set many channel variables for the channel calling the application"
#define MULTISET_SYNTAX "[^^<delim>]<varname>=<value> <var2>=<val2>"

#define UNSET_SHORT_DESC "Unset a channel variable"
#define UNSET_LONG_DESC "Unset a channel variable for the channel calling the application."
#define UNSET_SYNTAX "<varname>"

#define MULTIUNSET_SHORT_DESC "Unset many channel variables"
#define MULTIUNSET_LONG_DESC "Unset many channel variables for the channel calling the application."
#define MULTIUNSET_SYNTAX "[^^<delim>]<varname> <var2> <var3>"

#define EXPORT_SHORT_DESC "Export many channel variables"
#define EXPORT_LONG_DESC "Export many channel variables for the channel calling the application"
#define EXPORT_SYNTAX "[^^<delim>]<varname>=<value> <var2>=<val2>"

#define PREFIX_UNSET_SHORT_DESC "clear variables by prefix"
#define PREFIX_UNSET_LONG_DESC "clears the channel variables that start with prefix supplied"
#define PREFIX_UNSET_SYNTAX "<prefix>"

#define UUID_MULTISET_SHORT_DESC "Set many channel variables"
#define UUID_MULTISET_LONG_DESC "Set many channel variables for a specific channel"
#define UUID_MULTISET_SYNTAX "<uuid> [^^<delim>]<varname>=<value> <var2>=<val2>"

#define KZ_ENDLESS_PLAYBACK_SHORT_DESC "Playback File Endlessly until break"
#define KZ_ENDLESS_PLAYBACK_LONG_DESC "Endlessly Playback a file to the channel until a break occurs"
#define KZ_ENDLESS_PLAYBACK_SYNTAX "<path>"

#define NOOP_SHORT_DESC "no operation"
#define NOOP_LONG_DESC "no operation. serves as a control point"
#define NOOP_SYNTAX "[<noop-id>]"

static void base_set (switch_core_session_t *session, const char *data, int urldecode, switch_stack_t stack)
{
	char *var, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *expanded = NULL;

		var = switch_core_session_strdup(session, data);

		if (!(val = strchr(var, '='))) {
			val = strchr(var, ',');
		}

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		if (val) {
			if(urldecode) {
				switch_url_decode(val);
			}
			expanded = switch_channel_expand_variables(channel, val);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SET [%s]=[%s] => [%s]\n", switch_channel_get_name(channel), var, val,
						  expanded ? expanded : "UNDEF");
		switch_channel_add_variable_var_check(channel, var, expanded, SWITCH_FALSE, stack);
		kz_check_set_profile_var(channel, var, expanded);
		if (expanded && expanded != val) {
			switch_safe_free(expanded);
		}
	}
}

static int kz_is_exported(switch_core_session_t *session, char *varname)
{
	char *array[256] = {0};
	int i, argc;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *exports = switch_channel_get_variable(channel, SWITCH_EXPORT_VARS_VARIABLE);
	char *arg = switch_core_session_strdup(session, exports);
	argc = switch_split(arg, ',', array);
	for(i=0; i < argc; i++) {
		if(!strcasecmp(array[i], varname))
			return 1;
	}

	return 0;
}

static void base_export (switch_core_session_t *session, const char *data, int urldecode, switch_stack_t stack)
{
        char *var, *val = NULL;

        if (zstr(data)) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
        } else {
                switch_channel_t *channel = switch_core_session_get_channel(session);
                char *expanded = NULL;

                var = switch_core_session_strdup(session, data);

                if (!(val = strchr(var, '='))) {
                        val = strchr(var, ',');
                }

                if (val) {
                        *val++ = '\0';
                        if (zstr(val)) {
                                val = NULL;
                        }
                }

				if (val) {
					if(urldecode) {
						switch_url_decode(val);
					}
					expanded = switch_channel_expand_variables(channel, val);

					if(!kz_is_exported(session, var)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s EXPORT [%s]=[%s]\n", switch_channel_get_name(channel), var, expanded ? expanded : "UNDEF");
						switch_channel_export_variable_var_check(channel, var, expanded, SWITCH_EXPORT_VARS_VARIABLE, SWITCH_FALSE);
					} else {
						if(strcmp(switch_str_nil(switch_channel_get_variable_dup(channel, var, SWITCH_FALSE, -1)), expanded)) {
							switch_channel_set_variable(channel, var, expanded);
						}
					}
					if (expanded && expanded != val) {
						switch_safe_free(expanded);
					}
				}
        }
}

SWITCH_STANDARD_APP(prefix_unset_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_header_t *ei = NULL;
	switch_event_t *clear;
	char *arg = (char *) data;

	if(switch_event_create(&clear, SWITCH_EVENT_CLONE) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	for (ei = switch_channel_variable_first(channel); ei; ei = ei->next) {
		const char *name = ei->name;
		char *value = ei->value;
		if (!strncasecmp(name, arg, strlen(arg))) {
			switch_event_add_header_string(clear, SWITCH_STACK_BOTTOM, name, value);
		}
	}

	switch_channel_variable_last(channel);
	for (ei = clear->headers; ei; ei = ei->next) {
		char *varname = ei->name;
		switch_channel_set_variable(channel, varname, NULL);
	}

	switch_event_destroy(&clear);
}

void kz_multiset(switch_core_session_t *session, const char* data, int urldecode)
{
	char delim = ' ';
	char *arg = (char *) data;
	switch_event_t *event;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if(delim != '\0') {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		if (arg) {
			char *array[256] = {0};
			int i, argc;

			arg = switch_core_session_strdup(session, arg);
			argc = switch_split(arg, delim, array);

			for(i = 0; i < argc; i++) {
				base_set(session, array[i], urldecode, SWITCH_STACK_BOTTOM);
			}
		}
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "multiset with empty args\n");
	}
}

SWITCH_STANDARD_APP(multiset_function)
{
	kz_multiset(session, data, 0);
}

SWITCH_STANDARD_APP(multiset_encoded_function)
{
	kz_multiset(session, data, 1);
}

void kz_uuid_multiset(switch_core_session_t *session, const char* data, int urldecode)
{
	char delim = ' ';
	char *arg0 = (char *) data;
	char *arg = strchr(arg0, ' ');
	switch_event_t *event;


	if(arg == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "uuid_multiset with invalid args\n");
		return;
	}
	*arg = '\0';
	arg++;

	if(zstr(arg0)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "uuid_multiset with invalid uuid\n");
		return;
	}


	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if(delim != '\0') {
		switch_core_session_t *uuid_session = NULL;
		if ((uuid_session = switch_core_session_force_locate(arg0)) != NULL) {
			switch_channel_t *uuid_channel = switch_core_session_get_channel(uuid_session);
			if (arg) {
				char *array[256] = {0};
				int i, argc;

				arg = switch_core_session_strdup(session, arg);
				argc = switch_split(arg, delim, array);

				for(i = 0; i < argc; i++) {
					base_set(uuid_session, array[i], urldecode, SWITCH_STACK_BOTTOM);
				}
			}
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(uuid_channel, event);
				switch_event_fire(&event);
			}
			switch_core_session_rwunlock(uuid_session);
		} else {
			base_set(session, data, urldecode, SWITCH_STACK_BOTTOM);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "multiset with empty args\n");
	}
}

SWITCH_STANDARD_APP(uuid_multiset_function)
{
	kz_uuid_multiset(session, data, 0);
}

SWITCH_STANDARD_APP(uuid_multiset_encoded_function)
{
	kz_uuid_multiset(session, data, 1);
}

void kz_set(switch_core_session_t *session, const char* data, int urldecode) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;

	base_set(session, data, urldecode, SWITCH_STACK_BOTTOM);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_APP(set_function)
{
	kz_set(session, data, 0);
}

SWITCH_STANDARD_APP(set_encoded_function)
{
	kz_set(session, data, 1);
}

SWITCH_STANDARD_APP(unset_function) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
		switch_channel_set_variable(switch_core_session_get_channel(session), data, NULL);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_APP(multiunset_function) {
	char delim = ' ';
	char *arg = (char *) data;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if(delim != '\0') {
		if (arg) {
			char *array[256] = {0};
			int i, argc;

			arg = switch_core_session_strdup(session, arg);
			argc = switch_split(arg, delim, array);

			for(i = 0; i < argc; i++) {
				switch_channel_set_variable(switch_core_session_get_channel(session), array[i], NULL);
			}

		} else {
			switch_channel_set_variable(switch_core_session_get_channel(session), arg, NULL);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "multiunset with empty args\n");
	}
}


void kz_export(switch_core_session_t *session, const char* data, int urldecode)
{
    char delim = ' ';
    char *arg = (char *) data;

    if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
            arg += 2;
            delim = *arg++;
    }

    if(delim != '\0') {
		if (arg) {
				char *array[256] = {0};
				int i, argc;

				arg = switch_core_session_strdup(session, arg);
				argc = switch_split(arg, delim, array);

				for(i = 0; i < argc; i++) {
						base_export(session, array[i], urldecode, SWITCH_STACK_BOTTOM);
				}
		} else {
				base_export(session, data, urldecode, SWITCH_STACK_BOTTOM);
		}
    } else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "export with empty args\n");
    }
}

SWITCH_STANDARD_APP(export_function)
{
	kz_export(session, data, 0);
}

SWITCH_STANDARD_APP(export_encoded_function)
{
	kz_export(session, data, 1);
}

// copied from mod_dptools with allow SWITCH_STATUS_BREAK
SWITCH_STANDARD_APP(kz_endless_playback_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *file = data;

	while (switch_channel_ready(channel)) {
		status = switch_ivr_play_file(session, NULL, file, NULL);

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}
	}

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
		break;
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK_INTERRUPTED");
		break;
	case SWITCH_STATUS_NOTFOUND:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE NOT FOUND");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK ERROR");
		break;
	}

}

SWITCH_STANDARD_APP(noop_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	const char *response = uuid_str;

	if (zstr(data)) {
		switch_uuid_str(uuid_str, sizeof(uuid_str));
	} else {
		response = data;
	}

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, response);
}

SWITCH_STANDARD_APP(kz_restore_caller_id_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *cp = switch_channel_get_caller_profile(channel);
	cp->caller_id_name = cp->orig_caller_id_name;
	cp->caller_id_number = cp->orig_caller_id_number;
}

SWITCH_STANDARD_APP(kz_audio_bridge_function)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_core_session_t *peer_session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (zstr(data)) {
		return;
	}

	status = switch_ivr_originate(session, &peer_session, &cause, data, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", switch_channel_cause2str(cause));

		switch_channel_set_variable(caller_channel, "originate_failed_cause", switch_channel_cause2str(cause));
		switch_channel_set_variable(caller_channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, switch_channel_cause2str(cause));
		switch_channel_handle_cause(caller_channel, cause);

		return;
	} else {
		const char* uuid = switch_core_session_get_uuid(session);
		const char* peer_uuid = switch_core_session_get_uuid(peer_session);


		switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
		if (switch_true(switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE)) ||
			switch_true(switch_channel_get_variable(peer_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE))) {
			switch_channel_set_flag(caller_channel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
		}

		while(1) {
			const char *xfer_uuid;
			switch_channel_state_t a_state , a_running_state;
			switch_channel_state_t b_state , b_running_state;
			status = switch_ivr_multi_threaded_bridge(session, peer_session, NULL, NULL, NULL);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BRIDGE RESULT %i\n", status);
			if(status  != 0) {
				break;
			}

			a_state = switch_channel_get_state(caller_channel);
			a_running_state = switch_channel_get_running_state(caller_channel);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "A STATE %s  %s =>  %s , %s\n", switch_channel_state_name(a_running_state), switch_channel_state_name(a_state), uuid, peer_uuid);

			if(a_state >= CS_HANGUP) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "A HANGUP =  %s , %s\n", uuid, peer_uuid);
				break;
			}

			b_state = switch_channel_get_state(peer_channel);
			b_running_state = switch_channel_get_running_state(peer_channel);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "B STATE %s  %s =>  %s , %s\n", switch_channel_state_name(b_running_state), switch_channel_state_name(b_state), uuid, peer_uuid);

			if(b_state >= CS_HANGUP) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "B HANGUP = %s , %s\n", uuid, peer_uuid);
				switch_channel_set_variable(caller_channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, switch_channel_cause2str(switch_channel_get_cause(peer_channel)));
				break;
			}

			if(!(xfer_uuid=switch_channel_get_variable(caller_channel, "att_xfer_peer_uuid"))) {
				if(!(xfer_uuid=switch_channel_get_variable(peer_channel, "att_xfer_peer_uuid"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "XFER UUID NULL\n");
					break;
				}
			}

			switch_channel_set_variable(caller_channel, "att_xfer_peer_uuid", NULL);
			switch_channel_set_variable(peer_channel, "att_xfer_peer_uuid", NULL);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "WAIT 1\n");

			switch_channel_clear_flag(peer_channel, CF_UUID_BRIDGE_ORIGINATOR);
			switch_channel_set_state(peer_channel, CS_RESET);
			switch_channel_wait_for_state(peer_channel, NULL, CS_RESET);
			switch_channel_clear_state_handler(peer_channel, NULL);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "WAIT 3\n");

			switch_channel_set_flag(caller_channel, CF_UUID_BRIDGE_ORIGINATOR);
			switch_channel_clear_flag(caller_channel, CF_TRANSFER);
			switch_channel_clear_flag(caller_channel, CF_REDIRECT);
			switch_channel_set_flag(peer_channel, CF_UUID_BRIDGE_ORIGINATOR);
			switch_channel_clear_flag(peer_channel, CF_TRANSFER);
			switch_channel_clear_flag(peer_channel, CF_REDIRECT);

			if(!switch_channel_media_up(caller_channel)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "A MEDIA DOWN HANGUP = %s, %s , %s\n", xfer_uuid, uuid, peer_uuid);
			}
			if(!switch_channel_media_up(peer_channel)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "B MEDIA DOWN HANGUP = %s, %s , %s\n", xfer_uuid, uuid, peer_uuid);
			}
			switch_channel_set_state(caller_channel, CS_EXECUTE);
			switch_channel_set_state(peer_channel, CS_EXECUTE);


			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "XFER LOOP %s %s , %s\n", xfer_uuid, uuid, peer_uuid);

		}

		if (peer_session) {
			switch_core_session_rwunlock(peer_session);
		}
	}
}

void add_kz_dptools(switch_loadable_module_interface_t **module_interface, switch_application_interface_t *app_interface) {
	SWITCH_ADD_APP(app_interface, "kz_set", SET_SHORT_DESC, SET_LONG_DESC, set_function, SET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_set_encoded", SET_SHORT_DESC, SET_LONG_DESC, set_encoded_function, SET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_multiset", MULTISET_SHORT_DESC, MULTISET_LONG_DESC, multiset_function, MULTISET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_multiset_encoded", MULTISET_SHORT_DESC, MULTISET_LONG_DESC, multiset_encoded_function, MULTISET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_unset", UNSET_SHORT_DESC, UNSET_LONG_DESC, unset_function, UNSET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_multiunset", MULTISET_SHORT_DESC, MULTISET_LONG_DESC, multiunset_function, MULTIUNSET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_export", EXPORT_SHORT_DESC, EXPORT_LONG_DESC, export_function, EXPORT_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_export_encoded", EXPORT_SHORT_DESC, EXPORT_LONG_DESC, export_encoded_function, EXPORT_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_prefix_unset", PREFIX_UNSET_SHORT_DESC, PREFIX_UNSET_LONG_DESC, prefix_unset_function, PREFIX_UNSET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_uuid_multiset", UUID_MULTISET_SHORT_DESC, UUID_MULTISET_LONG_DESC, uuid_multiset_function, UUID_MULTISET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_uuid_multiset_encoded", UUID_MULTISET_SHORT_DESC, UUID_MULTISET_LONG_DESC, uuid_multiset_encoded_function, UUID_MULTISET_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_endless_playback", KZ_ENDLESS_PLAYBACK_SHORT_DESC, KZ_ENDLESS_PLAYBACK_LONG_DESC, kz_endless_playback_function, KZ_ENDLESS_PLAYBACK_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "kz_restore_caller_id", NOOP_SHORT_DESC, NOOP_LONG_DESC, kz_restore_caller_id_function, NOOP_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "noop", NOOP_SHORT_DESC, NOOP_LONG_DESC, noop_function, NOOP_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "kz_bridge", "Bridge Audio", "Bridge the audio between two sessions", kz_audio_bridge_function, "<channel_url>", SAF_SUPPORT_NOMEDIA|SAF_SUPPORT_TEXT_ONLY);
}
