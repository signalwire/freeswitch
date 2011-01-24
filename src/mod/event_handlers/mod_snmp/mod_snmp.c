/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * Daniel Swarbrick <daniel.swarbrick@seventhsignal.de>
 * Stefan Knoblich <s.knoblich@axsentis.de>
 * 
 * mod_snmp.c -- SNMP AgentX Subagent Module
 *
 */
#include <switch.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "subagent.h"

static struct {
	switch_memory_pool_t *pool;
	int shutdown;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_snmp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snmp_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_snmp_runtime);
SWITCH_MODULE_DEFINITION(mod_snmp, mod_snmp_load, mod_snmp_shutdown, mod_snmp_runtime);


static int snmp_callback_log(int major, int minor, void *serverarg, void *clientarg)
{
	struct snmp_log_message *slm = (struct snmp_log_message *) serverarg;
	switch_log_printf(SWITCH_CHANNEL_LOG, slm->priority, "%s", slm->msg);
	return SNMP_ERR_NOERROR;
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL
};


static switch_status_t load_config(switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_snmp_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	load_config(pool);

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* Register callback function so we get Net-SNMP logging handled by FreeSWITCH */
	snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, snmp_callback_log, NULL);
	snmp_enable_calllog();

	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
	netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

	init_agent("mod_snmp");
	init_subagent();  
	init_snmp("mod_snmp");

	return status;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_snmp_runtime)
{
	/* block on select() */
	agent_check_and_process(1);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snmp_shutdown)
{
	globals.shutdown = 1;
	switch_core_remove_state_handler(&state_handlers);

	snmp_shutdown("mod_snmp");

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
