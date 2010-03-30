/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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

#define DETECT_SPEECH_SYNTAX "<mod_name> <gram_name> <gram_path> [<addr>] OR grammar <gram_name> [<path>] OR pause OR resume"
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
		} else if (!strcasecmp(argv[0], "pause")) {
			switch_ivr_pause_detect_speech(session);
		} else if (!strcasecmp(argv[0], "resume")) {
			switch_ivr_resume_detect_speech(session);
		} else if (!strcasecmp(argv[0], "stop")) {
			switch_ivr_stop_detect_speech(session);
		} else if (!strcasecmp(argv[0], "param")) {
			switch_ivr_set_param_detect_speech(session, argv[1], argv[2]);
		} else if (argc >= 3) {
			switch_ivr_detect_speech(session, argv[0], argv[1], argv[2], argv[3], NULL);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage: %s\n", DETECT_SPEECH_SYNTAX);
	}
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

#define UNBIND_SYNTAX "[<key>]"
SWITCH_STANDARD_APP(dtmf_unbind_function)
{
	char *key = (char *) data;
	int kval = 0;

	if (key) {
		kval = atoi(key);
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
		int kval = atoi(argv[0]);
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
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *require_group = switch_channel_get_variable(channel, "eavesdrop_require_group");
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

			if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Database Error!\n");
			}

			while (switch_channel_ready(channel)) {
				for (x = 0; x < MAX_SPY; x++) {
					switch_safe_free(e_data.uuid_list[x]);
				}
				e_data.total = 0;
				switch_cache_db_execute_sql_callback(db, sql, e_callback, &e_data, &errmsg);
				if (errmsg) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Error: %s\n", errmsg);
					switch_core_db_free(errmsg);
					if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_failed"))) {
						switch_ivr_play_file(session, NULL, file, NULL);
					}
					switch_ivr_collect_digits_count(session, buf, buflen, 1, "*", &terminator, 5000, 0, 0);
					continue;
				}
				if (e_data.total) {
					for (x = 0; x < e_data.total && switch_channel_ready(channel); x++) {
						/* If we have a group and 1000 concurrent calls, we will flood the logs. This check avoids this */
						if (!require_group)
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Spy: %s\n", e_data.uuid_list[x]);
						if ((file = switch_channel_get_variable(channel, "eavesdrop_indicate_new"))) {
							switch_ivr_play_file(session, NULL, file, NULL);
						}
						if ((status = switch_ivr_eavesdrop_session(session, e_data.uuid_list[x], require_group, ED_DTMF)) != SWITCH_STATUS_SUCCESS) {
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
			switch_cache_db_release_db_handle(&db);

		} else {
			switch_ivr_eavesdrop_session(session, data, require_group, ED_DTMF);
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
	switch_channel_ring_ready(switch_core_session_get_channel(session));
}

SWITCH_STANDARD_APP(remove_bugs_function)
{
	switch_core_media_bug_remove_all(session);
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
				if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
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

			if (*argv[0] == '+') {
				when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
			} else {
				when = atol(argv[0]);
			}

			switch_ivr_schedule_transfer(when, switch_core_session_get_uuid(session), argv[1], argv[2], argv[3]);
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

			if (*argv[0] == '+') {
				when = switch_epoch_time_now(NULL) + atol(argv[0] + 1);
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

			if (*argv[0] == '+') {
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

			switch_ivr_schedule_broadcast(when, switch_core_session_get_uuid(session), argv[1], flags);
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
	switch_channel_answer(channel);
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

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *expanded = NULL;

		var = switch_core_session_strdup(session, data);
		val = strchr(var, '=');

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		if (val) {
			expanded = switch_channel_expand_variables(channel, val);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SET [%s]=[%s]\n", switch_channel_get_name(channel), var,
						  expanded ? expanded : "UNDEF");
		switch_channel_set_variable_var_check(channel, var, expanded, SWITCH_FALSE);

		if (expanded && expanded != val) {
			switch_safe_free(expanded);
		}
	}
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
	const char *exports;
	char *new_exports = NULL, *new_exports_d = NULL, *var, *val = NULL, *var_name = NULL;
	int local = 1;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		exports = switch_channel_get_variable(channel, SWITCH_EXPORT_VARS_VARIABLE);
		var = switch_core_session_strdup(session, data);
		if (var) {
			val = strchr(var, '=');
			if (!strncasecmp(var, "nolocal:", 8)) {
				var_name = var + 8;
				local = 0;
			} else {
				var_name = var;
			}
		}

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "EXPORT %s[%s]=[%s]\n", local ? "" : "(REMOTE ONLY) ",
						  var_name ? var_name : "", val ? val : "UNDEF");
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
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
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

SWITCH_STANDARD_API(strftime_api_function)
{
	switch_size_t retsize;
	switch_time_exp_t tm;
	char date[80] = "";
	switch_time_t thetime;
	char *p;
	if (!zstr(cmd) && (p = strchr(cmd, '|'))) {
		thetime = switch_time_make(atoi(cmd), 0);
		cmd = p + 1;
	} else {
		thetime = switch_micro_time_now();
	}
	switch_time_exp_lt(&tm, thetime);
	if (zstr(cmd)) {
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
	} else {
		switch_strftime(date, &retsize, sizeof(date), cmd, &tm);
	}
	stream->write_function(stream, "%s", date);

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

		if (switch_core_chat_send(argv[0], "dp", argv[1], argv[2], "", argv[3], !zstr(argv[4]) ? argv[4] : NULL, "") == SWITCH_STATUS_SUCCESS) {
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

SWITCH_STANDARD_APP(tone_detect_session_function)
{
	char *argv[7] = { 0 };
	int argc;
	char *mydata = NULL;
	time_t to = 0;
	int hits = 1;

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

	if (argv[6]) {
		hits = atoi(argv[6]);
		if (hits < 0) {
			hits = 1;
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
	char *argv[4] = { 0 };
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
				switch_channel_set_flag(peer_channel, CF_TRANSFER);
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

SWITCH_STANDARD_APP(att_xfer_function)
{
	switch_core_session_t *peer_session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	switch_channel_t *channel, *peer_channel = NULL;
	const char *bond = NULL;
	switch_core_session_t *b_session = NULL;

	channel = switch_core_session_get_channel(session);

	if ((bond = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
		bond = switch_core_session_strdup(session, bond);
	}

	switch_channel_set_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, bond);

	if (switch_ivr_originate(session, &peer_session, &cause, data, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL)
		!= SWITCH_STATUS_SUCCESS || !peer_session) {
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
		goto end;
	}

	if (bond) {
		char buf[128] = "";
		int br = 0;

		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, bond);

		if (!switch_channel_down(peer_channel)) {
			if (!switch_channel_ready(channel)) {
				switch_ivr_uuid_bridge(switch_core_session_get_uuid(peer_session), bond);
				br++;
			} else if ((b_session = switch_core_session_locate(bond))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				switch_snprintf(buf, sizeof(buf), "%s %s", switch_core_session_get_uuid(peer_session), switch_core_session_get_uuid(session));
				switch_channel_set_variable(b_channel, "xfer_uuids", buf);

				switch_snprintf(buf, sizeof(buf), "%s %s", switch_core_session_get_uuid(peer_session), bond);
				switch_channel_set_variable(channel, "xfer_uuids", buf);

				switch_core_event_hook_add_state_change(session, hanguphook);
				switch_core_event_hook_add_state_change(b_session, hanguphook);

				switch_core_session_rwunlock(b_session);
			}
		}

		if (!br) {
			switch_ivr_uuid_bridge(switch_core_session_get_uuid(session), bond);
		}

	}

	switch_core_session_rwunlock(peer_session);

  end:
	switch_channel_set_variable(channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, NULL);
	switch_channel_clear_flag(channel, CF_XFER_ZOMBIE);
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

	switch_ivr_read(session, min_digits, max_digits, prompt_audio_file, var_name, digit_buffer, sizeof(digit_buffer), timeout, valid_terminators);
}

SWITCH_STANDARD_APP(play_and_get_digits_function)
{
	char *mydata;
	char *argv[9] = { 0 };
	int argc;
	int32_t min_digits = 0;
	int32_t max_digits = 0;
	int32_t max_tries = 0;
	int timeout = 1000;
	char digit_buffer[128] = "";
	const char *prompt_audio_file = NULL;
	const char *bad_input_audio_file = NULL;
	const char *var_name = NULL;
	const char *valid_terminators = NULL;
	const char *digits_regex = NULL;

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

	switch_play_and_get_digits(session, min_digits, max_digits, max_tries, timeout, valid_terminators,
							   prompt_audio_file, bad_input_audio_file, var_name, digit_buffer, sizeof(digit_buffer), digits_regex);
}

#define SAY_SYNTAX "<module_name> <say_type> <say_method> [<say_gender>] <text>"
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

	if (!zstr(data) && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
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
	const char *continue_on_fail = NULL, *failure_causes = NULL,
		*v_campon = NULL, *v_campon_retries, *v_campon_sleep, *v_campon_timeout, *v_campon_fallback_exten = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	int campon_retries = 100, campon_timeout = 10, campon_sleep = 10, tmp, camping = 0, fail = 0, thread_started = 0;
	struct camping_stake stake = { 0 };
	const char *moh = NULL;
	switch_thread_t *thread = NULL;
	switch_threadattr_t *thd_attr = NULL;
	char *camp_data = NULL;
	switch_status_t status;

	if (zstr(data)) {
		return;
	}

	continue_on_fail = switch_channel_get_variable(caller_channel, "continue_on_fail");
	failure_causes = switch_channel_get_variable(caller_channel, "failure_causes");

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

		if (!(moh = switch_channel_get_variable(caller_channel, "hold_music"))) {
			moh = switch_channel_get_variable(caller_channel, "campon_hold_music");
		}

		do {
			fail = 0;
			status = switch_ivr_originate(NULL, &peer_session, &cause, camp_data, campon_timeout, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL);

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


				if (--campon_retries <= 0 || stake.do_xfer) {
					camping = 0;
					stake.do_xfer = 1;
					break;
				}

				if (fail) {
					int64_t wait = campon_sleep * 1000000;

					while (stake.running && wait > 0 && switch_channel_ready(caller_channel)) {
						switch_yield(100000);
						wait -= 100000;
					}
				}
			}
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

		/* 
		   if the variable continue_on_fail is set it can be:
		   'true' to continue on all failures.
		   'false' to not continue.
		   A list of codes either names or numbers eg "user_busy,normal_temporary_failure,603"
		   failure_causes acts as the opposite version  
		   EXCEPTION... ATTENDED_TRANSFER never is a reason to continue.......
		 */
		if (cause != SWITCH_CAUSE_ATTENDED_TRANSFER) {
			if (continue_on_fail || failure_causes) {
				const char *cause_str;
				char cause_num[35] = "";

				cause_str = switch_channel_cause2str(cause);
				switch_snprintf(cause_num, sizeof(cause_num), "%u", cause);

				if (failure_causes) {
					char *lbuf = switch_core_session_strdup(session, failure_causes);
					char *argv[256] = { 0 };
					int argc = switch_separate_string(lbuf, ',', argv, (sizeof(argv) / sizeof(argv[0])));
					int i, x = 0;

					for (i = 0; i < argc; i++) {
						if (!strcasecmp(argv[i], cause_str) || !strcasecmp(argv[i], cause_num)) {
							x++;
							break;
						}
					}
					if (!x) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "Failure causes [%s]:  Cause: %s\n", failure_causes, cause_str);
						return;
					}
				}

				if (continue_on_fail) {
					if (switch_true(continue_on_fail)) {
						return;
					} else {
						char *lbuf = switch_core_session_strdup(session, continue_on_fail);
						char *argv[256] = { 0 };
						int argc = switch_separate_string(lbuf, ',', argv, (sizeof(argv) / sizeof(argv[0])));
						int i;

						for (i = 0; i < argc; i++) {
							if (!strcasecmp(argv[i], cause_str) || !strcasecmp(argv[i], cause_num)) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
												  "Continue on fail [%s]:  Cause: %s\n", continue_on_fail, cause_str);
								return;
							}
						}
					}
				}
			} else {
				/* no answer is *always* a reason to continue */
				if (cause == SWITCH_CAUSE_NO_ANSWER || cause == SWITCH_CAUSE_NO_USER_RESPONSE || cause == SWITCH_CAUSE_ORIGINATOR_CANCEL) {
					return;
				}
			}
		}
		if (!switch_channel_test_flag(caller_channel, CF_TRANSFER) && switch_channel_get_state(caller_channel) != CS_ROUTING) {
			switch_channel_hangup(caller_channel, cause);
		}
		return;
	} else {

		if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)) {
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
			char *a_key = (char *) switch_channel_get_variable(channel, "bridge_terminate_key");
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

			if (switch_true(switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE)) ||
				switch_true(switch_channel_get_variable(peer_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE))) {
				switch_channel_set_flag(caller_channel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
			}

			switch_ivr_multi_threaded_bridge(session, peer_session, func, a_key, b_key);
		}

		if (peer_session) {
			switch_core_session_rwunlock(peer_session);
		}
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
	char *domain = NULL;
	switch_channel_t *new_channel = NULL;
	unsigned int timelimit = 60;
	const char *skip, *var;

	group = strdup(outbound_profile->destination_number);

	if (!group)
		goto done;

	if ((domain = strchr(group, '@'))) {
		*domain++ = '\0';
	} else {
		domain = switch_core_get_variable("domain");
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
	switch_xml_t x_domain = NULL, xml = NULL, x_user = NULL, x_group = NULL, x_param, x_params;
	char *user = NULL, *domain = NULL;
	const char *dest = NULL;
	static switch_call_cause_t cause = SWITCH_CAUSE_NONE;
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
		domain = switch_core_get_variable("domain");
	}

	if (!domain) {
		goto done;
	}


	if (var_event && (skip = switch_event_get_header(var_event, "user_recurse_variables")) && switch_false(skip)) {
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		var_event = NULL;
	}



	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "as_channel", "true");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "user_call");

	if (switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, &x_group, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", user, domain);
		cause = SWITCH_CAUSE_SUBSCRIBER_ABSENT;
		goto done;
	}

	if ((x_params = switch_xml_child(x_domain, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr(x_param, "name");
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

	if ((x_params = switch_xml_child(x_group, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr(x_param, "name");
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

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr(x_param, "name");
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
				|| (varval = switch_event_get_header(var_event, "leg_timeout"))) {
				timelimit = atoi(varval);
			}

			switch_channel_set_variable(channel, "dialed_user", user);
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

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_user", user);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
			d_dest = switch_event_expand_headers(event, dest);
			switch_event_destroy(&event);
		}

		if ((flags & SOF_FORKED_DIAL)) {
			myflags |= SOF_NOBLOCK;
		}

		switch_snprintf(stupid, sizeof(stupid), "user/%s", user);
		if (switch_stristr(stupid, d_dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Waddya Daft? You almost called '%s' in an infinate loop!\n",
							  stupid);
			cause = SWITCH_CAUSE_INVALID_IE_CONTENTS;
		} else if (switch_ivr_originate(session, new_session, &cause, d_dest, timelimit, NULL,
										cid_name_override, cid_num_override, outbound_profile, var_event, myflags,
										cancel_cause) == SWITCH_STATUS_SUCCESS) {
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
				const char *pvar = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_channel_set_variable(new_channel, pvar, val);
			}
		}

		if ((x_params = switch_xml_child(x_user, "variables"))) {
			for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
				const char *pvar = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_channel_set_variable(new_channel, pvar, val);
			}
		}
	}

  done:

	switch_xml_free(xml);

	if (params) {
		switch_event_destroy(&params);
	}

	if (var_event && var_event_orig != var_event) {
		switch_event_destroy(&var_event);
	}

	switch_safe_free(user);

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

SWITCH_STANDARD_APP(verbose_events_function)
{
	switch_channel_set_flag(switch_core_session_get_channel(session), CF_VERBOSE_EVENTS);
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
			if ((timeout_ms = atoi(argv[3])) < 0) {
				timeout_ms = 0;
			}
		}

		if (thresh > 0 && silence_hits > 0 && listen_hits > 0) {
			switch_ivr_wait_for_silence(session, thresh, silence_hits, listen_hits, timeout_ms, argv[4]);
			return;
		}

	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", WAIT_FOR_SILENCE_SYNTAX);
}

static switch_status_t event_chat_send(const char *proto, const char *from, const char *to, const char *subject,
									   const char *body, const char *type, const char *hint)
{
	switch_event_t *event;

	if (switch_event_create(&event, SWITCH_EVENT_RECV_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		if (proto)
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Proto", proto);
		if (from)
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "From", from);
		if (subject)
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Subject", subject);
		if (hint)
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Hint", hint);
		if (body)
			switch_event_add_body(event, "%s", body);
		if (to) {
			const char *v;
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "To", to);
			if ((v = switch_core_get_variable(to))) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Command", v);
			}
		}

		if (switch_event_fire(&event) == SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_SUCCESS;
		}

		switch_event_destroy(&event);
	}

	return SWITCH_STATUS_MEMERR;
}

static switch_status_t api_chat_send(const char *proto, const char *from, const char *to, const char *subject,
									 const char *body, const char *type, const char *hint)
{
	if (to) {
		const char *v;
		switch_stream_handle_t stream = { 0 };
		char *cmd = NULL, *arg;

		if (!(v = switch_core_get_variable(to))) {
			v = to;
		}

		cmd = strdup(v);
		switch_assert(cmd);

		switch_url_decode(cmd);

		if ((arg = strchr(cmd, ' '))) {
			*arg++ = '\0';
		}

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg, NULL, &stream);

		if (proto) {
			switch_core_chat_send(proto, "api", to, hint && strchr(hint, '/') ? hint : from, !zstr(type) ? type : NULL, (char *) stream.data, NULL, NULL);
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
#define TRANSFER_LONG_DESC "Immediately transfer the calling channel to a new extension"
#define SLEEP_LONG_DESC "Pause the channel for a given number of milliseconds, consuming the audio for that period of time."
SWITCH_MODULE_LOAD_FUNCTION(mod_dptools_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;
	switch_chat_interface_t *chat_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	error_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	error_endpoint_interface->interface_name = "error";
	error_endpoint_interface->io_routines = &error_io_routines;

	group_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	group_endpoint_interface->interface_name = "group";
	group_endpoint_interface->io_routines = &group_io_routines;

	user_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	user_endpoint_interface->interface_name = "user";
	user_endpoint_interface->io_routines = &user_io_routines;

	SWITCH_ADD_CHAT(chat_interface, "event", event_chat_send);
	SWITCH_ADD_CHAT(chat_interface, "api", api_chat_send);

	SWITCH_ADD_API(api_interface, "strepoch", "Convert a date string into epoch time", strepoch_api_function, "<string>");
	SWITCH_ADD_API(api_interface, "chat", "chat", chat_api_function, "<proto>|<from>|<to>|<message>|[<content-type>]");
	SWITCH_ADD_API(api_interface, "strftime", "strftime", strftime_api_function, "<format_string>");
	SWITCH_ADD_API(api_interface, "presence", "presence", presence_api_function, PRESENCE_USAGE);
	SWITCH_ADD_APP(app_interface, "privacy", "Set privacy on calls", "Set caller privacy on calls.", privacy_function, "off|on|name|full|number",
				   SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "set_audio_level", "set volume", "set volume", set_audio_level_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "set_mute", "set mute", "set mute", set_mute_function, "", SAF_NONE);

	SWITCH_ADD_APP(app_interface, "flush_dtmf", "flush any queued dtmf", "flush any queued dtmf", flush_dtmf_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hold", "Send a hold message", "Send a hold message", hold_function, HOLD_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unhold", "Send a un-hold message", "Send a un-hold message", unhold_function, UNHOLD_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "transfer", "Transfer a channel", TRANSFER_LONG_DESC, transfer_function, "<exten> [<dialplan> <context>]",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "check_acl", "Check an ip against an ACL list", "Check an ip against an ACL list", check_acl_function,
				   "<ip> <acl | cidr> [<hangup_cause>]", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "verbose_events", "Make ALL Events verbose.", "Make ALL Events verbose.", verbose_events_function, "",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "early_hangup", "Enable early hangup", "", early_hangup_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "sleep", "Pause a channel", SLEEP_LONG_DESC, sleep_function, "<pausemilliseconds>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "delay_echo", "echo audio at a specified delay", "Delay n ms", delay_function, "<delay ms>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "strftime", "strftime", "strftime", strftime_function, "[<epoch>|]<format string>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "phrase", "Say a Phrase", "Say a Phrase", phrase_function, "<macro_name>,<data>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "eval", "Do Nothing", "Do Nothing", eval_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "pre_answer", "Pre-Answer the call", "Pre-Answer the call for a channel.", pre_answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "answer", "Answer the call", "Answer the call for a channel.", answer_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hangup", "Hangup the call", "Hangup the call for a channel.", hangup_function, "[<cause>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "set_name", "Name the channel", "Name the channel", set_name_function, "<name>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "presence", "Send Presence", "Send Presence.", presence_function, "<rpid> <status> [<id>]",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "log", "Logs to the logger", LOG_LONG_DESC, log_function, "<log_level> <log_string>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "info", "Display Call Info", "Display Call Info", info_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "event", "Fire an event", "Fire an event", event_function, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "sound_test", "Analyze Audio", "Analyze Audio", sound_test_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "export", "Export a channel variable across a bridge", EXPORT_LONG_DESC, export_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "set", "Set a channel variable", SET_LONG_DESC, set_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "set_global", "Set a global variable", SET_GLOBAL_LONG_DESC, set_global_function, "<varname>=<value>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "set_profile_var", "Set a caller profile variable", SET_PROFILE_VAR_LONG_DESC, set_profile_var_function,
				   "<varname>=<value>", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "unset", "Unset a channel variable", UNSET_LONG_DESC, unset_function, "<varname>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_APP(app_interface, "ring_ready", "Indicate Ring_Ready", "Indicate Ring_Ready on a channel.", ring_ready_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "remove_bugs", "Remove media bugs", "Remove all media bugs from a channel.", remove_bugs_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "break", "Break", "Set the break flag.", break_function, "", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "detect_speech", "Detect speech", "Detect speech on a channel.", detect_speech_function, DETECT_SPEECH_SYNTAX, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "ivr", "Run an ivr menu", "Run an ivr menu.", ivr_application_function, "<menu_name>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "redirect", "Send session redirect", "Send a redirect message to a session.", redirect_function, "<redirect_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "send_display", "Send session a new display", "Send session a new display.", display_function, "<text>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "respond", "Send session respond", "Send a respond message to a session.", respond_function, "<respond_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "deflect", "Send call deflect", "Send a call deflect.", deflect_function, "<deflect_data>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "queue_dtmf", "Queue dtmf to be sent", "Queue dtmf to be sent from a session", queue_dtmf_function, "<dtmf_data>",
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "send_dtmf", "Send dtmf to be sent", "Send dtmf to be sent from a session", send_dtmf_function, "<dtmf_data>",
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
	SWITCH_ADD_APP(app_interface, "mkdir", "Create a directory", "Create a directory", mkdir_function, MKDIR_SYNTAX, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "soft_hold", "Put a bridged channel on hold", "Put a bridged channel on hold", soft_hold_function, SOFT_HOLD_SYNTAX,
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "bind_meta_app", "Bind a key to an application", "Bind a key to an application", dtmf_bind_function, BIND_SYNTAX,
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "unbind_meta_app", "Unbind a key from an application", "Unbind a key from an application", dtmf_unbind_function,
				   UNBIND_SYNTAX, SAF_SUPPORT_NOMEDIA);
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
	SWITCH_ADD_APP(app_interface, "park", "Park", "Park", park_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "park_state", "Park State", "Park State", park_state_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "gentones", "Generate Tones", "Generate tones to the channel", gentones_function, "<tgml_script>[|<loops>]", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "playback", "Playback File", "Playback a file to the channel", playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "endless_playback", "Playback File Endlessly", "Endlessly Playback a file to the channel",
				   endless_playback_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "att_xfer", "Attended Transfer", "Attended Transfer", att_xfer_function, "<channel_url>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "read", "Read Digits", "Read Digits", read_function, "<min> <max> <file> <var_name> <timeout> <terminators>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "play_and_get_digits", "Play and get Digits", "Play and get Digits",
				   play_and_get_digits_function, "<min> <max> <tries> <timeout> <terminators> <file> <invalid_file> <var_name> <regexp>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stop_record_session", "Stop Record Session", STOP_SESS_REC_DESC, stop_record_session_function, "<path>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_session", "Record Session", SESS_REC_DESC, record_session_function, "<path> [+<timeout>]", SAF_MEDIA_TAP);
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
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "say", "say", "say", say_function, SAY_SYNTAX, SAF_NONE);

	SWITCH_ADD_APP(app_interface, "wait_for_silence", "wait_for_silence", "wait_for_silence", wait_for_silence_function, WAIT_FOR_SILENCE_SYNTAX,
				   SAF_NONE);
	SWITCH_ADD_APP(app_interface, "session_loglevel", "session_loglevel", "session_loglevel", session_loglevel_function, SESSION_LOGLEVEL_SYNTAX,
				   SAF_SUPPORT_NOMEDIA);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
