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

struct blade_realm_s {
	const char *name;
	ks_rwl_t *lock;
	ks_hash_t *protocols;
};


static void blade_realm_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_realm_t *br = (blade_realm_t *)ptr;

	ks_assert(br);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (br->name) ks_pool_free(&br->name);
		if (br->lock) ks_rwl_destroy(&br->lock);
		if (br->protocols) ks_hash_destroy(&br->protocols);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_realm_create(blade_realm_t **brP, ks_pool_t *pool, const char *name)
{
	blade_realm_t *br = NULL;

	ks_assert(brP);
	ks_assert(pool);
	ks_assert(name);

	br = ks_pool_alloc(pool, sizeof(blade_realm_t));
	br->name = ks_pstrdup(pool, name);

	ks_rwl_create(&br->lock, pool);
	ks_assert(br->lock);

	ks_hash_create(&br->protocols, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(br->protocols);

	ks_pool_set_cleanup(br, NULL, blade_realm_cleanup);

	*brP = br;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_realm_destroy(blade_realm_t **brP)
{
	ks_assert(brP);
	ks_assert(*brP);

	ks_pool_free(brP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_realm_name_get(blade_realm_t *br)
{
	ks_assert(br);
	return br->name;
}

KS_DECLARE(ks_status_t) blade_realm_read_lock(blade_realm_t *br)
{
	ks_assert(br);
	return ks_rwl_read_lock(br->lock);
}

KS_DECLARE(ks_status_t) blade_realm_read_unlock(blade_realm_t *br)
{
	ks_assert(br);
	return ks_rwl_read_unlock(br->lock);
}

KS_DECLARE(ks_status_t) blade_realm_write_lock(blade_realm_t *br)
{
	ks_assert(br);
	return ks_rwl_write_lock(br->lock);
}

KS_DECLARE(ks_status_t) blade_realm_write_unlock(blade_realm_t *br)
{
	ks_assert(br);
	return ks_rwl_write_unlock(br->lock);
}

KS_DECLARE(ks_hash_iterator_t *) blade_realm_protocols_iterator(blade_realm_t *br, ks_locked_t locked)
{
	ks_assert(br);
	return ks_hash_first(br->protocols, locked);
}

KS_DECLARE(blade_protocol_t *) blade_realm_protocol_lookup(blade_realm_t *br, const char *protocol, ks_bool_t writelocked)
{
	blade_protocol_t *bp = NULL;

	ks_assert(br);
	ks_assert(protocol);

	bp = (blade_protocol_t *)ks_hash_search(br->protocols, (void *)protocol, KS_READLOCKED);
	if (bp) {
		if (writelocked) blade_protocol_write_lock(bp);
		else blade_protocol_read_lock(bp);
	}
	ks_hash_read_unlock(br->protocols);

	return bp;
}

KS_DECLARE(ks_status_t) blade_realm_protocol_add(blade_realm_t *br, blade_protocol_t *protocol)
{
	ks_assert(br);
	ks_assert(protocol);

	ks_hash_insert(br->protocols, (void *)ks_pstrdup(ks_pool_get(br), blade_protocol_name_get(protocol)), (void *)protocol);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_realm_protocol_remove(blade_realm_t *br, const char *protocol)
{
	ks_assert(br);
	ks_assert(protocol);

	ks_hash_remove(br->protocols, (void *)protocol);

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
