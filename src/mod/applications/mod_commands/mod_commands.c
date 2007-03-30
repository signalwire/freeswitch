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
 * Michael Jerris <mike@jerris.com>
 * Johny Kadarisman <jkr888@gmail.com>
 * Paul Tinsley <jackhammer@gmail.com>
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>
#include <switch_version.h>


static const char modname[] = "mod_commands";
static switch_api_interface_t ctl_api_interface;
static switch_api_interface_t uuid_bridge_api_interface;
static switch_api_interface_t session_record_api_interface;
static switch_api_interface_t status_api_interface;
static switch_api_interface_t show_api_interface;
static switch_api_interface_t pause_api_interface;
static switch_api_interface_t transfer_api_interface;
static switch_api_interface_t load_api_interface;
static switch_api_interface_t reload_api_interface;
static switch_api_interface_t kill_api_interface;
static switch_api_interface_t originate_api_interface;
static switch_api_interface_t media_api_interface;
static switch_api_interface_t hold_api_interface;
static switch_api_interface_t broadcast_api_interface;
static switch_api_interface_t sched_broadcast_api_interface;
static switch_api_interface_t sched_transfer_api_interface;
static switch_api_interface_t sched_hangup_api_interface;

static switch_status_t status_function(char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	uint8_t html = 0;
	switch_core_time_duration_t duration;
	char *http = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_measure_time(switch_core_uptime(), &duration);

	if (stream->event) {
		http = switch_event_get_header(stream->event, "http-host");
	}

	if (http || (cmd && strstr(cmd, "html"))) {
		html = 1;
		stream->write_function(stream, "<h1>FreeSWITCH Status</h1>\n<b>");
	}

	stream->write_function(stream,
						   "UP %u year%s, %u day%s, %u hour%s, %u minute%s, %u second%s, %u millisecond%s, %u microsecond%s\n",
						   duration.yr, duration.yr == 1 ? "" : "s", duration.day, duration.day == 1 ? "" : "s",
						   duration.hr, duration.hr == 1 ? "" : "s", duration.min, duration.min == 1 ? "" : "s",
						   duration.sec, duration.sec == 1 ? "" : "s", duration.ms, duration.ms == 1 ? "" : "s", duration.mms,
						   duration.mms == 1 ? "" : "s");

	stream->write_function(stream, "%d sessions\n", switch_core_session_count());

	if (html) {
		stream->write_function(stream, "</b>\n");
	}

	if (cmd && strstr(cmd, "refresh=")) {
		char *refresh = strchr(cmd, '=');
		if (refresh) {
			int r;
			refresh++;
			r = atoi(refresh);
			if (r > 0) {
				stream->write_function(stream, "<META HTTP-EQUIV=REFRESH CONTENT=\"%d; URL=/api/status?refresh=%d%s\">\n", r, r, html ? "html=1" : "");
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ctl_function(char *data, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	int argc;
	char *mydata, *argv[5];
	uint32_t arg = 0;

	if (switch_strlen_zero(data)) {
		stream->write_function(stream, "USAGE: %s\n", ctl_api_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((mydata = strdup(data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if (!strcmp(argv[0], "hupall")) {
			arg = 1;
			switch_core_session_ctl(SCSC_HUPALL, &arg);
		} else if (!strcmp(argv[0], "pause")) {
			arg = 1;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
		} else if (!strcmp(argv[0], "resume")) {
			arg = 0;
			switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
		} else if (!strcmp(argv[0], "shutdown")) {
			arg = 0;
			switch_core_session_ctl(SCSC_SHUTDOWN, &arg);
		} else {
			stream->write_function(stream, "INVALID COMMAND [%s]\nUSAGE: fsctl [hupall|pause|resume|shutdown]\n", argv[0]);
			goto end;
		}

		stream->write_function(stream, "OK\n");
	  end:
		free(mydata);
	} else {
		stream->write_function(stream, "MEM ERR\n");
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t load_function(char *mod, switch_core_session_t *session, switch_stream_handle_t *stream)
{

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(mod)) {
		stream->write_function(stream, "USAGE: %s\n", load_api_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) mod, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "ERROR\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t reload_function(char *args, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	const char *err;
	switch_xml_t xml_root;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_xml_free(xml_root);
	}

	stream->write_function(stream, "OK [%s]\n", err);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kill_function(char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	if (!dest) {
		stream->write_function(stream, "USAGE: %s\n", kill_api_interface.syntax);
	} else if ((session = switch_core_session_locate(dest))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "No Such Channel!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t transfer_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = { 0 };
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 4) {
		stream->write_function(stream, "USAGE: %s\n", transfer_api_interface.syntax);
	} else {
		char *uuid = argv[0];
		char *dest = argv[1];
		char *dp = argv[2];
		char *context = argv[3];

		if ((session = switch_core_session_locate(uuid))) {

			if (switch_ivr_session_transfer(session, dest, dp, context) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "OK\n");
			} else {
				stream->write_function(stream, "ERROR\n");
			}

			switch_core_session_rwunlock(session);

		} else {
			stream->write_function(stream, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sched_transfer_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[6] = { 0 };
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 5) {
		stream->write_function(stream, "USAGE: %s\n", sched_transfer_api_interface.syntax);
	} else {
		char *uuid = argv[1];
		char *dest = argv[2];
		char *dp = argv[3];
		char *context = argv[4];
		time_t when;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if ((session = switch_core_session_locate(uuid))) {
			switch_ivr_schedule_transfer(when, uuid, dest, dp, context);
			stream->write_function(stream, "OK\n");
			switch_core_session_rwunlock(session);
		} else {
			stream->write_function(stream, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sched_hangup_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = { 0 };
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 1) {
		stream->write_function(stream, "USAGE: %s\n", sched_hangup_api_interface.syntax);
	} else {
		char *uuid = argv[1];
		char *cause_str = argv[2];
		time_t when;
		switch_call_cause_t cause = SWITCH_CAUSE_ALLOTTED_TIMEOUT;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if (cause_str) {
			cause = switch_channel_str2cause(cause_str);
		}

		if ((session = switch_core_session_locate(uuid))) {
			switch_ivr_schedule_hangup(when, uuid, cause, SWITCH_FALSE);
			stream->write_function(stream, "OK\n");
			switch_core_session_rwunlock(session);
		} else {
			stream->write_function(stream, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t uuid_media_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (isession) {
		return status;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 1) {
		stream->write_function(stream, "USAGE: %s\n", media_api_interface.syntax);
	} else {
		if (!strcmp(argv[0], "off")) {
			status = switch_ivr_nomedia(argv[1], SMF_REBRIDGE);
		} else {
			status = switch_ivr_media(argv[0], SMF_REBRIDGE);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t uuid_broadcast_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (isession) {
		return status;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 2) {
		stream->write_function(stream, "USAGE: %s\n", broadcast_api_interface.syntax);
	} else {
		switch_media_flag_t flags = SMF_NONE;

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

		status = switch_ivr_broadcast(argv[0], argv[1], flags);
		stream->write_function(stream, "+OK Message Sent\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sched_broadcast_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (isession) {
		return status;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 3) {
		stream->write_function(stream, "USAGE: %s\n", sched_broadcast_api_interface.syntax);
	} else {
		switch_media_flag_t flags = SMF_NONE;
		time_t when;

		if (*argv[0] == '+') {
			when = time(NULL) + atol(argv[0] + 1);
		} else {
			when = atol(argv[0]);
		}

		if (argv[3]) {
			if (!strcmp(argv[3], "both")) {
				flags |= (SMF_ECHO_ALEG | SMF_ECHO_BLEG);
			} else if (!strcmp(argv[3], "aleg")) {
				flags |= SMF_ECHO_ALEG;
			} else if (!strcmp(argv[3], "bleg")) {
				flags |= SMF_ECHO_BLEG;
			}
		} else {
			flags |= SMF_ECHO_ALEG;
		}

		status = switch_ivr_schedule_broadcast(when, argv[1], argv[2], flags);
		stream->write_function(stream, "+OK Message Scheduled\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t uuid_hold_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (isession) {
		return status;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 1) {
		stream->write_function(stream, "USAGE: %s\n", hold_api_interface.syntax);
	} else {
		if (!strcmp(argv[0], "off")) {
			status = switch_ivr_unhold_uuid(argv[1]);
		} else {
			status = switch_ivr_hold_uuid(argv[0]);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t uuid_bridge_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = { 0 };
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc != 2) {
		stream->write_function(stream, "USAGE: %s\n", uuid_bridge_api_interface.syntax);
	} else {
		if (switch_ivr_uuid_bridge(argv[0], argv[1]) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "Invalid uuid\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t session_record_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = { 0 };
	char *uuid = NULL, *action = NULL, *path = NULL;
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

	if ((argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) != 3) {
		goto usage;
	}

	uuid = argv[0];
	action = argv[1];
	path = argv[2];

	if (!(session = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-Error Cannot locate session!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_strlen_zero(action) || switch_strlen_zero(path)) {
		goto usage;
	}

	if (!strcasecmp(action, "start")) {
		switch_ivr_record_session(session, path, NULL);
	} else if (!strcasecmp(action, "stop")) {
		switch_ivr_stop_record_session(session, path);
	} else {
		goto usage;
	}

	goto done;

  usage:

	stream->write_function(stream, "USAGE: %s\n", session_record_api_interface.syntax);
	return SWITCH_STATUS_SUCCESS;

  done:

	if (session) {
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t pause_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = { 0 };
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 2) {
		stream->write_function(stream, "USAGE: %s\n", pause_api_interface.syntax);
	} else {
		char *uuid = argv[0];
		char *dest = argv[1];

		if ((session = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(session);

			if (!strcasecmp(dest, "on")) {
				switch_channel_set_flag(channel, CF_HOLD);
			} else {
				switch_channel_clear_flag(channel, CF_HOLD);
			}

			switch_core_session_rwunlock(session);

		} else {
			stream->write_function(stream, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t originate_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_channel_t *caller_channel;
	switch_core_session_t *caller_session;
	char *argv[7] = { 0 };
	int i = 0, x, argc = 0;
	char *aleg, *exten, *dp, *context, *cid_name, *cid_num;
	uint32_t timeout = 60;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	uint8_t machine = 1;

	if (isession) {
		stream->write_function(stream, "Illegal Usage\n");
		return SWITCH_STATUS_SUCCESS;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 7) {
		stream->write_function(stream, "USAGE: %s\n", originate_api_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	for (x = 0; x < argc; x++) {
		if (!strcasecmp(argv[x], "undef")) {
			argv[x] = NULL;
		}
	}

	if (!strcasecmp(argv[0], "machine")) {
		machine = 1;
		i++;
	}

	aleg = argv[i++];
	exten = argv[i++];
	dp = argv[i++];
	context = argv[i++];
	cid_name = argv[i++];
	cid_num = argv[i++];

	if (!dp) {
		dp = "XML";
	}

	if (!context) {
		context = "default";
	}

	if (argv[6]) {
		timeout = atoi(argv[6]);
	}

	if (switch_ivr_originate(NULL, &caller_session, &cause, aleg, timeout, NULL, cid_name, cid_num, NULL) != SWITCH_STATUS_SUCCESS) {
		if (machine) {
			stream->write_function(stream, "fail: %s\n", switch_channel_cause2str(cause));
		} else {
			stream->write_function(stream, "Cannot Create Outgoing Channel! [%s] cause: %s\n", aleg, switch_channel_cause2str(cause));
		}
		return SWITCH_STATUS_SUCCESS;
	}

	caller_channel = switch_core_session_get_channel(caller_session);
	assert(caller_channel != NULL);
	switch_channel_clear_state_handler(caller_channel, NULL);

	if (*exten == '&' && *(exten + 1)) {
		switch_caller_extension_t *extension = NULL;
		char *app_name = switch_core_session_strdup(caller_session, (exten + 1));
		char *arg = NULL, *e;

		if ((e = strchr(app_name, ')'))) {
			*e = '\0';
		}

		if ((arg = strchr(app_name, '('))) {
			*arg++ = '\0';
		}

		if ((extension = switch_caller_extension_new(caller_session, app_name, arg)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
			switch_channel_hangup(caller_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_MEMERR;
		}
		switch_caller_extension_add_application(caller_session, extension, app_name, arg);
		switch_channel_set_caller_extension(caller_channel, extension);
		switch_channel_set_state(caller_channel, CS_EXECUTE);
	} else {
		switch_ivr_session_transfer(caller_session, exten, dp, context);
	}

	if (machine) {
		stream->write_function(stream, "success: %s\n", switch_core_session_get_uuid(caller_session));
	} else {
		stream->write_function(stream, "Created Session: %s\n", switch_core_session_get_uuid(caller_session));
	}

	return SWITCH_STATUS_SUCCESS;;
}

static void sch_api_callback(switch_scheduler_task_t *task)
{
	char *cmd, *arg = NULL;
	switch_stream_handle_t stream = { 0 };

	assert(task);

	cmd = (char *) task->cmd_arg;

	if ((arg = strchr(cmd, ' '))) {
		*arg++ = '\0';
	}

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(cmd, arg, NULL, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Command %s(%s):\n%s\n", cmd, arg, switch_str_nil((char *) stream.data));
	switch_safe_free(stream.data);
}

static switch_status_t sched_del_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	uint32_t cnt = 0;
	
	if (switch_is_digit_string(cmd)) {
		int64_t tmp;
		tmp = (uint32_t) atoi(cmd);
		if (tmp > 0) {
			cnt = switch_scheduler_del_task_id((uint32_t)tmp);
		}
	} else {
		cnt = switch_scheduler_del_task_group(cmd);
	}

	stream->write_function(stream, "DELETED: %u\n", cnt);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sched_api_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *tm = NULL, *dcmd, *group;
	time_t when;

	assert(cmd != NULL);
	tm = strdup(cmd);
	assert(tm != NULL);

	if ((group = strchr(tm, ' '))) {
		uint32_t id;

		*group++ = '\0';

		if ((dcmd = strchr(group, ' '))) {
			*dcmd++ = '\0';

			if (*tm == '+') {
				when = time(NULL) + atol(tm + 1);
			} else {
				when = atol(tm);
			}
		
			id = switch_scheduler_add_task(when, sch_api_callback, (char *) __SWITCH_FUNC__, group, 0, strdup(dcmd), SSHF_FREE_ARG);
			stream->write_function(stream, "ADDED: %u\n", id);
			goto good;
		} 
	}

	stream->write_function(stream, "Invalid syntax\n");

 good:

	switch_safe_free(tm);

	return SWITCH_STATUS_SUCCESS;
}


struct holder {
	switch_stream_handle_t *stream;
	char *http;
	uint32_t count;
	int print_title;
};

static int show_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	int x;


	if (holder->print_title && holder->count == 0) {
		if (holder->http) {
			holder->stream->write_function(holder->stream, "\n<tr>");
		}

		for (x = 0; x < argc; x++) {
			if (holder->http) {
				holder->stream->write_function(holder->stream, "<td>");
				holder->stream->write_function(holder->stream, "<b>%s</b>%s", columnNames[x], x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
			} else {
				holder->stream->write_function(holder->stream, "%s%s", columnNames[x], x == (argc - 1) ? "\n" : ",");
			}
		}
	}

	if (holder->http) {
		holder->stream->write_function(holder->stream, "<tr bgcolor=%s>", holder->count % 2 == 0 ? "eeeeee" : "ffffff");
	}

	for (x = 0; x < argc; x++) {
		if (holder->http) {
			holder->stream->write_function(holder->stream, "<td>");
			holder->stream->write_function(holder->stream, "%s%s", argv[x] ? argv[x] : "", x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
		} else {
			holder->stream->write_function(holder->stream, "%s%s", argv[x] ? argv[x] : "", x == (argc - 1) ? "\n" : ",");
		}
	}

	holder->count++;
	return 0;
}

static switch_status_t show_function(char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char sql[1024];
	char *errmsg;
	switch_core_db_t *db = switch_core_db_handle();
	struct holder holder = { 0 };
	int help = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (stream->event) {
		holder.http = switch_event_get_header(stream->event, "http-host");
	}

	holder.print_title = 1;

	// If you changes the field qty or order of any of these select
	// statmements, you must also change show_callback and friends to match!
	if (!cmd) {
		stream->write_function(stream, "USAGE: %s\n", show_api_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	} else if (!strcmp(cmd, "codec") || !strcmp(cmd, "dialplan") || !strcmp(cmd, "file") || !strcmp(cmd, "timer")) {
		sprintf(sql, "select type, name from interfaces where type = '%s'", cmd);
	} else if (!strcmp(cmd, "tasks")) {
		sprintf(sql, "select * from %s", cmd);
	} else if (!strcmp(cmd, "application") || !strcmp(cmd, "api")) {
		sprintf(sql, "select name, description, syntax from interfaces where type = '%s' and description != ''", cmd);
	} else if (!strcmp(cmd, "calls")) {
		sprintf(sql, "select * from calls");
	} else if (!strcmp(cmd, "channels")) {
		sprintf(sql, "select * from channels");
	} else if (!strncasecmp(cmd, "help", 4)) {
		char *cmdname = NULL;

		help = 1;
		holder.print_title = 0;
		if ((cmdname = strchr(cmd, ' ')) != 0) {
			*cmdname++ = '\0';
			snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api' and name = '%s'", cmdname);
		} else {
			snprintf(sql, sizeof(sql) - 1, "select name, syntax, description from interfaces where type = 'api'");
		}
	} else {
		stream->write_function(stream, "USAGE: %s\n", show_api_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	holder.stream = stream;
	holder.count = 0;

	if (holder.http) {
		holder.stream->write_function(holder.stream, "<table cellpadding=1 cellspacing=4 border=1>\n");
	}

	switch_core_db_exec(db, sql, show_callback, &holder, &errmsg);

	if (holder.http) {
		holder.stream->write_function(holder.stream, "</table>");
	}

	if (errmsg) {
		stream->write_function(stream, "SQL ERR [%s]\n", errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	} else if (help) {
		if (holder.count == 0)
			stream->write_function(stream, "No such command.\n");
	} else {
		stream->write_function(stream, "\n%u total.\n", holder.count);
	}

	switch_core_db_close(db);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t version_function(char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char version_string[1024];
	snprintf(version_string, sizeof(version_string) - 1, "FreeSwitch Version %s\n", SWITCH_VERSION_FULL);

	stream->write_function(stream, version_string);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t help_function(char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char showcmd[1024];
	int all = 0;
	if (switch_strlen_zero(cmd)) {
		sprintf(showcmd, "help");
		all = 1;
	} else {
		snprintf(showcmd, sizeof(showcmd) - 1, "help %s", cmd);
	}

	if (all) {
		stream->write_function(stream, "\nValid Commands:\n\n");
	}

	show_function(showcmd, session, stream);

	return SWITCH_STATUS_SUCCESS;
}

static switch_api_interface_t sched_del_api_interface = {
	/*.interface_name */ "sched_del",
	/*.desc */ "Delete a Scheduled task",
	/*.function */ sched_del_function,
	/*.syntax */ "<task_id>|<group_id>",
	/*.next */ NULL
};

static switch_api_interface_t sched_api_api_interface = {
	/*.interface_name */ "sched_api",
	/*.desc */ "Schedule an api command",
	/*.function */ sched_api_function,
	/*.syntax */ "[+]<time> <group_name> <command_string>",
	/*.next */ &sched_del_api_interface
};

static switch_api_interface_t sched_transfer_api_interface = {
	/*.interface_name */ "sched_transfer",
	/*.desc */ "Schedule a broadcast event to a running call",
	/*.function */ sched_transfer_function,
	/*.syntax */ "[+]<time> <uuid> <extension> [<dialplan>] [<context>]",
	/*.next */ &sched_api_api_interface
};

static switch_api_interface_t sched_broadcast_api_interface = {
	/*.interface_name */ "sched_broadcast",
	/*.desc */ "Schedule a broadcast event to a running call",
	/*.function */ sched_broadcast_function,
	/*.syntax */ "[+]<time> <uuid> <path> [aleg|bleg|both]",
	/*.next */ &sched_transfer_api_interface
};

static switch_api_interface_t sched_hangup_api_interface = {
	/*.interface_name */ "sched_hangup",
	/*.desc */ "Schedule a running call to hangup",
	/*.function */ sched_hangup_function,
	/*.syntax */ "[+]<time> <uuid> [<cause>]",
	/*.next */ &sched_broadcast_api_interface
};

static switch_api_interface_t version_api_interface = {
	/*.interface_name */ "version",
	/*.desc */ "Show version of the switch",
	/*.function */ version_function,
	/*.syntax */ "",
	/*.next */ &sched_hangup_api_interface
};

static switch_api_interface_t help_api_interface = {
	/*.interface_name */ "help",
	/*.desc */ "Show help for all the api commands",
	/*.function */ help_function,
	/*.syntax */ "",
	/*.next */ &version_api_interface,
};

static switch_api_interface_t ctl_api_interface = {
	/*.interface_name */ "fsctl",
	/*.desc */ "control messages",
	/*.function */ ctl_function,
	/*.syntax */ "[hupall|pause|resume|shutdown]",
	/*.next */ &help_api_interface
};

static switch_api_interface_t media_api_interface = {
	/*.interface_name */ "media",
	/*.desc */ "media",
	/*.function */ uuid_media_function,
	/*.syntax */ "<uuid>",
	/*.next */ &ctl_api_interface
};

static switch_api_interface_t hold_api_interface = {
	/*.interface_name */ "hold",
	/*.desc */ "hold",
	/*.function */ uuid_hold_function,
	/*.syntax */ "<uuid>",
	/*.next */ &media_api_interface
};

static switch_api_interface_t broadcast_api_interface = {
	/*.interface_name */ "broadcast",
	/*.desc */ "broadcast",
	/*.function */ uuid_broadcast_function,
	/*.syntax */ "<uuid> <path> [aleg|bleg|both]",
	/*.next */ &hold_api_interface
};

static switch_api_interface_t session_record_api_interface = {
	/*.interface_name */ "session_record",
	/*.desc */ "session record",
	/*.function */ session_record_function,
	/*.syntax */ "<uuid> [start|stop] <path>",
	/*.next */ &broadcast_api_interface
};

static switch_api_interface_t uuid_bridge_api_interface = {
	/*.interface_name */ "uuid_bridge",
	/*.desc */ "uuid_bridge",
	/*.function */ uuid_bridge_function,
	/*.syntax */ "<uuid> <other_uuid>",
	/*.next */ &session_record_api_interface
};

static switch_api_interface_t status_api_interface = {
	/*.interface_name */ "status",
	/*.desc */ "status",
	/*.function */ status_function,
	/*.syntax */ "",
	/*.next */ &uuid_bridge_api_interface
};

static switch_api_interface_t show_api_interface = {
	/*.interface_name */ "show",
	/*.desc */ "Show",
	/*.function */ show_function,
	/*.syntax */ "codec|application|api|dialplan|file|timer|calls|channels",
	/*.next */ &status_api_interface
};

static switch_api_interface_t pause_api_interface = {
	/*.interface_name */ "pause",
	/*.desc */ "Pause",
	/*.function */ pause_function,
	/*.syntax */ "<uuid> <on|off>",
	/*.next */ &show_api_interface
};

static switch_api_interface_t transfer_api_interface = {
	/*.interface_name */ "transfer",
	/*.desc */ "Transfer",
	/*.function */ transfer_function,
	/*.syntax */ "<uuid> <dest-exten> [<dialplan>] [<context>]",
	/*.next */ &pause_api_interface
};

static switch_api_interface_t load_api_interface = {
	/*.interface_name */ "load",
	/*.desc */ "Load Module",
	/*.function */ load_function,
	/*.syntax */ "<mod_name>",
	/*.next */ &transfer_api_interface
};

static switch_api_interface_t reload_api_interface = {
	/*.interface_name */ "reloadxml",
	/*.desc */ "Reload XML",
	/*.function */ reload_function,
	/*.syntax */ "",
	/*.next */ &load_api_interface,

};

static switch_api_interface_t kill_api_interface = {
	/*.interface_name */ "killchan",
	/*.desc */ "Kill Channel",
	/*.function */ kill_function,
	/*.syntax */ "<uuid>",
	/*.next */ &reload_api_interface
};

static switch_api_interface_t originate_api_interface = {
	/*.interface_name */ "originate",
	/*.desc */ "Originate a Call",
	/*.function */ originate_function,
	/*.syntax */
	"<call url> <exten>|&<application_name>(<app_args>) [<dialplan>] [<context>] [<cid_name>] [<cid_num>] [<timeout_sec>]",
	/*.next */ &kill_api_interface
};

static const switch_loadable_module_interface_t mod_commands_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ &originate_api_interface
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_commands_module_interface;

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
