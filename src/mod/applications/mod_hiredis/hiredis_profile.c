/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
* mod_hiredis.c -- redis client built using the C client library hiredis
*
*/

#include <mod_hiredis.h>
	
switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t port)
{
	hiredis_profile_t *profile = NULL;
	switch_memory_pool_t *pool = NULL;
	
  	switch_core_new_memory_pool(&pool);

	profile = switch_core_alloc(pool, sizeof(hiredis_profile_t));

	profile->pool = pool;
	profile->name = name ? switch_core_strdup(profile->pool, name) : "default";
	profile->conn = NULL;
	profile->conn_head = NULL;

	switch_core_hash_insert(mod_hiredis_globals.profiles, name, (void *) profile);

	*new_profile = profile;
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t hiredis_profile_destroy(hiredis_profile_t **old_profile)
{
	hiredis_profile_t *profile = NULL;

	if ( !old_profile || !*old_profile ) {
		return SWITCH_STATUS_SUCCESS;
	} else {
		profile = *old_profile;
		*old_profile = NULL;
	}

	switch_core_hash_delete(mod_hiredis_globals.profiles, profile->name);
	switch_core_destroy_memory_pool(&(profile->pool));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t hiredis_profile_connection_add(hiredis_profile_t *profile, char *host, char *password, uint32_t port, uint32_t timeout_ms)
{
	hiredis_connection_t *connection = NULL, *new_conn = NULL;

	new_conn = switch_core_alloc(profile->pool, sizeof(hiredis_connection_t));
	new_conn->host = host ? switch_core_strdup(profile->pool, host) : "localhost";
	new_conn->password = password ? switch_core_strdup(profile->pool, password) : NULL;
	new_conn->port = port ? port : 6379;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "hiredis: adding conn[%d]\n", new_conn->port);

	if ( timeout_ms ) {
		new_conn->timeout.tv_sec = 0;
		new_conn->timeout.tv_usec = timeout_ms * 1000;
	} else {
		new_conn->timeout.tv_sec = 0;
		new_conn->timeout.tv_usec = 500 * 1000;
	}
	
	if ( profile->conn_head != NULL ){
		/* Adding 'another' connection */
		connection = profile->conn_head;
		while ( connection->next != NULL ){
			connection = connection->next;
		}
		connection->next = new_conn;
	} else {
		profile->conn_head = new_conn;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t hiredis_profile_reconnect(hiredis_profile_t *profile)
{
	hiredis_connection_t *conn = profile->conn_head;
	profile->conn = NULL;

	/* TODO: Needs thorough expansion to handle all disconnection scenarios */
	
	while ( conn ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hiredis: attempting[%s, %d]\n", conn->host, conn->port);
		conn->context = redisConnectWithTimeout(conn->host, conn->port, conn->timeout);
	
		if ( conn->context && conn->context->err) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: connection error[%s]\n", conn->context->errstr);
			conn = conn->next;
			continue;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hiredis: connection success[%s]\n", conn->host);
			
		/* successful redis connection */
		profile->conn = conn;
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: unable to reconnect\n");
	return SWITCH_STATUS_GENERR;
}

switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, const char *data, char **resp)
{
	char *str = NULL;
	redisReply *response = NULL;

	/* Check connection */
	if ( !profile->conn && hiredis_profile_reconnect(profile) != SWITCH_STATUS_SUCCESS ) {
		*resp = strdup("hiredis profile unable to establish connection");
		return SWITCH_STATUS_GENERR;
	}
	
	response = redisCommand(profile->conn->context, data);

	if ( !response ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: empty response received\n");
		return SWITCH_STATUS_GENERR;
	}
	
	switch(response->type) {
	case REDIS_REPLY_STATUS: /* fallthrough */
	case REDIS_REPLY_STRING:
		str = strdup(response->str);
		break;
	case REDIS_REPLY_INTEGER:
		str = switch_mprintf("%lld", response->integer);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: response error[%s][%d]\n", response->str, response->type);
		freeReplyObject(response);
		return SWITCH_STATUS_GENERR;
	}

	freeReplyObject(response);

	*resp = str;
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
