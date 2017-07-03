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

#ifndef _BLADE_UPSTREAMMGR_H_
#define _BLADE_UPSTREAMMGR_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_upstreammgr_create(blade_upstreammgr_t **bumgrP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_upstreammgr_destroy(blade_upstreammgr_t **bumgrP);
KS_DECLARE(blade_handle_t *) blade_upstreammgr_handle_get(blade_upstreammgr_t *bumgr);
KS_DECLARE(ks_status_t) blade_upstreammgr_localid_set(blade_upstreammgr_t *bumgr, const char *id);
KS_DECLARE(ks_bool_t) blade_upstreammgr_localid_compare(blade_upstreammgr_t *bumgr, const char *id);
KS_DECLARE(ks_status_t) blade_upstreammgr_localid_copy(blade_upstreammgr_t *bumgr, ks_pool_t *pool, const char **id);
KS_DECLARE(ks_bool_t) blade_upstreammgr_session_established(blade_upstreammgr_t *bumgr);
KS_DECLARE(blade_session_t *) blade_upstreammgr_session_get(blade_upstreammgr_t *bumgr);
KS_DECLARE(ks_status_t) blade_upstreammgr_masterid_set(blade_upstreammgr_t *bumgr, const char *id);
KS_DECLARE(ks_bool_t) blade_upstreammgr_masterid_compare(blade_upstreammgr_t *bumgr, const char *id);
KS_DECLARE(ks_status_t) blade_upstreammgr_masterid_copy(blade_upstreammgr_t *bumgr, ks_pool_t *pool, const char **id);
//KS_DECLARE(ks_hash_t *) blade_upstreammgr_realm_lookup(blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_upstreammgr_realm_add(blade_upstreammgr_t *bumgr, const char *realm);
KS_DECLARE(ks_status_t) blade_upstreammgr_realm_remove(blade_upstreammgr_t *bumgr, const char *realm);
KS_DECLARE(ks_status_t) blade_upstreammgr_realm_clear(blade_upstreammgr_t *bumgr);
KS_DECLARE(ks_status_t) blade_upstreammgr_realm_propagate(blade_upstreammgr_t *bumgr, blade_session_t *bs);

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
