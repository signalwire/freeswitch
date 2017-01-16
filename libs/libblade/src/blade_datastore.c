/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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


typedef enum {
	BDS_NONE = 0,
	BDS_MYPOOL = (1 << 0),
} bdspvt_flag_t;

struct blade_datastore_s {
	bdspvt_flag_t flags;
	ks_pool_t *pool;
};


KS_DECLARE(ks_status_t) blade_datastore_destroy(blade_datastore_t **bdsP)
{
	blade_datastore_t *bds = NULL;
	bdspvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bdsP);

	bds = *bdsP;
	*bdsP = NULL;

	ks_assert(bds);

	flags = bds->flags;
	pool = bds->pool;

	ks_pool_free(bds->pool, &bds);

	if (pool && (flags & BDS_MYPOOL)) ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_datastore_create(blade_datastore_t **bdsP, ks_pool_t *pool)
{
	bdspvt_flag_t newflags = BDS_NONE;
	blade_datastore_t *bds = NULL;

	if (!pool) {
		newflags |= BDS_MYPOOL;
		ks_pool_open(&pool);
		ks_assert(pool);
	}

	bds = ks_pool_alloc(pool, sizeof(*bds));
	bds->flags = newflags;
	bds->pool = pool;
	*bdsP = bds;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_datastore_pulse(blade_datastore_t *bds, int32_t timeout)
{
	ks_assert(bds);
	ks_assert(timeout >= 0);
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
