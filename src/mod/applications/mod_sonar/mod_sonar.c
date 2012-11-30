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
 * William King <william.king@quentustech.com>
 *
 * mod_sonar.c -- Sonar ping timer
 *
 * 
 */

/*
  TODO:
  1. Use libteltone directly
  2. Use an energy detection to listen for first set of sound back. Use timestamp of detection of energy as the recv stamp if a tone is eventually detected.
  3. Check for milliwatt pings. Listen for frequency changes, and audio loss
 */


#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sonar_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_sonar_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_sonar_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_sonar, mod_sonar_load, mod_sonar_shutdown, NULL);

switch_time_t start, end, diff;

switch_bool_t sonar_ping_callback(switch_core_session_t *session, const char *app, const char *app_data){
	
	if ( end ) {
		return SWITCH_TRUE;
	}
	
	end = switch_time_now();
	diff = end - start;
	start = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sonar ping took %ld milliseconds\n", (long)diff / 1000);
	
	return SWITCH_TRUE;
}

SWITCH_STANDARD_APP(sonar_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *tone = "%(500,0,1004)";
	int loops = atoi(data);
	
	if ( ! loops ) {
		loops = 5;
	}
	
	switch_channel_answer(channel);
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);
	
	switch_ivr_tone_detect_session(session, 
								   "ping", "1004",
								   "r", 0, 
								   1, NULL, NULL, sonar_ping_callback);
	
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);

	for( int x = 0; x < loops; x++ ) {
		end = 0;
		start = switch_time_now();
		switch_ivr_gentones(session, tone, 1, NULL);
		switch_ivr_sleep(session, 2000, SWITCH_FALSE, NULL);
		if ( start ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Lost sonar ping\n");			
		}
	}
	
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);
	switch_ivr_stop_tone_detect_session(session);
}

/* Macro expands to: switch_status_t mod_sonar_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_sonar_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "sonar", "sonar", "sonar", sonar_app, "", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_sonar_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sonar_shutdown)
{

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
