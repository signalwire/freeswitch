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
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_commands";

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

	stream->write_function(stream, "UP %u year%s, %u day%s, %u hour%s, %u minute%s, %u second%s, %u millisecond%s, %u microsecond%s\n",
						   duration.yr, duration.yr == 1 ? "" : "s",
						   duration.day, duration.day == 1 ? "" : "s",
						   duration.hr, duration.hr == 1 ? "" : "s",
						   duration.min, duration.min == 1 ? "" : "s",
						   duration.sec, duration.sec == 1 ? "" : "s",
						   duration.ms, duration.ms == 1 ? "" : "s",
						   duration.mms, duration.mms == 1 ? "" : "s"
						   );

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
		stream->write_function(stream, "USAGE: fsctl [hupall|pause|resume|shutdown]\n");
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
		stream->write_function(stream, "USAGE: load <mod_name>\n");
		return SWITCH_STATUS_SUCCESS;
	}
	switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) mod);
	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t reload_function(char *mod, switch_core_session_t *session, switch_stream_handle_t *stream)
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
		stream->write_function(stream, "USAGE: killchan <uuid>\n");
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
	char *argv[4] = {0};
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}
	
	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2 || argc > 4) {
		stream->write_function(stream, "USAGE: transfer <uuid> <dest-exten> [<dialplan>] [<context>]\n");
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



static switch_status_t uuid_bridge_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	char *argv[4] = {0};
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}
	
	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc != 2) {
		stream->write_function(stream, "Invalid Parameters\nUSAGE: uuid_bridge <uuid> <other_uuid>\n");
	} else {
		if (switch_ivr_uuid_bridge(argv[0], argv[1]) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "Invalid uuid\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t pause_function(char *cmd, switch_core_session_t *isession, switch_stream_handle_t *stream)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = {0};
	int argc = 0;

	if (isession) {
		return SWITCH_STATUS_FALSE;
	}
	
	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		stream->write_function(stream, "USAGE: pause <uuid> <on|off>\n");
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
	char *argv[7] = {0};
	int x, argc = 0;
	char *aleg, *exten, *dp, *context, *cid_name, *cid_num;
	uint32_t timeout = 60;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	if (isession) {
		stream->write_function(stream, "Illegal Usage\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_strlen_zero(cmd) || argc < 2 || argc > 7) {
		stream->write_function(stream, "USAGE: originate <call url> <exten>|&<application_name>(<app_args>) [<dialplan>] [<context>] [<cid_name>] [<cid_num>] [<timeout_sec>]\n");
		return SWITCH_STATUS_SUCCESS;
	}

	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	for(x = 0; x < argc; x++) {
		if (!strcasecmp(argv[x], "undef")) {
			argv[x] = NULL;
		}
	}

	aleg = argv[0];
	exten = argv[1];
	dp = argv[2];
	context = argv[3];
	cid_name = argv[4];
	cid_num = argv[5];
	
	if (!dp) {
		dp = "XML";
	}

	if (!context) {
		context = "default";
	}

	if (argv[6]) {
		timeout = atoi(argv[6]);
	}

	if (!aleg || !exten) {
		stream->write_function(stream, "Invalid Arguements\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_ivr_originate(NULL, &caller_session, &cause, aleg, timeout, &noop_state_handler, cid_name, cid_num, NULL) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "Cannot Create Outgoing Channel! [%s]\n", aleg);
		return SWITCH_STATUS_SUCCESS;
	} 

	caller_channel = switch_core_session_get_channel(caller_session);
	assert(caller_channel != NULL);
	switch_channel_clear_state_handler(caller_channel, NULL);


	if (*exten == '&') {
		switch_caller_extension_t *extension = NULL;
		char *app_name = switch_core_session_strdup(caller_session, (exten + 1));
		char *arg, *e;

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
	stream->write_function(stream, "Created Session: %s\n", switch_core_session_get_uuid(caller_session));
	return SWITCH_STATUS_SUCCESS;;
}

struct holder {
	switch_stream_handle_t *stream;
	char *http;
	uint32_t count;
};

static int show_callback(void *pArg, int argc, char **argv, char **columnNames){
	struct holder *holder = (struct holder *) pArg;
	int x;


	if (holder->count == 0) {
		if (holder->http) {
			holder->stream->write_function(holder->stream, "\n<tr>");
		}

		for(x = 0; x < argc; x++) {
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

	for(x = 0; x < argc; x++) {
		if (holder->http) {
			holder->stream->write_function(holder->stream, "<td>");
			holder->stream->write_function(holder->stream, "%s%s", argv[x], x == (argc - 1) ? "</td></tr>\n" : "</td><td>");
		} else {
			holder->stream->write_function(holder->stream, "%s%s", argv[x], x == (argc - 1) ? "\n" : ",");
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
	struct holder holder = {0};

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (stream->event) {
        holder.http = switch_event_get_header(stream->event, "http-host");
    } 

    if (!cmd) {
        sprintf (sql, "select * from interfaces");
    }
    else if ( !strcmp(cmd,"codec") || !strcmp(cmd,"application") || 
              !strcmp(cmd,"api") || !strcmp(cmd,"dialplan") || 
              !strcmp(cmd,"file") || !strcmp(cmd,"timer") 
            ) {
        sprintf (sql, "select * from interfaces where type = '%s'", cmd);
    }
    else if ( !strcmp(cmd,"calls")) {
        sprintf (sql, "select * from calls");
    }
    else if ( !strcmp(cmd,"channels")) {
        sprintf (sql, "select * from channels");
    }
    else {
        stream->write_function(stream, "Invalid interfaces type!\n");
        stream->write_function(stream, "USAGE:\n");
        stream->write_function(stream, "show <blank>|codec|application|api|dialplan|file|timer|calls|channels\n");
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
		stream->write_function(stream, "SQL ERR [%s]\n",errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	} else {
		stream->write_function(stream, "\n%u total.\n", holder.count);
	}

	switch_core_db_close(db);
	return SWITCH_STATUS_SUCCESS;
}



static switch_api_interface_t ctl_api_interface = {
	/*.interface_name */ "fsctl",
	/*.desc */ "control messages",
	/*.function */ ctl_function,
	/*.syntax */ NULL,
	/*.next */ 
};

static switch_api_interface_t uuid_bridge_api_interface = {
	/*.interface_name */ "uuid_bridge",
	/*.desc */ "uuid_bridge",
	/*.function */ uuid_bridge_function,
	/*.syntax */ NULL,
	/*.next */ &ctl_api_interface
};

static switch_api_interface_t status_api_interface = {
	/*.interface_name */ "status",
	/*.desc */ "status",
	/*.function */ status_function,
	/*.syntax */ NULL,
	/*.next */ &uuid_bridge_api_interface
};

static switch_api_interface_t show_api_interface = {
	/*.interface_name */ "show",
	/*.desc */ "Show",
	/*.function */ show_function,
	/*.syntax */ NULL,
	/*.next */ &status_api_interface
};

static switch_api_interface_t pause_api_interface = {
	/*.interface_name */ "pause",
	/*.desc */ "Pause",
	/*.function */ pause_function,
	/*.syntax */ NULL,
	/*.next */ &show_api_interface
};

static switch_api_interface_t transfer_api_interface = {
	/*.interface_name */ "transfer",
	/*.desc */ "Transfer",
	/*.function */ transfer_function,
	/*.syntax */ NULL,
	/*.next */ &pause_api_interface
};

static switch_api_interface_t load_api_interface = {
	/*.interface_name */ "load",
	/*.desc */ "Load Module",
	/*.function */ load_function,
	/*.syntax */ NULL,
	/*.next */ &transfer_api_interface
};

static switch_api_interface_t reload_api_interface = {
	/*.interface_name */ "reloadxml",
	/*.desc */ "Reload XML",
	/*.function */ reload_function,
	/*.syntax */ NULL,
	/*.next */ &load_api_interface,

};

static switch_api_interface_t commands_api_interface = {
	/*.interface_name */ "killchan",
	/*.desc */ "Kill Channel",
	/*.function */ kill_function,
	/*.syntax */ NULL,
	/*.next */ &reload_api_interface
};

static switch_api_interface_t originate_api_interface = {
	/*.interface_name */ "originate",
	/*.desc */ "Originate a Call",
	/*.function */ originate_function,
	/*.syntax */ NULL,
	/*.next */ &commands_api_interface
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

