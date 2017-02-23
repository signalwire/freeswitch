/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#ifndef _BLADE_TYPES_H_
#define _BLADE_TYPES_H_
#include <ks.h>
#include <libconfig.h>

KS_BEGIN_EXTERN_C

typedef struct blade_handle_s blade_handle_t;
typedef struct blade_identity_s blade_identity_t;
typedef struct blade_module_s blade_module_t;
typedef struct blade_module_callbacks_s blade_module_callbacks_t;
typedef struct blade_transport_callbacks_s blade_transport_callbacks_t;
typedef struct blade_connection_s blade_connection_t;
typedef struct blade_session_s blade_session_t;
typedef struct blade_request_s blade_request_t;
typedef struct blade_response_s blade_response_t;

typedef struct blade_datastore_s blade_datastore_t;

typedef ks_bool_t (*blade_datastore_fetch_callback_t)(blade_datastore_t *bds, const void *data, uint32_t data_length, void *userdata);



typedef enum {
	BLADE_CONNECTION_STATE_NONE,
	BLADE_CONNECTION_STATE_DISCONNECT,
	BLADE_CONNECTION_STATE_NEW,
	BLADE_CONNECTION_STATE_CONNECT,
	BLADE_CONNECTION_STATE_ATTACH,
	BLADE_CONNECTION_STATE_DETACH,
	BLADE_CONNECTION_STATE_READY,
} blade_connection_state_t;

typedef enum {
	BLADE_CONNECTION_DIRECTION_INBOUND,
	BLADE_CONNECTION_DIRECTION_OUTBOUND,
} blade_connection_direction_t;

typedef enum {
	BLADE_CONNECTION_STATE_CONDITION_PRE,
	BLADE_CONNECTION_STATE_CONDITION_POST,
} blade_connection_state_condition_t;

typedef enum {
	BLADE_CONNECTION_STATE_HOOK_SUCCESS,
	BLADE_CONNECTION_STATE_HOOK_DISCONNECT,
	BLADE_CONNECTION_STATE_HOOK_BYPASS,
} blade_connection_state_hook_t;

typedef enum {
	BLADE_CONNECTION_RANK_POOR,
	BLADE_CONNECTION_RANK_AVERAGE,
	BLADE_CONNECTION_RANK_GOOD,
	BLADE_CONNECTION_RANK_GREAT,
} blade_connection_rank_t;


typedef enum {
	BLADE_SESSION_STATE_NONE,
	BLADE_SESSION_STATE_DESTROY,
	BLADE_SESSION_STATE_HANGUP,
	BLADE_SESSION_STATE_CONNECT,
	BLADE_SESSION_STATE_ATTACH,
	BLADE_SESSION_STATE_DETACH,
	BLADE_SESSION_STATE_READY,
} blade_session_state_t;



typedef ks_status_t (*blade_module_load_callback_t)(blade_module_t **bmP, blade_handle_t *bh);
typedef ks_status_t (*blade_module_unload_callback_t)(blade_module_t *bm);
typedef ks_status_t (*blade_module_startup_callback_t)(blade_module_t *bm, config_setting_t *config);
typedef ks_status_t (*blade_module_shutdown_callback_t)(blade_module_t *bm);

struct blade_module_callbacks_s {
	blade_module_load_callback_t onload;
	blade_module_unload_callback_t onunload;
	blade_module_startup_callback_t onstartup;
	blade_module_shutdown_callback_t onshutdown;
};


typedef ks_status_t (*blade_transport_connect_callback_t)(blade_connection_t **bcP, blade_module_t *bm, blade_identity_t *target, const char *session_id);
typedef blade_connection_rank_t (*blade_transport_rank_callback_t)(blade_connection_t *bc, blade_identity_t *target);
typedef ks_status_t (*blade_transport_send_callback_t)(blade_connection_t *bc, cJSON *json);
typedef ks_status_t (*blade_transport_receive_callback_t)(blade_connection_t *bc, cJSON **json);
typedef blade_connection_state_hook_t (*blade_transport_state_callback_t)(blade_connection_t *bc, blade_connection_state_condition_t condition);

struct blade_transport_callbacks_s {
	blade_transport_connect_callback_t onconnect;
	blade_transport_rank_callback_t onrank;
	blade_transport_send_callback_t onsend;
	blade_transport_receive_callback_t onreceive;

	blade_transport_state_callback_t onstate_disconnect_inbound;
	blade_transport_state_callback_t onstate_disconnect_outbound;
	blade_transport_state_callback_t onstate_new_inbound;
	blade_transport_state_callback_t onstate_new_outbound;
	blade_transport_state_callback_t onstate_connect_inbound;
	blade_transport_state_callback_t onstate_connect_outbound;
	blade_transport_state_callback_t onstate_attach_inbound;
	blade_transport_state_callback_t onstate_attach_outbound;
	blade_transport_state_callback_t onstate_detach_inbound;
	blade_transport_state_callback_t onstate_detach_outbound;
	blade_transport_state_callback_t onstate_ready_inbound;
	blade_transport_state_callback_t onstate_ready_outbound;
};


struct blade_request_s {
	ks_pool_t *pool;
	uint32_t refs;
	const char *session_id;

	cJSON *message;
	const char *message_id; // pulled from message for easier keying
	// @todo ttl to wait for response before injecting an error response locally
	// @todo rpc response callback
};

struct blade_response_s {
	ks_pool_t *pool;
	uint32_t refs;
	const char *session_id;
	blade_request_t *request;

	cJSON *message;
};

KS_END_EXTERN_C

#endif

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
