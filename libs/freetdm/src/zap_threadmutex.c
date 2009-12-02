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
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 *
 */

#ifdef WIN32
/* required for TryEnterCriticalSection definition.  Must be defined before windows.h include */
#define _WIN32_WINNT 0x0400
#endif

#include "openzap.h"
#include "zap_threadmutex.h"

#ifdef WIN32
#include <process.h>

#define ZAP_THREAD_CALLING_CONVENTION __stdcall

struct zap_mutex {
	CRITICAL_SECTION mutex;
};

struct zap_condition {
	HANDLE condition;
};

#else

#include <pthread.h>

#define ZAP_THREAD_CALLING_CONVENTION

struct zap_mutex {
	pthread_mutex_t mutex;
};

struct zap_condition {
	pthread_cond_t condition;
	pthread_mutex_t *mutex;
};

#endif

struct zap_thread {
#ifdef WIN32
	void *handle;
#else
	pthread_t handle;
#endif
	void *private_data;
	zap_thread_function_t function;
	zap_size_t stack_size;
#ifndef WIN32
	pthread_attr_t attribute;
#endif
};

zap_size_t thread_default_stacksize = 0;

OZ_DECLARE(void) zap_thread_override_default_stacksize(zap_size_t size)
{
	thread_default_stacksize = size;
}

static void * ZAP_THREAD_CALLING_CONVENTION thread_launch(void *args)
{
	void *exit_val;
    zap_thread_t *thread = (zap_thread_t *)args;
	exit_val = thread->function(thread, thread->private_data);
#ifndef WIN32
	pthread_attr_destroy(&thread->attribute);
#endif
	zap_safe_free(thread);

	return exit_val;
}

OZ_DECLARE(zap_status_t) zap_thread_create_detached(zap_thread_function_t func, void *data)
{
	return zap_thread_create_detached_ex(func, data, thread_default_stacksize);
}

OZ_DECLARE(zap_status_t) zap_thread_create_detached_ex(zap_thread_function_t func, void *data, zap_size_t stack_size)
{
	zap_thread_t *thread = NULL;
	zap_status_t status = ZAP_FAIL;

	if (!func || !(thread = (zap_thread_t *)zap_malloc(sizeof(zap_thread_t)))) {
		goto done;
	}

	thread->private_data = data;
	thread->function = func;
	thread->stack_size = stack_size;

#if defined(WIN32)
	thread->handle = (void *)_beginthreadex(NULL, (unsigned)thread->stack_size, (unsigned int (__stdcall *)(void *))thread_launch, thread, 0, NULL);
	if (!thread->handle) {
		goto fail;
	}
	CloseHandle(thread->handle);

	status = ZAP_SUCCESS;
	goto done;
#else
	
	if (pthread_attr_init(&thread->attribute) != 0)	goto fail;

	if (pthread_attr_setdetachstate(&thread->attribute, PTHREAD_CREATE_DETACHED) != 0) goto failpthread;

	if (thread->stack_size && pthread_attr_setstacksize(&thread->attribute, thread->stack_size) != 0) goto failpthread;

	if (pthread_create(&thread->handle, &thread->attribute, thread_launch, thread) != 0) goto failpthread;

	status = ZAP_SUCCESS;
	goto done;
 failpthread:
	pthread_attr_destroy(&thread->attribute);
#endif

 fail:
	if (thread) {
		zap_safe_free(thread);
	}
 done:
	return status;
}


OZ_DECLARE(zap_status_t) zap_mutex_create(zap_mutex_t **mutex)
{
	zap_status_t status = ZAP_FAIL;
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	zap_mutex_t *check = NULL;

	check = (zap_mutex_t *)zap_malloc(sizeof(**mutex));
	if (!check)
		goto done;
#ifdef WIN32
	InitializeCriticalSection(&check->mutex);
#else
	if (pthread_mutexattr_init(&attr))
		goto done;

	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE))
		goto fail;

	if (pthread_mutex_init(&check->mutex, &attr))
		goto fail;

	goto success;

 fail:
	pthread_mutexattr_destroy(&attr);
	goto done;

 success:
#endif
	*mutex = check;
	status = ZAP_SUCCESS;

 done:
	return status;
}

OZ_DECLARE(zap_status_t) zap_mutex_destroy(zap_mutex_t **mutex)
{
	zap_mutex_t *mp = *mutex;
	*mutex = NULL;
	if (!mp) {
		return ZAP_FAIL;
	}
#ifdef WIN32
	DeleteCriticalSection(&mp->mutex);
#else
	if (pthread_mutex_destroy(&mp->mutex))
		return ZAP_FAIL;
#endif
	zap_safe_free(mp);
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) _zap_mutex_lock(zap_mutex_t *mutex)
{
#ifdef WIN32
	EnterCriticalSection(&mutex->mutex);
#else
	int err;
	if ((err = pthread_mutex_lock(&mutex->mutex))) {
		zap_log(ZAP_LOG_ERROR, "Failed to lock mutex %d:%s\n", err, strerror(err));
		return ZAP_FAIL;
	}
#endif
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) _zap_mutex_trylock(zap_mutex_t *mutex)
{
#ifdef WIN32
	if (!TryEnterCriticalSection(&mutex->mutex))
		return ZAP_FAIL;
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return ZAP_FAIL;
#endif
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) _zap_mutex_unlock(zap_mutex_t *mutex)
{
#ifdef WIN32
	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex))
		return ZAP_FAIL;
#endif
	return ZAP_SUCCESS;
}


OZ_DECLARE(zap_status_t) zap_condition_create(zap_condition_t **incondition, zap_mutex_t *mutex)
{
	zap_condition_t *condition = NULL;

	zap_assert_return(incondition != NULL, ZAP_FAIL, "Condition double pointer is null!\n");
	zap_assert_return(mutex != NULL, ZAP_FAIL, "Mutex for condition must not be null!\n");

	condition = zap_calloc(1, sizeof(*condition));
	if (!condition) {
		return ZAP_FAIL;
	}

#ifdef WIN32
	condition->condition = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!condition->condition) {
		goto failed;
	}
#else
	condition->mutex = &mutex->mutex;
	if (pthread_cond_init(&condition->condition, NULL)) {
		goto failed;
	}
#endif

	*incondition = condition;
	return ZAP_SUCCESS;

failed:
	if (condition) {
		zap_safe_free(condition);
	}
	return ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_condition_wait(zap_condition_t *condition, int ms)
{
#ifdef WIN32
	DWORD res = 0;
#endif
	zap_assert_return(condition != NULL, ZAP_FAIL, "Condition is null!\n");
#ifdef WIN32
	res = WaitForSingleObject(condition->condition, ms > 0 ? ms : INFINITE);
	switch (res) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		return ZAP_TIMEOUT;
	case WAIT_FAILED:
		return ZAP_FAIL;
	case WAIT_OBJECT_0:
		return ZAP_SUCCESS;
	default:
		zap_log(ZAP_LOG_ERROR, "Error waiting for openzap condition event (WaitForSingleObject returned %d)\n", res);
		return ZAP_FAIL;
	}
#else
	int res = 0;
	if (ms > 0) {
		struct timespec waitms; 
		waitms.tv_sec = time(NULL) + ( ms / 1000 );
		waitms.tv_nsec = 1000 * 1000 * ( ms % 1000 );
		res = pthread_cond_timedwait(&condition->condition, condition->mutex, &waitms);
	} else {
		res = pthread_cond_wait(&condition->condition, condition->mutex);
	}
	if (res != 0) {
		if (res == ETIMEDOUT) {
			return ZAP_TIMEOUT;
		}
		return ZAP_FAIL;
	}
	return ZAP_SUCCESS;
#endif
}

OZ_DECLARE(zap_status_t) zap_condition_signal(zap_condition_t *condition)
{
	zap_assert_return(condition != NULL, ZAP_FAIL, "Condition is null!\n");
#ifdef WIN32
	if (!SetEvent(condition->condition)) {
		return ZAP_FAIL;
	}
#else
	int err;
	if ((err = pthread_cond_signal(&condition->condition))) {
		zap_log(ZAP_LOG_ERROR, "Failed to signal condition %d:%s\n", err, strerror(err));
		return ZAP_FAIL;
	}
#endif
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_condition_destroy(zap_condition_t **incondition)
{
	zap_condition_t *condition = NULL;
	zap_assert_return(incondition != NULL, ZAP_FAIL, "Condition null when destroying!\n");
	condition = *incondition;
#ifdef WIN32
	CloseHandle(condition->condition);
#else
	if (pthread_cond_destroy(&condition->condition)) {
		return ZAP_FAIL;
	}
	zap_safe_free(condition);
#endif
	*incondition = NULL;
	return ZAP_SUCCESS;
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
