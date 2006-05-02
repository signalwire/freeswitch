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
 * mod_dialplan_flatfile.c -- Example Dialplan Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static const char modname[] = "mod_dialplan_flatfile";


static switch_caller_extension_t *flatfile_dialplan_hunt(switch_core_session_t *session)
{
	switch_caller_profile_t *caller_profile = NULL;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel;
	char *cf = "extensions.conf";
	switch_config_t cfg;
	char *var, *val;
	char app[1024];
	char *context = NULL;

	channel = switch_core_session_get_channel(session);

	if ((caller_profile = switch_channel_get_caller_profile(channel))) {
		context = caller_profile->context ? caller_profile->context : "default";
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Hello %s You Dialed %s!\n", caller_profile->caller_id_name,
					  caller_profile->destination_number);

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return NULL;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, context)) {
			if (!strcmp(var, caller_profile->destination_number) && val) {
				char *data;

				memset(app, 0, sizeof(app));
				strncpy(app, val, sizeof(app));

				if ((data = strchr(app, ' ')) != 0) {
					*data = '\0';
					data++;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid extension on line %d\n", cfg.lineno);
					continue;
				}
				if (!extension) {
					if ((extension =
						 switch_caller_extension_new(session, caller_profile->destination_number,
													 caller_profile->destination_number)) == 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Cannot locate extension %s in context %s\n", caller_profile->destination_number, context);
		switch_channel_hangup(channel, SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST);
	}

	return extension;
}


static const switch_dialplan_interface_t flatfile_dialplan_interface = {
	/*.interface_name = */ "flatfile",
	/*.hunt_function = */ flatfile_dialplan_hunt
	/*.next = NULL */
};

static const switch_loadable_module_interface_t flatfile_dialplan_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ &flatfile_dialplan_interface,
	/*.codec_interface = */ NULL,
	/*.application_interface = */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &flatfile_dialplan_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
