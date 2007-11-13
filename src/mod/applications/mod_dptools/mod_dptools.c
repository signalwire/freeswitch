/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Ken Rice, Asteria Solutions Group, Inc <ken@asteriasgi.com>
 * Michael Murdock <mike at mmurdock dot org>
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter AT 0xdecafbad dot com>
 *
 * mod_dptools.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load);
SWITCH_MODULE_DEFINITION(mod_dptools, mod_dptools_load, NULL, NULL);

#define DETECT_SPEECH_SYNTAX "<mod_name> <gram_name> <gram_path> [<addr>] OR grammar <gram_name> [<path>] OR pause OR resume"
SWITCH_STANDARD_APP(detect_speech_function)
{
	char *argv[4];
	int argc;
	char *lbuf = NULL;

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (!strcasecmp(argv[0], "grammar") && argc >= 1) {
			switch_ivr_detect_speech_load_grammar(session, argv[1], argv[2]);
		} else if (!strcasecmp(argv[0], "nogrammar")) {
			switch_ivr_detect_speech_unload_grammar(session, argv[1]);
		} else if (!strcasecmp(argv[0], "pause")) {
			switch_ivr_pause_detect_speech(session);
		} else if (!strcasecmp(argv[0], "resume")) {
			switch_ivr_resume_detect_speech(session);
		} else if (!strcasecmp(argv[0], "stop")) {
			switch_ivr_stop_detect_speech(session);
		} else if (argc >= 3) {
			switch_ivr_detect_speech(session, argv[0], argv[1], argv[2], argv[3], NULL);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", DETECT_SPEECH_SYNTAX);
	}

}

#define EXE_SYNTAX "<extension> <dialplan> <context>"
SWITCH_STANDARD_APP(exe_function)
{
	char *argv[4];
	int argc;
	char *lbuf = NULL;
	char *extension;
	char *context;
	char *dialplan;
	
	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		extension = argv[0];
		dialplan = argv[1];
		context = argv[2];
		switch_core_session_execute_exten(session, extension, dialplan, context);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", EXE_SYNTAX);
	}
}


#define SET_USER_SYNTAX "<user>@<domain>"
SWITCH_STANDARD_APP(set_user_function)
{
	switch_xml_t x_domain, xml = NULL, x_user, x_param, x_params;	
	char *user, *mailbox, *domain;
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_strlen_zero(data)) {
		goto error;
	}

	user = switch_core_session_strdup(session, data);

	if (!(domain = strchr(user, '@'))) {
		goto error;
	}

	*domain++ = '\0';
	
	if (switch_xml_locate_user(user, domain, NULL, &xml, &x_domain, &x_user, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", user, domain);
		goto done;
	}

	if ((mailbox = (char *)switch_xml_attr(x_user, "mailbox"))) {
		switch_channel_set_variable(channel, "mailbox", mailbox);
	}
	
	if ((x_params = switch_xml_child(x_user, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			
			if (var && val) {
				switch_channel_set_variable(channel, var, val);
			}
		}
	}

	switch_channel_set_variable(channel, "user_name", user);
	switch_channel_set_variable(channel, "domain_name", domain);

	goto done;

 error:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No user@domain specified.\n");

 done:
	
	if (xml) {
		switch_xml_free(xml);
	}


}


SWITCH_STANDARD_APP(ring_ready_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_channel_ring_ready(channel);
}

SWITCH_STANDARD_APP(break_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_channel_set_flag(channel, CF_BREAK);
}

SWITCH_STANDARD_APP(queue_dtmf_function)
{
	switch_channel_t *channel;
	if (!switch_strlen_zero(data)) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		switch_channel_queue_dtmf(channel, data);
	}
}

SWITCH_STANDARD_APP(transfer_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No extension specified.\n");
		}
	}
}

SWITCH_STANDARD_APP(sched_transfer_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;

			if (*argv[0] == '+') {
				when = time(NULL) + atol(argv[0] + 1);
			} else {
				when = atol(argv[0]);
			}

			switch_ivr_schedule_transfer(when, switch_core_session_get_uuid(session), argv[1], argv[2], argv[3]);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Args\n");
		}
	}
}

SWITCH_STANDARD_APP(sched_hangup_function)
{
	int argc;
	char *argv[5] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			time_t when;
			switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;
			switch_bool_t bleg = SWITCH_FALSE;

			if (*argv[0] == '+') {
				when = time(NULL) + atol(argv[0] + 1);
			} else {
				when = atol(argv[0]);
			}

			if (argv[1]) {
				cause = switch_channel_str2cause(argv[1]);
			}

			if (argv[2] && !strcasecmp(argv[2], "bleg")) {
				bleg = SWITCH_TRUE;
			}

			switch_ivr_schedule_hangup(when, switch_core_session_get_uuid(session), cause, bleg);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No time specified.\n");
		}
	}
}


SWITCH_STANDARD_APP(sched_broadcast_function)
{
	int argc;
	char *argv[6] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;
			switch_media_flag_t flags = SMF_NONE;

			if (*argv[0] == '+') {
				when = time(NULL) + atol(argv[0] + 1);
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

			switch_ivr_schedule_broadcast(when, switch_core_session_get_uuid(session), argv[1], flags);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Args\n");
		}
	}
}

SWITCH_STANDARD_APP(sleep_function)
{

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No timeout specified.\n");
	} else {
		uint32_t ms = atoi(data);
		switch_ivr_sleep(session, ms);
	}
}

SWITCH_STANDARD_APP(delay_function)
{
	uint32_t len = 0;

	if (switch_strlen_zero(data)) {
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

SWITCH_STANDARD_APP(phrase_function)
{
	switch_channel_t *channel;
	char *mydata = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if ((mydata = switch_core_session_strdup(session, data))) {
		const char *lang;
		char *macro = mydata;
		char *mdata = NULL;

		if ((mdata = strchr(macro, ','))) {
			*mdata++ = '\0';
		}

		lang = switch_channel_get_variable(channel, "language");

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Execute %s(%s) lang %s\n", macro, mdata, lang);
		switch_ivr_phrase_macro(session, macro, mdata, lang, NULL);
	}

}


SWITCH_STANDARD_APP(hangup_function)
{
	switch_channel_t *channel;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!switch_strlen_zero((char *) data)) {
		cause = switch_channel_str2cause((char *) data);
	}

	switch_channel_hangup(channel, cause);
}

SWITCH_STANDARD_APP(answer_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);

	assert(channel != NULL);
	switch_channel_answer(channel);
}

SWITCH_STANDARD_APP(pre_answer_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);

	assert(channel != NULL);
	switch_channel_pre_answer(channel);
}

SWITCH_STANDARD_APP(redirect_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to redirect */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
	switch_core_session_receive_message(session, &msg);

}

SWITCH_STANDARD_APP(reject_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to reject the call */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_REJECT;
	switch_core_session_receive_message(session, &msg);

}


SWITCH_STANDARD_APP(set_function)
{
	switch_channel_t *channel;
	char *var, *val = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = switch_core_session_strdup(session, data);
		val = strchr(var, '=');

		if (val) {
			*val++ = '\0';
			if (switch_strlen_zero(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", var, val ? val : "UNDEF");
		switch_channel_set_variable(channel, var, val);
	}
}

SWITCH_STANDARD_APP(set_global_function)
{
	char *var, *val = NULL;

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = strdup(data);
		assert(var);
		val = strchr(var, '=');

		if (val) {
			*val++ = '\0';
			if (switch_strlen_zero(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET GLOBAL [%s]=[%s]\n", var, val ? val : "UNDEF");
		switch_core_set_variable(var, val);
		free(var);
	}
}


SWITCH_STANDARD_APP(set_profile_var_function)
{
	switch_channel_t *channel;
	switch_caller_profile_t *caller_profile;
	char *name, *val = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	caller_profile = switch_channel_get_caller_profile(channel);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		name = switch_core_session_strdup(session, data);
		val = strchr(name, '=');

		if (val) {
			*val++ = '\0';
			if (switch_strlen_zero(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET_PROFILE_VAR [%s]=[%s]\n", name, val ? val : "UNDEF");

		if (!strcasecmp(name, "dialplan")) {
			caller_profile->dialplan = val;
		}
		if (!strcasecmp(name, "username")) {
			caller_profile->username = val;
		}
		if (!strcasecmp(name, "caller_id_name")) {
			caller_profile->caller_id_name = val;
		}
		if (!strcasecmp(name, "caller_id_number")) {
			caller_profile->caller_id_number = val;
		}
		if (!strcasecmp(name, "caller_ton")) {
			caller_profile->caller_ton = (uint8_t)atoi(val);
		}
		if (!strcasecmp(name, "caller_numplan")) {
			caller_profile->caller_numplan = (uint8_t)atoi(val);
		}
		if (!strcasecmp(name, "destination_number_ton")) {
			caller_profile->destination_number_ton = (uint8_t)atoi(val);
		}
		if (!strcasecmp(name, "destination_number_numplan")) {
			caller_profile->destination_number_numplan = (uint8_t)atoi(val);
		}
		if (!strcasecmp(name, "ani")) {
			caller_profile->ani = val;
		}
		if (!strcasecmp(name, "aniii")) {
			caller_profile->aniii = val;
		}
		if (!strcasecmp(name, "network_addr")) {
			caller_profile->network_addr = val;
		}
		if (!strcasecmp(name, "rdnis")) {
			caller_profile->rdnis = val;
		}
		if (!strcasecmp(name, "destination_number")) {
			caller_profile->destination_number = val;
		}
		if (!strcasecmp(name, "uuid")) {
			caller_profile->uuid = val;
		}
		if (!strcasecmp(name, "source")) {
			caller_profile->source = val;
		}
		if (!strcasecmp(name, "context")) {
			caller_profile->context = val;
		}
		if (!strcasecmp(name, "chan_name")) {
			caller_profile->chan_name = val;
		}
	}
}


SWITCH_STANDARD_APP(export_function)
{
	switch_channel_t *channel;
	const char *exports;
	char *new_exports = NULL, *new_exports_d = NULL, *var, *val = NULL, *var_name = NULL;
	int local = 1;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		exports = switch_channel_get_variable(channel, SWITCH_EXPORT_VARS_VARIABLE);
		var = switch_core_session_strdup(session, data);
		val = strchr(var, '=');

		if (val) {
			*val++ = '\0';
			if (switch_strlen_zero(val)) {
				val = NULL;
			}
		}

		if (!strncasecmp(var, "nolocal:", 8)) {
			var_name = var + 8;
			local = 0;
		} else {
			var_name = var;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXPORT %s[%s]=[%s]\n", local ? "" : "(REMOTE ONLY) ", var_name, val ? val : "UNDEF");
		switch_channel_set_variable(channel, var, val);

		if (var && val) {
			if (exports) {
				new_exports_d = switch_mprintf("%s,%s", exports, var);
				new_exports = new_exports_d;
			} else {
				new_exports = var;
			}

			switch_channel_set_variable(channel, SWITCH_EXPORT_VARS_VARIABLE, new_exports);

			switch_safe_free(new_exports_d);
		}
	}
}

SWITCH_STANDARD_APP(unset_function)
{
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
		switch_channel_set_variable(channel, data, NULL);
	}
}

SWITCH_STANDARD_APP(log_function)
{
	switch_channel_t *channel;
	char *level, *log_str;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (data && (level = strdup(data))) {
		switch_log_level_t ltype = SWITCH_LOG_DEBUG;

		if ((log_str = strchr(level, ' '))) {
			*log_str++ = '\0';
			ltype = switch_log_str2level(level);
		} else {
			log_str = level;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, ltype, "%s\n", log_str);
		switch_safe_free(level);
	}
}


SWITCH_STANDARD_APP(info_function)
{
	switch_channel_t *channel;
	switch_event_t *event;
	char *buf;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_serialize(event, &buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "CHANNEL_DATA:\n%s\n", buf);
		switch_event_destroy(&event);
	}

}



SWITCH_STANDARD_APP(event_function)
{
	switch_channel_t *channel;
	switch_event_t *event;
	char *argv[25];
	int argc = 0;
	char *lbuf;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_APPLICATION) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		if (data && (lbuf = switch_core_session_strdup(session, data))
			&& (argc = switch_separate_string(lbuf, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			int x = 0;

			for (x = 0; x < argc; x++) {
				char *p, *this = argv[x];
				p = this;
				while(*p == ' ') *p++ = '\0';
				this = p;
				
				if (this) {
					char *var = this, *val = NULL;
					if ((val = strchr(var, '='))) {
						p = val - 1;
						*val++ = '\0';
						while(*p == ' ') *p-- = '\0';
						p = val;
						while(*p == ' ') *p++ = '\0';
						val = p;
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, var, "%s", val);
					}
				}

			}
		}
		
		switch_event_fire(&event);
	}

}


SWITCH_STANDARD_APP(privacy_function)
{
	switch_channel_t *channel;
	switch_caller_profile_t *caller_profile;
	char *arg;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	caller_profile = switch_channel_get_caller_profile(channel);

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No privacy mode specified.\n");
	} else {
		arg = switch_core_session_strdup(session, data);

		switch_set_flag(caller_profile, SWITCH_CPF_SCREEN);

		if (!strcasecmp(arg, "no")) {
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(arg, "yes")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(arg, "full")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(arg, "name")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
		} else if (!strcasecmp(arg, "number")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID privacy mode specified. Use a valid mode [no|yes|name|full|number].\n");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Privacy to %s [%d]\n", arg, caller_profile->flags);
	}
}

SWITCH_STANDARD_APP(strftime_function)
{
	char *argv[2];
	int argc;
	char *lbuf;

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, '=', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
		switch_size_t retsize;
		switch_time_exp_t tm;
		char date[80] = "";
		switch_channel_t *channel;

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		switch_time_exp_lt(&tm, switch_time_now());
		switch_strftime(date, &retsize, sizeof(date), argv[1], &tm);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", argv[0], date);
		switch_channel_set_variable(channel, argv[0], date);
	}
}


SWITCH_STANDARD_API(strepoch_api_function)
{
	switch_time_t out;

	if (switch_strlen_zero(cmd)) {
		out = switch_time_now();
	} else {
		out = switch_str_time(cmd);
	}

	stream->write_function(stream, "%d", (uint32_t) ((out) / (int64_t) (1000000)));

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(strftime_api_function)
{

	switch_size_t retsize;
	switch_time_exp_t tm;
	char date[80] = "";
	switch_time_t thetime;
	char *p;
	if (!switch_strlen_zero(cmd) && (p = strchr(cmd, '|'))) {
		thetime = switch_time_make(atoi(cmd),0);
		cmd = p+1;
	} else {
		thetime = switch_time_now();
	}
	switch_time_exp_lt(&tm, thetime);
	switch_strftime(date, &retsize, sizeof(date), switch_strlen_zero(cmd) ? "%Y-%m-%d %T" : cmd, &tm);
	stream->write_function(stream, "%s", date);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(presence_api_function)
{
	switch_event_t *event;
	char *lbuf, *argv[4];
	int argc = 0;
	switch_event_types_t type = SWITCH_EVENT_PRESENCE_IN;

	if (cmd && (lbuf = strdup(cmd))
		&& (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) > 0) {
		if (!strcasecmp(argv[0], "out")) {
			type = SWITCH_EVENT_PRESENCE_OUT;
		} else if (argc != 4) {
			stream->write_function(stream, "Invalid");
			return SWITCH_STATUS_SUCCESS;
		}

		if (switch_event_create(&event, type) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "dp");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", __FILE__);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s", argv[1]);
			if (type == SWITCH_EVENT_PRESENCE_IN) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", argv[2]);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", argv[3]);
			}
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
		stream->write_function(stream, "Event Sent");
		switch_safe_free(lbuf);
	} else {
		stream->write_function(stream, "Invalid");
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(chat_api_function)
{
	char *lbuf, *argv[4];
	int argc = 0;

	if (cmd && (lbuf = strdup(cmd))
		&& (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) == 4) {
		switch_chat_interface_t *ci;

		if ((ci = switch_loadable_module_get_chat_interface(argv[0]))) {
			ci->chat_send("dp", argv[1], argv[2], "", argv[3], "");
			stream->write_function(stream, "Sent");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Chat Interface [%s]!\n", argv[0]);
		}
	} else {
		stream->write_function(stream, "Invalid");
	}

	return SWITCH_STATUS_SUCCESS;
}

static char *ivr_cf_name = "ivr.conf";

#ifdef _TEST_CALLBACK_
static switch_ivr_action_t menu_handler(switch_ivr_menu_t * menu, char *param, char *buf, size_t buflen, void *obj)
{
	switch_ivr_action_t action = SWITCH_IVR_ACTION_NOOP;

	if (param != NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "menu_handler '%s'\n", param);
	}

	return action;
}
#endif

SWITCH_STANDARD_APP(ivr_application_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *params;

	if (channel && data && (params = switch_core_session_strdup(session, data))) {
		switch_xml_t cxml = NULL, cfg = NULL, xml_menus = NULL, xml_menu = NULL;

		// Open the config from the xml registry
		if ((cxml = switch_xml_open_cfg(ivr_cf_name, &cfg, NULL)) != NULL) {
			if ((xml_menus = switch_xml_child(cfg, "menus"))) {
				xml_menu = switch_xml_find_child(xml_menus, "menu", "name", params);

				// if the menu was found
				if (xml_menu != NULL) {
					switch_ivr_menu_xml_ctx_t *xml_ctx = NULL;
					switch_ivr_menu_t *menu_stack = NULL;

					// build a menu tree and execute it
					if (switch_ivr_menu_stack_xml_init(&xml_ctx, NULL) == SWITCH_STATUS_SUCCESS
#ifdef _TEST_CALLBACK_
						&& switch_ivr_menu_stack_xml_add_custom(xml_ctx, "custom", &menu_handler) == SWITCH_STATUS_SUCCESS
#endif
						&& switch_ivr_menu_stack_xml_build(xml_ctx, &menu_stack, xml_menus, xml_menu) == SWITCH_STATUS_SUCCESS) {
						switch_xml_free(cxml);
						cxml = NULL;
						switch_channel_pre_answer(channel);
						switch_ivr_menu_execute(session, menu_stack, params, NULL);
						switch_ivr_menu_stack_free(menu_stack);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create menu '%s'\n", params);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to find menu '%s'\n", params);
				}
			}
			switch_xml_free(cxml);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", ivr_cf_name);
		}
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
	
	if (!switch_strlen_zero(data)) {
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
	switch_ivr_tone_detect_session(session, "fax", "1100.0", "r", 0, NULL, NULL);
}

SWITCH_STANDARD_APP(system_session_function)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Executing command: %s\n",data);
    if(!system(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Failed to execute command: %s\n",data);
	}
}

SWITCH_STANDARD_APP(tone_detect_session_function)
{
	char *argv[6] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;

	mydata = switch_core_session_strdup(session, data);
	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID ARGS!\n");
	}
	if (argv[3]) {
		uint32_t mto;
		if (*argv[3] == '+') {
			if ((mto = atol(argv[3]+1)) > 0) {
				to = time(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
			}
		} else {
			if ((to = atol(argv[3])) < time(NULL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
				to = 0;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Enabling tone detection '%s' '%s'\n", argv[0], argv[1]);
	
	switch_ivr_tone_detect_session(session, argv[0], argv[1], argv[2], to, argv[4], argv[5]);
}

SWITCH_STANDARD_APP(stop_fax_detect_session_function)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Disabling tone detection\n");
	switch_ivr_stop_tone_detect_session(session);
}

SWITCH_STANDARD_APP(echo_function)
{
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_answer(channel);

	switch_channel_set_state(channel, CS_LOOPBACK);
}

SWITCH_STANDARD_APP(park_function)
{
	switch_ivr_park(session, NULL);

}

/********************************************************************************/
/*						Playback/Record Functions								*/
/********************************************************************************/

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			char *dtmf = (char *) input;
			const char *terminators;
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *p;
			
			assert(channel);

			if (!(terminators = switch_channel_get_variable(channel, SWITCH_PLAYBACK_TERMINATORS_VARIABLE))) {
				terminators = "*";
			}
			if (!strcasecmp(terminators, "none")) {
				terminators = NULL;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);
			
			for (p = terminators; p && *p; p++) {
				char *d;
				for (d = dtmf; d && *d; d++) {
					if (*p == *d) {
						return SWITCH_STATUS_BREAK;
					}
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(clear_speech_cache_function)
{
	switch_ivr_clear_speech_cache(session);
}

SWITCH_STANDARD_APP(speak_function)
{
	switch_channel_t *channel;
	char buf[10];
	char *argv[4] = { 0 };
	int argc;
	const char *engine = NULL;
	const char *voice = NULL;
	char *text = NULL;
	char *mydata = NULL;
	switch_codec_t *codec;
	switch_input_args_t args = { 0 };

	codec = switch_core_session_get_read_codec(session);
	assert(codec != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	mydata = switch_core_session_strdup(session, data);
	argc = switch_separate_string(mydata, '|', argv, sizeof(argv) / sizeof(argv[0]));
	
	if (argc == 1) {
		text = argv[0];
	} else if (argc == 2) {
		voice = argv[0];
		text = argv[1];
	} else {
		engine = argv[0];
		voice = argv[1];
		text = argv[2];
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Params! [%s][%s][%s]\n", engine, voice, text);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

	switch_channel_pre_answer(channel);

	args.input_callback = on_dtmf;
	args.buf = buf;
	args.buflen = sizeof(buf);
	switch_ivr_speak_text(session, engine, voice, text, &args);

}

SWITCH_STANDARD_APP(playback_function)
{
	switch_channel_t *channel;
	char *file_name = NULL;
	switch_input_args_t args = { 0 };

	file_name = switch_core_session_strdup(session, data);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_pre_answer(channel);

	args.input_callback = on_dtmf;
	switch_ivr_play_file(session, NULL, file_name, &args);

}

SWITCH_STANDARD_APP(gentones_function)
{
	switch_channel_t *channel;
	char *tone_script = NULL;
	switch_input_args_t args = { 0 };
	char *l;
	int32_t loops = 0;

	tone_script = switch_core_session_strdup(session, data);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	switch_channel_pre_answer(channel);

	if ((l = strchr(tone_script, '|'))) {
		*l++ = '\0';
		loops = atoi(l);

		if (loops < 0) {
			loops = -1;
		}
	}

	args.input_callback = on_dtmf;
	switch_ivr_gentones(session, tone_script, loops, &args);

}

SWITCH_STANDARD_APP(displace_session_function)
{
	switch_channel_t *channel;
	char *path = NULL;
	uint32_t limit = 0;
    char *argv[6];
	int x, argc;
	char *lbuf = NULL;
	char *flags = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		path = argv[0];
		for(x = 0; x < argc; x++) {
			if (strchr(argv[x], '+')) {
				limit = atoi(argv[x]);
			} else if (!switch_strlen_zero(argv[x])) {
				flags = argv[x];
			}
		}
		switch_ivr_displace_session(session, path, limit, flags);
	}
}


SWITCH_STANDARD_APP(stop_displace_session_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_ivr_stop_displace_session(session, data);
}


SWITCH_STANDARD_APP(record_function)
{
	switch_channel_t *channel;
	switch_status_t status;
	uint32_t limit = 0;
	char *path;
	switch_input_args_t args = { 0 };
	switch_file_handle_t fh = { 0 };
	int argc;
    char *mydata, *argv[4] = { 0 };
	char *l = NULL;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No file specified.\n");		
		return;
	}
	
	path = argv[0];
	l = argv[1];

	if (l) {
		if (*l == '+') {
			l++;
			if (l) {
				limit = atoi(l);
				if (limit < 0) {
					limit = 0;
				}
			}
		}
	}
	

	if (argv[2]) {
		fh.thresh = atoi(argv[2]);
		if (fh.thresh < 0) {
			fh.thresh = 0;
		}
	}

	if (argv[3]) {
		fh.silence_hits = atoi(argv[3]);
		if (fh.silence_hits < 0) {
			fh.silence_hits = 0;
		}
	}

	args.input_callback = on_dtmf;
	status = switch_ivr_record_file(session, &fh, path, &args, limit);

	if (!switch_channel_ready(channel) || (status != SWITCH_STATUS_SUCCESS && !SWITCH_STATUS_IS_BREAK(status))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
}


SWITCH_STANDARD_APP(record_session_function)
{
	switch_channel_t *channel;
	char *p, *path = NULL;
	uint32_t limit = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	path = switch_core_session_strdup(session, data);
	if ((p = strchr(path, '+'))) {
		char *q = p - 1;
		while(q && *q == ' ') {
			*q = '\0';
			q--;
		}
		*p++ = '\0';
		limit = atoi(p);
	}
	
	switch_ivr_record_session(session, path, limit, NULL);
}


SWITCH_STANDARD_APP(stop_record_session_function)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_ivr_stop_record_session(session, data);
}

/********************************************************************************/
/*								Bridge Functions								*/
/********************************************************************************/

SWITCH_STANDARD_APP(audio_bridge_function)
{
	switch_channel_t *caller_channel;
	switch_core_session_t *peer_session = NULL;
	unsigned int timelimit = 60;
	const char *var, *continue_on_fail = NULL;
	uint8_t no_media_bridge = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (switch_strlen_zero(data)) {
		return;
	}

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	if ((var = switch_channel_get_variable(caller_channel, "call_timeout"))) {
		timelimit = atoi(var);
	}

	continue_on_fail = switch_channel_get_variable(caller_channel, "continue_on_fail");

	if (switch_channel_test_flag(caller_channel, CF_BYPASS_MEDIA)
		|| ((var = switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_VARIABLE)) && switch_true(var))) {
		if (!switch_channel_test_flag(caller_channel, CF_ANSWERED)
			&& !switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
			switch_channel_set_flag(caller_channel, CF_BYPASS_MEDIA);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel is already up, delaying point-to-point mode 'till both legs are up.\n");
			no_media_bridge = 1;
		}
	}

	if (switch_ivr_originate(session, &peer_session, &cause, data, timelimit, NULL, NULL, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", switch_channel_cause2str(cause));

		/* no answer is *always* a reason to continue */
		if (cause == SWITCH_CAUSE_NO_ANSWER || cause == SWITCH_CAUSE_NO_USER_RESPONSE) {
			return;
		}

		/* 
		   if the variable continue_on_fail is set it can be:
		   'true' to continue on all failures.
		   'false' to not continue.
		   A list of codes either names or numbers eg "user_busy,normal_temporary_failure"
		*/
		if (continue_on_fail) {
			const char *cause_str;
			char cause_num[35] = "";

			cause_str = switch_channel_cause2str(cause);
			snprintf(cause_num, sizeof(cause_num), "%u", cause);

			if (switch_true(continue_on_fail) || switch_stristr(cause_str, continue_on_fail) || strstr(cause_str, cause_num)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Continue on fail [%s]:  Cause: %s\n", continue_on_fail, cause_str);
				return;
			}

		}

		switch_channel_hangup(caller_channel, cause);
		return;
	} else {
		if (no_media_bridge) {
			switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
			switch_frame_t *read_frame;
			/* SIP won't let us redir media until the call has been answered #$^#%& so we will proxy any early media until they do */
			while (switch_channel_ready(caller_channel) && switch_channel_ready(peer_channel)
				   && !switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
				switch_status_t status = switch_core_session_read_frame(peer_session, &read_frame, -1, 0);
				uint8_t bad = 1;

				if (SWITCH_READ_ACCEPTABLE(status)
					&& switch_core_session_write_frame(session, read_frame, -1, 0) == SWITCH_STATUS_SUCCESS) {
					bad = 0;
				}
				if (bad) {
					switch_channel_hangup(caller_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					switch_channel_hangup(peer_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					goto end;
				}
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Redirecting media to point-to-point mode.\n");
			switch_ivr_nomedia(switch_core_session_get_uuid(session), SMF_FORCE);
			switch_ivr_nomedia(switch_core_session_get_uuid(peer_session), SMF_FORCE);
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			if (switch_channel_test_flag(caller_channel, CF_BYPASS_MEDIA)) {
				switch_ivr_signal_bridge(session, peer_session);
			} else {
				switch_ivr_multi_threaded_bridge(session, peer_session, NULL, NULL, NULL);
			}
		}
	end:
		if (peer_session) {
			switch_core_session_rwunlock(peer_session);
		}
	}
}


#define SPEAK_DESC "Speak text to a channel via the tts interface"
#define DISPLACE_DESC "Displace audio from a file to the channels input"
#define SESS_REC_DESC "Starts a background recording of the entire session"
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
#define TRANSFER_LONG_DESC "Immediatly transfer the calling channel to a new extension"
#define SLEEP_LONG_DESC "Pause the channel for a given number of milliseconds, consuming the audio for that period of time."
SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

 	SWITCH_ADD_API(api_interface, "strepoch", "Convert a date string into epoch time", strepoch_api_function, "<string>");
	SWITCH_ADD_API(api_interface, "chat", "chat", chat_api_function, "<proto>|<from>|<to>|<message>");
	SWITCH_ADD_API(api_interface, "strftime", "strftime", strftime_api_function, "<format_string>");
	SWITCH_ADD_API(api_interface, "presence", "presence", presence_api_function, "<user> <rpid> <message>");
	SWITCH_ADD_APP(app_interface, "privacy", "Set privacy on calls", "Set caller privacy on calls.", privacy_function, "off|on|name|full|number", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "transfer", "Transfer a channel", TRANSFER_LONG_DESC, transfer_function, "<exten> [<dialplan> <context>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sleep", "Pause a channel", SLEEP_LONG_DESC, sleep_function, "<pausemilliseconds>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "delay_echo", "echo audio at a specified delay", "Delay n ms", delay_function, "<delay ms>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "strftime", NULL, NULL, strftime_function, NULL, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "phrase", "Say a Phrase", "Say a Phrase", phrase_function, "<macro_name>,<data>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eval", "Do Nothing", "Do Nothing", eval_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "pre_answer", "Pre-Answer the call", "Pre-Answer the call for a channel.", pre_answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "answer", "Answer the call", "Answer the call for a channel.", answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hangup", "Hangup the call", "Hangup the call for a channel.", hangup_function, "[<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "log", "Logs a channel variable", LOG_LONG_DESC, log_function, "<varname>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "info", "Display Call Info", "Display Call Info", info_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "event", "Fire an event", "Fire an event", event_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "export", "Export a channel variable across a bridge", EXPORT_LONG_DESC, export_function, "<varname>=<value>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set", "Set a channel variable", SET_LONG_DESC, set_function, "<varname>=<value>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_global", "Set a global variable", SET_GLOBAL_LONG_DESC, set_global_function, "<varname>=<value>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_profile_var", "Set a caller profile variable", SET_PROFILE_VAR_LONG_DESC, set_profile_var_function, "<varname>=<value>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unset", "Unset a channel variable", UNSET_LONG_DESC, unset_function, "<varname>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "ring_ready", "Indicate Ring_Ready", "Indicate Ring_Ready on a channel.", ring_ready_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "break", "Break", "Set the break flag.", break_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "detect_speech", "Detect speech", "Detect speech on a channel.", detect_speech_function, DETECT_SPEECH_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "ivr", "Run an ivr menu", "Run an ivr menu.", ivr_application_function, "<menu_name>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "redirect", "Send session redirect", "Send a redirect message to a session.", redirect_function, "<redirect_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "reject", "Send session reject", "Send a reject message to a session.", reject_function, "<reject_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "queue_dtmf", "Queue dtmf to be sent", "Queue dtmf to be sent from a session", queue_dtmf_function, "<dtmf_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_hangup", SCHED_HANGUP_DESCR, SCHED_HANGUP_DESCR, sched_hangup_function, "[+]<time> [<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_broadcast", SCHED_BROADCAST_DESCR, SCHED_BROADCAST_DESCR, sched_broadcast_function, "[+]<time> <path> [aleg|bleg|both]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_transfer", SCHED_TRANSF_DESCR, SCHED_TRANSF_DESCR, sched_transfer_function, "[+]<time> <extension> <dialplan> <context>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "execute_extension", "Execute an extension", "Execute an extension", exe_function, EXE_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_user", "Set a User", "Set a User", set_user_function, SET_USER_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "stop_dtmf", "stop inband dtmf", "Stop detecting inband dtmf.", stop_dtmf_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "start_dtmf", "Detect dtmf", "Detect inband dtmf on the session", dtmf_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_dtmf_generate", "stop inband dtmf generation", "Stop generating inband dtmf.", 
				   stop_dtmf_session_generate_function, "[write]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "start_dtmf_generate", "Generate dtmf", "Generate inband dtmf on the session", dtmf_session_generate_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_tone_detect", "stop detecting tones", "Stop detecting tones", stop_fax_detect_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fax_detect", "Detect faxes", "Detect fax send tone", fax_detect_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "tone_detect", "Detect tones", "Detect tones", tone_detect_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "echo", "Echo", "Perform an echo test against the calling channel", echo_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "park", NULL, NULL, park_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "gentones", "Generate Tones", "Generate tones to the channel", gentones_function, "<tgml_script>[|<loops>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "playback", "Playback File", "Playback a file to the channel", playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_record_session", "Stop Record Session", STOP_SESS_REC_DESC, stop_record_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_session", "Record Session", SESS_REC_DESC, record_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record", "Record File", "Record a file from the channels input", record_function, "<path> [<time_limit_secs>] [<silence_thresh>] [<silence_hits>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_displace_session", "Stop Displace File", "Stop Displacing to a file", stop_displace_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "displace_session", "Displace File", DISPLACE_DESC, displace_session_function, "<path> [+time_limit_ms] [mux]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "speak", "Speak text", SPEAK_DESC, speak_function, "<engine>|<voice>|<text>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "clear_speech_cache", "Clear Speech Handle Cache", "Clear Speech Handle Cache", clear_speech_cache_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "bridge", "Bridge Audio", "Bridge the audio between two sessions", audio_bridge_function, "<channel_url>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "system", "Execute a system command", "Execute a system command", system_session_function, "<command>", SAF_SUPPORT_NOMEDIA);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
