/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fspr.h"
#include "fspr_portable.h"
#include "fspr_arch_threadproc.h"

#if APR_HAS_THREADS

#if APR_HAVE_PTHREAD_H

/* Destroy the threadattr object */
static fspr_status_t threadattr_cleanup(void *data)
{
    fspr_threadattr_t *attr = data;
    fspr_status_t rv;

    rv = pthread_attr_destroy(&attr->attr);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_create(fspr_threadattr_t **new,
                                                fspr_pool_t *pool)
{
    fspr_status_t stat;

    (*new) = fspr_palloc(pool, sizeof(fspr_threadattr_t));
    (*new)->pool = pool;
    stat = pthread_attr_init(&(*new)->attr);

    if (stat == 0) {
        fspr_pool_cleanup_register(pool, *new, threadattr_cleanup,
                                  fspr_pool_cleanup_null);
        return APR_SUCCESS;
    }
#ifdef PTHREAD_SETS_ERRNO
    stat = errno;
#endif

    return stat;
}

#define DETACH_ARG(v) ((v) ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE)

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_set(fspr_threadattr_t *attr,
                                                    fspr_int32_t on)
{
    fspr_status_t stat;
#ifdef PTHREAD_ATTR_SETDETACHSTATE_ARG2_ADDR
    int arg = DETACH_ARG(v);

    if ((stat = pthread_attr_setdetachstate(&attr->attr, &arg)) == 0) {
#else
    if ((stat = pthread_attr_setdetachstate(&attr->attr, 
                                            DETACH_ARG(on))) == 0) {
#endif
        return APR_SUCCESS;
    }
    else {
#ifdef PTHREAD_SETS_ERRNO
        stat = errno;
#endif

        return stat;
    }
}

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_get(fspr_threadattr_t *attr)
{
    int state;

#ifdef PTHREAD_ATTR_GETDETACHSTATE_TAKES_ONE_ARG
    state = pthread_attr_getdetachstate(&attr->attr);
#else
    pthread_attr_getdetachstate(&attr->attr, &state);
#endif
    if (state == 1)
        return APR_DETACH;
    return APR_NOTDETACH;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_stacksize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t stacksize)
{
    int stat;

    stat = pthread_attr_setstacksize(&attr->attr, stacksize);
    if (stat == 0) {
        return APR_SUCCESS;
    }
#ifdef PTHREAD_SETS_ERRNO
    stat = errno;
#endif

    return stat;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_guardsize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t size)
{
#ifdef HAVE_PTHREAD_ATTR_SETGUARDSIZE
    fspr_status_t rv;

    rv = pthread_attr_setguardsize(&attr->attr, size);
    if (rv == 0) {
        return APR_SUCCESS;
    }
#ifdef PTHREAD_SETS_ERRNO
    rv = errno;
#endif
    return rv;
#else
    return APR_ENOTIMPL;
#endif
}

static void *dummy_worker(void *opaque)
{
    fspr_thread_t *thread = (fspr_thread_t*)opaque;

#ifdef HAVE_PTHREAD_SETSCHEDPARAM
	if (thread->priority) {
		int policy;
		struct sched_param param = { 0 };
		pthread_t tt = pthread_self();
	
		pthread_getschedparam(tt, &policy, &param);
		param.sched_priority = thread->priority;
		pthread_setschedparam(tt, policy, &param);
	}
#endif

    return thread->func(thread, thread->data);
}

APR_DECLARE(fspr_status_t) fspr_thread_create(fspr_thread_t **new,
                                            fspr_threadattr_t *attr,
                                            fspr_thread_start_t func,
                                            void *data,
                                            fspr_pool_t *pool)
{
    fspr_status_t stat;
    pthread_attr_t *temp;
	pthread_t tt;

    (*new) = (fspr_thread_t *)fspr_pcalloc(pool, sizeof(fspr_thread_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->td = (pthread_t *)fspr_pcalloc(pool, sizeof(pthread_t));

    if ((*new)->td == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
    (*new)->data = data;
    (*new)->func = func;

    if (attr)
        temp = &attr->attr;
    else
        temp = NULL;

    stat = fspr_pool_create(&(*new)->pool, pool);
    if (stat != APR_SUCCESS) {
        return stat;
    }

	if (attr && attr->priority) {
		(*new)->priority = attr->priority;
	}

    if ((stat = pthread_create(&tt, temp, dummy_worker, (*new))) == 0) {
		*(*new)->td = tt;

        return APR_SUCCESS;
    }
    else {
#ifdef PTHREAD_SETS_ERRNO
        stat = errno;
#endif

        return stat;
    }
}

APR_DECLARE(fspr_os_thread_t) fspr_os_thread_current(void)
{
    return pthread_self();
}

APR_DECLARE(int) fspr_os_thread_equal(fspr_os_thread_t tid1,
                                     fspr_os_thread_t tid2)
{
    return pthread_equal(tid1, tid2);
}

APR_DECLARE(fspr_status_t) fspr_thread_exit(fspr_thread_t *thd,
                                          fspr_status_t retval)
{
    thd->exitval = retval;
    fspr_pool_destroy(thd->pool);
    pthread_exit(NULL);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_join(fspr_status_t *retval,
                                          fspr_thread_t *thd)
{
    fspr_status_t stat;
    fspr_status_t *thread_stat;

    if ((stat = pthread_join(*thd->td,(void *)&thread_stat)) == 0) {
        *retval = thd->exitval;
        return APR_SUCCESS;
    }
    else {
#ifdef PTHREAD_SETS_ERRNO
        stat = errno;
#endif

        return stat;
    }
}

APR_DECLARE(fspr_status_t) fspr_thread_detach(fspr_thread_t *thd)
{
    fspr_status_t stat;

#ifdef PTHREAD_DETACH_ARG1_ADDR
    if ((stat = pthread_detach(thd->td)) == 0) {
#else
    if ((stat = pthread_detach(*thd->td)) == 0) {
#endif

        return APR_SUCCESS;
    }
    else {
#ifdef PTHREAD_SETS_ERRNO
        stat = errno;
#endif

        return stat;
    }
}

void fspr_thread_yield()
{
}

APR_DECLARE(fspr_status_t) fspr_thread_data_get(void **data, const char *key,
                                              fspr_thread_t *thread)
{
    return fspr_pool_userdata_get(data, key, thread->pool);
}

APR_DECLARE(fspr_status_t) fspr_thread_data_set(void *data, const char *key,
                              fspr_status_t (*cleanup)(void *),
                              fspr_thread_t *thread)
{
    return fspr_pool_userdata_set(data, key, cleanup, thread->pool);
}

APR_DECLARE(fspr_status_t) fspr_os_thread_get(fspr_os_thread_t **thethd,
                                            fspr_thread_t *thd)
{
    *thethd = thd->td;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_thread_put(fspr_thread_t **thd,
                                            fspr_os_thread_t *thethd,
                                            fspr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }

    if ((*thd) == NULL) {
        (*thd) = (fspr_thread_t *)fspr_pcalloc(pool, sizeof(fspr_thread_t));
        (*thd)->pool = pool;
    }

    (*thd)->td = thethd;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_once_init(fspr_thread_once_t **control,
                                               fspr_pool_t *p)
{
    static const pthread_once_t once_init = PTHREAD_ONCE_INIT;

    *control = fspr_palloc(p, sizeof(**control));
    (*control)->once = once_init;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_once(fspr_thread_once_t *control,
                                          void (*func)(void))
{
    return pthread_once(&control->once, func);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread)

#endif  /* HAVE_PTHREAD_H */
#endif  /* APR_HAS_THREADS */

#if !APR_HAS_THREADS

/* avoid warning for no prototype */
APR_DECLARE(fspr_status_t) fspr_os_thread_get(void);

APR_DECLARE(fspr_status_t) fspr_os_thread_get(void)
{
    return APR_ENOTIMPL;
}

#endif
