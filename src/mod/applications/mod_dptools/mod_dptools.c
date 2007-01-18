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
 *
 * mod_dptools.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_dptools";

static const switch_application_interface_t detect_speech_application_interface;

static void detect_speech_function(switch_core_session_t *session, char *data)
{
	char *argv[4];
	int argc;
	char *lbuf = NULL;

	if (data && (lbuf = switch_core_session_strdup(session, data)) && (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", detect_speech_application_interface.syntax);
	}
	
}

static void ringback_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);
	switch_channel_ringback(channel);
}

static void transfer_function(switch_core_session_t *session, char *data)
{
	int argc;
	char *argv[4] = {0};
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
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

static void phrase_function(switch_core_session_t *session, char *data)
{
    switch_channel_t *channel;
    char *mydata = NULL;

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);
    
    if ((mydata = switch_core_session_strdup(session, data))) {
        char *lang;
        char *macro = mydata;
        char *mdata = NULL;

        if ((mdata = strchr(macro, ','))) {
            *mdata++ = '\0';
        }
        if (!(lang = switch_channel_get_variable(channel, "language"))) {
            lang = "en";
        }
        
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Execute %s(%s) lang %s\n", macro, mdata, lang);
        switch_ivr_phrase_macro(session, macro, mdata, lang, NULL);
    }

}


static void hangup_function(switch_core_session_t *session, char *data)
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

static void answer_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);

    assert(channel != NULL);
	switch_channel_answer(channel);
}

static void redirect_function(switch_core_session_t *session, char *data)
{
    switch_core_session_message_t msg = {0};

    /* Tell the channel to redirect */
	msg.from = __FILE__;
	msg.string_arg = data;
    msg.message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
	switch_core_session_receive_message(session, &msg);

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

static void log_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
    char *level, *log_str;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    if (data && (level = strdup(data))) {
        switch_event_types_t etype = SWITCH_LOG_DEBUG;
        
        if ((log_str = strchr(level, ' '))) {
            *log_str++ = '\0';
            switch_name_event(level, &etype);
        } else {
            log_str = level;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, etype, "%s\n", log_str);
        switch_safe_free(level);
    }
}


static void info_function(switch_core_session_t *session, char *data)
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

	if (data && (lbuf = switch_core_session_strdup(session, data)) && (argc = switch_separate_string(lbuf, '=', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
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


static switch_status_t strepoch_api_function(char *data, switch_core_session_t *session, switch_stream_handle_t *stream)
{
    switch_time_t out;
    
    if (switch_strlen_zero(data)) {
        out = switch_time_now();
    } else {
        out = switch_str_time(data);
    }

    stream->write_function(stream, "%d", (uint32_t)apr_time_sec(out));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t strftime_api_function(char *fmt, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	
	switch_size_t retsize;
	switch_time_exp_t tm;
	char date[80] = "";
	
	switch_time_exp_lt(&tm, switch_time_now());
	switch_strftime(date, &retsize, sizeof(date), fmt ? fmt : "%Y-%m-%d %T", &tm);
	stream->write_function(stream, "%s", date);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t presence_api_function(char *fmt, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_event_t *event;
	char *lbuf, *argv[4];
	int argc = 0;
	switch_event_types_t type = SWITCH_EVENT_PRESENCE_IN;

	if (fmt && (lbuf = strdup(fmt)) && (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) > 0) {
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

	if (fmt && (lbuf = strdup(fmt)) && (argc = switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])))) == 4) {
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

static char *ivr_cf_name = "ivr.conf";

#ifdef _TEST_CALLBACK_
static switch_ivr_action_t menu_handler(switch_ivr_menu_t *menu, char *param, char *buf, size_t buflen, void *obj)
{
	switch_ivr_action_t action = SWITCH_IVR_ACTION_NOOP;

	if (param != NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "menu_handler '%s'\n",param);
	}

	return action;
}
#endif

static void ivr_application_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *params;

	if (channel && data && (params = switch_core_session_strdup(session,data))) {
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
					if (switch_ivr_menu_stack_xml_init(&xml_ctx,NULL) == SWITCH_STATUS_SUCCESS
#ifdef _TEST_CALLBACK_
						&& switch_ivr_menu_stack_xml_add_custom(xml_ctx, "custom", &menu_handler) == SWITCH_STATUS_SUCCESS
#endif
						&& switch_ivr_menu_stack_xml_build(xml_ctx,&menu_stack,xml_menus,xml_menu) == SWITCH_STATUS_SUCCESS)
					{
						switch_channel_answer(channel);
						switch_ivr_menu_execute(session,menu_stack,params,NULL);
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

static switch_api_interface_t strepoch_api_interface = {
	/*.interface_name */ "strepoch",
	/*.desc */ "Convert a date string into epoch time",
	/*.function */ strepoch_api_function,
	/*.syntax */ "<string>",
	/*.next */ NULL
};

static switch_api_interface_t chat_api_interface = {
	/*.interface_name */ "chat",
	/*.desc */ "chat",
	/*.function */ chat_api_function,
	/*.syntax */ "<proto>|<from>|<to>|<message>",
	/*.next */ &strepoch_api_interface
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

static const switch_application_interface_t redirect_application_interface = {
	/*.interface_name */ "redirect",
	/*.application_function */ redirect_function,	
	/* long_desc */ "Send a redirect message to a session.",
	/* short_desc */ "Send session redirect",
	/* syntax */ "<redirect_data>",
	/*.next */ NULL
};

static const switch_application_interface_t ivr_application_interface = {
	/*.interface_name */ "ivr",
	/*.application_function */ ivr_application_function,	
	/* long_desc */ "Run an ivr menu.",
	/* short_desc */ "Run an ivr menu",
	/* syntax */ "<menu_name>",
	/*.next */ &redirect_application_interface
};

static const switch_application_interface_t detect_speech_application_interface = {
	/*.interface_name */ "detect_speech",
	/*.application_function */ detect_speech_function,	
	/* long_desc */ "Detect speech on a channel.",
	/* short_desc */ "Detect speech",
	/* syntax */ "<mod_name> <gram_name> <gram_path> [<addr>] OR grammar <gram_name> [<path>] OR pause OR resume",
	/*.next */ &ivr_application_interface
};

static const switch_application_interface_t ringback_application_interface = {
	/*.interface_name */ "ringback",
	/*.application_function */ ringback_function,
	/* long_desc */ "Indicate Ringback on a channel.",
	/* short_desc */ "Indicate Ringback",
	/* syntax */ "",
	/*.next */ &detect_speech_application_interface
};

static const switch_application_interface_t set_application_interface = {
	/*.interface_name */ "set",
	/*.application_function */ set_function,
	/* long_desc */ "Set a channel varaible for the channel calling the application.",
	/* short_desc */ "Set a channel varaible",
	/* syntax */ "<varname>=[<value>|_UNDEF_]",
	/*.next */ &ringback_application_interface
};

static const switch_application_interface_t info_application_interface = {
	/*.interface_name */ "info",
	/*.application_function */ info_function,
	/* long_desc */ "Display Call Info",
	/* short_desc */ "Display Call Info",
	/* syntax */ "",
	/*.next */ &set_application_interface
};

static const switch_application_interface_t log_application_interface = {
	/*.interface_name */ "log",
	/*.application_function */ log_function,
	/* long_desc */ "Logs a channel varaible for the channel calling the application.",
	/* short_desc */ "Logs a channel varaible",
	/* syntax */ "<varname>",
	/*.next */ &info_application_interface
};


static const switch_application_interface_t hangup_application_interface = {
	/*.interface_name */ "hangup",
	/*.application_function */ hangup_function,
	/* long_desc */ "Hangup the call for a channel.",
	/* short_desc */ "Hangup the call",
	/* syntax */ "[<cause>]",
	/*.next */ &log_application_interface

};

static const switch_application_interface_t answer_application_interface = {
	/*.interface_name */ "answer",
	/*.application_function */ answer_function,
	/* long_desc */ "Answer the call for a channel.",
	/* short_desc */ "Answer the call",
	/* syntax */ "",
	/*.next */ &hangup_application_interface

};

static const switch_application_interface_t eval_application_interface = {
	/*.interface_name */ "eval",
	/*.application_function */ eval_function,
	/* long_desc */ "Do Nothing",
	/* short_desc */ "Do Nothing",
	/* syntax */ "",
	/*.next */ &answer_application_interface

};

static const switch_application_interface_t phrase_application_interface = {
	/*.interface_name */ "phrase",
	/*.application_function */ phrase_function,
	/* long_desc */ "Say a Phrase",
	/* short_desc */ "Say a Phrase",
	/* syntax */ "<macro_name>,<data>",
	/*.next */ &eval_application_interface

};

static const switch_application_interface_t strftime_application_interface = {
	/*.interface_name */ "strftime",
	/*.application_function */ strftime_function,
	/* long_desc */ NULL,
	/* short_desc */ NULL,
	/* syntax */ NULL,
	/*.next */ &phrase_application_interface

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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
