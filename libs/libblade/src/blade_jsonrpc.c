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

struct blade_jsonrpc_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	const char *method;

	blade_jsonrpc_request_callback_t callback;
	void *callback_data;
};

struct blade_jsonrpc_request_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	const char *session_id;

	cJSON *message;
	const char *message_id; // pulled from message for easier keying
	blade_jsonrpc_response_callback_t callback;
	// @todo ttl to wait for response before injecting an error response locally
};

struct blade_jsonrpc_response_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	const char *session_id;

	blade_jsonrpc_request_t *request;

	cJSON *message;
};


static void blade_jsonrpc_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_jsonrpc_t *bjsonrpc = (blade_jsonrpc_t *)ptr;

	//ks_assert(bjsonrpc);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_jsonrpc_create(blade_jsonrpc_t **bjsonrpcP, blade_handle_t *bh, const char *method, blade_jsonrpc_request_callback_t callback, void *callback_data)
{
	blade_jsonrpc_t *bjsonrpc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bjsonrpcP);
	ks_assert(bh);
	ks_assert(method);
	ks_assert(callback);

	ks_pool_open(&pool);
	ks_assert(pool);

	bjsonrpc = ks_pool_alloc(pool, sizeof(blade_jsonrpc_t));
	bjsonrpc->handle = bh;
	bjsonrpc->pool = pool;
	bjsonrpc->method = ks_pstrdup(pool, method);
	bjsonrpc->callback = callback;
	bjsonrpc->callback_data = callback_data;

	ks_pool_set_cleanup(pool, bjsonrpc, NULL, blade_jsonrpc_cleanup);

	*bjsonrpcP = bjsonrpc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_jsonrpc_destroy(blade_jsonrpc_t **bjsonrpcP)
{
	blade_jsonrpc_t *bjsonrpc = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bjsonrpcP);
	ks_assert(*bjsonrpcP);

	bjsonrpc = *bjsonrpcP;

	pool = bjsonrpc->pool;

	ks_pool_close(&pool);

	*bjsonrpcP = NULL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_jsonrpc_handle_get(blade_jsonrpc_t *bjsonrpc)
{
	ks_assert(bjsonrpc);

	return bjsonrpc->handle;
}

KS_DECLARE(const char *) blade_jsonrpc_method_get(blade_jsonrpc_t *bjsonrpc)
{
	ks_assert(bjsonrpc);

	return bjsonrpc->method;
}

KS_DECLARE(blade_jsonrpc_request_callback_t) blade_jsonrpc_callback_get(blade_jsonrpc_t *bjsonrpc)
{
	ks_assert(bjsonrpc);

	return bjsonrpc->callback;
}

KS_DECLARE(void *) blade_jsonrpc_callback_data_get(blade_jsonrpc_t *bjsonrpc)
{
	ks_assert(bjsonrpc);

	return bjsonrpc->callback_data;
}


static void blade_jsonrpc_request_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_jsonrpc_request_t *bjsonrpcreq = (blade_jsonrpc_request_t *)ptr;

	ks_assert(bjsonrpcreq);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free(bjsonrpcreq->pool, (void **)&bjsonrpcreq->session_id);
		cJSON_Delete(bjsonrpcreq->message);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_jsonrpc_request_create(blade_jsonrpc_request_t **bjsonrpcreqP,
													 blade_handle_t *bh,
													 ks_pool_t *pool,
													 const char *session_id,
													 cJSON *json,
													 blade_jsonrpc_response_callback_t callback)
{
	blade_jsonrpc_request_t *bjsonrpcreq = NULL;

	ks_assert(bjsonrpcreqP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(session_id);
	ks_assert(json);

	bjsonrpcreq = ks_pool_alloc(pool, sizeof(blade_jsonrpc_request_t));
	bjsonrpcreq->handle = bh;
	bjsonrpcreq->pool = pool;
	bjsonrpcreq->session_id = ks_pstrdup(pool, session_id);
	bjsonrpcreq->message = cJSON_Duplicate(json, 1);
	bjsonrpcreq->message_id = cJSON_GetObjectCstr(bjsonrpcreq->message, "id");
	bjsonrpcreq->callback = callback;

	ks_pool_set_cleanup(pool, bjsonrpcreq, NULL, blade_jsonrpc_request_cleanup);

	*bjsonrpcreqP = bjsonrpcreq;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_jsonrpc_request_destroy(blade_jsonrpc_request_t **bjsonrpcreqP)
{
	blade_jsonrpc_request_t *bjsonrpcreq = NULL;

	ks_assert(bjsonrpcreqP);
	ks_assert(*bjsonrpcreqP);

	bjsonrpcreq = *bjsonrpcreqP;

	ks_pool_free(bjsonrpcreq->pool, bjsonrpcreqP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_jsonrpc_request_handle_get(blade_jsonrpc_request_t *bjsonrpcreq)
{
	ks_assert(bjsonrpcreq);
	return bjsonrpcreq->handle;
}

KS_DECLARE(const char *) blade_jsonrpc_request_messageid_get(blade_jsonrpc_request_t *bjsonrpcreq)
{
	ks_assert(bjsonrpcreq);
	return bjsonrpcreq->message_id;
}

KS_DECLARE(blade_jsonrpc_response_callback_t) blade_jsonrpc_request_callback_get(blade_jsonrpc_request_t *bjsonrpcreq)
{
	ks_assert(bjsonrpcreq);
	return bjsonrpcreq->callback;
}

KS_DECLARE(ks_status_t) blade_jsonrpc_request_raw_create(ks_pool_t *pool, cJSON **json, cJSON **params, const char **id, const char *method)
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
	ks_pool_free(pool, &mid);

	cJSON_AddStringToObject(root, "method", method);

	p = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "params", p);

	*json = root;
	if (params) *params = p;
	if (id) *id = cJSON_GetObjectCstr(root, "id");

	return KS_STATUS_SUCCESS;
}


static void blade_jsonrpc_response_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_jsonrpc_response_t *bjsonrpcres = (blade_jsonrpc_response_t *)ptr;

	ks_assert(bjsonrpcres);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free(bjsonrpcres->pool, (void **)&bjsonrpcres->session_id);
		blade_jsonrpc_request_destroy(&bjsonrpcres->request);
		cJSON_Delete(bjsonrpcres->message);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_jsonrpc_response_create(blade_jsonrpc_response_t **bjsonrpcresP,
													  blade_handle_t *bh,
													  ks_pool_t *pool,
													  const char *session_id,
													  blade_jsonrpc_request_t *bjsonrpcreq,
													  cJSON *json)
{
	blade_jsonrpc_response_t *bjsonrpcres = NULL;

	ks_assert(bjsonrpcresP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(session_id);
	ks_assert(bjsonrpcreq);
	ks_assert(json);

	bjsonrpcres = ks_pool_alloc(pool, sizeof(blade_jsonrpc_response_t));
	bjsonrpcres->handle = bh;
	bjsonrpcres->pool = pool;
	bjsonrpcres->session_id = ks_pstrdup(pool, session_id);
	bjsonrpcres->request = bjsonrpcreq;
	bjsonrpcres->message = cJSON_Duplicate(json, 1);

	ks_pool_set_cleanup(pool, bjsonrpcres, NULL, blade_jsonrpc_response_cleanup);

	*bjsonrpcresP = bjsonrpcres;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_jsonrpc_response_destroy(blade_jsonrpc_response_t **bjsonrpcresP)
{
	blade_jsonrpc_response_t *bjsonrpcres = NULL;

	ks_assert(bjsonrpcresP);
	ks_assert(*bjsonrpcresP);

	bjsonrpcres = *bjsonrpcresP;

	ks_pool_free(bjsonrpcres->pool, bjsonrpcresP);

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_jsonrpc_response_raw_create(cJSON **json, cJSON **result, const char *id)
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

KS_DECLARE(ks_status_t) blade_jsonrpc_error_raw_create(cJSON **json, cJSON **error, const char *id, int32_t code, const char *message)
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
