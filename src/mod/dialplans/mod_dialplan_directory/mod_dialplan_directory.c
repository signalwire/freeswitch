/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_dialplan_directory.c -- Example Dialplan Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_directory_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dialplan_directory_shutdown);
SWITCH_MODULE_DEFINITION(mod_dialplan_directory, mod_dialplan_directory_load, mod_dialplan_directory_shutdown, NULL);

static struct {
	char *directory_name;
	char *host;
	char *dn;
	char *pass;
	char *base;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_directory_name, globals.directory_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_host, globals.host);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dn, globals.dn);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_pass, globals.pass);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_base, globals.base);

static void load_config(void)
{
	char *cf = "dialplan_directory.conf";
	switch_xml_t cfg, xml, settings, param;


	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "directory-name") && val) {
				set_global_directory_name(val);
			} else if (!strcmp(var, "host") && val) {
				set_global_host(val);
			} else if (!strcmp(var, "dn") && val) {
				set_global_dn(val);
			} else if (!strcmp(var, "pass") && val) {
				set_global_pass(val);
			} else if (!strcmp(var, "base") && val) {
				set_global_base(val);
			}
		}
	}
	switch_xml_free(xml);
}

SWITCH_STANDARD_DIALPLAN(directory_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *var, *val;
	char filter[256];
	switch_directory_handle_t dh;
	char app[512];
	char *data;

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Hello %s You Dialed %s!\n", caller_profile->caller_id_name,
					  caller_profile->destination_number);

	if (!(globals.directory_name && globals.host && globals.dn && globals.base && globals.pass)) {
		return NULL;
	}

	if (switch_core_directory_open(&dh, globals.directory_name, globals.host, globals.dn, globals.pass, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't connect\n");
		return NULL;
	}

	switch_snprintf(filter, sizeof(filter), "exten=%s", caller_profile->destination_number);
	if (caller_profile->context) {
		switch_snprintf(filter + strlen(filter), sizeof(filter) - strlen(filter), "context=%s", caller_profile->context);
	}


	switch_core_directory_query(&dh, globals.base, filter);
	while (switch_core_directory_next(&dh) == SWITCH_STATUS_SUCCESS) {
		while (switch_core_directory_next_pair(&dh, &var, &val) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DIRECTORY VALUE [%s]=[%s]\n", var, val);
			if (!strcasecmp(var, "callflow")) {
				if (!extension) {
					if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
						goto out;
					}
				}
				switch_copy_string(app, val, sizeof(app));
				if ((data = strchr(app, ' ')) != 0) {
					*data++ = '\0';
				}
				switch_caller_extension_add_application(session, extension, app, data);
			}
		}
	}
  out:

	switch_core_directory_close(&dh);

	return extension;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dialplan_directory_shutdown)
{
	switch_safe_free(globals.directory_name);
	switch_safe_free(globals.host);
	switch_safe_free(globals.dn);
	switch_safe_free(globals.pass);
	switch_safe_free(globals.base);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_directory_load)
{
	switch_dialplan_interface_t *dp_interface;

	memset(&globals, 0, sizeof(globals));
	load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_DIALPLAN(dp_interface, "directory", directory_dialplan_hunt);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
