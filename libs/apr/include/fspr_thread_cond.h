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

#ifndef APR_THREAD_COND_H
#define APR_THREAD_COND_H

/**
 * @file fspr_thread_cond.h
 * @brief APR Condition Variable Routines
 */

#include "fspr.h"
#include "fspr_pools.h"
#include "fspr_errno.h"
#include "fspr_time.h"
#include "fspr_thread_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if APR_HAS_THREADS || defined(DOXYGEN)

/**
 * @defgroup fspr_thread_cond Condition Variable Routines
 * @ingroup APR 
 * @{
 */

/** Opaque structure for thread condition variables */
typedef struct fspr_thread_cond_t fspr_thread_cond_t;

/**
 * Note: destroying a condition variable (or likewise, destroying or
 * clearing the pool from which a condition variable was allocated) if
 * any threads are blocked waiting on it gives undefined results.
 */

/**
 * Create and initialize a condition variable that can be used to signal
 * and schedule threads in a single process.
 * @param cond the memory address where the newly created condition variable
 *        will be stored.
 * @param pool the pool from which to allocate the mutex.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_create(fspr_thread_cond_t **cond,
                                                 fspr_pool_t *pool);

/**
 * Put the active calling thread to sleep until signaled to wake up. Each
 * condition variable must be associated with a mutex, and that mutex must
 * be locked before  calling this function, or the behavior will be
 * undefined. As the calling thread is put to sleep, the given mutex
 * will be simultaneously released; and as this thread wakes up the lock
 * is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_wait(fspr_thread_cond_t *cond,
                                               fspr_thread_mutex_t *mutex);

/**
 * Put the active calling thread to sleep until signaled to wake up or
 * the timeout is reached. Each condition variable must be associated
 * with a mutex, and that mutex must be locked before calling this
 * function, or the behavior will be undefined. As the calling thread
 * is put to sleep, the given mutex will be simultaneously released;
 * and as this thread wakes up the lock is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 * @param timeout The amount of time in microseconds to wait. This is 
 *        a maximum, not a minimum. If the condition is signaled, we 
 *        will wake up before this time, otherwise the error APR_TIMEUP
 *        is returned.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                    fspr_thread_mutex_t *mutex,
                                                    fspr_interval_time_t timeout);

/**
 * Signals a single thread, if one exists, that is blocking on the given
 * condition variable. That thread is then scheduled to wake up and acquire
 * the associated mutex. Although it is not required, if predictable scheduling
 * is desired, that mutex must be locked while calling this function.
 * @param cond the condition variable on which to produce the signal.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_signal(fspr_thread_cond_t *cond);

/**
 * Signals all threads blocking on the given condition variable.
 * Each thread that was signaled is then scheduled to wake up and acquire
 * the associated mutex. This will happen in a serialized manner.
 * @param cond the condition variable on which to produce the broadcast.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_broadcast(fspr_thread_cond_t *cond);

/**
 * Destroy the condition variable and free the associated memory.
 * @param cond the condition variable to destroy.
 */
APR_DECLARE(fspr_status_t) fspr_thread_cond_destroy(fspr_thread_cond_t *cond);

/**
 * Get the pool used by this thread_cond.
 * @return fspr_pool_t the pool
 */
APR_POOL_DECLARE_ACCESSOR(thread_cond);

#endif /* APR_HAS_THREADS */

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! APR_THREAD_COND_H */
