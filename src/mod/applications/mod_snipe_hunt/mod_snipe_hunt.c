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
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_snipe_hunt.c -- Framework Demo Module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snipe_hunt_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_snipe_hunt_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_snipe_hunt_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_snipe_hunt, mod_snipe_hunt_load, mod_snipe_hunt_shutdown, NULL);

typedef enum {
	CODEC_NEGOTIATION_GREEDY = 1,
	CODEC_NEGOTIATION_GENEROUS = 2,
	CODEC_NEGOTIATION_EVIL = 3
} codec_negotiation_t;

static struct {
	char *codec_negotiation_str;
	codec_negotiation_t codec_negotiation;
	switch_bool_t sip_trace;
	int integer;
} globals;

static switch_status_t config_callback_siptrace(switch_xml_config_item_t *data, switch_config_callback_type_t callback_type, switch_bool_t changed)
{
	switch_bool_t value = *(switch_bool_t *) data->ptr;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "In siptrace callback: value %s changed %s\n",
					  value ? "true" : "false", changed ? "true" : "false");


	/*
	   if ((callback_type == CONFIG_LOG || callback_type == CONFIG_RELOAD) && changed) {
	   nua_set_params(((sofia_profile_t*)data->functiondata)->nua, TPTAG_LOG(value), TAG_END());
	   } 
	 */

	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_config_string_options_t config_opt_codec_negotiation = { NULL, 0, "greedy|generous|evil" };

/* enforce_min, min, enforce_max, max */
static switch_xml_config_int_options_t config_opt_integer = { SWITCH_TRUE, 0, SWITCH_TRUE, 10 };
static switch_xml_config_enum_item_t config_opt_codec_negotiation_enum[] = {
	{"greedy", CODEC_NEGOTIATION_GREEDY},
	{"generous", CODEC_NEGOTIATION_GENEROUS},
	{"evil", CODEC_NEGOTIATION_EVIL},
	{NULL, 0}
};

static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM("codec-negotiation-str", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.codec_negotiation_str, "greedy",
					   &config_opt_codec_negotiation,
					   "greedy|generous|evil", "Specifies the codec negotiation scheme to be used."),
	SWITCH_CONFIG_ITEM("codec-negotiation", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &globals.codec_negotiation, (void *) CODEC_NEGOTIATION_GREEDY,
					   &config_opt_codec_negotiation_enum,
					   "greedy|generous|evil", "Specifies the codec negotiation scheme to be used."),
	SWITCH_CONFIG_ITEM_CALLBACK("sip-trace", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &globals.sip_trace, (void *) SWITCH_FALSE,
								(switch_xml_config_callback_t) config_callback_siptrace, NULL,
								"yes|no", "If enabled, print out sip messages on the console."),
	SWITCH_CONFIG_ITEM("integer", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &globals.integer, (void *) 100, &config_opt_integer,
					   NULL, NULL),
	SWITCH_CONFIG_ITEM_END()
};

static switch_status_t do_config(switch_bool_t reload)
{
	memset(&globals, 0, sizeof(globals));

	if (switch_xml_config_parse_module_settings("snipe_hunt.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open snipe_hunt.conf\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(snipe_hunt_function)
{
	do_config(SWITCH_TRUE);

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_snipe_hunt_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_snipe_hunt_load)
{
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	do_config(SWITCH_FALSE);

	SWITCH_ADD_API(api_interface, "snipe_hunt", "Snipe_Hunt API", snipe_hunt_function, "syntax");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_snipe_hunt_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snipe_hunt_shutdown)
{
	/* Cleanup dynamically allocated config settings */
	switch_xml_config_cleanup(instructions);
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_snipe_hunt_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_snipe_hunt_runtime)
{
	while(looping)
	{
		switch_cond_next();
	}
	return SWITCH_STATUS_TERM;
}
*/

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
