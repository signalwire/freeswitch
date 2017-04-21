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

struct blade_module_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	const char *id;

	void *module_data;
	blade_module_callbacks_t *module_callbacks;
};

static void blade_module_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_module_t *bm = (blade_module_t *)ptr;

	//ks_assert(bm);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_module_create(blade_module_t **bmP, blade_handle_t *bh, ks_pool_t *pool, void *module_data, blade_module_callbacks_t *module_callbacks)
{
	blade_module_t *bm = NULL;
	uuid_t uuid;

	ks_assert(bmP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(module_data);
	ks_assert(module_callbacks);

	ks_uuid(&uuid);

	bm = ks_pool_alloc(pool, sizeof(blade_module_t));
	bm->handle = bh;
	bm->pool = pool;
	bm->id = ks_uuid_str(pool, &uuid);
	bm->module_data = module_data;
	bm->module_callbacks = module_callbacks;

	ks_pool_set_cleanup(pool, bm, NULL, blade_module_cleanup);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*bmP = bm;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_destroy(blade_module_t **bmP)
{
	blade_module_t *bm = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bmP);
	ks_assert(*bmP);

	bm = *bmP;

	pool = bm->pool;
	//ks_pool_free(bm->pool, bmP);
	ks_pool_close(&pool);

	*bmP = NULL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_module_handle_get(blade_module_t *bm)
{
	ks_assert(bm);

	return bm->handle;
}

KS_DECLARE(ks_pool_t *) blade_module_pool_get(blade_module_t *bm)
{
	ks_assert(bm);

	return bm->pool;
}

KS_DECLARE(const char *) blade_module_id_get(blade_module_t *bm)
{
	ks_assert(bm);

	return bm->id;
}

KS_DECLARE(void *) blade_module_data_get(blade_module_t *bm)
{
	ks_assert(bm);

	return bm->module_data;
}

KS_DECLARE(blade_module_callbacks_t *) blade_module_callbacks_get(blade_module_t *bm)
{
	ks_assert(bm);

	return bm->module_callbacks;
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
