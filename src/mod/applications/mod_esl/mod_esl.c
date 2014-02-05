/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * mod_esl.c -- Allow to generate remote ESL commands
 *
 */
#include <switch.h>

#include <esl.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_esl_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_esl_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_esl_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_esl, mod_esl_load, mod_esl_shutdown, NULL);

SWITCH_STANDARD_API(single_esl_api_function)
{
	esl_handle_t handle = {{0}};
	char *host = "127.0.0.1";
	char *s_port = NULL;
	int port = 8021;
	char *username = NULL;
	char *password = "ClueCon";
	char *args = NULL;
	char *s_timeout = NULL;
	int timeout = 5000;
	char *dup = strdup(cmd);
	char *send = NULL;

	username = dup;

	if (username && (password = strchr(username, '|'))) {
		*password++ = '\0';
	}

	if (password && (host = strchr(password, ' '))) {
		*host++ = '\0';
	}   

	if (host && (s_timeout = strchr(host, ' '))) {
		*s_timeout++ = '\0';
	}

	if (host && (s_port = strchr(host, ':'))) {
		*s_port++ = '\0';
	}

	if (s_timeout && (args = strchr(s_timeout, ' '))) {
		*args++ = '\0';
	}

	if (!zstr(s_port)) {
		port = atoi(s_port);
	}

	if (zstr(host) || zstr(password) || zstr(args) || zstr(s_timeout)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad parameters\n");
		goto end;
	}


	timeout = atoi(s_timeout);


	if  (esl_connect_timeout(&handle, host, port, username, password, timeout) != ESL_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to remote ESL at %s:%d\n",
				host, port);
		goto end;
	} else {
		send = switch_mprintf("api %s", args);
		if (esl_send_recv_timed(&handle, send, timeout) != ESL_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Disconnected from remote ESL at %s:%d\n",
					host, port);
			goto end;
		} else {
			stream->write_function(stream, handle.last_sr_event->body);
		}
	}

end:
	esl_disconnect(&handle);
	memset(&handle, 0, sizeof(handle));
	switch_safe_free(send);
	switch_safe_free(dup);

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_esl_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_esl_load)
{
	switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "single_esl", "Allow to do a single connection api call to a remote ESL server", single_esl_api_function, "[<user>]|<password> <host>[:<port>] <timeout> <remote api> <arguments>");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_esl_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_esl_shutdown)
{
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
