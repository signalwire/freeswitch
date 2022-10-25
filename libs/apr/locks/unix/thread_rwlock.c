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

#include "fspr_arch_thread_rwlock.h"
#include "fspr_private.h"

#if APR_HAS_THREADS

#ifdef HAVE_PTHREAD_RWLOCKS

/* The rwlock must be initialized but not locked by any thread when
 * cleanup is called. */
static fspr_status_t thread_rwlock_cleanup(void *data)
{
    fspr_thread_rwlock_t *rwlock = (fspr_thread_rwlock_t *)data;
    fspr_status_t stat;

    stat = pthread_rwlock_destroy(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    return stat;
} 

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_create(fspr_thread_rwlock_t **rwlock,
                                                   fspr_pool_t *pool)
{
    fspr_thread_rwlock_t *new_rwlock;
    fspr_status_t stat;

    new_rwlock = fspr_palloc(pool, sizeof(fspr_thread_rwlock_t));
    new_rwlock->pool = pool;

    if ((stat = pthread_rwlock_init(&new_rwlock->rwlock, NULL))) {
#ifdef PTHREAD_SETS_ERRNO
        stat = errno;
#endif
        return stat;
    }

    fspr_pool_cleanup_register(new_rwlock->pool,
                              (void *)new_rwlock, thread_rwlock_cleanup,
                              fspr_pool_cleanup_null);

    *rwlock = new_rwlock;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_rdlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t stat;

    stat = pthread_rwlock_rdlock(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_tryrdlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t stat;

    stat = pthread_rwlock_tryrdlock(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    /* Normalize the return code. */
    if (stat == EBUSY)
        stat = APR_EBUSY;
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_wrlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t stat;

    stat = pthread_rwlock_wrlock(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_trywrlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t stat;

    stat = pthread_rwlock_trywrlock(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    /* Normalize the return code. */
    if (stat == EBUSY)
        stat = APR_EBUSY;
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_unlock(fspr_thread_rwlock_t *rwlock)
{
    fspr_status_t stat;

    stat = pthread_rwlock_unlock(&rwlock->rwlock);
#ifdef PTHREAD_SETS_ERRNO
    if (stat) {
        stat = errno;
    }
#endif
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_destroy(fspr_thread_rwlock_t *rwlock)
{
    return fspr_pool_cleanup_run(rwlock->pool, rwlock, thread_rwlock_cleanup);
}

#else  /* HAVE_PTHREAD_RWLOCKS */

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_create(fspr_thread_rwlock_t **rwlock,
                                                   fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_rdlock(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_tryrdlock(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_wrlock(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_trywrlock(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_unlock(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_rwlock_destroy(fspr_thread_rwlock_t *rwlock)
{
    return APR_ENOTIMPL;
}

#endif /* HAVE_PTHREAD_RWLOCKS */
APR_POOL_IMPLEMENT_ACCESSOR(thread_rwlock)

#endif /* APR_HAS_THREADS */
