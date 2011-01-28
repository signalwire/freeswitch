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
#include <switch_version.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "subagent.h"


void init_subagent(void)
{
	static oid identity_oid[] = { 1,3,6,1,4,1,27880,1,1 };
	static oid systemStats_oid[] = { 1,3,6,1,4,1,27880,1,2 };

	DEBUGMSGTL(("init_subagent", "Initializing\n"));

	netsnmp_register_scalar_group(netsnmp_create_handler_registration("identity", handle_identity, identity_oid, OID_LENGTH(identity_oid), HANDLER_CAN_RONLY), 1, 2);
	netsnmp_register_scalar_group(netsnmp_create_handler_registration("systemStats", handle_systemStats, systemStats_oid, OID_LENGTH(systemStats_oid), HANDLER_CAN_RONLY), 1, 7);
}


static int sql_count_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	uint32_t *count = (uint32_t *) pArg;
	*count = atoi(argv[0]);
	return 0;
}


int handle_identity(netsnmp_mib_handler *handler, netsnmp_handler_registration *reginfo, netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests)
{
	netsnmp_request_info *request = NULL;
	oid subid;
	static char const version[] = SWITCH_VERSION_FULL;
	char uuid[40] = "";

	switch(reqinfo->mode) {
	case MODE_GET:
		subid = requests->requestvb->name[reginfo->rootoid_len - 2];

		switch (subid) {
		case ID_VERSION_STR:
			snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, (u_char *) &version, strlen(version));
			break;
		case ID_UUID:
			strncpy(uuid, switch_core_get_uuid(), sizeof(uuid));
			snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, (u_char *) &uuid, strlen(uuid));
			break;
		default:
			snmp_log(LOG_WARNING, "Unregistered OID-suffix requested (%d)\n", (int) subid);
			netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
		}
		break;

	default:
		/* we should never get here, so this is a really bad error */
		snmp_log(LOG_ERR, "Unknown mode (%d) in handle_identity\n", reqinfo->mode );
		return SNMP_ERR_GENERR;
	}

	return SNMP_ERR_NOERROR;
}


int handle_systemStats(netsnmp_mib_handler *handler, netsnmp_handler_registration *reginfo, netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests)
{
	netsnmp_request_info *request = NULL;
	oid subid;
	switch_time_t uptime;
	uint32_t int_val;

	switch(reqinfo->mode) {
	case MODE_GET:
		subid = requests->requestvb->name[reginfo->rootoid_len - 2];

		switch (subid) {
		case SS_UPTIME:
			uptime = switch_core_uptime() / 10000;
			snmp_set_var_typed_value(requests->requestvb, ASN_TIMETICKS, (u_char *) &uptime, sizeof(uptime));
			break;
		case SS_SESSIONS_SINCE_STARTUP:
			int_val = switch_core_session_id() - 1;
			snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER, (u_char *) &int_val, sizeof(int_val));
			break;
		case SS_CURRENT_SESSIONS:
			int_val = switch_core_session_count();
			snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char *) &int_val, sizeof(int_val));
			break;
		case SS_MAX_SESSIONS:
			switch_core_session_ctl(SCSC_MAX_SESSIONS, &int_val);
			snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char *) &int_val, sizeof(int_val));
			break;
		case SS_CURRENT_CALLS:
			{
			switch_cache_db_handle_t *dbh;
			char sql[1024] = "", hostname[256] = "";

			if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
				return SNMP_ERR_GENERR;
			}

			gethostname(hostname, sizeof(hostname));
			sprintf(sql, "SELECT COUNT(*) FROM calls WHERE hostname='%s'", hostname);
			switch_cache_db_execute_sql_callback(dbh, sql, sql_count_callback, &int_val, NULL);
			snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char *) &int_val, sizeof(int_val));
			switch_cache_db_release_db_handle(&dbh);
			}
			break;
		case SS_SESSIONS_PER_SECOND:
			switch_core_session_ctl(SCSC_LAST_SPS, &int_val);
			snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char *) &int_val, sizeof(int_val));
			break;
		case SS_MAX_SESSIONS_PER_SECOND:
			switch_core_session_ctl(SCSC_SPS, &int_val);
			snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE, (u_char *) &int_val, sizeof(int_val));
			break;
		default:
			snmp_log(LOG_WARNING, "Unregistered OID-suffix requested (%d)\n", (int) subid);
			netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
		}
		break;

	default:
		/* we should never get here, so this is a really bad error */
		snmp_log(LOG_ERR, "Unknown mode (%d) in handle_systemStats\n", reqinfo->mode);
		return SNMP_ERR_GENERR;
	}

	return SNMP_ERR_NOERROR;
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
