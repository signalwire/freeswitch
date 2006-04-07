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


static switch_status kill_function(char *dest, char *out, size_t outlen)
{
	switch_core_session *session = NULL;

	if ((session = switch_core_session_locate(dest))) {
		switch_channel *channel = switch_core_session_get_channel(session);
		switch_core_session_kill_channel(session, SWITCH_SIG_KILL);
		switch_channel_hangup(channel);
		snprintf(out, outlen, "OK\n");
	} else {
		snprintf(out, outlen, "No Such Channel!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


static struct switch_api_interface commands_api_interface = {
	/*.interface_name */ "killchan",
	/*.desc */ "Kill Channel",
	/*.function */ kill_function,
	/*.next */ NULL
};


static const switch_loadable_module_interface mod_commands_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ &commands_api_interface
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_commands_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

