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
 * switch_curl.c
 *
 */

#include <switch.h>

#if defined(CORE_USE_CURL) && defined(HAVE_OPENSSL)
#include <openssl/crypto.h>
#include <curl/curl.h>

static switch_mutex_t **ssl_mutexes;

static void switch_curl_ssl_lock_callback(int mode, int type, char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		switch_mutex_lock(ssl_mutexes[type]);
	}
	else {
		switch_mutex_unlock(ssl_mutexes[type]);
	}
}

static unsigned long switch_curl_ssl_thread_id(void)
{
	return (unsigned long) switch_thread_self();
}

static void switch_curl_init_ssl_locks(switch_memory_pool_t *pool)
{
	int i, num = CRYPTO_num_locks();

	ssl_mutexes = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(switch_mutex_t*));
	switch_assert(ssl_mutexes != NULL);

	for (i = 0; i < num; i++) {
		switch_mutex_init(&(ssl_mutexes[i]), SWITCH_MUTEX_NESTED, pool);
		switch_assert(ssl_mutexes[i] != NULL);
	}

	CRYPTO_set_id_callback(switch_curl_ssl_thread_id);
	CRYPTO_set_locking_callback((void (*)(int, int, const char*, int))switch_curl_ssl_lock_callback);
}

static void switch_curl_destroy_ssl_locks()
{
	int i;

	CRYPTO_set_locking_callback(NULL);
	for (i = 0; i < CRYPTO_num_locks(); i++) {
		switch_mutex_destroy(ssl_mutexes[i]);
	}

	OPENSSL_free(ssl_mutexes);
}


SWITCH_DECLARE(void) switch_curl_init(switch_memory_pool_t *pool)
{
	curl_global_init(CURL_GLOBAL_ALL);
	switch_curl_init_ssl_locks(pool);
}

SWITCH_DECLARE(void) switch_curl_destroy()
{
	switch_curl_destroy_ssl_locks();
	curl_global_cleanup();
}

#else
SWITCH_DECLARE(void) switch_curl_init(switch_memory_pool_t *pool)
{
	return;
}

SWITCH_DECLARE(void) switch_curl_destroy()
{
	return;
}
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

