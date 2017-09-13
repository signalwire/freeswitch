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

struct blade_protocol_s {
	blade_realm_t *realm;
	const char *name;
	ks_rwl_t *lock;
	ks_hash_t *controllers;
	ks_hash_t *channels;
	// @todo descriptors (schema, etc) for each method within a protocol
};


static void blade_protocol_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_protocol_t *bp = (blade_protocol_t *)ptr;

	ks_assert(bp);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (bp->name) ks_pool_free(&bp->name);
		if (bp->lock) ks_rwl_destroy(&bp->lock);
		if (bp->controllers) ks_hash_destroy(&bp->controllers);
		if (bp->channels) ks_hash_destroy(&bp->channels);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_protocol_create(blade_protocol_t **bpP, ks_pool_t *pool, blade_realm_t *realm, const char *name)
{
	blade_protocol_t *bp = NULL;

	ks_assert(bpP);
	ks_assert(pool);
	ks_assert(realm);
	ks_assert(name);

	bp = ks_pool_alloc(pool, sizeof(blade_protocol_t));
	bp->realm = realm;
	bp->name = ks_pstrdup(pool, name);

	ks_rwl_create(&bp->lock, pool);
	ks_assert(bp->lock);

	ks_hash_create(&bp->controllers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bp->controllers);

	ks_hash_create(&bp->channels, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bp->channels);

	ks_pool_set_cleanup(bp, NULL, blade_protocol_cleanup);

	*bpP = bp;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_protocol_destroy(blade_protocol_t **bpP)
{
	ks_assert(bpP);
	ks_assert(*bpP);

	ks_pool_free(bpP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_realm_t *) blade_protocol_realm_get(blade_protocol_t *bp)
{
	ks_assert(bp);
	return bp->realm;
}

KS_DECLARE(const char *) blade_protocol_name_get(blade_protocol_t *bp)
{
	ks_assert(bp);
	return bp->name;
}

KS_DECLARE(ks_status_t) blade_protocol_read_lock(blade_protocol_t *bp)
{
	ks_assert(bp);
	return ks_rwl_read_lock(bp->lock);
}

KS_DECLARE(ks_status_t) blade_protocol_read_unlock(blade_protocol_t *bp)
{
	ks_assert(bp);
	return ks_rwl_read_unlock(bp->lock);
}

KS_DECLARE(ks_status_t) blade_protocol_write_lock(blade_protocol_t *bp)
{
	ks_assert(bp);
	return ks_rwl_write_lock(bp->lock);
}

KS_DECLARE(ks_status_t) blade_protocol_write_unlock(blade_protocol_t *bp)
{
	ks_assert(bp);
	return ks_rwl_write_unlock(bp->lock);
}

KS_DECLARE(ks_bool_t) blade_protocol_purge(blade_protocol_t *bp, const char *nodeid)
{
	ks_assert(bp);
	ks_assert(nodeid);

	// @todo iterate all channels, remove the nodeid from all authorized hashes
	ks_hash_write_lock(bp->channels);
	for (ks_hash_iterator_t *it = ks_hash_first(bp->channels, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		ks_hash_t *authorizations = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&authorizations);

		if (ks_hash_remove(authorizations, (void *)nodeid)) {
			ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Removed: %s from %s@%s/%s\n", nodeid, bp->name, blade_realm_name_get(bp->realm), key);
		}
	}
	ks_hash_write_unlock(bp->channels);

	return blade_protocol_controller_remove(bp, nodeid);
}

KS_DECLARE(cJSON *) blade_protocol_controller_pack(blade_protocol_t *bp)
{
	cJSON *controllers = cJSON_CreateObject();

	ks_assert(bp);

	ks_hash_read_lock(bp->controllers);
	for (ks_hash_iterator_t *it = ks_hash_first(bp->controllers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);

		cJSON_AddItemToArray(controllers, cJSON_CreateString(key));
	}
	ks_hash_read_unlock(bp->controllers);

	return controllers;
}

KS_DECLARE(ks_bool_t) blade_protocol_controller_verify(blade_protocol_t *bp, const char *controller)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bp);
	ks_assert(controller);

	ret = (ks_bool_t)(uintptr_t)ks_hash_search(bp->controllers, controller, KS_READLOCKED);
	ks_hash_read_unlock(bp->controllers);

	return ret;
}

KS_DECLARE(ks_status_t) blade_protocol_controller_add(blade_protocol_t *bp, const char *nodeid)
{
	char *key = NULL;

	ks_assert(bp);
	ks_assert(nodeid);

	key = ks_pstrdup(ks_pool_get(bp), nodeid);
	ks_hash_insert(bp->controllers, (void *)key, (void *)KS_TRUE);

	ks_log(KS_LOG_DEBUG, "Protocol Controller Added: %s to %s@%s\n", nodeid, bp->name, blade_realm_name_get(bp->realm));

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_protocol_controller_remove(blade_protocol_t *bp, const char *nodeid)
{
	ks_bool_t ret  = KS_FALSE;

	ks_assert(bp);
	ks_assert(nodeid);

	ks_hash_write_lock(bp->controllers);
	if (ks_hash_remove(bp->controllers, (void *)nodeid)) {
		ret = KS_TRUE;
		ks_log(KS_LOG_DEBUG, "Protocol Controller Removed: %s from %s@%s\n", nodeid, bp->name, blade_realm_name_get(bp->realm));
	}
	ks_hash_write_unlock(bp->controllers);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_protocol_controller_available(blade_protocol_t *bp)
{
	ks_assert(bp);
	return ks_hash_count(bp->controllers) > 0;
}

KS_DECLARE(blade_channel_t *) blade_protocol_channel_lookup(blade_protocol_t *bp, const char *channel, ks_bool_t writelocked)
{
	blade_channel_t *bc = NULL;

	ks_assert(bp);
	ks_assert(channel);

	bc = (blade_channel_t *)ks_hash_search(bp->channels, (void *)channel, KS_READLOCKED);
	if (bc) {
		if (writelocked) blade_channel_write_lock(bc);
		else blade_channel_read_lock(bc);
	}
	ks_hash_read_unlock(bp->channels);

	return bc;
}

KS_DECLARE(ks_status_t) blade_protocol_channel_add(blade_protocol_t *bp, blade_channel_t *channel)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	char *key = NULL;

	ks_assert(bp);
	ks_assert(channel);

	pool = ks_pool_get(bp);

	ks_hash_write_lock(bp->channels);

	if (ks_hash_search(bp->channels, (void *)blade_channel_name_get(channel), KS_UNLOCKED)) {
		ret = KS_STATUS_DUPLICATE_OPERATION;
		goto done;
	}

	key = ks_pstrdup(pool, blade_channel_name_get(channel));
	ks_hash_insert(bp->channels, (void *)key, (void *)channel);

done:

	ks_hash_write_unlock(bp->channels);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_protocol_channel_remove(blade_protocol_t *bp, const char *channel)
{
	ks_assert(bp);
	ks_assert(channel);

	return ks_hash_remove(bp->channels, (void *)channel) != NULL;
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
