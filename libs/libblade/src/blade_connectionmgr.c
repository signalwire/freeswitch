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

struct blade_connectionmgr_s {
	blade_handle_t *handle;

	ks_hash_t *connections; // id, blade_connection_t*
};


static void blade_connectionmgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_connectionmgr_t *bcmgr = (blade_connectionmgr_t *)ptr;

	//ks_assert(bcmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_connectionmgr_create(blade_connectionmgr_t **bcmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_connectionmgr_t *bcmgr = NULL;

	ks_assert(bcmgrP);
	
	ks_pool_open(&pool);
	ks_assert(pool);

	bcmgr = ks_pool_alloc(pool, sizeof(blade_connectionmgr_t));
	bcmgr->handle = bh;

	ks_hash_create(&bcmgr->connections, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bcmgr->connections);

	ks_pool_set_cleanup(bcmgr, NULL, blade_connectionmgr_cleanup);

	*bcmgrP = bcmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connectionmgr_destroy(blade_connectionmgr_t **bcmgrP)
{
	blade_connectionmgr_t *bcmgr = NULL;
	ks_pool_t *pool;

	ks_assert(bcmgrP);
	ks_assert(*bcmgrP);

	bcmgr = *bcmgrP;
	*bcmgrP = NULL;

	pool = ks_pool_get(bcmgr);
	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_connectionmgr_handle_get(blade_connectionmgr_t *bcmgr)
{
	ks_assert(bcmgr);
	return bcmgr->handle;
}

KS_DECLARE(ks_status_t) blade_connectionmgr_shutdown(blade_connectionmgr_t *bcmgr)
{
	ks_hash_iterator_t *it = NULL;

	ks_assert(bcmgr);

	ks_hash_read_lock(bcmgr->connections);
	for (it = ks_hash_first(bcmgr->connections, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_connection_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		blade_connection_disconnect(value);
	}
	ks_hash_read_unlock(bcmgr->connections);
	while (ks_hash_count(bcmgr->connections) > 0) ks_sleep_ms(100);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_connection_t *) blade_connectionmgr_connection_lookup(blade_connectionmgr_t *bcmgr, const char *id)
{
	blade_connection_t *bc = NULL;

	ks_assert(bcmgr);
	ks_assert(id);

	bc = ks_hash_search(bcmgr->connections, (void *)id, KS_READLOCKED);
	if (bc) blade_connection_read_lock(bc, KS_TRUE);
	ks_hash_read_unlock(bcmgr->connections);

	return bc;
}

KS_DECLARE(ks_status_t) blade_connectionmgr_connection_add(blade_connectionmgr_t *bcmgr, blade_connection_t *bc)
{
	char *key = NULL;

	ks_assert(bcmgr);
	ks_assert(bc);

	key = ks_pstrdup(ks_pool_get(bcmgr), blade_connection_id_get(bc));
	ks_hash_insert(bcmgr->connections, (void *)key, bc);

	ks_log(KS_LOG_DEBUG, "Connection Added: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_connectionmgr_connection_remove(blade_connectionmgr_t *bcmgr, blade_connection_t *bc)
{
	const char *id = NULL;

	ks_assert(bcmgr);
	ks_assert(bc);

	blade_connection_write_lock(bc, KS_TRUE);

	id = blade_connection_id_get(bc);
	ks_hash_remove(bcmgr->connections, (void *)id);

	blade_connection_write_unlock(bc);

	ks_log(KS_LOG_DEBUG, "Connection Removed: %s\n", id);

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
