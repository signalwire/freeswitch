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

#ifndef _BLADE_CONNECTION_H_
#define _BLADE_CONNECTION_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_connection_create(blade_connection_t **bcP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_connection_destroy(blade_connection_t **bcP);
KS_DECLARE(ks_status_t) blade_connection_startup(blade_connection_t *bc, blade_connection_direction_t direction);
KS_DECLARE(ks_status_t) blade_connection_shutdown(blade_connection_t *bc);
KS_DECLARE(blade_handle_t *) blade_connection_handle_get(blade_connection_t *bc);
KS_DECLARE(ks_pool_t *) blade_connection_pool_get(blade_connection_t *bc);
KS_DECLARE(const char *) blade_connection_id_get(blade_connection_t *bc);
KS_DECLARE(ks_status_t) blade_connection_read_lock(blade_connection_t *bc, ks_bool_t block);
KS_DECLARE(ks_status_t) blade_connection_read_unlock(blade_connection_t *bc);
KS_DECLARE(ks_status_t) blade_connection_write_lock(blade_connection_t *bc, ks_bool_t block);
KS_DECLARE(ks_status_t) blade_connection_write_unlock(blade_connection_t *bc);
KS_DECLARE(void *) blade_connection_transport_get(blade_connection_t *bc);
KS_DECLARE(void) blade_connection_transport_set(blade_connection_t *bc, void *transport_data, blade_transport_callbacks_t *transport_callbacks);
KS_DECLARE(void) blade_connection_state_set(blade_connection_t *bc, blade_connection_state_t state);
KS_DECLARE(blade_connection_state_t) blade_connection_state_get(blade_connection_t *bc);
KS_DECLARE(void) blade_connection_disconnect(blade_connection_t *bc);
KS_DECLARE(blade_connection_rank_t) blade_connection_rank(blade_connection_t *bc, blade_identity_t *target);
KS_DECLARE(ks_status_t) blade_connection_sending_push(blade_connection_t *bc, cJSON *json);
KS_DECLARE(ks_status_t) blade_connection_sending_pop(blade_connection_t *bc, cJSON **json);
KS_DECLARE(const char *) blade_connection_session_get(blade_connection_t *bc);
KS_DECLARE(void) blade_connection_session_set(blade_connection_t *bc, const char *id);
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
