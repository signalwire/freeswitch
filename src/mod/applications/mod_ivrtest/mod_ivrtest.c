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
 * mod_ivrtest.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>


static const char modname[] = "mod_ivrtest";


/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status on_dtmf(switch_core_session *session, char *dtmf, void *buf, unsigned int buflen)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Digits %s\n", dtmf);
	
	switch_copy_string((char *)buf, dtmf, buflen);
	return SWITCH_STATUS_BREAK;

}

static void dirtest_function(switch_core_session *session, char *data)
{
	char *var, *val;
	switch_channel *channel;
	switch_directory_handle dh;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);


	if (switch_core_directory_open(&dh, 
								   "ldap",
								   "ldap.freeswitch.org",
								   "cn=Manager,dc=freeswitch,dc=org",
								   "test",
								   NULL) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't connect\n");
		return;
	}


	switch_core_directory_query(&dh, "ou=dialplan,dc=freeswitch,dc=org", "(objectClass=*)");
	while (switch_core_directory_next(&dh) == SWITCH_STATUS_SUCCESS) {
		while (switch_core_directory_next_pair(&dh, &var, &val) == SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "VALUE [%s]=[%s]\n", var, val);
		}
	}

	switch_core_directory_close(&dh);
	switch_channel_hangup(channel);

}

static void ivrtest_function(switch_core_session *session, char *data)
{
	switch_channel *channel;
	switch_status status = SWITCH_STATUS_SUCCESS;
	char buf[10] = "";
	char term;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	switch_channel_answer(channel);
	
	
	while (switch_channel_get_state(channel) == CS_EXECUTE) {
		memset(buf, 0, sizeof(buf));
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Enter up to 10 digits, press # to terminate, * to hangup\n");

		if (data) {
			/* you could have passed NULL instead of on_dtmf to get this exact behaviour (copy the digits to buf and stop playing)
			   but you may want to pass the function if you have something cooler to do...
			*/
			status = switch_ivr_play_file(session, data, NULL, on_dtmf, buf, sizeof(buf));
			
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				switch_channel_hangup(channel);
				break;
			}
		}

		if (switch_ivr_collect_digits_count(session, buf, sizeof(buf), 10, "#*", &term) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel);
			break;
		}

		if (term && term == '*') {
			break;
		}

		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "You Dialed [%s]\n", buf);
	}
	
}


static switch_status my_on_hangup(switch_core_session *session)
{
	switch_channel *channel;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "I globally hooked to [%s] on the hangup event\n", switch_channel_get_name(channel));
	return SWITCH_STATUS_SUCCESS;

}

static const switch_state_handler_table state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

static const switch_application_interface dirtest_application_interface = {
	/*.interface_name */ "dirtest",
	/*.application_function */ dirtest_function
};

static const switch_application_interface ivrtest_application_interface = {
	/*.interface_name */ "ivrtest",
	/*.application_function */ ivrtest_function,
	NULL, NULL, NULL,
	/*.next*/ &dirtest_application_interface
};

static const switch_loadable_module_interface mod_ivrtest_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &ivrtest_application_interface
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_ivrtest_module_interface;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
   if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
*/


//switch_status switch_module_runtime(void)
