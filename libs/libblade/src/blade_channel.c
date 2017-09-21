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

struct blade_channel_s {
	const char *name;
	blade_channel_flags_t flags;
	ks_rwl_t *lock;
	ks_hash_t *authorizations;
};


static void blade_channel_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_channel_t *bc = (blade_channel_t *)ptr;

	ks_assert(bc);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (bc->name) ks_pool_free(&bc->name);
		if (bc->lock) ks_rwl_destroy(&bc->lock);
		if (bc->authorizations) ks_hash_destroy(&bc->authorizations);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_channel_create(blade_channel_t **bcP, ks_pool_t *pool, const char *name, blade_channel_flags_t flags)
{
	blade_channel_t *bc = NULL;

	ks_assert(bcP);
	ks_assert(pool);
	ks_assert(name);

	bc = ks_pool_alloc(pool, sizeof(blade_channel_t));
	bc->name = ks_pstrdup(pool, name);
	bc->flags = flags;

	ks_rwl_create(&bc->lock, pool);
	ks_assert(bc->lock);

	ks_hash_create(&bc->authorizations, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bc->authorizations);

	ks_pool_set_cleanup(bc, NULL, blade_channel_cleanup);

	*bcP = bc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_channel_destroy(blade_channel_t **bcP)
{
	ks_assert(bcP);
	ks_assert(*bcP);

	ks_pool_free(bcP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_channel_name_get(blade_channel_t *bc)
{
	ks_assert(bc);
	return bc->name;
}

KS_DECLARE(blade_channel_flags_t) blade_channel_flags_get(blade_channel_t *bc)
{
	ks_assert(bc);
	return bc->flags;
}

KS_DECLARE(ks_status_t) blade_channel_read_lock(blade_channel_t *bc)
{
	ks_assert(bc);
	return ks_rwl_read_lock(bc->lock);
}

KS_DECLARE(ks_status_t) blade_channel_read_unlock(blade_channel_t *bc)
{
	ks_assert(bc);
	return ks_rwl_read_unlock(bc->lock);
}

KS_DECLARE(ks_status_t) blade_channel_write_lock(blade_channel_t *bc)
{
	ks_assert(bc);
	return ks_rwl_write_lock(bc->lock);
}

KS_DECLARE(ks_status_t) blade_channel_write_unlock(blade_channel_t *bc)
{
	ks_assert(bc);
	return ks_rwl_write_unlock(bc->lock);
}

KS_DECLARE(ks_bool_t) blade_channel_authorization_verify(blade_channel_t *bc, const char *target)
{
	ks_bool_t authorized = KS_FALSE;

	ks_assert(bc);
	ks_assert(target);

	authorized = (ks_bool_t)(uintptr_t)ks_hash_search(bc->authorizations, (void *)target, KS_READLOCKED);
	ks_hash_read_unlock(bc->authorizations);

	return authorized;
}

KS_DECLARE(ks_status_t) blade_channel_authorization_add(blade_channel_t *bc, const char *target)
{
	ks_assert(bc);
	ks_assert(target);

	ks_hash_insert(bc->authorizations, (void *)ks_pstrdup(ks_pool_get(bc), target), (void *)KS_TRUE);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_channel_authorization_remove(blade_channel_t *bc, const char *target)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bc);
	ks_assert(target);

	if (ks_hash_remove(bc->authorizations, (void *)target)) {
		ret = KS_TRUE;
	}

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
