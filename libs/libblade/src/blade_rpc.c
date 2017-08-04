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

struct blade_rpc_s {
	blade_handle_t *handle;

	const char *method;
	const char *protocol;
	const char *realm;

	blade_rpc_request_callback_t callback;
	void *data;
};

struct blade_rpc_request_s {
	blade_handle_t *handle;

	const char *session_id;

	cJSON *message;
	const char *message_id; // pulled from message for easier keying
	blade_rpc_response_callback_t callback;
	void *data;
	// @todo ttl to wait for response before injecting an error response locally
};

struct blade_rpc_response_s {
	blade_handle_t *handle;

	const char *session_id;

	blade_rpc_request_t *request;

	cJSON *message;
};


static void blade_rpc_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_rpc_t *brpc = (blade_rpc_t *)ptr;

	//ks_assert(brpc);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		// @todo delete data if present, requires update to ks_pool for self tracking the pool in allocation header
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_rpc_create(blade_rpc_t **brpcP, blade_handle_t *bh, const char *method, const char *protocol, const char *realm, blade_rpc_request_callback_t callback, void *data)
{
	blade_rpc_t *brpc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(brpcP);
	ks_assert(bh);
	ks_assert(method);
	ks_assert(callback);

	ks_pool_open(&pool);
	ks_assert(pool);

	brpc = ks_pool_alloc(pool, sizeof(blade_rpc_t));
	brpc->handle = bh;
	brpc->method = ks_pstrdup(pool, method);
	if (protocol) brpc->protocol = ks_pstrdup(pool, protocol);
	if (realm) brpc->realm = ks_pstrdup(pool, realm);
	brpc->callback = callback;
	brpc->data = data;

	ks_pool_set_cleanup(brpc, NULL, blade_rpc_cleanup);

	*brpcP = brpc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_rpc_destroy(blade_rpc_t **brpcP)
{
	blade_rpc_t *brpc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(brpcP);
	ks_assert(*brpcP);

	brpc = *brpcP;
	*brpcP = NULL;

	pool = ks_pool_get(brpc);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_rpc_handle_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->handle;
}

KS_DECLARE(const char *) blade_rpc_method_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->method;
}

KS_DECLARE(const char *) blade_rpc_protocol_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->protocol;
}

KS_DECLARE(const char *) blade_rpc_realm_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->realm;
}

KS_DECLARE(blade_rpc_request_callback_t) blade_rpc_callback_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->callback;
}

KS_DECLARE(void *) blade_rpc_data_get(blade_rpc_t *brpc)
{
	ks_assert(brpc);

	return brpc->data;
}


static void blade_rpc_request_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_rpc_request_t *brpcreq = (blade_rpc_request_t *)ptr;

	ks_assert(brpcreq);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free((void **)&brpcreq->session_id);
		cJSON_Delete(brpcreq->message);
		// @todo delete data if present, requires update to ks_pool for self tracking the pool in allocation header
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_rpc_request_create(blade_rpc_request_t **brpcreqP,
													 blade_handle_t *bh,
													 ks_pool_t *pool,
													 const char *session_id,
													 cJSON *json,
													 blade_rpc_response_callback_t callback,
													 void *data)
{
	blade_rpc_request_t *brpcreq = NULL;

	ks_assert(brpcreqP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(session_id);
	ks_assert(json);

	brpcreq = ks_pool_alloc(pool, sizeof(blade_rpc_request_t));
	brpcreq->handle = bh;
	brpcreq->session_id = ks_pstrdup(pool, session_id);
	brpcreq->message = cJSON_Duplicate(json, 1);
	brpcreq->message_id = cJSON_GetObjectCstr(brpcreq->message, "id");
	brpcreq->callback = callback;
	brpcreq->data = data;

	ks_pool_set_cleanup(brpcreq, NULL, blade_rpc_request_cleanup);

	*brpcreqP = brpcreq;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_rpc_request_destroy(blade_rpc_request_t **brpcreqP)
{
	blade_rpc_request_t *brpcreq = NULL;

	ks_assert(brpcreqP);
	ks_assert(*brpcreqP);

	brpcreq = *brpcreqP;

	ks_pool_free(brpcreqP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_rpc_request_duplicate(blade_rpc_request_t **brpcreqP, blade_rpc_request_t *brpcreq)
{
	return blade_rpc_request_create(brpcreqP, brpcreq->handle, ks_pool_get(brpcreq), brpcreq->session_id, brpcreq->message, brpcreq->callback, brpcreq->data);
}

KS_DECLARE(blade_handle_t *) blade_rpc_request_handle_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->handle;
}

KS_DECLARE(const char *) blade_rpc_request_sessionid_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->session_id;
}

KS_DECLARE(cJSON *) blade_rpc_request_message_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->message;
}

KS_DECLARE(const char *) blade_rpc_request_messageid_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->message_id;
}

KS_DECLARE(blade_rpc_response_callback_t) blade_rpc_request_callback_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->callback;
}

KS_DECLARE(void *) blade_rpc_request_data_get(blade_rpc_request_t *brpcreq)
{
	ks_assert(brpcreq);
	return brpcreq->data;
}

KS_DECLARE(ks_status_t) blade_rpc_request_raw_create(ks_pool_t *pool, cJSON **json, cJSON **params, const char **id, const char *method)
{
	cJSON *root = NULL;
	cJSON *p = NULL;
	uuid_t msgid;
	const char *mid = NULL;

	ks_assert(pool);
	ks_assert(json);
	ks_assert(method);

	root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "jsonrpc", "2.0");

	ks_uuid(&msgid);
	mid = ks_uuid_str(pool, &msgid);
	cJSON_AddStringToObject(root, "id", mid);
	ks_pool_free(&mid);

	cJSON_AddStringToObject(root, "method", method);

	p = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "params", p);

	*json = root;
	if (params) *params = p;
	if (id) *id = cJSON_GetObjectCstr(root, "id");

	return KS_STATUS_SUCCESS;
}


static void blade_rpc_response_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_rpc_response_t *brpcres = (blade_rpc_response_t *)ptr;

	ks_assert(brpcres);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free((void **)&brpcres->session_id);
		blade_rpc_request_destroy(&brpcres->request);
		cJSON_Delete(brpcres->message);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_rpc_response_create(blade_rpc_response_t **brpcresP,
													  blade_handle_t *bh,
													  ks_pool_t *pool,
													  const char *session_id,
													  blade_rpc_request_t *brpcreq,
													  cJSON *json)
{
	blade_rpc_response_t *brpcres = NULL;

	ks_assert(brpcresP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(session_id);
	ks_assert(brpcreq);
	ks_assert(json);

	brpcres = ks_pool_alloc(pool, sizeof(blade_rpc_response_t));
	brpcres->handle = bh;
	brpcres->session_id = ks_pstrdup(pool, session_id);
	brpcres->request = brpcreq;
	brpcres->message = cJSON_Duplicate(json, 1);

	ks_pool_set_cleanup(brpcres, NULL, blade_rpc_response_cleanup);

	*brpcresP = brpcres;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_rpc_response_destroy(blade_rpc_response_t **brpcresP)
{
	blade_rpc_response_t *brpcres = NULL;

	ks_assert(brpcresP);
	ks_assert(*brpcresP);

	brpcres = *brpcresP;

	ks_pool_free(brpcresP);

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_rpc_response_raw_create(cJSON **json, cJSON **result, const char *id)
{
	cJSON *root = NULL;
	cJSON *r = NULL;

	ks_assert(json);
	ks_assert(id);

	root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "jsonrpc", "2.0");

	cJSON_AddStringToObject(root, "id", id);

	r = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "result", r);

	*json = root;
	if (result) *result = r;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_rpc_response_handle_get(blade_rpc_response_t *brpcres)
{
	ks_assert(brpcres);
	return brpcres->handle;
}

KS_DECLARE(const char *) blade_rpc_response_sessionid_get(blade_rpc_response_t *brpcres)
{
	ks_assert(brpcres);
	return brpcres->session_id;
}

KS_DECLARE(blade_rpc_request_t *) blade_rpc_response_request_get(blade_rpc_response_t *brpcres)
{
	ks_assert(brpcres);
	return brpcres->request;
}

KS_DECLARE(cJSON *) blade_rpc_response_message_get(blade_rpc_response_t *brpcres)
{
	ks_assert(brpcres);
	return brpcres->message;
}

KS_DECLARE(ks_status_t) blade_rpc_error_raw_create(cJSON **json, cJSON **error, const char *id, int32_t code, const char *message)
{
	cJSON *root = NULL;
	cJSON *e = NULL;

	ks_assert(json);
	//ks_assert(id);
	ks_assert(message);

	root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "jsonrpc", "2.0");

	if (id) cJSON_AddStringToObject(root, "id", id);

	e = cJSON_CreateObject();
	cJSON_AddNumberToObject(e, "code", code);
	cJSON_AddStringToObject(e, "message", message);
	cJSON_AddItemToObject(root, "error", e);

	*json = root;
	if (error) *error = e;

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
