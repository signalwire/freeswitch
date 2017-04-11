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


struct blade_datastore_s {
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	
	const char *config_database_path;
	
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

	ks_assert(bdsP);

	bds = *bdsP;
	*bdsP = NULL;

	ks_assert(bds);

	blade_datastore_shutdown(bds);

	ks_pool_free(bds->pool, &bds);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_datastore_create(blade_datastore_t **bdsP, ks_pool_t *pool, ks_thread_pool_t *tpool)
{
	blade_datastore_t *bds = NULL;

	ks_assert(bdsP);
	ks_assert(pool);
	//ks_assert(tpool);
	
	bds = ks_pool_alloc(pool, sizeof(*bds));
	bds->pool = pool;
	bds->tpool = tpool;
	*bdsP = bds;

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_datastore_config(blade_datastore_t *bds, config_setting_t *config)
{
	config_setting_t *tmp;
	config_setting_t *database = NULL;
	const char *config_database_path = NULL;

	ks_assert(bds);

	if (!config) return KS_STATUS_FAIL;
	if (!config_setting_is_group(config)) return KS_STATUS_FAIL;

	database = config_setting_get_member(config, "database");
	if (!database) return KS_STATUS_FAIL;
	tmp = config_lookup_from(database, "path");
	if (!tmp) return KS_STATUS_FAIL;
	if (config_setting_type(tmp) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
	config_database_path = config_setting_get_string(tmp);

	if (bds->config_database_path) ks_pool_free(bds->pool, &bds->config_database_path);
	bds->config_database_path = ks_pstrdup(bds->pool, config_database_path);
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_datastore_startup(blade_datastore_t *bds, config_setting_t *config)
{
	ks_assert(bds);

	// @todo check if already started
	
	if (blade_datastore_config(bds, config) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	if (unqlite_open(&bds->db, bds->config_database_path, UNQLITE_OPEN_CREATE) != UNQLITE_OK) {
		const char *errbuf = NULL;
		blade_datastore_error(bds, &errbuf, NULL);
		ks_log(KS_LOG_ERROR, "BDS Open Error: %s\n", errbuf);
		return KS_STATUS_FAIL;
	}

	// @todo unqlite_lib_config(UNQLITE_LIB_CONFIG_MEM_ERR_CALLBACK)

	// @todo VM init if document store is used (and output consumer callback)
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_datastore_shutdown(blade_datastore_t *bds)
{
	ks_assert(bds);

	if (bds->db) {
		unqlite_close(bds->db);
		bds->db = NULL;
	}

	if (bds->config_database_path) ks_pool_free(bds->pool, &bds->config_database_path);
	
	return KS_STATUS_SUCCESS;
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
			ks_log(KS_LOG_ERROR, "BDS Store Error: %s\n", errbuf);
			
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
		else if(rc == UNQLITE_NOTFOUND) ret = KS_STATUS_NOT_FOUND;
		else {
			const char *errbuf;
			blade_datastore_error(bds, &errbuf, NULL);
			ks_log(KS_LOG_ERROR, "BDS Fetch Error: %s\n", errbuf);
			
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
