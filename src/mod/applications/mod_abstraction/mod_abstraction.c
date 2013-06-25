/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_abstraction.c -- Abstraction
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_abstraction_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_abstraction_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_abstraction_load);

const char *global_cf = "abstraction.conf";

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_abstraction, mod_abstraction_load, mod_abstraction_shutdown, NULL);

SWITCH_STANDARD_API(api_abstraction_function)
{
	const char *api_name = switch_event_get_header(stream->param_event, "API-Command");
	switch_xml_t cfg, xml, x_apis, x_api;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		goto end;
	}

	if (!(x_apis = switch_xml_child(cfg, "apis"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No apis group\n");
		goto end;
	}

	if ((x_api = switch_xml_find_child_multi(x_apis, "api", "name", api_name , NULL))) {
		const char *parse = switch_xml_attr_soft(x_api, "parse");
		const char *destination = switch_xml_attr_soft(x_api, "destination");
		const char *arguments = switch_xml_attr_soft(x_api, "argument");

		int proceed = 0;
		switch_regex_t *re = NULL;
		int ovector[30];

		if ((proceed = switch_regex_perform(cmd, parse, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
			const char *api_args = NULL;
			char *substituted = NULL;

			if (cmd && strchr(parse, '(')) {
				uint32_t len = (uint32_t) (strlen(cmd) + strlen(arguments) + 10) * proceed;
				if (!(substituted = malloc(len))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
					proceed = 0;
					goto end;
				}
				memset(substituted, 0, len);
				switch_perform_substitution(re, proceed, arguments, cmd , substituted, len, ovector);
				api_args = substituted;
			} else {
				api_args = arguments;
			}
			switch_api_execute(destination, api_args, session, stream);

			switch_safe_free(substituted);	
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No match for API %s  (%s != %s)\n", api_name, parse, cmd);
		}
		switch_regex_safe_free(re);
	
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "API %s doesn't exist inside the xml structure.  You might have forgot to reload the module after editing it\n", api_name);
	}

end:
	if (xml)
		switch_xml_free(xml);


	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_abstraction_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_abstraction_load)
{
	switch_status_t status = SWITCH_STATUS_TERM;
	switch_api_interface_t *api_interface;
	switch_xml_t cfg, xml, x_apis, x_api;
	int count = 0;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		goto end;
	}

	if (!(x_apis = switch_xml_child(cfg, "apis"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No apis group\n");
		goto end;
	}

	for (x_api = switch_xml_child(x_apis, "api"); x_api; x_api = x_api->next) {
		const char *name = switch_xml_attr_soft(x_api, "name");
		const char *description = switch_xml_attr_soft(x_api, "description");
		const char *syntax = switch_xml_attr_soft(x_api, "syntax");
		SWITCH_ADD_API(api_interface, name, description, api_abstraction_function, syntax);
		count++;

	}
	if (count > 0) {
		status = SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No API abstraction defined\n");
	}
end:
	if (xml)
		switch_xml_free(xml);

	return status;
}

/*
   Called when the system shuts down
   Macro expands to: switch_status_t mod_abstraction_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_abstraction_shutdown)
{
	/* Cleanup dynamically allocated config settings */
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
