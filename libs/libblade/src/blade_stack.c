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
	BH_NONE = 0,
	BH_MYPOOL = (1 << 0),
	BH_MYTPOOL = (1 << 1)
} bhpvt_flag_t;

struct blade_handle_s {
	bhpvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	
	config_setting_t *config_datastore;
	config_setting_t *config_directory;
	
	//blade_peer_t *peer;
	blade_directory_t *directory;
	blade_datastore_t *datastore;
};


KS_DECLARE(ks_status_t) blade_handle_destroy(blade_handle_t **bhP)
{
	blade_handle_t *bh = NULL;
	bhpvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bhP);

	bh = *bhP;
	*bhP = NULL;

	ks_assert(bh);

	flags = bh->flags;
	pool = bh->pool;

	blade_handle_shutdown(bh);
	
	//blade_peer_destroy(&bh->peer);
    if (bh->tpool && (flags & BH_MYTPOOL)) ks_thread_pool_destroy(&bh->tpool);

	ks_pool_free(bh->pool, &bh);

	if (pool && (flags & BH_MYPOOL)) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_create(blade_handle_t **bhP, ks_pool_t *pool, ks_thread_pool_t *tpool)
{
	bhpvt_flag_t newflags = BH_NONE;
	blade_handle_t *bh = NULL;

	ks_assert(bhP);

	if (!pool) {
		newflags |= BH_MYPOOL;
		ks_pool_open(&pool);
	}
    if (!tpool) {
		newflags |= BH_MYTPOOL;
		ks_thread_pool_create(&tpool, BLADE_HANDLE_TPOOL_MIN, BLADE_HANDLE_TPOOL_MAX, BLADE_HANDLE_TPOOL_STACK, KS_PRI_NORMAL, BLADE_HANDLE_TPOOL_IDLE);
		ks_assert(tpool);
	}
	
	bh = ks_pool_alloc(pool, sizeof(*bh));
	bh->flags = newflags;
	bh->pool = pool;
	bh->tpool = tpool;
	//blade_peer_create(&bh->peer, bh->pool, bh->tpool);

	*bhP = bh;

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_handle_config(blade_handle_t *bh, config_setting_t *config)
{
	config_setting_t *datastore = NULL;
	config_setting_t *directory = NULL;
	
	ks_assert(bh);

	if (!config) return KS_STATUS_FAIL;
    if (!config_setting_is_group(config)) return KS_STATUS_FAIL;

    datastore = config_setting_get_member(config, "datastore");
    //if (datastore && !config_setting_is_group(datastore)) return KS_STATUS_FAIL;
	
    directory = config_setting_get_member(config, "directory");
    //if (directory && !config_setting_is_group(directory)) return KS_STATUS_FAIL;

	bh->config_datastore = datastore;
	bh->config_directory = directory;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config)
{
	ks_assert(bh);

    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	if (bh->config_datastore && !blade_handle_datastore_available(bh)) {
		blade_datastore_create(&bh->datastore, bh->pool, bh->tpool);
		blade_datastore_startup(bh->datastore, bh->config_datastore);
	}
	
	if (bh->config_directory && !blade_handle_directory_available(bh)) {
		blade_directory_create(&bh->directory, bh->pool, bh->tpool);
		blade_directory_startup(bh->directory, config);
	}
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_shutdown(blade_handle_t *bh)
{
	ks_assert(bh);
	
	if (blade_handle_directory_available(bh)) blade_directory_destroy(&bh->directory);
	
	if (blade_handle_datastore_available(bh)) blade_datastore_destroy(&bh->datastore);
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_handle_datastore_available(blade_handle_t *bh)
{
	ks_assert(bh);

	return bh->datastore != NULL;
}

KS_DECLARE(ks_bool_t) blade_handle_directory_available(blade_handle_t *bh)
{
	ks_assert(bh);

	return bh->directory != NULL;
}

KS_DECLARE(ks_status_t) blade_handle_datastore_store(blade_handle_t *bh, const void *key, int32_t key_length, const void *data, int64_t data_length)
{
	ks_assert(bh);
	ks_assert(key);
	ks_assert(key_length > 0);
	ks_assert(data);
	ks_assert(data_length > 0);

	if (!blade_handle_datastore_available(bh)) return KS_STATUS_INACTIVE;
	
	return blade_datastore_store(bh->datastore, key, key_length, data, data_length);
}

KS_DECLARE(ks_status_t) blade_handle_datastore_fetch(blade_handle_t *bh,
													 blade_datastore_fetch_callback_t callback,
													 const void *key,
													 int32_t key_length,
													 void *userdata)
{
	ks_assert(bh);
	ks_assert(callback);
	ks_assert(key);
	ks_assert(key_length > 0);
	
	if (!blade_handle_datastore_available(bh)) return KS_STATUS_INACTIVE;
	
	return blade_datastore_fetch(bh->datastore, callback, key, key_length, userdata);
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
