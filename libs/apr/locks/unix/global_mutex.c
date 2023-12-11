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
#include "fspr_strings.h"
#include "fspr_arch_global_mutex.h"
#include "fspr_proc_mutex.h"
#include "fspr_thread_mutex.h"
#include "fspr_portable.h"

static fspr_status_t global_mutex_cleanup(void *data)
{
    fspr_global_mutex_t *m = (fspr_global_mutex_t *)data;
    fspr_status_t rv;

    rv = fspr_proc_mutex_destroy(m->proc_mutex);

#if APR_HAS_THREADS
    if (m->thread_mutex) {
        if (rv != APR_SUCCESS) {
            (void)fspr_thread_mutex_destroy(m->thread_mutex);
        }
        else {
            rv = fspr_thread_mutex_destroy(m->thread_mutex);
        }
    }
#endif /* APR_HAS_THREADS */

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_create(fspr_global_mutex_t **mutex,
                                                  const char *fname,
                                                  fspr_lockmech_e mech,
                                                  fspr_pool_t *pool)
{
    fspr_status_t rv;
    fspr_global_mutex_t *m;

    m = (fspr_global_mutex_t *)fspr_palloc(pool, sizeof(*m));
    m->pool = pool;

    rv = fspr_proc_mutex_create(&m->proc_mutex, fname, mech, m->pool);
    if (rv != APR_SUCCESS) {
        return rv;
    }

#if APR_HAS_THREADS
    if (m->proc_mutex->inter_meth->flags & APR_PROCESS_LOCK_MECH_IS_GLOBAL) {
        m->thread_mutex = NULL; /* We don't need a thread lock. */
    }
    else {
        rv = fspr_thread_mutex_create(&m->thread_mutex,
                                     APR_THREAD_MUTEX_DEFAULT, m->pool);
        if (rv != APR_SUCCESS) {
            rv = fspr_proc_mutex_destroy(m->proc_mutex);
            return rv;
        }
    }
#endif /* APR_HAS_THREADS */

    fspr_pool_cleanup_register(m->pool, (void *)m,
                              global_mutex_cleanup, fspr_pool_cleanup_null);
    *mutex = m;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_child_init(
                              fspr_global_mutex_t **mutex,
                              const char *fname,
                              fspr_pool_t *pool)
{
    fspr_status_t rv;

    rv = fspr_proc_mutex_child_init(&((*mutex)->proc_mutex), fname, pool);
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_lock(fspr_global_mutex_t *mutex)
{
    fspr_status_t rv;

#if APR_HAS_THREADS
    if (mutex->thread_mutex) {
        rv = fspr_thread_mutex_lock(mutex->thread_mutex);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    }
#endif /* APR_HAS_THREADS */

    rv = fspr_proc_mutex_lock(mutex->proc_mutex);

#if APR_HAS_THREADS
    if (rv != APR_SUCCESS) {
        if (mutex->thread_mutex) {
            (void)fspr_thread_mutex_unlock(mutex->thread_mutex);
        }
    }
#endif /* APR_HAS_THREADS */

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_trylock(fspr_global_mutex_t *mutex)
{
    fspr_status_t rv;

#if APR_HAS_THREADS
    if (mutex->thread_mutex) {
        rv = fspr_thread_mutex_trylock(mutex->thread_mutex);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    }
#endif /* APR_HAS_THREADS */

    rv = fspr_proc_mutex_trylock(mutex->proc_mutex);

#if APR_HAS_THREADS
    if (rv != APR_SUCCESS) {
        if (mutex->thread_mutex) {
            (void)fspr_thread_mutex_unlock(mutex->thread_mutex);
        }
    }
#endif /* APR_HAS_THREADS */

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_unlock(fspr_global_mutex_t *mutex)
{
    fspr_status_t rv;

    rv = fspr_proc_mutex_unlock(mutex->proc_mutex);
#if APR_HAS_THREADS
    if (mutex->thread_mutex) {
        if (rv != APR_SUCCESS) {
            (void)fspr_thread_mutex_unlock(mutex->thread_mutex);
        }
        else {
            rv = fspr_thread_mutex_unlock(mutex->thread_mutex);
        }
    }
#endif /* APR_HAS_THREADS */
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_os_global_mutex_get(fspr_os_global_mutex_t *ospmutex,
                                                fspr_global_mutex_t *pmutex)
{
    ospmutex->pool = pmutex->pool;
    ospmutex->proc_mutex = pmutex->proc_mutex;
#if APR_HAS_THREADS
    ospmutex->thread_mutex = pmutex->thread_mutex;
#endif
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_global_mutex_destroy(fspr_global_mutex_t *mutex)
{
    return fspr_pool_cleanup_run(mutex->pool, mutex, global_mutex_cleanup);
}

APR_POOL_IMPLEMENT_ACCESSOR(global_mutex)
