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

typedef struct blade_module_chat_s blade_module_chat_t;

struct blade_module_chat_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_module_t *module;
	blade_module_callbacks_t *module_callbacks;

	const char *session_state_callback_id;
	list_t participants;
};


ks_status_t blade_module_chat_create(blade_module_chat_t **bm_chatP, blade_handle_t *bh);
ks_status_t blade_module_chat_destroy(blade_module_chat_t **bm_chatP);

// @todo remove exporting this, it's only temporary until DSO loading is in place so wss module can be loaded
KS_DECLARE(ks_status_t) blade_module_chat_on_load(blade_module_t **bmP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_module_chat_on_unload(blade_module_t *bm);
KS_DECLARE(ks_status_t) blade_module_chat_on_startup(blade_module_t *bm, config_setting_t *config);
KS_DECLARE(ks_status_t) blade_module_chat_on_shutdown(blade_module_t *bm);

void blade_module_chat_on_session_state(blade_session_t *bs, blade_session_state_condition_t condition, void *data);

ks_bool_t blade_chat_join_request_handler(blade_module_t *bm, blade_request_t *breq);
ks_bool_t blade_chat_leave_request_handler(blade_module_t *bm, blade_request_t *breq);
ks_bool_t blade_chat_send_request_handler(blade_module_t *bm, blade_request_t *breq);

static blade_module_callbacks_t g_module_chat_callbacks =
{
	blade_module_chat_on_load,
	blade_module_chat_on_unload,
	blade_module_chat_on_startup,
	blade_module_chat_on_shutdown,
};



ks_status_t blade_module_chat_create(blade_module_chat_t **bm_chatP, blade_handle_t *bh)
{
	blade_module_chat_t *bm_chat = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bm_chatP);
	ks_assert(bh);

	pool = blade_handle_pool_get(bh);

    bm_chat = ks_pool_alloc(pool, sizeof(blade_module_chat_t));
	bm_chat->handle = bh;
	bm_chat->pool = pool;
	bm_chat->tpool = blade_handle_tpool_get(bh);
	bm_chat->session_state_callback_id = NULL;
	list_init(&bm_chat->participants);

	blade_module_create(&bm_chat->module, bh, bm_chat, &g_module_chat_callbacks);
	bm_chat->module_callbacks = &g_module_chat_callbacks;

	*bm_chatP = bm_chat;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_chat_destroy(blade_module_chat_t **bm_chatP)
{
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bm_chatP);
	ks_assert(*bm_chatP);

	bm_chat = *bm_chatP;

	blade_module_chat_on_shutdown(bm_chat->module);

	list_destroy(&bm_chat->participants);

	blade_module_destroy(&bm_chat->module);

	ks_pool_free(bm_chat->pool, bm_chatP);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_chat_on_load(blade_module_t **bmP, blade_handle_t *bh)
{
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bmP);
	ks_assert(bh);

	blade_module_chat_create(&bm_chat, bh);
	ks_assert(bm_chat);

	*bmP = bm_chat->module;

	ks_log(KS_LOG_DEBUG, "Loaded\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_chat_on_unload(blade_module_t *bm)
{
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bm);

	bm_chat = blade_module_data_get(bm);

	blade_module_chat_destroy(&bm_chat);

	ks_log(KS_LOG_DEBUG, "Unloaded\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_chat_config(blade_module_chat_t *bm_chat, config_setting_t *config)
{
	config_setting_t *chat = NULL;

	ks_assert(bm_chat);
	ks_assert(config);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	chat = config_setting_get_member(config, "chat");
	if (chat) {
	}


	// Configuration is valid, now assign it to the variables that are used
	// If the configuration was invalid, then this does not get changed

	ks_log(KS_LOG_DEBUG, "Configured\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_chat_on_startup(blade_module_t *bm, config_setting_t *config)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_space_t *space = NULL;
	blade_method_t *method = NULL;

	ks_assert(bm);
	ks_assert(config);

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);

    if (blade_module_chat_config(bm_chat, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_module_chat_config failed\n");
		return KS_STATUS_FAIL;
	}

	blade_space_create(&space, bm_chat->handle, bm, "blade.chat");
	ks_assert(space);

	blade_method_create(&method, space, "join", blade_chat_join_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_method_create(&method, space, "leave", blade_chat_leave_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_method_create(&method, space, "send", blade_chat_send_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_handle_space_register(space);

	blade_handle_session_state_callback_register(blade_module_handle_get(bm), bm, blade_module_chat_on_session_state, &bm_chat->session_state_callback_id);

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_chat_on_shutdown(blade_module_t *bm)
{
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bm);

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	if (bm_chat->session_state_callback_id)	blade_handle_session_state_callback_unregister(blade_module_handle_get(bm), bm_chat->session_state_callback_id);
	bm_chat->session_state_callback_id = NULL;

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

void blade_module_chat_on_session_state(blade_session_t *bs, blade_session_state_condition_t condition, void *data)
{
	blade_module_t *bm = NULL;
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bs);
	ks_assert(data);

	bm = (blade_module_t *)data;
	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	if (blade_session_state_get(bs) == BLADE_SESSION_STATE_HANGUP && condition == BLADE_SESSION_STATE_CONDITION_PRE) {
		cJSON *props = NULL;

		ks_log(KS_LOG_DEBUG, "Removing session from chat participants if present\n");

		props = blade_session_properties_get(bs);
		ks_assert(props);

		cJSON_DeleteItemFromObject(props, "blade.chat.participant");

		list_delete(&bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and search manually, also free the id
	}
}

ks_bool_t blade_chat_join_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *res = NULL;
	cJSON *props = NULL;
	cJSON *props_participant = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	// @todo properties only used to demonstrate a flexible container for session data, should just rely on the participants list/hash
	blade_session_properties_write_lock(bs, KS_TRUE);

	props = blade_session_properties_get(bs);
	ks_assert(props);

	props_participant = cJSON_GetObjectItem(props, "blade.chat.participant");
	if (props_participant && props_participant->type == cJSON_True) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to join chat but is already a participant\n", blade_session_id_get(bs));
		blade_rpc_error_create(breq->pool, &res, NULL, breq->message_id, -10000, "Already a participant of chat");
	} else {
		ks_log(KS_LOG_DEBUG, "Session (%s) joined chat\n", blade_session_id_get(bs));

		if (props_participant) props_participant->type = cJSON_True;
		else cJSON_AddTrueToObject(props, "blade.chat.participant");

		list_append(&bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and cleanup when removed

		blade_rpc_response_create(breq->pool, &res, NULL, breq->message_id);

		// @todo create an event to send to participants when a session joins and leaves, send after main response though
	}

	blade_session_properties_write_unlock(bs);

	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	return KS_FALSE;
}

ks_bool_t blade_chat_leave_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *res = NULL;
	cJSON *props = NULL;
	cJSON *props_participant = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	blade_session_properties_write_lock(bs, KS_TRUE);

	props = blade_session_properties_get(bs);
	ks_assert(props);

	props_participant = cJSON_GetObjectItem(props, "blade.chat.participant");
	if (!props_participant || props_participant->type == cJSON_False) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to leave chat but is not a participant\n", blade_session_id_get(bs));
		blade_rpc_error_create(breq->pool, &res, NULL, breq->message_id, -10000, "Not a participant of chat");
	} else {
		ks_log(KS_LOG_DEBUG, "Session (%s) left chat\n", blade_session_id_get(bs));

		cJSON_DeleteItemFromObject(props, "blade.chat.participant");

		list_delete(&bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and search manually, also free the id

		blade_rpc_response_create(breq->pool, &res, NULL, breq->message_id);

		// @todo create an event to send to participants when a session joins and leaves, send after main response though
	}

	blade_session_properties_write_unlock(bs);

	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	return KS_FALSE;
}

ks_bool_t blade_chat_send_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *params = NULL;
	cJSON *res = NULL;
	cJSON *event = NULL;
	const char *message = NULL;
	ks_bool_t sendevent = KS_FALSE;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	params = cJSON_GetObjectItem(breq->message, "params"); // @todo cache this in blade_request_t for quicker/easier access
	if (!params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to send chat message with no 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_create(breq->pool, &res, NULL, breq->message_id, -32602, "Missing params object");
	} else if (!(message = cJSON_GetObjectCstr(params, "message"))) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to send chat message with no 'message'\n", blade_session_id_get(bs));
		blade_rpc_error_create(breq->pool, &res, NULL, breq->message_id, -32602, "Missing params message string");
	}

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	if (!res) {
		blade_rpc_response_create(breq->pool, &res, NULL, breq->message_id);
		sendevent = KS_TRUE;
	}
	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	if (sendevent) {
		blade_rpc_event_create(breq->pool, &event, &res, "blade.chat.message");
		ks_assert(event);
		cJSON_AddStringToObject(res, "from", breq->session_id); // @todo should really be the identity, but we don't have that in place yet
		cJSON_AddStringToObject(res, "message", message);

		blade_handle_sessions_send(breq->handle, &bm_chat->participants, NULL, event);

		cJSON_Delete(event);
	}

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
