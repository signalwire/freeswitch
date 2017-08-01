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

struct blade_mastermgr_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	// @todo how does "exclusive" play into the controllers, does "exclusive" mean only one provider can exist for a given protocol and realm? what does non exclusive mean?
	ks_hash_t *protocols; // protocols that have been published with blade.publish, and the details to locate a protocol controller with blade.locate
};


static void blade_mastermgr_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_mastermgr_t *bmmgr = (blade_mastermgr_t *)ptr;

	//ks_assert(bmmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_mastermgr_create(blade_mastermgr_t **bmmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_mastermgr_t *bmmgr = NULL;

	ks_assert(bmmgrP);

	ks_pool_open(&pool);
	ks_assert(pool);

	bmmgr = ks_pool_alloc(pool, sizeof(blade_mastermgr_t));
	bmmgr->handle = bh;
	bmmgr->pool = pool;

	ks_hash_create(&bmmgr->protocols, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bmmgr->pool);
	ks_assert(bmmgr->protocols);

	ks_pool_set_cleanup(pool, bmmgr, NULL, blade_mastermgr_cleanup);

	*bmmgrP = bmmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_destroy(blade_mastermgr_t **bmmgrP)
{
	blade_mastermgr_t *bmmgr = NULL;
	ks_pool_t *pool;

	ks_assert(bmmgrP);
	ks_assert(*bmmgrP);

	bmmgr = *bmmgrP;
	*bmmgrP = NULL;

	ks_assert(bmmgr);

	pool = bmmgr->pool;

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_mastermgr_handle_get(blade_mastermgr_t *bmmgr)
{
	ks_assert(bmmgr);

	return bmmgr->handle;
}

KS_DECLARE(ks_status_t) blade_mastermgr_purge(blade_mastermgr_t *bmmgr, const char *nodeid)
{
	ks_hash_t *cleanup = NULL;

	ks_hash_write_lock(bmmgr->protocols);
	for (ks_hash_iterator_t *it = ks_hash_first(bmmgr->protocols, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		blade_protocol_t *bp = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&bp);

		if (blade_protocol_purge(bp, nodeid)) {
			if (!cleanup) ks_hash_create(&cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bmmgr->pool);
			ks_hash_insert(cleanup, (void *)key, bp);
		}
	}
	if (cleanup) {
		for (ks_hash_iterator_t *it = ks_hash_first(cleanup, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const char *key = NULL;
			blade_protocol_t *bp = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&bp);

			ks_log(KS_LOG_DEBUG, "Protocol Removed: %s\n", key);
			ks_hash_remove(bmmgr->protocols, (void *)key);
		}
		ks_hash_destroy(&cleanup);
	}

	ks_hash_write_unlock(bmmgr->protocols);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_protocol_t *) blade_mastermgr_protocol_lookup(blade_mastermgr_t *bmmgr, const char *protocol, const char *realm)
{
	blade_protocol_t *bp = NULL;
	char *key = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_READLOCKED);
	// @todo if (bp) blade_protocol_read_lock(bp);
	ks_hash_read_unlock(bmmgr->protocols);

	return bp;
}

KS_DECLARE(ks_status_t) blade_mastermgr_controller_add(blade_mastermgr_t *bmmgr, const char *protocol, const char *realm, const char *controller)
{
	blade_protocol_t *bp = NULL;
	char *key = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(controller);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	ks_hash_write_lock(bmmgr->protocols);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_UNLOCKED);
	if (bp) {
		// @todo deal with exclusive stuff when the protocol is already registered
	}

	if (!bp) {
		blade_protocol_create(&bp, bmmgr->pool, protocol, realm);
		ks_assert(bp);

		ks_log(KS_LOG_DEBUG, "Protocol Added: %s\n", key);
		ks_hash_insert(bmmgr->protocols, (void *)ks_pstrdup(bmmgr->pool, key), bp);
	}

	blade_protocol_controllers_add(bp, controller);

	ks_pool_free(bmmgr->pool, &key);

	ks_hash_write_unlock(bmmgr->protocols);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_channel_add(blade_mastermgr_t *bmmgr, const char *protocol, const char *realm, const char *channel)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_protocol_t *bp = NULL;
	char *key = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(channel);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_READLOCKED);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	blade_protocol_channel_add(bp, channel);

done:
	ks_pool_free(bmmgr->pool, &key);

	ks_hash_read_unlock(bmmgr->protocols);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_channel_remove(blade_mastermgr_t *bmmgr, const char *protocol, const char *realm, const char *channel)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_protocol_t *bp = NULL;
	char *key = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(channel);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_READLOCKED);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	blade_protocol_channel_remove(bp, channel);

done:
	ks_pool_free(bmmgr->pool, &key);

	ks_hash_read_unlock(bmmgr->protocols);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_channel_authorize(blade_mastermgr_t *bmmgr, ks_bool_t remove, const char *protocol, const char *realm, const char *channel, const char *controller, const char *target)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_protocol_t *bp = NULL;
	char *key = NULL;
	//ks_hash_t *cleanup = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(channel);
	ks_assert(controller);
	ks_assert(target);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_READLOCKED);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	ret = blade_protocol_channel_authorize(bp, remove, channel, controller, target);

done:
	ks_pool_free(bmmgr->pool, &key);

	ks_hash_read_unlock(bmmgr->protocols);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_mastermgr_channel_verify(blade_mastermgr_t *bmmgr, const char *protocol, const char *realm, const char *channel, const char *target)
{
	ks_bool_t ret = KS_FALSE;
	blade_protocol_t *bp = NULL;
	char *key = NULL;
	//ks_hash_t *cleanup = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(channel);
	ks_assert(target);

	key = ks_psprintf(bmmgr->pool, "%s@%s", protocol, realm);

	bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, (void *)key, KS_READLOCKED);
	if (!bp) goto done;

	ret = blade_protocol_channel_verify(bp, channel, target);

done:
	ks_pool_free(bmmgr->pool, &key);

	ks_hash_read_unlock(bmmgr->protocols);

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
