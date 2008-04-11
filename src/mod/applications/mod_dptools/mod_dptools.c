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

	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
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
	char *argv[4] = { 0 };
	int argc;
	char *lbuf = NULL;
	
	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_core_session_execute_exten(session, argv[0], argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", EXE_SYNTAX);
	}
}

#define SOFT_HOLD_SYNTAX "<unhold key> [<moh_a>] [<moh_b>]"
SWITCH_STANDARD_APP(soft_hold_function)
{
	char *argv[3] = { 0 };
	int argc;
	char *lbuf = NULL;
	
	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
		switch_ivr_soft_hold(session, argv[0], argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", SOFT_HOLD_SYNTAX);
	}
}

#define BIND_SYNTAX "<key> [a|b] [a|b] <app>"
SWITCH_STANDARD_APP(dtmf_bind_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *lbuf = NULL;
	
	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) == 4) {
		int kval = atoi(argv[0]);
		char a1 = (char)tolower(*argv[1]);
		char a2 = (char)tolower(*argv[2]);
		
		if (switch_ivr_bind_dtmf_meta_session(session, kval, a1 == 'b', a2 == 'b', argv[3]) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bind Error!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", BIND_SYNTAX);
	}
}

#define INTERCEPT_SYNTAX "<uuid>"
SWITCH_STANDARD_APP(intercept_function)
{
	switch_ivr_intercept_session(session, data);
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
	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", eavesdrop_SYNTAX);
	} else {
		if (!strcasecmp((char *)data, "all")) {
			switch_core_db_t *db = switch_core_db_handle();
			char *errmsg = NULL;
			struct e_data e_data = {{ 0 }};
			char *sql = switch_mprintf("select uuid from channels where uuid != '%q'", switch_core_session_get_uuid(session));
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *file = NULL;
			int x = 0;
			char buf[2] = "";
			switch_size_t buflen = sizeof(buf);
			char terminator;

			while(switch_channel_ready(channel)) {
				for(x = 0; x < MAX_SPY; x++) {
					switch_safe_free(e_data.uuid_list[x]);
				}
				e_data.total = 0;
				switch_core_db_exec(db, sql, e_callback, &e_data, &errmsg);
				if (errmsg) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error: %s\n", errmsg);
					switch_core_db_free(errmsg);
					if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_failed"))) {
						switch_ivr_play_file(session, NULL, file, NULL);
					}
					switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 5000, 0, 0);
					continue;
				}
				if (e_data.total) {
					for (x = 0; x < e_data.total && switch_channel_ready(channel); x++) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Spy: %s\n", e_data.uuid_list[x]);
						if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_new"))) {
							switch_ivr_play_file(session, NULL, file, NULL);
						}
						if (switch_ivr_eavesdrop_session(session, e_data.uuid_list[x], ED_DTMF) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Spy: %s Failed\n", e_data.uuid_list[x]);
							if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_failed"))) {
								switch_ivr_play_file(session, NULL, file, NULL);
							}
							switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 5000, 0, 0);
						}
					}
				} else {
					if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_idle"))) {
						switch_ivr_play_file(session, NULL, file, NULL);
					}
					switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 2000, 0, 0);
				}
			}

			for(x = 0; x < MAX_SPY; x++) {
				switch_safe_free(e_data.uuid_list[x]);
			}

			switch_core_db_close(db);
			
		} else {
			switch_ivr_eavesdrop_session(session, data, ED_DTMF);
		}
	}
}


#define eavesdrop_SYNTAX "<uuid>"
SWITCH_STANDARD_APP(three_way_function)
{
	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", eavesdrop_SYNTAX);
	} else {
		switch_ivr_eavesdrop_session(session, data, ED_MUX_READ | ED_MUX_WRITE);
	}
}

#define SET_USER_SYNTAX "<user>@<domain>"
SWITCH_STANDARD_APP(set_user_function)
{
	switch_xml_t x_domain, xml = NULL, x_user, x_param, x_params;	
	char *user, *mailbox, *domain;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_strlen_zero(data)) {
		goto error;
	}

	user = switch_core_session_strdup(session, data);

	if (!(domain = strchr(user, '@'))) {
		goto error;
	}

	*domain++ = '\0';
	
	if (switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, NULL) != SWITCH_STATUS_SUCCESS) {
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
	switch_channel_ring_ready(switch_core_session_get_channel(session));
}

SWITCH_STANDARD_APP(break_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_BREAK);
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

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
			if (!switch_check_network_list_ip(argv[0], argv[1])) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				if (argc > 2) {
					cause = switch_channel_str2cause(argv[2]);
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call failed acl check for ip %s on list %s\n", argv[0], argv[1]);
				switch_channel_hangup(channel, cause);
			}
		}
	}
	
}

SWITCH_STANDARD_APP(transfer_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;
	int bleg = 0, both = 0;

	
	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			bleg = !strcasecmp(argv[0], "-bleg");
			both = !strcasecmp(argv[0], "-both");

			if (bleg || both) {
				const char *uuid;
				switch_channel_t *channel = switch_core_session_get_channel(session);																		
				if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
					switch_core_session_t *b_session;
					if ((b_session = switch_core_session_locate(uuid))) {
						switch_ivr_session_transfer(b_session, argv[1], argv[2], argv[3]);
						switch_core_session_rwunlock(b_session);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No B-leg present.\n");
				}
				if (both) {
					switch_ivr_session_transfer(session, argv[1], argv[2], argv[3]);
				}
			} else {
				switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
			}
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

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;

			if (*argv[0] == '+') {
				when = switch_timestamp(NULL) + atol(argv[0] + 1);
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

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			time_t when;
			switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;
			switch_bool_t bleg = SWITCH_FALSE;

			if (*argv[0] == '+') {
				when = switch_timestamp(NULL) + atol(argv[0] + 1);
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

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			time_t when;
			switch_media_flag_t flags = SMF_NONE;

			if (*argv[0] == '+') {
				when = switch_timestamp(NULL) + atol(argv[0] + 1);
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
	char *mydata = NULL;

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		const char *lang;
		char *macro = mydata;
		char *mdata = NULL;

		if ((mdata = strchr(macro, ','))) {
			*mdata++ = '\0';
		}

		lang = switch_channel_get_variable(switch_core_session_get_channel(session), "language");

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Execute %s(%s) lang %s\n", macro, switch_str_nil(mdata), switch_str_nil(lang));
		switch_ivr_phrase_macro(session, macro, mdata, lang, NULL);
	}
}

SWITCH_STANDARD_APP(hangup_function)
{
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (!switch_strlen_zero(data)) {
		cause = switch_channel_str2cause(data);
	}

	switch_channel_hangup(switch_core_session_get_channel(session), cause);
}

SWITCH_STANDARD_APP(set_name_function)
{

	if (!switch_strlen_zero(data)) {
		switch_channel_set_name(switch_core_session_get_channel(session), (char *) data);
	}
}

SWITCH_STANDARD_APP(answer_function)
{
	switch_channel_answer(switch_core_session_get_channel(session));
}

SWITCH_STANDARD_APP(pre_answer_function)
{
	switch_channel_pre_answer(switch_core_session_get_channel(session));
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

SWITCH_STANDARD_APP(display_function)
{
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to redirect */
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


SWITCH_STANDARD_APP(set_function)
{
	char *var, *val = NULL;

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
		switch_channel_set_variable(switch_core_session_get_channel(session), var, val);
	}
}

SWITCH_STANDARD_APP(set_global_function)
{
	char *var, *val = NULL;

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		var = strdup(data);
		switch_assert(var);
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
	switch_caller_profile_t *caller_profile;
	char *name, *val = NULL;

	caller_profile = switch_channel_get_caller_profile(switch_core_session_get_channel(session));

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
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *exports;
	char *new_exports = NULL, *new_exports_d = NULL, *var, *val = NULL, *var_name = NULL;
	int local = 1;

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
	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
		switch_channel_set_variable(switch_core_session_get_channel(session), data, NULL);
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

		switch_log_printf(SWITCH_CHANNEL_LOG, ltype, "%s\n", log_str);
		switch_safe_free(level);
	}
}

SWITCH_STANDARD_APP(info_function)
{
	switch_event_t *event;
	char *buf;

	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(switch_core_session_get_channel(session), event);
		switch_event_serialize(event, &buf, SWITCH_FALSE);
		switch_assert(buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHANNEL_DATA:\n%s\n", buf);
		switch_event_destroy(&event);
		free(buf);
	}
}

SWITCH_STANDARD_APP(event_function)
{
	switch_event_t *event;
	char *argv[25] = { 0 };
	int argc = 0;
	char *lbuf;

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_APPLICATION) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(switch_core_session_get_channel(session), event);
		if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
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
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(switch_core_session_get_channel(session));

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No privacy mode specified.\n");
	} else {
		switch_set_flag(caller_profile, SWITCH_CPF_SCREEN);

		if (!strcasecmp(data, "no")) {
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
			switch_clear_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(data, "yes")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(data, "full")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
		} else if (!strcasecmp(data, "name")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NAME);
		} else if (!strcasecmp(data, "number")) {
			switch_set_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID privacy mode specified. Use a valid mode [no|yes|name|full|number].\n");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Privacy to %s [%d]\n", data, caller_profile->flags);
	}
}

SWITCH_STANDARD_APP(strftime_function)
{
	char *argv[2] = { 0 };
	int argc;
	char *lbuf;

	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, '=', argv, (sizeof(argv) / sizeof(argv[0])))) > 1) {
		switch_size_t retsize;
		switch_time_exp_t tm;
		char date[80] = "";

		switch_time_exp_lt(&tm, switch_timestamp_now());
		switch_strftime(date, &retsize, sizeof(date), argv[1], &tm);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET [%s]=[%s]\n", argv[0], date);
		switch_channel_set_variable(switch_core_session_get_channel(session), argv[0], date);
	}
}

SWITCH_STANDARD_API(strepoch_api_function)
{
	switch_time_t out;

	if (switch_strlen_zero(cmd)) {
		out = switch_timestamp_now();
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
		thetime = switch_timestamp_now();
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

	if (!switch_strlen_zero(cmd) && (lbuf = strdup(cmd))
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

	if (!switch_strlen_zero(cmd) && (lbuf = strdup(cmd))
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
	switch_event_t *params;
	
	if (channel) {
		switch_xml_t cxml = NULL, cfg = NULL, xml_menus = NULL, xml_menu = NULL;

		// Open the config from the xml registry
		switch_event_create(&params, SWITCH_EVENT_MESSAGE);
		switch_assert(params);
		switch_channel_event_set_data(channel, params);

		if ((cxml = switch_xml_open_cfg(ivr_cf_name, &cfg, params)) != NULL) {
			if ((xml_menus = switch_xml_child(cfg, "menus"))) {
				xml_menu = switch_xml_find_child(xml_menus, "menu", "name", (char *)data);

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
						switch_ivr_menu_execute(session, menu_stack, (char *)data, NULL);
						switch_ivr_menu_stack_free(menu_stack);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create menu\n");
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to find menu\n");
				}
			}
			switch_xml_free(cxml);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", ivr_cf_name);
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
    if (!system(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Failed to execute command: %s\n",data);
	}
}

SWITCH_STANDARD_APP(tone_detect_session_function)
{
	char *argv[6] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;

	if (switch_strlen_zero(data) || !(mydata = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID ARGS!\n");
		return;
	}

	if (argv[3]) {
		uint32_t mto;
		if (*argv[3] == '+') {
			if ((mto = atol(argv[3]+1)) > 0) {
				to = switch_timestamp(NULL) + mto;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID Timeout!\n");
			}
		} else {
			if ((to = atol(argv[3])) < switch_timestamp(NULL)) {
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
	switch_ivr_session_echo(session);
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
static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
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
			if (!strcasecmp(terminators, "none")) {
				terminators = NULL;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digit %c\n", dtmf->digit);
			
			for (p = terminators; p && *p; p++) {
				if (*p == dtmf->digit) {
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

SWITCH_STANDARD_APP(clear_speech_cache_function)
{
	switch_ivr_clear_speech_cache(session);
}

SWITCH_STANDARD_APP(speak_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char buf[10];
	char *argv[4] = { 0 };
	int argc;
	const char *engine = NULL;
	const char *voice = NULL;
	char *text = NULL;
	char *mydata = NULL;
	switch_input_args_t args = { 0 };

	if (switch_strlen_zero(data) || !(mydata = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	}

	argc = switch_separate_string(mydata, '|', argv, sizeof(argv) / sizeof(argv[0]));
	
	if (argc == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	} else if (argc == 1) {
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
			
			if (dtmf->digit == '#') {
				return SWITCH_STATUS_FALSE;
			}

			if (dtmf->digit == '0') {
				switch_caller_extension_t *extension = NULL;
				const char *app = "three_way";
				const char *app_arg = switch_core_session_get_uuid(session);
				const char *holding = switch_channel_get_variable(channel, SWITCH_HOLDING_UUID_VARIABLE);
				switch_core_session_t *b_session;

				if (holding && (b_session = switch_core_session_locate(holding))) {
					switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
					if (!switch_channel_ready(b_channel)) {
						app = "intercept";
					}
					switch_core_session_rwunlock(b_session);
				}
				
				if ((extension = switch_caller_extension_new(peer_session, app, app_arg)) == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
					abort();
				}
				
				switch_caller_extension_add_application(peer_session, extension, app, app_arg);
				switch_channel_set_caller_extension(peer_channel, extension);
				switch_channel_set_flag(peer_channel, CF_TRANSFER);
				switch_channel_set_state(peer_channel, CS_EXECUTE);

				return SWITCH_STATUS_FALSE;
			}
			
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;


}

static switch_status_t hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	const char *id = NULL;

	if (state == CS_HANGUP || state == CS_RING) {
		if ((id = switch_channel_get_variable(channel, "xfer_uuids"))) {
			switch_stream_handle_t stream = { 0 };
			SWITCH_STANDARD_STREAM(stream);
			switch_api_execute("uuid_bridge", id, NULL, &stream);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nHangup Command uuid_bridge(%s):\n%s\n", id, switch_str_nil((char *) stream.data));
			switch_safe_free(stream.data);
		}
		
		switch_core_event_hook_remove_state_change(session, hanguphook);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(att_xfer_function)
{
	const char *var;
	switch_core_session_t *peer_session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	switch_channel_t *channel, *peer_channel = NULL;
	const char *bond = NULL;
	int timelimit = 60;

	channel = switch_core_session_get_channel(session);

	if ((bond = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
		bond = switch_core_session_strdup(session, bond);
	}

	switch_channel_set_variable(channel, SWITCH_HOLDING_UUID_VARIABLE, bond);
	
	
	if ((var = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE))) {
		timelimit = atoi(var);
	}

	if (switch_ivr_originate(session, &peer_session, &cause, data, timelimit, NULL, NULL, NULL, NULL, SOF_NONE) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	peer_channel = switch_core_session_get_channel(peer_session);
	switch_channel_set_flag(peer_channel, CF_INNER_BRIDGE);
	switch_channel_set_flag(channel, CF_INNER_BRIDGE);
	
	switch_ivr_multi_threaded_bridge(session, peer_session, xfer_on_dtmf, peer_session, NULL);
	
	if (bond) {
		switch_core_session_t *b_session;
		char buf[128] = "";

		if ((b_session = switch_core_session_locate(bond))) {
			switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
			switch_snprintf(buf, sizeof(buf), "%s %s", switch_core_session_get_uuid(peer_session), switch_core_session_get_uuid(session));
			switch_channel_set_variable(b_channel, "xfer_uuids", buf);

			switch_snprintf(buf, sizeof(buf), "%s %s", switch_core_session_get_uuid(peer_session), bond);
			switch_channel_set_variable(channel, "xfer_uuids", buf);
			
			switch_core_event_hook_add_state_change(session, hanguphook);
			switch_core_event_hook_add_state_change(b_session, hanguphook);

			switch_core_session_rwunlock(b_session);
		}
		
		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, bond);
	}

	if (peer_session) {
		switch_core_session_rwunlock(peer_session);
	}
	
	switch_channel_set_variable(channel, SWITCH_HOLDING_UUID_VARIABLE, NULL);

}



SWITCH_STANDARD_APP(read_function)
{
	char *mydata;
	char *argv[6] = { 0 };
	int argc;
	int32_t min_digits = 0;
	int32_t max_digits = 0;
	int timeout = 1000;
	char digit_buffer[128] = "";
	const char *prompt_audio_file = NULL;
	const char *var_name = NULL;
	const char *valid_terminators = NULL;

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No arguements specified.\n");		
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
	
	if (min_digits <= 1) {
		min_digits = 1;
	}

	if (max_digits < min_digits) {
		max_digits = min_digits;
	}
	
	if (timeout <= 1000) {
		timeout = 1000;
	}

	if (switch_strlen_zero(valid_terminators)) {
		valid_terminators = "#";
	}

	switch_ivr_read(session, min_digits, max_digits, prompt_audio_file, var_name, digit_buffer, sizeof(digit_buffer), timeout, valid_terminators);

}


SWITCH_STANDARD_APP(playback_function)
{
	switch_input_args_t args = { 0 };

	switch_channel_pre_answer(switch_core_session_get_channel(session));

	args.input_callback = on_dtmf;
	switch_ivr_play_file(session, NULL, data, &args);
}

SWITCH_STANDARD_APP(gentones_function)
{
	char *tone_script = NULL;
	switch_input_args_t args = { 0 };
	char *l;
	int32_t loops = 0;

	if (switch_strlen_zero(data) || !(tone_script = switch_core_session_strdup(session, data))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Params!\n");
		return;
	}

	switch_channel_pre_answer(switch_core_session_get_channel(session));

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
	char *path = NULL;
	uint32_t limit = 0;
	char *argv[6] = { 0 };
	int x, argc;
	char *lbuf = NULL;
	char *flags = NULL;

	if (!switch_strlen_zero(data) && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		path = argv[0];
		for(x = 1; x < argc; x++) {
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
	switch_ivr_stop_displace_session(session, data);
}

SWITCH_STANDARD_APP(record_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	uint32_t limit = 0;
	char *path;
	switch_input_args_t args = { 0 };
	switch_file_handle_t fh = { 0 };
	int argc;
    char *mydata, *argv[4] = { 0 };
	char *l = NULL;
	const char *tmp;
	int rate;

	if (!switch_strlen_zero(data) && (mydata = switch_core_session_strdup(session, data))) {
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
		}
		if (l) {
			limit = atoi(l);
			if (limit < 0) {
				limit = 0;
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

	if ((tmp = switch_channel_get_variable(channel, "record_rate"))) {
		rate = atoi(tmp);
		if (rate > 0) {
			fh.samplerate = rate;
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
	char *p, *path = NULL;
	uint32_t limit = 0;

	if (switch_strlen_zero(data)) {
		return;
	}

	path = switch_core_session_strdup(session, data);
	if (path && (p = strchr(path, '+'))) {
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
	switch_ivr_stop_record_session(session, data);
}

/********************************************************************************/
/*								Bridge Functions								*/
/********************************************************************************/

SWITCH_STANDARD_APP(audio_bridge_function)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_core_session_t *peer_session = NULL;
	unsigned int timelimit = 60;
	const char *var, *continue_on_fail = NULL;
	uint8_t no_media_bridge = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (switch_strlen_zero(data)) {
		return;
	}

	if ((var = switch_channel_get_variable(caller_channel, SWITCH_CALL_TIMEOUT_VARIABLE))) {
		timelimit = atoi(var);
	}

	continue_on_fail = switch_channel_get_variable(caller_channel, "continue_on_fail");
	
	if ((var = switch_channel_get_variable(caller_channel, SWITCH_PROXY_MEDIA_VARIABLE)) && switch_true(var)) {
		switch_channel_set_flag(caller_channel, CF_PROXY_MEDIA);
	}
	
	if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)
		|| ((var = switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_VARIABLE)) && switch_true(var))) {
		if (!switch_channel_test_flag(caller_channel, CF_ANSWERED)
			&& !switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
			switch_channel_set_flag(caller_channel, CF_PROXY_MODE);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel is already up, delaying proxy mode 'till both legs are up.\n");
			no_media_bridge = 1;
		}
	}

	if (switch_ivr_originate(session, &peer_session, &cause, data, timelimit, NULL, NULL, NULL, NULL, SOF_NONE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", switch_channel_cause2str(cause));

		/* no answer is *always* a reason to continue */
		if (cause == SWITCH_CAUSE_NO_ANSWER || cause == SWITCH_CAUSE_NO_USER_RESPONSE) {
			return;
		}

		/* 
		   if the variable continue_on_fail is set it can be:
		   'true' to continue on all failures.
		   'false' to not continue.
		   A list of codes either names or numbers eg "user_busy,normal_temporary_failure,603"
		*/
		if (continue_on_fail) {
			const char *cause_str;
			char cause_num[35] = "";

			cause_str = switch_channel_cause2str(cause);
			switch_snprintf(cause_num, sizeof(cause_num), "%u", cause);

			if (switch_true(continue_on_fail) || switch_stristr(cause_str, continue_on_fail) || strstr(cause_num, continue_on_fail)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Continue on fail [%s]:  Cause: %s\n", continue_on_fail, cause_str);
				return;
			}

		}
		if (!switch_channel_test_flag(caller_channel, CF_TRANSFER) && switch_channel_get_state(caller_channel) != CS_RING) {
			switch_channel_hangup(caller_channel, cause);
		}
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

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Redirecting media to proxy mode.\n");
			switch_ivr_nomedia(switch_core_session_get_uuid(session), SMF_FORCE);
			switch_ivr_nomedia(switch_core_session_get_uuid(peer_session), SMF_FORCE);
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)) {
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

/* fake chan_user */
switch_endpoint_interface_t *user_endpoint_interface;
static switch_call_cause_t user_outgoing_channel(switch_core_session_t *session,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, 
												 switch_memory_pool_t **pool,
												 switch_originate_flag_t flags);
switch_io_routines_t user_io_routines = {
	/*.outgoing_channel */ user_outgoing_channel
};

static switch_call_cause_t user_outgoing_channel(switch_core_session_t *session,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, 
												 switch_memory_pool_t **pool,
												 switch_originate_flag_t flags)
{
	switch_xml_t x_domain = NULL, xml = NULL, x_user = NULL, x_param, x_params;	
	char *user = NULL, *domain = NULL;
	const char *dest = NULL;
	static switch_call_cause_t cause = SWITCH_CAUSE_UNALLOCATED;
	unsigned int timelimit = 60;
	switch_channel_t *new_channel = NULL;
	switch_event_t *params = NULL;
	if (switch_strlen_zero(outbound_profile->destination_number)) {
		goto done;
	}

	user = strdup(outbound_profile->destination_number);

	if (!(domain = strchr(user, '@'))) {
		goto done;
	}

	*domain++ = '\0';

	switch_event_create(&params, SWITCH_EVENT_MESSAGE);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "as_channel", "true");

	if (switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", user, domain);
		cause = SWITCH_CAUSE_SUBSCRIBER_ABSENT;
		goto done;
	}

	if ((x_params = switch_xml_child(x_domain, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			
			if (!strcasecmp(var, "dial-string")) {
				dest = val;
				break;
			}
		}
	}

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			
			if (!strcasecmp(var, "dial-string")) {
				dest = val;
				break;
			}
		}
	}

	if (dest) {
		const char *var;
		char *d_dest = NULL;
		switch_channel_t *channel;
		switch_originate_flag_t myflags = SOF_NONE;

		if (session) {
			channel = switch_core_session_get_channel(session);
			if ((var = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE))) {
				timelimit = atoi(var);
			}
			
			switch_channel_set_variable(channel, "dialed_user", user);
			switch_channel_set_variable(channel, "dialed_domain", domain);
			
			d_dest = switch_channel_expand_variables(channel, dest);

		} else {
			d_dest = strdup(dest);
		}
		
		if ((flags & SOF_FORKED_DIAL)) {
			myflags |= SOF_NOBLOCK;
		}

		if (switch_ivr_originate(session, new_session, &cause, d_dest, timelimit, NULL, NULL, NULL, NULL, myflags) == SWITCH_STATUS_SUCCESS) {
			const char *context;
			switch_caller_profile_t *cp;

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

	if (new_channel && xml) {
		if ((x_params = switch_xml_child(x_domain, "variables"))) {
			for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_channel_set_variable(new_channel, var, val);
			}
		}

		if ((x_params = switch_xml_child(x_user, "variables"))) {
			for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
				const char *var = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_channel_set_variable(new_channel, var, val);
			}
		}
	}

 done:

	if (xml) {
		switch_xml_free(xml);
	}
	
	if (params) {
		switch_event_destroy(&params);
	}

	switch_safe_free(user);

	return cause;
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

	user_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
    user_endpoint_interface->interface_name = "USER";
    user_endpoint_interface->io_routines = &user_io_routines;

	SWITCH_ADD_API(api_interface, "strepoch", "Convert a date string into epoch time", strepoch_api_function, "<string>");
	SWITCH_ADD_API(api_interface, "chat", "chat", chat_api_function, "<proto>|<from>|<to>|<message>");
	SWITCH_ADD_API(api_interface, "strftime", "strftime", strftime_api_function, "<format_string>");
	SWITCH_ADD_API(api_interface, "presence", "presence", presence_api_function, "<user> <rpid> <message>");
	SWITCH_ADD_APP(app_interface, "privacy", "Set privacy on calls", "Set caller privacy on calls.", privacy_function, "off|on|name|full|number", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "transfer", "Transfer a channel", TRANSFER_LONG_DESC, transfer_function, "<exten> [<dialplan> <context>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "check_acl", "Check an ip against an ACL list", 
				   "Check an ip against an ACL list", check_acl_function, "<ip> <acl | cidr>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sleep", "Pause a channel", SLEEP_LONG_DESC, sleep_function, "<pausemilliseconds>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "delay_echo", "echo audio at a specified delay", "Delay n ms", delay_function, "<delay ms>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "strftime", NULL, NULL, strftime_function, NULL, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "phrase", "Say a Phrase", "Say a Phrase", phrase_function, "<macro_name>,<data>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eval", "Do Nothing", "Do Nothing", eval_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "pre_answer", "Pre-Answer the call", "Pre-Answer the call for a channel.", pre_answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "answer", "Answer the call", "Answer the call for a channel.", answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hangup", "Hangup the call", "Hangup the call for a channel.", hangup_function, "[<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_name", "Name the channel", "Name the channel", set_name_function, "<name>", SAF_SUPPORT_NOMEDIA);
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
	SWITCH_ADD_APP(app_interface, "send_display", "Send session a new display", "Send session a new display.", display_function, "<text>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "respond", "Send session respond", "Send a respond message to a session.", respond_function, "<respond_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "deflect", "Send call deflect", "Send a call deflect.", deflect_function, "<deflect_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "reject", "Send session reject (depricated)", "Send a respond message to a session.", respond_function, "<respond_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "queue_dtmf", "Queue dtmf to be sent", "Queue dtmf to be sent from a session", queue_dtmf_function, "<dtmf_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "send_dtmf", "Send dtmf to be sent", "Send dtmf to be sent from a session", send_dtmf_function, "<dtmf_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_hangup", SCHED_HANGUP_DESCR, SCHED_HANGUP_DESCR, sched_hangup_function, "[+]<time> [<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_broadcast", SCHED_BROADCAST_DESCR, SCHED_BROADCAST_DESCR, sched_broadcast_function, "[+]<time> <path> [aleg|bleg|both]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "sched_transfer", SCHED_TRANSF_DESCR, SCHED_TRANSF_DESCR, sched_transfer_function, "[+]<time> <extension> <dialplan> <context>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "execute_extension", "Execute an extension", "Execute an extension", exe_function, EXE_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "soft_hold", "Put a bridged channel on hold", "Put a bridged channel on hold", soft_hold_function, SOFT_HOLD_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "bind_meta_app", "Bind a key to an application", "Bind a key to an application", dtmf_bind_function, BIND_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "intercept", "intercept", "intercept", intercept_function, INTERCEPT_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eavesdrop", "eavesdrop on a uuid", "eavesdrop on a uuid", eavesdrop_function, eavesdrop_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "three_way", "three way call with a uuid", "three way call with a uuid", three_way_function, eavesdrop_SYNTAX, SAF_NONE);
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
	SWITCH_ADD_APP(app_interface, "park", "Park", "Park", park_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "park_state", "Park State", "Park State", park_state_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "gentones", "Generate Tones", "Generate tones to the channel", gentones_function, "<tgml_script>[|<loops>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "playback", "Playback File", "Playback a file to the channel", playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "att_xfer", "Attended Transfer", "Attended Transfer", att_xfer_function, "<channel_url>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "read", "Read Digits", "Read Digits", read_function, "<min> <max> <file> <var name> <timeout> <terminators>", SAF_NONE);
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
