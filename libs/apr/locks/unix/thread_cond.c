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

#if APR_HAS_THREADS

#include "fspr_arch_thread_mutex.h"
#include "fspr_arch_thread_cond.h"

static fspr_status_t thread_cond_cleanup(void *data)
{
    fspr_thread_cond_t *cond = (fspr_thread_cond_t *)data;
    fspr_status_t rv;

    rv = pthread_cond_destroy(&cond->cond);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    return rv;
} 

APR_DECLARE(fspr_status_t) fspr_thread_cond_create(fspr_thread_cond_t **cond,
                                                 fspr_pool_t *pool)
{
    fspr_thread_cond_t *new_cond;
    fspr_status_t rv;

    new_cond = fspr_palloc(pool, sizeof(fspr_thread_cond_t));

    new_cond->pool = pool;

    if ((rv = pthread_cond_init(&new_cond->cond, NULL))) {
#ifdef PTHREAD_SETS_ERRNO
        rv = errno;
#endif
        return rv;
    }

    fspr_pool_cleanup_register(new_cond->pool,
                              (void *)new_cond, thread_cond_cleanup,
                              fspr_pool_cleanup_null);

    *cond = new_cond;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_wait(fspr_thread_cond_t *cond,
                                               fspr_thread_mutex_t *mutex)
{
    fspr_status_t rv;

    rv = pthread_cond_wait(&cond->cond, &mutex->mutex);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                    fspr_thread_mutex_t *mutex,
                                                    fspr_interval_time_t timeout)
{
    fspr_status_t rv;
    fspr_time_t then;
    struct timespec abstime;

    then = fspr_time_now() + timeout;
    abstime.tv_sec = fspr_time_sec(then);
    abstime.tv_nsec = fspr_time_usec(then) * 1000; /* nanoseconds */

    rv = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &abstime);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    if (ETIMEDOUT == rv) {
        return APR_TIMEUP;
    }
    return rv;
}


APR_DECLARE(fspr_status_t) fspr_thread_cond_signal(fspr_thread_cond_t *cond)
{
    fspr_status_t rv;

    rv = pthread_cond_signal(&cond->cond);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_broadcast(fspr_thread_cond_t *cond)
{
    fspr_status_t rv;

    rv = pthread_cond_broadcast(&cond->cond);
#ifdef PTHREAD_SETS_ERRNO
    if (rv) {
        rv = errno;
    }
#endif
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_destroy(fspr_thread_cond_t *cond)
{
    return fspr_pool_cleanup_run(cond->pool, cond, thread_cond_cleanup);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_cond)

#endif /* APR_HAS_THREADS */
