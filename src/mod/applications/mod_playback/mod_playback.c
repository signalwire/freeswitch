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
 * mod_playback.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>
#include <switch_ivr.h>



static const char modname[] = "mod_playback";

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status on_dtmf(switch_core_session *session, char *dtmf)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Digits %s\n", dtmf);

	if (*dtmf == '*') {
		return SWITCH_STATUS_FALSE;
	}
	
	return SWITCH_STATUS_SUCCESS;
}


void playback_function(switch_core_session *session, char *data)
{
	switch_channel *channel;
	char *timer_name = NULL;
	char *file_name = NULL;

	file_name = switch_core_session_strdup(session, data);

	if ((timer_name = strchr(file_name, ' '))) {
		*timer_name++ = '\0';
	}

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (switch_ivr_play_file(session, file_name, timer_name, on_dtmf) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel);
	}
	
}

static const switch_application_interface playback_application_interface = {
	/*.interface_name */ "playback",
	/*.application_function */ playback_function
};

static const switch_loadable_module_interface mod_playback_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &playback_application_interface
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_playback_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
*/


//switch_status switch_module_runtime(void)
