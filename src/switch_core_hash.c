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

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_auto_free(switch_hash_t *hash, const char *key, const void *data)
{
	char *dkey = strdup(key);

	if (switch_hashtable_insert_destructor(hash, dkey, (void *)data, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE | HASHTABLE_DUP_CHECK, NULL)) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_safe_free(dkey);

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_destructor(switch_hash_t *hash, const char *key, const void *data, hashtable_destructor_t destructor)
{
	char *dkey = strdup(key);

	if (switch_hashtable_insert_destructor(hash, dkey, (void *)data, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_DUP_CHECK, destructor)) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_safe_free(dkey);

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_locked(switch_hash_t *hash, const char *key, const void *data, switch_mutex_t *mutex)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	status = switch_core_hash_insert(hash, key, data);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_wrlock(switch_hash_t *hash, const char *key, const void *data, switch_thread_rwlock_t *rwlock)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (rwlock) {
		switch_thread_rwlock_wrlock(rwlock);
	}

	status = switch_core_hash_insert(hash, key, data);

	if (rwlock) {
		switch_thread_rwlock_unlock(rwlock);
	}

	return status;
}

SWITCH_DECLARE(void *) switch_core_hash_delete(switch_hash_t *hash, const char *key)
{
	return switch_hashtable_remove(hash, (void *)key);
}

SWITCH_DECLARE(void *) switch_core_hash_delete_locked(switch_hash_t *hash, const char *key, switch_mutex_t *mutex)
{
	void *ret = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	ret = switch_core_hash_delete(hash, key);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

SWITCH_DECLARE(void *) switch_core_hash_delete_wrlock(switch_hash_t *hash, const char *key, switch_thread_rwlock_t *rwlock)
{
	void *ret = NULL;

	if (rwlock) {
		switch_thread_rwlock_wrlock(rwlock);
	}

	ret = switch_core_hash_delete(hash, key);

	if (rwlock) {
		switch_thread_rwlock_unlock(rwlock);
	}

	return ret;
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
		if (switch_core_hash_delete(hash, header->value)) {
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
		free(hi);
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

SWITCH_DECLARE(void) switch_core_hash_this_val(switch_hash_index_t *hi, void *val)
{
	switch_hashtable_this_val(hi, val);
}


SWITCH_DECLARE(switch_status_t) switch_core_inthash_init(switch_inthash_t **hash)
{
	return switch_create_hashtable(hash, 16, switch_hash_default_int, switch_hash_equalkeys_int);
}

SWITCH_DECLARE(switch_status_t) switch_core_inthash_destroy(switch_inthash_t **hash)
{
	switch_hashtable_destroy(hash);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_inthash_insert(switch_inthash_t *hash, uint32_t key, const void *data)
{
	uint32_t *k = NULL;
	int r = 0;

	switch_zmalloc(k, sizeof(*k));
	*k = key;
	r = switch_hashtable_insert_destructor(hash, k, (void *)data, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_DUP_CHECK, NULL);

	return r ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(void *) switch_core_inthash_delete(switch_inthash_t *hash, uint32_t key)
{
	return switch_hashtable_remove(hash, (void *)&key);
}

SWITCH_DECLARE(void *) switch_core_inthash_find(switch_inthash_t *hash, uint32_t key)
{
	return switch_hashtable_search(hash, (void *)&key);
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
