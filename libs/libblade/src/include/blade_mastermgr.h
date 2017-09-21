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

#ifndef _BLADE_MASTERMGR_H_
#define _BLADE_MASTERMGR_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_mastermgr_create(blade_mastermgr_t **bmmgrP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_mastermgr_destroy(blade_mastermgr_t **bmmgrP);
KS_DECLARE(ks_status_t) blade_mastermgr_startup(blade_mastermgr_t *bmmgr, config_setting_t *config);
KS_DECLARE(ks_status_t) blade_mastermgr_shutdown(blade_mastermgr_t *bmmgr);
KS_DECLARE(blade_handle_t *) blade_mastermgr_handle_get(blade_mastermgr_t *bmmgr);
KS_DECLARE(ks_status_t) blade_mastermgr_purge(blade_mastermgr_t *bmmgr, const char *nodeid);
KS_DECLARE(blade_protocol_t *) blade_mastermgr_protocol_lookup(blade_mastermgr_t *bmmgr, const char *protocol, ks_bool_t writelocked);
KS_DECLARE(ks_status_t) blade_mastermgr_protocol_controller_add(blade_mastermgr_t *bmmgr, const char *protocol, const char *controller);
KS_DECLARE(ks_status_t) blade_mastermgr_protocol_controller_remove(blade_mastermgr_t *bmmgr, const char *protocol, const char *controller);
KS_DECLARE(ks_status_t) blade_mastermgr_protocol_channel_add(blade_mastermgr_t *bmmgr, const char *protocol, const char *channel, blade_channel_flags_t flags);
KS_DECLARE(ks_status_t) blade_mastermgr_protocol_channel_remove(blade_mastermgr_t *bmmgr, const char *protocol, const char *channel);
KS_DECLARE(ks_status_t) blade_mastermgr_protocol_channel_authorize(blade_mastermgr_t *bmmgr, ks_bool_t remove, const char *protocol, const char *channel, const char *controller, const char *target);
KS_DECLARE(ks_bool_t) blade_mastermgr_protocol_channel_authorization_verify(blade_mastermgr_t *bmmgr, const char *protocol, const char *channel, const char *target);
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
