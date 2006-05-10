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
 *
 * 
 * mod_commands.c -- Misc. Command Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_commands";


static switch_status_t load_function(char *mod, char *out, size_t outlen)
{
	switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) mod);
	snprintf(out, outlen, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kill_function(char *dest, char *out, size_t outlen)
{
	switch_core_session_t *session = NULL;

	if ((session = switch_core_session_locate(dest))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_core_session_kill_channel(session, SWITCH_SIG_KILL);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
		snprintf(out, outlen, "OK\n");
	} else {
		snprintf(out, outlen, "No Such Channel!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t transfer_function(char *cmd, char *out, size_t outlen)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = {0};
	int argc = 0;
	
	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2 || argc > 4) {
		snprintf(out, outlen, "Invalid Parameters\n");
	} else {
		char *uuid = argv[0];
		char *dest = argv[1];
		char *dp = argv[2];
		char *context = argv[3];
		
		if ((session = switch_core_session_locate(uuid))) {

			if (switch_ivr_session_transfer(session, dest, dp, context) == SWITCH_STATUS_SUCCESS) {
				snprintf(out, outlen, "OK\n");
			} else {
				snprintf(out, outlen, "ERROR\n");
			}

			switch_core_session_rwunlock(session);

		} else {
			snprintf(out, outlen, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}




static switch_status_t pause_function(char *cmd, char *out, size_t outlen)
{
	switch_core_session_t *session = NULL;
	char *argv[4] = {0};
	int argc = 0;
	
	argc = switch_separate_string(cmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		snprintf(out, outlen, "Invalid Parameters\n");
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
			snprintf(out, outlen, "No Such Channel!\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

struct show_return {
	char *out;
	size_t remaining;
};

static int show_callback(void *pArg, int argc, char **argv, char **columnNames){
	struct show_return *returnval = (struct show_return *) pArg;
	char temp[1024];
	size_t len;

	sprintf(temp, "%s\n", argv[1]);
	len = strlen(temp);

	if (len < returnval->remaining) {
		strcpy(returnval->out, temp);
		returnval->remaining -= len;
		returnval->out += len;
	}
	return 0;
}

static switch_status_t status_function(char *cmd, char *out, size_t outlen)
{
	switch_core_time_duration_t duration;
	switch_core_measure_time(switch_core_uptime(), &duration);

	snprintf(out, outlen, "<b>UP %u year(s), %u day(s), %u hour(s), %u minute(s), %u second(s), %u millisecond(s), %u microsecond(s)</b>\n",
			 duration.yr,
			 duration.day,
			 duration.hr,
			 duration.min,
			 duration.sec,
			 duration.ms,
			 duration.mms
			 );
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t show_function(char *cmd, char *out, size_t outlen)
{
	char sql[1024];
	char *errmsg;
	struct show_return returnval;
	switch_core_db_t *db = switch_core_db_handle();

	sprintf (sql, "select * from interfaces");
	returnval.out = out;
	returnval.remaining = outlen;

	switch_core_db_exec(db, sql, show_callback, &returnval, &errmsg);

	if (errmsg) {
		snprintf(out, outlen, "SQL ERR [%s]\n",errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	}

	switch_core_db_close(db);
	return SWITCH_STATUS_SUCCESS;
}


static switch_api_interface_t status_api_interface = {
	/*.interface_name */ "status",
	/*.desc */ "status",
	/*.function */ status_function,
	/*.next */ NULL
};

static switch_api_interface_t show_api_interface = {
	/*.interface_name */ "show",
	/*.desc */ "Show",
	/*.function */ show_function,
	/*.next */ &status_api_interface
};

static switch_api_interface_t pause_api_interface = {
	/*.interface_name */ "pause",
	/*.desc */ "Pause",
	/*.function */ pause_function,
	/*.next */ &show_api_interface
};

static switch_api_interface_t transfer_api_interface = {
	/*.interface_name */ "transfer",
	/*.desc */ "Transfer",
	/*.function */ transfer_function,
	/*.next */ &pause_api_interface
};

static switch_api_interface_t load_api_interface = {
	/*.interface_name */ "load",
	/*.desc */ "Load Modile",
	/*.function */ load_function,
	/*.next */ &transfer_api_interface
};


static switch_api_interface_t commands_api_interface = {
	/*.interface_name */ "killchan",
	/*.desc */ "Kill Channel",
	/*.function */ kill_function,
	/*.next */ &load_api_interface
};


static const switch_loadable_module_interface_t mod_commands_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ &commands_api_interface
};


SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_commands_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

