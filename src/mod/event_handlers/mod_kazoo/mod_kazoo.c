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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * mod_kazoo.c -- Socket Controlled Event Handler
 *
 */
#include "mod_kazoo.h"

globals_t kazoo_globals = {0};



SWITCH_MODULE_DEFINITION(mod_kazoo, mod_kazoo_load, mod_kazoo_shutdown, mod_kazoo_runtime);

SWITCH_MODULE_LOAD_FUNCTION(mod_kazoo_load) {
	switch_api_interface_t *api_interface = NULL;
	switch_application_interface_t *app_interface = NULL;

	memset(&kazoo_globals, 0, sizeof(kazoo_globals));

	kazoo_globals.pool = pool;
	kazoo_globals.ei_nodes = NULL;

	// ensure epmd is running

	if(kazoo_load_config() != SWITCH_STATUS_SUCCESS) {
		// TODO: what would we need to clean up here?
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Improper configuration!\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_thread_rwlock_create(&kazoo_globals.ei_nodes_lock, pool);

	switch_set_flag(&kazoo_globals, LFLAG_RUNNING);

	/* create all XML fetch agents */
	bind_fetch_agents();

	/* create an api for cli debug commands */
	add_cli_api(module_interface, api_interface);

	/* add our modified commands */
	add_kz_commands(module_interface, api_interface);

	/* add our modified dptools */
	add_kz_dptools(module_interface, app_interface);

	/* add our endpoints */
	add_kz_endpoints(module_interface);

	/* add tweaks */
	kz_tweaks_start();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_kazoo_shutdown) {
	int sanity = 0;


	remove_cli_api();

	kz_tweaks_stop();

	/* stop taking new requests and start shuting down the threads */
	switch_clear_flag(&kazoo_globals, LFLAG_RUNNING);

	/* give everyone time to cleanly shutdown */
	while (switch_atomic_read(&kazoo_globals.threads)) {
		switch_yield(100000);
		if (++sanity >= 200) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to kill all threads, continuing. This probably wont end well.....good luck!\n");
			break;
		}
	}

	/* close the connection to epmd and the acceptor */
	close_socketfd(&kazoo_globals.epmdfd);
	close_socket(&kazoo_globals.acceptor);

	/* remove all XML fetch agents */
	unbind_fetch_agents();

	if (kazoo_globals.event_filter) {
		switch_core_hash_destroy(&kazoo_globals.event_filter);
	}

	switch_thread_rwlock_wrlock(kazoo_globals.ei_nodes_lock);
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);
	switch_thread_rwlock_destroy(kazoo_globals.ei_nodes_lock);

	/* Close the port we reserved for uPnP/Switch behind firewall, if necessary */
	if (kazoo_globals.nat_map && switch_nat_get_type()) {
		switch_nat_del_mapping(kazoo_globals.port, SWITCH_NAT_TCP);
	}

	kazoo_destroy_config();

	/* clean up our allocated preferences */
	switch_safe_free(kazoo_globals.ip);
	switch_safe_free(kazoo_globals.ei_cookie);
	switch_safe_free(kazoo_globals.ei_nodename);

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
