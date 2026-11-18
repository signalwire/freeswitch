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

#include "fspr_private.h"
#include "win32/fspr_arch_threadproc.h"
#include "fspr_thread_proc.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_portable.h"
#if APR_HAVE_PROCESS_H
#include <process.h>
#endif
#include "fspr_arch_misc.h"   

/* Chosen for us by fspr_initialize */
DWORD tls_fspr_thread = 0;

APR_DECLARE(fspr_status_t) fspr_threadattr_create(fspr_threadattr_t **new,
                                                fspr_pool_t *pool)
{
    (*new) = (fspr_threadattr_t *)fspr_palloc(pool, 
              sizeof(fspr_threadattr_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
    (*new)->detach = 0;
    (*new)->stacksize = 0;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_set(fspr_threadattr_t *attr,
                                                   fspr_int32_t on)
{
    attr->detach = on;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_get(fspr_threadattr_t *attr)
{
    if (attr->detach == 1)
        return APR_DETACH;
    return APR_NOTDETACH;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_stacksize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t stacksize)
{
    attr->stacksize = stacksize;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_guardsize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t size)
{
    return APR_ENOTIMPL;
}

static void *dummy_worker(void *opaque)
{
    fspr_thread_t *thd = (fspr_thread_t *)opaque;
    TlsSetValue(tls_fspr_thread, thd->td);
    return thd->func(thd, thd->data);
}

APR_DECLARE(fspr_status_t) fspr_thread_create(fspr_thread_t **new,
                                            fspr_threadattr_t *attr,
                                            fspr_thread_start_t func,
                                            void *data, fspr_pool_t *pool)
{
    fspr_status_t stat;
	unsigned temp;
    HANDLE handle;
    int priority = THREAD_PRIORITY_NORMAL;

    (*new) = (fspr_thread_t *)fspr_palloc(pool, sizeof(fspr_thread_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
    (*new)->data = data;
    (*new)->func = func;
    (*new)->td   = NULL;
    stat = fspr_pool_create(&(*new)->pool, pool);
    if (stat != APR_SUCCESS) {
        return stat;
    }

    if (attr && attr->priority && attr->priority > 0) {
        if (attr->priority >= 99) {
            priority = THREAD_PRIORITY_TIME_CRITICAL;
        } else if (attr->priority >= 50) {
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
        } else if (attr->priority >= 10) {
            priority = THREAD_PRIORITY_NORMAL;
        } else if (attr->priority >= 1) {
            priority = THREAD_PRIORITY_LOWEST;
        }
    }

    /* Use 0 for Thread Stack Size, because that will default the stack to the
     * same size as the calling thread. 
     */
#ifndef _WIN32_WCE
    if ((handle = (HANDLE)_beginthreadex(NULL,
                        attr && attr->stacksize > 0 ? attr->stacksize : 0,
                        (unsigned int (APR_THREAD_FUNC *)(void *))dummy_worker,
                        (*new), 0, &temp)) == 0) {
        return APR_FROM_OS_ERROR(_doserrno);
    }
#else
   if ((handle = CreateThread(NULL,
                        attr && attr->stacksize > 0 ? attr->stacksize : 0,
                        (unsigned int (APR_THREAD_FUNC *)(void *))dummy_worker,
                        (*new), 0, &temp)) == 0) {
        return fspr_get_os_error();
    }
#endif

   if (priority) {
       SetThreadPriority(handle, priority);
   }

    if (attr && attr->detach) {
        CloseHandle(handle);
    }
    else
        (*new)->td = handle;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_exit(fspr_thread_t *thd,
                                          fspr_status_t retval)
{
    thd->exitval = retval;
    fspr_pool_destroy(thd->pool);
    thd->pool = NULL;
#ifndef _WIN32_WCE
    _endthreadex(0);
#else
    ExitThread(0);
#endif
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_join(fspr_status_t *retval,
                                          fspr_thread_t *thd)
{
    fspr_status_t rv = APR_SUCCESS;
    
    if (!thd->td) {
        /* Can not join on detached threads */
        return APR_DETACH;
    }
    rv = WaitForSingleObject(thd->td, INFINITE);
    if ( rv == WAIT_OBJECT_0 || rv == WAIT_ABANDONED) {
        /* If the thread_exit has been called */
        if (!thd->pool)
            *retval = thd->exitval;
        else
            rv = APR_INCOMPLETE;
    }
    else
        rv = fspr_get_os_error();
    CloseHandle(thd->td);
    thd->td = NULL;

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_detach(fspr_thread_t *thd)
{
    if (thd->td && CloseHandle(thd->td)) {
        thd->td = NULL;
        return APR_SUCCESS;
    }
    else {
        return fspr_get_os_error();
    }
}

APR_DECLARE(void) fspr_thread_yield()
{
    /* SwitchToThread is not supported on Win9x, but since it's
     * primarily a noop (entering time consuming code, therefore
     * providing more critical threads a bit larger timeslice)
     * we won't worry too much if it's not available.
     */
#ifndef _WIN32_WCE
    if (fspr_os_level >= APR_WIN_NT) {
        SwitchToThread();
    }
#endif
}

APR_DECLARE(fspr_status_t) fspr_thread_data_get(void **data, const char *key,
                                             fspr_thread_t *thread)
{
    return fspr_pool_userdata_get(data, key, thread->pool);
}

APR_DECLARE(fspr_status_t) fspr_thread_data_set(void *data, const char *key,
                                             fspr_status_t (*cleanup) (void *),
                                             fspr_thread_t *thread)
{
    return fspr_pool_userdata_set(data, key, cleanup, thread->pool);
}


APR_DECLARE(fspr_os_thread_t) fspr_os_thread_current(void)
{
    HANDLE hthread = (HANDLE)TlsGetValue(tls_fspr_thread);
    HANDLE hproc;

    if (hthread) {
        return hthread;
    }
    
    hproc = GetCurrentProcess();
    hthread = GetCurrentThread();
    if (!DuplicateHandle(hproc, hthread, 
                         hproc, &hthread, 0, FALSE, 
                         DUPLICATE_SAME_ACCESS)) {
        return NULL;
    }
    TlsSetValue(tls_fspr_thread, hthread);
    return hthread;
}

APR_DECLARE(fspr_status_t) fspr_os_thread_get(fspr_os_thread_t **thethd,
                                            fspr_thread_t *thd)
{
    if (thd == NULL) {
        return APR_ENOTHREAD;
    }
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
        (*thd) = (fspr_thread_t *)fspr_palloc(pool, sizeof(fspr_thread_t));
        (*thd)->pool = pool;
    }
    (*thd)->td = thethd;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_once_init(fspr_thread_once_t **control,
                                               fspr_pool_t *p)
{
    (*control) = fspr_pcalloc(p, sizeof(**control));
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_once(fspr_thread_once_t *control,
                                          void (*func)(void))
{
    if (!InterlockedExchange(&control->value, 1)) {
        func();
    }
    return APR_SUCCESS;
}

APR_DECLARE(int) fspr_os_thread_equal(fspr_os_thread_t tid1,
                                     fspr_os_thread_t tid2)
{
    /* Since the only tid's we support our are own, and
     * fspr_os_thread_current returns the identical handle
     * to the one we created initially, the test is simple.
     */
    return (tid1 == tid2);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread)
