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

static switch_application_interface_t detect_speech_application_interface;
static switch_application_interface_t exe_application_interface;

static void detect_speech_function(switch_core_session_t *session, char *data)
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", detect_speech_application_interface.syntax);
	}

}

static void exe_function(switch_core_session_t *session, char *data)
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", exe_application_interface.syntax);
	}
}

static void ring_ready_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_channel_ring_ready(channel);
}

static void queue_dtmf_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	if (!switch_strlen_zero(data)) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);
		switch_channel_queue_dtmf(channel, data);
	}
}

static void transfer_function(switch_core_session_t *session, char *data)
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

static void sched_transfer_function(switch_core_session_t *session, char *data)
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

static void sched_hangup_function(switch_core_session_t *session, char *data)
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


static void sched_broadcast_function(switch_core_session_t *session, char *data)
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

		lang = switch_channel_get_variable(channel, "language");

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

static void pre_answer_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);

	assert(channel != NULL);
	switch_channel_pre_answer(channel);
}

static void redirect_function(switch_core_session_t *session, char *data)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to redirect */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
	switch_core_session_receive_message(session, &msg);

}

static void reject_function(switch_core_session_t *session, char *data)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to reject the call */
	msg.from = __FILE__;
	msg.string_arg = data;
	msg.message_id = SWITCH_MESSAGE_INDICATE_REJECT;
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
			if (switch_strlen_zero(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", var, val ? val : "UNDEF");
		switch_channel_set_variable(channel, var, val);
	}
}

static void export_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	char *exports, *new_exports = NULL, *new_exports_d = NULL, *var, *val = NULL, *var_name = NULL;
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

static void unset_function(switch_core_session_t *session, char *data)
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

static void log_function(switch_core_session_t *session, char *data)
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

static void strftime_function(switch_core_session_t *session, char *data)
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

	switch_time_exp_lt(&tm, switch_time_now());
	switch_strftime(date, &retsize, sizeof(date), cmd ? cmd : "%Y-%m-%d %T", &tm);
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

static void ivr_application_function(switch_core_session_t *session, char *data)
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


static void dtm_session_function(switch_core_session_t *session, char *data)
{
	switch_ivr_inband_dtmf_session(session);
}


static void stop_dtmf_session_function(switch_core_session_t *session, char *data)
{
	switch_ivr_stop_inband_dtmf_session(session);
}

static void fax_detect_session_function(switch_core_session_t *session, char *data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Enabling fax detection\n");
	switch_ivr_fax_detect_session(session);
}

static void stop_fax_detect_session_function(switch_core_session_t *session, char *data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Disabling fax detection\n");
	switch_ivr_stop_fax_detect_session(session);
}

static void echo_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_answer(channel);

	switch_channel_set_state(channel, CS_LOOPBACK);
}

static void park_function(switch_core_session_t *session, char *data)
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
	case SWITCH_INPUT_TYPE_DTMF:{
			char *dtmf = (char *) input;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);

			if (*dtmf == '*') {
				return SWITCH_STATUS_BREAK;
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


static void speak_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	char buf[10];
	char *argv[4] = { 0 };
	int argc;
	char *engine = NULL;
	char *voice = NULL;
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

	engine = argv[0];
	voice = argv[1];
	text = argv[2];

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
	switch_ivr_speak_text(session, engine, voice, codec->implementation->samples_per_second, text, &args);

}

static void playback_function(switch_core_session_t *session, char *data)
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

static void displace_session_function(switch_core_session_t *session, char *data)
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


static void stop_displace_session_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_ivr_stop_displace_session(session, data);
}


static void record_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	switch_status_t status;
	uint32_t limit = 0;
	char *path;
	char *p;
	switch_input_args_t args = { 0 };

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

	args.input_callback = on_dtmf;
	status = switch_ivr_record_file(session, NULL, path, &args, limit);

	if (!switch_channel_ready(channel) || (status != SWITCH_STATUS_SUCCESS && !SWITCH_STATUS_IS_BREAK(status))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
}


static void record_session_function(switch_core_session_t *session, char *data)
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


static void stop_record_session_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_ivr_stop_record_session(session, data);
}

/********************************************************************************/
/*								Bridge Functions								*/
/********************************************************************************/

static void audio_bridge_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *caller_channel;
	switch_core_session_t *peer_session = NULL;
	unsigned int timelimit = 60;
	char *var;
	uint8_t no_media_bridge = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	uint8_t do_continue = 0;

	if (switch_strlen_zero(data)) {
		return;
	}

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	if ((var = switch_channel_get_variable(caller_channel, "call_timeout"))) {
		timelimit = atoi(var);
	}

	if ((var = switch_channel_get_variable(caller_channel, "continue_on_fail"))) {
		do_continue = switch_true(var);
	}

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
		if (!do_continue && cause != SWITCH_CAUSE_NO_ANSWER) {
			/* All Causes besides NO_ANSWER terminate the originating session unless continue_on_fail is set.
			   We will pass the fail cause on when we hangup. */
			switch_channel_hangup(caller_channel, cause);
		}
		/* Otherwise.. nobody answered.  Go back to the dialplan instructions in case there was more to do. */
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


static switch_application_interface_t bridge_application_interface = {
	/*.interface_name */ "bridge",
	/*.application_function */ audio_bridge_function,
	/* long_desc */ "Bridge the audio between two sessions",
	/* short_desc */ "Bridge Audio",
	/* syntax */ "<channel_url>",
	/* flags */ SAF_SUPPORT_NOMEDIA
};

static switch_application_interface_t speak_application_interface = {
	/*.interface_name */ "speak",
	/*.application_function */ speak_function,
	/* long_desc */ "Speak text to a channel via the tts interface",
	/* short_desc */ "Speak text",
	/* syntax */ "<engine>|<voice>|<text>",
	/* flags */ SAF_NONE,
	&bridge_application_interface
};

static switch_application_interface_t displace_application_interface = {
	/*.interface_name */ "displace",
	/*.application_function */ displace_session_function,
	/* long_desc */ "Displace audio from a file to the channels input",
	/* short_desc */ "Displace File",
	/* syntax */ "<path> [+time_limit_ms] [mux]",
	/* flags */ SAF_NONE,
	&speak_application_interface
};

static switch_application_interface_t stop_displace_application_interface = {
	/*.interface_name */ "displace",
	/*.application_function */ stop_displace_session_function,
	/* long_desc */ "Stop Displacing to a file",
	/* short_desc */ "Stop Displace File",
	/* syntax */ "<path>",
	/* flags */ SAF_NONE,
	&displace_application_interface
};

static switch_application_interface_t record_application_interface = {
	/*.interface_name */ "record",
	/*.application_function */ record_function,
	/* long_desc */ "Record a file from the channels input",
	/* short_desc */ "Record File",
	/* syntax */ "<path> [+time_limit_ms]",
	/* flags */ SAF_NONE,
	&stop_displace_application_interface
};


static switch_application_interface_t record_session_application_interface = {
	/*.interface_name */ "record_session",
	/*.application_function */ record_session_function,
	/* long_desc */ "Starts a background recording of the entire session",
	/* short_desc */ "Record Session",
	/* syntax */ "<path>",
	/* flags */ SAF_NONE,
	&record_application_interface
};


static switch_application_interface_t stop_record_session_application_interface = {
	/*.interface_name */ "stop_record_session",
	/*.application_function */ stop_record_session_function,
	/* long_desc */ "Stops a background recording of the entire session",
	/* short_desc */ "Stop Record Session",
	/* syntax */ "<path>",
	/* flags */ SAF_NONE,
	&record_session_application_interface
};

static switch_application_interface_t playback_application_interface = {
	/*.interface_name */ "playback",
	/*.application_function */ playback_function,
	/* long_desc */ "Playback a file to the channel",
	/* short_desc */ "Playback File",
	/* syntax */ "<path>",
	/* flags */ SAF_NONE,
	/*.next */ &stop_record_session_application_interface
};
static switch_application_interface_t park_application_interface = {
	/*.interface_name */ "park",
	/*.application_function */ park_function,
	/* long_desc */ NULL,
	/* short_desc */ NULL,
	/* syntax */ NULL,
	/* flags */ SAF_NONE,
	/*.next */ &playback_application_interface
};

static switch_application_interface_t echo_application_interface = {
	/*.interface_name */ "echo",
	/*.application_function */ echo_function,
	/* long_desc */ "Perform an echo test against the calling channel",
	/* short_desc */ "Echo",
	/* syntax */ "",
	/* flags */ SAF_NONE,
	/*.next */ &park_application_interface
};

static switch_application_interface_t fax_detect_application_interface = {
	/*.interface_name */ "fax_detect",
	/*.application_function */ fax_detect_session_function,
	/* long_desc */ "Detect fax send tone",
	/* short_desc */ "Detect faxes",
	/* syntax */ "",
	/* flags */ SAF_NONE,
	/*.next */ &echo_application_interface
};

static switch_application_interface_t stop_fax_detect_application_interface = {
	/*.interface_name */ "stop_fax_detect",
	/*.application_function */ stop_fax_detect_session_function,
	/* long_desc */ "Stop detecting fax send tones",
	/* short_desc */ "stop detecting faxes",
	/* syntax */ "",
	/* flags */ SAF_NONE,
	/* next */ &fax_detect_application_interface
};

static switch_application_interface_t dtmf_application_interface = {
	/*.interface_name */ "start_dtmf",
	/*.application_function */ dtm_session_function,
	/* long_desc */ "Detect inband dtmf on the session",
	/* short_desc */ "Detect dtmf",
	/* syntax */ "",
	/* flags */ SAF_NONE,
	/* next */ &stop_fax_detect_application_interface
};

static switch_application_interface_t stop_dtmf_application_interface = {
	/*.interface_name */ "stop_dtmf",
	/*.application_function */ stop_dtmf_session_function,
	/* long_desc */ "Stop detecting inband dtmf.",
	/* short_desc */ "stop inband dtmf.",
	/* syntax */ "",
	/* flags */ SAF_NONE,
	&dtmf_application_interface
};

static switch_application_interface_t exe_application_interface = {
	/*.interface_name */ "execute_extension",
	/*.application_function */ exe_function,
	/*.long_desc */ "Execute an extension",
	/*.short_desc */ "Execute an extension",
	/*.syntax */ "<extension> <dialplan> <context>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &stop_dtmf_application_interface
};

static switch_application_interface_t sched_transfer_application_interface = {
	/*.interface_name */ "sched_transfer",
	/*.application_function */ sched_transfer_function,
	/*.long_desc */ "Schedule a transfer in the future",
	/*.short_desc */ "Schedule a transfer in the future",
	/*.syntax */ "[+]<time> <extension> <dialplan> <context>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &exe_application_interface
};

static switch_application_interface_t sched_broadcast_application_interface = {
	/*.interface_name */ "sched_broadcast",
	/*.application_function */ sched_broadcast_function,
	/*.long_desc */ "Schedule a broadcast in the future",
	/*.short_desc */ "Schedule a broadcast in the future",
	/*.syntax */ "[+]<time> <path> [aleg|bleg|both]",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &sched_transfer_application_interface
};

static switch_application_interface_t sched_hangup_application_interface = {
	/*.interface_name */ "sched_hangup",
	/*.application_function */ sched_hangup_function,
	/*.long_desc */ "Schedule a hangup in the future",
	/*.short_desc */ "Schedule a hangup in the future",
	/*.syntax */ "[+]<time> [<cause>]",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &sched_broadcast_application_interface
};


static switch_application_interface_t queuedtmf_application_interface = {
	/*.interface_name */ "queue_dtmf",
	/*.application_function */ queue_dtmf_function,
	/* long_desc */ "Queue dtmf to be sent from a session",
	/* short_desc */ "Queue dtmf to be sent",
	/* syntax */ "<dtmf_data>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &sched_hangup_application_interface
};

static switch_application_interface_t reject_application_interface = {
	/*.interface_name */ "reject",
	/*.application_function */ reject_function,
	/* long_desc */ "Send a reject message to a session.",
	/* short_desc */ "Send session reject",
	/* syntax */ "<reject_data>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &queuedtmf_application_interface
};

static switch_application_interface_t redirect_application_interface = {
	/*.interface_name */ "redirect",
	/*.application_function */ redirect_function,
	/* long_desc */ "Send a redirect message to a session.",
	/* short_desc */ "Send session redirect",
	/* syntax */ "<redirect_data>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &reject_application_interface
};

static switch_application_interface_t ivr_application_interface = {
	/*.interface_name */ "ivr",
	/*.application_function */ ivr_application_function,
	/* long_desc */ "Run an ivr menu.",
	/* short_desc */ "Run an ivr menu",
	/* syntax */ "<menu_name>",
	/* flags */ SAF_NONE,
	/*.next */ &redirect_application_interface
};

static switch_application_interface_t detect_speech_application_interface = {
	/*.interface_name */ "detect_speech",
	/*.application_function */ detect_speech_function,
	/* long_desc */ "Detect speech on a channel.",
	/* short_desc */ "Detect speech",
	/* syntax */ "<mod_name> <gram_name> <gram_path> [<addr>] OR grammar <gram_name> [<path>] OR pause OR resume",
	/* flags */ SAF_NONE,
	/*.next */ &ivr_application_interface
};

static switch_application_interface_t ring_ready_application_interface = {
	/*.interface_name */ "ring_ready",
	/*.application_function */ ring_ready_function,
	/* long_desc */ "Indicate Ring_Ready on a channel.",
	/* short_desc */ "Indicate Ring_Ready",
	/* syntax */ "",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &detect_speech_application_interface
};

static switch_application_interface_t unset_application_interface = {
	/*.interface_name */ "unset",
	/*.application_function */ unset_function,
	/* long_desc */ "Unset a channel varaible for the channel calling the application.",
	/* short_desc */ "Unset a channel varaible",
	/* syntax */ "<varname>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &ring_ready_application_interface
};

static switch_application_interface_t set_application_interface = {
	/*.interface_name */ "set",
	/*.application_function */ set_function,
	/* long_desc */ "Set a channel varaible for the channel calling the application.",
	/* short_desc */ "Set a channel varaible",
	/* syntax */ "<varname>=<value>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &unset_application_interface
};

static switch_application_interface_t export_application_interface = {
	/*.interface_name */ "export",
	/*.application_function */ export_function,
	/* long_desc */ "Set and export a channel varaible for the channel calling the application.",
	/* short_desc */ "Export a channel varaible across a bridge",
	/* syntax */ "<varname>=<value>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &set_application_interface
};

static switch_application_interface_t info_application_interface = {
	/*.interface_name */ "info",
	/*.application_function */ info_function,
	/* long_desc */ "Display Call Info",
	/* short_desc */ "Display Call Info",
	/* syntax */ "",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &export_application_interface
};

static switch_application_interface_t log_application_interface = {
	/*.interface_name */ "log",
	/*.application_function */ log_function,
	/* long_desc */ "Logs a channel varaible for the channel calling the application.",
	/* short_desc */ "Logs a channel varaible",
	/* syntax */ "<varname>",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &info_application_interface
};


static switch_application_interface_t hangup_application_interface = {
	/*.interface_name */ "hangup",
	/*.application_function */ hangup_function,
	/* long_desc */ "Hangup the call for a channel.",
	/* short_desc */ "Hangup the call",
	/* syntax */ "[<cause>]",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &log_application_interface
};

static switch_application_interface_t answer_application_interface = {
	/*.interface_name */ "answer",
	/*.application_function */ answer_function,
	/* long_desc */ "Answer the call for a channel.",
	/* short_desc */ "Answer the call",
	/* syntax */ "",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &hangup_application_interface
};

static switch_application_interface_t pre_answer_application_interface = {
	/*.interface_name */ "pre_answer",
	/*.application_function */ pre_answer_function,
	/* long_desc */ "Pre-Answer the call for a channel.",
	/* short_desc */ "Pre-Answer the call",
	/* syntax */ "",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &answer_application_interface
};

static switch_application_interface_t eval_application_interface = {
	/*.interface_name */ "eval",
	/*.application_function */ eval_function,
	/* long_desc */ "Do Nothing",
	/* short_desc */ "Do Nothing",
	/* syntax */ "",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &pre_answer_application_interface
};

static switch_application_interface_t phrase_application_interface = {
	/*.interface_name */ "phrase",
	/*.application_function */ phrase_function,
	/* long_desc */ "Say a Phrase",
	/* short_desc */ "Say a Phrase",
	/* syntax */ "<macro_name>,<data>",
	/* flags */ SAF_NONE,
	/*.next */ &eval_application_interface
};

static switch_application_interface_t strftime_application_interface = {
	/*.interface_name */ "strftime",
	/*.application_function */ strftime_function,
	/* long_desc */ NULL,
	/* short_desc */ NULL,
	/* syntax */ NULL,
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &phrase_application_interface
};

static switch_application_interface_t sleep_application_interface = {
	/*.interface_name */ "sleep",
	/*.application_function */ sleep_function,
	/* long_desc */
	"Pause the channel for a given number of milliseconds, consuming the audio for that period of time.",
	/* short_desc */ "Pause a channel",
	/* syntax */ "<pausemilliseconds>",
	/* flags */ SAF_NONE,
	/* next */ &strftime_application_interface
};

static switch_application_interface_t transfer_application_interface = {
	/*.interface_name */ "transfer",
	/*.application_function */ transfer_function,
	/* long_desc */ "Immediatly transfer the calling channel to a new extension",
	/* short_desc */ "Transfer a channel",
	/* syntax */ "<exten> [<dialplan> <context>]",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/* next */ &sleep_application_interface
};

static switch_application_interface_t privacy_application_interface = {
	/*.interface_name */ "privacy",
	/*.application_function */ privacy_function,
	/* long_desc */ "Set caller privacy on calls.",
	/* short_desc */ "Set privacy on calls",
	/* syntax */ "off|on|name|full|number",
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &transfer_application_interface
};

static switch_loadable_module_interface_t dptools_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &privacy_application_interface,
	/*.api_interface */ &presence_api_interface
};

SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &dptools_module_interface;


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
