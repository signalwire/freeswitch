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
#   if (_WIN32_WINNT < 0x0400)
#       error "Need to target at least Windows 95/WINNT 4.0 because TryEnterCriticalSection is needed"
#   endif
#   include <windows.h>
#endif
/*#define FTDM_DEBUG_MUTEX 0*/

#include "private/ftdm_core.h"
#include "ftdm_threadmutex.h"

#ifdef WIN32
#include <process.h>

#define FTDM_THREAD_CALLING_CONVENTION __stdcall

struct ftdm_mutex {
	CRITICAL_SECTION mutex;
};

#else
#include <pthread.h>
#include <poll.h>

#define FTDM_THREAD_CALLING_CONVENTION

#ifdef FTDM_DEBUG_MUTEX
#define FTDM_MUTEX_MAX_REENTRANCY 30
typedef struct ftdm_lock_entry {
	const char *file;
	const char *func;
	uint32_t line;
} ftdm_lock_entry_t;

typedef struct ftdm_lock_history {
	ftdm_lock_entry_t locked;
	ftdm_lock_entry_t unlocked;
} ftdm_lock_history_t;
#endif

struct ftdm_mutex {
	pthread_mutex_t mutex;
#ifdef FTDM_DEBUG_MUTEX
	ftdm_lock_history_t lock_history[FTDM_MUTEX_MAX_REENTRANCY];
	uint8_t reentrancy;
#endif
};

#endif

struct ftdm_interrupt {
	ftdm_socket_t device;
	ftdm_wait_flag_t device_input_flags;
	ftdm_wait_flag_t device_output_flags;
#ifdef WIN32
	/* for generic interruption */
	HANDLE event;
#else
	/* In theory we could be using thread conditions for generic interruption,
	 * however, Linux does not have a primitive like Windows WaitForMultipleObjects
	 * to wait for both thread condition and file descriptors, therefore we decided
	 * to use a dummy pipe for generic interruption/condition logic
	 * */
	int readfd;
	int writefd;
#endif
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

	if (!func || !(thread = (ftdm_thread_t *)ftdm_calloc(1, sizeof(ftdm_thread_t)))) {
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

	check = (ftdm_mutex_t *)ftdm_calloc(1, sizeof(**mutex));
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

#define ADD_LOCK_HISTORY(mutex, file, line, func) \
	{ \
		if ((mutex)->reentrancy < FTDM_MUTEX_MAX_REENTRANCY) { \
			(mutex)->lock_history[mutex->reentrancy].locked.file = (file); \
			(mutex)->lock_history[mutex->reentrancy].locked.func = (func); \
			(mutex)->lock_history[mutex->reentrancy].locked.line = (line); \
			(mutex)->lock_history[mutex->reentrancy].unlocked.file = NULL; \
			(mutex)->lock_history[mutex->reentrancy].unlocked.func = NULL; \
			(mutex)->lock_history[mutex->reentrancy].unlocked.line = 0; \
			(mutex)->reentrancy++; \
			if ((mutex)->reentrancy == FTDM_MUTEX_MAX_REENTRANCY) { \
				ftdm_log((file), (func), (line), FTDM_LOG_LEVEL_ERROR, "Max reentrancy reached for mutex %p\n", (mutex)); \
			} \
		} \
	}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_lock(const char *file, int line, const char *func, ftdm_mutex_t *mutex)
{
#ifdef WIN32
	ftdm_unused_arg(file);
	ftdm_unused_arg(line);
	ftdm_unused_arg(func);

	EnterCriticalSection(&mutex->mutex);
#else
	int err;
	if ((err = pthread_mutex_lock(&mutex->mutex))) {
		ftdm_log(file, func, line, FTDM_LOG_LEVEL_ERROR, "Failed to lock mutex %d:%s\n", err, strerror(err));
		return FTDM_FAIL;
	}
#endif
#ifdef FTDM_DEBUG_MUTEX
	ADD_LOCK_HISTORY(mutex, file, line, func);
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_trylock(const char *file, int line, const char *func, ftdm_mutex_t *mutex)
{
	ftdm_unused_arg(file);
	ftdm_unused_arg(line);
	ftdm_unused_arg(func);
#ifdef WIN32
	if (!TryEnterCriticalSection(&mutex->mutex))
		return FTDM_FAIL;
#else
	if (pthread_mutex_trylock(&mutex->mutex))
		return FTDM_FAIL;
#endif
#ifdef FTDM_DEBUG_MUTEX
	ADD_LOCK_HISTORY(mutex, file, line, func);
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_mutex_unlock(const char *file, int line, const char *func, ftdm_mutex_t *mutex)
{
#ifdef FTDM_DEBUG_MUTEX
	int i = 0;
	if (mutex->reentrancy == 0) {
		ftdm_log(file, func, line, FTDM_LOG_LEVEL_ERROR, "Cannot unlock something that is not locked!\n");
		return FTDM_FAIL;
	}
	i = mutex->reentrancy - 1;
	/* I think this is a fair assumption when debugging */
	if (func != mutex->lock_history[i].locked.func) {
		ftdm_log(file, func, line, FTDM_LOG_LEVEL_WARNING, "Mutex %p was suspiciously locked at %s->%s:%d but unlocked at %s->%s:%d!\n",
				mutex, mutex->lock_history[i].locked.func, mutex->lock_history[i].locked.file, mutex->lock_history[i].locked.line, 
				func, file, line);
	}
	mutex->lock_history[i].unlocked.file = file;
	mutex->lock_history[i].unlocked.line = line;
	mutex->lock_history[i].unlocked.func = func;
	mutex->reentrancy--;
#endif
#ifdef WIN32
	ftdm_unused_arg(file);
	ftdm_unused_arg(line);
	ftdm_unused_arg(func);

	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex)) {
		ftdm_log(file, func, line, FTDM_LOG_LEVEL_ERROR, "Failed to unlock mutex: %s\n", strerror(errno));
#ifdef FTDM_DEBUG_MUTEX
		mutex->reentrancy++;
#endif
		return FTDM_FAIL;
	}
#endif
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_interrupt_create(ftdm_interrupt_t **ininterrupt, ftdm_socket_t device, ftdm_wait_flag_t device_flags)
{
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_interrupt_t *interrupt = NULL;
#ifndef WIN32
	int fds[2];
#endif

	ftdm_assert_return(ininterrupt != NULL, FTDM_FAIL, "interrupt double pointer is null!\n");

	interrupt = ftdm_calloc(1, sizeof(*interrupt));
	if (!interrupt) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to allocate interrupt memory\n");
		return FTDM_ENOMEM;
	}

	interrupt->device = device;
	interrupt->device_input_flags = device_flags;
#ifdef WIN32
	interrupt->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!interrupt->event) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to allocate interrupt event\n");
		status = FTDM_ENOMEM;
		goto failed;
	}
#else
	if (pipe(fds)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to allocate interrupt pipe: %s\n", strerror(errno));
		status = FTDM_FAIL;
		goto failed;
	}
	interrupt->readfd = fds[0];
	interrupt->writefd = fds[1];
#endif

	*ininterrupt = interrupt;
	return FTDM_SUCCESS;

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
		ftdm_safe_free(interrupt);
	}
	return status;
}

#define ONE_BILLION 1000000000

FT_DECLARE(ftdm_status_t) ftdm_interrupt_wait(ftdm_interrupt_t *interrupt, int ms)
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

	ftdm_assert_return(interrupt != NULL, FTDM_FAIL, "Interrupt is null!\n");

	interrupt->device_output_flags = FTDM_NO_FLAGS;
	/* start implementation */
#ifdef WIN32
	ints[0] = interrupt->event;
	if (interrupt->device != FTDM_INVALID_SOCKET) {
		num++;
		ints[1] = interrupt->device;
		ftdm_log(FTDM_LOG_CRIT, "implement me! (Windows support for device_output_flags member!)\n");
	}
	res = WaitForMultipleObjects(num, ints, FALSE, ms >= 0 ? ms : INFINITE);
	switch (res) {
	case WAIT_TIMEOUT:
		return FTDM_TIMEOUT;
	case WAIT_FAILED:
	case WAIT_ABANDONED: /* is it right to fail with abandoned? */
		return FTDM_FAIL;
	default:
		if (res >= (sizeof(ints)/sizeof(ints[0]))) {
			ftdm_log(FTDM_LOG_ERROR, "Error waiting for freetdm interrupt event (WaitForSingleObject returned %d)\n", res);
			return FTDM_FAIL;
		}
		return FTDM_SUCCESS;
	}
#else
pollagain:
	ints[0].fd = interrupt->readfd;
	ints[0].events = POLLIN;
	ints[0].revents = 0;

	if (interrupt->device != FTDM_INVALID_SOCKET) {
		num++;
		ints[1].fd = interrupt->device;
		ints[1].events = interrupt->device_input_flags;
		ints[1].revents = 0;
	}

	res = poll(ints, num, ms);

	if (res == -1) {
		if (errno == EINTR) {
			goto pollagain;
		}
		ftdm_log(FTDM_LOG_CRIT, "interrupt poll failed (%s)\n", strerror(errno));
		return FTDM_FAIL;
	}

	if (res == 0) {
		return FTDM_TIMEOUT;
	}

	if (ints[0].revents & POLLIN) {
		res = read(ints[0].fd, pipebuf, sizeof(pipebuf));
		if (res == -1) {
			ftdm_log(FTDM_LOG_CRIT, "reading interrupt descriptor failed (%s)\n", strerror(errno));
		}
	}
	if (interrupt->device != FTDM_INVALID_SOCKET) {
		if (ints[1].revents & POLLIN) {
			interrupt->device_output_flags |= FTDM_READ;
		}
		if (ints[1].revents & POLLOUT) {
			interrupt->device_output_flags |= FTDM_WRITE;
		}
		if (ints[1].revents & POLLPRI) {
			interrupt->device_output_flags |= FTDM_EVENTS;
		}
	}
	return FTDM_SUCCESS;
#endif
}

FT_DECLARE(ftdm_status_t) ftdm_interrupt_signal(ftdm_interrupt_t *interrupt)
{
	ftdm_assert_return(interrupt != NULL, FTDM_FAIL, "Interrupt is null!\n");
#ifdef WIN32
	if (!SetEvent(interrupt->event)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to signal interrupt\n");
		return FTDM_FAIL;

	}
#else
	int err;
	struct pollfd testpoll;
	testpoll.revents = 0;
	testpoll.events = POLLIN;
	testpoll.fd = interrupt->readfd;
	err = poll(&testpoll, 1, 0);
	if (err == 0 && !(testpoll.revents & POLLIN)) {
		/* we just try to notify if there is nothing on the read fd already, 
		 * otherwise users that never call interrupt wait eventually will 
		 * eventually have the pipe buffer filled */
		if ((err = write(interrupt->writefd, "w", 1)) != 1) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to signal interrupt: %s\n", strerror(errno));
			return FTDM_FAIL;
		}
	}
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_interrupt_destroy(ftdm_interrupt_t **ininterrupt)
{
	ftdm_interrupt_t *interrupt = NULL;
	ftdm_assert_return(ininterrupt != NULL, FTDM_FAIL, "Interrupt null when destroying!\n");
	interrupt = *ininterrupt;
#ifdef WIN32
	CloseHandle(interrupt->event);
#else
	close(interrupt->readfd);
	close(interrupt->writefd);

	interrupt->readfd = -1;
	interrupt->writefd = -1;
#endif
	ftdm_safe_free(interrupt);
	*ininterrupt = NULL;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_interrupt_multiple_wait(ftdm_interrupt_t *interrupts[], ftdm_size_t size, int ms)
{
	int numdevices = 0;
	unsigned i = 0;

#if defined(__WINDOWS__)
	DWORD res = 0;
	HANDLE ints[20];
	if (size > (ftdm_array_len(ints)/2)) {
		/* improve if needed: dynamically allocate the list of interrupts *only* when exceeding the default size */
		ftdm_log(FTDM_LOG_CRIT, "Unsupported size of interrupts: %d, implement me!\n", size);
		return FTDM_FAIL;
	}

	for (i = 0; i < size; i++) {
		ints[i] = interrupts[i]->event;
		interrupts[i]->device_output_flags = FTDM_NO_FLAGS;
		if (interrupts[i]->device != FTDM_INVALID_SOCKET) {
			/* WARNING: if the device is ready for data we must implement for Windows the device_output_flags member */
			ints[size+numdevices] = interrupts[i]->device;
			numdevices++;
			ftdm_log(FTDM_LOG_CRIT, "implement me! (Windows support for device_data_ready member!)\n", size);
		}
	}

	res = WaitForMultipleObjects((DWORD)size+numdevices, ints, FALSE, ms >= 0 ? ms : INFINITE);

	switch (res) {
	case WAIT_TIMEOUT:
		return FTDM_TIMEOUT;
	case WAIT_FAILED:
	case WAIT_ABANDONED: /* is it right to fail with abandoned? */
		return FTDM_FAIL;
	default:
		if (res >= (size+numdevices)) {
			ftdm_log(FTDM_LOG_ERROR, "Error waiting for freetdm interrupt event (WaitForSingleObject returned %d)\n", res);
			return FTDM_FAIL;
		}
		/* fall-through to FTDM_SUCCESS at the end of the function */
	}
#elif defined(__linux__) || defined(__FreeBSD__)
	int res = 0;
	char pipebuf[255];
	struct pollfd ints[size*2];

	memset(&ints, 0, sizeof(ints));
pollagain:
	for (i = 0; i < size; i++) {
		ints[i].events = POLLIN;
		ints[i].revents = 0;
		ints[i].fd = interrupts[i]->readfd;
		interrupts[i]->device_output_flags = FTDM_NO_FLAGS;
		if (interrupts[i]->device != FTDM_INVALID_SOCKET) {
			ints[size+numdevices].events = interrupts[i]->device_input_flags;
			ints[size+numdevices].revents = 0;
			ints[size+numdevices].fd = interrupts[i]->device;
			numdevices++;
		}
	}

	res = poll(ints, size + numdevices, ms);
	if (res == -1) {
		if (errno == EINTR) {
			goto pollagain;
		}
		ftdm_log(FTDM_LOG_CRIT, "interrupt poll failed (%s)\n", strerror(errno));
		return FTDM_FAIL;
	}

	if (res == 0) {
		return FTDM_TIMEOUT;
	}

	/* check for events in the pipes and in the devices, but service only the pipes  */
	numdevices = 0;
	for (i = 0; i < size; i++) {
		if (ints[i].revents & POLLIN) {
			res = read(ints[i].fd, pipebuf, sizeof(pipebuf));
			if (res == -1) {
				ftdm_log(FTDM_LOG_CRIT, "reading interrupt descriptor failed (%s)\n", strerror(errno));
			}
		}
		if (interrupts[i]->device != FTDM_INVALID_SOCKET) {
			if (ints[size+numdevices].revents & POLLIN) {
				interrupts[i]->device_output_flags |= FTDM_READ;
			}
			if (ints[size+numdevices].revents & POLLOUT) {
				interrupts[i]->device_output_flags |= FTDM_WRITE;
			}
			if (ints[size+numdevices].revents & POLLPRI) {
				interrupts[i]->device_output_flags |= FTDM_EVENTS;
			}
			numdevices++;
		}
	}
#else
	/* for MacOS compilation, unused vars */
	numdevices = i;
#endif
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_wait_flag_t) ftdm_interrupt_device_ready(ftdm_interrupt_t *interrupt)
{
#if defined(__WINDOWS__)
	/* device output flags are not currently filled for Windows upon returning from a wait function */
	ftdm_log(FTDM_LOG_CRIT, "IMPLEMENT ME!\n");
#endif
	return interrupt->device_output_flags;
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
