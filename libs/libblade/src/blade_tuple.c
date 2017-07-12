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

struct blade_tuple_s {
	ks_pool_t *pool;

	void *value1;
	void *value2;
};


static void blade_tuple_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_tuple_t *bt = (blade_tuple_t *)ptr;

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

KS_DECLARE(ks_status_t) blade_tuple_create(blade_tuple_t **btP, ks_pool_t *pool, void *value1, void *value2)
{
	blade_tuple_t *bt = NULL;

	ks_assert(btP);
	ks_assert(pool);

	bt = ks_pool_alloc(pool, sizeof(blade_tuple_t));
	bt->pool = pool;
	bt->value1 = value1;
	bt->value2 = value2;

	ks_pool_set_cleanup(pool, bt, NULL, blade_tuple_cleanup);

	*btP = bt;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_tuple_destroy(blade_tuple_t **btP)
{
	ks_assert(btP);
	ks_assert(*btP);

	ks_pool_free((*btP)->pool, btP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void *) blade_tuple_value1_get(blade_tuple_t *bt)
{
	ks_assert(bt);
	return bt->value1;
}

KS_DECLARE(void *) blade_tuple_value2_get(blade_tuple_t *bt)
{
	ks_assert(bt);
	return bt->value2;
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
