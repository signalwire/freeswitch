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

	ks_bool_t shutdown;
	// @todo add auto generated UUID
    ks_thread_t *state_thread;
	blade_connection_state_t state;
	
	ks_q_t *sending;
	ks_q_t *receiving;
};

void *blade_connection_state_thread(ks_thread_t *thread, void *data);


KS_DECLARE(ks_status_t) blade_connection_create(blade_connection_t **bcP,
												blade_handle_t *bh,
												void *transport_data,
												blade_transport_callbacks_t *transport_callbacks)
{
	blade_connection_t *bc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bcP);
	ks_assert(bh);
	ks_assert(transport_data);
	ks_assert(transport_callbacks);

	pool = blade_handle_pool_get(bh);

	bc = ks_pool_alloc(pool, sizeof(blade_connection_t));
	bc->handle = bh;
	bc->pool = pool;
	bc->transport_data = transport_data;
	bc->transport_callbacks = transport_callbacks;
	ks_q_create(&bc->sending, pool, 0);
	ks_q_create(&bc->receiving, pool, 0);
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
	ks_q_destroy(&bc->receiving);

	ks_pool_free(bc->pool, bcP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_startup(blade_connection_t *bc)
{
	ks_assert(bc);

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NONE);

    if (ks_thread_create_ex(&bc->state_thread,
							blade_connection_state_thread,
							bc,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bc->pool) != KS_STATUS_SUCCESS) {
		// @todo error logging
		blade_connection_disconnect(bc);
		return KS_STATUS_FAIL;
	}
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_shutdown(blade_connection_t *bc)
{
	ks_assert(bc);

	if (bc->state_thread) {
		bc->shutdown = KS_TRUE;
		ks_thread_join(bc->state_thread);
		ks_pool_free(bc->pool, &bc->state_thread);
		bc->shutdown = KS_FALSE;
	}

	//while (ks_q_trypop(bc->sending, (void **)&message) == KS_STATUS_SUCCESS && message) blade_message_discard(&message);
	//while (ks_q_trypop(bc->receiving, (void **)&message) == KS_STATUS_SUCCESS && message) blade_message_discard(&message);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void *) blade_connection_transport_get(blade_connection_t *bc)
{
	ks_assert(bc);

	return bc->transport_data;
}

KS_DECLARE(void) blade_connection_state_set(blade_connection_t *bc, blade_connection_state_t state)
{
	ks_assert(bc);

	bc->transport_callbacks->onstate(bc, state, BLADE_CONNECTION_STATE_CONDITION_PRE);
	bc->state = state;
}

KS_DECLARE(void) blade_connection_disconnect(blade_connection_t *bc)
{
	ks_assert(bc);

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_DISCONNECT);
}

KS_DECLARE(ks_status_t) blade_connection_sending_push(blade_connection_t *bc, blade_identity_t *target, cJSON *json)
{
	ks_assert(bc);
	ks_assert(json);

	// @todo need internal envelope to wrap an identity object and a json object just for the queue

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_sending_pop(blade_connection_t *bc, blade_identity_t **target, cJSON **json)
{
	ks_assert(bc);
	ks_assert(json);

	// @todo need internal envelope to wrap an identity object and a json object just for the queue
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connection_receiving_push(blade_connection_t *bc, cJSON *json)
{
	ks_assert(bc);
	ks_assert(json);

	return ks_q_push(bc->receiving, json);
}

KS_DECLARE(ks_status_t) blade_connection_receiving_pop(blade_connection_t *bc, cJSON **json)
{
	ks_assert(bc);
	ks_assert(json);
	
	return ks_q_trypop(bc->receiving, (void **)json);
}

void *blade_connection_state_thread(ks_thread_t *thread, void *data)
{
	blade_connection_t *bc = NULL;
	blade_connection_state_hook_t hook;

	ks_assert(thread);
	ks_assert(data);

	bc = (blade_connection_t *)data;

	while (!bc->shutdown) {
		// @todo need to get messages from the transport into receiving queue, and pop messages from sending queue to write out using transport
		// sending is relatively easy, but receiving cannot occur universally due to cases like kws_init() blocking and expecting data to be on the wire
		// and other transports may have similar behaviours, but CONNECTIN, ATTACH, and READY require async message passing into application layer
		// and sending whenever the response hits the queue
		
		// @todo it's possible that onstate could handle receiving and sending messages during the appropriate states, but this means some states
		// like CONNECTIN which may send and receive multiple messages require BYPASSing until the application layer updates the state or disconnects
		
		hook = bc->transport_callbacks->onstate(bc, bc->state, BLADE_CONNECTION_STATE_CONDITION_POST);
		if (hook == BLADE_CONNECTION_STATE_HOOK_DISCONNECT)
			blade_connection_disconnect(bc);
		else if (hook == BLADE_CONNECTION_STATE_HOOK_SUCCESS) {
			// @todo pop from sending queue, and pass to transport callback to send out
			switch (bc->state) {
			case BLADE_CONNECTION_STATE_NEW:
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_CONNECT);
				break;
			case BLADE_CONNECTION_STATE_CONNECT:
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_ATTACH);
				break;
			case BLADE_CONNECTION_STATE_ATTACH:
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_READY);
				break;
			case BLADE_CONNECTION_STATE_DETACH:
				blade_connection_disconnect(bc);
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
