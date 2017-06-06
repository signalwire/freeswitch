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

#ifndef _BLADE_STACK_H_
#define _BLADE_STACK_H_
#include <blade.h>

#define BLADE_HANDLE_TPOOL_MIN 2
#define BLADE_HANDLE_TPOOL_MAX 4096
#define BLADE_HANDLE_TPOOL_STACK (1024 * 256)
#define BLADE_HANDLE_TPOOL_IDLE 10

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_handle_destroy(blade_handle_t **bhP);
KS_DECLARE(ks_status_t) blade_handle_create(blade_handle_t **bhP);
KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config);
KS_DECLARE(ks_status_t) blade_handle_shutdown(blade_handle_t *bh);
KS_DECLARE(ks_pool_t *) blade_handle_pool_get(blade_handle_t *bh);
KS_DECLARE(ks_thread_pool_t *) blade_handle_tpool_get(blade_handle_t *bh);

KS_DECLARE(ks_status_t) blade_handle_local_nodeid_set(blade_handle_t *bh, const char *nodeid);
KS_DECLARE(ks_bool_t) blade_handle_local_nodeid_compare(blade_handle_t *bh, const char *nodeid);

KS_DECLARE(const char *) blade_handle_master_nodeid_copy(blade_handle_t *bh, ks_pool_t *pool);
KS_DECLARE(ks_status_t) blade_handle_master_nodeid_set(blade_handle_t *bh, const char *nodeid);
KS_DECLARE(ks_bool_t) blade_handle_master_nodeid_compare(blade_handle_t *bh, const char *nodeid);

KS_DECLARE(ks_status_t) blade_handle_realm_register(blade_handle_t *bh, const char *realm);
KS_DECLARE(ks_status_t) blade_handle_realm_unregister(blade_handle_t *bh, const char *realm);
KS_DECLARE(ks_hash_t *) blade_handle_realms_get(blade_handle_t *bh);

KS_DECLARE(ks_status_t) blade_handle_route_add(blade_handle_t *bh, const char *nodeid, const char *sessionid);
KS_DECLARE(ks_status_t) blade_handle_route_remove(blade_handle_t *bh, const char *nodeid);
KS_DECLARE(blade_session_t *) blade_handle_route_lookup(blade_handle_t *bh, const char *nodeid);

KS_DECLARE(ks_status_t) blade_handle_transport_register(blade_transport_t *bt);
KS_DECLARE(ks_status_t) blade_handle_transport_unregister(blade_transport_t *bt);

KS_DECLARE(ks_status_t) blade_handle_corerpc_register(blade_rpc_t *brpc);
KS_DECLARE(ks_status_t) blade_handle_corerpc_unregister(blade_rpc_t *brpc);
KS_DECLARE(blade_rpc_t *) blade_handle_corerpc_lookup(blade_handle_t *bh, const char *method);

KS_DECLARE(ks_status_t) blade_handle_requests_add(blade_rpc_request_t *brpcreq);
KS_DECLARE(ks_status_t) blade_handle_requests_remove(blade_rpc_request_t *brpcreq);
KS_DECLARE(blade_rpc_request_t *) blade_handle_requests_lookup(blade_handle_t *bh, const char *id);

KS_DECLARE(ks_status_t) blade_handle_protocolrpc_register(blade_rpc_t *brpc);
KS_DECLARE(ks_status_t) blade_handle_protocolrpc_unregister(blade_rpc_t *brpc);
KS_DECLARE(blade_rpc_t *) blade_handle_protocolrpc_lookup(blade_handle_t *bh, const char *method, const char *protocol, const char *realm);


KS_DECLARE(ks_status_t) blade_handle_connect(blade_handle_t *bh, blade_connection_t **bcP, blade_identity_t *target, const char *session_id);

KS_DECLARE(ks_status_t) blade_handle_connections_add(blade_connection_t *bc);
KS_DECLARE(ks_status_t) blade_handle_connections_remove(blade_connection_t *bc);
KS_DECLARE(blade_connection_t *) blade_handle_connections_lookup(blade_handle_t *bh, const char *id);

KS_DECLARE(ks_status_t) blade_handle_sessions_add(blade_session_t *bs);
KS_DECLARE(ks_status_t) blade_handle_sessions_remove(blade_session_t *bs);
KS_DECLARE(blade_session_t *) blade_handle_sessions_lookup(blade_handle_t *bh, const char *id);
KS_DECLARE(blade_session_t *) blade_handle_sessions_upstream(blade_handle_t *bh);
KS_DECLARE(void) blade_handle_sessions_send(blade_handle_t *bh, ks_list_t *sessions, const char *exclude, cJSON *json);

KS_DECLARE(ks_status_t) blade_handle_session_state_callback_register(blade_handle_t *bh, void *data, blade_session_state_callback_t callback, const char **id);
KS_DECLARE(ks_status_t) blade_handle_session_state_callback_unregister(blade_handle_t *bh, const char *id);
KS_DECLARE(void) blade_handle_session_state_callbacks_execute(blade_session_t *bs, blade_session_state_condition_t condition);

KS_DECLARE(ks_status_t) blade_protocol_register(blade_handle_t *bh, const char *nodeid, ks_bool_t remove, blade_rpc_response_callback_t callback, void *data);

KS_DECLARE(ks_status_t) blade_protocol_publish(blade_handle_t *bh, const char *name, const char *realm, blade_rpc_response_callback_t callback, void *data);

KS_DECLARE(ks_status_t) blade_protocol_locate(blade_handle_t *bh, const char *name, const char *realm, blade_rpc_response_callback_t callback, void *data);

KS_DECLARE(ks_status_t) blade_protocol_execute(blade_handle_t *bh, const char *nodeid, const char *method, const char *protocol, const char *realm, cJSON *params, blade_rpc_response_callback_t callback, void *data);
KS_DECLARE(cJSON *) blade_protocol_execute_request_params_get(blade_rpc_request_t *brpcreq);
KS_DECLARE(cJSON *) blade_protocol_execute_response_result_get(blade_rpc_response_t *brpcres);
KS_DECLARE(void) blade_protocol_execute_response_send(blade_rpc_request_t *brpcreq, cJSON *result);

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
