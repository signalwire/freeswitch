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
	ks_pool_t *pool;

	const char *name;
	const char *realm;
	ks_hash_t *controllers;
	ks_hash_t *channels;
	// @todo descriptors (schema, etc) for each method within a protocol
};


static void blade_protocol_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_protocol_t *bp = (blade_protocol_t *)ptr;

	ks_assert(bp);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (bp->name) ks_pool_free(bp->pool, &bp->name);
		if (bp->realm) ks_pool_free(bp->pool, &bp->realm);
		if (bp->controllers) ks_hash_destroy(&bp->controllers);
		if (bp->channels) ks_hash_destroy(&bp->channels);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_protocol_create(blade_protocol_t **bpP, ks_pool_t *pool, const char *name, const char *realm)
{
	blade_protocol_t *bp = NULL;

	ks_assert(bpP);
	ks_assert(pool);
	ks_assert(name);
	ks_assert(realm);

	bp = ks_pool_alloc(pool, sizeof(blade_protocol_t));
	bp->pool = pool;
	bp->name = ks_pstrdup(pool, name);
	bp->realm = ks_pstrdup(pool, realm);

	ks_hash_create(&bp->controllers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bp->pool);
	ks_assert(bp->controllers);

	ks_hash_create(&bp->channels, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bp->pool);
	ks_assert(bp->channels);

	ks_pool_set_cleanup(pool, bp, NULL, blade_protocol_cleanup);

	*bpP = bp;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_protocol_destroy(blade_protocol_t **bpP)
{
	blade_protocol_t *bp = NULL;

	ks_assert(bpP);
	ks_assert(*bpP);

	bp = *bpP;

	ks_pool_free(bp->pool, bpP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_protocol_purge(blade_protocol_t *bp, const char *nodeid)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bp);
	ks_assert(nodeid);

	// @todo iterate all channels, remove the nodeid from all authorized hashes
	ks_hash_write_lock(bp->channels);
	for (ks_hash_iterator_t *it = ks_hash_first(bp->channels, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		ks_hash_t *authorizations = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&authorizations);

		if (ks_hash_remove(authorizations, (void *)nodeid)) {
			ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Removed: %s from %s@%s/%s\n", nodeid, bp->name, bp->realm, key);
		}
	}
	ks_hash_write_unlock(bp->channels);

	ks_hash_write_lock(bp->controllers);
	if (ks_hash_remove(bp->controllers, (void *)nodeid)) {
		ks_log(KS_LOG_DEBUG, "Protocol Controller Removed: %s from %s@%s\n", nodeid, bp->name, bp->realm);
	}
	ret = ks_hash_count(bp->controllers) == 0;
	ks_hash_write_unlock(bp->controllers);

	return ret;
}

KS_DECLARE(cJSON *) blade_protocol_controllers_pack(blade_protocol_t *bp)
{
	cJSON *controllers = cJSON_CreateObject();

	ks_assert(bp);

	for (ks_hash_iterator_t *it = ks_hash_first(bp->controllers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);

		cJSON_AddItemToArray(controllers, cJSON_CreateString(key));
	}

	return controllers;
}

KS_DECLARE(ks_status_t) blade_protocol_controllers_add(blade_protocol_t *bp, const char *nodeid)
{
	char *key = NULL;

	ks_assert(bp);
	ks_assert(nodeid);

	key = ks_pstrdup(bp->pool, nodeid);
	ks_hash_insert(bp->controllers, (void *)key, (void *)KS_TRUE);

	ks_log(KS_LOG_DEBUG, "Protocol Controller Added: %s to %s@%s\n", nodeid, bp->name, bp->realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_protocol_channel_add(blade_protocol_t *bp, const char *name)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_hash_t *authorized = NULL;
	char *key = NULL;

	ks_assert(bp);
	ks_assert(name);

	ks_hash_write_lock(bp->channels);

	if (ks_hash_search(bp->channels, (void *)name, KS_UNLOCKED)) {
		ret = KS_STATUS_DUPLICATE_OPERATION;
		goto done;
	}

	ks_hash_create(&authorized, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bp->pool);

	key = ks_pstrdup(bp->pool, name);
	ks_hash_insert(bp->channels, (void *)key, (void *)authorized);

	ks_log(KS_LOG_DEBUG, "Protocol Channel Added: %s to %s@%s\n", key, bp->name, bp->realm);

done:

	ks_hash_write_unlock(bp->channels);

	return ret;
}

KS_DECLARE(ks_status_t) blade_protocol_channel_remove(blade_protocol_t *bp, const char *name)
{
	ks_assert(bp);
	ks_assert(name);

	ks_hash_remove(bp->channels, (void *)name);

	ks_log(KS_LOG_DEBUG, "Protocol Channel Removed: %s from %s@%s\n", name, bp->name, bp->realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_protocol_channel_authorize(blade_protocol_t *bp, ks_bool_t remove, const char *channel, const char *controller, const char *target)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_hash_t *authorizations = NULL;
	ks_bool_t allowed = KS_FALSE;

	ks_assert(bp);
	ks_assert(channel);
	ks_assert(controller);
	ks_assert(target);

	allowed = (ks_bool_t)(intptr_t)ks_hash_search(bp->controllers, (void *)controller, KS_READLOCKED);
	ks_hash_read_unlock(bp->controllers);

	if (!allowed) {
		ret = KS_STATUS_NOT_ALLOWED;
		goto done;
	}

	// @todo verify controller, get ks_hash_t* value based on channel, add target to the channels hash
	authorizations = (ks_hash_t *)ks_hash_search(bp->channels, (void *)channel, KS_READLOCKED);
	if (authorizations) {
		if (remove) {
			if (ks_hash_remove(authorizations, (void *)target)) {
				ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Removed: %s from %s@%s/%s\n", target, bp->name, bp->realm, channel);
			} else ret = KS_STATUS_NOT_FOUND;
		}
		else {
			ks_hash_insert(authorizations, (void *)ks_pstrdup(bp->pool, target), (void *)KS_TRUE);
			ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Added: %s to %s@%s/%s\n", target, bp->name, bp->realm, channel);
		}
	}
	ks_hash_read_unlock(bp->channels);

	if (!authorizations) ret = KS_STATUS_NOT_FOUND;

done:
	return ret;
}

KS_DECLARE(ks_bool_t) blade_protocol_channel_verify(blade_protocol_t *bp, const char *channel, const char *target)
{
	ks_bool_t ret = KS_FALSE;
	ks_hash_t *authorizations = NULL;

	ks_assert(bp);
	ks_assert(channel);
	ks_assert(target);

	// @todo verify controller, get ks_hash_t* value based on channel, add target to the channels hash
	authorizations = (ks_hash_t *)ks_hash_search(bp->channels, (void *)channel, KS_READLOCKED);
	if (authorizations) ret = ks_hash_search(authorizations, (void *)target, KS_UNLOCKED) != NULL;
	ks_hash_read_unlock(bp->channels);

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
