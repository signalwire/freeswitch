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

	ks_bool_t shutdown;
    ks_thread_t *state_thread;
	blade_session_state_t state;

	const char *id;
	ks_rwl_t *lock;
	list_t connections;
	ks_time_t ttl;

	ks_q_t *sending;
	ks_q_t *receiving;
};

void *blade_session_state_thread(ks_thread_t *thread, void *data);
ks_status_t blade_session_state_on_destroy(blade_session_t *bs);
ks_status_t blade_session_state_on_hangup(blade_session_t *bs);
ks_status_t blade_session_state_on_ready(blade_session_t *bs);
ks_status_t blade_session_process(blade_session_t *bs, cJSON *json);

KS_DECLARE(ks_status_t) blade_session_create(blade_session_t **bsP, blade_handle_t *bh)
{
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
    uuid_t id;

	ks_assert(bsP);
	ks_assert(bh);

	pool = blade_handle_pool_get(bh);

	bs = ks_pool_alloc(pool, sizeof(blade_session_t));
	bs->handle = bh;
	bs->pool = pool;

    ks_uuid(&id);
	bs->id = ks_uuid_str(pool, &id);

    ks_rwl_create(&bs->lock, pool);
	ks_assert(bs->lock);

	list_init(&bs->connections);
	ks_q_create(&bs->sending, pool, 0);
	ks_assert(bs->sending);
	ks_q_create(&bs->receiving, pool, 0);
	ks_assert(bs->receiving);

	*bsP = bs;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_destroy(blade_session_t **bsP)
{
	blade_session_t *bs = NULL;

	ks_assert(bsP);
	ks_assert(*bsP);

	bs = *bsP;

	blade_session_shutdown(bs);

	list_destroy(&bs->connections);
	ks_q_destroy(&bs->receiving);
	ks_q_destroy(&bs->sending);

	ks_rwl_destroy(&bs->lock);

    ks_pool_free(bs->pool, &bs->id);

	ks_pool_free(bs->pool, bsP);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_startup(blade_session_t *bs)
{
	ks_assert(bs);

	blade_session_state_set(bs, BLADE_SESSION_STATE_NONE);

    if (ks_thread_create_ex(&bs->state_thread,
							blade_session_state_thread,
							bs,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bs->pool) != KS_STATUS_SUCCESS) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_shutdown(blade_session_t *bs)
{
	cJSON *json = NULL;

	ks_assert(bs);

	if (bs->state_thread) {
		bs->shutdown = KS_TRUE;
		ks_thread_join(bs->state_thread);
		ks_pool_free(bs->pool, &bs->state_thread);
		bs->shutdown = KS_FALSE;
	}

	while (ks_q_trypop(bs->sending, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);
	while (ks_q_trypop(bs->receiving, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);

	list_iterator_start(&bs->connections);
	while (list_iterator_hasnext(&bs->connections)) {
		const char *id = (const char *)list_iterator_next(&bs->connections);
		ks_pool_free(bs->pool, &id);
	}
	list_iterator_stop(&bs->connections);
	list_clear(&bs->connections);

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

KS_DECLARE(void) blade_session_id_set(blade_session_t *bs, const char *id)
{
	ks_assert(bs);
	ks_assert(id);

	if (bs->id) ks_pool_free(bs->pool, &bs->id);
	bs->id = ks_pstrdup(bs->pool, id);
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


KS_DECLARE(void) blade_session_state_set(blade_session_t *bs, blade_session_state_t state)
{
	ks_assert(bs);

	bs->state = state;
}

KS_DECLARE(void) blade_session_hangup(blade_session_t *bs)
{
	ks_assert(bs);

	if (bs->state != BLADE_SESSION_STATE_HANGUP && bs->state != BLADE_SESSION_STATE_DESTROY) {
		ks_log(KS_LOG_DEBUG, "Session (%s) hanging up\n", bs->id);
		blade_session_state_set(bs, BLADE_SESSION_STATE_HANGUP);
	}
}

KS_DECLARE(ks_bool_t) blade_session_terminating(blade_session_t *bs)
{
	ks_assert(bs);

	return bs->state == BLADE_SESSION_STATE_HANGUP || bs->state == BLADE_SESSION_STATE_DESTROY;
}

KS_DECLARE(ks_status_t) blade_session_connections_add(blade_session_t *bs, const char *id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	const char *cid = NULL;

	ks_assert(bs);

	cid = ks_pstrdup(bs->pool, id);
	ks_assert(cid);

	list_append(&bs->connections, cid);

	ks_log(KS_LOG_DEBUG, "Session (%s) connection added (%s)\n", bs->id, id);

	bs->ttl = 0;

	return ret;
}

KS_DECLARE(ks_status_t) blade_session_connections_remove(blade_session_t *bs, const char *id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	uint32_t size = 0;

	ks_assert(bs);

	size = list_size(&bs->connections);
	for (uint32_t i = 0; i < size; ++i) {
		const char *cid = (const char *)list_get_at(&bs->connections, i);
		if (!strcasecmp(cid, id)) {
			ks_log(KS_LOG_DEBUG, "Session (%s) connection removed (%s)\n", bs->id, id);
			list_delete_at(&bs->connections, i);
			ks_pool_free(bs->pool, &cid);
			break;
		}
	}

	if (list_size(&bs->connections) == 0) bs->ttl = ks_time_now() + (5 * KS_USEC_PER_SEC);

	return ret;
}

ks_status_t blade_session_connections_choose(blade_session_t *bs, cJSON *json, blade_connection_t **bcP)
{
	blade_connection_t *bc = NULL;
	const char *cid = NULL;

	ks_assert(bs);
	ks_assert(json);
	ks_assert(bcP);

	// @todo may be multiple connections, for now let's just assume there will be only one
	// later there will need to be a way to pick which connection to use
	cid = list_get_at(&bs->connections, 0);
	if (!cid) {
		// no connections available
		return KS_STATUS_FAIL;
	}

	bc = blade_handle_connections_get(bs->handle, cid);
	if (!bc) {
		// @todo error logging... this shouldn't happen
		return KS_STATUS_FAIL;
	}
	// @todo make sure the connection is in the READY state before allowing it to be choosen, just in case it is detaching or not quite fully attached

	*bcP = bc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_session_sending_push(blade_session_t *bs, cJSON *json)
{
    cJSON *json_copy = NULL;

    ks_assert(bs);
    ks_assert(json);

    json_copy = cJSON_Duplicate(json, 1);
    return ks_q_push(bs->sending, json_copy);
}

KS_DECLARE(ks_status_t) blade_session_sending_pop(blade_session_t *bs, cJSON **json)
{
	ks_assert(bs);
	ks_assert(json);

	return ks_q_trypop(bs->sending, (void **)json);
}

KS_DECLARE(ks_status_t) blade_session_receiving_push(blade_session_t *bs, cJSON *json)
{
    cJSON *json_copy = NULL;

    ks_assert(bs);
    ks_assert(json);

    json_copy = cJSON_Duplicate(json, 1);
    return ks_q_push(bs->receiving, json_copy);
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

	ks_assert(thread);
	ks_assert(data);

	bs = (blade_session_t *)data;

	while (!bs->shutdown) {

		state = bs->state;

		if (!list_empty(&bs->connections)) {
			while (blade_session_sending_pop(bs, &json) == KS_STATUS_SUCCESS && json) {
				blade_connection_t *bc = NULL;
				if (blade_session_connections_choose(bs, json, &bc) == KS_STATUS_SUCCESS) {
					blade_connection_sending_push(bc, json);
					blade_connection_read_unlock(bc);
				}
				cJSON_Delete(json);
			}
		}

		switch (state) {
		case BLADE_SESSION_STATE_DESTROY:
			blade_session_state_on_destroy(bs);
			return NULL;
		case BLADE_SESSION_STATE_HANGUP:
			blade_session_state_on_hangup(bs);
			break;
		case BLADE_SESSION_STATE_CONNECT:
			ks_log(KS_LOG_DEBUG, "Session (%s) state connect\n", bs->id);
			ks_sleep_ms(1000);
			break;
		case BLADE_SESSION_STATE_ATTACH:
			ks_log(KS_LOG_DEBUG, "Session (%s) state attach\n", bs->id);
			ks_sleep_ms(1000);
			break;
		case BLADE_SESSION_STATE_DETACH:
			ks_log(KS_LOG_DEBUG, "Session (%s) state detach\n", bs->id);
			ks_sleep_ms(1000);
			break;
		case BLADE_SESSION_STATE_READY:
			blade_session_state_on_ready(bs);
			break;
		default: break;
		}

		if (list_empty(&bs->connections) &&
			bs->ttl > 0 &&
			bs->state != BLADE_SESSION_STATE_HANGUP &&
			bs->state != BLADE_SESSION_STATE_DESTROY &&
			ks_time_now() >= bs->ttl) {
			ks_log(KS_LOG_DEBUG, "Session (%s) TTL timeout\n", bs->id);
			blade_session_hangup(bs);
		}
	}

	return NULL;
}

ks_status_t blade_session_state_on_destroy(blade_session_t *bs)
{
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) state destroy\n", bs->id);
	blade_handle_sessions_remove(bs);
	blade_session_destroy(&bs);

	// @todo ignoring returns for now, see what makes sense later
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_session_state_on_hangup(blade_session_t *bs)
{
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) state hangup\n", bs->id);

	list_iterator_start(&bs->connections);
	while (list_iterator_hasnext(&bs->connections)) {
		const char *cid = (const char *)list_iterator_next(&bs->connections);
		blade_connection_t *bc = blade_handle_connections_get(bs->handle, cid);
		ks_assert(bc);

		blade_connection_disconnect(bc);
		blade_connection_read_unlock(bc);
	}
	list_iterator_stop(&bs->connections);

	while (!list_empty(&bs->connections)) ks_sleep(100);

	blade_session_state_set(bs, BLADE_SESSION_STATE_DESTROY);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_session_state_on_ready(blade_session_t *bs)
{
	cJSON *json = NULL;

	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) state ready\n", bs->id);

	// @todo for now only process messages if there is a connection available
	if (list_size(&bs->connections) > 0) {
		// @todo may only want to pop once per call to give sending a chance to keep up
		while (blade_session_receiving_pop(bs, &json) == KS_STATUS_SUCCESS && json) {
			blade_session_process(bs, json);
			cJSON_Delete(json);
		}
	}

	ks_sleep_ms(1000);
	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_session_send(blade_session_t *bs, cJSON *json)
{
	ks_assert(bs);
	ks_assert(json);

	// @todo check json for "method", if this is an outgoing request then build up the data for a response to lookup the message id and get back to the request
	// this can reuse blade_request_t so that when the blade_response_t is passed up the blade_request_t within it is familiar from inbound requests

	if (list_empty(&bs->connections)) {
		// @todo cache the blade_request_t here if it exists to gaurentee it's cached before a response could be received
		blade_session_sending_push(bs, json);
	} else {
		blade_connection_t *bc = NULL;
		if (blade_session_connections_choose(bs, json, &bc) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
		// @todo cache the blade_request_t here if it exists to gaurentee it's cached before a response could be received
		blade_connection_sending_push(bc, json);
		blade_connection_read_unlock(bc);
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_session_process(blade_session_t *bs, cJSON *json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bs);
	ks_assert(json);

	ks_log(KS_LOG_DEBUG, "Session (%s) processing\n", bs->id);

	// @todo teardown the message, convert into a blade_request_t or blade_response_t

	return ret;
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
