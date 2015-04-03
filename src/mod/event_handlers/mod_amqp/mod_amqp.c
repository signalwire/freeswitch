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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* mod_amqp.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#include "mod_amqp.h"

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amqp_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_amqp_load);
SWITCH_MODULE_DEFINITION(mod_amqp, mod_amqp_load, mod_amqp_shutdown, NULL);

SWITCH_STANDARD_API(amqp_reload)
{
  return mod_amqp_do_config(SWITCH_TRUE);
}


/* ------------------------------
   Startup
   ------------------------------
*/
SWITCH_MODULE_LOAD_FUNCTION(mod_amqp_load)
{
	switch_api_interface_t *api_interface;

	memset(&globals, 0, sizeof(globals));
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	globals.pool = pool;
	switch_core_hash_init(&(globals.producer_hash));
	switch_core_hash_init(&(globals.command_hash));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_apqp loading: Version %s\n", switch_version_full());

	/* Create producer profiles */
	if ( mod_amqp_do_config(SWITCH_FALSE) != SWITCH_STATUS_SUCCESS ){
		return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_API(api_interface, "amqp", "amqp API", amqp_reload, "syntax");

	return SWITCH_STATUS_SUCCESS;
}

/* ------------------------------
   Shutdown
   ------------------------------
*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amqp_shutdown)
{
	switch_hash_index_t *hi;
	mod_amqp_producer_profile_t *producer;
	mod_amqp_command_profile_t *command;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Mod starting shutting down\n");
	switch_event_unbind_callback(mod_amqp_producer_event_handler);

	for (hi = switch_core_hash_first(globals.producer_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, NULL, NULL, (void **)&producer);
		mod_amqp_producer_destroy(&producer);
	}

	for (hi = switch_core_hash_first(globals.command_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, NULL, NULL, (void **)&command);
		mod_amqp_command_destroy(&command);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Mod finished shutting down\n");
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
