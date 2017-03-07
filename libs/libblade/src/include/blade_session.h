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

#ifndef _BLADE_SESSION_H_
#define _BLADE_SESSION_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_session_create(blade_session_t **bsP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_session_destroy(blade_session_t **bsP);
KS_DECLARE(ks_status_t) blade_session_startup(blade_session_t *bs);
KS_DECLARE(ks_status_t) blade_session_shutdown(blade_session_t *bs);
KS_DECLARE(blade_handle_t *) blade_session_handle_get(blade_session_t *bs);
KS_DECLARE(const char *) blade_session_id_get(blade_session_t *bs);
KS_DECLARE(void) blade_session_id_set(blade_session_t *bs, const char *id);
KS_DECLARE(ks_status_t) blade_session_read_lock(blade_session_t *bs, ks_bool_t block);
KS_DECLARE(ks_status_t) blade_session_read_unlock(blade_session_t *bs);
KS_DECLARE(ks_status_t) blade_session_write_lock(blade_session_t *bs, ks_bool_t block);
KS_DECLARE(ks_status_t) blade_session_write_unlock(blade_session_t *bs);
KS_DECLARE(void) blade_session_state_set(blade_session_t *bs, blade_session_state_t state);
KS_DECLARE(void) blade_session_hangup(blade_session_t *bs);
KS_DECLARE(ks_bool_t) blade_session_terminating(blade_session_t *bs);
KS_DECLARE(ks_status_t) blade_session_connections_add(blade_session_t *bs, const char *id);
KS_DECLARE(ks_status_t) blade_session_connections_remove(blade_session_t *bs, const char *id);
KS_DECLARE(ks_status_t) blade_session_send(blade_session_t *bs, cJSON *json, blade_response_callback_t callback);
KS_DECLARE(ks_status_t) blade_session_sending_push(blade_session_t *bs, cJSON *json);
KS_DECLARE(ks_status_t) blade_session_sending_pop(blade_session_t *bs, cJSON **json);
KS_DECLARE(ks_status_t) blade_session_receiving_push(blade_session_t *bs, cJSON *json);
KS_DECLARE(ks_status_t) blade_session_receiving_pop(blade_session_t *bs, cJSON **json);
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
