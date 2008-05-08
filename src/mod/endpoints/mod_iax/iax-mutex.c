/* 
 * Simple Mutex abstraction
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

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include "iax-mutex.h"


#ifdef WIN32
#define _WIN32_WINNT 0x0400
#include <windows.h>
struct mutex {
	CRITICAL_SECTION mutex;
};

#else

#include <pthread.h>
struct mutex {
	pthread_mutex_t mutex;
};

#endif


mutex_status_t iax_mutex_create(mutex_t **mutex)
{
	mutex_status_t status = MUTEX_FAILURE;
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	mutex_t *check = NULL;

	check = (mutex_t *)malloc(sizeof(**mutex));
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
	status = MUTEX_SUCCESS;

done:
	return status;
}

mutex_status_t iax_mutex_destroy(mutex_t *mutex)
{
#ifdef WIN32
	DeleteCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_destroy(&mutex->mutex))
		return MUTEX_FAILURE;
#endif
	free(mutex);
	return MUTEX_SUCCESS;
}

mutex_status_t iax_mutex_lock(mutex_t *mutex)
{
#ifdef WIN32
	EnterCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_lock(&mutex->mutex))
		return MUTEX_FAILURE;
#endif
	return MUTEX_SUCCESS;
}

mutex_status_t iax_mutex_trylock(mutex_t *mutex)
{
#ifdef WIN32
	if (!TryEnterCriticalSection(&mutex->mutex))
		return MUTEX_FAILURE;
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return MUTEX_FAILURE;
#endif
	return MUTEX_SUCCESS;
}

mutex_status_t iax_mutex_unlock(mutex_t *mutex)
{
#ifdef WIN32
	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex))
		return MUTEX_FAILURE;
#endif
	return MUTEX_SUCCESS;
}
