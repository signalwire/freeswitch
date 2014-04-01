/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_hash.c -- Main Core Library (hash functions)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"
#include "private/switch_hashtable_private.h"

SWITCH_DECLARE(switch_status_t) switch_core_hash_init_case(switch_hash_t **hash, switch_bool_t case_sensitive)
{
	if (case_sensitive) {
		return switch_create_hashtable(hash, 16, switch_hash_default, switch_hash_equalkeys);
	} else {
		return switch_create_hashtable(hash, 16, switch_hash_default_ci, switch_hash_equalkeys_ci);
	}
}


SWITCH_DECLARE(switch_status_t) switch_core_hash_destroy(switch_hash_t **hash)
{
	switch_assert(hash != NULL && *hash != NULL);

	switch_hashtable_destroy(hash);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert(switch_hash_t *hash, const char *key, const void *data)
{
	switch_hashtable_insert(hash, strdup(key), (void *)data, HASHTABLE_FLAG_FREE_KEY);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_locked(switch_hash_t *hash, const char *key, const void *data, switch_mutex_t *mutex)
{
	if (mutex) {
		switch_mutex_lock(mutex);
	}

	switch_core_hash_insert(hash, key, data);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_wrlock(switch_hash_t *hash, const char *key, const void *data, switch_thread_rwlock_t *rwlock)
{
	if (rwlock) {
		switch_thread_rwlock_wrlock(rwlock);
	}

	switch_core_hash_insert(hash, key, data);

	if (rwlock) {
		switch_thread_rwlock_unlock(rwlock);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete(switch_hash_t *hash, const char *key)
{
	switch_hashtable_remove(hash, (void *)key);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete_locked(switch_hash_t *hash, const char *key, switch_mutex_t *mutex)
{
	if (mutex) {
		switch_mutex_lock(mutex);
	}

	switch_core_hash_delete(hash, key);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete_wrlock(switch_hash_t *hash, const char *key, switch_thread_rwlock_t *rwlock)
{
	if (rwlock) {
		switch_thread_rwlock_wrlock(rwlock);
	}

	switch_core_hash_delete(hash, key);

	if (rwlock) {
		switch_thread_rwlock_unlock(rwlock);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete_multi(switch_hash_t *hash, switch_hash_delete_callback_t callback, void *pData) {

	switch_hash_index_t *hi = NULL;
	switch_event_t *event = NULL;
	switch_event_header_t *header = NULL;
	switch_status_t status = SWITCH_STATUS_GENERR;
	
	switch_event_create_subclass(&event, SWITCH_EVENT_CLONE, NULL);
	switch_assert(event);
	
	/* iterate through the hash, call callback, if callback returns NULL or true, put the key on the list (event)
	   When done, iterate through the list deleting hash entries
	 */
	
	for (hi = switch_core_hash_first(hash); hi; hi = switch_core_hash_next(&hi)) {
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		if (!callback || callback(key, val, pData)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
		}
	}
	
	/* now delete them */
	for (header = event->headers; header; header = header->next) {
		if (switch_core_hash_delete(hash, header->value) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	switch_event_destroy(&event);
	
	return status;
}


SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash_t *hash, const char *key)
{
	return switch_hashtable_search(hash, (void *)key);
}

SWITCH_DECLARE(void *) switch_core_hash_find_locked(switch_hash_t *hash, const char *key, switch_mutex_t *mutex)
{
	void *val;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	val = switch_core_hash_find(hash, key);


	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return val;
}

SWITCH_DECLARE(void *) switch_core_hash_find_rdlock(switch_hash_t *hash, const char *key, switch_thread_rwlock_t *rwlock)
{
	void *val;

	if (rwlock) {
		switch_thread_rwlock_rdlock(rwlock);
	}

	val = switch_core_hash_find(hash, key);

	if (rwlock) {
		switch_thread_rwlock_unlock(rwlock);
	}

	return val;
}

SWITCH_DECLARE(switch_bool_t) switch_core_hash_empty(switch_hash_t *hash)
{
	switch_hash_index_t *hi = switch_core_hash_first(hash);

	if (hi) {
		switch_safe_free(hi);
		return SWITCH_FALSE;
	}

	return SWITCH_TRUE;

}

SWITCH_DECLARE(switch_hash_index_t *) switch_core_hash_first_iter(switch_hash_t *hash, switch_hash_index_t *hi)
{
	return switch_hashtable_first_iter(hash, hi);
}

SWITCH_DECLARE(switch_hash_index_t *) switch_core_hash_next(switch_hash_index_t **hi)
{
	return switch_hashtable_next(hi);
}

SWITCH_DECLARE(void) switch_core_hash_this(switch_hash_index_t *hi, const void **key, switch_ssize_t *klen, void **val)
{
	switch_hashtable_this(hi, key, klen, val);
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
