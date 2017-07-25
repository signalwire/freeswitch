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

#include "blade.h"

struct blade_upstreammgr_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	// local node id, can be used to get the upstream session, provided by upstream "blade.connect" response
	const char *localid;
	ks_rwl_t *localid_rwl;

	// master node id, provided by upstream "blade.connect" response
	const char *masterid;
	ks_rwl_t *masterid_rwl;

	// realms for new nodes, these originate from the master, and are provided by upstream session "blade.connect" response
	ks_hash_t *realms;
};


static void blade_upstreammgr_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_upstreammgr_t *bumgr = (blade_upstreammgr_t *)ptr;

	//ks_assert(brmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_upstreammgr_create(blade_upstreammgr_t **bumgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_upstreammgr_t *bumgr = NULL;

	ks_assert(bumgrP);
	
	ks_pool_open(&pool);
	ks_assert(pool);

	bumgr = ks_pool_alloc(pool, sizeof(blade_upstreammgr_t));
	bumgr->handle = bh;
	bumgr->pool = pool;

	//ks_hash_create(&bumgr->routes, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bumgr->pool);
	//ks_assert(bumgr->routes);
	ks_rwl_create(&bumgr->localid_rwl, bumgr->pool);
	ks_assert(bumgr->localid_rwl);

	ks_rwl_create(&bumgr->masterid_rwl, bumgr->pool);
	ks_assert(bumgr->masterid_rwl);

	ks_hash_create(&bumgr->realms, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bumgr->pool);
	ks_assert(bumgr->realms);

	ks_pool_set_cleanup(pool, bumgr, NULL, blade_upstreammgr_cleanup);

	*bumgrP = bumgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_destroy(blade_upstreammgr_t **bumgrP)
{
	blade_upstreammgr_t *bumgr = NULL;
	ks_pool_t *pool;

	ks_assert(bumgrP);
	ks_assert(*bumgrP);

	bumgr = *bumgrP;
	*bumgrP = NULL;

	ks_assert(bumgr);

	pool = bumgr->pool;

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_upstreammgr_handle_get(blade_upstreammgr_t *bumgr)
{
	ks_assert(bumgr);
	return bumgr->handle;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_localid_set(blade_upstreammgr_t *bumgr, const char *id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bumgr);

	ks_rwl_write_lock(bumgr->localid_rwl);
	if (bumgr->localid && id) {
		ret = KS_STATUS_NOT_ALLOWED;
		goto done;
	}
	if (!bumgr->localid && !id) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	if (bumgr->localid) ks_pool_free(bumgr->pool, &bumgr->localid);
	if (id) bumgr->localid = ks_pstrdup(bumgr->pool, id);

	ks_log(KS_LOG_DEBUG, "LocalID: %s\n", id);

done:
	ks_rwl_write_unlock(bumgr->localid_rwl);
	return ret;
}

KS_DECLARE(ks_bool_t) blade_upstreammgr_localid_compare(blade_upstreammgr_t *bumgr, const char *id)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bumgr);
	ks_assert(id);

	ks_rwl_read_lock(bumgr->localid_rwl);
	ret = ks_safe_strcasecmp(bumgr->localid, id) == 0;
	ks_rwl_read_unlock(bumgr->localid_rwl);

	return ret;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_localid_copy(blade_upstreammgr_t *bumgr, ks_pool_t *pool, const char **id)
{
	ks_assert(bumgr);
	ks_assert(pool);
	ks_assert(id);

	*id = NULL;

	ks_rwl_read_lock(bumgr->localid_rwl);
	if (bumgr->localid) *id = ks_pstrdup(pool, bumgr->localid);
	ks_rwl_read_unlock(bumgr->localid_rwl);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_upstreammgr_session_established(blade_upstreammgr_t *bumgr)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bumgr);

	ks_rwl_read_lock(bumgr->localid_rwl);
	ret = bumgr->localid != NULL;
	ks_rwl_read_unlock(bumgr->localid_rwl);

	return ret;
}

KS_DECLARE(blade_session_t *) blade_upstreammgr_session_get(blade_upstreammgr_t *bumgr)
{
	blade_session_t *bs = NULL;

	ks_assert(bumgr);

	ks_rwl_read_lock(bumgr->localid_rwl);
	if (bumgr->localid) bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bumgr->handle), bumgr->localid);
	ks_rwl_read_unlock(bumgr->localid_rwl);

	return bs;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_masterid_set(blade_upstreammgr_t *bumgr, const char *id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bumgr);

	ks_rwl_write_lock(bumgr->masterid_rwl);
	if (bumgr->masterid) ks_pool_free(bumgr->pool, &bumgr->masterid);
	if (id) bumgr->masterid = ks_pstrdup(bumgr->pool, id);

	ks_log(KS_LOG_DEBUG, "MasterID: %s\n", id);

	ks_rwl_write_unlock(bumgr->masterid_rwl);
	return ret;
}

KS_DECLARE(ks_bool_t) blade_upstreammgr_masterid_compare(blade_upstreammgr_t *bumgr, const char *id)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bumgr);
	ks_assert(id);

	ks_rwl_read_lock(bumgr->masterid_rwl);
	ret = ks_safe_strcasecmp(bumgr->masterid, id) == 0;
	ks_rwl_read_unlock(bumgr->masterid_rwl);

	return ret;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_masterid_copy(blade_upstreammgr_t *bumgr, ks_pool_t *pool, const char **id)
{
	ks_assert(bumgr);
	ks_assert(pool);
	ks_assert(id);

	*id = NULL;

	ks_rwl_read_lock(bumgr->masterid_rwl);
	if (bumgr->masterid) *id = ks_pstrdup(pool, bumgr->masterid);
	ks_rwl_read_unlock(bumgr->masterid_rwl);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_upstreammgr_masterlocal(blade_upstreammgr_t *bumgr)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bumgr);

	ks_rwl_read_lock(bumgr->masterid_rwl);
	ks_rwl_read_lock(bumgr->localid_rwl);
	ret = bumgr->masterid && bumgr->localid && !ks_safe_strcasecmp(bumgr->masterid, bumgr->localid);
	ks_rwl_read_unlock(bumgr->localid_rwl);
	ks_rwl_read_unlock(bumgr->masterid_rwl);

	return ret;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_realm_add(blade_upstreammgr_t *bumgr, const char *realm)
{
	char *key = NULL;

	ks_assert(bumgr);
	ks_assert(realm);

	key = ks_pstrdup(bumgr->pool, realm);
	ks_hash_insert(bumgr->realms, (void *)key, (void *)KS_TRUE);

	ks_log(KS_LOG_DEBUG, "Realm Added: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_realm_remove(blade_upstreammgr_t *bumgr, const char *realm)
{
	ks_assert(bumgr);
	ks_assert(realm);

	ks_hash_remove(bumgr->realms, (void *)realm);

	ks_log(KS_LOG_DEBUG, "Realm Removed: %s\n", realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_realm_clear(blade_upstreammgr_t *bumgr)
{
	ks_hash_iterator_t *it = NULL;

	ks_assert(bumgr);

	while ((it = ks_hash_first(bumgr->realms, KS_UNLOCKED))) {
		void *key = NULL;
		void *value = NULL;
		ks_hash_this(it, (const void **)&key, NULL, &value);
		ks_log(KS_LOG_DEBUG, "Realm Removed: %s\n", key);
		ks_hash_remove(bumgr->realms, key);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_upstreammgr_realm_propagate(blade_upstreammgr_t *bumgr, blade_session_t *bs)
{
	ks_assert(bumgr);
	ks_assert(bs);

	ks_hash_read_lock(bumgr->realms);
	for (ks_hash_iterator_t *it = ks_hash_first(bumgr->realms, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		void *value = NULL;
		ks_hash_this(it, (const void **)&key, NULL, &value);
		blade_session_realm_add(bs, (const char *)key);
	}
	ks_hash_read_unlock(bumgr->realms);

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
