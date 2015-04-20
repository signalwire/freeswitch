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

void mod_amqp_connection_close(mod_amqp_connection_t *connection)
{
	amqp_connection_state_t old_state = connection->state;
	int status = 0;

	connection->state = NULL;

	if (old_state != NULL) {
		mod_amqp_log_if_amqp_error(amqp_channel_close(old_state, 1, AMQP_REPLY_SUCCESS), "Closing channel");
		mod_amqp_log_if_amqp_error(amqp_connection_close(old_state, AMQP_REPLY_SUCCESS), "Closing connection");

		if ((status = amqp_destroy_connection(old_state))) {
			const char *errstr = amqp_error_string2(-status);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error destroying amqp connection: %s\n", errstr);
		}
	}
}

switch_status_t mod_amqp_connection_open(mod_amqp_connection_t *connections, mod_amqp_connection_t **active, char *profile_name, char *custom_attr)
{
	int channel_max = 0;
	int frame_max = 131072;
	amqp_table_t loginProperties;
	amqp_table_entry_t loginTableEntries[5];
	char hostname[64];
	int bHasHostname;
	char key_string[256] = {0};
	amqp_rpc_reply_t status;
	amqp_socket_t *socket = NULL;
	int amqp_status = -1;
	mod_amqp_connection_t *connection_attempt = NULL;
	amqp_connection_state_t newConnection = amqp_new_connection();
	amqp_connection_state_t oldConnection = NULL;

	if (active && *active) {
		oldConnection = (*active)->state;
	}

	/* Set up meta data for connection */
	bHasHostname = gethostname(hostname, sizeof(hostname)) == 0;

	loginProperties.num_entries = sizeof(loginTableEntries)/sizeof(*loginTableEntries);
	loginProperties.entries = loginTableEntries;

	snprintf(key_string, 256, "x_%s_HostMachineName", custom_attr);
	loginTableEntries[0].key = amqp_cstring_bytes(key_string);
	loginTableEntries[0].value.kind = AMQP_FIELD_KIND_BYTES;
	loginTableEntries[0].value.value.bytes = amqp_cstring_bytes(bHasHostname ? hostname : "(unknown)");

	snprintf(key_string, 256, "x_%s_ProcessDescription", custom_attr);
	loginTableEntries[1].key = amqp_cstring_bytes(key_string);
	loginTableEntries[1].value.kind = AMQP_FIELD_KIND_BYTES;
	loginTableEntries[1].value.value.bytes = amqp_cstring_bytes("FreeSwitch");

	snprintf(key_string, 256, "x_%s_ProcessType", custom_attr);
	loginTableEntries[2].key = amqp_cstring_bytes(key_string);
	loginTableEntries[2].value.kind = AMQP_FIELD_KIND_BYTES;
	loginTableEntries[2].value.value.bytes = amqp_cstring_bytes("TAP");

	snprintf(key_string, 256, "x_%s_ProcessBuildVersion", custom_attr);
	loginTableEntries[3].key = amqp_cstring_bytes(key_string);
	loginTableEntries[3].value.kind = AMQP_FIELD_KIND_BYTES;
	loginTableEntries[3].value.value.bytes = amqp_cstring_bytes(switch_version_full());

	snprintf(key_string, 256, "x_%s_Liquid_ProcessBuildBornOn", custom_attr);
	loginTableEntries[4].key = amqp_cstring_bytes(key_string);
	loginTableEntries[4].value.kind = AMQP_FIELD_KIND_BYTES;
	loginTableEntries[4].value.value.bytes = amqp_cstring_bytes(__DATE__ " " __TIME__);

	if (!(socket = amqp_tcp_socket_new(newConnection))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not create TCP socket\n");
		return SWITCH_STATUS_GENERR;
	}

	connection_attempt = connections;
	amqp_status = -1;

	while (connection_attempt && amqp_status){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] trying to connect to AMQP broker %s:%d\n",
						  profile_name, connection_attempt->hostname, connection_attempt->port);

		if ((amqp_status = amqp_socket_open(socket, connection_attempt->hostname, connection_attempt->port))){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Could not open socket connection to AMQP broker %s:%d status(%d) %s\n",
							  connection_attempt->hostname, connection_attempt->port,	amqp_status, amqp_error_string2(amqp_status));
			connection_attempt = connection_attempt->next;
		}
	}

	*active = connection_attempt;

	if (!connection_attempt) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] could not connect to any AMQP brokers\n", profile_name);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] opened socket connection to AMQP broker %s:%d\n",
					  profile_name, connection_attempt->hostname, connection_attempt->port);

	/* We have a connection, now log in */
	status = amqp_login_with_properties(newConnection,
										connection_attempt->virtualhost,
										channel_max,
										frame_max,
										connection_attempt->heartbeat,
										&loginProperties,
										AMQP_SASL_METHOD_PLAIN,
										connection_attempt->username,
										connection_attempt->password);

	if (mod_amqp_log_if_amqp_error(status, "Logging in")) {
		mod_amqp_close_connection(*active);
		*active = NULL;
		return SWITCH_STATUS_GENERR;
	}

	// Open a channel (1). This is fairly standard
	amqp_channel_open(newConnection, 1);
	if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(newConnection), "Opening channel")) {
		return SWITCH_STATUS_GENERR;
	}

	(*active)->state = newConnection;

	if (oldConnection) {
		amqp_destroy_connection(oldConnection);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_amqp_connection_create(mod_amqp_connection_t **conn, switch_xml_t cfg, switch_memory_pool_t *pool)
{
	mod_amqp_connection_t *new_con = switch_core_alloc(pool, sizeof(mod_amqp_connection_t));
	switch_xml_t param;
	char *name = (char *) switch_xml_attr_soft(cfg, "name");
	char *hostname = NULL, *virtualhost = NULL, *username = NULL, *password = NULL;
	unsigned int port = 0, heartbeat = 0;

	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Connection missing name attribute\n%s\n", switch_xml_toxml(cfg, 1));
		return SWITCH_STATUS_GENERR;
	}

	new_con->name = switch_core_strdup(pool, name);
	new_con->state = NULL;
	new_con->next = NULL;

	for (param = switch_xml_child(cfg, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");

		if (!var) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "AMQP connection[%s] param missing 'name' attribute\n", name);
			continue;
		}

		if (!val) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "AMQP connection[%s] param[%s] missing 'value' attribute\n", name, var);
			continue;
		}

		if (!strncmp(var, "hostname", 8)) {
			hostname = switch_core_strdup(pool, val);
		} else if (!strncmp(var, "virtualhost", 11)) {
			virtualhost = switch_core_strdup(pool, val);
		} else if (!strncmp(var, "username", 8)) {
			username = switch_core_strdup(pool, val);
		} else if (!strncmp(var, "password", 8)) {
			password = switch_core_strdup(pool, val);
		} else if (!strncmp(var, "port", 4)) {
			int interval = atoi(val);
			if (interval && interval > 0) {
				port = interval;
			}
		} else if (!strncmp(var, "heartbeat", 4)) {
			int interval = atoi(val);
			if (interval && interval > 0) {
				heartbeat = interval;
			}
		}
	}

	new_con->hostname = hostname ? hostname : "localhost";
	new_con->virtualhost = virtualhost ? virtualhost : "/";
	new_con->username = username ? username : "guest";
	new_con->password = password ? password : "guest";
	new_con->port = port ? port : 5672;
	new_con->heartbeat = heartbeat ? heartbeat : 0;

	*conn = new_con;
	return SWITCH_STATUS_SUCCESS;
}

void mod_amqp_connection_destroy(mod_amqp_connection_t **conn)
{
	if (conn && *conn) {
		*conn = NULL;
	}
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
