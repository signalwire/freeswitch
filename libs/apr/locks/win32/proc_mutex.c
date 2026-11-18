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
#include "fspr_portable.h"
#include "fspr_arch_file_io.h"
#include "fspr_arch_proc_mutex.h"
#include "fspr_arch_misc.h"

static fspr_status_t proc_mutex_cleanup(void *mutex_)
{
    fspr_proc_mutex_t *mutex = mutex_;

    if (mutex->handle) {
        if (CloseHandle(mutex->handle) == 0) {
            return fspr_get_os_error();
        }
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_create(fspr_proc_mutex_t **mutex,
                                                const char *fname,
                                                fspr_lockmech_e mech,
                                                fspr_pool_t *pool)
{
    HANDLE hMutex;
    void *mutexkey;

    /* res_name_from_filename turns fname into a pseduo-name
     * without slashes or backslashes, and prepends the \global
     * prefix on Win2K and later
     */
    if (fname) {
        mutexkey = res_name_from_filename(fname, 1, pool);
    }
    else {
        mutexkey = NULL;
    }

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        hMutex = CreateMutexW(NULL, FALSE, mutexkey);
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        hMutex = CreateMutexA(NULL, FALSE, mutexkey);
    }
#endif

    if (!hMutex) {
        return fspr_get_os_error();
    }

    *mutex = (fspr_proc_mutex_t *)fspr_palloc(pool, sizeof(fspr_proc_mutex_t));
    (*mutex)->pool = pool;
    (*mutex)->handle = hMutex;
    (*mutex)->fname = fname;
    fspr_pool_cleanup_register((*mutex)->pool, *mutex, 
                              proc_mutex_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_child_init(fspr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    fspr_pool_t *pool)
{
    HANDLE hMutex;
    void *mutexkey;

    if (!fname) {
        /* Reinitializing unnamed mutexes is a noop in the Unix code. */
        return APR_SUCCESS;
    }

    /* res_name_from_filename turns file into a pseudo-name
     * without slashes or backslashes, and prepends the \global
     * prefix on Win2K and later
     */
    mutexkey = res_name_from_filename(fname, 1, pool);

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutexkey);
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mutexkey);
    }
#endif

    if (!hMutex) {
        return fspr_get_os_error();
    }

    *mutex = (fspr_proc_mutex_t *)fspr_palloc(pool, sizeof(fspr_proc_mutex_t));
    (*mutex)->pool = pool;
    (*mutex)->handle = hMutex;
    (*mutex)->fname = fname;
    fspr_pool_cleanup_register((*mutex)->pool, *mutex, 
                              proc_mutex_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}
    
APR_DECLARE(fspr_status_t) fspr_proc_mutex_lock(fspr_proc_mutex_t *mutex)
{
    DWORD rv;

    rv = WaitForSingleObject(mutex->handle, INFINITE);

    if (rv == WAIT_OBJECT_0 || rv == WAIT_ABANDONED) {
        return APR_SUCCESS;
    } 
    else if (rv == WAIT_TIMEOUT) {
        return APR_EBUSY;
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_trylock(fspr_proc_mutex_t *mutex)
{
    DWORD rv;

    rv = WaitForSingleObject(mutex->handle, 0);

    if (rv == WAIT_OBJECT_0 || rv == WAIT_ABANDONED) {
        return APR_SUCCESS;
    } 
    else if (rv == WAIT_TIMEOUT) {
        return APR_EBUSY;
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_unlock(fspr_proc_mutex_t *mutex)
{
    if (ReleaseMutex(mutex->handle) == 0) {
        return fspr_get_os_error();
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_destroy(fspr_proc_mutex_t *mutex)
{
    fspr_status_t stat;

    stat = proc_mutex_cleanup(mutex);
    if (stat == APR_SUCCESS) {
        fspr_pool_cleanup_kill(mutex->pool, mutex, proc_mutex_cleanup);
    }
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_cleanup(void *mutex)
{
    return fspr_proc_mutex_destroy((fspr_proc_mutex_t *)mutex);
}

APR_DECLARE(const char *) fspr_proc_mutex_lockfile(fspr_proc_mutex_t *mutex)
{
    return NULL;
}

APR_DECLARE(const char *) fspr_proc_mutex_name(fspr_proc_mutex_t *mutex)
{
    return mutex->fname;
}

APR_DECLARE(const char *) fspr_proc_mutex_defname(void)
{
    return "win32mutex";
}

APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)

/* Implement OS-specific accessors defined in fspr_portable.h */

APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_get(fspr_os_proc_mutex_t *ospmutex,
                                                fspr_proc_mutex_t *mutex)
{
    *ospmutex = mutex->handle;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_put(fspr_proc_mutex_t **pmutex,
                                                fspr_os_proc_mutex_t *ospmutex,
                                                fspr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*pmutex) == NULL) {
        (*pmutex) = (fspr_proc_mutex_t *)fspr_palloc(pool,
                                                   sizeof(fspr_proc_mutex_t));
        (*pmutex)->pool = pool;
    }
    (*pmutex)->handle = *ospmutex;
    return APR_SUCCESS;
}

