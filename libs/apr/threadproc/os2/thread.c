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

#define INCL_DOSERRORS
#define INCL_DOS
#include "fspr_arch_threadproc.h"
#include "fspr_thread_proc.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_portable.h"
#include "fspr_arch_file_io.h"
#include <stdlib.h>

APR_DECLARE(fspr_status_t) fspr_threadattr_create(fspr_threadattr_t **new, fspr_pool_t *pool)
{
    (*new) = (fspr_threadattr_t *)fspr_palloc(pool, sizeof(fspr_threadattr_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
    (*new)->attr = 0;
    (*new)->stacksize = 0;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_threadattr_detach_set(fspr_threadattr_t *attr, fspr_int32_t on)
{
    attr->attr |= APR_THREADATTR_DETACHED;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_threadattr_detach_get(fspr_threadattr_t *attr)
{
    return (attr->attr & APR_THREADATTR_DETACHED) ? APR_DETACH : APR_NOTDETACH;
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

static void fspr_thread_begin(void *arg)
{
  fspr_thread_t *thread = (fspr_thread_t *)arg;
  thread->exitval = thread->func(thread, thread->data);
}



APR_DECLARE(fspr_status_t) fspr_thread_create(fspr_thread_t **new, fspr_threadattr_t *attr, 
                                            fspr_thread_start_t func, void *data, 
                                            fspr_pool_t *pool)
{
    fspr_status_t stat;
    fspr_thread_t *thread;
 
    thread = (fspr_thread_t *)fspr_palloc(pool, sizeof(fspr_thread_t));
    *new = thread;

    if (thread == NULL) {
        return APR_ENOMEM;
    }

    thread->pool = pool;
    thread->attr = attr;
    thread->func = func;
    thread->data = data;
    stat = fspr_pool_create(&thread->pool, pool);
    
    if (stat != APR_SUCCESS) {
        return stat;
    }

    if (attr == NULL) {
        stat = fspr_threadattr_create(&thread->attr, thread->pool);
        
        if (stat != APR_SUCCESS) {
            return stat;
        }
    }

    thread->tid = _beginthread(fspr_thread_begin, NULL, 
                               thread->attr->stacksize > 0 ?
                               thread->attr->stacksize : APR_THREAD_STACKSIZE,
                               thread);
        
    if (thread->tid < 0) {
        return errno;
    }

    return APR_SUCCESS;
}



APR_DECLARE(fspr_os_thread_t) fspr_os_thread_current()
{
    PIB *ppib;
    TIB *ptib;
    DosGetInfoBlocks(&ptib, &ppib);
    return ptib->tib_ptib2->tib2_ultid;
}



APR_DECLARE(fspr_status_t) fspr_thread_exit(fspr_thread_t *thd, fspr_status_t retval)
{
    thd->exitval = retval;
    _endthread();
    return -1; /* If we get here something's wrong */
}



APR_DECLARE(fspr_status_t) fspr_thread_join(fspr_status_t *retval, fspr_thread_t *thd)
{
    ULONG rc;
    TID waittid = thd->tid;

    if (thd->attr->attr & APR_THREADATTR_DETACHED)
        return APR_EINVAL;

    rc = DosWaitThread(&waittid, DCWW_WAIT);

    if (rc == ERROR_INVALID_THREADID)
        rc = 0; /* Thread had already terminated */

    *retval = thd->exitval;
    return APR_OS2_STATUS(rc);
}



APR_DECLARE(fspr_status_t) fspr_thread_detach(fspr_thread_t *thd)
{
    thd->attr->attr |= APR_THREADATTR_DETACHED;
    return APR_SUCCESS;
}



void fspr_thread_yield()
{
    DosSleep(0);
}



APR_DECLARE(fspr_status_t) fspr_os_thread_get(fspr_os_thread_t **thethd, fspr_thread_t *thd)
{
    *thethd = &thd->tid;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_os_thread_put(fspr_thread_t **thd, fspr_os_thread_t *thethd, 
                                            fspr_pool_t *pool)
{
    if ((*thd) == NULL) {
        (*thd) = (fspr_thread_t *)fspr_pcalloc(pool, sizeof(fspr_thread_t));
        (*thd)->pool = pool;
    }
    (*thd)->tid = *thethd;
    return APR_SUCCESS;
}



int fspr_os_thread_equal(fspr_os_thread_t tid1, fspr_os_thread_t tid2)
{
    return tid1 == tid2;
}



APR_DECLARE(fspr_status_t) fspr_thread_data_get(void **data, const char *key, fspr_thread_t *thread)
{
    return fspr_pool_userdata_get(data, key, thread->pool);
}



APR_DECLARE(fspr_status_t) fspr_thread_data_set(void *data, const char *key,
                                              fspr_status_t (*cleanup) (void *),
                                              fspr_thread_t *thread)
{
    return fspr_pool_userdata_set(data, key, cleanup, thread->pool);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread)



static fspr_status_t thread_once_cleanup(void *vcontrol)
{
    fspr_thread_once_t *control = (fspr_thread_once_t *)vcontrol;

    if (control->sem) {
        DosCloseEventSem(control->sem);
    }

    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_thread_once_init(fspr_thread_once_t **control,
                                               fspr_pool_t *p)
{
    ULONG rc;
    *control = (fspr_thread_once_t *)fspr_pcalloc(p, sizeof(fspr_thread_once_t));
    rc = DosCreateEventSem(NULL, &(*control)->sem, 0, TRUE);
    fspr_pool_cleanup_register(p, control, thread_once_cleanup, fspr_pool_cleanup_null);
    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_thread_once(fspr_thread_once_t *control, 
                                          void (*func)(void))
{
    if (!control->hit) {
        ULONG count, rc;
        rc = DosResetEventSem(control->sem, &count);

        if (rc == 0 && count) {
            control->hit = 1;
            func();
        }
    }

    return APR_SUCCESS;
}
