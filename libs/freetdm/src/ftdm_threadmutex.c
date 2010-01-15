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

#include "freetdm.h"
#include "ftdm_threadmutex.h"

#ifdef WIN32
#include <process.h>

#define FTDM_THREAD_CALLING_CONVENTION __stdcall

struct ftdm_mutex {
	CRITICAL_SECTION mutex;
};

#else
#include <pthread.h>

#define FTDM_THREAD_CALLING_CONVENTION

struct ftdm_mutex {
	pthread_mutex_t mutex;
};

#endif

struct ftdm_condition {
#ifdef WIN32
	HANDLE condition;
#else
	pthread_cond_t condition;
#endif
	ftdm_mutex_t *mutex;
};


struct ftdm_thread {
#ifdef WIN32
	void *handle;
#else
	pthread_t handle;
#endif
	void *private_data;
	ftdm_thread_function_t function;
	ftdm_size_t stack_size;
#ifndef WIN32
	pthread_attr_t attribute;
#endif
};

ftdm_size_t thread_default_stacksize = 0;

FT_DECLARE(void) ftdm_thread_override_default_stacksize(ftdm_size_t size)
{
	thread_default_stacksize = size;
}

static void * FTDM_THREAD_CALLING_CONVENTION thread_launch(void *args)
{
	void *exit_val;
    ftdm_thread_t *thread = (ftdm_thread_t *)args;
	exit_val = thread->function(thread, thread->private_data);
#ifndef WIN32
	pthread_attr_destroy(&thread->attribute);
#endif
	ftdm_safe_free(thread);

	return exit_val;
}

FT_DECLARE(ftdm_status_t) ftdm_thread_create_detached(ftdm_thread_function_t func, void *data)
{
	return ftdm_thread_create_detached_ex(func, data, thread_default_stacksize);
}

FT_DECLARE(ftdm_status_t) ftdm_thread_create_detached_ex(ftdm_thread_function_t func, void *data, ftdm_size_t stack_size)
{
	ftdm_thread_t *thread = NULL;
	ftdm_status_t status = FTDM_FAIL;

	if (!func || !(thread = (ftdm_thread_t *)ftdm_malloc(sizeof(ftdm_thread_t)))) {
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

	status = FTDM_SUCCESS;
	goto done;
#else
	
	if (pthread_attr_init(&thread->attribute) != 0)	goto fail;

	if (pthread_attr_setdetachstate(&thread->attribute, PTHREAD_CREATE_DETACHED) != 0) goto failpthread;

	if (thread->stack_size && pthread_attr_setstacksize(&thread->attribute, thread->stack_size) != 0) goto failpthread;

	if (pthread_create(&thread->handle, &thread->attribute, thread_launch, thread) != 0) goto failpthread;

	status = FTDM_SUCCESS;
	goto done;
 failpthread:
	pthread_attr_destroy(&thread->attribute);
#endif

 fail:
	if (thread) {
		ftdm_safe_free(thread);
	}
 done:
	return status;
}


FT_DECLARE(ftdm_status_t) ftdm_mutex_create(ftdm_mutex_t **mutex)
{
	ftdm_status_t status = FTDM_FAIL;
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	ftdm_mutex_t *check = NULL;

	check = (ftdm_mutex_t *)ftdm_malloc(sizeof(**mutex));
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
	status = FTDM_SUCCESS;

 done:
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_mutex_destroy(ftdm_mutex_t **mutex)
{
	ftdm_mutex_t *mp = *mutex;
	*mutex = NULL;
	if (!mp) {
		return FTDM_FAIL;
	}
#ifdef WIN32
	DeleteCriticalSection(&mp->mutex);
#else
	if (pthread_mutex_destroy(&mp->mutex))
		return FTDM_FAIL;
#endif
	ftdm_safe_free(mp);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_lock(ftdm_mutex_t *mutex)
{
#ifdef WIN32
	EnterCriticalSection(&mutex->mutex);
#else
	int err;
	if ((err = pthread_mutex_lock(&mutex->mutex))) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to lock mutex %d:%s\n", err, strerror(err));
		return FTDM_FAIL;
	}
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_trylock(ftdm_mutex_t *mutex)
{
#ifdef WIN32
	if (!TryEnterCriticalSection(&mutex->mutex))
		return FTDM_FAIL;
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return FTDM_FAIL;
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_unlock(ftdm_mutex_t *mutex)
{
#ifdef WIN32
	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex))
		return FTDM_FAIL;
#endif
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_condition_create(ftdm_condition_t **incondition, ftdm_mutex_t *mutex)
{
	ftdm_condition_t *condition = NULL;

	ftdm_assert_return(incondition != NULL, FTDM_FAIL, "Condition double pointer is null!\n");
	ftdm_assert_return(mutex != NULL, FTDM_FAIL, "Mutex for condition must not be null!\n");

	condition = ftdm_calloc(1, sizeof(*condition));
	if (!condition) {
		return FTDM_FAIL;
	}

	condition->mutex = mutex;
#ifdef WIN32
	condition->condition = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!condition->condition) {
		goto failed;
	}
#else
	if (pthread_cond_init(&condition->condition, NULL)) {
		goto failed;
	}
#endif

	*incondition = condition;
	return FTDM_SUCCESS;

failed:
	if (condition) {
		ftdm_safe_free(condition);
	}
	return FTDM_FAIL;
}

#define ONE_BILLION 1000000000

FT_DECLARE(ftdm_status_t) ftdm_condition_wait(ftdm_condition_t *condition, int ms)
{
#ifdef WIN32
	DWORD res = 0;
#endif
	ftdm_assert_return(condition != NULL, FTDM_FAIL, "Condition is null!\n");
#ifdef WIN32
	ftdm_mutex_unlock(condition->mutex);
	res = WaitForSingleObject(condition->condition, ms > 0 ? ms : INFINITE);
	ftdm_mutex_lock(condition->mutex);
	switch (res) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		return FTDM_TIMEOUT;
	case WAIT_FAILED:
		return FTDM_FAIL;
	case WAIT_OBJECT_0:
		return FTDM_SUCCESS;
	default:
		ftdm_log(FTDM_LOG_ERROR, "Error waiting for freetdm condition event (WaitForSingleObject returned %d)\n", res);
		return FTDM_FAIL;
	}
#else
	int res = 0;
	if (ms > 0) {
		struct timeval t;
		struct timespec waitms;
		gettimeofday(&t, NULL);
		waitms.tv_sec = t.tv_sec + ( ms / 1000 );
		waitms.tv_nsec = 1000*(t.tv_usec + (1000 * ( ms % 1000 )));
		if (waitms.tv_nsec >= ONE_BILLION) {
				waitms.tv_sec++;
				waitms.tv_nsec -= ONE_BILLION;
		}
		res = pthread_cond_timedwait(&condition->condition, &condition->mutex->mutex, &waitms);
	} else {
		res = pthread_cond_wait(&condition->condition, &condition->mutex->mutex);
	}
	if (res != 0) {
		if (res == ETIMEDOUT) {
			return FTDM_TIMEOUT;
		}

		ftdm_log(FTDM_LOG_CRIT,"pthread_cond_timedwait failed (%d)\n", res);
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
#endif
}

FT_DECLARE(ftdm_status_t) ftdm_condition_signal(ftdm_condition_t *condition)
{
	ftdm_assert_return(condition != NULL, FTDM_FAIL, "Condition is null!\n");
#ifdef WIN32
	if (!SetEvent(condition->condition)) {
		return FTDM_FAIL;
	}
#else
	int err;
	if ((err = pthread_cond_signal(&condition->condition))) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to signal condition %d:%s\n", err, strerror(err));
		return FTDM_FAIL;
	}
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_condition_destroy(ftdm_condition_t **incondition)
{
	ftdm_condition_t *condition = NULL;
	ftdm_assert_return(incondition != NULL, FTDM_FAIL, "Condition null when destroying!\n");
	condition = *incondition;
#ifdef WIN32
	CloseHandle(condition->condition);
#else
	if (pthread_cond_destroy(&condition->condition)) {
		return FTDM_FAIL;
	}
#endif
	ftdm_safe_free(condition);
	*incondition = NULL;
	return FTDM_SUCCESS;
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
