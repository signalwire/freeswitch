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

#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include "fspr_arch_proc_mutex.h"
#include "fspr_arch_file_io.h"
#include <string.h>
#include <stddef.h>

#define CurrentTid (*_threadid)

static char *fixed_name(const char *fname, fspr_pool_t *pool)
{
    char *semname;

    if (fname == NULL)
        semname = NULL;
    else {
        // Semaphores don't live in the file system, fix up the name
        while (*fname == '/' || *fname == '\\') {
            fname++;
        }

        semname = fspr_pstrcat(pool, "/SEM32/", fname, NULL);

        if (semname[8] == ':') {
            semname[8] = '$';
        }
    }

    return semname;
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_cleanup(void *vmutex)
{
    fspr_proc_mutex_t *mutex = vmutex;
    return fspr_proc_mutex_destroy(mutex);
}

APR_DECLARE(const char *) fspr_proc_mutex_lockfile(fspr_proc_mutex_t *mutex)
{
    return NULL;
}

APR_DECLARE(const char *) fspr_proc_mutex_name(fspr_proc_mutex_t *mutex)
{
    return "os2sem";
}

APR_DECLARE(const char *) fspr_proc_mutex_defname(void)
{
    return "os2sem";
}


APR_DECLARE(fspr_status_t) fspr_proc_mutex_create(fspr_proc_mutex_t **mutex,
                                                const char *fname,
                                                fspr_lockmech_e mech,
                                                fspr_pool_t *pool)
{
    fspr_proc_mutex_t *new;
    ULONG rc;
    char *semname;

    if (mech != APR_LOCK_DEFAULT) {
        return APR_ENOTIMPL;
    }

    new = (fspr_proc_mutex_t *)fspr_palloc(pool, sizeof(fspr_proc_mutex_t));
    new->pool       = pool;
    new->owner      = 0;
    new->lock_count = 0;
    *mutex = new;

    semname = fixed_name(fname, pool);
    rc = DosCreateMutexSem(semname, &(new->hMutex), DC_SEM_SHARED, FALSE);

    if (!rc) {
        fspr_pool_cleanup_register(pool, new, fspr_proc_mutex_cleanup, fspr_pool_cleanup_null);
    }

    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_child_init(fspr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    fspr_pool_t *pool)
{
    fspr_proc_mutex_t *new;
    ULONG rc;
    char *semname;

    new = (fspr_proc_mutex_t *)fspr_palloc(pool, sizeof(fspr_proc_mutex_t));
    new->pool       = pool;
    new->owner      = 0;
    new->lock_count = 0;

    semname = fixed_name(fname, pool);
    rc = DosOpenMutexSem(semname, &(new->hMutex));
    *mutex = new;

    if (!rc) {
        fspr_pool_cleanup_register(pool, new, fspr_proc_mutex_cleanup, fspr_pool_cleanup_null);
    }

    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_lock(fspr_proc_mutex_t *mutex)
{
    ULONG rc = DosRequestMutexSem(mutex->hMutex, SEM_INDEFINITE_WAIT);

    if (rc == 0) {
        mutex->owner = CurrentTid;
        mutex->lock_count++;
    }

    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_trylock(fspr_proc_mutex_t *mutex)
{
    ULONG rc = DosRequestMutexSem(mutex->hMutex, SEM_IMMEDIATE_RETURN);

    if (rc == 0) {
        mutex->owner = CurrentTid;
        mutex->lock_count++;
    }

    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_unlock(fspr_proc_mutex_t *mutex)
{
    ULONG rc;

    if (mutex->owner == CurrentTid && mutex->lock_count > 0) {
        mutex->lock_count--;
        rc = DosReleaseMutexSem(mutex->hMutex);
        return APR_FROM_OS_ERROR(rc);
    }

    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_proc_mutex_destroy(fspr_proc_mutex_t *mutex)
{
    ULONG rc;
    fspr_status_t status = APR_SUCCESS;

    if (mutex->owner == CurrentTid) {
        while (mutex->lock_count > 0 && status == APR_SUCCESS) {
            status = fspr_proc_mutex_unlock(mutex);
        }
    }

    if (status != APR_SUCCESS) {
        return status;
    }

    if (mutex->hMutex == 0) {
        return APR_SUCCESS;
    }

    rc = DosCloseMutexSem(mutex->hMutex);

    if (!rc) {
        mutex->hMutex = 0;
    }

    return APR_FROM_OS_ERROR(rc);
}



APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)



/* Implement OS-specific accessors defined in fspr_portable.h */

APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_get(fspr_os_proc_mutex_t *ospmutex,
                                                fspr_proc_mutex_t *pmutex)
{
    *ospmutex = pmutex->hMutex;
    return APR_ENOTIMPL;
}



APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_put(fspr_proc_mutex_t **pmutex,
                                                fspr_os_proc_mutex_t *ospmutex,
                                                fspr_pool_t *pool)
{
    fspr_proc_mutex_t *new;

    new = (fspr_proc_mutex_t *)fspr_palloc(pool, sizeof(fspr_proc_mutex_t));
    new->pool       = pool;
    new->owner      = 0;
    new->lock_count = 0;
    new->hMutex     = *ospmutex;
    *pmutex = new;

    return APR_SUCCESS;
}

