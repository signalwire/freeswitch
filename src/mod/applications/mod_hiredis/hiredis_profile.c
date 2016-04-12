/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2016, Anthony Minessale II <anthm@freeswitch.org>
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
* Christopher Rienzo <chris.rienzo@citrix.com>
*
* mod_hiredis.c -- redis client built using the C client library hiredis
*
*/

#include <mod_hiredis.h>

/* reconnect to redis server */
static switch_status_t hiredis_context_reconnect(hiredis_context_t *context)
{
	redisFree(context->context);
	context->context = redisConnectWithTimeout(context->connection->host, context->connection->port, context->connection->timeout);
	if ( context->context && !context->context->err ) {
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

/* Return a context back to the pool */
static void hiredis_context_release(hiredis_context_t *context, switch_core_session_t *session)
{
	if (switch_queue_push(context->connection->context_pool, context) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "hiredis: failed to release back to pool [%s, %d]\n", context->connection->host, context->connection->port);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: release back to pool [%s, %d]\n", context->connection->host, context->connection->port);
	}
}

/* Grab a context from the pool, reconnect/connect as needed */
static hiredis_context_t *hiredis_connection_get_context(hiredis_connection_t *conn, switch_core_session_t *session)
{
	void *val = NULL;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: waiting for [%s, %d]\n", conn->host, conn->port);
	if ( switch_queue_pop_timeout(conn->context_pool, &val, conn->timeout_us ) == SWITCH_STATUS_SUCCESS ) {
		hiredis_context_t *context = (hiredis_context_t *)val;
		if ( !context->context ) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: attempting[%s, %d]\n", conn->host, conn->port);
			context->context = redisConnectWithTimeout(conn->host, conn->port, conn->timeout);
			if ( context->context && !context->context->err ) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: connection success[%s, %d]\n", conn->host, conn->port);
				return context;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: connection error[%s, %d] (%s)\n", conn->host, conn->port, context->context->errstr);
				hiredis_context_release(context, session);
				return NULL;
			}
		} else if ( context->context->err ) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: reconnecting[%s, %d]\n", conn->host, conn->port);
			if (hiredis_context_reconnect(context) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: reconnection success[%s, %d]\n", conn->host, conn->port);
				return context;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: reconnection error[%s, %d] (%s)\n", conn->host, conn->port, context->context->errstr);
				hiredis_context_release(context, session);
				return NULL;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: recycled from pool[%s, %d]\n", conn->host, conn->port);
			return context;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: timed out waiting for [%s, %d]\n", conn->host, conn->port);
	}

	return NULL;
}

switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t ignore_connect_fail)
{
	hiredis_profile_t *profile = NULL;
	switch_memory_pool_t *pool = NULL;
	
	switch_core_new_memory_pool(&pool);

	profile = switch_core_alloc(pool, sizeof(hiredis_profile_t));

	profile->pool = pool;
	profile->name = name ? switch_core_strdup(profile->pool, name) : "default";
	profile->conn_head = NULL;
	profile->ignore_connect_fail = ignore_connect_fail;

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

switch_status_t hiredis_profile_connection_add(hiredis_profile_t *profile, char *host, char *password, uint32_t port, uint32_t timeout_ms, uint32_t max_contexts)
{
	hiredis_connection_t *connection = NULL, *new_conn = NULL;

	new_conn = switch_core_alloc(profile->pool, sizeof(hiredis_connection_t));
	new_conn->host = host ? switch_core_strdup(profile->pool, host) : "localhost";
	new_conn->password = password ? switch_core_strdup(profile->pool, password) : NULL;
	new_conn->port = port ? port : 6379;
	new_conn->pool = profile->pool;

	/* create fixed size context pool */
	max_contexts = max_contexts > 0 ? max_contexts : 3;
	if (switch_queue_create(&new_conn->context_pool, max_contexts, new_conn->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "hiredis: failed to allocate context pool\n");
		return SWITCH_STATUS_GENERR;
	} else {
		int i = 0;
		for (i = 0; i < max_contexts; i++) {
			hiredis_context_t *new_context = switch_core_alloc(new_conn->pool, sizeof(hiredis_context_t));
			new_context->connection = new_conn;
			new_context->context = NULL;
			switch_queue_push(new_conn->context_pool, new_context);
		}
	}

	if ( timeout_ms ) {
		new_conn->timeout_us = timeout_ms * 1000;
		new_conn->timeout.tv_sec = timeout_ms / 1000;
		new_conn->timeout.tv_usec = (timeout_ms % 1000) * 1000;
	} else {
		new_conn->timeout_us = 500 * 1000;
		new_conn->timeout.tv_sec = 0;
		new_conn->timeout.tv_usec = 500 * 1000;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "hiredis: adding conn[%s,%d], pool size = %d\n", new_conn->host, new_conn->port, max_contexts);

	if ( profile->conn_head != NULL ){
		/* Adding 'another' connection */
		connection = profile->conn_head;
		while ( connection->next != NULL ) {
			connection = connection->next;
		}
		connection->next = new_conn;
	} else {
		profile->conn_head = new_conn;
	}

	return SWITCH_STATUS_SUCCESS;
}

static hiredis_context_t *hiredis_profile_get_context(hiredis_profile_t *profile, hiredis_connection_t *initial_conn, switch_core_session_t *session)
{
	hiredis_connection_t *conn = initial_conn ? initial_conn : profile->conn_head;
	hiredis_context_t *context;

	while ( conn ) {
		context = hiredis_connection_get_context(conn, session);
		if (context) {
			/* successful redis connection */
			return context;
		}
		conn = conn->next;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: unable to connect\n");
	return NULL;
}

static switch_status_t hiredis_context_execute_sync(hiredis_context_t *context, const char *data, char **resp, switch_core_session_t *session)
{
	redisReply *response;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: %s\n", data);
	response = redisCommand(context->context, data);
	if ( !response ) {
		*resp = NULL;
		return SWITCH_STATUS_GENERR;
	}

	switch(response->type) {
	case REDIS_REPLY_STATUS: /* fallthrough */
	case REDIS_REPLY_STRING:
		*resp = strdup(response->str);
		break;
	case REDIS_REPLY_INTEGER:
		*resp = switch_mprintf("%lld", response->integer);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: response error[%s][%d]\n", response->str, response->type);
		freeReplyObject(response);
		*resp = NULL;
		return SWITCH_STATUS_GENERR;
	}

	freeReplyObject(response);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, const char *data, char **resp, switch_core_session_t *session)
{
	hiredis_context_t *context = NULL;
	int reconnected = 0;

	context = hiredis_profile_get_context(profile, NULL, session);
	while (context) {
		if (hiredis_context_execute_sync(context, data, resp, session) == SWITCH_STATUS_SUCCESS) {
			/* got result */
			hiredis_context_release(context, session);
			return SWITCH_STATUS_SUCCESS;
		} else if (context->context->err) {
			/* have a bad connection, try a single reconnect attempt before moving on to alternate connection */
			if (reconnected || hiredis_context_reconnect(context) != SWITCH_STATUS_SUCCESS) {
				/* try alternate connection */
				hiredis_context_t *new_context = hiredis_profile_get_context(profile, context->connection, session);
				hiredis_context_release(context, session);
				context = new_context;
				if (context) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: got alternate connection to [%s, %d]\n", context->connection->host, context->connection->port);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: no more alternate connections to try\n");
				}
				reconnected = 0;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: reconnection success[%s, %d]\n", context->connection->host, context->connection->port);
				reconnected = 1;
			}
		} else {
			/* no problem with context, so don't retry */
			hiredis_context_release(context, session);
			return SWITCH_STATUS_GENERR;
		}
	}
	return SWITCH_STATUS_SOCKERR;
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
