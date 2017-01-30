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

struct blade_message_s {
	ks_pool_t *pool;
	blade_handle_t *handle;

	void *data;
	ks_size_t data_length;
	ks_size_t data_size;
};


KS_DECLARE(ks_status_t) blade_message_destroy(blade_message_t **bmP)
{
	blade_message_t *bm = NULL;

	ks_assert(bmP);

	bm = *bmP;
	*bmP = NULL;

	ks_assert(bm);

	if (bm->data) ks_pool_free(bm->pool, &bm->data);
	
	ks_pool_free(bm->pool, &bm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_message_create(blade_message_t **bmP, ks_pool_t *pool, blade_handle_t *handle)
{
	blade_message_t *bm = NULL;

	ks_assert(bmP);
	ks_assert(pool);
	ks_assert(handle);

	bm = ks_pool_alloc(pool, sizeof(*bm));
	bm->pool = pool;
	bm->handle = handle;
	*bmP = bm;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_message_discard(blade_message_t **bm)
{
	ks_assert(bm);
	ks_assert(*bm);

	return blade_handle_message_discard((*bm)->handle, bm);
}

KS_DECLARE(ks_status_t) blade_message_set(blade_message_t *bm, void *data, ks_size_t data_length)
{
	ks_assert(bm);
	ks_assert(data);
	ks_assert(data_length > 0);

	// @todo fail on a max message size?

	if (data_length > bm->data_size) {
		// @todo talk to tony about adding flags to ks_pool_resize_ex to prevent the memcpy, don't need to copy old memory here
		// otherwise switch to a new allocation instead of resizing
		bm->data = ks_pool_resize(bm->pool, bm->data, data_length);
		ks_assert(bm->data);
		bm->data_size = data_length;
	}
	memcpy(bm->data, data, data_length);
	bm->data_length = data_length;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_message_get(blade_message_t *bm, void **data, ks_size_t *data_length)
{
	ks_assert(bm);

	*data = bm->data;
	*data_length = bm->data_length;

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
