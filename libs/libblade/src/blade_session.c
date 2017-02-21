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
	list_t connections;
	
	ks_q_t *sending;
	ks_q_t *receiving;
};

void *blade_session_state_thread(ks_thread_t *thread, void *data);


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

	list_init(&bs->connections);
	ks_q_create(&bs->sending, pool, 0);
	ks_assert(bs->sending);
	ks_q_create(&bs->receiving, pool, 0);
	ks_assert(bs->receiving);

	*bsP = bs;

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

    ks_pool_free(bs->pool, &bs->id);

	ks_pool_free(bs->pool, bsP);

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

	return KS_STATUS_SUCCESS;
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

KS_DECLARE(void) blade_session_state_set(blade_session_t *bs, blade_session_state_t state)
{
	ks_assert(bs);

	bs->state = state;
}

KS_DECLARE(void) blade_session_hangup(blade_session_t *bs)
{
	ks_assert(bs);

	if (bs->state != BLADE_SESSION_STATE_HANGUP && bs->state != BLADE_SESSION_STATE_DESTROY)
		blade_session_state_set(bs, BLADE_SESSION_STATE_HANGUP);
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
		// @todo error logging... this shouldn't happen
		return KS_STATUS_FAIL;
	}
		
	bc = blade_handle_connections_get(bs->handle, cid);
	if (!bc) {
		// @todo error logging... this shouldn't happen
		return KS_STATUS_FAIL;
	}

	*bcP = bc;

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
	}
	
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

// @todo receive queue push and pop

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
				if (blade_session_connections_choose(bs, json, &bc) == KS_STATUS_SUCCESS) blade_connection_sending_push(bc, json);
				cJSON_Delete(json);
			}
		}

		switch (state) {
		case BLADE_SESSION_STATE_DESTROY:
			return NULL;
		case BLADE_SESSION_STATE_HANGUP:
			// @todo detach from session if this connection is attached
			blade_session_state_set(bs, BLADE_SESSION_STATE_DESTROY);
			break;
		case BLADE_SESSION_STATE_READY:
			// @todo pop from session receiving queue and pass to blade_protocol_process()
			break;
		default: break;
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
