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
#include "fspr_portable.h"
#include "fspr_arch_proc_mutex.h"
#include "fspr_arch_thread_mutex.h"

APR_DECLARE(fspr_status_t) fspr_proc_mutex_create(fspr_proc_mutex_t **mutex,
                                                const char *fname,
                                                fspr_lockmech_e mech,
                                                fspr_pool_t *pool)
{
    fspr_status_t ret;
    fspr_proc_mutex_t *new_mutex = NULL;
    new_mutex = (fspr_proc_mutex_t *)fspr_pcalloc(pool, sizeof(fspr_proc_mutex_t));
	
	if(new_mutex ==NULL) {
        return APR_ENOMEM;
    }     
    
    new_mutex->pool = pool;
    ret = fspr_thread_mutex_create(&(new_mutex->mutex), APR_THREAD_MUTEX_DEFAULT, pool);

    if (ret == APR_SUCCESS)
        *mutex = new_mutex;

    return ret;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_child_init(fspr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    fspr_pool_t *pool)
{
    return APR_SUCCESS;
}
    
APR_DECLARE(fspr_status_t) fspr_proc_mutex_lock(fspr_proc_mutex_t *mutex)
{
    if (mutex)
        return fspr_thread_mutex_lock(mutex->mutex);
    return APR_ENOLOCK;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_trylock(fspr_proc_mutex_t *mutex)
{
    if (mutex)
        return fspr_thread_mutex_trylock(mutex->mutex);
    return APR_ENOLOCK;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_unlock(fspr_proc_mutex_t *mutex)
{
    if (mutex)
        return fspr_thread_mutex_unlock(mutex->mutex);
    return APR_ENOLOCK;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_cleanup(void *mutex)
{
    return fspr_proc_mutex_destroy(mutex);
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_destroy(fspr_proc_mutex_t *mutex)
{
    if (mutex)
        return fspr_thread_mutex_destroy(mutex->mutex);
    return APR_ENOLOCK;
}

APR_DECLARE(const char *) fspr_proc_mutex_lockfile(fspr_proc_mutex_t *mutex)
{
    return NULL;
}

APR_DECLARE(const char *) fspr_proc_mutex_name(fspr_proc_mutex_t *mutex)
{
    return "netwarethread";
}

APR_DECLARE(const char *) fspr_proc_mutex_defname(void)
{
    return "netwarethread";
}

APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)

/* Implement OS-specific accessors defined in fspr_portable.h */

fspr_status_t fspr_os_proc_mutex_get(fspr_os_proc_mutex_t *ospmutex,
                                   fspr_proc_mutex_t *pmutex)
{
    if (pmutex)
        ospmutex = pmutex->mutex->mutex;
    return APR_ENOLOCK;
}

fspr_status_t fspr_os_proc_mutex_put(fspr_proc_mutex_t **pmutex,
                                   fspr_os_proc_mutex_t *ospmutex,
                                   fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

