/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 * Copyright 2006, Author: Yossi Neiman of Cartis Solutions, Inc. <freeswitch AT cartissolutions.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 *
 * The Initial Developer of the Original Code is
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 *
 * Description: This source file describes the most basic portions of the CDR module.  These are the functions
 * and structures that the Freeswitch core looks for when opening up the DSO file to create the load, shutdown
 * and runtime threads as necessary.
 *
 * mod_cdr.cpp
 *
 */

#include "cdrcontainer.h"
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char modname[] = "mod_cdr - CDR Engine";
static int RUNNING = 0;
static CDRContainer *newcdrcontainer;
static switch_memory_pool_t *module_pool;
static switch_status_t my_on_hangup(switch_core_session_t *session);

/* Now begins the glue that will tie this into the system.
*/

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

static const switch_loadable_module_interface_t cdr_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	newcdrcontainer->add_cdr(session);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &cdr_module_interface;
	
	switch_core_add_state_handler(&state_handlers);
	
	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) 
	{
		switch_console_printf(SWITCH_CHANNEL_LOG, "OH OH - Can't swim, no pool\n");
		return SWITCH_STATUS_TERM;
	}

	newcdrcontainer = new CDRContainer(module_pool);  // Instantiates the new object, automatically loads config
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	RUNNING = 1;
	switch_console_printf(SWITCH_CHANNEL_LOG, "mod_cdr made it to runtime.  Wee!\n");
	newcdrcontainer->process_records();
	
	return RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_TERM;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	delete newcdrcontainer;
	return SWITCH_STATUS_SUCCESS;
}
