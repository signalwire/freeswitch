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
#include "private/switch_core_pvt.h"
#include <sqlite3.h>
#include "../../../libs/sqlite/src/hash.h"


struct switch_hash {
	Hash table;
};


SWITCH_DECLARE(switch_status_t) switch_core_hash_init(switch_hash_t ** hash, switch_memory_pool_t *pool)
{
	switch_hash_t *newhash;
	
	newhash = switch_core_alloc(pool, sizeof(*newhash));
	switch_assert(newhash);

	sqlite3HashInit(&newhash->table, SQLITE_HASH_STRING, 1);
	*hash = newhash;
	
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_core_hash_destroy(switch_hash_t **hash)
{
	switch_assert(hash != NULL && *hash != NULL);
	sqlite3HashClear(&(*hash)->table);
	*hash = NULL;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert(switch_hash_t * hash, const char *key, const void *data)
{
	sqlite3HashInsert(&hash->table, key, (int)strlen(key)+1, (void *)data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_locked(switch_hash_t * hash, const char *key, const void *data, switch_mutex_t *mutex)
{
	if (mutex) {
        switch_mutex_lock(mutex);
    }

	sqlite3HashInsert(&hash->table, key, (int)strlen(key)+1, (void *)data);

	if (mutex) {
        switch_mutex_unlock(mutex);
    }	

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete(switch_hash_t * hash, const char *key)
{
	sqlite3HashInsert(&hash->table, key, (int)strlen(key)+1, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete_locked(switch_hash_t * hash, const char *key, switch_mutex_t *mutex)
{
	if (mutex) {
        switch_mutex_lock(mutex);
    }
	
	sqlite3HashInsert(&hash->table, key, (int)strlen(key)+1, NULL);
	
	if (mutex) {
        switch_mutex_unlock(mutex);
    }

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash_t * hash, const char *key)
{
	return sqlite3HashFind(&hash->table, key, (int)strlen(key)+1);
}

SWITCH_DECLARE(void *) switch_core_hash_find_locked(switch_hash_t * hash, const char *key, switch_mutex_t *mutex)
{
	void *val;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	val = sqlite3HashFind(&hash->table, key, (int)strlen(key)+1);
	
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return val;
}


SWITCH_DECLARE(switch_hash_index_t *) switch_hash_first(char *depricate_me, switch_hash_t *hash)
{
	return (switch_hash_index_t *) sqliteHashFirst(&hash->table);
}


SWITCH_DECLARE(switch_hash_index_t *) switch_hash_next(switch_hash_index_t *hi)
{
	return (switch_hash_index_t *) sqliteHashNext((HashElem *) hi);
}

SWITCH_DECLARE(void) switch_hash_this(switch_hash_index_t *hi, const void **key, switch_ssize_t *klen, void **val)
{
	if (key) {
		*key = sqliteHashKey((HashElem *) hi);
		if (klen) {
			*klen = strlen((char *) *key) + 1;
		}
	}
	if (val) {
		*val = sqliteHashData((HashElem *) hi);
	}
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
