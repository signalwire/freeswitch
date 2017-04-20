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

struct blade_method_s {
	blade_handle_t *handle;
	ks_pool_t *pool;

	blade_space_t *space;
	const char *name;

	blade_request_callback_t callback;
	// @todo more fun descriptive information about the call for remote registrations
};

static void blade_method_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_method_t *bm = (blade_method_t *)ptr;

	ks_assert(bm);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free(bm->pool, &bm->name);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_method_create(blade_method_t **bmP, blade_space_t *bs, const char *name, blade_request_callback_t callback)
{
	blade_handle_t *bh = NULL;
	blade_method_t *bm = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bmP);
	ks_assert(bs);
	ks_assert(name);

	bh = blade_space_handle_get(bs);
	ks_assert(bh);

	pool = blade_space_pool_get(bs);
	ks_assert(pool);

	bm = ks_pool_alloc(pool, sizeof(blade_method_t));
	bm->handle = bh;
	bm->pool = pool;
	bm->space = bs;
	bm->name = ks_pstrdup(pool, name);
	bm->callback = callback;

	ks_assert(ks_pool_set_cleanup(pool, bm, NULL, blade_method_cleanup) == KS_STATUS_SUCCESS);

	*bmP = bm;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_method_name_get(blade_method_t *bm)
{
	ks_assert(bm);

	return bm->name;
}

KS_DECLARE(blade_request_callback_t) blade_method_callback_get(blade_method_t *bm)
{
	ks_assert(bm);

	return bm->callback;
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
