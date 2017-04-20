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

#ifndef _BLADE_PROTOCOL_H_
#define _BLADE_PROTOCOL_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_request_create(blade_request_t **breqP,
											 blade_handle_t *bh,
											 ks_pool_t *pool,
											 const char *session_id,
											 cJSON *json,
											 blade_response_callback_t callback);
KS_DECLARE(ks_status_t) blade_request_destroy(blade_request_t **breqP);
KS_DECLARE(ks_status_t) blade_response_create(blade_response_t **bresP, blade_handle_t *bh, ks_pool_t *pool, const char *session_id, blade_request_t *breq, cJSON *json);
KS_DECLARE(ks_status_t) blade_response_destroy(blade_response_t **bresP);
KS_DECLARE(ks_status_t) blade_event_create(blade_event_t **bevP, blade_handle_t *bh, ks_pool_t *pool, const char *session_id, cJSON *json);
KS_DECLARE(ks_status_t) blade_event_destroy(blade_event_t **bevP);
KS_DECLARE(ks_status_t) blade_rpc_request_create(ks_pool_t *pool, cJSON **json, cJSON **params, const char **id, const char *method);
KS_DECLARE(ks_status_t) blade_rpc_response_create(cJSON **json, cJSON **result, const char *id);
KS_DECLARE(ks_status_t) blade_rpc_error_create(cJSON **json, cJSON **error, const char *id, int32_t code, const char *message);
KS_DECLARE(ks_status_t) blade_rpc_event_create(cJSON **json, cJSON **result, const char *event);
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
