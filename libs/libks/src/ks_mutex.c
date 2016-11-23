/* 
 * Cross Platform Thread/Mutex abstraction
 * Copyright(C) 2007 Michael Jerris
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

#ifdef WIN32
/* required for TryEnterCriticalSection definition.  Must be defined before windows.h include */
#define _WIN32_WINNT 0x0400
#endif

#include "ks.h"

#ifdef WIN32
#include <process.h>
#else
#include <pthread.h>
#endif

typedef enum {
	KS_MUTEX_TYPE_DEFAULT,
	KS_MUTEX_TYPE_NON_RECURSIVE
} ks_mutex_type_t;

struct ks_mutex {
#ifdef WIN32
	CRITICAL_SECTION mutex;
	HANDLE handle;
#else
	pthread_mutex_t mutex;
#endif
	ks_pool_t * pool;
	ks_mutex_type_t type;
	uint8_t malloc;
};

static void ks_mutex_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	ks_mutex_t *mutex = (ks_mutex_t *) ptr;

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
#ifdef WIN32
		if (mutex->type == KS_MUTEX_TYPE_NON_RECURSIVE) {
			CloseHandle(mutex->handle);
		} else {
			DeleteCriticalSection(&mutex->mutex);
		}
#else
		pthread_mutex_destroy(&mutex->mutex);
#endif
		break;
	}
}

KS_DECLARE(ks_status_t) ks_mutex_destroy(ks_mutex_t **mutexP)
{
	ks_mutex_t *mutex;

	ks_assert(mutexP);
	
	mutex = *mutexP;
	*mutexP = NULL;

	if (!mutex) return KS_STATUS_FAIL;
	
	if (mutex->malloc) {
#ifdef WIN32
		if (mutex->type == KS_MUTEX_TYPE_NON_RECURSIVE) {
			CloseHandle(mutex->handle);
		} else {
			DeleteCriticalSection(&mutex->mutex);
		}
#else
		pthread_mutex_destroy(&mutex->mutex);
#endif
		free(mutex);
	} else {
		ks_pool_free(mutex->pool, (void *)mutex);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_create(ks_mutex_t **mutex, unsigned int flags, ks_pool_t *pool)
{
	ks_status_t status = KS_STATUS_FAIL;
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	ks_mutex_t *check = NULL;

	if (pool) {
		if (!(check = (ks_mutex_t *) ks_pool_alloc(pool, sizeof(**mutex)))) {
			goto done;
		}
	} else {
		check = malloc(sizeof(**mutex));
		memset(check, 0, sizeof(**mutex));
		check->malloc = 1;
	}

	check->pool = pool;
	check->type = KS_MUTEX_TYPE_DEFAULT;

#ifdef WIN32
	if (flags & KS_MUTEX_FLAG_NON_RECURSIVE) {
		check->type = KS_MUTEX_TYPE_NON_RECURSIVE;
		check->handle = CreateEvent(NULL, FALSE, TRUE, NULL);
	} else {
		InitializeCriticalSection(&check->mutex);
	}
#else
	if (flags & KS_MUTEX_FLAG_NON_RECURSIVE) {
		if (pthread_mutex_init(&check->mutex, NULL))
			goto done;

	} else {
		if (pthread_mutexattr_init(&attr))
			goto done;

		if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE))
			goto fail;

		if (pthread_mutex_init(&check->mutex, &attr))
			goto fail;
	}

	goto success;

  fail:
	pthread_mutexattr_destroy(&attr);
	goto done;

  success:
#endif
	*mutex = check;
	status = KS_STATUS_SUCCESS;

	if (pool) {
		ks_pool_set_cleanup(pool, check, NULL, 0, ks_mutex_cleanup);
	}

  done:
	return status;
}

KS_DECLARE(ks_status_t) ks_mutex_lock(ks_mutex_t *mutex)
{
#ifdef WIN32
	if (mutex->type == KS_MUTEX_TYPE_NON_RECURSIVE) {
        DWORD ret = WaitForSingleObject(mutex->handle, INFINITE);
		if ((ret != WAIT_OBJECT_0) && (ret != WAIT_ABANDONED)) {
            return KS_STATUS_FAIL;
		}
	} else {
		EnterCriticalSection(&mutex->mutex);
	}
#else
	if (pthread_mutex_lock(&mutex->mutex))
		return KS_STATUS_FAIL;
#endif
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_trylock(ks_mutex_t *mutex)
{
#ifdef WIN32
	if (mutex->type == KS_MUTEX_TYPE_NON_RECURSIVE) {
        DWORD ret = WaitForSingleObject(mutex->handle, 0);
		if ((ret != WAIT_OBJECT_0) && (ret != WAIT_ABANDONED)) {
            return KS_STATUS_FAIL;
		}
	} else {
		if (!TryEnterCriticalSection(&mutex->mutex))
			return KS_STATUS_FAIL;
	}
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return KS_STATUS_FAIL;
#endif
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_unlock(ks_mutex_t *mutex)
{
#ifdef WIN32
	if (mutex->type == KS_MUTEX_TYPE_NON_RECURSIVE) {
        if (!SetEvent(mutex->handle)) {
			return KS_STATUS_FAIL;
		}
	} else {
		LeaveCriticalSection(&mutex->mutex);
	}
#else
	if (pthread_mutex_unlock(&mutex->mutex))
		return KS_STATUS_FAIL;
#endif
	return KS_STATUS_SUCCESS;
}



struct ks_cond {
	ks_pool_t * pool;
	ks_mutex_t *mutex;
#ifdef WIN32
	CONDITION_VARIABLE cond;
#else
	pthread_cond_t cond;
#endif
	uint8_t static_mutex;
};

static void ks_cond_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	ks_cond_t *cond = (ks_cond_t *) ptr;

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		if (!cond->static_mutex) {
			ks_mutex_destroy(&cond->mutex);
		}
#ifndef WIN32
		pthread_cond_destroy(&cond->cond);
#endif
		break;
	}
}

KS_DECLARE(ks_status_t) ks_cond_create_ex(ks_cond_t **cond, ks_pool_t *pool, ks_mutex_t *mutex)
{
	ks_status_t status = KS_STATUS_FAIL;
	ks_cond_t *check = NULL;

	*cond = NULL;

	if (!pool)
		goto done;

	if (!(check = (ks_cond_t *) ks_pool_alloc(pool, sizeof(**cond)))) {
		goto done;
	}

	check->pool = pool;
	if (mutex) {
		check->mutex = mutex;
		check->static_mutex = 1;
	} else {
		if (ks_mutex_create(&check->mutex, KS_MUTEX_FLAG_DEFAULT, pool) != KS_STATUS_SUCCESS) {
			goto done;
		}
	}

#ifdef WIN32
	InitializeConditionVariable(&check->cond);
#else
	if (pthread_cond_init(&check->cond, NULL)) {
		if (!check->static_mutex) {
			ks_mutex_destroy(&check->mutex);
		}
		goto done;
	}
#endif

	*cond = check;
	status = KS_STATUS_SUCCESS;
	ks_pool_set_cleanup(pool, check, NULL, 0, ks_cond_cleanup);

  done:
	return status;
}

KS_DECLARE(ks_mutex_t *) ks_cond_get_mutex(ks_cond_t *cond)
{
	return cond->mutex;
}

KS_DECLARE(ks_status_t) ks_cond_create(ks_cond_t **cond, ks_pool_t *pool)
{
	return ks_cond_create_ex(cond, pool, NULL);
}

KS_DECLARE(ks_status_t) ks_cond_lock(ks_cond_t *cond)
{
	return ks_mutex_lock(cond->mutex);
}

KS_DECLARE(ks_status_t) ks_cond_trylock(ks_cond_t *cond)
{
	return ks_mutex_trylock(cond->mutex);
}

KS_DECLARE(ks_status_t) ks_cond_unlock(ks_cond_t *cond)
{
	return ks_mutex_unlock(cond->mutex);
}

KS_DECLARE(ks_status_t) ks_cond_signal(ks_cond_t *cond)
{
	ks_cond_lock(cond);
#ifdef WIN32
	WakeConditionVariable(&cond->cond);
#else
	pthread_cond_signal(&cond->cond);
#endif
	ks_cond_unlock(cond);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_broadcast(ks_cond_t *cond)
{
	ks_cond_lock(cond);
#ifdef WIN32
	WakeAllConditionVariable(&cond->cond);
#else
	pthread_cond_broadcast(&cond->cond);
#endif
	ks_cond_unlock(cond);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_try_signal(ks_cond_t *cond)
{
	if (ks_cond_trylock(cond) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
#ifdef WIN32
	WakeConditionVariable(&cond->cond);
#else
	pthread_cond_signal(&cond->cond);
#endif
	ks_cond_unlock(cond);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_try_broadcast(ks_cond_t *cond)
{
	if (ks_cond_trylock(cond) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
#ifdef WIN32
	WakeAllConditionVariable(&cond->cond);
#else
	pthread_cond_broadcast(&cond->cond);
#endif
	ks_cond_unlock(cond);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_wait(ks_cond_t *cond)
{
#ifdef WIN32
	SleepConditionVariableCS(&cond->cond, &cond->mutex->mutex, INFINITE);
#else
	pthread_cond_wait(&cond->cond, &cond->mutex->mutex);
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_timedwait(ks_cond_t *cond, ks_time_t ms)
{
#ifdef WIN32
	if(!SleepConditionVariableCS(&cond->cond, &cond->mutex->mutex, (DWORD)ms)) {
		if (GetLastError() == ERROR_TIMEOUT) {
			return KS_STATUS_TIMEOUT;
		} else {
			return KS_STATUS_FAIL;
		}
	}
#else
	struct timespec ts;
	ks_time_t n = ks_time_now() + (ms * 1000);
	ts.tv_sec   = ks_time_sec(n);
	ts.tv_nsec  = ks_time_nsec(n);
	if (pthread_cond_timedwait(&cond->cond, &cond->mutex->mutex, &ts)) {
		switch(errno) {
		case ETIMEDOUT:
			return KS_STATUS_TIMEOUT;
		default:
			return KS_STATUS_FAIL;
		}
	}
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_cond_destroy(ks_cond_t **cond)
{
	ks_cond_t *condp = *cond;

	if (!condp) {
		return KS_STATUS_FAIL;
	}

	*cond = NULL;

	return ks_pool_free(condp->pool, condp);
}


struct ks_rwl {
#ifdef WIN32
	SRWLOCK rwlock;
	ks_hash_t *read_lock_list;
#else
	pthread_rwlock_t rwlock;
#endif
	ks_pool_t *pool;
	ks_thread_os_handle_t write_locker;
	uint32_t wlc;
};

static void ks_rwl_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
#ifndef WIN32
	ks_rwl_t *rwlock = (ks_rwl_t *) ptr;
#endif

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
#ifndef WIN32
		pthread_rwlock_destroy(&rwlock->rwlock);
#endif
		break;
	}
}

KS_DECLARE(ks_status_t) ks_rwl_create(ks_rwl_t **rwlock, ks_pool_t *pool)
{
	ks_status_t status = KS_STATUS_FAIL;
	ks_rwl_t *check = NULL;
	*rwlock = NULL;

	if (!pool) {
		goto done;
	}

	if (!(check = (ks_rwl_t *) ks_pool_alloc(pool, sizeof(**rwlock)))) {
		goto done;
	}

	check->pool = pool;

#ifdef WIN32

	if (ks_hash_create(&check->read_lock_list, KS_HASH_MODE_PTR, KS_HASH_FLAG_NONE, pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	InitializeSRWLock(&check->rwlock);
#else
	if ((pthread_rwlock_init(&check->rwlock, NULL))) {
		goto done;
	}
#endif

	*rwlock = check;
	status = KS_STATUS_SUCCESS;
	ks_pool_set_cleanup(pool, check, NULL, 0, ks_rwl_cleanup);
 done:
	return status;
}

KS_DECLARE(ks_status_t) ks_rwl_read_lock(ks_rwl_t *rwlock)
{
#ifdef WIN32
	int count = (int)(intptr_t)ks_hash_remove(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self());

	if (count) {
		ks_hash_insert(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self(), (void *)(intptr_t)++count);
		return KS_STATUS_SUCCESS;
	}


	AcquireSRWLockShared(&rwlock->rwlock);
#else
	pthread_rwlock_rdlock(&rwlock->rwlock);
#endif

#ifdef WIN32
	ks_hash_insert(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self(), (void *)(intptr_t)(int)1);
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_write_lock(ks_rwl_t *rwlock)
{

	int me = (rwlock->write_locker == ks_thread_self());

	if (me) {
		rwlock->wlc++;
		return KS_STATUS_SUCCESS;
	}

#ifdef WIN32
	AcquireSRWLockExclusive(&rwlock->rwlock);
#else
	pthread_rwlock_wrlock(&rwlock->rwlock);
#endif
	rwlock->write_locker = ks_thread_self();

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_try_read_lock(ks_rwl_t *rwlock)
{
#ifdef WIN32
	int count = (int)(intptr_t)ks_hash_remove(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self());

	if (count) {
		ks_hash_insert(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self(), (void *)(intptr_t)++count);
		return KS_STATUS_SUCCESS;
	}

	if (!TryAcquireSRWLockShared(&rwlock->rwlock)) {
		return KS_STATUS_FAIL;
	}
#else
	if (pthread_rwlock_tryrdlock(&rwlock->rwlock)) {
		return KS_STATUS_FAIL;
	}
#endif

#ifdef WIN32
	ks_hash_insert(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self(), (void *)(intptr_t)(int)1);
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_try_write_lock(ks_rwl_t *rwlock)
{
	int me = (rwlock->write_locker == ks_thread_self());

	if (me) {
		rwlock->wlc++;
		return KS_STATUS_SUCCESS;
	}

#ifdef WIN32
	if (!TryAcquireSRWLockExclusive(&rwlock->rwlock)) {
		return KS_STATUS_FAIL;
	}
#else
	if (pthread_rwlock_trywrlock(&rwlock->rwlock)) {
		return KS_STATUS_FAIL;
	}
#endif

	rwlock->write_locker = ks_thread_self();

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_read_unlock(ks_rwl_t *rwlock)
{
#ifdef WIN32
	int count = (int)(intptr_t)ks_hash_remove(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self());

	if (count > 1) {
		ks_hash_insert(rwlock->read_lock_list, (void *)(intptr_t)ks_thread_self(), (void *)(intptr_t)--count);
		return KS_STATUS_SUCCESS;
	}

	ReleaseSRWLockShared(&rwlock->rwlock);
#else
	pthread_rwlock_unlock(&rwlock->rwlock);
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_write_unlock(ks_rwl_t *rwlock)
{
	int me = (rwlock->write_locker == ks_thread_self());

	if (me && rwlock->wlc > 0) {
		rwlock->wlc--;
		return KS_STATUS_SUCCESS;
	}

	rwlock->write_locker = 0;

#ifdef WIN32
	ReleaseSRWLockExclusive(&rwlock->rwlock);
#else
	pthread_rwlock_unlock(&rwlock->rwlock);
#endif

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_rwl_destroy(ks_rwl_t **rwlock)
{
	ks_rwl_t *rwlockp = *rwlock;


	if (!rwlockp) {
		return KS_STATUS_FAIL;
	}

	*rwlock = NULL;

	return ks_pool_free(rwlockp->pool, rwlockp);
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
