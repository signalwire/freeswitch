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

	const char *master_nodeid;
	// @todo how does "exclusive" play into the controllers, does "exclusive" mean only one provider can exist for a given protocol and realm? what does non exclusive mean?
	ks_hash_t *realms;
	//ks_hash_t *protocols; // protocols that have been published with blade.publish, and the details to locate a protocol controller with blade.locate
};


static void blade_mastermgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
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

	ks_hash_create(&bmmgr->realms, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bmmgr->realms);

	ks_pool_set_cleanup(bmmgr, NULL, blade_mastermgr_cleanup);

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

	pool = ks_pool_get(bmmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_mastermgr_handle_get(blade_mastermgr_t *bmmgr)
{
	ks_assert(bmmgr);

	return bmmgr->handle;
}

ks_status_t blade_mastermgr_config(blade_mastermgr_t *bmmgr, config_setting_t *config)
{
	ks_pool_t *pool = NULL;
	config_setting_t *master = NULL;
	config_setting_t *master_nodeid = NULL;
	const char *nodeid = NULL;

	ks_assert(bmmgr);

	pool = ks_pool_get(bmmgr);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	master = config_setting_get_member(config, "master");
	if (master) {
		master_nodeid = config_lookup_from(master, "nodeid");
		if (!master_nodeid) return KS_STATUS_FAIL;

		if (config_setting_type(master_nodeid) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
		nodeid = config_setting_get_string(master_nodeid);
	}

	if (master) {
		bmmgr->master_nodeid = ks_pstrdup(pool, nodeid);

		ks_log(KS_LOG_DEBUG, "Configured\n");
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_startup(blade_mastermgr_t *bmmgr, config_setting_t *config)
{
	ks_pool_t *pool = NULL;

	ks_assert(bmmgr);

	pool = ks_pool_get(bmmgr);

	if (blade_mastermgr_config(bmmgr, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_mastermgr_config failed\n");
		return KS_STATUS_FAIL;
	}

	if (bmmgr->master_nodeid) {
		blade_realm_t *br = NULL;

		blade_upstreammgr_localid_set(blade_handle_upstreammgr_get(bmmgr->handle), bmmgr->master_nodeid);
		blade_upstreammgr_masterid_set(blade_handle_upstreammgr_get(bmmgr->handle), bmmgr->master_nodeid);

		// build the internal blade protocol controlled by the master for the purpose of global event channels for node presence
		blade_realm_create(&br, pool, "blade");
		// @note realm should remain public, these event channels must be available to any node
		blade_mastermgr_realm_add(bmmgr, br);

		blade_mastermgr_realm_protocol_controller_add(bmmgr, "blade", "presence", bmmgr->master_nodeid);

		blade_mastermgr_realm_protocol_channel_add(bmmgr, "blade", "presence", "join");
		blade_mastermgr_realm_protocol_channel_add(bmmgr, "blade", "presence", "leave");
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_shutdown(blade_mastermgr_t *bmmgr)
{
	ks_assert(bmmgr);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_purge(blade_mastermgr_t *bmmgr, const char *nodeid)
{
	ks_pool_t *pool = NULL;
	ks_hash_t *cleanup = NULL;

	ks_assert(bmmgr);

	pool = ks_pool_get(bmmgr);

	ks_hash_write_lock(bmmgr->realms);

	for (ks_hash_iterator_t *it = ks_hash_first(bmmgr->realms, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *realm = NULL;
		blade_realm_t *br = NULL;
		ks_hash_this(it, (const void **)&realm, NULL, (void **)&br);

		blade_realm_write_lock(br);

		for (ks_hash_iterator_t *it2 = blade_realm_protocols_iterator(br, KS_UNLOCKED); it2; it2 = ks_hash_next(&it2)) {
			const char *protocol = NULL;
			blade_protocol_t *bp = NULL;

			ks_hash_this(it2, (const void **)&protocol, NULL, (void **)&bp);

			blade_protocol_write_lock(bp);

			if (blade_protocol_purge(bp, nodeid)) {
				if (!blade_protocol_controller_available(bp)) {
					if (!cleanup) ks_hash_create(&cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
					ks_hash_insert(cleanup, (void *)protocol, bp);
				}
				else {
					// @todo not the last controller, may need to propagate that the controller is no longer available?
				}
			}
		}

		if (cleanup) {
			for (ks_hash_iterator_t *it2 = ks_hash_first(cleanup, KS_UNLOCKED); it2; it2 = ks_hash_next(&it2)) {
				const char *protocol = NULL;
				blade_protocol_t *bp = NULL;

				ks_hash_this(it2, (const void **)&protocol, NULL, (void **)&bp);

				blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(bmmgr->handle), BLADE_RPCBROADCAST_COMMAND_PROTOCOL_REMOVE, NULL, protocol, realm, NULL, NULL, NULL, NULL, NULL);

				ks_log(KS_LOG_DEBUG, "Protocol Removed: %s@%s\n", protocol, realm);
				blade_protocol_write_unlock(bp);
				blade_realm_protocol_remove(br, protocol);
			}
			ks_hash_destroy(&cleanup);
		}

		blade_realm_write_unlock(br);
	}

	ks_hash_write_unlock(bmmgr->realms);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_add(blade_mastermgr_t *bmmgr, blade_realm_t *realm)
{
	ks_assert(bmmgr);
	ks_assert(realm);

	ks_log(KS_LOG_DEBUG, "Realm Added: %s\n", blade_realm_name_get(realm));
	ks_hash_insert(bmmgr->realms, (void *)ks_pstrdup(ks_pool_get(bmmgr), blade_realm_name_get(realm)), (void *)realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_mastermgr_realm_remove(blade_mastermgr_t *bmmgr, const char *realm)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bmmgr);
	ks_assert(realm);

	if (ks_hash_remove(bmmgr->realms, (void *)realm)) {
		ret = KS_TRUE;
		ks_log(KS_LOG_DEBUG, "Realm Removed: %s\n", realm);
	}

	return ret;
}

KS_DECLARE(blade_protocol_t *) blade_mastermgr_realm_protocol_lookup(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, ks_bool_t writelocked)
{
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;

	ks_assert(bmmgr);
	ks_assert(protocol);
	ks_assert(realm);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_READLOCKED);
	if (br) bp = blade_realm_protocol_lookup(br, protocol, writelocked);
	ks_hash_read_unlock(bmmgr->realms);

	return bp;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_protocol_controller_add(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, const char *controller)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(controller);

	pool = ks_pool_get(bmmgr);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_TRUE);
	if (bp) {
		// @todo deal with exclusive stuff when the protocol is already registered
	}

	if (!bp) {
		blade_protocol_create(&bp, pool, br, protocol);
		ks_assert(bp);

		blade_protocol_write_lock(bp);

		ks_log(KS_LOG_DEBUG, "Protocol Added: %s@%s\n", protocol, realm);
		blade_realm_protocol_add(br, bp);
	}

	blade_protocol_controller_add(bp, controller);

	blade_protocol_write_unlock(bp);

done:
	ks_hash_read_unlock(bmmgr->realms);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_protocol_controller_remove(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, const char *controller)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(controller);

	pool = ks_pool_get(bmmgr);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_TRUE);
	if (bp) {
		if (blade_protocol_controller_remove(bp, controller)) {
			if (!blade_protocol_controller_available(bp)) {
				blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(bmmgr->handle), BLADE_RPCBROADCAST_COMMAND_PROTOCOL_REMOVE, NULL, protocol, realm, NULL, NULL, NULL, NULL, NULL);

				ks_log(KS_LOG_DEBUG, "Protocol Removed: %s@%s\n", protocol, realm);
				blade_realm_protocol_remove(br, protocol);
			} else {
				// @todo not the last controller, may need to propagate when a specific controller becomes unavailable though
			}
		}
		blade_protocol_write_unlock(bp);
	}

done:
	ks_hash_read_unlock(bmmgr->realms);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_protocol_channel_add(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, const char *channel)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;
	blade_channel_t *bc = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(channel);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_TRUE);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	bc = blade_protocol_channel_lookup(bp, channel, KS_TRUE);
	if (!bc) {
		ret = KS_STATUS_DUPLICATE_OPERATION;
		goto done;
	}

	blade_channel_create(&bc, ks_pool_get(bc), channel);
	ks_assert(bc);

	blade_channel_write_lock(bc);

	if (blade_protocol_channel_add(bp, bc) == KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Protocol Channel Added: %s@%s/%s\n", blade_protocol_name_get(bp), blade_realm_name_get(br), blade_channel_name_get(bc));
	}

done:
	if (bc) blade_channel_write_unlock(bc);
	if (bp) blade_protocol_write_unlock(bp);
	ks_hash_read_unlock(bmmgr->realms);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_protocol_channel_remove(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, const char *channel)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;
	blade_channel_t *bc = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(channel);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_TRUE);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	bc = blade_protocol_channel_lookup(bp, channel, KS_TRUE);
	if (!bc) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	if (blade_protocol_channel_remove(bp, channel)) {
		blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(bmmgr->handle), BLADE_RPCBROADCAST_COMMAND_CHANNEL_REMOVE, NULL, protocol, realm, channel, NULL, NULL, NULL, NULL);
		ks_log(KS_LOG_DEBUG, "Protocol Channel Removed: %s@%s/%s\n", blade_protocol_name_get(bp), blade_realm_name_get(br), blade_channel_name_get(bc));
		blade_channel_write_unlock(bc);
		blade_channel_destroy(&bc);
	}

done:
	if (bp) blade_protocol_write_unlock(bp);
	ks_hash_read_unlock(bmmgr->realms);

	return ret;
}

KS_DECLARE(ks_status_t) blade_mastermgr_realm_protocol_channel_authorize(blade_mastermgr_t *bmmgr, ks_bool_t remove, const char *realm, const char *protocol, const char *channel, const char *controller, const char *target)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;
	blade_channel_t *bc = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(channel);
	ks_assert(controller);
	ks_assert(target);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_FALSE);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	if (!blade_protocol_controller_verify(bp, controller)) {
		ret = KS_STATUS_NOT_ALLOWED;
		goto done;
	}

	bc = blade_protocol_channel_lookup(bp, channel, KS_TRUE);
	if (!bc) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	if (remove) {
		if (blade_channel_authorization_remove(bc, target)) {
			ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Removed: %s from %s@%s/%s\n", target, blade_protocol_name_get(bp), blade_realm_name_get(br), blade_channel_name_get(bc));
		} else ret = KS_STATUS_NOT_FOUND;
	} else {
		if (blade_channel_authorization_add(bc, target)) {
			ks_log(KS_LOG_DEBUG, "Protocol Channel Authorization Added: %s to %s@%s/%s\n", target, blade_protocol_name_get(bp), blade_realm_name_get(br), blade_channel_name_get(bc));
		}
	}

done:
	if (bc) blade_channel_write_unlock(bc);
	if (bp) blade_protocol_read_unlock(bp);
	ks_hash_read_unlock(bmmgr->realms);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_mastermgr_realm_protocol_channel_authorization_verify(blade_mastermgr_t *bmmgr, const char *realm, const char *protocol, const char *channel, const char *target)
{
	ks_bool_t ret = KS_FALSE;
	blade_realm_t *br = NULL;
	blade_protocol_t *bp = NULL;
	blade_channel_t *bc = NULL;

	ks_assert(bmmgr);
	ks_assert(realm);
	ks_assert(protocol);
	ks_assert(channel);
	ks_assert(target);

	ks_hash_read_lock(bmmgr->realms);

	br = (blade_realm_t *)ks_hash_search(bmmgr->realms, (void *)realm, KS_UNLOCKED);
	if (!br) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	bp = blade_realm_protocol_lookup(br, protocol, KS_FALSE);
	if (!bp) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	bc = blade_protocol_channel_lookup(bp, channel, KS_FALSE);
	if (!bc) {
		ret = KS_STATUS_NOT_FOUND;
		goto done;
	}

	ret = blade_channel_authorization_verify(bc, target);

	blade_protocol_read_unlock(bp);

done:
	if (bc) blade_channel_read_unlock(bc);
	if (bp) blade_protocol_read_unlock(bp);
	ks_hash_read_unlock(bmmgr->realms);

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
