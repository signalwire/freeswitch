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
	ks_hash_t *protocols; // protocols that have been published with blade.publish, and the details to locate a protocol provider with blade.locate
	ks_hash_t *protocols_cleanup; // keyed by the nodeid, each value is a hash_t* of which contains string keys matching the "protocol@realm" keys to remove each nodeid from as a provider during cleanup
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

	ks_hash_create(&bmmgr->protocols_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bmmgr->pool);
	ks_assert(bmmgr->protocols_cleanup);

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
	ks_hash_t *cleanup = NULL;

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

	cleanup = (ks_hash_t *)ks_hash_search(bmmgr->protocols_cleanup, (void *)controller, KS_UNLOCKED);
	if (!cleanup) {
		ks_hash_create(&cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bmmgr->pool);
		ks_assert(cleanup);

		ks_hash_insert(bmmgr->protocols_cleanup, (void *)ks_pstrdup(bmmgr->pool, controller), cleanup);
	}
	ks_hash_insert(cleanup, (void *)ks_pstrdup(bmmgr->pool, key), (void *)KS_TRUE);
	blade_protocol_controllers_add(bp, controller);
	ks_log(KS_LOG_DEBUG, "Protocol Controller Added: %s to %s\n", controller, key);

	ks_hash_write_unlock(bmmgr->protocols);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_controller_remove(blade_mastermgr_t *bmmgr, const char *controller)
{
	ks_hash_t *cleanup = NULL;

	ks_assert(bmmgr);
	ks_assert(controller);

	ks_hash_write_lock(bmmgr->protocols);
	cleanup = (ks_hash_t *)ks_hash_search(bmmgr->protocols_cleanup, (void *)controller, KS_UNLOCKED);
	if (cleanup) {
		for (ks_hash_iterator_t *it = ks_hash_first(cleanup, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;
			blade_protocol_t *bp = NULL;
			ks_hash_t *controllers = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bp = (blade_protocol_t *)ks_hash_search(bmmgr->protocols, key, KS_UNLOCKED);
			ks_assert(bp); // should not happen when a cleanup still has a provider tracked for a protocol

			ks_log(KS_LOG_DEBUG, "Protocol Controller Removed: %s from %s\n", controller, key);
			blade_protocol_controllers_remove(bp, controller);

			controllers = blade_protocol_controllers_get(bp);
			if (ks_hash_count(controllers) == 0) {
				// @note this depends on locking something outside of the protocol that won't be destroyed, like the top level
				// protocols hash, but assumes then that any reader keeps the top level hash read locked while using the protocol
				// so it cannot be deleted
				ks_log(KS_LOG_DEBUG, "Protocol Removed: %s\n", key);
				ks_hash_remove(bmmgr->protocols, key);
			}
		}
		ks_hash_remove(bmmgr->protocols_cleanup, (void *)controller);
	}
	ks_hash_write_unlock(bmmgr->protocols);

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
