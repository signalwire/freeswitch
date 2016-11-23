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

#include "ks.h"

size_t thread_default_stacksize = 240 * 1024;

#ifndef WIN32
pthread_once_t init_priority = PTHREAD_ONCE_INIT;
#endif

KS_DECLARE(ks_thread_os_handle_t) ks_thread_os_handle(ks_thread_t *thread)
{
	return thread->handle;
}

KS_DECLARE(ks_thread_os_handle_t) ks_thread_self(void)
{
#ifdef WIN32
	return GetCurrentThread();
#else
	return pthread_self();
#endif
}

static void ks_thread_init_priority(void)
{
#ifdef WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
#ifdef USE_SCHED_SETSCHEDULER
    /*
     * Try to use a round-robin scheduler
     * with a fallback if that does not work
     */
    struct sched_param sched = { 0 };
    sched.sched_priority = KS_PRI_LOW;
    if (sched_setscheduler(0, SCHED_FIFO, &sched)) {
        sched.sched_priority = 0;
        if (sched_setscheduler(0, SCHED_OTHER, &sched)) {
            return;
        }
    }
#endif
#endif
    return;
}

void ks_thread_override_default_stacksize(size_t size)
{
	thread_default_stacksize = size;
}

static void ks_thread_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	ks_thread_t *thread = (ks_thread_t *) ptr;

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		thread->running = 0;
		break;
	case KS_MPCL_TEARDOWN:
		if (!(thread->flags & KS_THREAD_FLAG_DETATCHED)) {
			ks_thread_join(thread);
		}
		break;
	case KS_MPCL_DESTROY:
#ifdef WIN32
		if (!(thread->flags & KS_THREAD_FLAG_DETATCHED)) {
			CloseHandle(thread->handle);
		}
#endif
		break;
	}
}

static void *KS_THREAD_CALLING_CONVENTION thread_launch(void *args)
{
	ks_thread_t *thread = (ks_thread_t *) args;

#ifdef HAVE_PTHREAD_SETSCHEDPARAM
	if (thread->priority) {
		int policy = SCHED_FIFO;
		struct sched_param param = { 0 };
		pthread_t tt = pthread_self();

		pthread_once(&init_priority, ks_thread_init_priority);
		pthread_getschedparam(tt, &policy, &param);
		param.sched_priority = thread->priority;
		pthread_setschedparam(tt, policy, &param);
	}
#endif

	thread->return_data = thread->function(thread, thread->private_data);
#ifndef WIN32
	pthread_attr_destroy(&thread->attribute);
#endif

	return thread->return_data;
}

KS_DECLARE(int) ks_thread_set_priority(int nice_val)
{
#ifdef WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
#ifdef USE_SCHED_SETSCHEDULER
    /*
     * Try to use a round-robin scheduler
     * with a fallback if that does not work
     */
    struct sched_param sched = { 0 };
    sched.sched_priority = KS_PRI_LOW;
    if (sched_setscheduler(0, SCHED_FIFO, &sched)) {
        sched.sched_priority = 0;
        if (sched_setscheduler(0, SCHED_OTHER, &sched)) {
            return -1;
        }
    }
#endif

	if (nice_val) {
#ifdef HAVE_SETPRIORITY
		/*
		 * setpriority() works on FreeBSD (6.2), nice() doesn't
		 */
		if (setpriority(PRIO_PROCESS, getpid(), nice_val) < 0) {
			ks_log(KS_LOG_CRIT, "Could not set nice level\n");
			return -1;
		}
#else
		if (nice(nice_val) != nice_val) {
			ks_log(KS_LOG_CRIT, "Could not set nice level\n");
			return -1;
		}
#endif
	}
#endif

    return 0;
}

KS_DECLARE(uint8_t) ks_thread_priority(ks_thread_t *thread) {
	uint8_t priority = 0;
#ifdef WIN32
	DWORD pri = GetThreadPriority(thread->handle);

	if (pri >= THREAD_PRIORITY_TIME_CRITICAL) {
		priority = 99;
	} else if (pri >= THREAD_PRIORITY_ABOVE_NORMAL) {
		priority = 50;
	} else {
		priority = 10;
	}
#else
	int policy;
	struct sched_param param = { 0 };

	pthread_getschedparam(thread->handle, &policy, &param);
	priority = param.sched_priority;
#endif
	return priority;
}

KS_DECLARE(ks_status_t) ks_thread_join(ks_thread_t *thread) {
#ifdef WIN32
	WaitForSingleObject(thread->handle, INFINITE);
#else
	void *ret;
	pthread_join(thread->handle, &ret);
#endif
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_thread_create_ex(ks_thread_t **rthread, ks_thread_function_t func, void *data,
										 uint32_t flags, size_t stack_size, ks_thread_priority_t priority, ks_pool_t *pool)
{
	ks_thread_t *thread = NULL;
	ks_status_t status = KS_STATUS_FAIL;

	if (!rthread) goto done;

	*rthread = NULL;

	if (!func || !pool) goto done;

	thread = (ks_thread_t *) ks_pool_alloc(pool, sizeof(ks_thread_t));

	if (!thread) goto done;

	thread->private_data = data;
	thread->function = func;
	thread->stack_size = stack_size;
	thread->running = 1;
	thread->flags = flags;
	thread->priority = priority;
	thread->pool = pool;

#if defined(WIN32)
	thread->handle = (void *) _beginthreadex(NULL, (unsigned) thread->stack_size, (unsigned int (__stdcall *) (void *)) thread_launch, thread, 0, NULL);

	if (!thread->handle) {
		goto fail;
	}

	if (priority >= 99) {
		SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL);
	} else if (priority >= 50) {
		SetThreadPriority(thread->handle, THREAD_PRIORITY_ABOVE_NORMAL);
	} else if (priority >= 10) {
		SetThreadPriority(thread->handle, THREAD_PRIORITY_NORMAL);
	} else if (priority >= 1) {
		SetThreadPriority(thread->handle, THREAD_PRIORITY_LOWEST);
	}

	if (flags & KS_THREAD_FLAG_DETATCHED) {
		CloseHandle(thread->handle);
	}

	status = KS_STATUS_SUCCESS;
	goto done;
#else

	if (pthread_attr_init(&thread->attribute) != 0)
		goto fail;

	if ((flags & KS_THREAD_FLAG_DETATCHED) && pthread_attr_setdetachstate(&thread->attribute, PTHREAD_CREATE_DETACHED) != 0)
		goto failpthread;

	if (thread->stack_size && pthread_attr_setstacksize(&thread->attribute, thread->stack_size) != 0)
		goto failpthread;

	if (pthread_create(&thread->handle, &thread->attribute, thread_launch, thread) != 0)
		goto failpthread;

	status = KS_STATUS_SUCCESS;
	goto done;

  failpthread:

	pthread_attr_destroy(&thread->attribute);
#endif

  fail:
	if (thread) {
		thread->running = 0;
		if (pool) {
			ks_pool_safe_free(pool, thread);
		}
	}
  done:
	if (status == KS_STATUS_SUCCESS) {
		*rthread = thread;
		ks_pool_set_cleanup(pool, thread, NULL, 0, ks_thread_cleanup);
	}

	return status;
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
