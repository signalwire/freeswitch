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

	void *transport_init_data;
	void *transport_data;
	blade_transport_callbacks_t *transport_callbacks;

	ks_bool_t shutdown;
	blade_connection_direction_t direction;
    ks_thread_t *state_thread;
	blade_connection_state_t state;

	const char *id;
	ks_rwl_t *lock;
	
	ks_q_t *sending;
};

void *blade_connection_state_thread(ks_thread_t *thread, void *data);


KS_DECLARE(ks_status_t) blade_connection_create(blade_connection_t **bcP,
												blade_handle_t *bh,
												void *transport_init_data,
												blade_transport_callbacks_t *transport_callbacks)
{
	blade_connection_t *bc = NULL;
	ks_pool_t *pool = NULL;
	uuid_t id;

	ks_assert(bcP);
	ks_assert(bh);
	ks_assert(transport_callbacks);

	pool = blade_handle_pool_get(bh);

	bc = ks_pool_alloc(pool, sizeof(blade_connection_t));
	bc->handle = bh;
	bc->pool = pool;
	bc->transport_init_data = transport_init_data;
	bc->transport_callbacks = transport_callbacks;

	ks_uuid(&id);
	bc->id = ks_uuid_str(pool, &id);
	ks_assert(bc->id);

	ks_rwl_create(&bc->lock, pool);
	ks_assert(bc->lock);
	
	ks_q_create(&bc->sending, pool, 0);
	ks_assert(bc->sending);

	*bcP = bc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_destroy(blade_connection_t **bcP)
{
	blade_connection_t *bc = NULL;

	ks_assert(bcP);
	ks_assert(*bcP);

	bc = *bcP;

	blade_connection_shutdown(bc);

	ks_q_destroy(&bc->sending);

	ks_rwl_destroy(&bc->lock);

	ks_pool_free(bc->pool, &bc->id);

	ks_pool_free(bc->pool, bcP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_startup(blade_connection_t *bc, blade_connection_direction_t direction)
{
	ks_assert(bc);

	bc->direction = direction;
	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NONE);

    if (ks_thread_create_ex(&bc->state_thread,
							blade_connection_state_thread,
							bc,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bc->pool) != KS_STATUS_SUCCESS) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_shutdown(blade_connection_t *bc)
{
	cJSON *json = NULL;

	ks_assert(bc);

	if (bc->state_thread) {
		bc->shutdown = KS_TRUE;
		ks_thread_join(bc->state_thread);
		ks_pool_free(bc->pool, &bc->state_thread);
		bc->shutdown = KS_FALSE;
	}

	while (ks_q_trypop(bc->sending, (void **)&json) == KS_STATUS_SUCCESS && json) cJSON_Delete(json);

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


KS_DECLARE(void *) blade_connection_transport_init_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->transport_init_data;
}

KS_DECLARE(void *) blade_connection_transport_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->transport_data;
}

KS_DECLARE(void) blade_connection_transport_set(blade_connection_t *bc, void *transport_data)
{
	ks_assert(bc);

	bc->transport_data = transport_data;
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

KS_DECLARE(void) blade_connection_disconnect(blade_connection_t *bc)
{
	ks_assert(bc);

	if (bc->state != BLADE_CONNECTION_STATE_DETACH && bc->state != BLADE_CONNECTION_STATE_DISCONNECT)
		blade_connection_state_set(bc, BLADE_CONNECTION_STATE_DETACH);
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


void *blade_connection_state_thread(ks_thread_t *thread, void *data)
{
	blade_connection_t *bc = NULL;
	blade_connection_state_t state;
	blade_transport_state_callback_t callback = NULL;
	blade_connection_state_hook_t hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	cJSON *json = NULL;

	ks_assert(thread);
	ks_assert(data);

	bc = (blade_connection_t *)data;

	while (!bc->shutdown) {
		
		state = bc->state;
		hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
		callback = blade_connection_state_callback_lookup(bc, state);

		// @todo only READY state?
		if (state != BLADE_CONNECTION_STATE_DETACH && state != BLADE_CONNECTION_STATE_DISCONNECT) {
			while (blade_connection_sending_pop(bc, &json) == KS_STATUS_SUCCESS && json) {
				ks_status_t ret = bc->transport_callbacks->onsend(bc, json);
				cJSON_Delete(json);

				if (ret != KS_STATUS_SUCCESS) {
					blade_connection_disconnect(bc);
					break;
				}
			}
		}

		if (state == BLADE_CONNECTION_STATE_READY) {
			ks_bool_t done = KS_FALSE;
			while (!done) {
				if (bc->transport_callbacks->onreceive(bc, &json) != KS_STATUS_SUCCESS) {
					blade_connection_disconnect(bc);
					break;
				}
				if (!(done = (json == NULL))) {
					// @todo push json to session receiving queue
					cJSON_Delete(json);
					json = NULL;
				}
			}
		}
		
		if (callback) hook = callback(bc, BLADE_CONNECTION_STATE_CONDITION_POST);

		if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT && (state == BLADE_CONNECTION_STATE_DETACH || state == BLADE_CONNECTION_STATE_DISCONNECT))
			hook = BLADE_CONNECTION_STATE_HOOK_SUCCESS;

		if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)	blade_connection_disconnect(bc);
		else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
			switch (state) {
			case BLADE_CONNECTION_STATE_DISCONNECT:
				return NULL;
			case BLADE_CONNECTION_STATE_NEW:
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_CONNECT);
				break;
			case BLADE_CONNECTION_STATE_CONNECT:
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_ATTACH);
				break;
			case BLADE_CONNECTION_STATE_ATTACH:
				// @todo receive message with nullable session id for reconnect and some sort of secure token for a reconnect challenge?
				// determine how much of session management is handled here... do we process these session negotiation messages without
				// passing it up to the application layer? or does the application layer give back a session and build the response?
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_READY);
				break;
			case BLADE_CONNECTION_STATE_DETACH:
				// @todo detach from session if this connection is attached
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_DISCONNECT);
				break;
			default: break;
			}
		}
	}

	return NULL;
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
