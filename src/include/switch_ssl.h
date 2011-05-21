/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * switch_ssl.h
 *
 */

#ifndef __SWITCH_SSL_H
#define __SWITCH_SSL_H

#if defined(HAVE_OPENSSL)
#include <openssl/crypto.h>

static switch_mutex_t **ssl_mutexes;
static switch_memory_pool_t *ssl_pool = NULL;


static inline void switch_ssl_ssl_lock_callback(int mode, int type, char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		switch_mutex_lock(ssl_mutexes[type]);
	}
	else {
		switch_mutex_unlock(ssl_mutexes[type]);
	}
}

static inline unsigned long switch_ssl_ssl_thread_id(void)
{
	return (unsigned long) switch_thread_self();
}

static inline void switch_ssl_init_ssl_locks(void)
{

	int ssl_count = switch_core_ssl_count(NULL);
	int i, num;

	if (ssl_count == 0) {
		num = CRYPTO_num_locks();
		
		ssl_mutexes = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(switch_mutex_t*));
		switch_assert(ssl_mutexes != NULL);

		switch_core_new_memory_pool(&ssl_pool);

		for (i = 0; i < num; i++) {
			switch_mutex_init(&(ssl_mutexes[i]), SWITCH_MUTEX_NESTED, ssl_pool);
			switch_assert(ssl_mutexes[i] != NULL);
		}

		CRYPTO_set_id_callback(switch_ssl_ssl_thread_id);
		CRYPTO_set_locking_callback((void (*)(int, int, const char*, int))switch_ssl_ssl_lock_callback);
	}

	ssl_count++;
	switch_core_ssl_count(&ssl_count);
}

static inline void switch_ssl_destroy_ssl_locks()
{
	int i;
	int ssl_count = switch_core_ssl_count(NULL);

	ssl_count--;
	
	if (ssl_count == 0) {
		CRYPTO_set_locking_callback(NULL);
		for (i = 0; i < CRYPTO_num_locks(); i++) {
			switch_mutex_destroy(ssl_mutexes[i]);
		}

		OPENSSL_free(ssl_mutexes);
	}

	switch_core_ssl_count(&ssl_count);

}
#else
static inline void switch_ssl_init_ssl_locks(void) { return; }
static inline void switch_ssl_destroy_ssl_locks(void) { return; }
#endif

#endif
