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

#ifndef __KS_SB_H__
#define __KS_SB_H__

#include "ks.h"

KS_BEGIN_EXTERN_C

typedef struct ks_sb_s ks_sb_t;

KS_DECLARE(ks_status_t) ks_sb_create(ks_sb_t **sbP, ks_pool_t *pool, ks_size_t preallocated);
KS_DECLARE(ks_status_t) ks_sb_destroy(ks_sb_t **sbP);
KS_DECLARE(const char *) ks_sb_cstr(ks_sb_t *sb);
KS_DECLARE(ks_size_t) ks_sb_length(ks_sb_t *sb);
KS_DECLARE(ks_status_t) ks_sb_accommodate(ks_sb_t *sb, ks_size_t len);
KS_DECLARE(ks_status_t) ks_sb_append(ks_sb_t *sb, const char *str);
KS_DECLARE(ks_status_t) ks_sb_append_ex(ks_sb_t *sb, const char *str, ks_size_t len);
KS_DECLARE(ks_status_t) ks_sb_printf(ks_sb_t *sb, const char *fmt, ...);
KS_DECLARE(ks_status_t) ks_sb_json(ks_sb_t *sb, const cJSON *json);

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
