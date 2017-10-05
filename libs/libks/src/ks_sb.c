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

#include "ks.h"
#include "ks_sb.h"

struct ks_sb_s {
	ks_bool_t pool_owner;
	char *data;
	ks_size_t size;
	ks_size_t used;
};

static void ks_sb_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	ks_sb_t *sb = (ks_sb_t *)ptr;

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (!sb->pool_owner && sb->data) ks_pool_free(&sb->data);
		break;
	case KS_MPCL_DESTROY:
		break;
	}

}

KS_DECLARE(ks_status_t) ks_sb_create(ks_sb_t **sbP, ks_pool_t *pool, ks_size_t preallocated)
{
	ks_sb_t *sb = NULL;
	ks_bool_t pool_owner = KS_FALSE;

	ks_assert(sbP);

	if ((pool_owner = !pool)) ks_pool_open(&pool);
	if (preallocated == 0) preallocated = KS_PRINT_BUF_SIZE * 2;

	sb = ks_pool_alloc(pool, sizeof(ks_sb_t));
	sb->pool_owner = pool_owner;
	sb->data = ks_pool_alloc(pool, preallocated);
	sb->size = preallocated;
	sb->used = 1;

	ks_pool_set_cleanup(sb, NULL, ks_sb_cleanup);

	*sbP = sb;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_sb_destroy(ks_sb_t **sbP)
{
	ks_sb_t *sb = NULL;

	ks_assert(sbP);
	ks_assert(*sbP);

	sb = *sbP;

	if (sb->pool_owner) {
		ks_pool_t *pool = ks_pool_get(sb);
		ks_pool_close(&pool);
	} else ks_pool_free(sbP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) ks_sb_cstr(ks_sb_t *sb)
{
	ks_assert(sb);
	return sb->data;
}

KS_DECLARE(ks_size_t) ks_sb_length(ks_sb_t *sb)
{
	ks_assert(sb);
	return sb->used - 1;
}

KS_DECLARE(ks_status_t) ks_sb_accommodate(ks_sb_t *sb, ks_size_t len)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(sb);

	if (len == 0) goto done;

	if ((sb->used + len) > sb->size) {
		ks_size_t needed = (sb->used + len) - sb->size;
		if (needed < KS_PRINT_BUF_SIZE) needed = KS_PRINT_BUF_SIZE;
		sb->size += needed;
		if (!sb->data) sb->data = ks_pool_alloc(ks_pool_get(sb), sb->size);
		else {
			sb->data = ks_pool_resize(sb->data, sb->size);
			if (!sb->data) ret = KS_STATUS_FAIL;
		}
	}

done:
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_sb_append(ks_sb_t *sb, const char *str)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(sb);

	if (str) ret = ks_sb_append_ex(sb, str, strlen(str));

	return ret;
}

KS_DECLARE(ks_status_t) ks_sb_append_ex(ks_sb_t *sb, const char *str, ks_size_t len)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(sb);

	if (!str || len == 0) goto done;

	if (ks_sb_accommodate(sb, len) != KS_STATUS_SUCCESS) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	memcpy(sb->data + (sb->used - 1), str, len + 1);
	sb->used += len;

done:

	return ret;
}

KS_DECLARE(ks_status_t) ks_sb_printf(ks_sb_t *sb, const char *fmt, ...)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	va_list ap;
	ks_size_t used = 0;
	char *result = NULL;

	ks_assert(sb);
	ks_assert(fmt);

	used = sb->used - 1;

	if (ks_sb_accommodate(sb, KS_PRINT_BUF_SIZE) != KS_STATUS_SUCCESS) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	va_start(ap, fmt);
	result = ks_vsnprintfv(sb->data + used, (int)(sb->size - used), fmt, ap);
	va_end(ap);

	sb->used += strlen(result);

done:
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

