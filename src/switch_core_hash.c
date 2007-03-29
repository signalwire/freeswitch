/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_hash.c -- Main Core Library (hash functions)
 *
 */
#include <switch.h>
#include "private/switch_core.h"

SWITCH_DECLARE(switch_status_t) switch_core_hash_init(switch_hash_t **hash, switch_memory_pool_t *pool)
{
	assert(pool != NULL);

	if ((*hash = apr_hash_make(pool)) != 0) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_destroy(switch_hash_t *hash)
{
	assert(hash != NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_dup(switch_hash_t *hash, const char *key, const void *data)
{
	apr_hash_set(hash, switch_core_strdup(apr_hash_pool_get(hash), key), APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert(switch_hash_t *hash, const char *key, const void *data)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete(switch_hash_t *hash, const char *key)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash_t *hash, const char *key)
{
	return apr_hash_get(hash, key, APR_HASH_KEY_STRING);
}
