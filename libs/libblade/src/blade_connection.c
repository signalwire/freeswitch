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

	void *transport_data;
	blade_transport_callbacks_t *transport_callbacks;

	blade_connection_direction_t direction;
	volatile blade_connection_state_t state;

	ks_cond_t *cond;

	const char *id;
	ks_rwl_t *lock;

	ks_q_t *sending;

	const char *session;
};

void *blade_connection_state_thread(ks_thread_t *thread, void *data);
ks_status_t blade_connection_onstate_startup(blade_connection_t *bc);
ks_status_t blade_connection_onstate_shutdown(blade_connection_t *bc);
ks_status_t blade_connection_onstate_run(blade_connection_t *bc);


static void blade_connection_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
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

	ks_cond_create(&bc->cond, pool);
	ks_assert(bc->cond);

	ks_uuid(&id);
	bc->id = ks_uuid_str(pool, &id);
	ks_assert(bc->id);

	ks_rwl_create(&bc->lock, pool);
	ks_assert(bc->lock);

	ks_q_create(&bc->sending, pool, 0);
	ks_assert(bc->sending);

	ks_pool_set_cleanup(bc, NULL, blade_connection_cleanup);

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
	*bcP = NULL;

	pool = ks_pool_get(bc);
	ks_pool_close(&pool);

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

	blade_connectionmgr_connection_remove(blade_handle_connectionmgr_get(bc->handle), bc);

	while (ks_q_trypop(bc->sending, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_connection_handle_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->handle;
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
	case BLADE_CONNECTION_STATE_SHUTDOWN:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_shutdown_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_shutdown_outbound;
		break;
	case BLADE_CONNECTION_STATE_STARTUP:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_startup_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_startup_outbound;
		break;
	case BLADE_CONNECTION_STATE_RUN:
		if (bc->direction == BLADE_CONNECTION_DIRECTION_INBOUND) callback = bc->transport_callbacks->onstate_run_inbound;
		else if(bc->direction == BLADE_CONNECTION_DIRECTION_OUTBOUND) callback = bc->transport_callbacks->onstate_run_outbound;
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

	ks_cond_lock(bc->cond);

	callback = blade_connection_state_callback_lookup(bc, state);

	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_PRE);

	bc->state = state;

	ks_cond_unlock(bc->cond);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT) blade_connection_disconnect(bc);

	ks_cond_try_signal(bc->cond);
}

KS_DECLARE(blade_connection_state_t) blade_connection_state_get(blade_connection_t *bc)
{
	ks_assert(bc);
	return bc->state;
}

KS_DECLARE(void) blade_connection_disconnect(blade_connection_t *bc)
{
	ks_assert(bc);

	if (bc->state != BLADE_CONNECTION_STATE_SHUTDOWN && bc->state != BLADE_CONNECTION_STATE_CLEANUP) {
		ks_log(KS_LOG_DEBUG, "Connection (%s) disconnecting\n", bc->id);
		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_SHUTDOWN);
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

	if (bc->session) ks_pool_free(&bc->session);
	bc->session = ks_pstrdup(ks_pool_get(bc), id);
}

void *blade_connection_state_thread(ks_thread_t *thread, void *data)
{
	blade_connection_t *bc = NULL;
	blade_connection_state_t state;
	ks_bool_t shutdown = KS_FALSE;

	ks_assert(thread);
	ks_assert(data);

	bc = (blade_connection_t *)data;

	ks_cond_lock(bc->cond);
	while (!shutdown) {
		// Entering the call below, the mutex is expected to be locked and will be unlocked by the call
		ks_cond_timedwait(bc->cond, 100);
		// Leaving the call above, the mutex will be locked after being signalled, timing out, or woken up for any reason

		state = bc->state;

		switch (state) {
		case BLADE_CONNECTION_STATE_SHUTDOWN:
			blade_connection_onstate_shutdown(bc);
			shutdown = KS_TRUE;
			break;
		case BLADE_CONNECTION_STATE_STARTUP:
			blade_connection_onstate_startup(bc);
			break;
		case BLADE_CONNECTION_STATE_RUN:
			blade_connection_onstate_run(bc);
			break;
		default: break;
		}
	}
	ks_cond_unlock(bc->cond);

	blade_connection_destroy(&bc);

	return NULL;
}

ks_status_t blade_connection_onstate_startup(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_STARTUP);
	if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)	blade_connection_disconnect(bc);
	else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
		// @todo this is adding a second lock, since we keep it locked in the callback to allow finishing, we don't want get locking here...
		// or just unlock twice...
		blade_session_t *bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bc->handle), bc->session);
		ks_assert(bs); // should not happen because bs should still be locked

		blade_session_connection_set(bs, bc->id);

		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_RUN);
		blade_session_state_set(bs, BLADE_SESSION_STATE_STARTUP); // if reconnecting, we go from RUN back to STARTUP for the purpose of the reconnect which will return to RUN

		blade_session_read_unlock(bs); // unlock the session we locked obtaining it above
		blade_session_read_unlock(bs); // unlock the session we expect to be locked during the callback to ensure we can finish attaching
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_onstate_shutdown(blade_connection_t *bc)
{
	blade_transport_state_callback_t callback = NULL;

	ks_assert(bc);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_SHUTDOWN);
	if (callback) callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

	if (bc->session) {
		blade_session_t *bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bc->handle), bc->session);
		ks_assert(bs);

		blade_session_connection_set(bs, NULL);
		blade_session_read_unlock(bs);
		// keep bc->session for later in case something triggers a reconnect later and needs the old session id for a hint
	}

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_CLEANUP);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_connection_onstate_run(blade_connection_t *bc)
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
				bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bc->handle), bc->session);
				ks_assert(bs);
			}
			blade_session_receiving_push(bs, json);
			cJSON_Delete(json);
			json = NULL;
		}
	}
	if (bs) blade_session_read_unlock(bs);

	callback = blade_connection_state_callback_lookup(bc, BLADE_CONNECTION_STATE_RUN);
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
