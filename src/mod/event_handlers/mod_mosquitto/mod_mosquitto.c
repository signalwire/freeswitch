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
 * Norm Brandinger <n.brandinger@gmail.com>
 *
 * mod_mosquitto: Interface to an MQTT broker using Mosquitto
 *				  Implements a Publish/Subscribe (pub/sub) messaging pattern using the Mosquitto API library
 *				  Publishes FreeSWITCH events to one more more MQTT brokers
 *				  Subscribes to topics located on one more more MQTT brokers
 *
 * MQTT http://mqtt.org/
 * Mosquitto https://mosquitto.org/
 *
 */
#include <switch.h>

#include "mod_mosquitto.h"
#include "mosquitto_config.h"
#include "mosquitto_cli.h"
#include "mosquitto_utils.h"
#include "mosquitto_events.h"
#include "mosquitto_mosq.h"

struct globals_s mosquitto_globals;
switch_loadable_module_interface_t *module_interface;

SWITCH_MODULE_LOAD_FUNCTION(mod_mosquitto_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mosquitto_shutdown);
//SWITCH_MODULE_RUNTIME_FUNCTION(mod_mosquitto_runtime);

//SWITCH_MODULE_DEFINITION(mod_mosquitto, mod_mosquitto_load, mod_mosquitto_shutdown, mod_mosquitto_runtime);
SWITCH_MODULE_DEFINITION(mod_mosquitto, mod_mosquitto_load, mod_mosquitto_shutdown, NULL);

/**
 * @brief	This function is called when FreeSWITCH loads the mod_mosquitto module
 *
 * @notee	The definition of this function is performed by the macro SWITCH_MODULE_LOAD_FUNCTION that expands to
 *		  switch_status_t mod_mosquitto_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
 *
 * \param[in]	switch_loadable_module_interface_t **module_interface
 * \param[in]	switch_memory_pool_t *pool
 *
 * @retval		switch_status_t
 *
 */

SWITCH_MODULE_LOAD_FUNCTION(mod_mosquitto_load) {

	memset(&mosquitto_globals, 0, sizeof(mosquitto_globals));
	mosquitto_globals.pool = pool;

	switch_mutex_init(&mosquitto_globals.mutex, SWITCH_MUTEX_NESTED, mosquitto_globals.pool);
	switch_thread_rwlock_create(&mosquitto_globals.bgapi_rwlock, mosquitto_globals.pool);
	switch_mutex_init(&mosquitto_globals.profiles_mutex, SWITCH_MUTEX_NESTED, mosquitto_globals.pool);
	switch_core_hash_init(&mosquitto_globals.profiles);

	/* create a loadable module interface structure named with modname */
	/* the module interface defines the different interfaces that this module has defined */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* create an api interface for the module command line interface (cli) */
	add_cli_api(module_interface, &mosquitto_globals.api_interface);

	if (mosquitto_load_config(MOSQUITTO_CONFIG_FILE) != SWITCH_STATUS_SUCCESS) {
		log(SWITCH_LOG_ERROR, "Configuration failed to load\n");
		return SWITCH_STATUS_TERM;
	}
	log(SWITCH_LOG_INFO, "Configuration loaded\n");

	mosquitto_globals.running = 1;
	mosq_startup();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/**
 * @brief	This function is called when FreeSWITCH unloads the mod_mosquitto module
 *
 * @note	The definition of this function is performed by the macro SWITCH_MODULE_SHUTDOWN_FUNCTION that expands to
 *		  switch_status_t mod_mosquitto_shutdown(void)
 *
 * @retval	switch_status_t
 *
 */

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mosquitto_shutdown) {

	switch_mutex_lock(mosquitto_globals.mutex);
	mosquitto_globals.running = 0;

	switch_queue_term(mosquitto_globals.event_queue);
	switch_event_unbind_callback(event_handler);

	remove_cli_api();

	mosq_shutdown();

	switch_mutex_destroy(mosquitto_globals.profiles_mutex);
	switch_mutex_unlock(mosquitto_globals.mutex);
	switch_mutex_destroy(mosquitto_globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * @brief	This is the runtime loop of the module, from here you can listen on sockets, spawn new threads to handle requests. etc.
 *
 * @notee	The definition of this function is performed by the macro SWITCH_MODULE_RUNTIME_FUNCTION that expands to
 *			switch_status_t mod_mosquitto_runtime(void)
 *
 * @retval	switch_status_t
 *
 */

/*
SWITCH_MODULE_RUNTIME_FUNCTION(mod_mosquitto_runtime) {
	log(SWITCH_LOG_DEBUG, "rutime called\n");
	return SWITCH_STATUS_SUCCESS;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
