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

struct blade_routemgr_s {
	blade_handle_t *handle;

	const char *local_nodeid;
	ks_rwl_t *local_lock;

	const char *master_nodeid;
	ks_rwl_t *master_lock;

	ks_hash_t *routes; // target nodeid, downstream router nodeid
	ks_hash_t *identities; // identity, target nodeid
};


static void blade_routemgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_routemgr_t *brmgr = (blade_routemgr_t *)ptr;

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

KS_DECLARE(ks_status_t) blade_routemgr_create(blade_routemgr_t **brmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_routemgr_t *brmgr = NULL;

	ks_assert(brmgrP);
	
	ks_pool_open(&pool);
	ks_assert(pool);

	brmgr = ks_pool_alloc(pool, sizeof(blade_routemgr_t));
	brmgr->handle = bh;

	ks_rwl_create(&brmgr->local_lock, pool);
	ks_assert(brmgr->local_lock);

	ks_rwl_create(&brmgr->master_lock, pool);
	ks_assert(brmgr->master_lock);

	// @note can let removes free keys and values for routes and identity, both use keys and values that are strings allocated from the same pool as the hash itself
	ks_hash_create(&brmgr->routes, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(brmgr->routes);

	ks_hash_create(&brmgr->identities, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(brmgr->routes);

	ks_pool_set_cleanup(brmgr, NULL, blade_routemgr_cleanup);

	*brmgrP = brmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_routemgr_destroy(blade_routemgr_t **brmgrP)
{
	blade_routemgr_t *brmgr = NULL;
	ks_pool_t *pool;

	ks_assert(brmgrP);
	ks_assert(*brmgrP);

	brmgr = *brmgrP;
	*brmgrP = NULL;

	pool = ks_pool_get(brmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_routemgr_handle_get(blade_routemgr_t *brmgr)
{
	ks_assert(brmgr);

	return brmgr->handle;
}

KS_DECLARE(ks_status_t) blade_routemgr_local_set(blade_routemgr_t *brmgr, const char *nodeid)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(brmgr);

	ks_rwl_write_lock(brmgr->local_lock);

	if (brmgr->local_nodeid) {
		ret = KS_STATUS_DUPLICATE_OPERATION;
		goto done;
	}
	if (nodeid) brmgr->local_nodeid = ks_pstrdup(ks_pool_get(brmgr), nodeid);

	ks_log(KS_LOG_DEBUG, "Local NodeID: %s\n", nodeid);

done:
	ks_rwl_write_unlock(brmgr->local_lock);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_routemgr_local_check(blade_routemgr_t *brmgr, const char *target)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);
	ks_assert(target);

	ks_rwl_read_lock(brmgr->local_lock);

	ret = !ks_safe_strcasecmp(brmgr->local_nodeid, target);

	if (!ret) {
		// @todo must parse target to an identity, and back to a properly formatted identity key
		blade_identity_t *identity = NULL;
		ks_pool_t *pool = ks_pool_get(brmgr);

		blade_identity_create(&identity, pool);
		if (blade_identity_parse(identity, target) == KS_STATUS_SUCCESS) {
			char *key = ks_psprintf(pool, "%s@%s/%s", blade_identity_user_get(identity), blade_identity_host_get(identity), blade_identity_path_get(identity));
			const char *value = (const char *)ks_hash_search(brmgr->identities, (void *)key, KS_READLOCKED);

			if (value) ret = !ks_safe_strcasecmp(brmgr->local_nodeid, value);

			ks_hash_read_unlock(brmgr->identities);

			ks_pool_free(&key);
		}

		blade_identity_destroy(&identity);
	}

	ks_rwl_read_unlock(brmgr->local_lock);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_routemgr_local_copy(blade_routemgr_t *brmgr, const char **nodeid)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);
	ks_assert(nodeid);

	*nodeid = NULL;

	ks_rwl_read_lock(brmgr->local_lock);

	if (brmgr->local_nodeid) {
		ret = KS_TRUE;
		*nodeid = ks_pstrdup(ks_pool_get(brmgr), brmgr->local_nodeid);
	}

	ks_rwl_read_unlock(brmgr->local_lock);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_routemgr_local_pack(blade_routemgr_t *brmgr, cJSON *json, const char *key)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);
	ks_assert(json);
	ks_assert(key);

	ks_rwl_read_lock(brmgr->local_lock);

	if (brmgr->local_nodeid) {
		ret = KS_TRUE;
		cJSON_AddStringToObject(json, key, brmgr->local_nodeid);
	}

	ks_rwl_read_unlock(brmgr->local_lock);

	return ret;
}

KS_DECLARE(blade_session_t *) blade_routemgr_upstream_lookup(blade_routemgr_t *brmgr)
{
	blade_session_t *bs = NULL;

	ks_assert(brmgr);

	ks_rwl_read_lock(brmgr->local_lock);

	if (brmgr->local_nodeid) bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(brmgr->handle), brmgr->local_nodeid);
	
	ks_rwl_read_unlock(brmgr->local_lock);

	return bs;
}

KS_DECLARE(ks_status_t) blade_routemgr_master_set(blade_routemgr_t *brmgr, const char *nodeid)
{
	ks_assert(brmgr);

	ks_rwl_write_lock(brmgr->master_lock);

	if (brmgr->master_nodeid) ks_pool_free(&brmgr->master_nodeid);
	if (nodeid) brmgr->master_nodeid = ks_pstrdup(ks_pool_get(brmgr), nodeid);

	ks_rwl_write_unlock(brmgr->master_lock);

	ks_log(KS_LOG_DEBUG, "Master NodeID: %s\n", nodeid);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_routemgr_master_check(blade_routemgr_t *brmgr, const char *target)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);
	ks_assert(target);

	ks_rwl_read_lock(brmgr->master_lock);

	ret = ks_safe_strcasecmp(brmgr->master_nodeid, target) == 0;

	// @todo may also need to check against master identities, there are a number of ways master identities
	// could be propagated to ensure this check works for identities, but for now just assume that master cannot
	// use identities for these checks which should generally only be done to validate certain blade CoreRPC's which
	// expect the responder to be the master, and which get packed in the function below
	// the following would only work on the master itself, where it registered it's own identities and thus has the
	// identities in the identities mapping, other nodes do not see master identity registrations and cannot currently
	// validate master identities
	//if (!ret) {
	//	const char *nodeid = (const char *)ks_hash_search(brmgr->identities, (void *)target, KS_READLOCKED);
	//	ret = nodeid && !ks_safe_strcasecmp(nodeid, brmgr->master_nodeid);
	//	ks_hash_read_unlock(brmgr->identities);
	//}

	ks_rwl_read_unlock(brmgr->master_lock);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_routemgr_master_pack(blade_routemgr_t *brmgr, cJSON *json, const char *key)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);
	ks_assert(json);
	ks_assert(key);

	ks_rwl_read_lock(brmgr->master_lock);
	
	if (brmgr->master_nodeid) {
		ret = KS_TRUE;
		cJSON_AddStringToObject(json, key, brmgr->master_nodeid);
		// @todo may need to pack master identities into the json as well, for now just use nodeid only and
		// assume master nodes have no identities, however this call is primarily used only to force certain
		// blade CoreRPC's to route to the known master node, but is also used to pass to downstream nodes
		// when they connect
	}
	
	ks_rwl_read_unlock(brmgr->master_lock);

	return ret;
}

KS_DECLARE(ks_bool_t) blade_routemgr_master_local(blade_routemgr_t *brmgr)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(brmgr);

	ks_rwl_read_lock(brmgr->master_lock);

	ret = brmgr->master_nodeid && brmgr->local_nodeid && !ks_safe_strcasecmp(brmgr->master_nodeid, brmgr->local_nodeid);

	//ret = brmgr->master_nodeid && brmgr->localid && !ks_safe_strcasecmp(brmgr->master_nodeid, brmgr->local_nodeid);

	ks_rwl_read_unlock(brmgr->master_lock);

	return ret;
}

KS_DECLARE(blade_session_t *) blade_routemgr_route_lookup(blade_routemgr_t *brmgr, const char *target)
{
	blade_session_t *bs = NULL;
	const char *router = NULL;

	ks_assert(brmgr);
	ks_assert(target);

	router = (const char *)ks_hash_search(brmgr->routes, (void *)target, KS_READLOCKED);
	if (!router) {
		// @todo this is all really inefficient, but we need the string to be parsed and recombined to ensure correctness for key matching
		blade_identity_t *identity = NULL;
		ks_pool_t *pool = ks_pool_get(brmgr);

		blade_identity_create(&identity, pool);
		if (blade_identity_parse(identity, target) == KS_STATUS_SUCCESS) {
			char *key = ks_psprintf(pool, "%s@%s/%s", blade_identity_user_get(identity), blade_identity_host_get(identity), blade_identity_path_get(identity));

			router = (const char *)ks_hash_search(brmgr->identities, (void *)key, KS_READLOCKED);
			ks_hash_read_unlock(brmgr->identities);

			ks_pool_free(&key);
		}

		blade_identity_destroy(&identity);
	}
	if (router) bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(brmgr->handle), router);
	ks_hash_read_unlock(brmgr->routes);

	return bs;
}

ks_status_t blade_routemgr_purge(blade_routemgr_t *brmgr, const char *target)
{
	ks_hash_t* cleanup = NULL;

	ks_assert(brmgr);
	ks_assert(target);

	// @note this approach is deliberately slower than it could be, as it ensures that if there is a race condition and another session has registered
	// the same identity before a prior session times out, then the correct targetted random nodeid is matched to confirm the identity removal and will
	// not remove if the target isn't what is expected

	ks_hash_write_lock(brmgr->identities);

	for (ks_hash_iterator_t *it = ks_hash_first(brmgr->identities, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key = NULL;
		const char *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		if (value && !ks_safe_strcasecmp(value, target)) {
			if (!cleanup) ks_hash_create(&cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, ks_pool_get(brmgr));
			ks_hash_insert(cleanup, (const void *)key, (void *)value);
		}
	}

	if (cleanup) {
		for (ks_hash_iterator_t *it = ks_hash_first(cleanup, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const char *key = NULL;
			const char *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

			ks_log(KS_LOG_DEBUG, "Identity Removed: %s through %s\n", key, value);

			ks_hash_remove(brmgr->identities, (void *)key);
		}
	}

	ks_hash_write_unlock(brmgr->identities);

	if (cleanup) ks_hash_destroy(&cleanup);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_routemgr_route_add(blade_routemgr_t *brmgr, const char *target, const char *router)
{
	ks_pool_t *pool = NULL;
	char *key = NULL;
	char *value = NULL;

	ks_assert(brmgr);
	ks_assert(target);
	ks_assert(router);

	pool = ks_pool_get(brmgr);

	key = ks_pstrdup(pool, target);
	value = ks_pstrdup(pool, router);

	ks_hash_insert(brmgr->routes, (void *)key, (void *)value);

	ks_log(KS_LOG_DEBUG, "Route Added: %s through %s\n", key, value);

	blade_handle_rpcroute(brmgr->handle, target, KS_FALSE, NULL, NULL);

	if (blade_routemgr_master_local(brmgr)) {
		cJSON *params = cJSON_CreateObject();
		cJSON_AddStringToObject(params, "nodeid", target);
		blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(brmgr->handle), BLADE_RPCBROADCAST_COMMAND_EVENT, NULL, "blade.presence", "join", "joined", params, NULL, NULL);
		cJSON_Delete(params);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_routemgr_route_remove(blade_routemgr_t *brmgr, const char *target)
{
	ks_assert(brmgr);
	ks_assert(target);

	ks_hash_remove(brmgr->routes, (void *)target);

	ks_log(KS_LOG_DEBUG, "Route Removed: %s\n", target);

	blade_handle_rpcroute(brmgr->handle, target, KS_TRUE, NULL, NULL);

	blade_subscriptionmgr_purge(blade_handle_subscriptionmgr_get(brmgr->handle), target);

	// @note protocols are cleaned up here because routes can be removed that are not locally connected with a session but still
	// have protocols published to the master node from further downstream, in which case if a route is announced upstream to be
	// removed, a master node is still able to catch that here even when there is no direct session, but is also hit when there
	// is a direct session being terminated

	blade_mastermgr_purge(blade_handle_mastermgr_get(brmgr->handle), target);

	blade_routemgr_purge(brmgr, target);

	if (blade_routemgr_master_local(brmgr)) {
		cJSON *params = cJSON_CreateObject();
		cJSON_AddStringToObject(params, "nodeid", target);
		blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(brmgr->handle), BLADE_RPCBROADCAST_COMMAND_EVENT, NULL, "blade.presence", "leave", "left", params, NULL, NULL);
		cJSON_Delete(params);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_routemgr_identity_add(blade_routemgr_t *brmgr, blade_identity_t *identity, const char *target)
{
	ks_pool_t *pool = NULL;
	char *key = NULL;
	char *value = NULL;

	ks_assert(brmgr);
	ks_assert(identity);
	ks_assert(target);

	pool = ks_pool_get(brmgr);

	key = ks_psprintf(pool, "%s@%s/%s", blade_identity_user_get(identity), blade_identity_host_get(identity), blade_identity_path_get(identity));
	value = ks_pstrdup(pool, target);

	ks_hash_insert(brmgr->identities, (void *)key, (void *)value);

	ks_log(KS_LOG_DEBUG, "Identity Added: %s through %s\n", key, value);

	//if (blade_routemgr_master_local(blade_handle_routemgr_get(brmgr->handle))) {
	//	cJSON *params = cJSON_CreateObject();
	//	cJSON_AddStringToObject(params, "identity", blade_identity_uri_get(identity)); // full identity uri string
	//	cJSON_AddStringToObject(params, "nodeid", target);
	//	blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(brmgr->handle), BLADE_RPCBROADCAST_COMMAND_EVENT, NULL, "blade.presence", "join", "joined", params, NULL, NULL);
	//	cJSON_Delete(params);
	//}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_routemgr_identity_remove(blade_routemgr_t *brmgr, blade_identity_t *identity, const char *target)
{
	ks_pool_t *pool = NULL;
	char *key = NULL;
	const char *value = NULL;

	ks_assert(brmgr);
	ks_assert(identity);
	ks_assert(target);

	pool = ks_pool_get(brmgr);

	key = ks_psprintf(pool, "%s@%s/%s", blade_identity_user_get(identity), blade_identity_host_get(identity), blade_identity_path_get(identity));

	// @note this approach is deliberately slower than it could be, as it ensures that if there is a race condition and another session has registered
	// the same identity before a prior session times out, then the correct targetted random nodeid is matched to confirm the identity removal and will
	// not remove if the target isn't what is expected

	ks_hash_write_lock(brmgr->identities);

	value = (const char *)ks_hash_search(brmgr->identities, (void *)key, KS_UNLOCKED);

	if (value && !ks_safe_strcasecmp(value, target)) {
		ks_hash_remove(brmgr->identities, (void *)key);

		ks_log(KS_LOG_DEBUG, "Identity Removed: %s through %s\n", key, value);
	}

	ks_hash_write_unlock(brmgr->identities);

	//if (blade_routemgr_master_local(blade_handle_routemgr_get(brmgr->handle))) {
	//	cJSON *params = cJSON_CreateObject();
	//	cJSON_AddStringToObject(params, "nodeid", target);
	//	blade_subscriptionmgr_broadcast(blade_handle_subscriptionmgr_get(brmgr->handle), BLADE_RPCBROADCAST_COMMAND_EVENT, NULL, "blade.presence", "leave", "left", params, NULL, NULL);
	//	cJSON_Delete(params);
	//}

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
