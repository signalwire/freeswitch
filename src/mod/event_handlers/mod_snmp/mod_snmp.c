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
 * Portions created by Seventh Signal Ltd. & Co. KG and its employees are Copyright (C)
 * Seventh Signal Ltd. & Co. KG, All Rights Reserverd.
 *
 * Contributor(s):
 * Daniel Swarbrick <daniel.swarbrick@gmail.com>
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
	switch_mutex_t *mutex;
	int shutdown;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_snmp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snmp_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_snmp_runtime);
SWITCH_MODULE_DEFINITION(mod_snmp, mod_snmp_load, mod_snmp_shutdown, mod_snmp_runtime);


static switch_status_t snmp_manage(char *relative_oid, switch_management_action_t action, char *data, switch_size_t datalen)
{
	if (action == SMA_GET) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Mutex lock request from relative OID %s.\n", relative_oid);
		switch_mutex_lock(globals.mutex);
	} else if (action == SMA_SET) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Mutex unlock request from relative OID %s.\n", relative_oid);
		switch_mutex_unlock(globals.mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}


static int snmp_callback_log(int major, int minor, void *serverarg, void *clientarg)
{
	struct snmp_log_message *slm = (struct snmp_log_message *) serverarg;
	switch_log_printf(SWITCH_CHANNEL_LOG, slm->priority, "%s", slm->msg);
	return SNMP_ERR_NOERROR;
}


static switch_status_t load_config(switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_snmp_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_management_interface_t *management_interface;

	load_config(pool);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	management_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_MANAGEMENT_INTERFACE);
	management_interface->relative_oid = "1000";
	management_interface->management_function = snmp_manage;

	/* Register callback function so we get Net-SNMP logging handled by FreeSWITCH */
	snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, snmp_callback_log, NULL);
	snmp_enable_calllog();

	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
	netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

	init_agent("mod_snmp");

	/*
	 * Override master/subagent ping interval to 2s, to ensure that
	 * agent_check_and_process() never blocks for longer than that.
	 */
	netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_PING_INTERVAL, 2);

	init_subagent(pool);
	init_snmp("mod_snmp");

	return status;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_snmp_runtime)
{
	if (!globals.shutdown) {
		switch_mutex_lock(globals.mutex);
		/* Block on select() */
		agent_check_and_process(1);
		switch_mutex_unlock(globals.mutex);
	}

	switch_yield(5000);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_snmp_shutdown)
{
	globals.shutdown = 1;

	switch_mutex_lock(globals.mutex);
	snmp_shutdown("mod_snmp");
	switch_mutex_unlock(globals.mutex);

	switch_mutex_destroy(globals.mutex);

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
