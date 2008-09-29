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
 * The Original Code is FreeSWITCH mod_timezone.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * mod_timezone.c -- Access to timezone informations and time string formatting
 *
 */

#include <switch.h>


/* **************************************************************************
   API FUNCTIONS AND COMMANDS
   ************************************************************************** */

SWITCH_STANDARD_API(strftime_tz_api_function)
{
	char *format = NULL;
	const char *tz_name = NULL;
	char date[80] = "";

	if (!switch_strlen_zero(cmd)) {
		format = strchr(cmd, ' ');
		tz_name = cmd;
		if (format) {
			*format++ = '\0';
		}
	}
	
	if (switch_strftime_tz(tz_name, format, date, sizeof(date)) == SWITCH_STATUS_SUCCESS) { /* The lookup of the zone may fail. */
		stream->write_function(stream, "%s", date);
	} else {
		stream->write_function(stream, "-ERR Invalid Timezone\n");
	}
	
	return SWITCH_STATUS_SUCCESS;
}


/* **************************************************************************

************************************************************************** */


SWITCH_MODULE_LOAD_FUNCTION(mod_timezone_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timezone_shutdown);
SWITCH_MODULE_DEFINITION(mod_timezone, mod_timezone_load, mod_timezone_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_timezone_load)
{
	switch_api_interface_t *api_interface;
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "strftime_tz", "strftime_tz", strftime_tz_api_function, "<Timezone name>,<format string>");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timezone_shutdown)
{

	return SWITCH_STATUS_UNLOAD;
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
