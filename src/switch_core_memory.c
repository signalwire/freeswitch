/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_core_memory.c -- Main Core Library (memory management)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

//#define DEBUG_ALLOC
//#define DEBUG_ALLOC2
//#define DESTROY_POOLS
//#define INSTANTLY_DESTROY_POOLS
//#define LOCK_MORE
//#define USE_MEM_LOCK
//#define SWITCH_POOL_RECYCLE
#ifndef SWITCH_POOL_RECYCLE
#define PER_POOL_LOCK 1
#endif

static struct {
#ifdef USE_MEM_LOCK
	switch_mutex_t *mem_lock;
#endif
	switch_queue_t *pool_queue;	/* 8 ball break */
	switch_queue_t *pool_recycle_queue;
	switch_memory_pool_t *memory_pool;
	int pool_thread_running;
} memory_manager;

SWITCH_DECLARE(switch_memory_pool_t *) switch_core_session_get_pool(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_assert(session->pool != NULL);
	return session->pool;
}

/* **ONLY** alloc things with this function that **WILL NOT** outlive
   the session itself or expect an earth shattering KABOOM!*/
SWITCH_DECLARE(void *) switch_core_perform_session_alloc(switch_core_session_t *session, switch_size_t memory, const char *file, const char *func,
														 int line)
{
	void *ptr = NULL;
	switch_assert(session != NULL);
	switch_assert(session->pool != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

#ifdef DEBUG_ALLOC
	if (memory > 500)
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p %p Session Allocate %s %d\n", 
						  (void *) session->pool, (void *) session, apr_pool_tag(session->pool, NULL), (int) memory);
#endif

	ptr = apr_palloc(session->pool, memory);
	switch_assert(ptr != NULL);

	memset(ptr, 0, memory);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return ptr;
}

/* **ONLY** alloc things with these functions that **WILL NOT** need
   to be freed *EVER* ie this is for *PERMANENT* memory allocation */

SWITCH_DECLARE(void *) switch_core_perform_permanent_alloc(switch_size_t memory, const char *file, const char *func, int line)
{
	void *ptr = NULL;
	switch_assert(memory_manager.memory_pool != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p Perm Allocate %s %d\n", 
					  (void *)memory_manager.memory_pool, apr_pool_tag(memory_manager.memory_pool, NULL), (int) memory);
#endif

	ptr = apr_palloc(memory_manager.memory_pool, memory);

	switch_assert(ptr != NULL);
	memset(ptr, 0, memory);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return ptr;
}

SWITCH_DECLARE(char *) switch_core_perform_permanent_strdup(const char *todup, const char *file, const char *func, int line)
{
	char *duped = NULL;
	switch_size_t len;
	switch_assert(memory_manager.memory_pool != NULL);

	if (!todup) {
		return NULL;
	}

	if (zstr(todup)) {
		return SWITCH_BLANK_STRING;
	}
#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

	len = strlen(todup) + 1;
	duped = apr_pstrmemdup(memory_manager.memory_pool, todup, len);
	switch_assert(duped != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p Perm Allocate %s %d\n", 
					  (void *) memory_manager.memory_pool, apr_pool_tag(memory_manager.memory_pool, NULL), (int) len);
#endif

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return duped;
}

SWITCH_DECLARE(char *) switch_core_session_sprintf(switch_core_session_t *session, const char *fmt, ...)
{
	va_list ap;
	char *result = NULL;

	va_start(ap, fmt);
	result = switch_core_vsprintf(session->pool, fmt, ap);
	va_end(ap);

	return result;
}

SWITCH_DECLARE(char *) switch_core_session_vsprintf(switch_core_session_t *session, const char *fmt, va_list ap)
{
	return switch_core_vsprintf(session->pool, fmt, ap);
}

SWITCH_DECLARE(char *) switch_core_vsprintf(switch_memory_pool_t *pool, const char *fmt, va_list ap)
{
	char *result = NULL;

	switch_assert(pool != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

	result = apr_pvsprintf(pool, fmt, ap);
	switch_assert(result != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return result;
}

SWITCH_DECLARE(char *) switch_core_sprintf(switch_memory_pool_t *pool, const char *fmt, ...)
{
	va_list ap;
	char *result;
	va_start(ap, fmt);
	result = switch_core_vsprintf(pool, fmt, ap);
	va_end(ap);

	return result;
}

SWITCH_DECLARE(char *) switch_core_perform_session_strdup(switch_core_session_t *session, const char *todup, const char *file, const char *func, int line)
{
	char *duped = NULL;
#ifdef DEBUG_ALLOC
	switch_size_t len;
#endif

	switch_assert(session != NULL);
	switch_assert(session->pool != NULL);

	if (!todup) {
		return NULL;
	}

	if (zstr(todup)) {
		return SWITCH_BLANK_STRING;
	}
#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif



#ifdef DEBUG_ALLOC
	len = strlen(todup);
	if (len > 500)
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p %p Sess Strdup Allocate %s %ld\n", 
						  (void *) session->pool, (void *)session, apr_pool_tag(session->pool, NULL), strlen(todup));
#endif

	duped = apr_pstrdup(session->pool, todup);
	switch_assert(duped != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return duped;
}

SWITCH_DECLARE(char *) switch_core_perform_strdup(switch_memory_pool_t *pool, const char *todup, const char *file, const char *func, int line)
{
	char *duped = NULL;
	switch_size_t len;
	switch_assert(pool != NULL);

	if (!todup) {
		return NULL;
	}

	if (zstr(todup)) {
		return SWITCH_BLANK_STRING;
	}
#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	if (len > 500)
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p Core Strdup Allocate %s %d\n", 
						  (void *) pool, apr_pool_tag(pool, NULL), (int)len);
#endif

	duped = apr_pstrmemdup(pool, todup, len);
	switch_assert(duped != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return duped;
}

SWITCH_DECLARE(void) switch_core_memory_pool_set_data(switch_memory_pool_t *pool, const char *key, void *data)
{
	apr_pool_userdata_set(data, key, NULL, pool);
}

SWITCH_DECLARE(void *) switch_core_memory_pool_get_data(switch_memory_pool_t *pool, const char *key)
{
	void *data = NULL;

	apr_pool_userdata_get(&data, key, pool);

	return data;
}

SWITCH_DECLARE(void) switch_core_memory_pool_tag(switch_memory_pool_t *pool, const char *tag)
{
	apr_pool_tag(pool, tag);
}

SWITCH_DECLARE(void) switch_pool_clear(switch_memory_pool_t *p)
{
#ifdef PER_POOL_LOCK
	apr_thread_mutex_t *my_mutex;
	apr_pool_mutex_set(p, NULL);
#endif

	apr_pool_clear(p);

#ifdef PER_POOL_LOCK

	if ((apr_thread_mutex_create(&my_mutex, APR_THREAD_MUTEX_NESTED, p)) != APR_SUCCESS) {
		abort();
	}

	apr_pool_mutex_set(p, my_mutex);

#endif

}



SWITCH_DECLARE(switch_status_t) switch_core_perform_new_memory_pool(switch_memory_pool_t **pool, const char *file, const char *func, int line)
{
	char *tmp;
#ifdef INSTANTLY_DESTROY_POOLS
	apr_pool_create(pool, NULL);
	switch_assert(*pool != NULL);
#else

#ifdef PER_POOL_LOCK
	apr_allocator_t *my_allocator = NULL;
	apr_thread_mutex_t *my_mutex;
#else
	void *pop = NULL;
#endif

#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
	switch_assert(pool != NULL);

#ifndef PER_POOL_LOCK
	if (switch_queue_trypop(memory_manager.pool_recycle_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		*pool = (switch_memory_pool_t *) pop;
	} else {
#endif

#ifdef PER_POOL_LOCK
		if ((apr_allocator_create(&my_allocator)) != APR_SUCCESS) {
			abort();
		}

		if ((apr_pool_create_ex(pool, NULL, NULL, my_allocator)) != APR_SUCCESS) {
			abort();
		}

		if ((apr_thread_mutex_create(&my_mutex, APR_THREAD_MUTEX_NESTED, *pool)) != APR_SUCCESS) {
			abort();
		}

		apr_allocator_mutex_set(my_allocator, my_mutex);
		apr_allocator_owner_set(my_allocator, *pool);

		apr_pool_mutex_set(*pool, my_mutex);

#else
		apr_pool_create(pool, NULL);
		switch_assert(*pool != NULL);
	}
#endif
#endif

	tmp = switch_core_sprintf(*pool, "%s:%d", file, line);
	apr_pool_tag(*pool, tmp);

#ifdef DEBUG_ALLOC2
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p New Pool %s\n", (void *) *pool, apr_pool_tag(*pool, NULL));
#endif


#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_perform_destroy_memory_pool(switch_memory_pool_t **pool, const char *file, const char *func, int line)
{
	switch_assert(pool != NULL);

#ifdef DEBUG_ALLOC2
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p Free Pool %s\n", (void *) *pool, apr_pool_tag(*pool, NULL));
#endif

#ifdef INSTANTLY_DESTROY_POOLS
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
	apr_pool_destroy(*pool);
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#else
	if ((memory_manager.pool_thread_running != 1) || (switch_queue_push(memory_manager.pool_queue, *pool) != SWITCH_STATUS_SUCCESS)) {
#ifdef USE_MEM_LOCK
		switch_mutex_lock(memory_manager.mem_lock);
#endif
		apr_pool_destroy(*pool);
#ifdef USE_MEM_LOCK
		switch_mutex_unlock(memory_manager.mem_lock);
#endif
	}
#endif

	*pool = NULL;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_core_perform_alloc(switch_memory_pool_t *pool, switch_size_t memory, const char *file, const char *func, int line)
{
	void *ptr = NULL;

	switch_assert(pool != NULL);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_lock(memory_manager.mem_lock);
#endif
#endif

#ifdef DEBUG_ALLOC
	if (memory > 500)
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CONSOLE, "%p Core Allocate %s %d\n", 
						  (void *) pool, apr_pool_tag(pool, NULL), (int) memory);
	/*switch_assert(memory < 20000); */
#endif

	ptr = apr_palloc(pool, memory);
	switch_assert(ptr != NULL);
	memset(ptr, 0, memory);

#ifdef LOCK_MORE
#ifdef USE_MEM_LOCK
	switch_mutex_unlock(memory_manager.mem_lock);
#endif
#endif

	return ptr;
}

SWITCH_DECLARE(void) switch_core_memory_reclaim(void)
{
#if !defined(PER_POOL_LOCK) && !defined(INSTANTLY_DESTROY_POOLS)
	switch_memory_pool_t *pool;
	void *pop = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Returning %d recycled memory pool(s)\n",
					  switch_queue_size(memory_manager.pool_recycle_queue) + switch_queue_size(memory_manager.pool_queue));

	while (switch_queue_trypop(memory_manager.pool_recycle_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		pool = (switch_memory_pool_t *) pop;
		if (!pool) {
			break;
		}
#ifdef USE_MEM_LOCK
		switch_mutex_lock(memory_manager.mem_lock);
#endif
		apr_pool_destroy(pool);
#ifdef USE_MEM_LOCK
		switch_mutex_unlock(memory_manager.mem_lock);
#endif
	}
#endif
	return;
}

static void *SWITCH_THREAD_FUNC pool_thread(switch_thread_t *thread, void *obj)
{
	memory_manager.pool_thread_running = 1;

	while (memory_manager.pool_thread_running == 1) {
		int len = switch_queue_size(memory_manager.pool_queue);

		if (len) {
			int x = len, done = 0;

			switch_yield(1000000);
#ifdef USE_MEM_LOCK
			switch_mutex_lock(memory_manager.mem_lock);
#endif
			while (x > 0) {
				void *pop = NULL;
				if (switch_queue_pop(memory_manager.pool_queue, &pop) != SWITCH_STATUS_SUCCESS || !pop) {
					done = 1;
					break;
				}
#if defined(PER_POOL_LOCK) || defined(DESTROY_POOLS)
#ifdef USE_MEM_LOCK
				switch_mutex_lock(memory_manager.mem_lock);
#endif

#ifdef DEBUG_ALLOC
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%p DESTROY POOL\n", (void *) pop);	
#endif
				apr_pool_destroy(pop);
#ifdef USE_MEM_LOCK
				switch_mutex_unlock(memory_manager.mem_lock);
#endif
#else
				apr_pool_mutex_set(pop, NULL);
#ifdef DEBUG_ALLOC
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%p DESTROY POOL\n", (void *) pop);	
#endif
				apr_pool_clear(pop);
				if (switch_queue_trypush(memory_manager.pool_recycle_queue, pop) != SWITCH_STATUS_SUCCESS) {
#ifdef USE_MEM_LOCK
					switch_mutex_lock(memory_manager.mem_lock);
#endif
#ifdef DEBUG_ALLOC
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%p DESTROY POOL\n", (void *) pop);	
#endif
					apr_pool_destroy(pop);
#ifdef USE_MEM_LOCK
					switch_mutex_unlock(memory_manager.mem_lock);
#endif

				}
#endif
				x--;
			}
#ifdef USE_MEM_LOCK
			switch_mutex_unlock(memory_manager.mem_lock);
#endif
			if (done) {
				goto done;
			}
		} else {
			switch_yield(1000000);
		}
	}

  done:
	switch_core_memory_reclaim();

	{
		void *pop = NULL;
		while (switch_queue_trypop(memory_manager.pool_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
#ifdef USE_MEM_LOCK
			switch_mutex_lock(memory_manager.mem_lock);
#endif
			apr_pool_destroy(pop);
			pop = NULL;
#ifdef USE_MEM_LOCK
			switch_mutex_unlock(memory_manager.mem_lock);
#endif

		}
	}

	memory_manager.pool_thread_running = 0;

	return NULL;
}

#ifndef INSTANTLY_DESTROY_POOLS
static switch_thread_t *pool_thread_p = NULL;
#endif

void switch_core_memory_stop(void)
{
#ifndef INSTANTLY_DESTROY_POOLS
	switch_status_t st;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Stopping memory pool queue.\n");

	memory_manager.pool_thread_running = 0;
	switch_thread_join(&st, pool_thread_p);
#endif
}

switch_memory_pool_t *switch_core_memory_init(void)
{
#ifndef INSTANTLY_DESTROY_POOLS
	switch_threadattr_t *thd_attr;
#endif
#ifdef PER_POOL_LOCK
	apr_allocator_t *my_allocator = NULL;
	apr_thread_mutex_t *my_mutex;
#endif

	memset(&memory_manager, 0, sizeof(memory_manager));

#ifdef PER_POOL_LOCK
	if ((apr_allocator_create(&my_allocator)) != APR_SUCCESS) {
		abort();
	}

	if ((apr_pool_create_ex(&memory_manager.memory_pool, NULL, NULL, my_allocator)) != APR_SUCCESS) {
		apr_allocator_destroy(my_allocator);
		my_allocator = NULL;
		abort();
	}

	if ((apr_thread_mutex_create(&my_mutex, APR_THREAD_MUTEX_NESTED, memory_manager.memory_pool)) != APR_SUCCESS) {
		abort();
	}

	apr_allocator_mutex_set(my_allocator, my_mutex);
	apr_pool_mutex_set(memory_manager.memory_pool, my_mutex);
	apr_allocator_owner_set(my_allocator, memory_manager.memory_pool);
	apr_pool_tag(memory_manager.memory_pool, "core_pool");
#else
	apr_pool_create(&memory_manager.memory_pool, NULL);
	switch_assert(memory_manager.memory_pool != NULL);
#endif

#ifdef USE_MEM_LOCK
	switch_mutex_init(&memory_manager.mem_lock, SWITCH_MUTEX_NESTED, memory_manager.memory_pool);
#endif

#ifdef INSTANTLY_DESTROY_POOLS
	{
		void *foo;
		foo = (void *) (intptr_t) pool_thread;
	}
#else

	switch_queue_create(&memory_manager.pool_queue, 50000, memory_manager.memory_pool);
	switch_queue_create(&memory_manager.pool_recycle_queue, 50000, memory_manager.memory_pool);

	switch_threadattr_create(&thd_attr, memory_manager.memory_pool);
	switch_threadattr_detach_set(thd_attr, 0);

	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&pool_thread_p, thd_attr, pool_thread, NULL, memory_manager.memory_pool);

	while (!memory_manager.pool_thread_running) {
		switch_cond_next();
	}
#endif

	return memory_manager.memory_pool;
}

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
