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

/* auth if password is set */
static switch_status_t hiredis_context_auth(hiredis_context_t *context)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	if ( !zstr(context->connection->password) ) {
		redisReply *response = redisCommand(context->context, "AUTH %s", context->connection->password);
		if ( !response || response->type == REDIS_REPLY_ERROR ) {
			status = SWITCH_STATUS_FALSE;
		}
		if ( response ) {
			freeReplyObject(response);
		}
	}
	return status;
}

/* reconnect to redis server */
static switch_status_t hiredis_context_reconnect(hiredis_context_t *context)
{
	redisFree(context->context);
	context->context = redisConnectWithTimeout(context->connection->host, context->connection->port, context->connection->timeout);
	if ( context->context && !context->context->err && hiredis_context_auth(context) == SWITCH_STATUS_SUCCESS ) {
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

/* Return a context back to the pool */
static void hiredis_context_release(hiredis_context_t *context, switch_core_session_t *session)
{
	if (context) {
		if (switch_queue_push(context->connection->context_pool, context) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "hiredis: failed to release back to pool [%s, %d]\n", context->connection->host, context->connection->port);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: release back to pool [%s, %d]\n", context->connection->host, context->connection->port);
		}
	}
}

/* Grab a context from the pool, reconnect/connect as needed */
static hiredis_context_t *hiredis_connection_get_context(hiredis_connection_t *conn, switch_core_session_t *session)
{
	void *val = NULL;
	switch_time_t now = switch_time_now();
	switch_time_t timeout = now + conn->timeout_us;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: waiting for [%s, %d]\n", conn->host, conn->port);
	while (now < timeout) {
		if ( switch_queue_pop_timeout(conn->context_pool, &val, timeout - now) == SWITCH_STATUS_SUCCESS ) {
			hiredis_context_t *context = (hiredis_context_t *)val;
			if ( !context->context ) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: attempting[%s, %d]\n", conn->host, conn->port);
				context->context = redisConnectWithTimeout(conn->host, conn->port, conn->timeout);
				if ( context->context && !context->context->err && hiredis_context_auth(context) == SWITCH_STATUS_SUCCESS ) {
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
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: recycled from pool[%s, %d]\n", conn->host, conn->port);
				return context;
			}
		}
		now = switch_time_now();
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: timed out waiting for [%s, %d]\n", conn->host, conn->port);

	return NULL;
}

switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t ignore_connect_fail, uint8_t ignore_error, int max_pipelined_requests, int delete_when_zero)
{
	hiredis_profile_t *profile = NULL;
	switch_memory_pool_t *pool = NULL;

	switch_core_new_memory_pool(&pool);

	profile = switch_core_alloc(pool, sizeof(hiredis_profile_t));

	profile->pool = pool;
	profile->name = name ? switch_core_strdup(profile->pool, name) : "default";
	profile->conn_head = NULL;
	profile->ignore_connect_fail = ignore_connect_fail;
	profile->ignore_error = ignore_error;
	profile->delete_when_zero = delete_when_zero;

	profile->pipeline_running = 0;
	profile->max_pipelined_requests = max_pipelined_requests;
	switch_thread_rwlock_create(&profile->pipeline_lock, pool);
	switch_queue_create(&profile->request_pool, 2000, pool);
	switch_queue_create(&profile->active_requests, 2000, pool);

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

	hiredis_pipeline_threads_stop(profile);

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

			hiredis_pipeline_thread_start(profile);
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

	if ( profile->conn_head != NULL ) {
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

static void hiredis_parse_response(hiredis_request_t *request, redisReply *response)
{
	if ( !response ) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(request->session_uuid), SWITCH_LOG_ERROR, "hiredis: no response\n");
		if ( request->response ) {
			*(request->response) = NULL;
		}
		request->status = SWITCH_STATUS_GENERR;
		return;
	}

	switch(response->type) {
	case REDIS_REPLY_STATUS: /* fallthrough */
	case REDIS_REPLY_STRING:
		if ( request->response ) {
			*(request->response ) = strdup(response->str);
		}
		request->status = SWITCH_STATUS_SUCCESS;
		break;
	case REDIS_REPLY_INTEGER:
		if ( request->response ) {
			*(request->response) = switch_mprintf("%lld", response->integer);
		}
		request->status = SWITCH_STATUS_SUCCESS;
		break;
	case REDIS_REPLY_NIL:
		if ( request->response ) {
			*(request->response) = NULL;
		}
		request->status = SWITCH_STATUS_SUCCESS;
		break;
	case REDIS_REPLY_ERROR:
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(request->session_uuid), SWITCH_LOG_ERROR, "hiredis: error response[%s][%d]\n", response->str, response->type);
		if ( request->response ) {
			if (!zstr(response->str)) {
				*(request->response) = strdup(response->str);
			} else {
				*(request->response) = NULL;
			}
		}
		request->status = SWITCH_STATUS_GENERR;
		break;
	case REDIS_REPLY_ARRAY:
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(request->session_uuid), SWITCH_LOG_WARNING, "hiredis: unsupported array response[%d]\n", response->type);
		if ( request->response ) {
			*(request->response) = NULL;
		}
		request->status = SWITCH_STATUS_IGNORE;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(request->session_uuid), SWITCH_LOG_WARNING, "hiredis: unsupported response[%d]\n", response->type);
		if ( request->response ) {
			*(request->response) = NULL;
		}
		request->status = SWITCH_STATUS_IGNORE;
		break;
	}
}

static switch_status_t hiredis_context_execute_requests(hiredis_context_t *context, hiredis_request_t *requests)
{
	hiredis_request_t *cur_request;
	int ok = 1;

	for ( cur_request = requests; cur_request; cur_request = cur_request->next ) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cur_request->session_uuid), SWITCH_LOG_DEBUG, "hiredis: %s\n", cur_request->request);
		if ( cur_request->do_eval ) {
			/* eval needs special formatting to work properly */
			redisAppendCommand(context->context, "eval %s %d %s", cur_request->request, cur_request->num_keys, cur_request->keys ? cur_request->keys : "");
		} else {
			if (cur_request->argc == 0) {
				cur_request->argc = switch_separate_string(cur_request->request, ' ', cur_request->argv, MOD_HIREDIS_MAX_ARGS);
			}
			if (cur_request->argc > 0) {
				redisAppendCommandArgv(context->context, cur_request->argc, (const char **)cur_request->argv, NULL);
			}
		}
	}

	for ( cur_request = requests; cur_request; cur_request = cur_request->next ) {
		redisReply *response = NULL;
		int ret = redisGetReply(context->context, (void **)&response);
		if ( ret == REDIS_OK ) {
			hiredis_parse_response(cur_request, response);
		} else {
			ok = 0;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cur_request->session_uuid), SWITCH_LOG_ERROR, "hiredis: failed to get reply\n");
			cur_request->status = SWITCH_STATUS_GENERR;
		}
		if ( response ) {
			freeReplyObject(response);
		}
	}

	if ( ok ) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

switch_status_t hiredis_profile_execute_requests(hiredis_profile_t *profile, switch_core_session_t *session, hiredis_request_t *requests)
{
	hiredis_context_t *context = hiredis_profile_get_context(profile, NULL, session);
	int reconnected = 0;
	hiredis_request_t *cur_request;

	while (context) {
		if (hiredis_context_execute_requests(context, requests) == SWITCH_STATUS_SUCCESS) {
			hiredis_context_release(context, session);
			return SWITCH_STATUS_SUCCESS;
		} else if ( context->context->err ) {
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
			hiredis_context_release(context, session);
			/* mark all requests as failed */
			for ( cur_request = requests; cur_request; cur_request = cur_request->next ) {
				cur_request->status = SWITCH_STATUS_GENERR;
			}
			return SWITCH_STATUS_GENERR;
		}
	}
	/* mark all requests as failed */
	for ( cur_request = requests; cur_request; cur_request = cur_request->next ) {
		cur_request->status = SWITCH_STATUS_SOCKERR;
	}
	return SWITCH_STATUS_SOCKERR;
}

switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, switch_core_session_t *session, char **resp, const char *data)
{
	hiredis_request_t request = { 0 };
	request.response = resp;
	request.request = (char *)data;
	request.next = NULL;
	request.session_uuid = session ? (char *)switch_core_session_get_uuid(session) : NULL;
	hiredis_profile_execute_requests(profile, session, &request);
	return request.status;
}

switch_status_t hiredis_profile_execute_sync_printf(hiredis_profile_t *profile, switch_core_session_t *session, char **resp, const char *format_string, ...)
{
	switch_status_t result = SWITCH_STATUS_GENERR;
	char *data = NULL;
	va_list ap;
	int ret;

	va_start(ap, format_string);
	ret = switch_vasprintf(&data, format_string, ap);
	va_end(ap);

	if (ret != -1) {
		result = hiredis_profile_execute_sync(profile, session, resp, data);
	}
	switch_safe_free(data);
	return result;
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
