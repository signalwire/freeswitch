/* 
 * Cross Platform Thread/Mutex abstraction
 * Copyright(C) 2007 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Soozware, and permit persons to whom the Soozware is
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

#else
#include <pthread.h>
#include <poll.h>

#define ZAP_THREAD_CALLING_CONVENTION

struct zap_mutex {
	pthread_mutex_t mutex;
};

#endif

struct zap_interrupt {
	zap_socket_t device;
#ifdef WIN32
	/* for generic interruption */
	HANDLE event;
#else
	/* for generic interruption */
	int readfd;
	int writefd;
#endif
};

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

	if (!func || !(thread = (zap_thread_t *)malloc(sizeof(zap_thread_t)))) {
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

	check = (zap_mutex_t *)malloc(sizeof(**mutex));
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


OZ_DECLARE(zap_status_t) zap_interrupt_create(zap_interrupt_t **ininterrupt, zap_socket_t device)
{
	zap_interrupt_t *interrupt = NULL;
#ifndef WIN32
	int fds[2];
#endif

	interrupt = calloc(1, sizeof(*interrupt));
	if (!interrupt) {
		zap_log(ZAP_LOG_ERROR, "Failed to allocate interrupt memory\n");
		return ZAP_FAIL;
	}

	interrupt->device = device;
#ifdef WIN32
	interrupt->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!interrupt->event) {
		zap_log(ZAP_LOG_ERROR, "Failed to allocate interrupt event\n");
		goto failed;
	}
#else
	if (pipe(fds)) {
		zap_log(ZAP_LOG_ERROR, "Failed to allocate interrupt pipe: %s\n", strerror(errno));
		goto failed;
	}
	
	interrupt->readfd = fds[0];
	interrupt->writefd = fds[1];
#endif

	*ininterrupt = interrupt;
	return ZAP_SUCCESS;

failed:
	if (interrupt) {
#ifndef WIN32
		if (interrupt->readfd) {
			close(interrupt->readfd);
			close(interrupt->writefd);
			interrupt->readfd = -1;
			interrupt->writefd = -1;
		}
#endif
		zap_safe_free(interrupt);
	}
	return ZAP_FAIL;
}

#define ONE_BILLION 1000000000

OZ_DECLARE(zap_status_t) zap_interrupt_wait(zap_interrupt_t *interrupt, int ms)
{
	int num = 1;
#ifdef WIN32
	DWORD res = 0;
	HANDLE ints[2];
#else
	int res = 0;
	struct pollfd ints[2];
	char pipebuf[255];
#endif

	/* start implementation */
#ifdef WIN32
	ints[0] = interrupt->event;
	if (interrupt->device != ZAP_INVALID_SOCKET) {
		num++;
		ints[1] = interrupt->device;
	}
	res = WaitForMultipleObjects(num, ints, FALSE, ms >= 0 ? ms : INFINITE);
	switch (res) {
	case WAIT_TIMEOUT:
		return ZAP_TIMEOUT;
	case WAIT_FAILED:
	case WAIT_ABANDONED: /* is it right to fail with abandoned? */
		return ZAP_FAIL;
	default:
		if (res >= (sizeof(ints)/sizeof(ints[0]))) {
			zap_log(ZAP_LOG_ERROR, "Error waiting for openzap interrupt event (WaitForSingleObject returned %d)\n", res);
			return ZAP_FAIL;
		}
		return ZAP_SUCCESS;
	}
#else
	ints[0].fd = interrupt->readfd;
	ints[0].events = POLLIN;
	ints[0].revents = 0;

	if (interrupt->device != ZAP_INVALID_SOCKET) {
		num++;
		ints[1].fd = interrupt->device;
		ints[1].events = POLLIN;
		ints[1].revents = 0;
	}

	res = poll(ints, num, ms);
	if (res == -1) {
		zap_log(ZAP_LOG_CRIT, "interrupt poll failed (%s)\n", strerror(errno));
		return ZAP_FAIL;
	}

	if (res == 0) {
		return ZAP_TIMEOUT;
	}

	if (ints[0].revents & POLLIN) {
		res = read(ints[0].fd, pipebuf, sizeof(pipebuf));
		if (res == -1) {
			zap_log(ZAP_LOG_CRIT, "reading interrupt descriptor failed (%s)\n", strerror(errno));
		}
	}

	return ZAP_SUCCESS;
#endif
}

OZ_DECLARE(zap_status_t) zap_interrupt_signal(zap_interrupt_t *interrupt)
{
#ifdef WIN32
	if (!SetEvent(interrupt->event)) {
		zap_log(ZAP_LOG_ERROR, "Failed to signal interrupt\n");
		return ZAP_FAIL;
	}
#else
	int err;
	if ((err = write(interrupt->writefd, "w", 1)) != 1) {
		zap_log(ZAP_LOG_ERROR, "Failed to signal interrupt: %s\n", errno, strerror(errno));
		return ZAP_FAIL;
	}
#endif
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_interrupt_destroy(zap_interrupt_t **ininterrupt)
{
	zap_interrupt_t *interrupt = NULL;
	interrupt = *ininterrupt;
#ifdef WIN32
	CloseHandle(interrupt->event);
#else
	close(interrupt->readfd);
	close(interrupt->writefd);
	interrupt->readfd = -1;
	interrupt->writefd = -1;
#endif
	zap_safe_free(interrupt);
	*ininterrupt = NULL;
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_interrupt_multiple_wait(zap_interrupt_t *interrupts[], zap_size_t size, int ms)
{
#ifndef WIN32
	int i;
	int res = 0;
	int numdevices = 0;
	char pipebuf[255];
	struct pollfd ints[size*2];

	memset(&ints, 0, sizeof(ints));

	for (i = 0; i < size; i++) {
		ints[i].events = POLLIN;
		ints[i].revents = 0;
		ints[i].fd = interrupts[i]->readfd;
		if (interrupts[i]->device != ZAP_INVALID_SOCKET) {
			ints[size+numdevices].events = POLLIN;
			ints[size+numdevices].revents = 0;
			ints[size+numdevices].fd = interrupts[i]->device;
			numdevices++;
		}
	}

	res = poll(ints, size + numdevices, ms);

	if (res == -1) {
		zap_log(ZAP_LOG_CRIT, "interrupt poll failed (%s)\n", strerror(errno));
		return ZAP_FAIL;
	}

	if (res == 0) {
		return ZAP_TIMEOUT;
	}

	for (i = 0; i < size; i++) {
		if (ints[i].revents & POLLIN) {
			res = read(ints[i].fd, pipebuf, sizeof(pipebuf));
			if (res == -1) {
				zap_log(ZAP_LOG_CRIT, "reading interrupt descriptor failed (%s)\n", strerror(errno));
			}
		}
	}

#elif defined(__WINDOWS__)
	UNREFERENCED_PARAMETER(interrupts);
	UNREFERENCED_PARAMETER(size);
	UNREFERENCED_PARAMETER(ms);
#endif
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
