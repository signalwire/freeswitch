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

typedef struct blade_module_master_s blade_module_master_t;

struct blade_module_master_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	blade_module_t *module;

	blade_space_t *blade_space;
	blade_space_t *blade_application_space;
};


ks_bool_t blade_register_request_handler(blade_module_t *bm, blade_request_t *breq);
// @todo blade_unregister_request_handler for more graceful shutdowns which intend to disconnect, and won't reconnect, which expire a session immediately
ks_bool_t blade_application_register_request_handler(blade_module_t *bm, blade_request_t *breq); // @todo response of registration indicates if you are the primary, or a slave
// @todo blade_application_unregister_request_handler for ability to unregister a slave (or primary) from the application, upon last node unregistering, the application entry would be automatically destroyed
// @todo event (or request to confirm acceptance with a response?) that allows a master to tell a slave it's the new primary for an application it has registered to provide when a primary disconnects, or a
// primary change is requested externally
// @todo to avoid a race condition, if a slave gets unexpected primary calls before being notified by an event, should it assume it has become the primary and not yet notified?

static blade_module_callbacks_t g_module_master_callbacks =
{
	blade_module_master_on_startup,
	blade_module_master_on_shutdown,
};


static void blade_module_master_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_module_master_t *bm_master = (blade_module_master_t *)ptr;

	//ks_assert(bm_master);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_module_master_create(blade_module_t **bmP, blade_handle_t *bh)
{
	blade_module_master_t *bm_master = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bmP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

    bm_master = ks_pool_alloc(pool, sizeof(blade_module_master_t));
	bm_master->handle = bh;
	bm_master->pool = pool;

	blade_module_create(&bm_master->module, bh, pool, bm_master, &g_module_master_callbacks);

	ks_pool_set_cleanup(pool, bm_master, NULL, blade_module_master_cleanup);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*bmP = bm_master->module;

	return KS_STATUS_SUCCESS;
}


ks_status_t blade_module_master_config(blade_module_master_t *bm_master, config_setting_t *config)
{
	ks_assert(bm_master);
	ks_assert(config);

	ks_log(KS_LOG_DEBUG, "Configured\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_master_on_startup(blade_module_t *bm, config_setting_t *config)
{
	blade_module_master_t *bm_master = NULL;
	blade_space_t *space = NULL;
	blade_method_t *method = NULL;

	ks_assert(bm);
	ks_assert(config);

	bm_master = (blade_module_master_t *)blade_module_data_get(bm);

    if (blade_module_master_config(bm_master, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_module_master_config failed\n");
		return KS_STATUS_FAIL;
	}

	blade_space_create(&space, bm_master->handle, bm, "blade");
	ks_assert(space);

	bm_master->blade_space = space;

	blade_method_create(&method, space, "register", blade_register_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_handle_space_register(space);


	blade_space_create(&space, bm_master->handle, bm, "blade.application");
	ks_assert(space);

	bm_master->blade_application_space = space;

	blade_method_create(&method, space, "register", blade_application_register_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_handle_space_register(space);


	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_master_on_shutdown(blade_module_t *bm)
{
	blade_module_master_t *bm_master = NULL;

	ks_assert(bm);

	bm_master = (blade_module_master_t *)blade_module_data_get(bm);

	if (bm_master->blade_application_space) blade_handle_space_unregister(bm_master->blade_application_space);
	if (bm_master->blade_space) blade_handle_space_unregister(bm_master->blade_space);

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

ks_bool_t blade_register_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_master_t *bm_master = NULL;
	blade_session_t *bs = NULL;
	cJSON *params = NULL;
	cJSON *res = NULL;
	const char *params_identity = NULL;
	const char *identity = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_master = (blade_module_master_t *)blade_module_data_get(bm);
	ks_assert(bm_master);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	blade_session_properties_write_lock(bs, KS_TRUE);

	params = cJSON_GetObjectItem(breq->message, "params"); // @todo cache this in blade_request_t for quicker/easier access
	if (!params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to register with no 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -32602, "Missing params object");
	}
	else if (!(params_identity = cJSON_GetObjectCstr(params, "identity"))) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to register with no 'identity'\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -32602, "Missing params identity string");
	} else {
		identity = blade_session_identity_get(bs);
		if (identity && identity[0]) {
			ks_log(KS_LOG_DEBUG, "Session (%s) attempted to register with master but is already registered as %s\n", blade_session_id_get(bs), identity);
			blade_rpc_error_create(&res, NULL, breq->message_id, -1000, "Already registered");
		} else {
			// @todo plug in authentication to confirm if this registration is permitted, just allow it for now as long as it's not already in use

			blade_rpc_response_create(&res, NULL, breq->message_id);

			// @todo this is completely unfinished, return to finish this after catching up other changes
			//blade_handle_session_identify(bh, identity, bs);
			//blade_session_identity_set(bs, params_identity);
		}
	}

	blade_session_properties_write_unlock(bs);

	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	return KS_FALSE;
}

ks_bool_t blade_application_register_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_master_t *bm_master = NULL;
	blade_session_t *bs = NULL;
	//cJSON *res = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_master = (blade_module_master_t *)blade_module_data_get(bm);
	ks_assert(bm_master);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	//blade_rpc_error_create(&res, NULL, breq->message_id, -10000, "???");
	//blade_rpc_response_create(&res, NULL, breq->message_id);


	//blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	//cJSON_Delete(res);

	return KS_FALSE;
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
