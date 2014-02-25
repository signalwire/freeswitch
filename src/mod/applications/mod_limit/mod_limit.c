/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Rupa Schomaker <rupa@rupa.com>
 *
 *
 * mod_limit.c -- backward compat module for transition to new core limits
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_limit_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_limit_load);

SWITCH_MODULE_DEFINITION(mod_limit, mod_limit_load, mod_limit_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_limit_load)
{
	const char *err = NULL;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Loading mod_limit - a shim for backwards compatability with the new limit system.  This is deprecated, remove mod_limit and instead load mod_hash and mod_db!\n");
	
	/* try to load mod_hash if it isn't loaded */
	if (switch_loadable_module_exists("mod_hash") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "mod_hash not loaded, trying to load...!\n");
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_hash", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load mod_hash (%s)!\n", err);
		}
	}
	
	/* try to load mod_db if it isn't loaded */
	if (switch_loadable_module_exists("mod_db") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "mod_db not loaded, trying to load...!\n");
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_db", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load mod_db (%s)!\n", err);
		}
	}
	
	/* set compat flag */
	switch_core_set_variable("switch_limit_backwards_compat_flag", "true");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_limit_shutdown)
{
	switch_core_set_variable("switch_limit_backwards_compat_flag", "false");
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
