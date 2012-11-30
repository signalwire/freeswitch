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
#include "ks_threadmutex.h"

#ifdef WIN32
#include <process.h>

#define KS_THREAD_CALLING_CONVENTION __stdcall

struct ks_mutex {
	CRITICAL_SECTION mutex;
};

#else

#include <pthread.h>

#define KS_THREAD_CALLING_CONVENTION

struct ks_mutex {
	pthread_mutex_t mutex;
};

#endif

struct ks_thread {
#ifdef WIN32
	void *handle;
#else
	pthread_t handle;
#endif
	void *private_data;
	ks_thread_function_t function;
	size_t stack_size;
#ifndef WIN32
	pthread_attr_t attribute;
#endif
};

size_t thread_default_stacksize = 240 * 1024;

void ks_thread_override_default_stacksize(size_t size)
{
	thread_default_stacksize = size;
}

static void * KS_THREAD_CALLING_CONVENTION thread_launch(void *args)
{
	void *exit_val;
    ks_thread_t *thread = (ks_thread_t *)args;
	exit_val = thread->function(thread, thread->private_data);
#ifndef WIN32
	pthread_attr_destroy(&thread->attribute);
#endif
	free(thread);

	return exit_val;
}

KS_DECLARE(ks_status_t) ks_thread_create_detached(ks_thread_function_t func, void *data)
{
	return ks_thread_create_detached_ex(func, data, thread_default_stacksize);
}

ks_status_t ks_thread_create_detached_ex(ks_thread_function_t func, void *data, size_t stack_size)
{
	ks_thread_t *thread = NULL;
	ks_status_t status = KS_FAIL;

	if (!func || !(thread = (ks_thread_t *)malloc(sizeof(ks_thread_t)))) {
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

	status = KS_SUCCESS;
	goto done;
#else
	
	if (pthread_attr_init(&thread->attribute) != 0)	goto fail;

	if (pthread_attr_setdetachstate(&thread->attribute, PTHREAD_CREATE_DETACHED) != 0) goto failpthread;

	if (thread->stack_size && pthread_attr_setstacksize(&thread->attribute, thread->stack_size) != 0) goto failpthread;

	if (pthread_create(&thread->handle, &thread->attribute, thread_launch, thread) != 0) goto failpthread;

	status = KS_SUCCESS;
	goto done;

 failpthread:

	pthread_attr_destroy(&thread->attribute);
#endif

 fail:
	if (thread) {
		free(thread);
	}
 done:
	return status;
}


KS_DECLARE(ks_status_t) ks_mutex_create(ks_mutex_t **mutex)
{
	ks_status_t status = KS_FAIL;
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	ks_mutex_t *check = NULL;

	check = (ks_mutex_t *)malloc(sizeof(**mutex));
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
	status = KS_SUCCESS;

 done:
	return status;
}

KS_DECLARE(ks_status_t) ks_mutex_destroy(ks_mutex_t **mutex)
{
	ks_mutex_t *mp = *mutex;
	*mutex = NULL;
	if (!mp) {
		return KS_FAIL;
	}
#ifdef WIN32
	DeleteCriticalSection(&mp->mutex);
#else
	if (pthread_mutex_destroy(&mp->mutex))
		return KS_FAIL;
#endif
	free(mp);
	return KS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_lock(ks_mutex_t *mutex)
{
#ifdef WIN32
	EnterCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_lock(&mutex->mutex))
		return KS_FAIL;
#endif
	return KS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_trylock(ks_mutex_t *mutex)
{
#ifdef WIN32
	if (!TryEnterCriticalSection(&mutex->mutex))
		return KS_FAIL;
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return KS_FAIL;
#endif
	return KS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_mutex_unlock(ks_mutex_t *mutex)
{
#ifdef WIN32
	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex))
		return KS_FAIL;
#endif
	return KS_SUCCESS;
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
