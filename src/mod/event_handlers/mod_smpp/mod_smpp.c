/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
* William King <william.king@quentustech.com>
*
* mod_smpp.c -- smpp client and server implementation using libsmpp
*
* using libsmpp from: http://cgit.osmocom.org/libsmpp/
*
*/

#include <mod_smpp.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_smpp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_smpp_shutdown);
SWITCH_MODULE_DEFINITION(mod_smpp, mod_smpp_load, mod_smpp_shutdown, NULL);

mod_smpp_globals_t mod_smpp_globals;

switch_status_t mod_smpp_interface_chat_send(switch_event_t *event)
{
	mod_smpp_gateway_t *gateway = NULL;
	char *gw_name = switch_event_get_header(event, "smpp_gateway");

	if (zstr(gw_name)) {
		gw_name = "default";
	}
	
	gateway = switch_core_hash_find(mod_smpp_globals.gateways, gw_name);

	if (!gateway) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMPP GATEWAY[%s].", gw_name);
		return SWITCH_STATUS_GENERR;
	}

	mod_smpp_gateway_send_message(gateway, event);

	return SWITCH_STATUS_SUCCESS;
}

/* static switch_status_t name (switch_event_t *message, const char *data) */
SWITCH_STANDARD_CHAT_APP(mod_smpp_chat_send_function)
{
	mod_smpp_gateway_t *gateway = NULL;

	gateway = switch_core_hash_find(mod_smpp_globals.gateways, data);

	if ( !gateway ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMPP GATEWAY[%s].", data);
		return SWITCH_STATUS_GENERR;
	}

	mod_smpp_gateway_send_message(gateway, message);
	return SWITCH_STATUS_SUCCESS;
}

/* static void name (switch_core_session_t *session, const char *data) */
SWITCH_STANDARD_APP(mod_smpp_app_send_function)
{
	switch_event_header_t *chan_var = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *message = NULL;

	if (switch_event_create(&message, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		return;
	}
	
	/* Copy over recognized channel vars. Then call the chat send function */
	/* Cycle through all of the channel headers, and ones with 'smpp_' prefix copy over without the prefix */
	for ( chan_var = switch_channel_variable_first(channel); chan_var; chan_var = chan_var->next) {
		if ( !strncmp(chan_var->name, "smpp_", 5) ) {
			switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, chan_var->name + 5, chan_var->value);
		} else {
			switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, chan_var->name, chan_var->value);			
		}
	}
	
	/* Unlock the channel variables */
	switch_channel_variable_last(channel);
	mod_smpp_chat_send_function(message, data);
	return;
}

/* static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream) */
SWITCH_STANDARD_API(mod_smpp_debug_api)
{
	mod_smpp_globals.debug = switch_true(cmd);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "debug is %s\n", (mod_smpp_globals.debug ? "on" : "off") );
	return SWITCH_STATUS_SUCCESS;
}

/* static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream) */
SWITCH_STANDARD_API(mod_smpp_send_api)
{
	mod_smpp_gateway_t *gateway = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_t *message = NULL;
	char *argv[1024] = { 0 };
	int argc = 0;
	char *cmd_dup = strdup(cmd);

	if (!(argc = switch_separate_string(cmd_dup, '|', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid format. Must be | separated like: gateway|destination|source|message\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	gateway = switch_core_hash_find(mod_smpp_globals.gateways, argv[0]);

	if ( !gateway ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMPP GATEWAY[%s].", argv[0]);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if (switch_event_create(&message, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}
	
	switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, "to_user", argv[1]);
	switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, "from_user", argv[2]);
	switch_event_set_body(message, argv[3]);

	if (mod_smpp_gateway_send_message(gateway, message) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(cmd_dup);
	return status;
									
}

switch_status_t mod_smpp_do_config() 
{
	char *conf = "smpp.conf";
	switch_xml_t xml, cfg, gateways, gateway, params, param;

	if (!(xml = switch_xml_open_cfg(conf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", conf);
		goto err;
	}

	if ( (gateways = switch_xml_child(cfg, "gateways")) != NULL) {
		for (gateway = switch_xml_child(gateways, "gateway"); gateway; gateway = gateway->next) {		
			mod_smpp_gateway_t *new_gateway = NULL;
			char *host = NULL, *system_id = NULL, *password = NULL, *profile = NULL, *system_type = NULL;
			int port = 0, debug = 0;
			
			char *name = (char *)switch_xml_attr_soft(gateway, "name");

			// Load params
			if ( (params = switch_xml_child(gateway, "params")) != NULL) {
				for (param = switch_xml_child(params, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( ! strncmp(var, "host", 4) ) {
						host = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "port", 4) ) {
						port = atoi(switch_xml_attr_soft(param, "value"));
					} else if ( ! strncmp(var, "debug", 5) ) {
						debug = atoi(switch_xml_attr_soft(param, "value"));
					} else if ( ! strncmp(var, "system_id", 9) ) {
						system_id = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "password", 8) ) {
						password = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "profile", 7) ) {
						profile = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "system_type", 11) ) {
						system_type = (char *) switch_xml_attr_soft(param, "value");
					}
				}
			}

			if ( mod_smpp_gateway_create(&new_gateway, name, host, port, debug, system_id, password, system_type, profile) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created gateway[%s]\n", name);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create gateway[%s]\n", name);
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Gateways config is missing\n");
		goto err;
	}
	
	return SWITCH_STATUS_SUCCESS;
	
 err:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Configuration failed\n");
	return SWITCH_STATUS_GENERR;
}

/* switch_status_t name (switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_smpp_load)
{
	switch_api_interface_t *mod_smpp_api_interface;
	switch_chat_interface_t *mod_smpp_chat_interface;
	switch_chat_application_interface_t *mod_smpp_chat_app_interface;
	switch_application_interface_t *mod_smpp_app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	memset(&mod_smpp_globals, 0, sizeof(mod_smpp_globals_t));
	mod_smpp_globals.pool = pool;
	mod_smpp_globals.debug = 0;
	switch_core_hash_init(&(mod_smpp_globals.gateways));
	
	if ( mod_smpp_do_config() != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load due to bad configs\n");
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_CHAT(mod_smpp_chat_interface, "smpp", mod_smpp_interface_chat_send);
	SWITCH_ADD_API(mod_smpp_api_interface, "smpp_debug", "mod_smpp toggle debug", mod_smpp_debug_api, NULL);
	SWITCH_ADD_API(mod_smpp_api_interface, "smpp_send", "mod_smpp send", mod_smpp_send_api, NULL);
	SWITCH_ADD_CHAT_APP(mod_smpp_chat_app_interface, "smpp_send", "send message to gateway", "send message to gateway", 
						mod_smpp_chat_send_function, "", SCAF_NONE);
	SWITCH_ADD_APP(mod_smpp_app_interface, "smpp_send", NULL, NULL, mod_smpp_app_send_function, 
				   "smpp_send", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_smpp_shutdown)
{
	switch_hash_index_t *hi;
	mod_smpp_gateway_t *gateway = NULL;
	/* loop through gateways, and destroy them */
	/* destroy gateways hash */

	while ((hi = switch_core_hash_first(mod_smpp_globals.gateways))) {
		switch_core_hash_this(hi, NULL, NULL, (void **)&gateway);
		mod_smpp_gateway_destroy(&gateway);
		switch_safe_free(hi);
	}

	switch_core_hash_destroy(&(mod_smpp_globals.gateways));

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
