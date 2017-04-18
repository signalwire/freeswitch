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

struct blade_connection_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	void *transport_data;
	blade_transport_callbacks_t *transport_callbacks;

	blade_connection_direction_t direction;
	volatile blade_connection_state_t state;

	const char *id;
	ks_rwl_t *lock;

	ks_q_t *sending;

	const char *session;
};

void *blade_connection_state_thread(ks_thread_t *thread, void *data);
ks_status_t blade_connection_state_on_disconnect(blade_connection_t *bc);
ks_status_t blade_connection_state_on_new(blade_connection_t *bc);
ks_status_t blade_connection_state_on_connect(blade_connection_t *bc);
ks_status_t blade_connection_state_on_attach(blade_connection_t *bc);
ks_status_t blade_connection_state_on_detach(blade_connection_t *bc);
ks_status_t blade_connection_state_on_ready(blade_connection_t *bc);


static void blade_connection_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_connection_t *bc = (blade_connection_t *)ptr;

	ks_assert(bc);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		blade_connection_shutdown(bc);
		break;
	case KS_MPCL_DESTROY:
		// @todo remove this, it's just for posterity in debugging
		bc->sending = NULL;
		bc->lock = NULL;

		//ks_pool_free(bc->pool, &bc->id);
		bc->id = NULL;
		break;
	}
}

KS_DECLARE(ks_status_t) blade_connection_create(blade_connection_t **bcP, blade_handle_t *bh)
{
	blade_connection_t *bc = NULL;
	ks_pool_t *pool = NULL;
	uuid_t id;

	ks_assert(bcP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

	bc = ks_pool_alloc(pool, sizeof(blade_connection_t));
	bc->handle = bh;
	bc->pool = pool;

	ks_uuid(&id);
	bc->id = ks_uuid_str(pool, &id);
	ks_assert(bc->id);

	ks_rwl_create(&bc->lock, pool);
	ks_assert(bc->lock);

	ks_q_create(&bc->sending, pool, 0);
	ks_assert(bc->sending);

	ks_assert(ks_pool_set_cleanup(pool, bc, NULL, blade_connection_cleanup) == KS_STATUS_SUCCESS);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*bcP = bc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_destroy(blade_connection_t **bcP)
{
	blade_connection_t *bc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bcP);
	ks_assert(*bcP);

	bc = *bcP;

	pool = bc->pool;
	//ks_pool_free(bc->pool, bcP);
	ks_pool_close(&pool);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	*bcP = NULL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_startup(blade_connection_t *bc, blade_connection_direction_t direction)
{
	blade_handle_t *bh = NULL;

	ks_assert(bc);

	bh = blade_connection_handle_get(bc);

	bc->direction = direction;
	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NONE);

	if (ks_thread_pool_add_job(blade_handle_tpool_get(bh), blade_connection_state_thread, bc) != KS_STATUS_SUCCESS) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_shutdown(blade_connection_t *bc)
{
	cJSON *json = NULL;

	ks_assert(bc);

	blade_handle_connections_remove(bc);

	while (ks_q_trypop(bc->sending, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_connection_handle_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->handle;
}

KS_DECLARE(ks_pool_t *) blade_connection_pool_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->pool;
}

KS_DECLARE(const char *) blade_connection_id_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->id;
}

KS_DECLARE(ks_status_t) blade_connection_read_lock(blade_connection_t *bc, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bc);

	if (block) ret = ks_rwl_read_lock(bc->lock);
	else ret = ks_rwl_try_read_lock(bc->lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_connection_read_unlock(blade_connection_t *bc)
{
	ks_assert(bc);

	return ks_rwl_read_unlock(bc->lock);
}

KS_DECLARE(ks_status_t) blade_connection_write_lock(blade_connection_t *bc, ks_bool_t block)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bc);

	if (block) ret = ks_rwl_write_lock(bc->lock);
	else ret = ks_rwl_try_write_lock(bc->lock);
	return ret;
}

KS_DECLARE(ks_status_t) blade_connection_write_unlock(blade_connection_t *bc)
{
	ks_assert(bc);

	return ks_rwl_write_unlock(bc->lock);
}


KS_DECLARE(void *) blade_connection_transport_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->transport_data;
}

KS_DECLARE(void) blade_connection_transport_set(blade_connection_t *bc, void *transport_data, blade_transport_callbacks_t *transport_callbacks)
{
	ks_assert(bc);
	ks_assert(transport_data);
	ks_assert(transport_callbacks);

	bc->transport_data = transport_data;
	bc->transport_callbacks = transport_callbacks;
}

blade_transport_state_callback_t blade_connection_state_callback_lookup(blade_connection_t *bc, blade_connection_state_t state)
{
	blade_transport_state_callback_t callback = NULL;

	ks_assert(bc);

	switch (state) {
	case BLADE_CONNECTION_STATE_DISCONNECT:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_disconnect_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_disconnect_outbound;
		break;
	case BLADE_CONNECTION_STATE_NEW:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_new_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_new_outbound;
		break;
	case BLADE_CONNECTION_STATE_CONNECT:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_connect_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_connect_outbound;
		break;
	case BLADE_CONNECTION_STATE_ATTACH:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_attach_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_attach_outbound;
		break;
	case BLADE_CONNECTION_STATE_DETACH:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_detach_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_detach_outbound;
		break;
	case BLADE_CONNECTION_STATE_READY:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_ready_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_ready_outbound;
		break;
	default: break;
	}

	return callback;
}

KS_DECLARE(void) blade_connection_state_set(blade_connection_t *bc, blade_connection_state_t state)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, state);

	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_PRE);

	bc->state = state;

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT) blade_connection_disconnect(bc);
}

KS_DECLARE(blade_connection_state_t) blade_connection_state_get(blade_connection_t *bc)
{
	ks_assert(bc);
	return bc->state;
}

KS_DECLARE(void) blade_connection_disconnect(blade_connection_t *bc)
{
	ks_assert(bc);

	if (bc->state != BLADE_CONNECTION_STATE_DETACH && bc->state != BLADE_CONNECTION_STATE_DISCONNECT && bc->state != BLADE_CONNECTION_STATE_CLEANUP) {
		ks_log(KS_LOG_DEBUG, "Connection (%s) disconnecting\n", bc->id);
		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_DETACH);
	}
}

KS_DECLARE(ks_status_t) blade_connection_sending_push(blade_connection_t *bc, cJSON *json)
{
	cJSON *json_copy = NULL;

	ks_assert(bc);
	ks_assert(json);

	json_copy = cJSON_Duplicate(json, 1);
	return ks_q_push(bc->sending, json_copy);
}

KS_DECLARE(ks_status_t) blade_connection_sending_pop(blade_connection_t *bc, cJSON **json)
{
	ks_assert(bc);
	ks_assert(json);

	return ks_q_trypop(bc->sending, (void **)json);
}

KS_DECLARE(const char *) blade_connection_session_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->session;
}

KS_DECLARE(void) blade_connection_session_set(blade_connection_t *bc, const char *id)
{
	ks_assert(bc);

	if (bc->session) ks_pool_free(bc->pool, &bc->session);
	bc->session = ks_pstrdup(bc->pool, id);
}

void *blade_connection_state_thread(ks_thread_t *thread, void *data)
{
	blade_connection_t *bc = NULL;
	blade_connection_state_t state;
	ks_bool_t shutdown = KS_FALSE;

	ks_assert(thread);
	ks_assert(data);

	bc = (blade_connection_t *)data;

	while (!shutdown) {
		state = bc->state;

		switch (state) {
		case BLADE_CONNECTION_STATE_DISCONNECT:
			blade_connection_state_on_disconnect(bc);
			shutdown = KS_TRUE;
			break;
		case BLADE_CONNECTION_STATE_NEW:
			blade_connection_state_on_new(bc);
			break;
		case BLADE_CONNECTION_STATE_CONNECT:
			blade_connection_state_on_connect(bc);
			break;
		case BLADE_CONNECTION_STATE_ATTACH:
			blade_connection_state_on_attach(bc);
			break;
		case BLADE_CONNECTION_STATE_DETACH:
			blade_connection_state_on_detach(bc);
			break;
		case BLADE_CONNECTION_STATE_READY:
			blade_connection_state_on_ready(bc);
			break;
		default: break;
		}
	}

	blade_connection_destroy(&bc);

	return NULL;
}

ks_status_t blade_connection_state_on_disconnect(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_DISCONNECT);
	if (callback) callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_CLEANUP);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_state_on_new(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_NEW);
	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)	blade_connection_disconnect(bc);
	else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_CONNECT);
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_state_on_connect(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_CONNECT);
	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)	blade_connection_disconnect(bc);
	else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_ATTACH);
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_state_on_attach(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_ATTACH);
	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT) blade_connection_disconnect(bc);
	else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
		// @todo this is adding a second lock, since we keep it locked in the callback to allow finishing, we don't want get locking here...
		// or just try unlocking twice to confirm...
		blade_session_t *bs = blade_handle_sessions_get(bc->handle, bc->session);
		ks_assert(bs); // should not happen because bs should still be locked

		blade_session_connections_add(bs, bc->id);

		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_READY);
		blade_session_state_set(bs, BLADE_SESSION_STATE_READY); // @todo only set this if it's not already in the READY state from prior connection

		blade_session_read_unlock(bs); // unlock the session we locked obtaining it above
		blade_session_read_unlock(bs); // unlock the session we expect to be locked during the callback to ensure we can finish attaching
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_state_on_detach(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_DETACH);
	if (callback) callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (bc->session) {
		blade_session_t *bs = blade_handle_sessions_get(bc->handle, bc->session);
		ks_assert(bs);

		blade_session_connections_remove(bs, bc->id);
		blade_session_read_unlock(bs);
		// keep bc->session for later in case something triggers a reconnect later and needs the old session id for a hint
	}
	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_DISCONNECT);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_state_on_ready(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	cJSON *json = NULL;
	blade_session_t *bs = NULL;
	ks_bool_t done = KS_FALSE;

	ks_assert(bc);

	while (blade_connection_sending_pop(bc, &json) == KS_STATUS_SUCCESS && json) {
		ks_status_t ret = bc->transport_callbacks->onsend(bc, json);
		cJSON_Delete(json);

		if (ret != KS_STATUS_SUCCESS) {
			blade_connection_disconnect(bc);
			break;
		}
	}

	while (!done) {
		if (bc->transport_callbacks->onreceive(bc, &json) != KS_STATUS_SUCCESS) {
			blade_connection_disconnect(bc);
			break;
		}

		if (!(done = (json == NULL))) {
			if (!bs) {
				bs = blade_handle_sessions_get(bc->handle, bc->session);
				ks_assert(bs);
			}
			blade_session_receiving_push(bs, json);
			cJSON_Delete(json);
			json = NULL;
		}
	}
	if (bs) blade_session_read_unlock(bs);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_READY);
	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)	blade_connection_disconnect(bc);

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
