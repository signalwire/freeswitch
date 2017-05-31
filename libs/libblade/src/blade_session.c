/*
 * Copyright (c) 2017, Shane Bryldt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "blade.h"

struct blade_session_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	volatile blade_session_state_t state;

	const char *id;
	ks_rwl_t *lock;

	ks_cond_t *cond;

	const char *connection;
	ks_time_t ttl;

	ks_q_t *sending;
	ks_q_t *receiving;

	ks_hash_t *realms;
	ks_hash_t *routes;

	cJSON *properties;
	ks_rwl_t *properties_lock;
};

void *blade_session_state_thread(ks_thread_t *thread, void *data);
ks_status_t blade_session_onstate_startup(blade_session_t *bs);
ks_status_t blade_session_onstate_shutdown(blade_session_t *bs);
ks_status_t blade_session_onstate_run(blade_session_t *bs);
ks_status_t blade_session_process(blade_session_t *bs, cJSON *json);

static void blade_session_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_session_t *bs = (blade_session_t *)ptr;

	ks_assert(bs);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		blade_session_shutdown(bs);
		break;
	case KS_MPCL_DESTROY:

		// @todo consider looking at supporting externally allocated memory entries that can have cleanup callbacks associated, but the memory is not freed from the pool, only linked as an external allocation for auto cleanup
		// which would allow calling something like ks_pool_set_cleanup(bs->properties, ...) and when the pool is destroyed, it can call a callback which handles calling cJSON_Delete, which is allocated externally
		cJSON_Delete(bs->properties);
		bs->properties = NULL;

		// @todo remove this, it's just for posterity in debugging
		//bs->state_thread = NULL;
		bs->properties_lock = NULL;
		bs->receiving = NULL;
		bs->sending = NULL;
		bs->connection = NULL;
		bs->cond = NULL;
		bs->lock = NULL;

		//ks_pool_free(bs->pool, &bs->id);
		bs->id = NULL;
		break;
	}
}


KS_DECLARE(ks_status_t) blade_session_create(blade_session_t **bsP, blade_handle_t *bh, const char *id)
{
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bsP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

	bs = ks_pool_alloc(pool, sizeof(blade_session_t));
	bs->handle = bh;
	bs->pool = pool;

	if (id) bs->id = ks_pstrdup(pool, id);
	else {
		uuid_t id;
		ks_uuid(&id);
		bs->id = ks_uuid_str(pool, &id);
	}

    ks_rwl_create(&bs->lock, pool);
	ks_assert(bs->lock);

	ks_cond_create(&bs->cond, pool);
	ks_assert(bs->cond);

	ks_q_create(&bs->sending, pool, 0);
	ks_assert(bs->sending);
	ks_q_create(&bs->receiving, pool, 0);
	ks_assert(bs->receiving);

	ks_hash_create(&bs->realms, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bs->pool);
	ks_assert(bs->realms);

	ks_hash_create(&bs->routes, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bs->pool);
	ks_assert(bs->routes);

	bs->properties = cJSON_CreateObject();
	ks_assert(bs->properties);
    ks_rwl_create(&bs->properties_lock, pool);
	ks_assert(bs->properties_lock);

	ks_pool_set_cleanup(pool, bs, NULL, blade_session_cleanup);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*bsP = bs;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_destroy(blade_session_t **bsP)
{
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bsP);
	ks_assert(*bsP);

	bs = *bsP;

	pool = bs->pool;
	//ks_pool_free(bs->pool, bsP);
	ks_pool_close(&pool);

	*bsP = NULL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_startup(blade_session_t *bs)
{
	ks_thread_pool_t *tpool = NULL;

	ks_assert(bs);

	tpool = blade_handle_tpool_get(bs->handle);
	ks_assert(tpool);

	blade_session_state_set(bs, BLADE_SESSION_STATE_NONE);

	if (ks_thread_pool_add_job(tpool, blade_session_state_thread, bs) != KS_STATUS_SUCCESS) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_shutdown(blade_session_t *bs)
{
	ks_hash_iterator_t *it = NULL;
	cJSON *json = NULL;

	ks_assert(bs);

	// if this is an upstream session there will be no routes, so this is harmless to always run regardless
	ks_hash_read_lock(bs->routes);
	for (it = ks_hash_first(bs->routes, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);

		blade_handle_route_remove(bs->handle, (const char *)key);
	}
	ks_hash_read_unlock(bs->routes);

	// this will also clear the identities and realms in the handle if this is the upstream session
	blade_handle_sessions_remove(bs);

	while (ks_q_trypop(bs->sending, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);
	while (ks_q_trypop(bs->receiving, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_session_handle_get(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->handle;
}

KS_DECLARE(const char *) blade_session_id_get(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->id;
}

KS_DECLARE(blade_session_state_t) blade_session_state_get(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->state;
}

KS_DECLARE(ks_status_t) blade_session_realm_add(blade_session_t *bs, const char *realm)
{
	char *key = NULL;

	ks_assert(bs);
	ks_assert(realm);

	key = ks_pstrdup(bs->pool, realm);
	ks_hash_insert(bs->realms, (void *)key, (void *)KS_TRUE);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_realm_remove(blade_session_t *bs, const char *realm)
{
	ks_assert(bs);
	ks_assert(realm);

	ks_hash_remove(bs->realms, (void *)realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_hash_t *) blade_session_realms_get(blade_session_t *bs)
{
	ks_assert(bs);
	return bs->realms;
}

KS_DECLARE(ks_status_t) blade_session_route_add(blade_session_t *bs, const char *identity)
{
	char *key = NULL;

	ks_assert(bs);
	ks_assert(identity);

	key = ks_pstrdup(bs->pool, identity);
	ks_hash_insert(bs->routes, (void *)key, (void *)KS_TRUE);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_route_remove(blade_session_t *bs, const char *identity)
{
	ks_assert(bs);
	ks_assert(identity);

	ks_hash_remove(bs->routes, (void *)identity);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(cJSON *) blade_session_properties_get(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->properties;
}

KS_DECLARE(ks_status_t) blade_session_read_lock(blade_session_t *bs, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bs);

	if (block) ret = ks_rwl_read_lock(bs->lock);
	else ret = ks_rwl_try_read_lock(bs->lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_read_unlock(blade_session_t *bs)
{
	ks_assert(bs);

	return ks_rwl_read_unlock(bs->lock);
}

KS_DECLARE(ks_status_t) blade_session_write_lock(blade_session_t *bs, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bs);

	if (block) ret = ks_rwl_write_lock(bs->lock);
	else ret = ks_rwl_try_write_lock(bs->lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_write_unlock(blade_session_t *bs)
{
	ks_assert(bs);

	return ks_rwl_write_unlock(bs->lock);
}


KS_DECLARE(ks_status_t) blade_session_properties_read_lock(blade_session_t *bs, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bs);

	if (block) ret = ks_rwl_read_lock(bs->properties_lock);
	else ret = ks_rwl_try_read_lock(bs->properties_lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_properties_read_unlock(blade_session_t *bs)
{
	ks_assert(bs);

	return ks_rwl_read_unlock(bs->properties_lock);
}

KS_DECLARE(ks_status_t) blade_session_properties_write_lock(blade_session_t *bs, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bs);

	if (block) ret = ks_rwl_write_lock(bs->properties_lock);
	else ret = ks_rwl_try_write_lock(bs->properties_lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_properties_write_unlock(blade_session_t *bs)
{
	ks_assert(bs);

	return ks_rwl_write_unlock(bs->properties_lock);
}


KS_DECLARE(void) blade_session_state_set(blade_session_t *bs, blade_session_state_t state)
{
	ks_assert(bs);

	ks_cond_lock(bs->cond);
	bs->state = state;
	blade_handle_session_state_callbacks_execute(bs, BLADE_SESSION_STATE_CONDITION_PRE);
	ks_cond_unlock(bs->cond);

	ks_cond_try_signal(bs->cond);
}

KS_DECLARE(void) blade_session_hangup(blade_session_t *bs)
{
	ks_assert(bs);

	if (!blade_session_terminating(bs)) {
		ks_log(KS_LOG_DEBUG, "Session (%s) hanging up\n", bs->id);
		blade_session_state_set(bs, BLADE_SESSION_STATE_SHUTDOWN);
	}
}

KS_DECLARE(ks_bool_t) blade_session_terminating(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->state == BLADE_SESSION_STATE_SHUTDOWN || bs->state == BLADE_SESSION_STATE_CLEANUP;
}

KS_DECLARE(const char *) blade_session_connection_get(blade_session_t *bs)
{
	ks_assert(bs);
	return bs->connection;
}

KS_DECLARE(ks_status_t) blade_session_connection_set(blade_session_t *bs, const char *id)
{
	ks_assert(bs);

	if (id) {
		if (bs->connection) {
			// @todo best that can be done in this situation is see if the connection is still available, and if so then disconnect it... this really shouldn't happen
			ks_pool_free(bs->pool, &bs->connection);
		}
		bs->connection = ks_pstrdup(bs->pool, id);
		ks_assert(bs->connection);

		bs->ttl = 0;

		ks_log(KS_LOG_DEBUG, "Session (%s) associated to connection (%s)\n", bs->id, id);

		// @todo signal the wait condition for the state machine to see a reconnect immediately
	} else if (bs->connection) {
		ks_log(KS_LOG_DEBUG, "Session (%s) cleared connection (%s)\n", bs->id, bs->connection);

		ks_pool_free(bs->pool, &bs->connection);

		bs->ttl = ks_time_now() + (5 * KS_USEC_PER_SEC);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_sending_push(blade_session_t *bs, cJSON *json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
    cJSON *json_copy = NULL;

    ks_assert(bs);
    ks_assert(json);

    json_copy = cJSON_Duplicate(json, 1);
	if ((ret = ks_q_push(bs->sending, json_copy)) == KS_STATUS_SUCCESS) ks_cond_try_signal(bs->cond);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_sending_pop(blade_session_t *bs, cJSON **json)
{
	ks_assert(bs);
	ks_assert(json);

	return ks_q_trypop(bs->sending, (void **)json);
}

KS_DECLARE(ks_status_t) blade_session_receiving_push(blade_session_t *bs, cJSON *json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
    cJSON *json_copy = NULL;

    ks_assert(bs);
    ks_assert(json);

    json_copy = cJSON_Duplicate(json, 1);
	if ((ret = ks_q_push(bs->receiving, json_copy)) == KS_STATUS_SUCCESS) ks_cond_try_signal(bs->cond);
	return ret;
}

KS_DECLARE(ks_status_t) blade_session_receiving_pop(blade_session_t *bs, cJSON **json)
{
	ks_assert(bs);
	ks_assert(json);

	return ks_q_trypop(bs->receiving, (void **)json);
}


void *blade_session_state_thread(ks_thread_t *thread, void *data)
{
	blade_session_t *bs = NULL;
	blade_session_state_t state;
	cJSON *json = NULL;
	ks_bool_t shutdown = KS_FALSE;

	ks_assert(thread);
	ks_assert(data);

	bs = (blade_session_t *)data;

	ks_cond_lock(bs->cond);
	while (!shutdown) {
		// Entering the call below, the mutex is expected to be locked and will be unlocked by the call
		ks_cond_timedwait(bs->cond, 100);
		// Leaving the call above, the mutex will be locked after being signalled, timing out, or woken up for any reason

		state = bs->state;

		if (bs->connection) {
			blade_connection_t *bc = blade_handle_connections_lookup(bs->handle, bs->connection);
			if (bc) {
				// @note in order for this to work on session reconnecting, the assumption is that as soon as a session has a connection set,
				// we can start stuffing any messages queued for output on the session straight to the connection right away, may need to only
				// do this when in session ready state but there may be implications of other states sending messages through the session
				while (blade_session_sending_pop(bs, &json) == KS_STATUS_SUCCESS && json) {
					blade_connection_sending_push(bc, json);
					cJSON_Delete(json);
				}
				blade_connection_read_unlock(bc);
			}
		}

		// @todo evolve this system, it's probably not the best way to handle receiving session state updates externally
		blade_handle_session_state_callbacks_execute(bs, BLADE_SESSION_STATE_CONDITION_POST);

		switch (state) {
		case BLADE_SESSION_STATE_STARTUP:
			// @todo this may occur from a reconnect, should have some way to identify it is a reconnected session until we hit RUN state at least
			ks_log(KS_LOG_DEBUG, "Session (%s) state startup\n", bs->id);
			blade_session_state_set(bs, BLADE_SESSION_STATE_RUN);
			break;
		case BLADE_SESSION_STATE_SHUTDOWN:
			blade_session_onstate_shutdown(bs);
			shutdown = KS_TRUE;
			break;
		case BLADE_SESSION_STATE_RUN:
			blade_session_onstate_run(bs);
			break;
		default: break;
		}

		if (!bs->connection &&
			bs->ttl > 0 &&
			!blade_session_terminating(bs) &&
			ks_time_now() >= bs->ttl) {
			ks_log(KS_LOG_DEBUG, "Session (%s) TTL timeout\n", bs->id);
			blade_session_hangup(bs);
		}
	}
	ks_cond_unlock(bs->cond);

	blade_session_destroy(&bs);

	return NULL;
}

ks_status_t blade_session_onstate_shutdown(blade_session_t *bs)
{
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) state shutdown\n", bs->id);

	blade_session_state_set(bs, BLADE_SESSION_STATE_CLEANUP);

	if (bs->connection) {
		blade_connection_t *bc = blade_handle_connections_lookup(bs->handle, bs->connection);
		if (bc) {
			blade_connection_disconnect(bc);
			blade_connection_read_unlock(bc);
		}
	}

	// @note wait for the connection to disconnect before we resume session cleanup
	while (bs->connection) ks_sleep(100);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_session_onstate_run(blade_session_t *bs)
{
	cJSON *json = NULL;

	ks_assert(bs);

	//ks_log(KS_LOG_DEBUG, "Session (%s) state run\n", bs->id);

	// @todo for now only process messages if there is a connection available
	if (bs->connection) {
		// @todo may only want to pop once per call to give sending a chance to keep up
		while (blade_session_receiving_pop(bs, &json) == KS_STATUS_SUCCESS && json) {
			// @todo all messages will pass through the local jsonrpc method handlers, but each needs to determine if the
			// message is destined for the local node, and if not then each handler can determine how routing occurs as
			// they differ, especially when it comes to the announcing of identities and propagation of multicast events
			blade_session_process(bs, json);
			cJSON_Delete(json);
		}
	}

	//ks_sleep_ms(1);
	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_session_send(blade_session_t *bs, cJSON *json, blade_jsonrpc_response_callback_t callback)
{
	blade_jsonrpc_request_t *bjsonrpcreq = NULL;
	const char *method = NULL;
	const char *id = NULL;

	ks_assert(bs);
	ks_assert(json);

	method = cJSON_GetObjectCstr(json, "method");

	id = cJSON_GetObjectCstr(json, "id");
	ks_assert(id);

	if (method) {
		// @note This is scenario 1
		// 1) Sending a request (client: method caller or consumer)
		ks_log(KS_LOG_DEBUG, "Session (%s) sending request (%s) for %s\n", bs->id, id, method);

		blade_jsonrpc_request_create(&bjsonrpcreq, bs->handle, blade_handle_pool_get(bs->handle), bs->id, json, callback);
		ks_assert(bjsonrpcreq);

		// @todo set request TTL and figure out when requests are checked for expiration (separate thread in the handle?)
		blade_handle_requests_add(bjsonrpcreq);
	} else {
		// @note This is scenario 3
		// 3) Sending a response or error (server: method callee or provider)
		ks_log(KS_LOG_DEBUG, "Session (%s) sending response (%s)\n", bs->id, id);
	}

	if (!bs->connection) {
		blade_session_sending_push(bs, json);
	} else {
		blade_connection_t *bc = blade_handle_connections_lookup(bs->handle, bs->connection);
		if (!bc) {
			blade_session_sending_push(bs, json);
			return KS_STATUS_FAIL;
		}
		blade_connection_sending_push(bc, json);
		blade_connection_read_unlock(bc);
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_session_process(blade_session_t *bs, cJSON *json)
{
	blade_handle_t *bh = NULL;
	blade_jsonrpc_request_t *bjsonrpcreq = NULL;
	blade_jsonrpc_response_t *bjsonrpcres = NULL;
	const char *jsonrpc = NULL;
	const char *id = NULL;
	const char *method = NULL;
	ks_bool_t disconnect = KS_FALSE;

	ks_assert(bs);
	ks_assert(json);

	ks_log(KS_LOG_DEBUG, "Session (%s) processing\n", bs->id);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	jsonrpc = cJSON_GetObjectCstr(json, "jsonrpc");
	if (!jsonrpc || strcmp(jsonrpc, "2.0")) {
        ks_log(KS_LOG_DEBUG, "Received message is not the expected protocol\n");
		// @todo send error response, code = -32600 (invalid request)
		// @todo hangup session entirely?
		return KS_STATUS_FAIL;
	}


	id = cJSON_GetObjectCstr(json, "id");
	if (!id) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'id'\n");
		// @todo send error response, code = -32600 (invalid request)
		// @todo hangup session entirely?
		return KS_STATUS_FAIL;
	}

	method = cJSON_GetObjectCstr(json, "method");
	if (method) {
		// @note This is scenario 2
		// 2) Receiving a request (server: method callee or provider)
		blade_jsonrpc_t *bjsonrpc = NULL;
		blade_jsonrpc_request_callback_t callback = NULL;
		cJSON *params = NULL;

		ks_log(KS_LOG_DEBUG, "Session (%s) receiving request (%s) for %s\n", bs->id, id, method);

		params = cJSON_GetObjectItem(json, "params");
		if (params) {
			const char *params_requester_nodeid = cJSON_GetObjectCstr(params, "requester-nodeid");
			const char *params_responder_nodeid = cJSON_GetObjectCstr(params, "responder-nodeid");
			if (params_requester_nodeid && params_responder_nodeid && !blade_handle_local_nodeid_compare(bh, params_responder_nodeid)) {
				// not meant for local processing, continue with standard unicast routing for requests
				blade_session_t *bs_router = blade_handle_route_lookup(bh, params_responder_nodeid);
				if (!bs_router) {
					bs_router = blade_handle_sessions_upstream(bh);
					if (!bs_router) {
						cJSON *res = NULL;
						cJSON *res_error = NULL;

						ks_log(KS_LOG_DEBUG, "Session (%s) request (%s => %s) but upstream session unavailable\n", blade_session_id_get(bs), params_requester_nodeid, params_responder_nodeid);
						blade_jsonrpc_error_raw_create(&res, &res_error, id, -32603, "Upstream session unavailable");

						// needed in case this error must propagate further than the session which sent it
						cJSON_AddStringToObject(res_error, "requester-nodeid", params_requester_nodeid);
						cJSON_AddStringToObject(res_error, "responder-nodeid", params_responder_nodeid); // @todo responder-nodeid should become the local_nodeid to inform of which node actually responded

						blade_session_send(bs, res, NULL);
						return KS_STATUS_DISCONNECTED;
					}
				}

				if (bs_router == bs) {
					// @todo avoid circular by sending back an error instead, really should not happen but check for posterity in case a node is misbehaving for some reason
				}
				
				ks_log(KS_LOG_DEBUG, "Session (%s) request (%s => %s) routing (%s)\n", blade_session_id_get(bs), params_requester_nodeid, params_responder_nodeid, blade_session_id_get(bs_router));
				blade_session_send(bs_router, json, NULL);
				blade_session_read_unlock(bs_router);

				return KS_STATUS_SUCCESS;
			}
		}

		// reach here if the request was not captured for routing, this SHOULD always mean the message is to be processed by local handlers
		bjsonrpc = blade_handle_jsonrpc_lookup(bs->handle, method);

		if (!bjsonrpc) {
			ks_log(KS_LOG_DEBUG, "Received unknown jsonrpc method %s\n", method);
			// @todo send error response, code = -32601 (method not found)
			return KS_STATUS_FAIL;
		}
		callback = blade_jsonrpc_callback_get(bjsonrpc);
		ks_assert(callback);

		blade_jsonrpc_request_create(&bjsonrpcreq, bs->handle, blade_handle_pool_get(bs->handle), bs->id, json, NULL);
		ks_assert(bjsonrpcreq);

		disconnect = callback(bjsonrpcreq, blade_jsonrpc_callback_data_get(bjsonrpc));

		blade_jsonrpc_request_destroy(&bjsonrpcreq);
	} else {
		// @note This is scenario 4
		// 4) Receiving a response or error (client: method caller or consumer)
		blade_jsonrpc_response_callback_t callback = NULL;
		cJSON *error = NULL;
		cJSON *result = NULL;
		cJSON *object = NULL;

		ks_log(KS_LOG_DEBUG, "Session (%s) receiving response (%s)\n", bs->id, id);

		error = cJSON_GetObjectItem(json, "error");
		result = cJSON_GetObjectItem(json, "result");
		object = error ? error : result;

		if (object) {
			const char *object_requester_nodeid = cJSON_GetObjectCstr(object, "requester-nodeid");
			const char *object_responder_nodeid = cJSON_GetObjectCstr(object, "responder-nodeid");
			if (object_requester_nodeid && object_responder_nodeid && !blade_handle_local_nodeid_compare(bh, object_requester_nodeid)) {
				// not meant for local processing, continue with standard unicast routing for responses
				blade_session_t *bs_router = blade_handle_route_lookup(bh, object_requester_nodeid);
				if (!bs_router) {
					bs_router = blade_handle_sessions_upstream(bh);
					if (!bs_router) {
						ks_log(KS_LOG_DEBUG, "Session (%s) response (%s <= %s) but upstream session unavailable\n", blade_session_id_get(bs), object_requester_nodeid, object_responder_nodeid);
						return KS_STATUS_DISCONNECTED;
					}
				}

				if (bs_router == bs) {
					// @todo avoid circular, really should not happen but check for posterity in case a node is misbehaving for some reason
				}

				ks_log(KS_LOG_DEBUG, "Session (%s) response (%s <= %s) routing (%s)\n", blade_session_id_get(bs), object_requester_nodeid, object_responder_nodeid, blade_session_id_get(bs_router));
				blade_session_send(bs_router, json, NULL);
				blade_session_read_unlock(bs_router);

				return KS_STATUS_SUCCESS;
			}
		}

		bjsonrpcreq = blade_handle_requests_lookup(bs->handle, id);
		if (!bjsonrpcreq) {
			// @todo hangup session entirely?
			return KS_STATUS_FAIL;
		}
		blade_handle_requests_remove(bjsonrpcreq);

		callback = blade_jsonrpc_request_callback_get(bjsonrpcreq);
		ks_assert(callback);

		blade_jsonrpc_response_create(&bjsonrpcres, bs->handle, bs->pool, bs->id, bjsonrpcreq, json);
		ks_assert(bjsonrpcres);

		disconnect = callback(bjsonrpcres);

		blade_jsonrpc_response_destroy(&bjsonrpcres);
	}

	if (disconnect) {
		// @todo hangup session entirely?
		return KS_STATUS_FAIL;
	}

	return KS_STATUS_SUCCESS;
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
