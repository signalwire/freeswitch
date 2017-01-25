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
	BDS_NONE = 0,
	BDS_MYPOOL = (1 << 0),
} bdspvt_flag_t;

struct blade_datastore_s {
	bdspvt_flag_t flags;
	ks_pool_t *pool;
	unqlite *db;
};

struct blade_datastore_fetch_userdata_s
{
	blade_datastore_t *bds;
	blade_datastore_fetch_callback_t callback;
	void *userdata;
};
typedef struct blade_datastore_fetch_userdata_s blade_datastore_fetch_userdata_t;



KS_DECLARE(ks_status_t) blade_datastore_destroy(blade_datastore_t **bdsP)
{
	blade_datastore_t *bds = NULL;
	bdspvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bdsP);

	bds = *bdsP;
	*bdsP = NULL;

	ks_assert(bds);

	flags = bds->flags;
	pool = bds->pool;

	if (bds->db) {
		unqlite_close(bds->db);
		bds->db = NULL;
	}

	ks_pool_free(bds->pool, &bds);

	if (pool && (flags & BDS_MYPOOL)) ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_datastore_create(blade_datastore_t **bdsP, ks_pool_t *pool)
{
	bdspvt_flag_t newflags = BDS_NONE;
	blade_datastore_t *bds = NULL;

	if (!pool) {
		newflags |= BDS_MYPOOL;
		ks_pool_open(&pool);
		ks_assert(pool);
	}

	bds = ks_pool_alloc(pool, sizeof(*bds));
	bds->flags = newflags;
	bds->pool = pool;
	*bdsP = bds;

	if (unqlite_open(&bds->db, NULL, UNQLITE_OPEN_IN_MEMORY) != UNQLITE_OK) {
		const char *errbuf = NULL;
		blade_datastore_error(bds, &errbuf, NULL);
		ks_log(KS_LOG_ERROR, "BDS Error: %s\n", errbuf);
		return KS_STATUS_FAIL;
	}

	// @todo unqlite_lib_config(UNQLITE_LIB_CONFIG_MEM_ERR_CALLBACK)

	// @todo VM init if document store is used (and output consumer callback)
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_datastore_pulse(blade_datastore_t *bds, int32_t timeout)
{
	ks_assert(bds);
	ks_assert(timeout >= 0);
}

KS_DECLARE(void) blade_datastore_error(blade_datastore_t *bds, const char **buffer, int32_t *buffer_length)
{
	ks_assert(bds);
	ks_assert(bds->db);
	ks_assert(buffer);
	
	unqlite_config(bds->db, UNQLITE_CONFIG_ERR_LOG, buffer, buffer_length);
}

KS_DECLARE(ks_status_t) blade_datastore_store(blade_datastore_t *bds, const void *key, int32_t key_length, const void *data, int64_t data_length)
{
	int32_t rc;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bds);
	ks_assert(bds->db);
	ks_assert(key);
	ks_assert(key_length > 0);
	ks_assert(data);
	ks_assert(data_length > 0);

	rc = unqlite_begin(bds->db);

	if (rc != UNQLITE_OK) {
		if (rc == UNQLITE_BUSY) ret = KS_STATUS_TIMEOUT;
		else {
			const char *errbuf;
			blade_datastore_error(bds, &errbuf, NULL);
			ks_log(KS_LOG_ERROR, "BDS Error: %s\n", errbuf);
			
			ret = KS_STATUS_FAIL;
		}
	} else if (unqlite_kv_store(bds->db, key, key_length, data, data_length) == UNQLITE_OK) unqlite_commit(bds->db);
	else unqlite_rollback(bds->db);
	
	return ret;
}

int blade_datastore_fetch_callback(const void *data, unsigned int data_length, void *userdata)
{
	int rc = UNQLITE_OK;
	blade_datastore_fetch_userdata_t *ud = NULL;

	ks_assert(data);
	ks_assert(data_length > 0);
	ks_assert(userdata);

	ud = (blade_datastore_fetch_userdata_t *)userdata;
	if (!ud->callback(ud->bds, data, data_length, ud->userdata)) rc = UNQLITE_ABORT;

	return rc;
}

KS_DECLARE(ks_status_t) blade_datastore_fetch(blade_datastore_t *bds,
											  blade_datastore_fetch_callback_t callback,
											  const void *key,
											  int32_t key_length,
											  void *userdata)
{
	int32_t rc;
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_datastore_fetch_userdata_t ud;

	ks_assert(bds);
	ks_assert(bds->db);
	ks_assert(callback);
	ks_assert(key);
	ks_assert(key_length > 0);

	ud.bds = bds;
	ud.callback = callback;
	ud.userdata = userdata;

	rc = unqlite_kv_fetch_callback(bds->db, key, key_length, blade_datastore_fetch_callback, &ud);
	
	if (rc != UNQLITE_OK) {
		if (rc == UNQLITE_BUSY) ret = KS_STATUS_TIMEOUT;
		else {
			const char *errbuf;
			blade_datastore_error(bds, &errbuf, NULL);
			ks_log(KS_LOG_ERROR, "BDS Error: %s\n", errbuf);
			
			ret = KS_STATUS_FAIL;
		}
	}
	
	return ret;
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
