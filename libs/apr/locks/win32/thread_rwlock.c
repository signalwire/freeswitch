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
#include "win32/fspr_arch_thread_rwlock.h"
#include "fspr_portable.h"

static fspr_status_t thread_rwlock_cleanup(void *data)
{
    fspr_thread_rwlock_t *rwlock = data;
    
    if (! CloseHandle(rwlock->read_event))
        return fspr_get_os_error();

	DeleteCriticalSection(&rwlock->read_section);

    if (! CloseHandle(rwlock->write_mutex))
        return fspr_get_os_error();
    
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t)fspr_thread_rwlock_create(fspr_thread_rwlock_t **rwlock,
                                                  fspr_pool_t *pool)
{
    *rwlock = fspr_palloc(pool, sizeof(**rwlock));

    (*rwlock)->pool        = pool;
    (*rwlock)->readers     = 0;

    if (! ((*rwlock)->read_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
        *rwlock = NULL;
        return fspr_get_os_error();
    }

    if (! ((*rwlock)->write_mutex = CreateMutex(NULL, FALSE, NULL))) {
        CloseHandle((*rwlock)->read_event);
        *rwlock = NULL;
        return fspr_get_os_error();
    }

	InitializeCriticalSection(&(*rwlock)->read_section);

    fspr_pool_cleanup_register(pool, *rwlock, thread_rwlock_cleanup,
                              fspr_pool_cleanup_null);

    return APR_SUCCESS;
}

static fspr_status_t fspr_thread_rwlock_rdlock_core(fspr_thread_rwlock_t *rwlock,
                                                  DWORD  milliseconds)
{
	DWORD   code;
	EnterCriticalSection(&rwlock->read_section); 
    code = WaitForSingleObject(rwlock->write_mutex, milliseconds);

	if (code == WAIT_FAILED || code == WAIT_TIMEOUT) {
		LeaveCriticalSection(&rwlock->read_section);
        return APR_FROM_OS_ERROR(code);
	}

    /* We've successfully acquired the writer mutex, we can't be locked
     * for write, so it's OK to add the reader lock.  The writer mutex
     * doubles as race condition protection for the readers counter.   
     */
    InterlockedIncrement(&rwlock->readers);
    
	if (! ResetEvent(rwlock->read_event)) {
		LeaveCriticalSection(&rwlock->read_section);
        return fspr_get_os_error();
	}
    
	if (! ReleaseMutex(rwlock->write_mutex)) {
		LeaveCriticalSection(&rwlock->read_section);
        return fspr_get_os_error();
	}

	LeaveCriticalSection(&rwlock->read_section);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_rdlock(fspr_thread_rwlock_t *rwlock)
{
    return fspr_thread_rwlock_rdlock_core(rwlock, INFINITE);
}

APR_DECLARE(fspr_status_t) 
fspr_thread_rwlock_tryrdlock(fspr_thread_rwlock_t *rwlock)
{
    return fspr_thread_rwlock_rdlock_core(rwlock, 0);
}

static fspr_status_t 
fspr_thread_rwlock_wrlock_core(fspr_thread_rwlock_t *rwlock, DWORD milliseconds)
{
    DWORD   code = WaitForSingleObject(rwlock->write_mutex, milliseconds);

    if (code == WAIT_FAILED || code == WAIT_TIMEOUT)
        return APR_FROM_OS_ERROR(code);

    /* We've got the writer lock but we have to wait for all readers to
     * unlock before it's ok to use it.
     */
    if (rwlock->readers) {
        /* Must wait for readers to finish before returning, unless this
         * is an trywrlock (milliseconds == 0):
         */
        code = milliseconds
          ? WaitForSingleObject(rwlock->read_event, milliseconds)
          : WAIT_TIMEOUT;
        
        if (code == WAIT_FAILED || code == WAIT_TIMEOUT) {
            /* Unable to wait for readers to finish, release write lock: */
            if (! ReleaseMutex(rwlock->write_mutex))
                return fspr_get_os_error();
            
            return APR_FROM_OS_ERROR(code);
        }
    }

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_wrlock(fspr_thread_rwlock_t *rwlock)
{
    return fspr_thread_rwlock_wrlock_core(rwlock, INFINITE);
}

APR_DECLARE(fspr_status_t)fspr_thread_rwlock_trywrlock(fspr_thread_rwlock_t *rwlock)
{
    return fspr_thread_rwlock_wrlock_core(rwlock, 0);
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_unlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t rv = 0;

    /* First, guess that we're unlocking a writer */
    if (! ReleaseMutex(rwlock->write_mutex))
        rv = fspr_get_os_error();
    
    if (rv == APR_FROM_OS_ERROR(ERROR_NOT_OWNER)) {
        /* Nope, we must have a read lock */
        if (rwlock->readers &&
            ! InterlockedDecrement(&rwlock->readers) &&
            ! SetEvent(rwlock->read_event)) {
            rv = fspr_get_os_error();
        }
        else {
            rv = 0;
        }
    }

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_destroy(fspr_thread_rwlock_t *rwlock)
{
    return fspr_pool_cleanup_run(rwlock->pool, rwlock, thread_rwlock_cleanup);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_rwlock)
