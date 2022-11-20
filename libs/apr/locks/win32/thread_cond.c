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
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_strings.h"
#include "fspr_arch_thread_mutex.h"
#include "fspr_arch_thread_cond.h"
#include "fspr_portable.h"

#include <limits.h>

static fspr_status_t thread_cond_cleanup(void *data)
{
    fspr_thread_cond_t *cond = data;
    CloseHandle(cond->semaphore);
    DeleteCriticalSection(&cond->csection);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_create(fspr_thread_cond_t **cond,
                                                 fspr_pool_t *pool)
{
    fspr_thread_cond_t *cv;

    cv = fspr_pcalloc(pool, sizeof(**cond));
    if (cv == NULL) {
        return APR_ENOMEM;
    }

    cv->semaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    if (cv->semaphore == NULL) {
        return fspr_get_os_error();
    }

    *cond = cv;
    cv->pool = pool;
    InitializeCriticalSection(&cv->csection);
    fspr_pool_cleanup_register(cv->pool, cv, thread_cond_cleanup,
                              fspr_pool_cleanup_null);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_destroy(fspr_thread_cond_t *cond)
{
    return fspr_pool_cleanup_run(cond->pool, cond, thread_cond_cleanup);
}

static APR_INLINE fspr_status_t _thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                      fspr_thread_mutex_t *mutex,
                                                      DWORD timeout_ms )
{
    DWORD res;
    fspr_status_t rv;
    unsigned int wake = 0;
    unsigned long generation;

    EnterCriticalSection(&cond->csection);
    cond->num_waiting++;
    generation = cond->generation;
    LeaveCriticalSection(&cond->csection);

    fspr_thread_mutex_unlock(mutex);

    do {
        res = WaitForSingleObject(cond->semaphore, timeout_ms);

        EnterCriticalSection(&cond->csection);

        if (cond->num_wake) {
            if (cond->generation != generation) {
                cond->num_wake--;
                cond->num_waiting--;
                rv = APR_SUCCESS;
                break;
            } else {
                wake = 1;
            }
        }
        else if (res != WAIT_OBJECT_0) {
            cond->num_waiting--;
            rv = APR_TIMEUP;
            break;
        }

        LeaveCriticalSection(&cond->csection);

        if (wake) {
            wake = 0;
            ReleaseSemaphore(cond->semaphore, 1, NULL);
        }
    } while (1);

    LeaveCriticalSection(&cond->csection);
    fspr_thread_mutex_lock(mutex);

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_wait(fspr_thread_cond_t *cond,
                                               fspr_thread_mutex_t *mutex)
{
    return _thread_cond_timedwait(cond, mutex, INFINITE);
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                    fspr_thread_mutex_t *mutex,
                                                    fspr_interval_time_t timeout)
{
    DWORD timeout_ms = (DWORD) fspr_time_as_msec(timeout);

    return _thread_cond_timedwait(cond, mutex, timeout_ms);
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_signal(fspr_thread_cond_t *cond)
{
    unsigned int wake = 0;

    EnterCriticalSection(&cond->csection);
    if (cond->num_waiting > cond->num_wake) {
        wake = 1;
        cond->num_wake++;
        cond->generation++;
    }
    LeaveCriticalSection(&cond->csection);

    if (wake) {
        ReleaseSemaphore(cond->semaphore, 1, NULL);
    }

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_broadcast(fspr_thread_cond_t *cond)
{
    unsigned long num_wake = 0;

    EnterCriticalSection(&cond->csection);
    if (cond->num_waiting > cond->num_wake) {
        num_wake = cond->num_waiting - cond->num_wake;
        cond->num_wake = cond->num_waiting;
        cond->generation++;
    }
    LeaveCriticalSection(&cond->csection);

    if (num_wake) {
        ReleaseSemaphore(cond->semaphore, num_wake, NULL);
    }

    return APR_SUCCESS;
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_cond)
