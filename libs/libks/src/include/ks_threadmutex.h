/*
 * Cross Platform Thread/Mutex abstraction
 * Copyright(C) 2015 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without warranty of any kind,
 * either expressed or implied, including, without limitation, warranties that the covered code
 * is free of defects, merchantable, fit for a particular purpose or non-infringing. The entire
 * risk as to the quality and performance of the covered code is with you. Should any covered
 * code prove defective in any respect, you (not the initial developer or any other contributor)
 * assume the cost of any necessary servicing, repair or correction. This disclaimer of warranty
 * constitutes an essential part of this license. No use of any covered code is authorized hereunder
 * except under this disclaimer.
 *
 */

#ifndef _KS_THREADMUTEX_H
#define _KS_THREADMUTEX_H

#include "ks.h"

KS_BEGIN_EXTERN_C

#ifdef WIN32
#include <process.h>
#define KS_THREAD_CALLING_CONVENTION __stdcall
#else
#include <pthread.h>
#define KS_THREAD_CALLING_CONVENTION
#endif

#define KS_THREAD_DEFAULT_STACK 240 * 1024

	typedef struct ks_thread ks_thread_t;
	typedef void *(*ks_thread_function_t) (ks_thread_t *, void *);

	typedef
#ifdef WIN32
	void *
#else
	pthread_t
#endif
	ks_thread_os_handle_t;

struct ks_thread {
		ks_pool_t *pool;
#ifdef WIN32
		void *handle;
#else
		pthread_t handle;
		pthread_attr_t attribute;
#endif
		void *private_data;
		ks_thread_function_t function;
		size_t stack_size;
		uint32_t flags;
		uint8_t running;
		uint8_t priority;
		void *return_data;
	};

	typedef enum {
		KS_PRI_LOW = 1,
		KS_PRI_NORMAL = 10,
		KS_PRI_IMPORTANT = 50,
		KS_PRI_REALTIME = 99,
	} ks_thread_priority_t;

	typedef enum {
		KS_THREAD_FLAG_DEFAULT   = 0,
		KS_THREAD_FLAG_DETATCHED = (1 << 0)
	} ks_thread_flags_t;

	KS_DECLARE(int) ks_thread_set_priority(int nice_val);
    KS_DECLARE(ks_thread_os_handle_t) ks_thread_self(void);
    KS_DECLARE(ks_thread_os_handle_t) ks_thread_os_handle(ks_thread_t *thread);
	KS_DECLARE(ks_status_t) ks_thread_create_ex(ks_thread_t **thread, ks_thread_function_t func, void *data,
											 uint32_t flags, size_t stack_size, ks_thread_priority_t priority, ks_pool_t *pool);
	KS_DECLARE(ks_status_t) ks_thread_join(ks_thread_t *thread);
	KS_DECLARE(uint8_t) ks_thread_priority(ks_thread_t *thread);

#define ks_thread_create(thread, func, data, pool)						\
	ks_thread_create_ex(thread, func, data, KS_THREAD_FLAG_DEFAULT, KS_THREAD_DEFAULT_STACK, KS_PRI_NORMAL, pool)

	typedef enum {
		KS_MUTEX_FLAG_DEFAULT       = 0,
		KS_MUTEX_FLAG_NON_RECURSIVE = (1 << 0)
	} ks_mutex_flags_t;

	typedef struct ks_mutex ks_mutex_t;

	KS_DECLARE(ks_status_t) ks_mutex_create(ks_mutex_t **mutex, unsigned int flags, ks_pool_t *pool);
	KS_DECLARE(ks_status_t) ks_mutex_lock(ks_mutex_t *mutex);
	KS_DECLARE(ks_status_t) ks_mutex_trylock(ks_mutex_t *mutex);
	KS_DECLARE(ks_status_t) ks_mutex_unlock(ks_mutex_t *mutex);
	KS_DECLARE(ks_status_t) ks_mutex_destroy(ks_mutex_t **mutex);

	typedef struct ks_cond ks_cond_t;

	KS_DECLARE(ks_status_t) ks_cond_create(ks_cond_t **cond, ks_pool_t *pool);
	KS_DECLARE(ks_status_t) ks_cond_create_ex(ks_cond_t **cond, ks_pool_t *pool, ks_mutex_t *mutex);
	KS_DECLARE(ks_status_t) ks_cond_lock(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_trylock(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_unlock(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_signal(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_broadcast(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_try_signal(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_try_broadcast(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_wait(ks_cond_t *cond);
	KS_DECLARE(ks_status_t) ks_cond_timedwait(ks_cond_t *cond, ks_time_t ms);
	KS_DECLARE(ks_status_t) ks_cond_destroy(ks_cond_t **cond);
	KS_DECLARE(ks_mutex_t *) ks_cond_get_mutex(ks_cond_t *cond);

	typedef struct ks_rwl ks_rwl_t;

	KS_DECLARE(ks_status_t) ks_rwl_create(ks_rwl_t **rwlock, ks_pool_t *pool);
	KS_DECLARE(ks_status_t) ks_rwl_read_lock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_write_lock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_try_read_lock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_try_write_lock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_read_unlock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_write_unlock(ks_rwl_t *rwlock);
	KS_DECLARE(ks_status_t) ks_rwl_destroy(ks_rwl_t **rwlock);


KS_END_EXTERN_C

#endif							/* defined(_KS_THREADMUTEX_H) */

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
