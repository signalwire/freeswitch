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

struct blade_sessionmgr_s {
	blade_handle_t *handle;

	blade_session_t *loopback;
	blade_session_t *upstream;
	ks_hash_t *sessions; // id, blade_session_t*
	ks_hash_t *callbacks; // id, blade_session_callback_data_t*
};

struct blade_session_callback_data_s {
	const char *id;
	void *data;
	blade_session_callback_t callback;
};


static void blade_sessionmgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_sessionmgr_t *bsmgr = (blade_sessionmgr_t *)ptr;

	//ks_assert(bsmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

static void blade_session_callback_data_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_session_callback_data_t *bscd = (blade_session_callback_data_t *)ptr;

	ks_assert(bscd);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free(&bscd->id);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_sessionmgr_create(blade_sessionmgr_t **bsmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_sessionmgr_t *bsmgr = NULL;

	ks_assert(bsmgrP);
	
	ks_pool_open(&pool);
	ks_assert(pool);

	bsmgr = ks_pool_alloc(pool, sizeof(blade_sessionmgr_t));
	bsmgr->handle = bh;

	ks_hash_create(&bsmgr->sessions, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bsmgr->sessions);

	ks_hash_create(&bsmgr->callbacks, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bsmgr->callbacks);

	ks_pool_set_cleanup(bsmgr, NULL, blade_sessionmgr_cleanup);

	*bsmgrP = bsmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_destroy(blade_sessionmgr_t **bsmgrP)
{
	blade_sessionmgr_t *bsmgr = NULL;
	ks_pool_t *pool;

	ks_assert(bsmgrP);
	ks_assert(*bsmgrP);

	bsmgr = *bsmgrP;
	*bsmgrP = NULL;

	pool = ks_pool_get(bsmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_sessionmgr_handle_get(blade_sessionmgr_t *bsmgr)
{
	ks_assert(bsmgr);
	return bsmgr->handle;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_startup(blade_sessionmgr_t *bsmgr, config_setting_t *config)
{
	ks_assert(bsmgr);

	blade_session_create(&bsmgr->loopback, bsmgr->handle, BLADE_SESSION_FLAGS_LOOPBACK, NULL);
	ks_assert(bsmgr->loopback);

	ks_log(KS_LOG_DEBUG, "Session (%s) created\n", blade_session_id_get(bsmgr->loopback));

	if (blade_session_startup(bsmgr->loopback) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Session (%s) startup failed\n", blade_session_id_get(bsmgr->loopback));
		blade_session_destroy(&bsmgr->loopback);
		return KS_STATUS_FAIL;
	}

	ks_log(KS_LOG_DEBUG, "Session (%s) started\n", blade_session_id_get(bsmgr->loopback));

	blade_sessionmgr_session_add(bsmgr, bsmgr->loopback);

	blade_session_state_set(bsmgr->loopback, BLADE_SESSION_STATE_STARTUP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_shutdown(blade_sessionmgr_t *bsmgr)
{
	ks_hash_iterator_t *it = NULL;

	ks_assert(bsmgr);

	//if (bsmgr->loopback) {
	//	blade_session_hangup(bsmgr->loopback);
	//	ks_sleep_ms(100);
	//}

	ks_hash_read_lock(bsmgr->sessions);
	for (it = ks_hash_first(bsmgr->sessions, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_session_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		blade_session_hangup(value);
	}
	ks_hash_read_unlock(bsmgr->sessions);
	while (ks_hash_count(bsmgr->sessions) > 0) ks_sleep_ms(100);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_session_t *) blade_sessionmgr_loopback_lookup(blade_sessionmgr_t *bsmgr)
{
	ks_assert(bsmgr);

	blade_session_read_lock(bsmgr->loopback, KS_TRUE);
	return bsmgr->loopback;
}

KS_DECLARE(blade_session_t *) blade_sessionmgr_upstream_lookup(blade_sessionmgr_t *bsmgr)
{
	ks_assert(bsmgr);

	if (bsmgr->upstream) blade_session_read_lock(bsmgr->upstream, KS_TRUE);
	return bsmgr->upstream;
}

KS_DECLARE(blade_session_t *) blade_sessionmgr_session_lookup(blade_sessionmgr_t *bsmgr, const char *id)
{
	blade_session_t *bs = NULL;

	ks_assert(bsmgr);
	ks_assert(id);

	bs = ks_hash_search(bsmgr->sessions, (void *)id, KS_READLOCKED);
	if (bs) blade_session_read_lock(bs, KS_TRUE);
	ks_hash_read_unlock(bsmgr->sessions);

	return bs;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_session_add(blade_sessionmgr_t *bsmgr, blade_session_t *bs)
{
	char *key = NULL;

	ks_assert(bsmgr);
	ks_assert(bs);

	key = ks_pstrdup(ks_pool_get(bsmgr), blade_session_id_get(bs));
	ks_hash_insert(bsmgr->sessions, (void *)key, bs);

	ks_log(KS_LOG_DEBUG, "Session Added: %s\n", key);

	if (blade_session_upstream(bs)) bsmgr->upstream = bs;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_session_remove(blade_sessionmgr_t *bsmgr, blade_session_t *bs)
{
	const char *id = NULL;
	blade_routemgr_t *routemgr = NULL;

	ks_assert(bsmgr);
	ks_assert(bs);

	blade_session_write_lock(bs, KS_TRUE);

	id = blade_session_id_get(bs);
	ks_hash_remove(bsmgr->sessions, (void *)id);

	ks_log(KS_LOG_DEBUG, "Session Removed: %s\n", id);

	routemgr = blade_handle_routemgr_get(bsmgr->handle);
	if (blade_session_upstream(bs)) {
		bsmgr->upstream = NULL;
		blade_routemgr_local_set(routemgr, NULL);
		blade_routemgr_master_set(routemgr, NULL);

		ks_hash_read_lock(bsmgr->sessions);
		for (ks_hash_iterator_t *it = ks_hash_first(bsmgr->sessions, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			blade_session_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

			if (blade_session_loopback(value)) continue;

			blade_session_hangup(value);
		}
		ks_hash_read_unlock(bsmgr->sessions);
	}

	blade_session_write_unlock(bs);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_callback_add(blade_sessionmgr_t *bsmgr, void *data, blade_session_callback_t callback, const char **id)
{
	ks_pool_t *pool = NULL;
	blade_session_callback_data_t *bscd = NULL;
	uuid_t uuid;

	ks_assert(bsmgr);
	ks_assert(callback);
	ks_assert(id);

	pool = ks_pool_get(bsmgr);

	ks_uuid(&uuid);

	bscd = ks_pool_alloc(pool, sizeof(blade_session_callback_data_t));
	bscd->id = ks_uuid_str(pool, &uuid);
	bscd->data = data;
	bscd->callback = callback;

	ks_pool_set_cleanup(bscd, NULL, blade_session_callback_data_cleanup);

	ks_hash_insert(bsmgr->callbacks, (void *)ks_pstrdup(pool, bscd->id), bscd);

	*id = bscd->id;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_sessionmgr_callback_remove(blade_sessionmgr_t *bsmgr, const char *id)
{
	ks_assert(bsmgr);
	ks_assert(id);

	ks_hash_remove(bsmgr->callbacks, (void *)id);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_sessionmgr_callback_execute(blade_sessionmgr_t *bsmgr, blade_session_t *bs, blade_session_state_condition_t condition)
{
	ks_assert(bsmgr);
	ks_assert(bs);

	if (blade_session_state_get(bs) == BLADE_SESSION_STATE_NONE) return;

	ks_hash_read_lock(bsmgr->callbacks);
	for (ks_hash_iterator_t *it = ks_hash_first(bsmgr->callbacks, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_session_callback_data_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		value->callback(bs, condition, value->data);
	}
	ks_hash_read_unlock(bsmgr->callbacks);
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
