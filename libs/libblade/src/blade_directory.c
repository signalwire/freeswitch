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

#include "blade.h"


typedef enum {
	BD_NONE = 0,
	BD_MYPOOL = (1 << 0),
	BD_MYTPOOL = (1 << 1),
} bdpvt_flag_t;

struct blade_directory_s {
	bdpvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	
	config_setting_t *config_service;
	
	blade_service_t *service;
};




KS_DECLARE(ks_status_t) blade_directory_destroy(blade_directory_t **bdP)
{
	blade_directory_t *bd = NULL;
	bdpvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bdP);

	bd = *bdP;
	*bdP = NULL;

	ks_assert(bd);

	flags = bd->flags;
	pool = bd->pool;

	blade_directory_shutdown(bd);

    if (bd->tpool && (flags & BD_MYTPOOL)) ks_thread_pool_destroy(&bd->tpool);
	
	ks_pool_free(bd->pool, &bd);

	if (pool && (flags & BD_MYPOOL)) ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_directory_create(blade_directory_t **bdP, ks_pool_t *pool, ks_thread_pool_t *tpool)
{
	bdpvt_flag_t newflags = BD_NONE;
	blade_directory_t *bd = NULL;

	if (!pool) {
		newflags |= BD_MYPOOL;
		ks_pool_open(&pool);
		ks_assert(pool);
	}
	// @todo: move thread pool creation to startup which allows thread pool to be configurable
    if (!tpool) {
		newflags |= BD_MYTPOOL;
		ks_thread_pool_create(&tpool,
							  BLADE_DIRECTORY_TPOOL_MIN,
							  BLADE_DIRECTORY_TPOOL_MAX,
							  BLADE_DIRECTORY_TPOOL_STACK,
							  KS_PRI_NORMAL,
							  BLADE_DIRECTORY_TPOOL_IDLE);
		ks_assert(tpool);
	}
	
	bd = ks_pool_alloc(pool, sizeof(*bd));
	bd->flags = newflags;
	bd->pool = pool;
	bd->tpool = tpool;
	*bdP = bd;

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_directory_config(blade_directory_t *bd, config_setting_t *config)
{
	config_setting_t *service = NULL;

	ks_assert(bd);

	if (!config) return KS_STATUS_FAIL;
	if (!config_setting_is_group(config)) return KS_STATUS_FAIL;
	
	service = config_setting_get_member(config, "service");
	if (!service) return KS_STATUS_FAIL;

	bd->config_service = service;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_directory_startup(blade_directory_t *bd, config_setting_t *config)
{
	ks_assert(bd);

	blade_directory_shutdown(bd);
	
	if (blade_directory_config(bd, config) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	blade_service_create(&bd->service, bd->pool, bd->tpool);
	ks_assert(bd->service);
	if (blade_service_startup(bd->service, bd->config_service) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_directory_shutdown(blade_directory_t *bd)
{
	ks_assert(bd);

	if (bd->service) blade_service_destroy(&bd->service);

	return KS_STATUS_SUCCESS;
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
