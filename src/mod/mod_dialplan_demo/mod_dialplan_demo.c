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
 * mod_dialplan_demo.c -- Example Dialplan Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static const char modname[] = "mod_dialplan_demo";


switch_caller_extension *demo_dialplan_hunt(switch_core_session *session)
{
	switch_caller_profile *caller_profile;
	switch_caller_extension *extension = NULL;
	switch_channel *channel;
	char *cf = "extensions.conf";
	switch_config cfg;
	char *var, *val;
	char app[1024];

	channel = switch_core_session_get_channel(session);
	caller_profile = switch_channel_get_caller_profile(channel);
	//switch_channel_set_variable(channel, "pleasework", "yay");

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hello %s You Dialed %s!\n", caller_profile->caller_id_name, caller_profile->destination_number);	

	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		switch_channel_hangup(channel);
		return NULL;
	}
	
	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "extensions")) {
			if (!strcmp(var, caller_profile->destination_number) && val) {
				char *data;

				memset(app, 0, sizeof(app));
				strncpy(app, val, sizeof(app));
			
				if ((data = strchr(app, ' '))) {
					*data = '\0';
					data++;
				} else {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "invalid extension on line %d\n", cfg.lineno);
					continue;
				}
				if (!extension) {
					if (!(extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number))) {
						switch_console_printf(SWITCH_CHANNEL_CONSOLE, "memory error!\n");
						break;
					}
				}

				switch_caller_extension_add_application(session, extension, app, data);
			} 
		}
	}

	switch_config_close_file(&cfg);

	if (extension) {
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		switch_channel_hangup(channel);
	}
	
	return extension;
}


static const switch_dialplan_interface demo_dialplan_interface = {
	/*.interface_name =*/ "demo",
	/*.hunt_function = */ demo_dialplan_hunt
	/*.next = NULL */
};

static const switch_loadable_module_interface demo_dialplan_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ &demo_dialplan_interface,
	/*.codec_interface = */ NULL,
	/*.application_interface =*/ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &demo_dialplan_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}




