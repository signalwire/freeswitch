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

struct blade_transportmgr_s {
	blade_handle_t *handle;

	ks_hash_t *transports; // name, blade_transport_t*
	blade_transport_t *default_transport; // default wss transport
};


static void blade_transportmgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_transportmgr_t *btmgr = (blade_transportmgr_t *)ptr;
	ks_hash_iterator_t *it = NULL;

	ks_assert(btmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		while ((it = ks_hash_first(btmgr->transports, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_transport_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(btmgr->transports, key);

			blade_transport_destroy(&value); // must call destroy to close the transport pool, using FREE_VALUE on the hash would attempt to free the transport from the wrong pool
		}
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_transportmgr_create(blade_transportmgr_t **btmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_transportmgr_t *btmgr = NULL;

	ks_assert(btmgrP);

	ks_pool_open(&pool);
	ks_assert(pool);

	btmgr = ks_pool_alloc(pool, sizeof(blade_transportmgr_t));
	btmgr->handle = bh;

	ks_hash_create(&btmgr->transports, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(btmgr->transports);

	ks_pool_set_cleanup(btmgr, NULL, blade_transportmgr_cleanup);

	*btmgrP = btmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_transportmgr_destroy(blade_transportmgr_t **btmgrP)
{
	blade_transportmgr_t *btmgr = NULL;
	ks_pool_t *pool;

	ks_assert(btmgrP);
	ks_assert(*btmgrP);

	btmgr = *btmgrP;
	*btmgrP = NULL;

	pool = ks_pool_get(btmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_transportmgr_startup(blade_transportmgr_t *btmgr, config_setting_t *config)
{
	ks_assert(btmgr);

	for (ks_hash_iterator_t *it = ks_hash_first(btmgr->transports, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_transport_t *value = NULL;
		blade_transport_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_transport_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onstartup) callbacks->onstartup(value, config);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_transportmgr_shutdown(blade_transportmgr_t *btmgr)
{
	ks_assert(btmgr);

	ks_hash_read_lock(btmgr->transports);
	for (ks_hash_iterator_t *it = ks_hash_first(btmgr->transports, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_transport_t *value = NULL;
		blade_transport_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_transport_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onshutdown) callbacks->onshutdown(value);
	}
	ks_hash_read_unlock(btmgr->transports);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_transportmgr_handle_get(blade_transportmgr_t *btmgr)
{
	ks_assert(btmgr);

	return btmgr->handle;
}

KS_DECLARE(blade_transport_t *) blade_transportmgr_default_get(blade_transportmgr_t *btmgr)
{
	ks_assert(btmgr);

	return btmgr->default_transport;
}

KS_DECLARE(ks_status_t) blade_transportmgr_default_set(blade_transportmgr_t *btmgr, blade_transport_t *bt)
{
	ks_assert(btmgr);
	ks_assert(bt);

	btmgr->default_transport = bt;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_transport_t *) blade_transportmgr_transport_lookup(blade_transportmgr_t *btmgr, const char *name, ks_bool_t ordefault)
{
	blade_transport_t *bt = NULL;

	ks_assert(btmgr);

	ks_hash_read_lock(btmgr->transports);
	if (name && name[0]) bt = (blade_transport_t *)ks_hash_search(btmgr->transports, (void *)name, KS_UNLOCKED);
	if (!bt && ordefault) bt = btmgr->default_transport;
	// @todo if (bt) blade_transport_read_lock(bt);
	ks_hash_read_unlock(btmgr->transports);

	return bt;

}

KS_DECLARE(ks_status_t) blade_transportmgr_transport_add(blade_transportmgr_t *btmgr, blade_transport_t *bt)
{
	char *key = NULL;

	ks_assert(btmgr);
	ks_assert(bt);

	key = ks_pstrdup(ks_pool_get(btmgr), blade_transport_name_get(bt));
	ks_hash_insert(btmgr->transports, (void *)key, (void *)bt);

	ks_log(KS_LOG_DEBUG, "Transport Added: %s\n", key);

	return KS_STATUS_SUCCESS;

}

KS_DECLARE(ks_status_t) blade_transportmgr_transport_remove(blade_transportmgr_t *btmgr, blade_transport_t *bt)
{
	const char *name = NULL;

	ks_assert(btmgr);
	ks_assert(bt);

	name = blade_transport_name_get(bt);
	ks_hash_remove(btmgr->transports, (void *)name);

	ks_log(KS_LOG_DEBUG, "Transport Removed: %s\n", name);

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
