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

KS_DECLARE(blade_transportmgr_t *) blade_handle_transportmgr_get(blade_handle_t *bh);
KS_DECLARE(blade_rpcmgr_t *) blade_handle_rpcmgr_get(blade_handle_t *bh);
KS_DECLARE(blade_routemgr_t *) blade_handle_routemgr_get(blade_handle_t *bh);
KS_DECLARE(blade_subscriptionmgr_t *) blade_handle_subscriptionmgr_get(blade_handle_t *bh);
KS_DECLARE(blade_upstreammgr_t *) blade_handle_upstreammgr_get(blade_handle_t *bh);
KS_DECLARE(blade_mastermgr_t *) blade_handle_mastermgr_get(blade_handle_t *bh);
KS_DECLARE(blade_connectionmgr_t *) blade_handle_connectionmgr_get(blade_handle_t *bh);
KS_DECLARE(blade_sessionmgr_t *) blade_handle_sessionmgr_get(blade_handle_t *bh);

KS_DECLARE(ks_status_t) blade_handle_connect(blade_handle_t *bh, blade_connection_t **bcP, blade_identity_t *target, const char *session_id);

KS_DECLARE(ks_status_t) blade_handle_rpcregister(blade_handle_t *bh, const char *nodeid, ks_bool_t remove, blade_rpc_response_callback_t callback, cJSON *data);

KS_DECLARE(ks_status_t) blade_handle_rpcpublish(blade_handle_t *bh, const char *protocol, const char *realm, cJSON *channels, blade_rpc_response_callback_t callback, cJSON *data);

KS_DECLARE(ks_status_t) blade_handle_rpcauthorize(blade_handle_t *bh, const char *nodeid, ks_bool_t remove, const char *protocol, const char *realm, cJSON *channels, blade_rpc_response_callback_t callback, cJSON *data);

KS_DECLARE(ks_status_t) blade_handle_rpclocate(blade_handle_t *bh, const char *protocol, const char *realm, blade_rpc_response_callback_t callback, cJSON *data);

KS_DECLARE(ks_status_t) blade_handle_rpcexecute(blade_handle_t *bh, const char *nodeid, const char *method, const char *protocol, const char *realm, cJSON *params, blade_rpc_response_callback_t callback, cJSON *data);
KS_DECLARE(const char *) blade_rpcexecute_request_requester_nodeid_get(blade_rpc_request_t *brpcreq);
KS_DECLARE(const char *) blade_rpcexecute_request_responder_nodeid_get(blade_rpc_request_t *brpcreq);
KS_DECLARE(cJSON *) blade_rpcexecute_request_params_get(blade_rpc_request_t *brpcreq);
KS_DECLARE(cJSON *) blade_rpcexecute_response_result_get(blade_rpc_response_t *brpcres);
KS_DECLARE(void) blade_rpcexecute_response_send(blade_rpc_request_t *brpcreq, cJSON *result);

KS_DECLARE(ks_status_t) blade_handle_rpcsubscribe(blade_handle_t *bh, const char *protocol, const char *realm, cJSON *subscribe_channels, cJSON *unsubscribe_channels, blade_rpc_response_callback_t callback, cJSON *data, blade_rpc_request_callback_t channel_callback, cJSON *channel_data);
KS_DECLARE(ks_status_t) blade_handle_rpcsubscribe_raw(blade_handle_t *bh, const char *protocol, const char *realm, cJSON *subscribe_channels, cJSON *unsubscribe_channels, const char *subscriber, ks_bool_t downstream, blade_rpc_response_callback_t callback, cJSON *data);

KS_DECLARE(ks_status_t) blade_handle_rpcbroadcast(blade_handle_t *bh, const char *protocol, const char *realm, const char *channel, const char *event, cJSON *params, blade_rpc_response_callback_t callback, cJSON *data);
KS_DECLARE(cJSON *) blade_rpcbroadcast_request_params_get(blade_rpc_request_t *brpcreq);

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
