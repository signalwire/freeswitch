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

struct blade_transport_s {
	blade_handle_t *handle;

	const char *name;
	void *data;
	blade_transport_callbacks_t *callbacks;
};


static void blade_transport_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_transport_t *bt = (blade_transport_t *)ptr;

	//ks_assert(bt);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_transport_create(blade_transport_t **btP, blade_handle_t *bh, ks_pool_t *pool, const char *name, void *data, blade_transport_callbacks_t *callbacks)
{
	blade_transport_t *bt = NULL;

	ks_assert(btP);
	ks_assert(bh);
	ks_assert(pool);
	ks_assert(name);
	ks_assert(callbacks);

	bt = ks_pool_alloc(pool, sizeof(blade_transport_t));
	bt->handle = bh;
	bt->name = ks_pstrdup(pool, name);
	bt->data = data;
	bt->callbacks = callbacks;

	ks_pool_set_cleanup(bt, NULL, blade_transport_cleanup);

	ks_log(KS_LOG_DEBUG, "Created transport %s\n", name);

	*btP = bt;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_transport_destroy(blade_transport_t **btP)
{
	blade_transport_t *bt = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(btP);
	ks_assert(*btP);

	bt = *btP;
	*btP = NULL;

	pool = ks_pool_get(bt);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_transport_handle_get(blade_transport_t *bt)
{
	ks_assert(bt);
	return bt->handle;
}

KS_DECLARE(const char *) blade_transport_name_get(blade_transport_t *bt)
{
	ks_assert(bt);
	return bt->name;
}

KS_DECLARE(void *) blade_transport_data_get(blade_transport_t *bt)
{
	ks_assert(bt);
	return bt->data;
}

KS_DECLARE(blade_transport_callbacks_t *) blade_transport_callbacks_get(blade_transport_t *bt)
{
	ks_assert(bt);
	return bt->callbacks;
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
