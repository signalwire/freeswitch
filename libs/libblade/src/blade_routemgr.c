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
	ks_pool_t *pool;

	ks_hash_t *routes; // id, id
};


static void blade_routemgr_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
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
	brmgr->pool = pool;

	// @note can let removes free keys and values for routes, both are strings and allocated from the same pool as the hash itself
	ks_hash_create(&brmgr->routes, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, brmgr->pool);
	ks_assert(brmgr->routes);

	ks_pool_set_cleanup(pool, brmgr, NULL, blade_routemgr_cleanup);

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

	ks_assert(brmgr);

	pool = brmgr->pool;

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_routemgr_handle_get(blade_routemgr_t *brmgr)
{
	ks_assert(brmgr);

	return brmgr->handle;
}

KS_DECLARE(blade_session_t *) blade_routemgr_route_lookup(blade_routemgr_t *brmgr, const char *target)
{
	blade_session_t *bs = NULL;
	const char *router = NULL;

	ks_assert(brmgr);
	ks_assert(target);

	router = (const char *)ks_hash_search(brmgr->routes, (void *)target, KS_READLOCKED);
	if (router) bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(brmgr->handle), router);
	ks_hash_read_unlock(brmgr->routes);

	return bs;
}

KS_DECLARE(ks_status_t) blade_routemgr_route_add(blade_routemgr_t *brmgr, const char *target, const char *router)
{
	char *key = NULL;
	char *value = NULL;

	ks_assert(brmgr);
	ks_assert(target);
	ks_assert(router);

	key = ks_pstrdup(brmgr->pool, target);
	value = ks_pstrdup(brmgr->pool, router);

	ks_hash_insert(brmgr->routes, (void *)key, (void *)value);

	ks_log(KS_LOG_DEBUG, "Route Added: %s through %s\n", key, value);

	blade_handle_rpcregister(brmgr->handle, target, KS_FALSE, NULL, NULL);

	return KS_STATUS_SUCCESS;

}

KS_DECLARE(ks_status_t) blade_routemgr_route_remove(blade_routemgr_t *brmgr, const char *target)
{
	ks_assert(brmgr);
	ks_assert(target);

	ks_hash_remove(brmgr->routes, (void *)target);

	ks_log(KS_LOG_DEBUG, "Route Removed: %s\n", target);

	blade_handle_rpcregister(brmgr->handle, target, KS_TRUE, NULL, NULL);

	// @note protocols are cleaned up here because routes can be removed that are not locally connected with a session but still
	// have protocols published to the master node from further downstream, in which case if a route is announced upstream to be
	// removed, a master node is still able to catch that here even when there is no direct session, but is also hit when there
	// is a direct session being terminated

	blade_mastermgr_controller_remove(blade_handle_mastermgr_get(brmgr->handle), target);

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
