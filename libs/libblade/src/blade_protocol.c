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
	ks_hash_t *providers;
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
		if (bp->providers) ks_hash_destroy(&bp->providers);
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

	ks_hash_create(&bp->providers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bp->pool);
	ks_assert(bp->providers);

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

KS_DECLARE(ks_hash_t *) blade_protocol_providers_get(blade_protocol_t *bp)
{
	ks_assert(bp);

	return bp->providers;

}

KS_DECLARE(ks_status_t) blade_protocol_providers_add(blade_protocol_t *bp, const char *nodeid)
{
	char *key = NULL;

	ks_assert(bp);
	ks_assert(nodeid);

	key = ks_pstrdup(bp->pool, nodeid);
	ks_hash_insert(bp->providers, (void *)key, (void *)KS_TRUE);

	return KS_STATUS_SUCCESS;

}

KS_DECLARE(ks_status_t) blade_protocol_providers_remove(blade_protocol_t *bp, const char *nodeid)
{
	ks_assert(bp);
	ks_assert(nodeid);

	ks_hash_remove(bp->providers, (void *)nodeid);

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
