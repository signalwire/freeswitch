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

struct blade_identity_s {
	ks_pool_t *pool;

	const char *uri;
	// @todo breakdown of uri into constituent parts
};


KS_DECLARE(ks_status_t) blade_identity_create(blade_identity_t **biP, ks_pool_t *pool)
{
	blade_identity_t *bi = NULL;

	ks_assert(biP);
	ks_assert(pool);

	bi = ks_pool_alloc(pool, sizeof(blade_identity_t));
	bi->pool = pool;
	*biP = bi;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_identity_destroy(blade_identity_t **biP)
{
	blade_identity_t *bi = NULL;
	
	ks_assert(biP);
	ks_assert(*biP);

	bi = *biP;

	ks_pool_free(bi->pool, biP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_identity_parse(blade_identity_t *bi, const char *uri)
{
	ks_assert(bi);
	ks_assert(uri);

	if (bi->uri) ks_pool_free(bi->pool, &bi->uri);
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_identity_uri(blade_identity_t *bi, const char **uri)
{
	ks_assert(bi);
	ks_assert(uri);

	*uri = bi->uri;
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
