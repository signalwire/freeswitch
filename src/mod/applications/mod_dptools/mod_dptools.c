/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Ken Rice <krice@freeswitch.org>
 * Michael Murdock <mike at mmurdock dot org>
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter AT 0xdecafbad dot com>
 * Luke Dashjr <luke@openmethods.com> (OpenMethods, LLC)
 * Cesar Cepeda <cesar@auronix.com>
 * Christopher M. Rienzo <chris@rienzo.com>
 * Seven Du <dujinfang@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_dptools.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dptools_shutdown);
SWITCH_MODULE_DEFINITION(mod_dptools, mod_dptools_load, mod_dptools_shutdown, NULL);

SWITCH_STANDARD_DIALPLAN(inline_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	char *argv[128] = { 0 };
	int argc;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int x = 0;
	char *lbuf;
	char *target = arg;
	char delim = ',';

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	if ((extension = switch_caller_extension_new(session, "inline", "inline")) == 0) {
		abort();
	}

	if (zstr(target)) {
		target = caller_profile->destination_number;
	}


	if (zstr(target)) {
		return NULL;
	} else {
		lbuf = switch_core_session_strdup(session, target);
	}

	if (*lbuf == 'm' && *(lbuf + 1) == ':' && *(lbuf + 3) == ':') {
		delim = *(lbuf + 2);
		lbuf += 4;
	}

	argc = switch_separate_string(lbuf, delim, argv, (sizeof(argv) / sizeof(argv[0])));

	for (x = 0; x < argc; x++) {
		char *app = argv[x];
		char *data = strchr(app, ':');

		if (data) {
			*data++ = '\0';
		}

		while (*app == ' ') {
			app++;
		}

		switch_caller_extension_add_application(session, extension, app, data);
	}

	caller_profile->destination_number = (char *) caller_profile->rdnis;
	caller_profile->rdnis = SWITCH_BLANK_STRING;

	return extension;
}

struct action_binding {
	char *realm;
	char *input;
	char *string;
	char *value;
	switch_digit_action_target_t target;
	switch_core_session_t *session;
};

static switch_status_t digit_nomatch_action_callback(switch_ivr_dmachine_match_t *match)
{
	switch_core_session_t *session = (switch_core_session_t *) match->user_data;
	switch_channel_t *channel;
	switch_event_t *event;
	switch_status_t status;
	switch_core_session_t *use_session = session;

	if (switch_ivr_dmachine_get_target(match->dmachine) == DIGIT_TARGET_PEER) {
		if (switch_core_session_get_partner(session, &use_session) != SWITCH_STATUS_SUCCESS) {
			use_session = session;
		}
	}

	channel = switch_core_session_get_channel(use_session);


	switch_channel_set_variable(channel, "last_non_matching_digits", match->match_digits);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(use_session), SWITCH_LOG_DEBUG, "%s Digit NOT match binding [%s]\n", 
					  switch_channel_get_name(channel), match->match_digits);

	if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "digits", match->match_digits);

		if ((status = switch_core_session_queue_event(use_session, &event)) != SWITCH_STATUS_SUCCESS) {
			switch_event_destroy(&event);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(use_session), SWITCH_LOG_WARNING, "%s event queue failure.\n", 
							  switch_core_session_get_name(use_session));
		}
	}

	/* send it back around and skip the dmachine */
	switch_channel_queue_dtmf_string(channel, match->match_digits);
	
	if (use_session != session) {
		switch_core_session_rwunlock(use_session);
	}


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t digit_action_callback(switch_ivr_dmachine_match_t *match)
{
	struct action_binding *act = (struct action_binding *) match->user_data;
	switch_event_t *event;
	switch_status_t status;
	int exec = 0;
	int api = 0;
	char *string = act->string;
	switch_channel_t *channel;
	switch_core_session_t *use_session = act->session;
	int x = 0;
	char *flags = "";

	if (switch_ivr_dmachine_get_target(match->dmachine) == DIGIT_TARGET_PEER || act->target == DIGIT_TARGET_PEER || act->target == DIGIT_TARGET_BOTH) {
		if (switch_core_session_get_partner(act->session, &use_session) != SWITCH_STATUS_SUCCESS) {
			use_session = act->session;
		}
	}

 top:
	x++;

	string = switch_core_session_strdup(use_session, act->string);
	exec = 0;
	api = 0;

	channel = switch_core_session_get_channel(use_session);

	switch_channel_set_variable(channel, "last_matching_digits", match->match_digits);

	
	if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(act->session), SWITCH_LOG_DEBUG, "%s Digit match binding [%s][%s]\n", 
						  switch_channel_get_name(channel), act->string, act->value);

		if (!strncasecmp(string, "exec", 4)) {
			char *e;
			
			string += 4;
			if (*string == ':') {
				string++;
				exec = 1;
			} else if (*string == '[') {
				flags = string;
				if ((e = switch_find_end_paren(flags, '[', ']'))) {
					if (e && *++e == ':') {
						flags++;
						*e++ = '\0';
						string = e;
						exec = strchr(flags, 'i') ? 2 : 1;
					}
				}
			}
		} else if (!strncasecmp(string, "api:", 4)) {
			string += 4;
			api = 1;
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, string, act->value);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "digits", match->match_digits);

		if (exec) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "execute", exec == 1 ? "non-blocking" : "blocking");
		}

		if ((status = switch_core_session_queue_event(use_session, &event)) != SWITCH_STATUS_SUCCESS) {
			switch_event_destroy(&event);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(use_session), SWITCH_LOG_WARNING, "%s event queue failure.\n", 
							  switch_core_session_get_name(use_session));
		}
	}

	if (exec) {
		if (exec == 2) {
			switch_core_session_execute_application(use_session, string, act->value);
		} else {
			char *cmd = switch_core_session_sprintf(use_session, "%s::%s", string, act->value);
			switch_media_flag_enum_t exec_flags = SMF_ECHO_ALEG;

			if (act->target != DIGIT_TARGET_BOTH && !strchr(flags, 'H')) {
				exec_flags |= SMF_HOLD_BLEG;
			}

			switch_ivr_broadcast_in_thread(use_session, cmd, exec_flags);
		}
	} else if (api) {
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(string, act->value, NULL, &stream);
		if (stream.data) {
			switch_channel_set_variable(channel, "bind_digit_action_api_result", (char *)stream.data);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(act->session), SWITCH_LOG_DEBUG, "%s Digit match binding [%s][%s] api executed, %s\n", 
				switch_core_session_get_name(use_session), act->string, act->value, (char *)stream.data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(act->session), SWITCH_LOG_DEBUG, "%s Digit match binding [%s][%s] api executed\n",
				switch_core_session_get_name(use_session), act->string, act->value);
		}
		switch_safe_free(stream.data);
	}
	

	if (use_session != act->session) {
		switch_core_session_rwunlock(use_session);

		if (act->target == DIGIT_TARGET_BOTH) {
			use_session = act->session;
			goto top;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_digit_action_target_t str2target(const char *target_str)
{
	if (!strcasecmp(target_str, "peer")) {
		return DIGIT_TARGET_PEER;
	}

	if (!strcasecmp(target_str, "both")) {
		return DIGIT_TARGET_BOTH;
	}
	
	return DIGIT_TARGET_SELF;
}

#define CLEAR_DIGIT_ACTION_USAGE "<realm>|all[,target]"
SWITCH_STANDARD_APP(clear_digit_action_function)
{
	//switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_ivr_dmachine_t *dmachine;
	char *realm = NULL;
	char *target_str;
	switch_digit_action_target_t target = DIGIT_TARGET_SELF;

	if (zstr((char *)data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "clear_digit_action called with no args");
		return;
	}

	realm = switch_core_session_strdup(session, data);

	if ((target_str = strchr(realm, ','))) {
		*target_str++ = '\0';
		target = str2target(target_str);
	}


	if ((dmachine = switch_core_session_get_dmachine(session, target))) {
		if (zstr(realm) || !strcasecmp(realm, "all")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Digit parser %s: Clearing all realms\n", switch_ivr_dmachine_get_name(dmachine));
			switch_core_session_set_dmachine(session, NULL, target);
			switch_ivr_dmachine_destroy(&dmachine);
		} else {
			switch_ivr_dmachine_clear_realm(dmachine, realm);
		}
	}
}

#define DIGIT_ACTION_SET_REALM_USAGE "<realm>[,<target>]"
SWITCH_STANDARD_APP(digit_action_set_realm_function)
{
	switch_ivr_dmachine_t *dmachine;
	char *realm = switch_core_session_strdup(session, data);
	char *target_str;
	switch_digit_action_target_t target = DIGIT_TARGET_SELF;

	if ((target_str = strchr(realm, ','))) {
		*target_str++ = '\0';
		target = str2target(target_str);
	}

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Syntax Error, USAGE %s\n", DIGIT_ACTION_SET_REALM_USAGE);
		return;
	}

	
	if ((dmachine = switch_core_session_get_dmachine(session, target))) {
		switch_ivr_dmachine_set_realm(dmachine, realm);
	}

}


static void bind_to_session(switch_core_session_t *session, 
							const char *arg0, const char *arg1, const char *arg2, const char *arg3,
							switch_digit_action_target_t target, switch_digit_action_target_t bind_target)
{
	struct action_binding *act;
	switch_ivr_dmachine_t *dmachine;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *terminators = NULL;

	if (!(dmachine = switch_core_session_get_dmachine(session, target))) {
		uint32_t digit_timeout = 1500;
		uint32_t input_timeout = 0;
		const char *var;

		if ((var = switch_channel_get_variable(channel, "bind_digit_digit_timeout"))) {
			digit_timeout = switch_atoul(var);
		}
		
		if ((var = switch_channel_get_variable(channel, "bind_digit_input_timeout"))) {
			input_timeout = switch_atoul(var);
		}
		
		switch_ivr_dmachine_create(&dmachine, "DPTOOLS", NULL, digit_timeout, input_timeout, NULL, digit_nomatch_action_callback, session);
		switch_core_session_set_dmachine(session, dmachine, target);
	}

	
	act = switch_core_session_alloc(session, sizeof(*act));
	act->realm = switch_core_session_strdup(session, arg0);
	act->input = switch_core_session_strdup(session, arg1);
	act->string = switch_core_session_strdup(session, arg2);
	act->value = switch_core_session_strdup(session, arg3);
	act->target = bind_target;
	act->session = session;
	switch_ivr_dmachine_bind(dmachine, act->realm, act->input, 0, digit_action_callback, act);

	if ((terminators = switch_channel_get_variable(channel, "bda_terminators"))) {
		switch_ivr_dmachine_set_terminators(dmachine, terminators);
	}
}

#define BIND_DIGIT_ACTION_USAGE "<realm>,<digits|~regex>,<string>[,<value>][,<dtmf target leg>][,<event target leg>]"
SWITCH_STANDARD_APP(bind_digit_action_function)
{

	char *mydata;
	int argc = 0;
	char *argv[6] = { 0 };
	switch_digit_action_target_t target, bind_target;
	char *target_str = "self", *bind_target_str = "self";
	char *value = "";

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Syntax Error, USAGE %s\n", BIND_DIGIT_ACTION_USAGE);
		return;
	}

	mydata = switch_core_session_strdup(session, data);

	argc = switch_separate_string(mydata, ',', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 3 || zstr(argv[0]) || zstr(argv[1]) || zstr(argv[2])) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Syntax Error, USAGE %s\n", BIND_DIGIT_ACTION_USAGE);
		return;
	}

	if (argv[3]) {
		value = argv[3];
	}

	if (argv[4]) {
		target_str = argv[4];
	}

	if (argv[5]) {
		bind_target_str = argv[5];
	}

	target = str2target(target_str);
	bind_target = str2target(bind_target_str);


	switch(target) {
	case DIGIT_TARGET_PEER:
		bind_to_session(session, argv[0], argv[1], argv[2], value, DIGIT_TARGET_PEER, bind_target);
		break;
	case DIGIT_TARGET_BOTH:
		bind_to_session(session, argv[0], argv[1], argv[2], value, DIGIT_TARGET_PEER, bind_target);
		bind_to_session(session, argv[0], argv[1], argv[2], value, DIGIT_TARGET_SELF, bind_target);
		break;
	default:
		bind_to_session(session, argv[0], argv[1], argv[2], value, DIGIT_TARGET_SELF, bind_target);
		break;
	}
}

#define DETECT_SPEECH_SYNTAX "<mod_name> <gram_name> <gram_path> [<addr>] OR grammar <gram_name> [<path>] OR nogrammar <gram_name> OR grammaron/grammaroff <gram_name> OR grammarsalloff OR pause OR resume OR start_input_timers OR stop OR param <name> <value>"
SWITCH_STANDARD_APP(detect_speech_function)
{
	char *argv[4];
	int argc;
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (!strcasecmp(argv[0], "grammar") && argc >= 1) {
			switch_ivr_detect_speech_load_grammar(session, argv[1], argv[2]);
		} else if (!strcasecmp(argv[0], "nogrammar")) {
			switch_ivr_detect_speech_unload_grammar(session, argv[1]);
		} else if (!strcasecmp(argv[0], "grammaron")) {
			switch_ivr_detect_speech_enable_grammar(session, argv[1]);
		} else if (!strcasecmp(argv[0], "grammaroff")) {
			switch_ivr_detect_speech_disable_grammar(session, argv[1]);
		} else if (!strcasecmp(argv[0], "grammarsalloff")) {
			switch_ivr_detect_speech_disable_all_grammars(session);
		} else if (!strcasecmp(argv[0], "init")) {
			switch_ivr_detect_speech_init(session, argv[1], argv[2], NULL);
		} else if (!strcasecmp(argv[0], "pause")) {
			switch_ivr_pause_detect_speech(session);
		} else if (!strcasecmp(argv[0], "resume")) {
			switch_ivr_resume_detect_speech(session);
		} else if (!strcasecmp(argv[0], "stop")) {
			switch_ivr_stop_detect_speech(session);
		} else if (!strcasecmp(argv[0], "param")) {
			switch_ivr_set_param_detect_speech(session, argv[1], argv[2]);
		} else if (!strcasecmp(argv[0], "start_input_timers")) {
			switch_ivr_detect_speech_start_input_timers(session);
		} else if (argc >= 3) {
			switch_ivr_detect_speech(session, argv[0], argv[1], argv[2], argv[3], NULL);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", DETECT_SPEECH_SYNTAX);
	}
}

#define PLAY_AND_DETECT_SPEECH_SYNTAX "<file> detect:<engine> {param1=val1,param2=val2}<grammar>"
SWITCH_STANDARD_APP(play_and_detect_speech_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *argv[2];
	char *lbuf = NULL;
	const char *response = "DONE";
	char *detect = NULL;
	char *s;

	switch_channel_set_variable(channel, "detect_speech_result", "");

	if (zstr(data) || !(lbuf = switch_core_session_strdup(session, data)) || !(detect = strstr(lbuf, "detect:"))) {
		/* bad input */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", PLAY_AND_DETECT_SPEECH_SYNTAX);
		response = "USAGE ERROR";
		goto done;
	}

	/* trim any trailing space */
	s = detect;
	while (--s >= lbuf && switch_isspace(*s)) {
		*s = '\0';
	}

	/* split input at "detect:" */
	detect[0] = '\0';
	detect += 7;
	if (zstr(detect)) {
		/* bad input */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", PLAY_AND_DETECT_SPEECH_SYNTAX);
		response = "USAGE ERROR";
		goto done;
	}

	/* need to have at 2 parameters for detect */
	if (switch_separate_string(detect, ' ', argv, (sizeof(argv) / sizeof(argv[0]))) == 2) {
		char *file = lbuf;
		char *engine = argv[0];
		char *grammar = argv[1];
		char *result = NULL;
		switch_status_t status = switch_ivr_play_and_detect_speech(session, file, engine, grammar, &result, 0, NULL);
		if (status == SWITCH_STATUS_SUCCESS) {
			if (!zstr(result)) {
				switch_channel_set_variable(channel, "detect_speech_result", result);
			}
		} else if (status == SWITCH_STATUS_GENERR) {
			response = "GRAMMAR ERROR";
		} else if (status == SWITCH_STATUS_NOT_INITALIZED) {
			response = "ASR INIT ERROR";
		} else {
			response = "ERROR";
		}
	} else {
		/* bad input */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", PLAY_AND_DETECT_SPEECH_SYNTAX);
		response = "USAGE ERROR";
	}

done:
	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, response);
}

#define SCHED_HEARTBEAT_SYNTAX "[0|<seconds>]"
SWITCH_STANDARD_APP(sched_heartbeat_function)
{
	int seconds = 0;

	if (data) {
		seconds = atoi(data);
		if (seconds >= 0) {
			switch_core_session_sched_heartbeat(session, seconds);
			return;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", SCHED_HEARTBEAT_SYNTAX);

}


#define HEARTBEAT_SYNTAX "[0|<seconds>]"
SWITCH_STANDARD_APP(heartbeat_function)
{
	int seconds = 0;

	if (data) {
		seconds = atoi(data);
		if (seconds >= 0) {

			switch_core_session_enable_heartbeat(session, seconds);
			return;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", HEARTBEAT_SYNTAX);

}


#define KEEPALIVE_SYNTAX "[0|<seconds>]"
SWITCH_STANDARD_APP(keepalive_function)
{
	int seconds = 0;

	if (data) {
		seconds = atoi(data);
		if (seconds >= 0) {
			switch_core_session_message_t msg = { 0 };

			msg.message_id = SWITCH_MESSAGE_INDICATE_KEEPALIVE;
			msg.numeric_arg = seconds;
			switch_core_session_receive_message(session, &msg);
			
			switch_core_session_enable_heartbeat(session, seconds);
			return;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", HEARTBEAT_SYNTAX);

}

#define EXE_SYNTAX "<extension> <dialplan> <context>"
SWITCH_STANDARD_APP(exe_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_core_session_execute_exten(session, argv[0], argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", EXE_SYNTAX);
	}
}

#define MKDIR_SYNTAX "<path>"
SWITCH_STANDARD_APP(mkdir_function)
{
	switch_dir_make_recursive(data, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s MKDIR: %s\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)), data);
}
#define RENAME_SYNTAX "<from_path> <to_path>"
SWITCH_STANDARD_APP(rename_function)
{
	char *argv[2] = { 0 };
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& switch_split(lbuf, ' ', argv) == 2) {
		switch_file_rename(argv[0], argv[1], switch_core_session_get_pool(session));
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RENAME: %s %s\n",
						  switch_channel_get_name(switch_core_session_get_channel(session)), argv[0], argv[1]);
	
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", RENAME_SYNTAX);
	}
}

#define TRANSFER_VARS_SYNTAX "<~variable_prefix|variable>"
SWITCH_STANDARD_APP(transfer_vars_function)
{
	char *argv[1] = { 0 };
	int argc;
	char *lbuf = NULL;
	
	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
		switch_core_session_t *nsession = NULL;

		switch_core_session_get_partner(session, &nsession);

		if (nsession) {
			switch_ivr_transfer_variable(session, nsession, argv[0]);
			switch_core_session_rwunlock(nsession);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", TRANSFER_VARS_SYNTAX);
		}
	}
}

#define SOFT_HOLD_SYNTAX "<unhold key> [<moh_a>] [<moh_b>]"
SWITCH_STANDARD_APP(soft_hold_function)
{
	char *argv[3] = { 0 };
	int argc;
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
		switch_ivr_soft_hold(session, argv[0], argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", SOFT_HOLD_SYNTAX);
	}
}

SWITCH_STANDARD_APP(dtmf_unblock_function)
{
	switch_ivr_unblock_dtmf_session(session);
}

SWITCH_STANDARD_APP(media_reset_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *name = switch_channel_get_name(channel);

	if (switch_channel_media_ready(channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s This function does not work once media has been established.\n", name);
		return;
	}

	switch_channel_clear_flag(channel, CF_PROXY_MODE);
	switch_channel_clear_flag(channel, CF_PROXY_MEDIA);
	switch_channel_set_variable(channel, "bypass_media", NULL);
	switch_channel_set_variable(channel, "proxy_media", NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%sReset MEDIA flags.\n", name);
}

SWITCH_STANDARD_APP(dtmf_block_function)
{
	switch_ivr_block_dtmf_session(session);
}

#define UNBIND_SYNTAX "[<key>]"
SWITCH_STANDARD_APP(dtmf_unbind_function)
{
	char *key = (char *) data;
	int kval = 0;

	if (key) {
		kval = switch_dtmftoi(key);
	}

	switch_ivr_unbind_dtmf_meta_session(session, kval);

}

#define BIND_SYNTAX "<key> [a|b|ab] [a|b|o|s|i|1] <app>"
SWITCH_STANDARD_APP(dtmf_bind_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) == 4) {
		int kval = switch_dtmftoi(argv[0]);
		switch_bind_flag_t bind_flags = 0;

		if (strchr(argv[1], 'a')) {
			bind_flags |= SBF_DIAL_ALEG;
		}

		if (strchr(argv[1], 'b')) {
			bind_flags |= SBF_DIAL_BLEG;
		}

		if (strchr(argv[2], 'a')) {
			if ((bind_flags & SBF_EXEC_BLEG)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot bind execute to multiple legs\n");
			} else {
				bind_flags |= SBF_EXEC_ALEG;
			}
		}

		if (strchr(argv[2], 'b')) {
			if ((bind_flags & SBF_EXEC_ALEG)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot bind execute to multiple legs\n");
			} else {
				bind_flags |= SBF_EXEC_BLEG;
			}
		}

		if (strchr(argv[2], 'a')) {
			if ((bind_flags & SBF_EXEC_BLEG)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot bind execute to multiple legs\n");
			} else {
				bind_flags |= SBF_EXEC_ALEG;
			}
		}

		if (strchr(argv[2], 'i')) {
			bind_flags |= SBF_EXEC_INLINE;
		}

		if (strchr(argv[2], 'o')) {
			if ((bind_flags & SBF_EXEC_BLEG) || (bind_flags & SBF_EXEC_ALEG) || (bind_flags & SBF_EXEC_SAME)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot bind execute to multiple legs\n");
			} else {
				bind_flags |= SBF_EXEC_OPPOSITE;
			}
		}

		if (strchr(argv[2], 's')) {
			if ((bind_flags & SBF_EXEC_BLEG) || (bind_flags & SBF_EXEC_ALEG) || (bind_flags & SBF_EXEC_SAME)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot bind execute to multiple legs\n");
			} else {
				bind_flags |= SBF_EXEC_SAME;
			}
		}

		if (strchr(argv[2], '1')) {
			bind_flags |= SBF_ONCE;
		}

		if (switch_ivr_bind_dtmf_meta_session(session, kval, bind_flags, argv[3]) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Bind Error!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", BIND_SYNTAX);
	}
}

#define INTERCEPT_SYNTAX "[-bleg] <uuid>"
SWITCH_STANDARD_APP(intercept_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;
	char *uuid;
	switch_bool_t bleg = SWITCH_FALSE;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			if (!strcasecmp(argv[0], "-bleg")) {
				if (argv[1]) {
					uuid = argv[1];
					bleg = SWITCH_TRUE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", INTERCEPT_SYNTAX);
					return;
				}
			} else {
				uuid = argv[0];
			}

			switch_ivr_intercept_session(session, uuid, bleg);
		}
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", INTERCEPT_SYNTAX);
}

#define MAX_SPY 3000
struct e_data {
	char *uuid_list[MAX_SPY];
	int total;
};

static int e_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char *uuid = argv[0];
	struct e_data *e_data = (struct e_data *) pArg;

	if (uuid && e_data) {
		e_data->uuid_list[e_data->total++] = strdup(uuid);
		return 0;
	}

	return 1;
}

#define eavesdrop_SYNTAX "[all | <uuid>]"
SWITCH_STANDARD_APP(eavesdrop_function)
{
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", eavesdrop_SYNTAX);
	} else {
		switch_eavesdrop_flag_t flags = ED_DTMF;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *require_group = switch_channel_get_variable(channel, "eavesdrop_require_group");
		const char *enable_dtmf = switch_channel_get_variable(channel, "eavesdrop_enable_dtmf");
		const char *bridge_aleg = switch_channel_get_variable(channel, "eavesdrop_bridge_aleg");
		const char *bridge_bleg = switch_channel_get_variable(channel, "eavesdrop_bridge_bleg");
		const char *whisper_aleg = switch_channel_get_variable(channel, "eavesdrop_whisper_aleg");
		const char *whisper_bleg = switch_channel_get_variable(channel, "eavesdrop_whisper_bleg");

		if (enable_dtmf) {
			flags = switch_true(enable_dtmf) ? ED_DTMF : ED_NONE;
		}

		if (switch_true(whisper_aleg)) {
			flags |= ED_MUX_READ;
		}
		if (switch_true(whisper_bleg)) {
			flags |= ED_MUX_WRITE;
		}

		/* Defaults to both, if neither is set */
		if (switch_true(bridge_aleg)) {
			flags |= ED_BRIDGE_READ;
		}
		if (switch_true(bridge_bleg)) {
			flags |= ED_BRIDGE_WRITE;
		}

		if (!strcasecmp((char *) data, "all")) {
			switch_cache_db_handle_t *db = NULL;
			char *errmsg = NULL;
			struct e_data e_data = { {0} };
			char *sql = switch_mprintf("select uuid from channels where uuid != '%q'", switch_core_session_get_uuid(session));
			const char *file = NULL;
			int x = 0;
			char buf[2] = "";
			switch_size_t buflen = sizeof(buf);
			char terminator;
			switch_status_t status;

			while (switch_channel_ready(channel)) {
				for (x = 0; x < MAX_SPY; x++) {
					switch_safe_free(e_data.uuid_list[x]);
				}
				e_data.total = 0;
				
				if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Database Error!\n");
					break;
				}
				switch_cache_db_execute_sql_callback(db, sql, e_callback, &e_data, &errmsg);
				switch_cache_db_release_db_handle(&db);
				if (errmsg) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Error: %s\n", errmsg);
					free(errmsg);
					if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_failed"))) {
						switch_ivr_play_file(session, NULL, file, NULL);
					}
					switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 5000, 0, 0);
					continue;
				}
				if (e_data.total) {
					for (x = 0; x < e_data.total && switch_channel_ready(channel); x++) {
						if (!switch_ivr_uuid_exists(e_data.uuid_list[x])) continue;

						/* If we have a group and 1000 concurrent calls, we will flood the logs. This check avoids this */
						if (!require_group)
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Spy: %s\n", e_data.uuid_list[x]);
						if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_new"))) {
							switch_ivr_play_file(session, NULL, file, NULL);
						}
						if ((status = switch_ivr_eavesdrop_session(session, e_data.uuid_list[x], require_group, flags)) != SWITCH_STATUS_SUCCESS) {
							if (status != SWITCH_STATUS_BREAK) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Spy: %s Failed\n", e_data.uuid_list[x]);
								if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_failed"))) {
									switch_ivr_play_file(session, NULL, file, NULL);
								}
								switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 5000, 0, 0);
							}
						}
					}
				} else {
					if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_idle"))) {
						switch_ivr_play_file(session, NULL, file, NULL);
					}
					switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 2000, 0, 0);
				}
			}

			for (x = 0; x < MAX_SPY; x++) {
				switch_safe_free(e_data.uuid_list[x]);
			}

			free(sql);

		} else {
			switch_ivr_eavesdrop_session(session, data, require_group, flags);
		}
	}
}

#define threeway_SYNTAX "<uuid>"
SWITCH_STANDARD_APP(three_way_function)
{
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", threeway_SYNTAX);
	} else {
		switch_ivr_eavesdrop_session(session, data, NULL, ED_MUX_READ | ED_MUX_WRITE);
	}
}

#define SET_USER_SYNTAX "<user>@<domain> [prefix]"
SWITCH_STANDARD_APP(set_user_function)
{
	switch_ivr_set_user(session, data);
}

#define SET_AUDIO_LEVEL_SYNTAX "[read|write] <vol>"
SWITCH_STANDARD_APP(set_audio_level_function)
{
	char *argv[2] = { 0 };
	int argc = 0;
	char *mydata;
	int level = 0;

	mydata = switch_core_session_strdup(session, data);
	argc = switch_split(mydata, ' ', argv);

	if (argc != 2) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Error. USAGE: %s\n",
						  switch_core_session_get_name(session), SET_AUDIO_LEVEL_SYNTAX);
		return;
	}

	level = atoi(argv[1]);

	switch_ivr_session_audio(session, "level", argv[0], level);


}

#define SET_MUTE_SYNTAX "[read|write] [[true|cn level]|false]"
SWITCH_STANDARD_APP(set_mute_function)
{
	char *argv[2] = { 0 };
	int argc = 0;
	char *mydata;
	int level = 0;

	mydata = switch_core_session_strdup(session, data);
	argc = switch_split(mydata, ' ', argv);

	if (argc != 2) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Error. USAGE: %s\n",
						  switch_core_session_get_name(session), SET_MUTE_SYNTAX);
		return;
	}

	if ((level = atoi(argv[1])) <= 0) {
		level = switch_true(argv[1]);
	}

	switch_ivr_session_audio(session, "mute", argv[0], level);

}


SWITCH_STANDARD_APP(ring_ready_function)
{
	if (!zstr(data)) {
		if (!strcasecmp(data, "queued")) {
			switch_channel_ring_ready_value(switch_core_session_get_channel(session), SWITCH_RING_READY_QUEUED);
			return;
		}
	}
	
	switch_channel_ring_ready(switch_core_session_get_channel(session));
}

SWITCH_STANDARD_APP(remove_bugs_function)
{
	const char *function = NULL;

	if (!zstr((char *)data)) {
		function = data;
	}

	switch_core_media_bug_remove_all_function(session, function);
}

SWITCH_STANDARD_APP(break_function)
{
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);

	if (data && strcasecmp(data, "all")) {
		switch_core_session_flush_private_events(session);
	}

	if (switch_channel_test_flag(channel, CF_BROADCAST)) {
		switch_channel_stop_broadcast(channel);
	} else {
		switch_channel_set_flag(channel, CF_BREAK);
	}
}

SWITCH_STANDARD_APP(queue_dtmf_function)
{
	switch_channel_queue_dtmf_string(switch_core_session_get_channel(session), (const char *) data);
}

SWITCH_STANDARD_APP(send_dtmf_function)
{
	switch_core_session_send_dtmf_string(session, (const char *) data);
}

SWITCH_STANDARD_APP(check_acl_function)
{
	int argc;
	char *argv[3] = { 0 };
	char *mydata;
	switch_call_cause_t cause = SWITCH_CAUSE_CALL_REJECTED;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
			if (!switch_check_network_list_ip(argv[0], argv[1])) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				if (argc > 2) {
					cause = switch_channel_str2cause(argv[2]);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Call failed acl check for ip %s on list %s\n", argv[0], argv[1]);
				switch_channel_hangup(channel, cause);
			}
		}
	}

}

SWITCH_STANDARD_APP(flush_dtmf_function)
{
	switch_channel_flush_dtmf(switch_core_session_get_channel(session));
}

SWITCH_STANDARD_APP(transfer_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;
	int bleg = 0, both = 0;


	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			bleg = !strcasecmp(argv[0], "-bleg");
			both = !strcasecmp(argv[0], "-both");

			if (bleg || both) {
				const char *uuid;
				switch_channel_t *channel = switch_core_session_get_channel(session);
				if ((uuid = switch_channel_get_partner_uuid(channel))) {
					switch_core_session_t *b_session;
					if ((b_session = switch_core_session_locate(uuid))) {
						switch_ivr_session_transfer(b_session, argv[1], argv[2], argv[3]);
						switch_core_session_rwunlock(b_session);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No B-leg present.\n");
				}
				if (both) {
					switch_ivr_session_transfer(session, argv[1], argv[2], argv[3]);
				}
			} else {
				switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No extension specified.\n");
		}
	}
}

SWITCH_STANDARD_APP(sched_transfer_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;
			uint32_t id;
			char ids[80] = "";

			if (*argv[0] == '+') {
				when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
			} else {
				when = atol(argv[0]);
			}

			id = switch_ivr_schedule_transfer(when, switch_core_session_get_uuid(session), argv[1], argv[2], argv[3]);
			snprintf(ids, sizeof(ids), "%u", id);
			switch_channel_set_variable(switch_core_session_get_channel(session), "last_sched_id", ids);			
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Args\n");
		}
	}
}

SWITCH_STANDARD_APP(sched_hangup_function)
{
	int argc;
	char *argv[5] = { 0 };
	char *mydata;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			time_t when;
			switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;
			switch_bool_t bleg = SWITCH_FALSE;
			int sec = atol(argv[0] + 1);

			if (*argv[0] == '+') {
				when = switch_epoch_time_now(NULL) + sec;
			} else {
				when = atol(argv[0]);
			}

			if (argv[1]) {
				cause = switch_channel_str2cause(argv[1]);
			}

			if (argv[2] && !strcasecmp(argv[2], "bleg")) {
				bleg = SWITCH_TRUE;
			}

			if (sec == 0) {
				switch_channel_hangup(switch_core_session_get_channel(session), cause);
			} else {
				switch_ivr_schedule_hangup(when, switch_core_session_get_uuid(session), cause, bleg);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No time specified.\n");
		}
	}
}

SWITCH_STANDARD_APP(sched_broadcast_function)
{
	int argc;
	char *argv[6] = { 0 };
	char *mydata;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;
			switch_media_flag_t flags = SMF_NONE;
			uint32_t id;
			char ids[80] = "";

			if (*argv[0] == '@') {
				when = atol(argv[0] + 1);
			} else if (*argv[0] == '+') {
				when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
			} else {
				when = atol(argv[0]);
			}

			if (argv[2]) {
				if (!strcmp(argv[2], "both")) {
					flags |= (SMF_ECHO_ALEG | SMF_ECHO_BLEG);
				} else if (!strcmp(argv[2], "aleg")) {
					flags |= SMF_ECHO_ALEG;
				} else if (!strcmp(argv[2], "bleg")) {
					flags |= SMF_ECHO_BLEG;
				}
			} else {
				flags |= SMF_ECHO_ALEG;
			}

			id = switch_ivr_schedule_broadcast(when, switch_core_session_get_uuid(session), argv[1], flags);
			snprintf(ids, sizeof(ids), "%u", id);
			switch_channel_set_variable(switch_core_session_get_channel(session), "last_sched_id", ids);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Args\n");
		}
	}
}

SWITCH_STANDARD_APP(delay_function)
{
	uint32_t len = 0;

	if (zstr(data)) {
		len = 1000;
	} else {
		len = atoi(data);
	}

	switch_ivr_delay_echo(session, len);
}

SWITCH_STANDARD_APP(eval_function)
{
	return;
}

SWITCH_STANDARD_APP(set_media_stats_function)
{
	switch_core_media_set_stats(session);

	return;
}

SWITCH_STANDARD_APP(zombie_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_channel_up(channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s MMM Brains....\n", switch_channel_get_name(channel));
		switch_channel_set_flag(channel, CF_ZOMBIE_EXEC);
	}

	return;
}


SWITCH_STANDARD_APP(hangup_function)
{
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (!zstr(data)) {
		cause = switch_channel_str2cause(data);
	}

	switch_channel_hangup(switch_core_session_get_channel(session), cause);
}

SWITCH_STANDARD_APP(set_name_function)
{

	if (!zstr(data)) {
		switch_channel_set_name(switch_core_session_get_channel(session), (char *) data);
	}
}

SWITCH_STANDARD_APP(answer_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *arg = (char *) data;

	if (zstr(arg)) {
		arg = switch_channel_get_variable(channel, "answer_flags");
	}

	if (!zstr(arg)) {
		if (switch_stristr("is_conference", arg)) {
			switch_channel_set_flag(channel, CF_CONFERENCE);
		}
	}

	switch_channel_answer(channel);
}

SWITCH_STANDARD_APP(wait_for_answer_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Waiting for answer\n");
	while (!switch_channel_test_flag(channel, CF_ANSWERED) && switch_channel_ready(channel)) {
		switch_ivr_sleep(session, 100, SWITCH_TRUE, NULL);
	}
}

SWITCH_STANDARD_APP(presence_function)
{
	char *argv[6] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (zstr(data) || !(mydata = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	switch_channel_presence(channel, argv[0], argv[1], argv[2]);
}

SWITCH_STANDARD_APP(pre_answer_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_pre_answer(channel);
}

SWITCH_STANDARD_APP(redirect_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to redirect */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
	msg.numeric_arg = 1;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(video_set_decode_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *txt = (char *) data;
	int on = 0, wait = 0;

	if (txt) {
		on = !strcasecmp(txt, "on");
		wait = !strcasecmp(txt, "wait");
	}

	if (data && (on || wait)) {
		switch_channel_set_flag_recursive(channel, CF_VIDEO_DECODED_READ);
		if (wait) {
			switch_core_session_wait_for_video_input_params(session, 10000);
		}
	} else {
		switch_channel_clear_flag_recursive(channel, CF_VIDEO_DECODED_READ);
	}
}

SWITCH_STANDARD_APP(video_refresh_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to refresh video */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(send_info_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to send info */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_INFO;
	msg.string_array_arg[2] = data;

	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(jitterbuffer_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to change the jitter buffer */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_JITTER_BUFFER;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(display_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to change display */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(respond_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to respond the call */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_RESPOND;
	msg.numeric_arg = -1;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(deflect_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to deflect the call */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_DEFLECT;
	switch_core_session_receive_message(session, &msg);
}

SWITCH_STANDARD_APP(recovery_refresh_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to recovery_refresh the call */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_RECOVERY_REFRESH;
	switch_core_session_receive_message(session, &msg);
}


SWITCH_STANDARD_APP(sched_cancel_function)
{
	const char *group = data;

	if (zstr(group)) {
		group = switch_core_session_get_uuid(session);
	}

	if (switch_is_digit_string(group)) {
		int64_t tmp;
		tmp = (uint32_t) atoi(group);
		if (tmp > 0) {
			switch_scheduler_del_task_id((uint32_t) tmp);
		}
	} else {
		switch_scheduler_del_task_group(group);
	}
}

static void base_set (switch_core_session_t *session, const char *data, switch_stack_t stack)
{
	char *var, *val = NULL;
	const char *what = "SET";

	switch (stack) {
	case SWITCH_STACK_PUSH:
		what = "PUSH";
		break;
	case SWITCH_STACK_UNSHIFT:
		what = "UNSHIFT";
		break;
	default:
		break;
	}

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
			expanded = switch_channel_expand_variables(channel, val);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s %s [%s]=[%s]\n", 
						  what, switch_channel_get_name(channel), var, expanded ? expanded : "UNDEF");
						  
		switch_channel_add_variable_var_check(channel, var, expanded, SWITCH_FALSE, stack);

		if (expanded && expanded != val) {
			switch_safe_free(expanded);
		}
	}
}

SWITCH_STANDARD_APP(multiset_function)
{
	char delim = ' ';
	char *arg = (char *) data;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if (arg) {
		char *array[256] = {0};
		int i, argc;

		arg = switch_core_session_strdup(session, arg);
		argc = switch_split(arg, delim, array);
		
		for(i = 0; i < argc; i++) {
			base_set(session, array[i], SWITCH_STACK_BOTTOM);
		}
		

	} else {
		base_set(session, data, SWITCH_STACK_BOTTOM);
	}
}

SWITCH_STANDARD_APP(set_function)
{
	base_set(session, data, SWITCH_STACK_BOTTOM);
}

SWITCH_STANDARD_APP(push_function)
{
	base_set(session, data, SWITCH_STACK_PUSH);
}

SWITCH_STANDARD_APP(unshift_function)
{
	base_set(session, data, SWITCH_STACK_UNSHIFT);
}

SWITCH_STANDARD_APP(set_global_function)
{
	char *var, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = strdup(data);
		switch_assert(var);
		val = strchr(var, '=');

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SET GLOBAL [%s]=[%s]\n", var, val ? val : "UNDEF");
		switch_core_set_variable(var, val);
		free(var);
	}
}

SWITCH_STANDARD_APP(set_profile_var_function)
{
	char *name, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		name = switch_core_session_strdup(session, data);
		val = strchr(name, '=');

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		switch_channel_set_profile_var(switch_core_session_get_channel(session), name, val);
	}
}

SWITCH_STANDARD_APP(export_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *var, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = switch_core_session_strdup(session, data);

		if ((val = strchr(var, '='))) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		switch_channel_export_variable_var_check(channel, var, val, SWITCH_EXPORT_VARS_VARIABLE, SWITCH_FALSE);
	}
}

SWITCH_STANDARD_APP(bridge_export_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *var, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = switch_core_session_strdup(session, data);

		if ((val = strchr(var, '='))) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		switch_channel_export_variable(channel, var, val, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	}
}

SWITCH_STANDARD_APP(unset_function)
{
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
		switch_channel_set_variable(switch_core_session_get_channel(session), data, NULL);
	}
}

SWITCH_STANDARD_APP(multiunset_function)
{
	char delim = ' ';
	char *arg = (char *) data;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

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
}


SWITCH_STANDARD_APP(log_function)
{
	char *level, *log_str;

	if (data && (level = strdup(data))) {
		switch_log_level_t ltype = SWITCH_LOG_DEBUG;

		if ((log_str = strchr(level, ' '))) {
			*log_str++ = '\0';
			ltype = switch_log_str2level(level);
		} else {
			log_str = level;
		}
		if (ltype == SWITCH_LOG_INVALID) {
			ltype = SWITCH_LOG_DEBUG;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), ltype, "%s\n", log_str);
		switch_safe_free(level);
	}
}

SWITCH_STANDARD_APP(info_function)
{
	switch_event_t *event;
	char *buf;
	int level = SWITCH_LOG_INFO;

	if (!zstr(data)) {
		level = switch_log_str2level(data);
	}

	if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(switch_core_session_get_channel(session), event);
		switch_event_serialize(event, &buf, SWITCH_FALSE);
		switch_assert(buf);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), level, "CHANNEL_DATA:\n%s\n", buf);
		switch_event_destroy(&event);
		free(buf);
	}
}

SWITCH_STANDARD_APP(sound_test_function)
{
	switch_ivr_sound_test(session);
}

SWITCH_STANDARD_APP(event_function)
{
	switch_event_t *event;
	char *argv[25] = { 0 };
	int argc = 0;
	char *lbuf;

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_APPLICATION) == SWITCH_STATUS_SUCCESS) {
		if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
			&& (argc = switch_separate_string(lbuf, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			int x = 0;

			for (x = 0; x < argc; x++) {
				char *p, *this = argv[x];
				if (this) {
					char *var, *val;
					p = this;
					while (*p == ' ')
						*p++ = '\0';
					this = p;

					var = this;
					val = NULL;
					if ((val = strchr(var, '='))) {
						p = val - 1;
						*val++ = '\0';
						while (*p == ' ')
							*p-- = '\0';
						p = val;
						while (*p == ' ')
							*p++ = '\0';
						val = p;
						if (!strcasecmp(var, "Event-Name")) {
							switch_name_event(val, &event->event_id);
							switch_event_del_header(event, var);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, var, val);
						} else if (!strcasecmp(var, "Event-Subclass")) {
							size_t len = strlen(val) + 1;
							void *new = malloc(len);
							switch_assert(new);
							memcpy(new, val, len);
							event->subclass_name = new;
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, var, val);
						} else {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, var, val);
						}
					}
				}
			}
		}
		switch_channel_event_set_data(switch_core_session_get_channel(session), event);
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_APP(privacy_function)
{
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(switch_core_session_get_channel(session));

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No privacy mode specified.\n");
	} else {
		switch_set_flag(caller_profile, SWITCH_CPF_SCREEN);


		if (!strcasecmp(data, "full")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(data, "name")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
		} else if (!strcasecmp(data, "number")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else if (switch_true(data)) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (switch_false(data)) {
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
							  "INVALID privacy mode specified. Use a valid mode [no|yes|name|full|number].\n");
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Privacy to %s [%d]\n", data, caller_profile->flags);
	}
}

SWITCH_STANDARD_APP(strftime_function)
{
	char *argv[2] = { 0 };
	int argc;
	char *lbuf;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, '=', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
		switch_size_t retsize;
		switch_time_exp_t tm;
		char date[80] = "";

		switch_time_exp_lt(&tm, switch_micro_time_now());
		switch_strftime(date, &retsize, sizeof(date), argv[1], &tm);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", argv[0], date);
		switch_channel_set_variable(switch_core_session_get_channel(session), argv[0], date);
	}
}

SWITCH_STANDARD_API(strepoch_api_function)
{
	switch_time_t out;

	if (zstr(cmd)) {
		out = switch_micro_time_now();
	} else {
		out = switch_str_time(cmd);
	}

	stream->write_function(stream, "%d", (uint32_t) ((out) / (int64_t) (1000000)));

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(strmicroepoch_api_function)
{
	switch_time_t out;

	if (zstr(cmd)) {
		out = switch_micro_time_now();
	} else {
		out = switch_str_time(cmd);
	}

	stream->write_function(stream, "%"SWITCH_TIME_T_FMT, out);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(strftime_api_function)
{
	switch_size_t retsize;
	switch_time_exp_t tm;
	char date[80] = "";
	switch_time_t thetime;
	char *p, *q = NULL;
	char *mycmd = NULL;

	if (!zstr(cmd)) {
		mycmd = strdup(cmd);
		q = mycmd;
	}

	if (!zstr(q) && (p = strchr(q, '|'))) {
		*p++ = '\0';
		
		thetime = switch_time_make(atol(q), 0);
		q = p + 1;
	} else {
		thetime = switch_micro_time_now();
	}
	switch_time_exp_lt(&tm, thetime);

	if (zstr(q)) {
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
	} else {
		switch_strftime(date, &retsize, sizeof(date), q, &tm);
	}
	stream->write_function(stream, "%s", date);
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}


#define PRESENCE_USAGE "[in|out] <user> <rpid> <message>"
SWITCH_STANDARD_API(presence_api_function)
{
	switch_event_t *event;
	char *lbuf = NULL, *argv[4];
	int argc = 0;
	switch_event_types_t type = SWITCH_EVENT_PRESENCE_IN;
	int need = 4;

	if (!zstr(cmd) && (lbuf = strdup(cmd))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) > 0) {

		if (!strcasecmp(argv[0], "out")) {
			type = SWITCH_EVENT_PRESENCE_OUT;
			need = 2;
		} else if (strcasecmp(argv[0], "in")) {
			goto error;
		}

		if (argc < need) {
			goto error;
		}

		if (switch_event_create(&event, type) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", "dp");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", __FILE__);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", argv[1]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", argv[2]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", argv[3]);
			if (type == SWITCH_EVENT_PRESENCE_IN) {
				if (!strncasecmp(argv[3], "cs_", 3) || switch_stristr("hangup", argv[3])) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "CS_HANGUP");
				}
			} else {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "CS_HANGUP");
			}
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", 0);
			switch_event_fire(&event);
		}
		stream->write_function(stream, "Event Sent");
	} else {
		goto error;
	}

	switch_safe_free(lbuf);
	return SWITCH_STATUS_SUCCESS;

  error:

	switch_safe_free(lbuf);
	stream->write_function(stream, "Invalid: presence %s", PRESENCE_USAGE);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(chat_api_function)
{
	char *lbuf = NULL, *argv[5];
	int argc = 0;

	if (!zstr(cmd) && (lbuf = strdup(cmd))
		&& (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) >= 4) {

		if (switch_core_chat_send_args(argv[0], "global", argv[1], argv[2], "", argv[3], !zstr(argv[4]) ? argv[4] : NULL, "", SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "Sent");
		} else {
			stream->write_function(stream, "Error! Message Not Sent");
		}
	} else {
		stream->write_function(stream, "Invalid");
	}

	switch_safe_free(lbuf);
	return SWITCH_STATUS_SUCCESS;
}

static char *ivr_cf_name = "ivr.conf";

#ifdef _TEST_CALLBACK_
static switch_ivr_action_t menu_handler(switch_ivr_menu_t *menu, char *param, char *buf, size_t buflen, void *obj)
{
	switch_ivr_action_t action = SWITCH_IVR_ACTION_NOOP;

	if (param != NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "menu_handler '%s'\n", param);
	}

	return action;
}
#endif

SWITCH_STANDARD_APP(ivr_application_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *params;
	const char *name = (const char *) data;

	if (channel) {
		switch_xml_t cxml = NULL, cfg = NULL, xml_menus = NULL, xml_menu = NULL;

		/* Open the config from the xml registry */
		switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(params);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Menu-Name", name);
		switch_channel_event_set_data(channel, params);

		if ((cxml = switch_xml_open_cfg(ivr_cf_name, &cfg, params)) != NULL) {
			if ((xml_menus = switch_xml_child(cfg, "menus"))) {
				xml_menu = switch_xml_find_child(xml_menus, "menu", "name", name);

				/* if the menu was found */
				if (xml_menu != NULL) {
					switch_ivr_menu_xml_ctx_t *xml_ctx = NULL;
					switch_ivr_menu_t *menu_stack = NULL;

					/* build a menu tree and execute it */
					if (switch_ivr_menu_stack_xml_init(&xml_ctx, NULL) == SWITCH_STATUS_SUCCESS
#ifdef _TEST_CALLBACK_
						&& switch_ivr_menu_stack_xml_add_custom(xml_ctx, "custom", &menu_handler) == SWITCH_STATUS_SUCCESS
#endif
						&& switch_ivr_menu_stack_xml_build(xml_ctx, &menu_stack, xml_menus, xml_menu) == SWITCH_STATUS_SUCCESS) {
						switch_xml_free(cxml);
						cxml = NULL;
						switch_ivr_menu_execute(session, menu_stack, (char *) name, NULL);
						switch_ivr_menu_stack_free(menu_stack);
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Unable to create menu\n");
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Unable to find menu\n");
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No menus configured\n");
			}
			switch_xml_free(cxml);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of %s failed\n", ivr_cf_name);
		}
		switch_event_destroy(&params);
	}
}

SWITCH_STANDARD_APP(dtmf_session_function)
{
	switch_ivr_inband_dtmf_session(session);
}

SWITCH_STANDARD_APP(stop_dtmf_session_function)
{
	switch_ivr_stop_inband_dtmf_session(session);
}

SWITCH_STANDARD_APP(dtmf_session_generate_function)
{
	switch_bool_t do_read = SWITCH_TRUE;

	if (!zstr(data)) {
		if (!strcasecmp(data, "write")) {
			do_read = SWITCH_FALSE;
		}
	}
	switch_ivr_inband_dtmf_generate_session(session, do_read);
}

SWITCH_STANDARD_APP(stop_dtmf_session_generate_function)
{
	switch_ivr_stop_inband_dtmf_generate_session(session);
}

SWITCH_STANDARD_APP(fax_detect_session_function)
{
	switch_ivr_tone_detect_session(session, "fax", "1100.0", "r", 0, 1, NULL, NULL, NULL);
}

SWITCH_STANDARD_APP(system_session_function)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Executing command: %s\n", data);
	if (switch_system(data, SWITCH_TRUE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", data);
	}
}

SWITCH_STANDARD_APP(bgsystem_session_function)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Executing command: %s\n", data);
	if (switch_system(data, SWITCH_FALSE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", data);
	}
}

SWITCH_STANDARD_APP(tone_detect_session_function)
{
	char *argv[7] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;
	int hits = 0;
	const char *hp = NULL;

	if (zstr(data) || !(mydata = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	if (argv[3]) {
		uint32_t mto;
		if (*argv[3] == '+') {
			if ((mto = atol(argv[3] + 1)) > 0) {
				to = switch_epoch_time_now(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID Timeout!\n");
			}
		} else {
			if ((to = atol(argv[3])) < switch_epoch_time_now(NULL)) {
				if (to >= 1) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				}
				to = 0;
			}
		}
	}

	if (argv[4] && argv[5]) {
		hp = argv[6];
	} else if (argv[4] && !argv[6]) {
		hp = argv[4];
	}

	if (hp) {
		hits = atoi(hp);
		if (hits < 0) {
			hits = 0;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Enabling tone detection '%s' '%s'\n", argv[0], argv[1]);

	switch_ivr_tone_detect_session(session, argv[0], argv[1], argv[2], to, hits, argv[4], argv[5], NULL);
}

SWITCH_STANDARD_APP(stop_fax_detect_session_function)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Disabling tone detection\n");
	switch_ivr_stop_tone_detect_session(session);
}

SWITCH_STANDARD_APP(echo_function)
{
	switch_ivr_session_echo(session, NULL);
}

SWITCH_STANDARD_APP(park_function)
{
	switch_ivr_park(session, NULL);
}

SWITCH_STANDARD_APP(park_state_function)
{
	switch_ivr_park_session(session);
}

/********************************************************************************/
/*						Playback/Record Functions								*/
/********************************************************************************/

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t bridge_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char *str = (char *) buf;

	if (str && input && itype == SWITCH_INPUT_TYPE_DTMF) {
		switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
		if (strchr(str, dtmf->digit)) {
			return SWITCH_STATUS_BREAK;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char sbuf[3];

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			const char *terminators;
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *p;

			if (!(terminators = switch_channel_get_variable(channel, SWITCH_PLAYBACK_TERMINATORS_VARIABLE))) {
				terminators = "*";
			}
			if (!strcasecmp(terminators, "any")) {
				terminators = "1234567890*#";
			}
			if (!strcasecmp(terminators, "none")) {
				terminators = NULL;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Digit %c\n", dtmf->digit);

			for (p = terminators; p && *p; p++) {
				if (*p == dtmf->digit) {
					switch_snprintf(sbuf, sizeof(sbuf), "%c", *p);
					switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, sbuf);
					return SWITCH_STATUS_BREAK;
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(sleep_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No timeout specified.\n");
	} else {
		uint32_t ms = atoi(data);
		char buf[10];
		switch_input_args_t args = { 0 };

		if (switch_true(switch_channel_get_variable(channel, "sleep_eat_digits"))) {
			args.input_callback = on_dtmf;
			args.buf = buf;
			args.buflen = sizeof(buf);
			switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");
		}

		switch_ivr_sleep(session, ms, SWITCH_TRUE, &args);
	}
}

SWITCH_STANDARD_APP(clear_speech_cache_function)
{
	switch_ivr_clear_speech_cache(session);
}

SWITCH_STANDARD_APP(speak_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char buf[10];
	char *argv[3] = { 0 };
	int argc;
	const char *engine = NULL;
	const char *voice = NULL;
	char *text = NULL;
	char *mydata = NULL;
	switch_input_args_t args = { 0 };

	if (zstr(data) || !(mydata = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	}

	argc = switch_separate_string(mydata, '|', argv, sizeof(argv) / sizeof(argv[0]));

	if (argc == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	} else if (argc == 1) {
		text = switch_core_session_strdup(session, data); /* unstripped text */
	} else if (argc == 2) {
		voice = argv[0];
		text = switch_core_session_strdup(session, data + (argv[1] - argv[0])); /* unstripped text */
	} else {
		engine = argv[0];
		voice = argv[1];
		text = switch_core_session_strdup(session, data + (argv[2] - argv[0])); /* unstripped text */
	}

	if (!engine) {
		engine = switch_channel_get_variable(channel, "tts_engine");
	}

	if (!voice) {
		voice = switch_channel_get_variable(channel, "tts_voice");
	}

	if (!(engine && voice && text)) {
		if (!engine) {
			engine = "NULL";
		}
		if (!voice) {
			voice = "NULL";
		}
		if (!text) {
			text = "NULL";
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Params! [%s][%s][%s]\n", engine, voice, text);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

	args.input_callback = on_dtmf;
	args.buf = buf;
	args.buflen = sizeof(buf);

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	switch_ivr_speak_text(session, engine, voice, text, &args);
}

static switch_status_t xfer_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch_core_session_t *peer_session = (switch_core_session_t *) buf;

	if (!buf || !peer_session) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			switch_channel_t *channel = switch_core_session_get_channel(session);
			switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);

			if (dtmf->digit == '*') {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				return SWITCH_STATUS_FALSE;
			}

			if (dtmf->digit == '#') {
				switch_channel_hangup(peer_channel, SWITCH_CAUSE_NORMAL_CLEARING);
				return SWITCH_STATUS_FALSE;
			}

			if (dtmf->digit == '0') {
				switch_caller_extension_t *extension = NULL;
				const char *app = "three_way";
				const char *app_arg = switch_core_session_get_uuid(session);
				const char *holding = switch_channel_get_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE);
				switch_core_session_t *b_session;

				if (holding && (b_session = switch_core_session_locate(holding))) {
					switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
					if (!switch_channel_ready(b_channel)) {
						app = "intercept";
					}
					switch_core_session_rwunlock(b_session);
				}

				if ((extension = switch_caller_extension_new(peer_session, app, app_arg)) == 0) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
					abort();
				}

				switch_caller_extension_add_application(peer_session, extension, app, app_arg);
				switch_channel_set_caller_extension(peer_channel, extension);
				switch_channel_set_state(peer_channel, CS_RESET);
				switch_channel_wait_for_state(peer_channel, channel, CS_RESET);
				switch_channel_set_state(peer_channel, CS_EXECUTE);
				switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, NULL);
				return SWITCH_STATUS_FALSE;
			}

		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tmp_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state == CS_HANGUP || state == CS_ROUTING) {
		const char *bond = switch_channel_get_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE);

		if (!zstr(bond)) {
			switch_core_session_t *b_session;
			
			if ((b_session = switch_core_session_locate(bond))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				if (switch_channel_up(b_channel)) {
					switch_channel_set_flag(b_channel, CF_REDIRECT);
				}
				switch_core_session_rwunlock(b_session);
			}
		}

		switch_core_event_hook_remove_state_change(session, tmp_hanguphook);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	const char *id = NULL;

	if (state == CS_HANGUP || state == CS_ROUTING) {
		if ((id = switch_channel_get_variable(channel, "xfer_uuids"))) {
			switch_stream_handle_t stream = { 0 };
			SWITCH_STANDARD_STREAM(stream);
			switch_api_execute("uuid_bridge", id, NULL, &stream);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "\nHangup Command uuid_bridge(%s):\n%s\n", id,
							  switch_str_nil((char *) stream.data));
			switch_safe_free(stream.data);
		}

		switch_core_event_hook_remove_state_change(session, hanguphook);
	}
	return SWITCH_STATUS_SUCCESS;
}


static void att_xfer_set_result(switch_channel_t *channel, switch_status_t status)
{
	switch_channel_set_variable(channel, SWITCH_ATT_XFER_RESULT_VARIABLE, status == SWITCH_STATUS_SUCCESS ? "success" : "failure");
}

struct att_obj {
	switch_core_session_t *session;
	const char *data;
	int running;
};

void *SWITCH_THREAD_FUNC att_thread_run(switch_thread_t *thread, void *obj)
{
	struct att_obj *att = (struct att_obj *) obj;
	switch_core_session_t *session = att->session;
	switch_core_session_t *peer_session = NULL;
	const char *data = att->data;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	switch_channel_t *channel = switch_core_session_get_channel(session), *peer_channel = NULL;
	const char *bond = NULL;
	switch_core_session_t *b_session = NULL;
	switch_bool_t follow_recording = switch_true(switch_channel_get_variable(channel, "recording_follow_attxfer"));

	att->running = 1;

	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}
		
	bond = switch_channel_get_partner_uuid(channel);
	switch_channel_set_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, bond);
	switch_core_event_hook_add_state_change(session, tmp_hanguphook);

	if (follow_recording && (b_session = switch_core_session_locate(bond))) {
		switch_ivr_transfer_recordings(b_session, session);
		switch_core_session_rwunlock(b_session);
	}

	if (switch_ivr_originate(session, &peer_session, &cause, data, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL)
		!= SWITCH_STATUS_SUCCESS || !peer_session) {
		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, bond);
		goto end;
	}

	peer_channel = switch_core_session_get_channel(peer_session);
	switch_channel_set_flag(peer_channel, CF_INNER_BRIDGE);
	switch_channel_set_flag(channel, CF_INNER_BRIDGE);

	switch_ivr_multi_threaded_bridge(session, peer_session, xfer_on_dtmf, peer_session, NULL);

	switch_channel_clear_flag(peer_channel, CF_INNER_BRIDGE);
	switch_channel_clear_flag(channel, CF_INNER_BRIDGE);

	if (zstr(bond) && switch_channel_down(peer_channel)) {
		switch_core_session_rwunlock(peer_session);
		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, bond);
		goto end;
	}

	if (bond) {
		int br = 0;

		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, bond);

		if (!switch_channel_down(peer_channel)) {
			if (!switch_channel_ready(channel)) {
				switch_status_t status;

				if (follow_recording) {
					switch_ivr_transfer_recordings(session, peer_session);
				}
				status = switch_ivr_uuid_bridge(switch_core_session_get_uuid(peer_session), bond);
				att_xfer_set_result(peer_channel, status);
				br++;
			} else if ((b_session = switch_core_session_locate(bond))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				switch_channel_set_variable_printf(b_channel, "xfer_uuids", "%s %s", switch_core_session_get_uuid(peer_session), switch_core_session_get_uuid(session));
				switch_channel_set_variable_printf(channel, "xfer_uuids", "%s %s", switch_core_session_get_uuid(peer_session), bond);

				switch_core_event_hook_add_state_change(session, hanguphook);
				switch_core_event_hook_add_state_change(b_session, hanguphook);

				switch_core_session_rwunlock(b_session);
			}
		}

		if (!br) {
			switch_status_t status = switch_ivr_uuid_bridge(switch_core_session_get_uuid(session), bond);
			att_xfer_set_result(channel, status);
		}

	}

	switch_core_session_rwunlock(peer_session);

  end:

	switch_core_event_hook_remove_state_change(session, tmp_hanguphook);

	switch_channel_set_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, NULL);
	switch_channel_clear_flag(channel, CF_XFER_ZOMBIE);

	switch_core_session_rwunlock(session);
	att->running = 0;

	return NULL;
}

SWITCH_STANDARD_APP(att_xfer_function)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	struct att_obj *att;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_detach_set(thd_attr, 1);

	att = switch_core_session_alloc(session, sizeof(*att));
	att->running = -1;
	att->session = session;
	att->data = switch_core_session_strdup(session, data);
	switch_thread_create(&thread, thd_attr, att_thread_run, att, pool);

	while(att->running && switch_channel_up(channel)) {
		switch_yield(100000);
	}
}

SWITCH_STANDARD_APP(read_function)
{
	char *mydata;
	char *argv[7] = { 0 };
	int argc;
	int32_t min_digits = 0;
	int32_t max_digits = 0;
	uint32_t digit_timeout = 0;
	int timeout = 1000;
	char digit_buffer[128] = "";
	const char *prompt_audio_file = NULL;
	const char *var_name = NULL;
	const char *valid_terminators = NULL;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No arguments specified.\n");
		return;
	}

	min_digits = atoi(argv[0]);

	if (argc > 1) {
		max_digits = atoi(argv[1]);
	}

	if (argc > 2) {
		prompt_audio_file = argv[2];
	}

	if (argc > 3) {
		var_name = argv[3];
	}

	if (argc > 4) {
		timeout = atoi(argv[4]);
	}

	if (argc > 5) {
		valid_terminators = argv[5];
	}

	if (argc > 6) {
		digit_timeout = switch_atoui(argv[6]);
	}

	if (min_digits <= 1) {
		min_digits = 1;
	}

	if (max_digits < min_digits) {
		max_digits = min_digits;
	}

	if (timeout <= 1000) {
		timeout = 1000;
	}

	if (zstr(valid_terminators)) {
		valid_terminators = "#";
	}

	switch_ivr_read(session, min_digits, max_digits, prompt_audio_file, var_name, digit_buffer, sizeof(digit_buffer), timeout, valid_terminators, 
					digit_timeout);
}

SWITCH_STANDARD_APP(play_and_get_digits_function)
{
	char *mydata;
	char *argv[11] = { 0 };
	int argc;
	int32_t min_digits = 0;
	int32_t max_digits = 0;
	int32_t max_tries = 0;
	uint32_t digit_timeout = 0;
	int timeout = 1000;
	char digit_buffer[128] = "";
	const char *prompt_audio_file = NULL;
	const char *bad_input_audio_file = NULL;
	const char *var_name = NULL;
	const char *valid_terminators = NULL;
	const char *digits_regex = NULL;
	const char *transfer_on_failure = NULL;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No arguments specified.\n");
		return;
	}

	min_digits = atoi(argv[0]);

	if (argc > 1) {
		max_digits = atoi(argv[1]);
	}

	if (argc > 2) {
		max_tries = atoi(argv[2]);
	}

	if (argc > 3) {
		timeout = atoi(argv[3]);
	}

	if (argc > 4) {
		valid_terminators = argv[4];
	}

	if (argc > 5) {
		prompt_audio_file = argv[5];
	}

	if (argc > 6) {
		bad_input_audio_file = argv[6];
	}

	if (argc > 7) {
		var_name = argv[7];
	}

	if (argc > 8) {
		digits_regex = argv[8];
	}

	if (argc > 9) {
		digit_timeout = switch_atoui(argv[9]);
	}

	if (argc > 10) {
		transfer_on_failure = argv[10];
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Transfer on failure = [%s].\n", transfer_on_failure);
	}

	if (min_digits <= 0) {
		min_digits = 0;
	}

	if (max_digits < min_digits) {
		max_digits = min_digits;
	}

	if (timeout <= 1000) {
		timeout = 1000;
	}

	if (zstr(valid_terminators)) {
		valid_terminators = "#";
	}

	switch_play_and_get_digits(session, min_digits, max_digits, max_tries, timeout, valid_terminators,
							   prompt_audio_file, bad_input_audio_file, var_name, digit_buffer, sizeof(digit_buffer), 
							   digits_regex, digit_timeout, transfer_on_failure);
}

#define SAY_SYNTAX "<module_name>[:<lang>] <say_type> <say_method> [<say_gender>] <text>"
SWITCH_STANDARD_APP(say_function)
{
	char *argv[5] = { 0 };
	int argc;
	char *lbuf = NULL;
	switch_input_args_t args = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) && (argc == 4 || argc == 5)) {

		args.input_callback = on_dtmf;

		switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

		/* Set default langauge according to the <module_name> */
		if (!strchr(argv[0], ':')) {
			argv[0] = switch_core_session_sprintf(session, "%s:%s", argv[0], argv[0]);
		}

		switch_ivr_say(session, (argc == 4) ? argv[3] : argv[4], argv[0], argv[1], argv[2], (argc == 5) ? argv[3] : NULL ,&args);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", SAY_SYNTAX);
	}

}


SWITCH_STANDARD_APP(phrase_function)
{
	char *mydata = NULL;
	switch_input_args_t args = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		const char *lang;
		char *macro = mydata;
		char *mdata = NULL;

		if ((mdata = strchr(macro, ','))) {
			*mdata++ = '\0';
		}

		lang = switch_channel_get_variable(channel, "language");

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Execute %s(%s) lang %s\n", macro, switch_str_nil(mdata),
						  switch_str_nil(lang));

		args.input_callback = on_dtmf;

		switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

		status = switch_ivr_phrase_macro(session, macro, mdata, lang, &args);
	} else {
		status = SWITCH_STATUS_NOOP;
	}

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PHRASE PLAYED");
		break;
	case SWITCH_STATUS_NOOP:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "NOTHING");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "UNKNOWN ERROR");
		break;
	}
}


SWITCH_STANDARD_APP(playback_function)
{
	switch_input_args_t args = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh = { 0 };
	char *p;
	const char *file = NULL;

	if (data) {
		file = switch_core_session_strdup(session, data);
		if ((p = strchr(file, '@')) && *(p + 1) == '@') {
			*p = '\0';
			p += 2;
			if (p && *p) {
				fh.samples = atoi(p);
			}
		}
	} else {
		file = data;
	}

	args.input_callback = on_dtmf;

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	status = switch_ivr_play_file(session, &fh, file, &args);
	switch_assert(!(fh.flags & SWITCH_FILE_OPEN));

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
		break;
	case SWITCH_STATUS_NOTFOUND:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE NOT FOUND");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK ERROR");
		break;
	}

}



SWITCH_STANDARD_APP(endless_playback_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *file = data;

	while (switch_channel_ready(channel)) {
		status = switch_ivr_play_file(session, NULL, file, NULL);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			break;
		}
	}

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
		break;
	case SWITCH_STATUS_NOTFOUND:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE NOT FOUND");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK ERROR");
		break;
	}

}

SWITCH_STANDARD_APP(loop_playback_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *file = data;
	int loop = 1;

	if (*file == '+') {
		const char *p = ++file;
		while(*file && *file++ != ' ') { }

		if (zstr(p)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing loop in data [%s]\n", data);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}

		loop = atoi(p);
	}

	if (zstr(file)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing file arg in data [%s]\n", data);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return;
	}

	while (switch_channel_ready(channel) && (loop < 0 || loop-- > 0)) {
		status = switch_ivr_play_file(session, NULL, file, NULL);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			break;
		}
	}

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
		break;
	case SWITCH_STATUS_NOTFOUND:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE NOT FOUND");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK ERROR");
		break;
	}

}

SWITCH_STANDARD_APP(gentones_function)
{
	char *tone_script = NULL;
	switch_input_args_t args = { 0 };
	char *l;
	int32_t loops = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (zstr(data) || !(tone_script = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	}

	if ((l = strchr(tone_script, '|'))) {
		*l++ = '\0';
		loops = atoi(l);

		if (loops < 0) {
			loops = -1;
		}
	}

	args.input_callback = on_dtmf;

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	switch_ivr_gentones(session, tone_script, loops, &args);
}

SWITCH_STANDARD_APP(displace_session_function)
{
	char *path = NULL;
	uint32_t limit = 0;
	char *argv[6] = { 0 };
	int x, argc;
	char *lbuf = NULL;
	char *flags = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		path = argv[0];
		for (x = 1; x < argc; x++) {
			if (strchr(argv[x], '+')) {
				limit = atoi(argv[x]);
			} else if (!zstr(argv[x])) {
				flags = argv[x];
			}
		}
		switch_ivr_displace_session(session, path, limit, flags);
	}
}

SWITCH_STANDARD_APP(stop_displace_session_function)
{
	switch_ivr_stop_displace_session(session, data);
}

SWITCH_STANDARD_APP(capture_function)
{
	char *argv[3] = { 0 };
	int argc;
	switch_regex_t *re = NULL;
	int ovector[30] = {0};
	char *lbuf;
	int proceed;
	
	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) == 3) {
		if ((proceed = switch_regex_perform(argv[1], argv[2], &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
			switch_capture_regex(re, proceed, argv[1], ovector, argv[0], switch_regex_set_var_callback, session);
		}
		switch_regex_safe_free(re);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No data specified.\n");
	}	
}

SWITCH_STANDARD_APP(record_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	uint32_t limit = 0;
	char *path;
	switch_input_args_t args = { 0 };
	switch_file_handle_t fh = { 0 };
	//int argc;
	char *mydata, *argv[4] = { 0 };
	char *l = NULL;
	const char *tmp;
	int rate;

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No file specified.\n");
		return;
	}

	path = argv[0];
	l = argv[1];

	if (l) {
		if (*l == '+') {
			l++;
		}
		if (l) {
			limit = switch_atoui(l);
		}
	}

	if (argv[2]) {
		fh.thresh = switch_atoui(argv[2]);
	}

	if (argv[3]) {
		fh.silence_hits = switch_atoui(argv[3]);
	}

	if ((tmp = switch_channel_get_variable(channel, "record_rate"))) {
		rate = atoi(tmp);
		if (rate > 0) {
			fh.samplerate = rate;
		}
	}

	args.input_callback = on_dtmf;

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	status = switch_ivr_record_file(session, &fh, path, &args, limit);

	if (!switch_channel_ready(channel) || (status != SWITCH_STATUS_SUCCESS && !SWITCH_STATUS_IS_BREAK(status))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
}

SWITCH_STANDARD_APP(preprocess_session_function)
{
	switch_ivr_preprocess_session(session, (char *) data);
}

SWITCH_STANDARD_APP(record_session_mask_function)
{
	switch_ivr_record_session_mask(session, (char *) data, SWITCH_TRUE);
}

SWITCH_STANDARD_APP(record_session_unmask_function)
{
	switch_ivr_record_session_mask(session, (char *) data, SWITCH_FALSE);
}

SWITCH_STANDARD_APP(record_session_function)
{
	char *path = NULL;
	char *path_end;
	uint32_t limit = 0;

	if (zstr(data)) {
		return;
	}

	path = switch_core_session_strdup(session, data);

	/* Search for a space then a plus followed by only numbers at the end of the path, 
	   if found trim any spaces to the left/right of the plus use the left side as the
	   path and right side as a time limit on the recording
	 */

	/* if we find a + and the character before it is a space */
	if ((path_end = strrchr(path, '+')) && path_end > path && *(path_end - 1) == ' ') {
		char *limit_start = path_end + 1;

		/* not at the end and the rest is numbers lets parse out the limit and fix up the path */
		if (*limit_start != '\0' && switch_is_number(limit_start) == SWITCH_TRUE) {
			limit = atoi(limit_start);
			/* back it off by one character to the char before the + */
			path_end--;

			/* trim spaces to the left of the plus */
			while (path_end > path && *path_end == ' ') {
				path_end--;
			}

			*(path_end + 1) = '\0';
		}
	}
	switch_ivr_record_session(session, path, limit, NULL);
}

SWITCH_STANDARD_APP(stop_record_session_function)
{
	switch_ivr_stop_record_session(session, data);
}


SWITCH_STANDARD_APP(video_write_overlay_session_function)
{
	char *mydata;
	char *argv[3] = { 0 };
	int argc = 0;
	switch_img_position_t pos = POS_LEFT_BOT;
	uint8_t alpha = 255;

	if (zstr(data)) {
		return;
	}

	mydata = switch_core_session_strdup(session, data);
	argc = switch_split(mydata, ' ', argv);

	if (argc > 1) {
		pos = parse_img_position(argv[1]);
	}

	if (argc > 2) {
		int x = atoi(argv[2]);
		if (x > 0 && x < 256) {
			alpha = (uint8_t) x;
		}
	}

	switch_ivr_video_write_overlay_session(session, argv[0], pos, alpha);
}

SWITCH_STANDARD_APP(stop_video_write_overlay_session_function)
{
	switch_ivr_stop_video_write_overlay_session(session);
}

/********************************************************************************/
/*								Bridge Functions								*/
/********************************************************************************/

static switch_status_t camp_fire(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			char *key = (char *) buf;

			if (dtmf->digit == *key) {
				return SWITCH_STATUS_BREAK;
			}
		}
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

struct camping_stake {
	switch_core_session_t *session;
	int running;
	int do_xfer;
	const char *moh;
};

static void *SWITCH_THREAD_FUNC camp_music_thread(switch_thread_t *thread, void *obj)
{
	struct camping_stake *stake = (struct camping_stake *) obj;
	switch_core_session_t *session = stake->session;
	switch_channel_t *channel = switch_core_session_get_channel(stake->session);
	const char *moh = stake->moh, *greet = NULL;
	switch_input_args_t args = { 0 };
	char dbuf[2] = "";
	switch_status_t status = SWITCH_STATUS_FALSE;
	const char *stop;

	if ((stop = switch_channel_get_variable(channel, "campon_stop_key"))) {
		*dbuf = *stop;
	}

	args.input_callback = camp_fire;
	args.buf = dbuf;
	args.buflen = sizeof(dbuf);

	switch_core_session_read_lock(session);

	/* don't set this to a local_stream:// or you will not be happy */
	if ((greet = switch_channel_get_variable(channel, "campon_announce_sound"))) {
		status = switch_ivr_play_file(session, NULL, greet, &args);
	}

	while (stake->running && switch_channel_ready(channel)) {
		switch_ivr_parse_signal_data(session, SWITCH_TRUE, SWITCH_FALSE);

		if (status != SWITCH_STATUS_BREAK) {
			if (!strcasecmp(moh, "silence")) {
				status = switch_ivr_collect_digits_callback(session, &args, 0, 0);
			} else {
				status = switch_ivr_play_file(session, NULL, stake->moh, &args);
			}
		}

		if (status == SWITCH_STATUS_BREAK) {
			switch_channel_set_flag(channel, CF_NOT_READY);
			stake->do_xfer = 1;
		}
	}
	switch_core_session_rwunlock(session);

	stake->running = 0;

	return NULL;
}

SWITCH_STANDARD_APP(audio_bridge_function)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_core_session_t *peer_session = NULL;
	const char *v_campon = NULL, *v_campon_retries, *v_campon_sleep, *v_campon_timeout, *v_campon_fallback_exten = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	int campon_retries = 100, campon_timeout = 10, campon_sleep = 10, tmp, camping = 0, fail = 0, thread_started = 0;
	struct camping_stake stake = { 0 };
	const char *moh = NULL;
	switch_thread_t *thread = NULL;
	switch_threadattr_t *thd_attr = NULL;
	char *camp_data = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int camp_loops = 0;

	if (zstr(data)) {
		return;
	}

	if ((v_campon = switch_channel_get_variable(caller_channel, "campon")) && switch_true(v_campon)) {
		const char *cid_name = NULL;
		const char *cid_number = NULL;

		if (!(cid_name = switch_channel_get_variable(caller_channel, "effective_caller_id_name"))) {
			cid_name = switch_channel_get_variable(caller_channel, "caller_id_name");
		}

		if (!(cid_number = switch_channel_get_variable(caller_channel, "effective_caller_id_number"))) {
			cid_number = switch_channel_get_variable(caller_channel, "caller_id_number");
		}

		if (cid_name && !cid_number) {
			cid_number = cid_name;
		}

		if (cid_number && !cid_name) {
			cid_name = cid_number;
		}

		v_campon_retries = switch_channel_get_variable(caller_channel, "campon_retries");
		v_campon_timeout = switch_channel_get_variable(caller_channel, "campon_timeout");
		v_campon_sleep = switch_channel_get_variable(caller_channel, "campon_sleep");
		v_campon_fallback_exten = switch_channel_get_variable(caller_channel, "campon_fallback_exten");

		if (v_campon_retries) {
			if ((tmp = atoi(v_campon_retries)) > 0) {
				campon_retries = tmp;
			}
		}

		if (v_campon_timeout) {
			if ((tmp = atoi(v_campon_timeout)) > 0) {
				campon_timeout = tmp;
			}
		}

		if (v_campon_sleep) {
			if ((tmp = atoi(v_campon_sleep)) > 0) {
				campon_sleep = tmp;
			}
		}

		switch_channel_answer(caller_channel);
		camping = 1;

		if (cid_name && cid_number) {
			camp_data = switch_core_session_sprintf(session, "{origination_caller_id_name='%s',origination_caller_id_number='%s'}%s",
													cid_name, cid_number, data);
		} else {
			camp_data = (char *) data;
		}

		if (!(moh = switch_channel_get_variable(caller_channel, "campon_hold_music"))) {
			moh = switch_channel_get_hold_music(caller_channel);
		}

		if (!zstr(moh) && !strcasecmp(moh, "silence")) { 
			moh = NULL;
		}

		do {
			fail = 0;

			if (!switch_channel_ready(caller_channel)) {
				fail = 1;
				break;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				camping = 0;
				break;
			} else {
				fail = 1;
			}

			if (camping) {

				if (!thread_started && fail && moh && !switch_channel_test_flag(caller_channel, CF_PROXY_MODE) &&
					!switch_channel_test_flag(caller_channel, CF_PROXY_MEDIA) &&
					!switch_true(switch_channel_get_variable(caller_channel, "bypass_media"))) {
					switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
					switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
					stake.running = 1;
					stake.moh = moh;
					stake.session = session;
					switch_thread_create(&thread, thd_attr, camp_music_thread, &stake, switch_core_session_get_pool(session));
					thread_started = 1;
				}

				if (camp_loops++) {
					if (--campon_retries <= 0 || stake.do_xfer) {
						camping = 0;
						stake.do_xfer = 1;
						break;
					}

					if (fail) {
						int64_t wait = (int64_t)campon_sleep * 1000000;
						
						while (stake.running && wait > 0 && switch_channel_ready(caller_channel)) {
							switch_yield(100000);
							wait -= 100000;
						}
					}
				}
			}

			status = switch_ivr_originate(NULL, &peer_session, 
										  &cause, camp_data, campon_timeout, NULL, NULL, NULL, NULL, NULL, SOF_NONE, 
										  switch_channel_get_cause_ptr(caller_channel));


		} while (camping && switch_channel_ready(caller_channel));

		if (thread) {
			stake.running = 0;
			switch_channel_set_flag(caller_channel, CF_NOT_READY);
			switch_thread_join(&status, thread);
		}

		switch_channel_clear_flag(caller_channel, CF_NOT_READY);

		if (stake.do_xfer && !zstr(v_campon_fallback_exten)) {
			switch_ivr_session_transfer(session,
										v_campon_fallback_exten,
										switch_channel_get_variable(caller_channel, "campon_fallback_dialplan"),
										switch_channel_get_variable(caller_channel, "campon_fallback_context"));

			if (peer_session) {
				switch_channel_hangup(switch_core_session_get_channel(peer_session), SWITCH_CAUSE_ORIGINATOR_CANCEL);
				switch_core_session_rwunlock(peer_session);
			}

			return;
		}

	} else {
		if ((status =
			 switch_ivr_originate(session, &peer_session, &cause, data, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL)) != SWITCH_STATUS_SUCCESS) {
			fail = 1;
		}
	}

	if (fail) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", switch_channel_cause2str(cause));

		switch_channel_handle_cause(caller_channel, cause);
		return;
	} else {

		switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
		if (switch_true(switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE)) ||
			switch_true(switch_channel_get_variable(peer_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE))) {
			switch_channel_set_flag(caller_channel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
		}

		if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)) {
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			char *a_key = (char *) switch_channel_get_variable(caller_channel, "bridge_terminate_key");
			char *b_key = (char *) switch_channel_get_variable(peer_channel, "bridge_terminate_key");
			int ok = 0;
			switch_input_callback_function_t func = NULL;

			if (a_key) {
				a_key = switch_core_session_strdup(session, a_key);
				ok++;
			}
			if (b_key) {
				b_key = switch_core_session_strdup(session, b_key);
				ok++;
			}
			if (ok) {
				func = bridge_on_dtmf;
			} else {
				a_key = NULL;
				b_key = NULL;
			}

			switch_ivr_multi_threaded_bridge(session, peer_session, func, a_key, b_key);
		}

		if (peer_session) {
			switch_core_session_rwunlock(peer_session);
		}
	}
}

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *pickup_hash;
	switch_mutex_t *pickup_mutex;
	switch_hash_t *mutex_hash;
	switch_mutex_t *mutex_mutex;
} globals;

/* pickup channel */



typedef struct pickup_node_s {
	char *key;
	char *uuid;
	struct pickup_node_s *next;
} pickup_node_t;


#define PICKUP_PROTO "pickup"
static int EC = 0;

static int pickup_count(const char *key_name)
{
	int count = 0;
	pickup_node_t *head, *np;

	switch_mutex_lock(globals.pickup_mutex);
	if ((head = switch_core_hash_find(globals.pickup_hash, key_name))) {
		for (np = head; np; np = np->next) count++;
	}
	switch_mutex_unlock(globals.pickup_mutex);

	return count;

}

static void pickup_send_presence(const char *key_name)
{

	char *domain_name, *dup_key_name = NULL, *dup_domain_name = NULL, *dup_id = NULL;
	switch_event_t *event;
	int count;

	
	dup_key_name = strdup(key_name);
	key_name = dup_key_name;

	if ((domain_name = strchr(dup_key_name, '@'))) {
		*domain_name++ = '\0';
	}
	
	if (zstr(domain_name)) {
		dup_domain_name = switch_core_get_domain(SWITCH_TRUE);
		domain_name = dup_domain_name;
	}
	
	if (zstr(domain_name)) {
		domain_name = "cluecon.com";
	}

	dup_id = switch_mprintf("%s@%s", key_name, domain_name);

	count = pickup_count(dup_id);

	if (count > 0) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", PICKUP_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", dup_id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", dup_id);

			
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d call%s)", count, count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "active");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", key_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
			switch_event_fire(&event);
		}
	} else {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", PICKUP_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", dup_id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", dup_id);

			
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Idle");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", dup_id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
			switch_event_fire(&event);
		}		
	}
	
	switch_safe_free(dup_domain_name);
	switch_safe_free(dup_key_name);
	switch_safe_free(dup_id);
						
}

static void pickup_pres_event_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *dup_to = NULL, *key_name, *dup_key_name = NULL, *domain_name, *dup_domain_name = NULL;
	int count = 0;

	if (!to || strncasecmp(to, "pickup+", 7) || !strchr(to, '@')) {
		return;
	}

	if (!(dup_to = strdup(to))) {
		return;
	}

	key_name = dup_to + 7;

	if ((domain_name = strchr(key_name, '@'))) {
		*domain_name++ = '\0';
	} else {
		dup_domain_name = switch_core_get_domain(SWITCH_TRUE);
		domain_name = dup_domain_name;
	}

	if (zstr(domain_name)) {
		switch_safe_free(dup_to);
		switch_safe_free(dup_domain_name);
		return;
	}

	dup_key_name = switch_mprintf("%q@%q", key_name, domain_name);
	count = pickup_count(dup_key_name);

	switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN);

	if (count) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", PICKUP_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", key_name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", key_name, domain_name);
			
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d call%s)", count, count == 1 ? "" : "s");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "active");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", key_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
	} else {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", PICKUP_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", key_name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", key_name, domain_name);
			
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Idle");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", key_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");

	}

	switch_event_fire(&event);
	switch_safe_free(dup_to);
	switch_safe_free(dup_key_name);
	switch_safe_free(dup_domain_name);
}



static void pickup_add_session(switch_core_session_t *session, const char *key)
{
	pickup_node_t *head, *node, *np;
	char *dup_key = NULL;

	if (!strchr(key, '@')) {
		dup_key = switch_mprintf("%s@%s", key, switch_core_get_domain(SWITCH_FALSE));
		key = dup_key;
	}

	node = malloc(sizeof(*node));
	node->key = strdup(key);
	node->uuid = strdup(switch_core_session_get_uuid(session));
	node->next = NULL;

	switch_mutex_lock(globals.pickup_mutex);
	head = switch_core_hash_find(globals.pickup_hash, key);

	if (head) {
		for (np = head; np && np->next; np = np->next);
		np->next = node;
	} else {
		head = node;
		switch_core_hash_insert(globals.pickup_hash, key, head);
	}

	switch_mutex_unlock(globals.pickup_mutex);

	pickup_send_presence(key);

	switch_safe_free(dup_key);
}

static char *pickup_pop_uuid(const char *key, const char *uuid)
{
	pickup_node_t *node = NULL, *head;
	char *r = NULL;
	char *dup_key = NULL;

	if (!strchr(key, '@')) {
		dup_key = switch_mprintf("%s@%s", key, switch_core_get_domain(SWITCH_FALSE));
		key = dup_key;
	}

	switch_mutex_lock(globals.pickup_mutex);

	if ((head = switch_core_hash_find(globals.pickup_hash, key))) {

		switch_core_hash_delete(globals.pickup_hash, key);
		
		if (uuid) {
			pickup_node_t *np, *lp = NULL;

			for(np = head; np; np = np->next) {
				if (!strcmp(np->uuid, uuid)) {
					if (lp) {
						lp->next = np->next;
					} else {
						head = np->next;
					}
					
					node = np;
					break;
				}

				lp = np;
			}
			
		} else {
			node = head;
			head = head->next;
		}


		if (head) {
			switch_core_hash_insert(globals.pickup_hash, key, head);
		}
	}

	if (node) {
		r = node->uuid;
		free(node->key);
		free(node);
	}
	
	switch_mutex_unlock(globals.pickup_mutex);

	if (r) pickup_send_presence(key);

	switch_safe_free(dup_key);
	
	return r;
}


typedef struct pickup_pvt_s {
	char *key;
	switch_event_t *vars;
} pickup_pvt_t;

switch_endpoint_interface_t *pickup_endpoint_interface;
static switch_call_cause_t pickup_outgoing_channel(switch_core_session_t *session,
												   switch_event_t *var_event,
												   switch_caller_profile_t *outbound_profile,
												   switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												   switch_call_cause_t *cancel_cause);
switch_io_routines_t pickup_io_routines = {
	/*.outgoing_channel */ pickup_outgoing_channel
};

static switch_status_t pickup_event_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_running_state(channel);
	pickup_pvt_t *tech_pvt = switch_core_session_get_private(session);
	char *uuid = NULL;

	switch(state) {
	case CS_DESTROY:
		if (tech_pvt->vars) {
			switch_event_destroy(&tech_pvt->vars);
		}
		break;
	case CS_REPORTING:
		return SWITCH_STATUS_FALSE;
	case CS_HANGUP:
		{
			
			if (switch_channel_test_flag(channel, CF_CHANNEL_SWAP)) {
				const char *key = switch_channel_get_variable(channel, "channel_swap_uuid");
				switch_core_session_t *swap_session;

				if ((swap_session = switch_core_session_locate(key))) {
					switch_channel_t *swap_channel = switch_core_session_get_channel(swap_session);
					switch_channel_hangup(swap_channel, SWITCH_CAUSE_PICKED_OFF);
					switch_core_session_rwunlock(swap_session);
				}
				switch_channel_clear_flag(channel, CF_CHANNEL_SWAP);
			}

			uuid = pickup_pop_uuid(tech_pvt->key, switch_core_session_get_uuid(session));
			switch_safe_free(uuid);
		}
		break;
	default:
		break;
	}


	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t pickup_event_handlers = {
	/*.on_init */ pickup_event_handler,
	/*.on_routing */ pickup_event_handler,
	/*.on_execute */ pickup_event_handler,
	/*.on_hangup */ pickup_event_handler,
	/*.on_exchange_media */ pickup_event_handler,
	/*.on_soft_execute */ pickup_event_handler,
	/*.on_consume_media */ pickup_event_handler,
	/*.on_hibernate */ pickup_event_handler,
	/*.on_reset */ pickup_event_handler,
	/*.on_park */ pickup_event_handler,
	/*.on_reporting */ pickup_event_handler,
	/*.on_destroy */ pickup_event_handler
};

static switch_call_cause_t pickup_outgoing_channel(switch_core_session_t *session,
												   switch_event_t *var_event,
												   switch_caller_profile_t *outbound_profile,
												   switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												   switch_call_cause_t *cancel_cause)
{
	char *pickup;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession;
	switch_channel_t *nchannel;
	char *name;
	pickup_pvt_t *tech_pvt;
	switch_caller_profile_t *caller_profile;

	if (zstr(outbound_profile->destination_number)) {
		goto done;
	}

	pickup = outbound_profile->destination_number;

	flags |= SOF_NO_LIMITS;

	if (!(nsession = switch_core_session_request(pickup_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	tech_pvt = switch_core_session_alloc(nsession, sizeof(*tech_pvt));
	tech_pvt->key = switch_core_session_strdup(nsession, pickup);


	switch_core_session_set_private(nsession, tech_pvt);
	
	nchannel = switch_core_session_get_channel(nsession);
	switch_channel_set_cap(nchannel, CC_PROXY_MEDIA);
	switch_channel_set_cap(nchannel, CC_BYPASS_MEDIA);

	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(nchannel, caller_profile);

	switch_channel_set_state(nchannel, CS_ROUTING);
	


	*new_session = nsession;
	cause = SWITCH_CAUSE_SUCCESS;
	name = switch_core_session_sprintf(nsession, "pickup/%s", pickup);
	switch_channel_set_name(nchannel, name);
	switch_channel_set_variable(nchannel, "process_cdr", "false");
	switch_channel_set_variable(nchannel, "presence_id", NULL);

	switch_event_del_header(var_event, "presence_id");

	pickup_add_session(nsession, pickup);
	switch_channel_set_flag(nchannel, CF_PICKUP);
	switch_channel_set_flag(nchannel, CF_NO_PRESENCE);

	switch_event_dup(&tech_pvt->vars, var_event);

	goto done;

  error:

	if (nsession) {
		switch_core_session_destroy(&nsession);
	}

	if (pool) {
		*pool = NULL;
	}

  done:


	return cause;
}

#define PICKUP_SYNTAX "[<key>]"
SWITCH_STANDARD_APP(pickup_function)
{
	char *uuid = NULL;
	switch_core_session_t *pickup_session;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing data.  Usage: pickup %s\n", PICKUP_SYNTAX);
		return;
	}

	if ((uuid = pickup_pop_uuid((char *)data, NULL))) {
		if ((pickup_session = switch_core_session_locate(uuid))) {
			switch_channel_t *pickup_channel = switch_core_session_get_channel(pickup_session);
			switch_caller_profile_t *pickup_caller_profile = switch_channel_get_caller_profile(pickup_channel), 
				*caller_profile = switch_channel_get_caller_profile(channel);
			const char *name, *num;
			switch_event_t *event;
			switch_event_header_t *hp;
			pickup_pvt_t *tech_pvt = switch_core_session_get_private(pickup_session);

			for(hp = tech_pvt->vars->headers; hp; hp = hp->next) {
				switch_channel_set_variable(channel, hp->name, hp->value);
			}

			
			switch_channel_set_flag(pickup_channel, CF_CHANNEL_SWAP);
			switch_channel_set_variable(pickup_channel, "channel_swap_uuid", switch_core_session_get_uuid(session));
			
			name = caller_profile->caller_id_name;
			num = caller_profile->caller_id_number;

			caller_profile->caller_id_name = switch_core_strdup(caller_profile->pool, pickup_caller_profile->caller_id_name);
			caller_profile->caller_id_number = switch_core_strdup(caller_profile->pool, pickup_caller_profile->caller_id_number);

			caller_profile->callee_id_name = name;
			caller_profile->callee_id_number = num;
			
			if (switch_event_create(&event, SWITCH_EVENT_CALL_UPDATE) == SWITCH_STATUS_SUCCESS) {
				const char *partner_uuid = switch_channel_get_partner_uuid(channel);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Direction", "RECV");
				
				if (partner_uuid) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridged-To", partner_uuid);
				}
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}


			switch_channel_set_state(channel, CS_HIBERNATE);

			switch_channel_mark_answered(pickup_channel);
			switch_core_session_rwunlock(pickup_session);
		}
		free(uuid);
	}
}





/* fake chan_error */
switch_endpoint_interface_t *error_endpoint_interface;
static switch_call_cause_t error_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause);
switch_io_routines_t error_io_routines = {
	/*.outgoing_channel */ error_outgoing_channel
};

static switch_call_cause_t error_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = switch_channel_str2cause(outbound_profile->destination_number);
	if (cause == SWITCH_CAUSE_NONE) {
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	return cause;
}


/* fake chan_group */
switch_endpoint_interface_t *group_endpoint_interface;
static switch_call_cause_t group_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause);
switch_io_routines_t group_io_routines = {
	/*.outgoing_channel */ group_outgoing_channel
};

static switch_call_cause_t group_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause)
{
	char *group = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	char *template = NULL, *dest = NULL;
	switch_originate_flag_t myflags = SOF_NONE;
	char *cid_name_override = NULL;
	char *cid_num_override = NULL;
	char *domain = NULL, *dup_domain = NULL;
	switch_channel_t *new_channel = NULL;
	unsigned int timelimit = 60;
	const char *skip, *var;

	group = strdup(outbound_profile->destination_number);

	if (!group)
		goto done;

	if ((domain = strchr(group, '@'))) {
		*domain++ = '\0';
	} else {
		domain = switch_core_get_domain(SWITCH_TRUE);
		dup_domain = domain;
	}

	if (!domain) {
		goto done;
	}

	if (var_event && (skip = switch_event_get_header(var_event, "group_recurse_variables")) && switch_false(skip)) {
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		var_event = NULL;
	}

	template = switch_mprintf("${group_call(%s@%s)}", group, domain);

	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		dest = switch_channel_expand_variables(channel, template);
		if ((var = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
	} else if (var_event) {
		dest = switch_event_expand_headers(var_event, template);
	} else {
		switch_event_t *event = NULL;
		switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
		dest = switch_event_expand_headers(event, template);
		switch_event_destroy(&event);
	}

	if (!dest) {
		goto done;
	}

	if (var_event) {
		cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
		cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
	}

	if ((flags & SOF_FORKED_DIAL)) {
		myflags |= SOF_NOBLOCK;
	}


	if (switch_ivr_originate(session, new_session, &cause, dest, timelimit, NULL,
							 cid_name_override, cid_num_override, NULL, var_event, myflags, cancel_cause) == SWITCH_STATUS_SUCCESS) {
		const char *context;
		switch_caller_profile_t *cp;

		new_channel = switch_core_session_get_channel(*new_session);

		if ((context = switch_channel_get_variable(new_channel, "group_context"))) {
			if ((cp = switch_channel_get_caller_profile(new_channel))) {
				cp->context = switch_core_strdup(cp->pool, context);
			}
		}
		switch_core_session_rwunlock(*new_session);
	}


  done:

	if (dest && dest != template) {
		switch_safe_free(dest);
	}

	switch_safe_free(template);
	switch_safe_free(group);
	switch_safe_free(dup_domain);

	if (cause == SWITCH_CAUSE_NONE) {
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	return cause;
}



/* fake chan_user */
switch_endpoint_interface_t *user_endpoint_interface;
static switch_call_cause_t user_outgoing_channel(switch_core_session_t *session,
												 switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause);
switch_io_routines_t user_io_routines = {
	/*.outgoing_channel */ user_outgoing_channel
};

static switch_call_cause_t user_outgoing_channel(switch_core_session_t *session,
												 switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause)
{
	switch_xml_t x_user = NULL, x_param, x_params;
	char *user = NULL, *domain = NULL, *dup_domain = NULL, *dialed_user = NULL;
	const char *dest = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	unsigned int timelimit = 60;
	switch_channel_t *new_channel = NULL;
	switch_event_t *params = NULL, *var_event_orig = var_event;
	char stupid[128] = "";
	const char *skip = NULL, *var = NULL;

	if (zstr(outbound_profile->destination_number)) {
		goto done;
	}

	user = strdup(outbound_profile->destination_number);

	if (!user)
		goto done;

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		domain = switch_core_get_domain(SWITCH_TRUE);
		dup_domain = domain;
	}

	if (!domain) {
		goto done;
	}


	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "as_channel", "true");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "user_call");

	if (var_event) {
		switch_event_merge(params, var_event);
	}

	if (var_event && (skip = switch_event_get_header(var_event, "user_recurse_variables")) && switch_false(skip)) {
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		var_event = NULL;
	}
	
	if (switch_xml_locate_user_merged("id", user, domain, NULL, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", user, domain);
		cause = SWITCH_CAUSE_SUBSCRIBER_ABSENT;
		goto done;
	}

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (!strcasecmp(pvar, "dial-string")) {
				dest = val;
			} else if (!strncasecmp(pvar, "dial-var-", 9)) {
				if (!var_event) {
					switch_event_create(&var_event, SWITCH_EVENT_GENERAL);
				} else {
					switch_event_del_header(var_event, pvar + 9);
				}
				switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, pvar + 9, val);
			}
		}
	}

	dialed_user = (char *)switch_xml_attr(x_user, "id");

	if (var_event) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_user", dialed_user);
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
		if (!zstr(dest) && !strstr(dest, "presence_id=")) {
			switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, "presence_id", "%s@%s", dialed_user, domain);
		}
	}

	if (!dest) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No dial-string available, please check your user directory.\n");
		cause = SWITCH_CAUSE_MANDATORY_IE_MISSING;
	} else {
		const char *varval;
		char *d_dest = NULL;
		switch_channel_t *channel;
		switch_originate_flag_t myflags = SOF_NONE;
		char *cid_name_override = NULL;
		char *cid_num_override = NULL;

		if (var_event) {
			cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
			cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
		}

		if (session) {
			channel = switch_core_session_get_channel(session);
			if ((varval = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE))
				|| (var_event && (varval = switch_event_get_header(var_event, "leg_timeout")))) {
				timelimit = atoi(varval);
			}

			switch_channel_set_variable(channel, "dialed_user", dialed_user);
			switch_channel_set_variable(channel, "dialed_domain", domain);

			d_dest = switch_channel_expand_variables(channel, dest);

		} else {
			switch_event_t *event = NULL;

			if (var_event) {
				switch_event_dup(&event, var_event);
				switch_event_del_header(event, "dialed_user");
				switch_event_del_header(event, "dialed_domain");
				if ((varval = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) ||
					(varval = switch_event_get_header(var_event, "leg_timeout"))) {
					timelimit = atoi(varval);
				}
			} else {
				switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
				switch_assert(event);
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_user", dialed_user);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
			d_dest = switch_event_expand_headers(event, dest);
			switch_event_destroy(&event);
		}

		if ((flags & SOF_NO_LIMITS)) {
			myflags |= SOF_NO_LIMITS;
		}

		if ((flags & SOF_FORKED_DIAL)) {
			myflags |= SOF_NOBLOCK;
		}


		switch_snprintf(stupid, sizeof(stupid), "user/%s", dialed_user);
		if (switch_stristr(stupid, d_dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Waddya Daft? You almost called '%s' in an infinate loop!\n",
							  stupid);
			cause = SWITCH_CAUSE_INVALID_IE_CONTENTS;
		} else if (switch_ivr_originate(session, new_session, &cause, d_dest, timelimit, NULL,
										cid_name_override, cid_num_override, outbound_profile, var_event, myflags,
										cancel_cause) == SWITCH_STATUS_SUCCESS) {
			const char *context;
			switch_caller_profile_t *cp;

			if (var_event) {
				switch_event_del_header(var_event, "origination_uuid");
			}


			new_channel = switch_core_session_get_channel(*new_session);

			if ((context = switch_channel_get_variable(new_channel, "user_context"))) {
				if ((cp = switch_channel_get_caller_profile(new_channel))) {
					cp->context = switch_core_strdup(cp->pool, context);
				}
			}
			switch_core_session_rwunlock(*new_session);
		}

		if (d_dest != dest) {
			switch_safe_free(d_dest);
		}
	}
	
	if (new_channel && x_user) {
		if ((x_params = switch_xml_child(x_user, "variables"))) {
			for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
				const char *pvar = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_channel_set_variable(new_channel, pvar, val);
			}
		}
	}

  done:

	if (x_user) {
		switch_xml_free(x_user);
	}

	if (params) {
		switch_event_destroy(&params);
	}

	if (var_event && var_event_orig != var_event) {
		switch_event_destroy(&var_event);
	}

	switch_safe_free(user);
	switch_safe_free(dup_domain);

	return cause;
}

#define HOLD_SYNTAX "[<display message>]"
SWITCH_STANDARD_APP(hold_function)
{
	switch_ivr_hold_uuid(switch_core_session_get_uuid(session), data, 1);
}

#define UNHOLD_SYNTAX ""
SWITCH_STANDARD_APP(unhold_function)
{
	switch_ivr_unhold_uuid(switch_core_session_get_uuid(session));
}

SWITCH_STANDARD_APP(novideo_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_NOVIDEO);
}

SWITCH_STANDARD_APP(verbose_events_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_VERBOSE_EVENTS);
}

SWITCH_STANDARD_APP(cng_plc_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_CNG_PLC);
}

SWITCH_STANDARD_APP(early_hangup_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_EARLY_HANGUP);
}

#define WAIT_FOR_SILENCE_SYNTAX "<silence_thresh> <silence_hits> <listen_hits> <timeout_ms> [<file>]"
SWITCH_STANDARD_APP(wait_for_silence_function)
{
	char *argv[5] = { 0 };
	uint32_t thresh, silence_hits, listen_hits, timeout_ms = 0;
	int argc;
	char *lbuf = NULL;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 3) {
		thresh = atoi(argv[0]);
		silence_hits = atoi(argv[1]);
		listen_hits = atoi(argv[2]);

		if (argv[3]) {
			timeout_ms = switch_atoui(argv[3]);
		}

		if (thresh > 0 && silence_hits > 0 && listen_hits > 0) {
			switch_ivr_wait_for_silence(session, thresh, silence_hits, listen_hits, timeout_ms, argv[4]);
			return;
		}

	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", WAIT_FOR_SILENCE_SYNTAX);
}

static switch_status_t event_chat_send(switch_event_t *message_event)
									   
{
	switch_event_t *event;
	const char *to;

	switch_event_dup(&event, message_event);
	event->event_id = SWITCH_EVENT_RECV_MESSAGE;

	if ((to = switch_event_get_header(event, "to"))) {
		char *v;
		if ((v = switch_core_get_variable_dup(to))) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Command", v);
			free(v);
		}
	}

	if (switch_event_fire(&event) == SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	switch_event_destroy(&event);

	return SWITCH_STATUS_MEMERR;
}

static switch_status_t api_chat_send(switch_event_t *message_event)
{
	const char *proto;
	const char *from; 
	const char *to;
	//const char *subject;
	//const char *body;
	const char *type;
	const char *hint;

	proto = switch_event_get_header(message_event, "proto");
	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	//subject = switch_event_get_header(message_event, "subject");
	//body = switch_event_get_body(message_event);
	type = switch_event_get_header(message_event, "type");
	hint = switch_event_get_header(message_event, "hint");	


	if (to) {
		char *v = NULL;
		switch_stream_handle_t stream = { 0 };
		char *cmd = NULL, *arg;

		if (!(v = switch_core_get_variable_dup(to))) {
			v = strdup(to);
		}

		cmd = v;
		switch_assert(cmd);

		switch_url_decode(cmd);

		if ((arg = strchr(cmd, ' '))) {
			*arg++ = '\0';
		}

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg, NULL, &stream);

		if (proto) {
			switch_core_chat_send_args(proto, "api", to, hint && strchr(hint, '/') ? hint : from, !zstr(type) ? type : NULL, (char *) stream.data, NULL, NULL, SWITCH_TRUE);
		}

		switch_safe_free(stream.data);

		free(cmd);

	}

	return SWITCH_STATUS_SUCCESS;
}


#define SESSION_LOGLEVEL_SYNTAX "<level>"
SWITCH_STANDARD_APP(session_loglevel_function)
{
	if (!zstr(data)) {
		switch_log_level_t level = switch_log_str2level(data);

		if (level == SWITCH_LOG_INVALID) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid log level: %s\n", data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting log level \"%s\" on session\n", switch_log_level2str(level));
			switch_core_session_set_loglevel(session, level);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No log level specified\n");
	}
}

/* LIMIT STUFF */
#define LIMIT_USAGE "<backend> <realm> <id> [<max>[/interval]] [number [dialplan [context]]]"
#define LIMIT_DESC "limit access to a resource and transfer to an extension if the limit is exceeded"
SWITCH_STANDARD_APP(limit_function)
{
	int argc = 0;
	char *argv[7] = { 0 };
	char *mydata = NULL;
	char *backend = NULL;
	char *realm = NULL;
	char *id = NULL;
	char *xfer_exten = NULL;
	int max = -1;
	int interval = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/* Parse application data  */
	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	backend = argv[0];

	/* must have at least one item */
	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE: limit %s\n", LIMIT_USAGE);
		return;
	}
	
	/* if this is an invalid backend, fallback to db backend */
	/* TODO: remove this when we can! */
	if (switch_true(switch_channel_get_variable(channel, "switch_limit_backwards_compat_flag")) && 
			!switch_loadable_module_get_limit_interface(backend)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown backend '%s'.  To maintain backwards compatability, falling back on db backend and shifting argumens. Either update your diaplan to include the backend, fix the typo, or load the appropriate limit implementation module.\n", backend);
		mydata = switch_core_session_sprintf(session, "db %s", data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		backend = argv[0];
	}

	if (argc < 3) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE: limit %s\n", LIMIT_USAGE);
		return;
	}

	realm = argv[1];
	id = argv[2];

	/* If max is omitted or negative, only act as a counter and skip maximum checks */
	if (argc > 3) {
		if (argv[3][0] == '-') {
			max = -1;
		} else {
			char *szinterval = NULL;
			if ((szinterval = strchr(argv[3], '/'))) {
				*szinterval++ = '\0';
				interval = atoi(szinterval);
			}

			max = atoi(argv[3]);

			if (max < 0) {
				max = 0;
			}
		}
	}

	if (argc > 4) {
		xfer_exten = argv[4];
	} else {
		xfer_exten = LIMIT_DEF_XFER_EXTEN;
	}

	if (switch_limit_incr(backend, session, realm, id, max, interval) != SWITCH_STATUS_SUCCESS) {
		/* Limit exceeded */
		if (*xfer_exten == '!') {
			switch_channel_hangup(channel, switch_channel_str2cause(xfer_exten + 1));
		} else {
			switch_ivr_session_transfer(session, xfer_exten, argv[5], argv[6]);
		}
	}
}

#define LIMIT_HASH_USAGE "<realm> <id> [<max>[/interval]] [number [dialplan [context]]]"
#define LIMIT_HASH_DESC "DEPRECATED: limit access to a resource and transfer to an extension if the limit is exceeded"
SWITCH_STANDARD_APP(limit_hash_function)
{
	char *mydata = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_true(switch_channel_get_variable(channel, "switch_limit_backwards_compat_flag"))) {
		mydata = switch_core_session_sprintf(session, "hash %s", data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Using deprecated 'limit_hash' api: Please use 'limit hash'.\n");
		limit_function(session, mydata);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "'limit_hash' (deprecated) is only available after loading mod_limit.\n");
	}
}

#define LIMITEXECUTE_USAGE "<backend> <realm> <id> <max>[/interval] <application> [application arguments]"
#define LIMITEXECUTE_DESC "limit access to a resource. the specified application will only be executed if the resource is available"
SWITCH_STANDARD_APP(limit_execute_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	char *backend = NULL;
	char *realm = NULL;
	char *id = NULL;
	char *app = NULL;
	char *app_arg = NULL;
	int max = -1;
	int interval = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/* Parse application data  */
	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	/* backwards compat version, if we have 5, just prepend with db and reparse */
	if (switch_true(switch_channel_get_variable(channel, "switch_limit_backwards_compat_flag")) && 
			argc == 5) {
		mydata = switch_core_session_sprintf(session, "db %s", data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Using deprecated limit api: Please specify backend.  Defaulting to 'db' backend.\n");
	}

	if (argc < 6) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE: limit_execute %s\n", LIMITEXECUTE_USAGE);
		return;
	}

	backend = argv[0];
	realm = argv[1];
	id = argv[2];

	/* Accept '-' as unlimited (act as counter) */
	if (argv[3][0] == '-') {
		max = -1;
	} else {
		char *szinterval = NULL;

		if ((szinterval = strchr(argv[3], '/'))) {
			*szinterval++ = '\0';
			interval = atoi(szinterval);
		}

		max = atoi(argv[3]);

		if (max < 0) {
			max = 0;
		}
	}

	app = argv[4];
	app_arg = argv[5];

	if (zstr(app)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing application\n");
		return;
	}

	if (switch_limit_incr(backend, session, realm, id, max, interval) == SWITCH_STATUS_SUCCESS) {
		switch_core_session_execute_application(session, app, app_arg);
		/* Only release the resource if we are still in CS_EXECUTE */
		if (switch_channel_get_state(switch_core_session_get_channel(session)) == CS_EXECUTE) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "immediately releasing\n");
			switch_limit_release(backend, session, realm, id);			
		}
	}
}

#define LIMITHASHEXECUTE_USAGE "<realm> <id> <max>[/interval] <application> [application arguments]"
#define LIMITHASHEXECUTE_DESC "DEPRECATED: limit access to a resource. the specified application will only be executed if the resource is available"
SWITCH_STANDARD_APP(limit_hash_execute_function)
{
	char *mydata = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_true(switch_channel_get_variable(channel, "switch_limit_backwards_compat_flag"))) {
		mydata = switch_core_session_sprintf(session, "hash %s", data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Using deprecated 'limit_hash_execute' api: Please use 'limit_execute hash'.\n");
		limit_execute_function(session, mydata);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "'limit_hash_execute' (deprecated) is only available after loading mod_limit.\n");
	}
}



/* FILE STRING INTERFACE */

/* for apr_pstrcat */
#define DEFAULT_PREBUFFER_SIZE 1024 * 64

struct file_string_audio_col {
	switch_audio_col_t col;
	char *value;
	struct file_string_audio_col *next;
};

typedef struct file_string_audio_col file_string_audio_col_t;

struct file_string_context {
	char *file;
	char *argv[128];
	int argc;
	int index;
	int samples;
	switch_file_handle_t fh;
	file_string_audio_col_t *audio_cols;
};

typedef struct file_string_context file_string_context_t;

#define FILE_STRING_OPEN "filestring::open"
#define FILE_STRING_CLOSE "filestring::close"
#define FILE_STRING_FAIL "filestring::fail"

static switch_status_t next_file(switch_file_handle_t *handle)
{
	file_string_context_t *context = handle->private_info;
	char *file;
	const char *prefix = handle->prefix;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_t *event = NULL;

  top:

	context->index++;

	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FILE_STRING_CLOSE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", context->argv[(context->index - 1)]);
			switch_event_fire(&event);
		}

		switch_core_file_close(&context->fh);
	}

	if (context->index >= context->argc) {
		return SWITCH_STATUS_FALSE;
	}


	if (!prefix) {
		if (!(prefix = switch_core_get_variable_pdup("sound_prefix", handle->memory_pool))) {
			prefix = SWITCH_GLOBAL_dirs.sounds_dir;
		}
	}

	if (!prefix || switch_is_file_path(context->argv[context->index])) {
		file = context->argv[context->index];
	} else {
		file = switch_core_sprintf(handle->memory_pool, "%s%s%s", prefix, SWITCH_PATH_SEPARATOR, context->argv[context->index]);
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		char *path = switch_core_strdup(handle->memory_pool, file);
		char *p;

		if ((p = strrchr(path, *SWITCH_PATH_SEPARATOR))) {
			*p = '\0';
			if (switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, handle->memory_pool) != SWITCH_STATUS_SUCCESS) {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FILE_STRING_FAIL) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", context->argv[context->index]);
					switch_event_fire(&event);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", path);
				return SWITCH_STATUS_FALSE;
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error finding the folder path section in '%s'\n", path);
		}
	}

	if (switch_core_file_open(&context->fh, file, handle->channels, handle->samplerate, handle->flags, NULL) != SWITCH_STATUS_SUCCESS) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FILE_STRING_FAIL) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", context->argv[context->index]);
			switch_event_fire(&event);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open file %s\n", file);
		if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
			switch_file_remove(file, handle->memory_pool);
		}
		goto top;
	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FILE_STRING_OPEN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", context->argv[context->index]);
		switch_event_fire(&event);
	}

	if (handle->dbuflen) {
		free(handle->dbuf);
		handle->dbuflen = 0;
		handle->dbuf = NULL;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		file_string_audio_col_t *col_ptr = context->audio_cols;

		while (col_ptr) {
			switch_core_file_set_string(&context->fh, col_ptr->col, col_ptr->value);
			col_ptr = col_ptr->next;
		}

		if (context->file && switch_test_flag(handle, SWITCH_FILE_DATA_SHORT)) { /* TODO handle other data type flags */
			switch_size_t len;			
			uint16_t buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_status_t stat;
			switch_file_handle_t fh = { 0 };

			if ((stat = switch_core_file_open(&fh, context->file, handle->channels, handle->samplerate, 
												SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL)) == SWITCH_STATUS_SUCCESS) {
					do {
						len = SWITCH_RECOMMENDED_BUFFER_SIZE / handle->channels;
						if ((stat = switch_core_file_read(&fh, buf, &len)) == SWITCH_STATUS_SUCCESS) {
							stat = switch_core_file_write(&context->fh, buf, &len);
						}
					} while (stat == SWITCH_STATUS_SUCCESS);

					switch_core_file_close(&fh);				
					switch_file_remove(context->file, handle->memory_pool);

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open %s\n", context->file);
			}
		}
	}
	context->file = file;

	handle->samples = context->fh.samples;
	handle->cur_samplerate = context->fh.samplerate;
	handle->cur_channels = context->fh.real_channels;
	handle->format = context->fh.format;
	handle->sections = context->fh.sections;
	handle->seekable = context->fh.seekable;
	handle->speed = context->fh.speed;
	handle->interval = context->fh.interval;
	handle->max_samples = 0;


	if (switch_test_flag((&context->fh), SWITCH_FILE_NATIVE)) {
		switch_set_flag_locked(handle, SWITCH_FILE_NATIVE);
	} else {
		switch_clear_flag_locked(handle, SWITCH_FILE_NATIVE);
	}


	if (!switch_test_flag(handle, SWITCH_FILE_NATIVE)) {
		if (context->index == 0) {
			context->samples = (handle->samplerate / 1000) * 250;
		}
	}

	return status;
}


static switch_status_t file_string_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	file_string_context_t *context = handle->private_info;

	if (samples == 0 && whence == SEEK_SET) {
		context->index = -1;
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!handle->seekable) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File is not seekable\n");
		return SWITCH_STATUS_NOTIMPL;
	}

	return switch_core_file_seek(&context->fh, cur_sample, samples, whence);
}

static switch_status_t file_string_file_open(switch_file_handle_t *handle, const char *path)
{
	file_string_context_t *context;
	char *file_dup;

	context = switch_core_alloc(handle->memory_pool, sizeof(*context));

	file_dup = switch_core_strdup(handle->memory_pool, path);
	context->argc = switch_separate_string(file_dup, '!', context->argv, (sizeof(context->argv) / sizeof(context->argv[0])));
	context->index = -1;

	handle->private_info = context;
	handle->pre_buffer_datalen = 0;

	return next_file(handle);
}

static switch_status_t file_string_file_close(switch_file_handle_t *handle)
{
	file_string_context_t *context = handle->private_info;

	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&context->fh);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t file_string_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	file_string_context_t *context = handle->private_info;
	file_string_audio_col_t *col_ptr = context->audio_cols;

	while (col_ptr && col != col_ptr->col) { 
		col_ptr = col_ptr->next;
	}

	if (col_ptr) {
		col_ptr->value = switch_core_strdup(handle->memory_pool, string);
	} else {
		col_ptr = switch_core_alloc(handle->memory_pool, sizeof(*col_ptr));
		col_ptr->value = switch_core_strdup(handle->memory_pool, string);
		col_ptr->col = col;
		col_ptr->next = context->audio_cols;
		context->audio_cols = col_ptr;
	}
	
	return switch_core_file_set_string(&context->fh, col, string);
}

static switch_status_t file_string_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	file_string_context_t *context = handle->private_info;

	return switch_core_file_get_string(&context->fh, col, string);
}


static switch_status_t file_string_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	file_string_context_t *context = handle->private_info;
	switch_status_t status;
	size_t llen = *len;

	if (context->samples > 0) {
		if (*len > (size_t) context->samples) {
			*len = context->samples;
		}

		context->samples -= (int) *len;
		memset(data, 255, *len *2);
		status = SWITCH_STATUS_SUCCESS;
	} else {
		status = switch_core_file_read(&context->fh, data, len);
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		if ((status = next_file(handle)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		if (switch_test_flag(handle, SWITCH_FILE_BREAK_ON_CHANGE)) {
			*len = 0;
			status = SWITCH_STATUS_BREAK;
		} else {
			*len = llen;
			status = switch_core_file_read(&context->fh, data, len);
		}
	}

	return status;
}


static switch_status_t file_string_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	file_string_context_t *context = handle->private_info;
	switch_status_t status;
	size_t llen = *len;

	status = switch_core_file_write(&context->fh, data, len);

	if (status != SWITCH_STATUS_SUCCESS) {
		if ((status = next_file(handle)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		*len = llen;
		status = switch_core_file_write(&context->fh, data, len);
	}
	return status;
}

static switch_status_t file_url_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_file_handle_t *fh = handle->private_info;
	return switch_core_file_seek(fh, cur_sample, samples, whence);
}

static switch_status_t file_url_file_close(switch_file_handle_t *handle)
{
	switch_file_handle_t *fh = handle->private_info;
	if (switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		switch_core_file_close(fh);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t file_url_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_file_handle_t *fh = handle->private_info;
	return switch_core_file_read(fh, data, len);
}

static switch_status_t file_url_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_file_handle_t *fh = switch_core_alloc(handle->memory_pool, sizeof(*fh));
	switch_status_t status;
	char *url_host;
	char *url_path;

	if (zstr(path)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NULL path\n");
		return SWITCH_STATUS_FALSE;
	}

	/* parse and check host */
	url_host = switch_core_strdup(handle->memory_pool, path);
	if (!(url_path = strchr(url_host, '/'))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing path\n");
		return SWITCH_STATUS_FALSE;
	}
	*url_path = '\0';
	/* TODO allow this host */
	if (!zstr(url_host) && strcasecmp(url_host, "localhost")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "not localhost\n");
		return SWITCH_STATUS_FALSE;
	}

	/* decode and check path */
	url_path++;
	if (zstr(url_path)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "empty path\n");
		return SWITCH_STATUS_FALSE;
	}
	if (strstr(url_path, "%2f") || strstr(url_path, "%2F")) {
		/* don't allow %2f or %2F encoding (/) */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encoded slash is not allowed\n");
		return SWITCH_STATUS_FALSE;
	}
	url_path = switch_core_sprintf(handle->memory_pool, "/%s", url_path);
	switch_url_decode(url_path);

	/* TODO convert to native file separators? */

	handle->private_info = fh;
	status = switch_core_file_open(fh, url_path, handle->channels, handle->samplerate, handle->flags, NULL);
	if (status == SWITCH_STATUS_SUCCESS) {
		handle->samples = fh->samples;
		handle->cur_samplerate = fh->samplerate;
		handle->cur_channels = fh->real_channels;
		handle->format = fh->format;
		handle->sections = fh->sections;
		handle->seekable = fh->seekable;
		handle->speed = fh->speed;
		handle->interval = fh->interval;
		handle->max_samples = 0;

		if (switch_test_flag(fh, SWITCH_FILE_NATIVE)) {
			switch_set_flag_locked(handle, SWITCH_FILE_NATIVE);
		} else {
			switch_clear_flag_locked(handle, SWITCH_FILE_NATIVE);
		}
	}
	return status;
}

static switch_status_t file_url_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_file_handle_t *fh = handle->private_info;
	return switch_core_file_write(fh, data, len);
}

/* Registration */

static char *file_string_supported_formats[SWITCH_MAX_CODECS] = { 0 };
static char *file_url_supported_formats[SWITCH_MAX_CODECS] = { 0 };


/* /FILE STRING INTERFACE */


SWITCH_STANDARD_APP(blind_transfer_ack_function)
{
	switch_bool_t val = 0;

	if (data) {
		val = (switch_bool_t)switch_true((char *) data);
	}

	switch_ivr_blind_transfer_ack(session, val);
}

/* /// mutex /// */

typedef struct mutex_node_s {
	char *uuid;
	struct mutex_node_s *next;
} mutex_node_t;

typedef enum {
	MUTEX_FLAG_WAIT = (1 << 0),
	MUTEX_FLAG_SET = (1 << 1)
} mutex_flag_t;

struct read_frame_data {
	const char *dp;
	const char *exten;
	const char *context;
	const char *key;
	long to;
};

typedef struct master_mutex_s {
	mutex_node_t *list;
	char *key;
} master_mutex_t;

static switch_status_t mutex_hanguphook(switch_core_session_t *session);
static void advance(master_mutex_t *master, switch_bool_t pop_current);

static switch_status_t read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct read_frame_data *rf = (struct read_frame_data *) user_data;

	if (rf->to && --rf->to <= 0) {
		rf->to = -1;
		return SWITCH_STATUS_FALSE;
	}

	return switch_channel_test_app_flag_key(rf->key, channel, MUTEX_FLAG_WAIT) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	
}

static void free_node(mutex_node_t **npp)
{
	mutex_node_t *np;

	if (npp) {
		np = *npp;
		*npp = NULL;
		switch_safe_free(np->uuid);
		free(np);
	}
}

static void cancel(switch_core_session_t *session, master_mutex_t *master)
{
	mutex_node_t *np, *lp = NULL;
	const char *uuid = switch_core_session_get_uuid(session);

	switch_mutex_lock(globals.mutex_mutex);
	for (np = master->list; np; np = np->next) {
		if (np && !strcmp(np->uuid, uuid)) {
			switch_core_event_hook_remove_state_change(session, mutex_hanguphook);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s %s mutex %s canceled\n", 
							  switch_core_session_get_uuid(session),
							  switch_core_session_get_name(session), master->key);

			if (lp) {
				lp->next = np->next;
			} else {
				if ((master->list = np->next)) {
					advance(master, SWITCH_FALSE);
				}
			}

			free_node(&np);
			
			break;
		}

		lp = np;
	}

	switch_mutex_unlock(globals.mutex_mutex);

}

static void advance(master_mutex_t *master, switch_bool_t pop_current)
{

	switch_mutex_lock(globals.mutex_mutex);

	if (!master || !master->list) {
		goto end;
	}
	
	while (master->list) {
		mutex_node_t *np;


		if (!pop_current) {
			pop_current++;
		} else {
			np = master->list;
			master->list = master->list->next;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ADVANCE POP %p\n", (void *)np);
			free_node(&np);
		}
	

		if (master->list) {
			switch_core_session_t *session;

			if ((session = switch_core_session_locate(master->list->uuid))) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
								  "%s mutex %s advanced\n", switch_channel_get_name(channel), master->key);
				switch_channel_set_app_flag_key(master->key, channel, MUTEX_FLAG_SET);
				switch_channel_clear_app_flag_key(master->key, channel, MUTEX_FLAG_WAIT);
				switch_core_event_hook_add_state_change(session, mutex_hanguphook);
				switch_core_session_rwunlock(session);
				break;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "uuid %s already gone\n", master->list->uuid);
			}
		}
	}


 end:

	switch_mutex_unlock(globals.mutex_mutex);

	
}

static void confirm(switch_core_session_t *session, master_mutex_t *master)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!master) {
		if (!(master = switch_channel_get_private(channel, "_mutex_master"))) {
			return;
		}
	}

	switch_mutex_lock(globals.mutex_mutex);

	if (master->list) {
		if (!strcmp(master->list->uuid, switch_core_session_get_uuid(session))) {
			switch_channel_clear_app_flag_key(master->key, channel, MUTEX_FLAG_SET);
			switch_core_event_hook_remove_state_change(session, mutex_hanguphook);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s %s mutex %s cleared\n", 
							  switch_core_session_get_uuid(session),
							  switch_channel_get_name(channel), master->key);
			advance(master, SWITCH_TRUE);
		} else {
			cancel(session, master);
		}
	}

	switch_mutex_unlock(globals.mutex_mutex);
}



static switch_status_t mutex_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state != CS_HANGUP) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s mutex hangup hook\n", switch_channel_get_name(channel));

	confirm(session, NULL);
	switch_core_event_hook_remove_state_change(session, mutex_hanguphook);

	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t do_mutex(switch_core_session_t *session, const char *key, switch_bool_t on)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *feedback, *var;
	switch_input_args_t args = { 0 };
	master_mutex_t *master = NULL;
	mutex_node_t *node, *np;
	int used;
	struct read_frame_data rf = { 0 };
	long to_val = 0;

	switch_mutex_lock(globals.mutex_mutex);
	used = switch_channel_test_app_flag_key(key, channel, MUTEX_FLAG_WAIT) || switch_channel_test_app_flag_key(key, channel, MUTEX_FLAG_SET);

	if ((on && used) || (!on && !used)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "INVALID STATE\n");
		switch_mutex_unlock(globals.mutex_mutex);
		return SWITCH_FALSE;
	}
	
	if (!(master = switch_core_hash_find(globals.mutex_hash, key))) {
		master = switch_core_alloc(globals.pool, sizeof(*master));
		master->key = switch_core_strdup(globals.pool, key);
		master->list = NULL;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NEW MASTER %s %p\n", key, (void *) master);
		switch_core_hash_insert(globals.mutex_hash, key, master);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "EXIST MASTER %s %p\n", key, (void *) master);
	}
		
	if (on) {

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "HIT ON\n");

		switch_zmalloc(node, sizeof(*node));
		node->uuid = strdup(switch_core_session_get_uuid(session));
		node->next = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHECK MASTER LIST %p\n", (void *) master->list);

		for (np = master->list; np && np->next; np = np->next);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "HIT ON np %p\n", (void *) np);

		if (np) {
			np->next = node;
			switch_channel_set_app_flag_key(key, channel, MUTEX_FLAG_WAIT);
		} else {
			master->list = node;
			switch_channel_set_app_flag_key(key, channel, MUTEX_FLAG_SET);
			switch_channel_clear_app_flag_key(key, channel, MUTEX_FLAG_WAIT);
			switch_channel_set_private(channel, "_mutex_master", master);
			switch_core_event_hook_add_state_change(session, mutex_hanguphook);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s %s mutex %s acquired\n", 
							  switch_core_session_get_uuid(session),
							  switch_channel_get_name(channel), key);
			switch_mutex_unlock(globals.mutex_mutex);
			return SWITCH_TRUE;
		}
	} else {
		confirm(session, master);

		switch_mutex_unlock(globals.mutex_mutex);
		return SWITCH_TRUE;
	}

	switch_mutex_unlock(globals.mutex_mutex);
	
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s mutex %s is busy, waiting...\n", switch_channel_get_name(channel), key);

	if ((feedback = switch_channel_get_variable(channel, "mutex_feedback"))) {
		if (!strcasecmp(feedback, "silence")) {
			feedback = "silence_stream://-1";
		}
	}

	if ((rf.exten = switch_channel_get_variable(channel, "mutex_orbit_exten"))) {
		to_val = 60;
	}
	
	if ((var = switch_channel_get_variable(channel, "mutex_timeout"))) {
		long tmp = atol(var);
		
		if (tmp > 0) {
			to_val = tmp;
		}
	}
	
	if (to_val) {
		switch_codec_implementation_t read_impl;
		switch_core_session_get_read_impl(session, &read_impl);
		
		rf.to = (1000 / (read_impl.microseconds_per_packet / 1000)) * to_val;
		rf.dp = switch_channel_get_variable(channel, "mutex_orbit_dialplan");
		rf.context = switch_channel_get_variable(channel, "mutex_orbit_context");
	}

	rf.key = key;

	args.read_frame_callback = read_frame_callback;
	args.user_data = &rf;

	while(switch_channel_ready(channel) && switch_channel_test_app_flag_key(key, channel, MUTEX_FLAG_WAIT)) {
		switch_status_t st;

		if (feedback) {
			switch_channel_pre_answer(channel);
			st = switch_ivr_play_file(session, NULL, feedback, &args);
		} else {
			if ((st = switch_ivr_sleep(session, 20, SWITCH_FALSE, NULL)) == SWITCH_STATUS_SUCCESS) {
				st = read_frame_callback(session, NULL, &rf);
			}
		}

		if (st != SWITCH_STATUS_SUCCESS) {
			break;
		}
	}

	switch_mutex_lock(globals.mutex_mutex);
	if (switch_channel_test_app_flag_key(key, channel, MUTEX_FLAG_WAIT) || !switch_channel_up(channel)) {
		cancel(session, master);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s %s mutex %s acquired\n", 
						  switch_core_session_get_uuid(session),
						  switch_channel_get_name(channel), key);
		switch_core_event_hook_add_state_change(session, mutex_hanguphook);
		switch_channel_set_private(channel, "_mutex_master", master);
	}
	switch_mutex_unlock(globals.mutex_mutex);

	return SWITCH_TRUE;
}

#define MUTEX_SYNTAX "<keyname>[ on|off]"
SWITCH_STANDARD_APP(mutex_function)
{
	char *key;
	char *arg;
	switch_bool_t on = SWITCH_TRUE;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing keyname\n");
		return;
	}

	key = switch_core_session_sprintf(session, "_mutex_key_%s", (char *)data);

	if ((arg = strchr(key, ' '))) {
		*arg++ = '\0';

		if (!strcasecmp(arg, "off")) {
			on = SWITCH_FALSE;
		}
	}

	do_mutex(session, key, on);
	

}

/* /// mutex /// */

typedef struct page_data_s {
	uint32_t *counter;
	const char *dial_str;
	const char *dp;
	const char *context;
	const char *exten;
	const char *path;
	switch_event_t *var_event;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
} page_data_t;

static switch_status_t page_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state == CS_HANGUP) {
		page_data_t *pd;

		if ((pd = (page_data_t *) switch_channel_get_private(channel, "__PAGE_DATA"))) {
			uint32_t *counter = pd->counter;

			switch_mutex_lock(pd->mutex);
			(*counter)--;
			switch_mutex_unlock(pd->mutex);


		}

		switch_core_event_hook_remove_state_change(session, page_hanguphook);
	}

	return SWITCH_STATUS_SUCCESS;
}

void *SWITCH_THREAD_FUNC page_thread(switch_thread_t *thread, void *obj)
{
	page_data_t *mypd, *pd = (page_data_t *) obj;
	switch_core_session_t *session;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	uint32_t *counter = pd->counter;
	switch_memory_pool_t *pool = pd->pool;


	if (switch_ivr_originate(NULL, &session, &cause, pd->dial_str, 60, NULL, NULL, NULL, NULL, pd->var_event, SOF_NONE, NULL) == SWITCH_STATUS_SUCCESS) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		
		switch_channel_set_variable(channel, "page_file", pd->path);

		mypd = switch_core_session_alloc(session, sizeof(*mypd));
		mypd->counter = pd->counter;
		mypd->mutex = pd->mutex;
		switch_core_event_hook_add_state_change(session, page_hanguphook);
		switch_channel_set_private(channel, "__PAGE_DATA", mypd);
		switch_ivr_session_transfer(session, pd->exten, pd->dp, pd->context);
		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "originate failed: %s [%s]\n", switch_channel_cause2str(cause), pd->dial_str);
		switch_mutex_lock(pd->mutex);
		(*counter)--;
		switch_mutex_unlock(pd->mutex);
	}

	switch_event_safe_destroy(&pd->var_event);

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static void launch_call(const char *dial_str, 
						const char *path, const char *exten, const char *context, const char *dp, 
						switch_mutex_t *mutex, uint32_t *counter, switch_event_t **var_event)
{
	switch_thread_data_t *td;
	switch_memory_pool_t *pool;
	page_data_t *pd;
	
	switch_core_new_memory_pool(&pool);

	pd = switch_core_alloc(pool, sizeof(*pd));
	pd->pool = pool;
	pd->exten = switch_core_strdup(pool, exten);
	pd->context = switch_core_strdup(pool, context);
	pd->dp = switch_core_strdup(pool, dp);
	pd->dial_str = switch_core_strdup(pool, dial_str);
	pd->path = switch_core_strdup(pool, path);
	pd->mutex = mutex;

	if (var_event && *var_event) {
		switch_event_dup(&pd->var_event, *var_event);
		switch_event_destroy(var_event);
	}

	switch_mutex_lock(pd->mutex);
	(*counter)++;
	switch_mutex_unlock(pd->mutex);

	pd->counter = counter;

	td = switch_core_alloc(pool, sizeof(*td));
	td->func = page_thread;
	td->obj = pd;

	switch_thread_pool_launch_thread(&td);
	
}

typedef struct call_monitor_s {
	switch_memory_pool_t *pool;
	const char *path;
	char *data;
	const char *context;
	const char *exten;
	const char *dp;
	uint32_t chunk_size;
	int nuke;
} call_monitor_t;



void *SWITCH_THREAD_FUNC call_monitor_thread(switch_thread_t *thread, void *obj)
{
	call_monitor_t *cm = (call_monitor_t *) obj;
	uint32_t sent = 0;
	switch_mutex_t *mutex;
	uint32_t counter = 0;
	switch_memory_pool_t *pool = cm->pool;
	unsigned int size;
	char *argv[512] = { 0 };
	int busy = 0;
	switch_event_t *var_event = NULL;
	char *data;

	switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, cm->pool);

	if (switch_file_exists(cm->path, cm->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s does not exist!\n", cm->path);
		goto end;
	}

	data = cm->data;

	while (data && *data && *data == ' ') {
		data++;
	}
	
	while (*data == '<') {
		char *parsed = NULL;

		if (switch_event_create_brackets(data, '<', '>', ',', &var_event, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
			goto end;
		}

		data = parsed;
	}

	while (data && *data && *data == ' ') {
		data++;
	}

	if (!(size = switch_separate_string_string(data, SWITCH_ENT_ORIGINATE_DELIM, argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No channels specified.\n");
		goto end;
	}


	if (cm->chunk_size > size) {
		cm->chunk_size = size;
	}

	while (sent < size) {
		do {				
			switch_mutex_lock(mutex);				
			busy = (counter >= cm->chunk_size);
			switch_mutex_unlock(mutex);
			
			if (busy) {					
				switch_yield(100000);
			}												
			
		} while (busy);

		launch_call(argv[sent++], cm->path, cm->exten, cm->context, cm->dp, mutex, &counter, &var_event);
	}


 end:

	while(counter) {
		switch_mutex_lock(mutex);
		switch_mutex_unlock(mutex);
		switch_yield(100000);
	}

	if (cm->nuke && !zstr(cm->path)) {
		unlink(cm->path);
	}

    if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static void launch_call_monitor(const char *path, int del, const char *data, uint32_t chunk_size, const char *exten, const char *context, const char *dp)
{
	switch_thread_data_t *td;
	switch_memory_pool_t *pool;
	call_monitor_t *cm;
	
	switch_core_new_memory_pool(&pool);

	cm = switch_core_alloc(pool, sizeof(*cm));

	if (del) {
		cm->nuke = 1;
	}

	cm->pool = pool;
	cm->path = switch_core_strdup(pool, path);
	cm->data = switch_core_strdup(pool, data);
	cm->exten = switch_core_strdup(pool, exten);
	cm->context = switch_core_strdup(pool, context);
	cm->dp = switch_core_strdup(pool, dp);
	cm->chunk_size = chunk_size;

	td = switch_core_alloc(pool, sizeof(*td));
	td->func = call_monitor_thread;
	td->obj = cm;

	switch_thread_pool_launch_thread(&td);
	
}


#define PAGE_SYNTAX "<var1=val1,var2=val2><chan1>[:_:<chanN>]"
SWITCH_STANDARD_APP(page_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	uint32_t limit = 0;
	const char *path = NULL;
	switch_input_args_t args = { 0 };
	switch_file_handle_t fh = { 0 };
	uint32_t chunk_size = 10;
	const char *l = NULL;
	const char *tmp;
	int del = 0, rate;
	const char *exten;
	const char *context = NULL;
	const char *dp = "inline";
	const char *pdata = data;

	if (zstr(pdata)) {
		pdata = switch_channel_get_variable(channel, "page_data");
	}

	if (zstr(pdata)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No channels specified.\n");
		return;
	}


	exten = switch_channel_get_variable(channel, "page_exten");
	context = switch_channel_get_variable(channel, "page_context");

	if ((l = switch_channel_get_variable(channel, "page_dp"))) {
		dp = l;
	}
	

	l = switch_channel_get_variable(channel, "page_record_limit");
	
	if (l) {
		if (*l == '+') {
			l++;
		}
		if (l) {
			limit = switch_atoui(l);
		}
	}

	if ((l = switch_channel_get_variable(channel, "page_record_thresh"))) {
		fh.thresh = switch_atoui(l);
	}

	if ((l = switch_channel_get_variable(channel, "page_chunk_size"))) {
		uint32_t chunk = switch_atoui(l);

		if (chunk > 0) {
			chunk_size = chunk;
		}
	}

	if ((l = switch_channel_get_variable(channel, "page_record_silence_hits"))) {
		fh.silence_hits = switch_atoui(l);
	}

	if ((tmp = switch_channel_get_variable(channel, "record_rate"))) {
		rate = atoi(tmp);
		if (rate > 0) {
			fh.samplerate = rate;
		}
	}

	args.input_callback = on_dtmf;

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");


	if (!(path = switch_channel_get_variable(channel, "page_path"))) {
		const char *beep;

		path = switch_core_session_sprintf(session, "%s%s%s.wav", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, switch_core_session_get_uuid(session));
		del = 1;

		if (!(beep = switch_channel_get_variable(channel, "page_beep"))) {
			beep = "tone_stream://%(500,0, 620)";
		}

		switch_ivr_play_file(session, NULL, beep, NULL);
		

		switch_ivr_record_file(session, &fh, path, &args, limit);
	}

	if (zstr(exten)) {
		exten = switch_core_session_sprintf(session, "playback:%s", path);
	}
	
	if (switch_file_exists(path, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		launch_call_monitor(path, del, pdata, chunk_size, exten, context, dp);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "File %s does not exist\n", path);
	}

}


SWITCH_STANDARD_API(page_api_function)
{
	char *odata = NULL, *data = NULL;
	switch_event_t *var_event = NULL;
	const char *exten;
	char *oexten = NULL;
	const char *context = NULL;
	const char *dp = "inline";
	const char *pdata = data;
	const char *l;
	uint32_t chunk_size = 10;
	const char *path;


	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no data");
		goto end;
	}

	odata = strdup(cmd);
	data = odata;

	while (data && *data && *data == ' ') {
		data++;
	}
	
	while (*data == '(') {
		char *parsed = NULL;

		if (switch_event_create_brackets(data, '(', ')', ',', &var_event, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
			goto end;
		}

		data = parsed;
	}

	while (data && *data && *data == ' ') {
		data++;
	}

	if (!var_event) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		goto end;
	}	   

	pdata = data;

	if (zstr(pdata)) {
		pdata = switch_event_get_header(var_event, "page_data");
	}

	if (zstr(pdata)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No channels specified.\n");
		goto end;
	}


	exten = switch_event_get_header(var_event, "page_exten");
	context = switch_event_get_header(var_event, "page_context");

	if ((l = switch_event_get_header(var_event, "page_dp"))) {
		dp = l;
	}
	

	if ((l = switch_event_get_header(var_event, "page_chunk_size"))) {
		uint32_t tmp = switch_atoui(l);

		if (tmp > 0) {
			chunk_size = tmp;
		}
	}

	if (!(path = switch_event_get_header(var_event, "page_path"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No file specified.\n");
		goto end;
	}

	if (zstr(exten)) {
		oexten = switch_mprintf("playback:%s", path);
		exten = oexten;
	}
	
	if (switch_file_exists(path, NULL) == SWITCH_STATUS_SUCCESS) {
		launch_call_monitor(path, 0, pdata, chunk_size, exten, context, dp);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s does not exist\n", path);
	}


 end:
	

	switch_safe_free(odata);
	switch_safe_free(oexten);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Convert DTMF source to human readable string
 */
static const char *to_dtmf_source_string(switch_dtmf_source_t source)
{
	switch(source) {
		case SWITCH_DTMF_ENDPOINT: return "SIP INFO";
		case SWITCH_DTMF_INBAND_AUDIO: return "INBAND";
		case SWITCH_DTMF_RTP: return "2833";
		case SWITCH_DTMF_UNKNOWN: return "UNKNOWN";
		case SWITCH_DTMF_APP: return "APP";
	}
	return "UNKNOWN";
}

struct deduplicate_dtmf_filter {
	int only_rtp;
	char last_dtmf;
	switch_dtmf_source_t last_dtmf_source;
};

/**
 * Filter incoming DTMF and ignore any duplicates
 */
static switch_status_t deduplicate_recv_dtmf_hook(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int only_rtp = 0;
	struct deduplicate_dtmf_filter *filter = switch_channel_get_private(switch_core_session_get_channel(session), "deduplicate_dtmf_filter");

	if (!filter) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Accept %s digit %c: deduplicate filter missing!\n", to_dtmf_source_string(dtmf->source), dtmf->digit);
		return SWITCH_STATUS_SUCCESS;
	}

	/* remember current state as it might change */
	only_rtp = filter->only_rtp;

	/* RTP DTMF is preferred over all others- and if it's demonstrated to be available, inband / info detection is disabled */
	if (only_rtp) {
		switch (dtmf->source) {
			case SWITCH_DTMF_ENDPOINT:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_endpoint", "true");
				break;
			case SWITCH_DTMF_INBAND_AUDIO:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_inband", "true");
				break;
			case SWITCH_DTMF_RTP:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_rtp", "true");
				/* pass through */
			case SWITCH_DTMF_UNKNOWN:
			case SWITCH_DTMF_APP:
				/* always allow */
				status = SWITCH_STATUS_SUCCESS;
				break;
		}
	} else {
		/* accept everything except duplicates until RTP digit is detected */
		switch (dtmf->source) {
			case SWITCH_DTMF_INBAND_AUDIO:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_inband", "true");
				break;
			case SWITCH_DTMF_RTP:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_rtp", "true");
				/* change state to only allow RTP events */
				filter->only_rtp = 1;

				/* stop inband detector */
				switch_ivr_broadcast(switch_core_session_get_uuid(session), "spandsp_stop_dtmf::", SMF_ECHO_ALEG);
				break;
			case SWITCH_DTMF_ENDPOINT:
				switch_channel_set_variable(switch_core_session_get_channel(session), "deduplicate_dtmf_seen_endpoint", "true");
				break;
			case SWITCH_DTMF_UNKNOWN:
			case SWITCH_DTMF_APP:
				/* always allow */
				status = SWITCH_STATUS_SUCCESS;
				break;
		}

		/* make sure not a duplicate DTMF */
		if (filter->last_dtmf_source == dtmf->source || filter->last_dtmf != dtmf->digit) {
			status = SWITCH_STATUS_SUCCESS;
		}
		filter->last_dtmf = dtmf->digit;
		filter->last_dtmf_source = dtmf->source;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "(%s) %s %s digit %c\n",
		(only_rtp) ? "ALLOW 2833" : "ALLOW ALL",
		(status == SWITCH_STATUS_SUCCESS) ? "Accept" : "Ignore", to_dtmf_source_string(dtmf->source), dtmf->digit);

	return status;
}

SWITCH_STANDARD_APP(deduplicate_dtmf_app_function)
{
	struct deduplicate_dtmf_filter *filter = switch_channel_get_private(switch_core_session_get_channel(session), "deduplicate_dtmf_filter");
	if (!filter) {
		filter = switch_core_session_alloc(session, sizeof(*filter));
		filter->only_rtp = !zstr(data) && !strcmp("only_rtp", data);
		filter->last_dtmf = 0;
		switch_channel_set_private(switch_core_session_get_channel(session), "deduplicate_dtmf_filter", filter);
		switch_core_event_hook_add_recv_dtmf(session, deduplicate_recv_dtmf_hook);
	}
}

#define SPEAK_DESC "Speak text to a channel via the tts interface"
#define DISPLACE_DESC "Displace audio from a file to the channels input"
#define SESS_REC_DESC "Starts a background recording of the entire session"

#define SESS_REC_MASK_DESC "Replace audio in a recording with blank data to mask critical voice sections"
#define SESS_REC_UNMASK_DESC "Resume normal operation after calling mask"

#define STOP_SESS_REC_DESC "Stops a background recording of the entire session"
#define SCHED_TRANSF_DESCR "Schedule a transfer in the future"
#define SCHED_BROADCAST_DESCR "Schedule a broadcast in the future"
#define SCHED_HANGUP_DESCR "Schedule a hangup in the future"
#define UNSET_LONG_DESC "Unset a channel variable for the channel calling the application."
#define SET_LONG_DESC "Set a channel variable for the channel calling the application."
#define SET_GLOBAL_LONG_DESC "Set a global variable."
#define SET_PROFILE_VAR_LONG_DESC "Set a caller profile variable for the channel calling the application."
#define EXPORT_LONG_DESC "Set and export a channel variable for the channel calling the application."
#define LOG_LONG_DESC "Logs a channel variable for the channel calling the application."
#define TRANSFER_LONG_DESC "Immediately transfer the calling channel to a new extension"
#define SLEEP_LONG_DESC "Pause the channel for a given number of milliseconds, consuming the audio for that period of time."

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dptools_shutdown)
{

	switch_event_free_subclass(FILE_STRING_CLOSE);
	switch_event_free_subclass(FILE_STRING_FAIL);
	switch_event_free_subclass(FILE_STRING_OPEN);

	switch_event_unbind_callback(pickup_pres_event_handler);
	switch_mutex_destroy(globals.pickup_mutex);
	switch_core_hash_destroy(&globals.pickup_hash);
	switch_mutex_destroy(globals.mutex_mutex);
	switch_core_hash_destroy(&globals.mutex_hash);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;
	switch_chat_interface_t *chat_interface;
	switch_file_interface_t *file_interface;

	if (switch_event_reserve_subclass(FILE_STRING_CLOSE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", FILE_STRING_CLOSE);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(FILE_STRING_FAIL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", FILE_STRING_FAIL);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(FILE_STRING_OPEN) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", FILE_STRING_OPEN);
		return SWITCH_STATUS_TERM;
	}

	globals.pool = pool;
	switch_core_hash_init(&globals.pickup_hash);
	switch_mutex_init(&globals.pickup_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_core_hash_init(&globals.mutex_hash);
	switch_mutex_init(&globals.mutex_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_event_bind(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pickup_pres_event_handler, NULL);


	file_string_supported_formats[0] = "file_string";

	file_interface = (switch_file_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = file_string_supported_formats;
	file_interface->file_open = file_string_file_open;
	file_interface->file_close = file_string_file_close;
	file_interface->file_read = file_string_file_read;
	file_interface->file_write = file_string_file_write;
	file_interface->file_seek = file_string_file_seek;
	file_interface->file_set_string = file_string_file_set_string;
	file_interface->file_get_string = file_string_file_get_string;

	file_url_supported_formats[0] = "file";

	file_interface = (switch_file_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = file_url_supported_formats;
	file_interface->file_open = file_url_file_open;
	file_interface->file_close = file_url_file_close;
	file_interface->file_read = file_url_file_read;
	file_interface->file_write = file_url_file_write;
	file_interface->file_seek = file_url_file_seek;


	error_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	error_endpoint_interface->interface_name = "error";
	error_endpoint_interface->io_routines = &error_io_routines;

	group_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	group_endpoint_interface->interface_name = "group";
	group_endpoint_interface->io_routines = &group_io_routines;

	user_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	user_endpoint_interface->interface_name = "user";
	user_endpoint_interface->io_routines = &user_io_routines;

	pickup_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	pickup_endpoint_interface->interface_name = "pickup";
	pickup_endpoint_interface->io_routines = &pickup_io_routines;
	pickup_endpoint_interface->state_handler = &pickup_event_handlers;

	SWITCH_ADD_CHAT(chat_interface, "event", event_chat_send);
	SWITCH_ADD_CHAT(chat_interface, "api", api_chat_send);

	SWITCH_ADD_API(api_interface, "strepoch", "Convert a date string into epoch time", strepoch_api_function, "<string>");
	SWITCH_ADD_API(api_interface, "page", "Send a file as a page", page_api_function, "(var1=val1,var2=val2)<var1=val1,var2=val2><chan1>[:_:<chanN>]");
	SWITCH_ADD_API(api_interface, "strmicroepoch", "Convert a date string into micoepoch time", strmicroepoch_api_function, "<string>");
	SWITCH_ADD_API(api_interface, "chat", "chat", chat_api_function, "<proto>|<from>|<to>|<message>|[<content-type>]");
	SWITCH_ADD_API(api_interface, "strftime", "strftime", strftime_api_function, "<format_string>");
	SWITCH_ADD_API(api_interface, "presence", "presence", presence_api_function, PRESENCE_USAGE);

	SWITCH_ADD_APP(app_interface, "blind_transfer_ack", "", "", blind_transfer_ack_function, "[true|false]", SAF_NONE);

	SWITCH_ADD_APP(app_interface, "bind_digit_action", "bind a key sequence or regex to an action", 
				   "bind a key sequence or regex to an action", bind_digit_action_function, BIND_DIGIT_ACTION_USAGE, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "capture", "capture data into a var", "capture data into a var", 
				   capture_function, "<varname>|<data>|<regex>", SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "clear_digit_action", "clear all digit bindings", "", 
				   clear_digit_action_function, CLEAR_DIGIT_ACTION_USAGE, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "digit_action_set_realm", "change binding realm", "", 
				   digit_action_set_realm_function, DIGIT_ACTION_SET_REALM_USAGE, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "privacy", "Set privacy on calls", "Set caller privacy on calls.", privacy_function, "off|on|name|full|number",
				   SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "set_audio_level", "set volume", "set volume", set_audio_level_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "set_mute", "set mute", "set mute", set_mute_function, "", SAF_NONE);

	SWITCH_ADD_APP(app_interface, "flush_dtmf", "flush any queued dtmf", "flush any queued dtmf", flush_dtmf_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hold", "Send a hold message", "Send a hold message", hold_function, HOLD_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unhold", "Send a un-hold message", "Send a un-hold message", unhold_function, UNHOLD_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "mutex", "block on a call flow only allowing one at a time", "", mutex_function, MUTEX_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "page", "", "", page_function, PAGE_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "transfer", "Transfer a channel", TRANSFER_LONG_DESC, transfer_function, "<exten> [<dialplan> <context>]",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "check_acl", "Check an ip against an ACL list", "Check an ip against an ACL list", check_acl_function,
				   "<ip> <acl | cidr> [<hangup_cause>]", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "verbose_events", "Make ALL Events verbose.", "Make ALL Events verbose.", verbose_events_function, "",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "novideo", "Refuse Inbound Video", "Refuse Inbound Video", novideo_function, "",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "cng_plc", "Do PLC on CNG frames", "", cng_plc_function, "",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "early_hangup", "Enable early hangup", "", early_hangup_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "sleep", "Pause a channel", SLEEP_LONG_DESC, sleep_function, "<pausemilliseconds>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "delay_echo", "echo audio at a specified delay", "Delay n ms", delay_function, "<delay ms>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "strftime", "strftime", "strftime", strftime_function, "[<epoch>|]<format string>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "phrase", "Say a Phrase", "Say a Phrase", phrase_function, "<macro_name>,<data>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eval", "Do Nothing", "Do Nothing", eval_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "set_media_stats", "Set Media Stats", "Set Media Stats", set_media_stats_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "stop", "Do Nothing", "Do Nothing", eval_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "set_zombie_exec", "Enable Zombie Execution", "Enable Zombie Execution", 
				   zombie_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "pre_answer", "Pre-Answer the call", "Pre-Answer the call for a channel.", pre_answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "answer", "Answer the call", "Answer the call for a channel.", answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "wait_for_answer", "Wait for call to be answered", "Wait for call to be answered.", wait_for_answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hangup", "Hangup the call", "Hangup the call for a channel.", hangup_function, "[<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_name", "Name the channel", "Name the channel", set_name_function, "<name>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "presence", "Send Presence", "Send Presence.", presence_function, "<rpid> <status> [<id>]",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "log", "Logs to the logger", LOG_LONG_DESC, log_function, "<log_level> <log_string>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "info", "Display Call Info", "Display Call Info", info_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "event", "Fire an event", "Fire an event", event_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "sound_test", "Analyze Audio", "Analyze Audio", sound_test_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "export", "Export a channel variable across a bridge", EXPORT_LONG_DESC, export_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "bridge_export", "Export a channel variable across a bridge", EXPORT_LONG_DESC, bridge_export_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "set", "Set a channel variable", SET_LONG_DESC, set_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

	SWITCH_ADD_APP(app_interface, "multiset", "Set many channel variables", SET_LONG_DESC, multiset_function, "[^^<delim>]<varname>=<value> <var2>=<val2>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

	SWITCH_ADD_APP(app_interface, "push", "Set a channel variable", SET_LONG_DESC, push_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

	SWITCH_ADD_APP(app_interface, "unshift", "Set a channel variable", SET_LONG_DESC, unshift_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

	SWITCH_ADD_APP(app_interface, "set_global", "Set a global variable", SET_GLOBAL_LONG_DESC, set_global_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "set_profile_var", "Set a caller profile variable", SET_PROFILE_VAR_LONG_DESC, set_profile_var_function,
				   "<varname>=<value>", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "unset", "Unset a channel variable", UNSET_LONG_DESC, unset_function, "<varname>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "multiunset", "Unset many channel variables", SET_LONG_DESC, multiunset_function, "[^^<delim>]<varname> <var2> <var3>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

	SWITCH_ADD_APP(app_interface, "ring_ready", "Indicate Ring_Ready", "Indicate Ring_Ready on a channel.", ring_ready_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "remove_bugs", "Remove media bugs", "Remove all media bugs from a channel.", remove_bugs_function, "[<function>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "break", "Break", "Set the break flag.", break_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "detect_speech", "Detect speech", "Detect speech on a channel.", detect_speech_function, DETECT_SPEECH_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "play_and_detect_speech", "Play and do speech recognition", "Play and do speech recognition", play_and_detect_speech_function, PLAY_AND_DETECT_SPEECH_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "ivr", "Run an ivr menu", "Run an ivr menu.", ivr_application_function, "<menu_name>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "redirect", "Send session redirect", "Send a redirect message to a session.", redirect_function, "<redirect_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "video_refresh", "Send video refresh.", "Send video refresh.", video_refresh_function, "",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "video_decode", "Set video decode.", "Set video decode.", video_set_decode_function, "[[on|wait]|off]",
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "send_info", "Send info", "Send info", send_info_function, "<info>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "jitterbuffer", "Send session jitterbuffer", "Send a jitterbuffer message to a session.", 
				   jitterbuffer_function, "<jitterbuffer_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "send_display", "Send session a new display", "Send session a new display.", display_function, "<text>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "respond", "Send session respond", "Send a respond message to a session.", respond_function, "<respond_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "deflect", "Send call deflect", "Send a call deflect.", deflect_function, "<deflect_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "recovery_refresh", "Send call recovery_refresh", "Send a call recovery_refresh.", recovery_refresh_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "queue_dtmf", "Queue dtmf to be sent", "Queue dtmf to be sent from a session", queue_dtmf_function, "<dtmf_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "send_dtmf", "Send dtmf to be sent", "Send dtmf to be sent from a session", send_dtmf_function, "<dtmf_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_cancel", "cancel scheduled tasks", "cancel scheduled tasks", sched_cancel_function, "[group]",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_hangup", SCHED_HANGUP_DESCR, SCHED_HANGUP_DESCR, sched_hangup_function, "[+]<time> [<cause>]",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_broadcast", SCHED_BROADCAST_DESCR, SCHED_BROADCAST_DESCR, sched_broadcast_function,
				   "[+]<time> <path> [aleg|bleg|both]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_transfer", SCHED_TRANSF_DESCR, SCHED_TRANSF_DESCR, sched_transfer_function,
				   "[+]<time> <extension> <dialplan> <context>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "execute_extension", "Execute an extension", "Execute an extension", exe_function, EXE_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_heartbeat", "Enable Scheduled Heartbeat", "Enable Scheduled Heartbeat",
				   sched_heartbeat_function, SCHED_HEARTBEAT_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "enable_heartbeat", "Enable Media Heartbeat", "Enable Media Heartbeat",
				   heartbeat_function, HEARTBEAT_SYNTAX, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "enable_keepalive", "Enable Keepalive", "Enable Keepalive",
				   keepalive_function, KEEPALIVE_SYNTAX, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "media_reset", "Reset all bypass/proxy media flags", "Reset all bypass/proxy media flags", media_reset_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "mkdir", "Create a directory", "Create a directory", mkdir_function, MKDIR_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "rename", "Rename file", "Rename file", rename_function, RENAME_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "transfer_vars", "Transfer variables", "Transfer variables", transfer_vars_function, TRANSFER_VARS_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "soft_hold", "Put a bridged channel on hold", "Put a bridged channel on hold", soft_hold_function, SOFT_HOLD_SYNTAX,
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "bind_meta_app", "Bind a key to an application", "Bind a key to an application", dtmf_bind_function, BIND_SYNTAX,
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unbind_meta_app", "Unbind a key from an application", "Unbind a key from an application", dtmf_unbind_function,
				   UNBIND_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "block_dtmf", "Block DTMF", "Block DTMF", dtmf_block_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unblock_dtmf", "Stop blocking DTMF", "Stop blocking DTMF", dtmf_unblock_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "intercept", "intercept", "intercept", intercept_function, INTERCEPT_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eavesdrop", "eavesdrop on a uuid", "eavesdrop on a uuid", eavesdrop_function, eavesdrop_SYNTAX, SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "three_way", "three way call with a uuid", "three way call with a uuid", three_way_function, threeway_SYNTAX,
				   SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "set_user", "Set a User", "Set a User", set_user_function, SET_USER_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "stop_dtmf", "stop inband dtmf", "Stop detecting inband dtmf.", stop_dtmf_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "start_dtmf", "Detect dtmf", "Detect inband dtmf on the session", dtmf_session_function, "", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "stop_dtmf_generate", "stop inband dtmf generation", "Stop generating inband dtmf.",
				   stop_dtmf_session_generate_function, "[write]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "start_dtmf_generate", "Generate dtmf", "Generate inband dtmf on the session", dtmf_session_generate_function, "",
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_tone_detect", "stop detecting tones", "Stop detecting tones", stop_fax_detect_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fax_detect", "Detect faxes", "Detect fax send tone", fax_detect_session_function, "", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "tone_detect", "Detect tones", "Detect tones", tone_detect_session_function, "", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "echo", "Echo", "Perform an echo test against the calling channel", echo_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "park", "Park", "Park", park_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "park_state", "Park State", "Park State", park_state_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "gentones", "Generate Tones", "Generate tones to the channel", gentones_function, "<tgml_script>[|<loops>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "playback", "Playback File", "Playback a file to the channel", playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "endless_playback", "Playback File Endlessly", "Endlessly Playback a file to the channel",
				   endless_playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "loop_playback", "Playback File looply", "Playback a file to the channel looply for limted times",
				   loop_playback_function, "[+loops] <path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "att_xfer", "Attended Transfer", "Attended Transfer", att_xfer_function, "<channel_url>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "read", "Read Digits", "Read Digits", read_function, 
				   "<min> <max> <file> <var_name> <timeout> <terminators> <digit_timeout>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "play_and_get_digits", "Play and get Digits", "Play and get Digits",
				   play_and_get_digits_function, 
				   "\n\t<min> <max> <tries> <timeout> <terminators> <file> <invalid_file> <var_name> <regexp> [<digit_timeout>] ['<failure_ext> [failure_dp [failure_context]]']", SAF_NONE);

	SWITCH_ADD_APP(app_interface, "stop_video_write_overlay", "Stop video write overlay", "Stop video write overlay", stop_video_write_overlay_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "video_write_overlay", "Video write overlay", "Video write overlay", video_write_overlay_session_function, "<path> [<pos>] [<alpha>]", SAF_MEDIA_TAP);

	SWITCH_ADD_APP(app_interface, "stop_record_session", "Stop Record Session", STOP_SESS_REC_DESC, stop_record_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_session", "Record Session", SESS_REC_DESC, record_session_function, "<path> [+<timeout>]", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "record_session_mask", "Mask audio in recording", SESS_REC_MASK_DESC, record_session_mask_function, "<path>", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "record_session_unmask", "Resume recording", SESS_REC_UNMASK_DESC, record_session_unmask_function, "<path>", SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "record", "Record File", "Record a file from the channels input", record_function,
				   "<path> [<time_limit_secs>] [<silence_thresh>] [<silence_hits>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "preprocess", "pre-process", "pre-process", preprocess_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_displace_session", "Stop Displace File", "Stop Displacing to a file", stop_displace_session_function, "<path>",
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "displace_session", "Displace File", DISPLACE_DESC, displace_session_function, "<path> [<flags>] [+time_limit_ms]",
				   SAF_MEDIA_TAP);
	SWITCH_ADD_APP(app_interface, "speak", "Speak text", SPEAK_DESC, speak_function, "<engine>|<voice>|<text>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "clear_speech_cache", "Clear Speech Handle Cache", "Clear Speech Handle Cache", clear_speech_cache_function, "",
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "bridge", "Bridge Audio", "Bridge the audio between two sessions", audio_bridge_function, "<channel_url>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "system", "Execute a system command", "Execute a system command", system_session_function, "<command>",
				   SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "bgsystem", "Execute a system command in the background", "Execute a background system command", bgsystem_session_function, "<command>",
				   SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "say", "say", "say", say_function, SAY_SYNTAX, SAF_NONE);

	SWITCH_ADD_APP(app_interface, "wait_for_silence", "wait_for_silence", "wait_for_silence", wait_for_silence_function, WAIT_FOR_SILENCE_SYNTAX,
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "session_loglevel", "session_loglevel", "session_loglevel", session_loglevel_function, SESSION_LOGLEVEL_SYNTAX,
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "limit", "Limit", LIMIT_DESC, limit_function, LIMIT_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "limit_hash", "Limit", LIMIT_HASH_DESC, limit_hash_function, LIMIT_HASH_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "limit_execute", "Limit", LIMITEXECUTE_DESC, limit_execute_function, LIMITEXECUTE_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "limit_hash_execute", "Limit", LIMITHASHEXECUTE_DESC, limit_hash_execute_function, LIMITHASHEXECUTE_USAGE, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "pickup", "Pickup", "Pickup a call", pickup_function, PICKUP_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "deduplicate_dtmf", "Prevent duplicate inband + 2833 dtmf", "", deduplicate_dtmf_app_function, "[only_rtp]", SAF_SUPPORT_NOMEDIA);


	SWITCH_ADD_DIALPLAN(dp_interface, "inline", inline_dialplan_hunt);

	/* indicate that the module should continue to be loaded */
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
