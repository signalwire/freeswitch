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
 *
 *
 * mod_dptools.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_dptools";

static void transfer_function(switch_core_session_t *session, char *data)
{
	int argc;
	char *argv[4] = {0};
	char *mydata;

	if ((mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No extension specified.\n");
		}
	}
}

static void sleep_function(switch_core_session_t *session, char *data)
{

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No timeout specified.\n");
	} else {
		uint32_t ms = atoi(data);
		switch_ivr_sleep(session, ms);
	}
}

static void eval_function(switch_core_session_t *session, char *data)
{
	return;
}

static void answer_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);
	switch_channel_answer(channel);
}

static void set_function(switch_core_session_t *session, char *data)
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
			if (!strcmp(val, "_UNDEF_")) {
				val = NULL;
			}
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", var, val ? val : "UNDEF");
		switch_channel_set_variable(channel, var, val);
	}
}

static void privacy_function(switch_core_session_t *session, char *data)
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

		if(!strcasecmp(arg, "no")) {
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

static void strftime_function(switch_core_session_t *session, char *data)
{
	char *argv[2];
	int argc;
	char *lbuf;

	if ((lbuf = switch_core_session_strdup(session, data))&&(argc = switch_separate_string(lbuf, '=', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
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

static switch_status_t strftime_api_function(char *fmt, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_size_t retsize;
	switch_time_exp_t tm;
	char date[80] = "";
	
	switch_time_exp_lt(&tm, switch_time_now());
	switch_strftime(date, &retsize, sizeof(date), fmt, &tm);
	stream->write_function(stream, date);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t presence_api_function(char *fmt, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_event_t *event;
	char *lbuf, *argv[4];
	int argc = 0;
	switch_event_types_t type = SWITCH_EVENT_PRESENCE_IN;

	if ((lbuf = strdup(fmt)) && (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) > 0) {
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


static switch_status_t chat_api_function(char *fmt, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char *lbuf, *argv[4];
	int argc = 0;

	if ((lbuf = strdup(fmt)) && (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) == 4) {
		switch_chat_interface_t *ci;
		
		if ((ci = switch_loadable_module_get_chat_interface(argv[0]))) {
			ci->chat_send("dp", argv[1], argv[2], "", argv[3], "");
			stream->write_function(stream, "Sent");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invaid Chat Interface [%s]!\n", argv[0]);
		}
	} else {
		stream->write_function(stream, "Invalid");
	}
	
	return SWITCH_STATUS_SUCCESS;
}


static switch_api_interface_t chat_api_interface = {
	/*.interface_name */ "chat",
	/*.desc */ "chat",
	/*.function */ chat_api_function,
	/*.syntax */ "<proto>|<from>|<to>|<message>",
	/*.next */ NULL
};

static switch_api_interface_t dptools_api_interface = {
	/*.interface_name */ "strftime",
	/*.desc */ "strftime",
	/*.function */ strftime_api_function,
	/*.syntax */ "<format_string>",
	/*.next */ &chat_api_interface
};


static switch_api_interface_t presence_api_interface = {
	/*.interface_name */ "presence",
	/*.desc */ "presence",
	/*.function */ presence_api_function,
	/*.syntax */ "<user> <rpid> <message>",
	/*.next */ &dptools_api_interface
};

static const switch_application_interface_t set_application_interface = {
	/*.interface_name */ "set",
	/*.application_function */ set_function,
	/* long_desc */ "Set a channel varaible for the channel calling the application.",
	/* short_desc */ "Set a channel varaible",
	/* syntax */ "<varname>=[<value>|_UNDEF_]",
	/*.next */ NULL
};

static const switch_application_interface_t answer_application_interface = {
	/*.interface_name */ "answer",
	/*.application_function */ answer_function,
	/* long_desc */ "Answer the call for a channel.",
	/* short_desc */ "Answer the call",
	/* syntax */ "",
	/*.next */ &set_application_interface

};

static const switch_application_interface_t eval_application_interface = {
	/*.interface_name */ "eval",
	/*.application_function */ eval_function,
	/* long_desc */ "Do Nothing",
	/* short_desc */ "Do Nothing",
	/* syntax */ "",
	/*.next */ &answer_application_interface

};

static const switch_application_interface_t strftime_application_interface = {
	/*.interface_name */ "strftime",
	/*.application_function */ strftime_function,
	/* long_desc */ NULL,
	/* short_desc */ NULL,
	/* syntax */ NULL,
	/*.next */ &eval_application_interface

};

static const switch_application_interface_t sleep_application_interface = {
	/*.interface_name */ "sleep",
	/*.application_function */ sleep_function,
	/* long_desc */ "Pause the channel for a given number of milliseconds, consuming the audio for that period of time.",
	/* short_desc */ "Pause a channel",
	/* syntax */ "<pausemilliseconds>",
	/* next */ &strftime_application_interface
};

static const switch_application_interface_t transfer_application_interface = {
	/*.interface_name */ "transfer",
	/*.application_function */ transfer_function,
	/* long_desc */ "Immediatly transfer the calling channel to a new extension",
	/* short_desc */ "Transfer a channel",
	/* syntax */ "<exten> [<dialplan> <context>]",
	/* next */ &sleep_application_interface
};

static const switch_application_interface_t privacy_application_interface = {
	/*.interface_name */ "privacy",
	/*.application_function */ privacy_function,
	/* long_desc */ "Set caller privacy on calls.",
	/* short_desc */ "Set privacy on calls",
	/* syntax */ "off|on|name|full|number",
	/*.next */ &transfer_application_interface
};

static const switch_loadable_module_interface_t mod_dptools_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &privacy_application_interface,
	/*.api_interface */ &presence_api_interface
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_dptools_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
*/


//switch_status_t switch_module_runtime(void)
