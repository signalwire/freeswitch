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

netsnmp_table_registration_info *ch_table_info;
netsnmp_tdata *ch_table;
netsnmp_handler_registration *ch_reginfo;
uint32_t idx;


static void time_t_to_datetime(time_t epoch, char *buf, switch_size_t buflen)
{
	struct tm *dt;
	uint16_t year;

	dt = gmtime(&epoch);
	year = dt->tm_year + 1900;
	switch_snprintf(buf, buflen, "%c%c%c%c%c%c%c%c+%c%c", year >> 8, year & 0xff, dt->tm_mon + 1, dt->tm_mday, dt->tm_hour, dt->tm_min, dt->tm_sec, 0, 0, 0);
}


static int sql_count_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	uint32_t *count = (uint32_t *) pArg;
	*count = atoi(argv[0]);
	return 0;
}


static int channelList_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	chan_entry_t *entry;
	netsnmp_tdata_row *row;

	switch_zmalloc(entry, sizeof(chan_entry_t));

	row = netsnmp_tdata_create_row();

	if (!row) {
		switch_safe_free(entry);
		return 0;
	}

	row->data = entry;

	entry->idx = idx++;
	strncpy(entry->uuid, switch_str_nil(argv[0]), sizeof(entry->uuid));
	strncpy(entry->direction, switch_str_nil(argv[1]), sizeof(entry->direction));
	entry->created_epoch = atoi(argv[3]);
	strncpy(entry->name, switch_str_nil(argv[4]), sizeof(entry->name));
	strncpy(entry->state, switch_str_nil(argv[5]), sizeof(entry->state));
	strncpy(entry->cid_name, switch_str_nil(argv[6]), sizeof(entry->cid_name));
	strncpy(entry->cid_num, switch_str_nil(argv[7]), sizeof(entry->cid_num));
	strncpy(entry->dest, switch_str_nil(argv[9]), sizeof(entry->dest));
	strncpy(entry->application, switch_str_nil(argv[10]), sizeof(entry->application));
	strncpy(entry->application_data, switch_str_nil(argv[11]), sizeof(entry->application_data));
	strncpy(entry->dialplan, switch_str_nil(argv[12]), sizeof(entry->dialplan));
	strncpy(entry->context, switch_str_nil(argv[13]), sizeof(entry->context));
	strncpy(entry->read_codec, switch_str_nil(argv[14]), sizeof(entry->read_codec));
	entry->read_rate = atoi(switch_str_nil(argv[15]));
	entry->read_bitrate = atoi(switch_str_nil(argv[16]));
	strncpy(entry->write_codec, switch_str_nil(argv[17]), sizeof(entry->write_codec));
	entry->write_rate = atoi(switch_str_nil(argv[18]));
	entry->write_bitrate = atoi(switch_str_nil(argv[19]));

	memset(&entry->ip_addr, 0, sizeof(entry->ip_addr));
	if (strchr(switch_str_nil(argv[8]), ':')) {
		switch_inet_pton(AF_INET6, switch_str_nil(argv[8]), &entry->ip_addr);
		entry->addr_family = AF_INET6;
	} else {
		switch_inet_pton(AF_INET, switch_str_nil(argv[8]), &entry->ip_addr);
		entry->addr_family = AF_INET;
	}

	netsnmp_tdata_row_add_index(row, ASN_INTEGER, &entry->idx, sizeof(entry->idx));
	netsnmp_tdata_add_row(ch_table, row);
	return 0;
}


void channelList_free(netsnmp_cache *cache, void *magic)
{
	netsnmp_tdata_row *row = netsnmp_tdata_row_first(ch_table);

	/* Delete table rows one by one */
	while (row) {
		netsnmp_tdata_remove_and_delete_row(ch_table, row);
		switch_safe_free(row->data);
		row = netsnmp_tdata_row_first(ch_table);
	}
}


int channelList_load(netsnmp_cache *cache, void *vmagic)
{
	switch_cache_db_handle_t *dbh;
	char sql[1024] = "";

	channelList_free(cache, NULL);

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		return 0;
	}

	idx = 1;

	sprintf(sql, "SELECT * FROM channels WHERE hostname='%s' ORDER BY created_epoch", switch_core_get_switchname());
	switch_cache_db_execute_sql_callback(dbh, sql, channelList_callback, NULL, NULL);

	switch_cache_db_release_db_handle(&dbh);

	return 0;
}


void init_subagent(switch_memory_pool_t *pool)
{
	static oid identity_oid[] = { 1,3,6,1,4,1,27880,1,1 };
	static oid systemStats_oid[] = { 1,3,6,1,4,1,27880,1,2 };
	static oid channelList_oid[] = { 1,3,6,1,4,1,27880,1,9 };

	DEBUGMSGTL(("init_subagent", "mod_snmp subagent initializing\n"));

	netsnmp_register_scalar_group(netsnmp_create_handler_registration("identity", handle_identity, identity_oid, OID_LENGTH(identity_oid), HANDLER_CAN_RONLY), 1, 2);
	netsnmp_register_scalar_group(netsnmp_create_handler_registration("systemStats", handle_systemStats, systemStats_oid, OID_LENGTH(systemStats_oid), HANDLER_CAN_RONLY), 1, 11);

	ch_table_info = switch_core_alloc(pool, sizeof(netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes(ch_table_info, ASN_INTEGER, 0);
	ch_table_info->min_column = CH_INDEX;
	ch_table_info->max_column = CH_WRITE_BITRATE;
	ch_table = netsnmp_tdata_create_table("channelList", 0);
	ch_reginfo = netsnmp_create_handler_registration("channelList", handle_channelList, channelList_oid, OID_LENGTH(channelList_oid), HANDLER_CAN_RONLY);
	netsnmp_tdata_register(ch_reginfo, ch_table, ch_table_info);
	netsnmp_inject_handler(ch_reginfo, netsnmp_get_cache_handler(5, channelList_load, channelList_free, channelList_oid, OID_LENGTH(channelList_oid)));
}


int handle_identity(netsnmp_mib_handler *handler, netsnmp_handler_registration *reginfo, netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests)
{
	netsnmp_request_info *request = NULL;
	oid subid;
	const char *version = switch_version_full();
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
	uint32_t int_val = 0;

	switch(reqinfo->mode) {
	case MODE_GET:
		subid = requests->requestvb->name[reginfo->rootoid_len - 2];
		snmp_log(LOG_DEBUG, "systemStats OID-suffix requested (%d)\n", (int) subid);

		switch (subid) {
		case SS_UPTIME:
			uptime = switch_core_uptime() / 10000;
			snmp_set_var_typed_value(requests->requestvb, ASN_TIMETICKS, (u_char *) &uptime, sizeof(uptime));
			break;
		case SS_SESSIONS_SINCE_STARTUP:
			int_val = switch_core_session_id() - 1;
			snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER, int_val);
			break;
		case SS_CURRENT_SESSIONS:
			int_val = switch_core_session_count();
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_MAX_SESSIONS:
			switch_core_session_ctl(SCSC_MAX_SESSIONS, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_CURRENT_CALLS:
			{
			switch_cache_db_handle_t *dbh;
			char sql[1024] = "";

			if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
				return SNMP_ERR_GENERR;
			}

			sprintf(sql, "SELECT COUNT(*) FROM calls WHERE hostname='%s'", switch_core_get_switchname());
			switch_cache_db_execute_sql_callback(dbh, sql, sql_count_callback, &int_val, NULL);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			switch_cache_db_release_db_handle(&dbh);
			}
			break;
		case SS_SESSIONS_PER_SECOND:
			switch_core_session_ctl(SCSC_LAST_SPS, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_MAX_SESSIONS_PER_SECOND:
			switch_core_session_ctl(SCSC_SPS, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_PEAK_SESSIONS_PER_SECOND:
			switch_core_session_ctl(SCSC_SPS_PEAK, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_PEAK_SESSIONS_PER_FIVEMIN:
			switch_core_session_ctl(SCSC_SPS_PEAK_FIVEMIN, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_PEAK_SESSIONS:
			switch_core_session_ctl(SCSC_SESSIONS_PEAK, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
			break;
		case SS_PEAK_SESSIONS_FIVEMIN:
			switch_core_session_ctl(SCSC_SESSIONS_PEAK_FIVEMIN, &int_val);
			snmp_set_var_typed_integer(requests->requestvb, ASN_GAUGE, int_val);
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


int handle_channelList(netsnmp_mib_handler *handler, netsnmp_handler_registration *reginfo, netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	chan_entry_t *entry;
	char dt_str[12];

	switch (reqinfo->mode) {
	case MODE_GET:
		for (request = requests; request; request = request->next) {
			if (request->processed)
				continue;

			table_info = netsnmp_extract_table_info(request);
			entry = (chan_entry_t *) netsnmp_tdata_extract_entry(request);

			switch (table_info->colnum) {
			case CH_INDEX:
				snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, entry->idx);
				break;
			case CH_UUID:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->uuid, strlen(entry->uuid));
				break;
			case CH_DIRECTION:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->direction, strlen(entry->direction));
				break;
			case CH_CREATED:
				time_t_to_datetime(entry->created_epoch, (char *) &dt_str, sizeof(dt_str));
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) &dt_str, sizeof(dt_str));
				break;
			case CH_NAME:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->name, strlen(entry->name));
				break;
			case CH_STATE:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->state, strlen(entry->state));
				break;
			case CH_CID_NAME:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->cid_name, strlen(entry->cid_name));
				break;
			case CH_CID_NUM:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->cid_num, strlen(entry->cid_num));
				break;
			case CH_IP_ADDR_TYPE:
				if (entry->addr_family == AF_INET6) {
					snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, INETADDRESSTYPE_IPV6);
				} else {
					snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, INETADDRESSTYPE_IPV4);
				}
				break;
			case CH_IP_ADDR:
				if (entry->addr_family == AF_INET6) {
					snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) &entry->ip_addr.v6, sizeof(entry->ip_addr.v6));
				} else {
					snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) &entry->ip_addr.v4, sizeof(entry->ip_addr.v4));
				}
				break;
			case CH_DEST:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->dest, strlen(entry->dest));
				break;
			case CH_APPLICATION:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->application, strlen(entry->application));
				break;
			case CH_APPLICATION_DATA:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->application_data, strlen(entry->application_data));
				break;
			case CH_DIALPLAN:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->dialplan, strlen(entry->dialplan));
				break;
			case CH_CONTEXT:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->context, strlen(entry->context));
				break;
			case CH_READ_CODEC:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->read_codec, strlen(entry->read_codec));
				break;
			case CH_READ_RATE:
				snmp_set_var_typed_value(request->requestvb, ASN_GAUGE, (u_char *) &entry->read_rate, sizeof(entry->read_rate));
				break;
			case CH_READ_BITRATE:
				snmp_set_var_typed_value(request->requestvb, ASN_GAUGE, (u_char *) &entry->read_bitrate, sizeof(entry->read_bitrate));
				break;
			case CH_WRITE_CODEC:
				snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR, (u_char *) entry->write_codec, strlen(entry->write_codec));
				break;
			case CH_WRITE_RATE:
				snmp_set_var_typed_value(request->requestvb, ASN_GAUGE, (u_char *) &entry->write_rate, sizeof(entry->write_rate));
				break;
			case CH_WRITE_BITRATE:
				snmp_set_var_typed_value(request->requestvb, ASN_GAUGE, (u_char *) &entry->write_bitrate, sizeof(entry->write_bitrate));
				break;
			default:
				snmp_log(LOG_WARNING, "Unregistered OID-suffix requested (%d)\n", table_info->colnum);
				netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
			}
		}
		break;
	default:
		/* we should never get here, so this is a really bad error */
		snmp_log(LOG_ERR, "Unknown mode (%d) in handle_channelList\n", reqinfo->mode );
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
