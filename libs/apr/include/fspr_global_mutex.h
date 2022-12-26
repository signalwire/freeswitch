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

#ifndef APR_GLOBAL_MUTEX_H
#define APR_GLOBAL_MUTEX_H

/**
 * @file fspr_global_mutex.h
 * @brief APR Global Locking Routines
 */

#include "fspr.h"
#include "fspr_proc_mutex.h"    /* only for fspr_lockmech_e */
#include "fspr_pools.h"
#include "fspr_errno.h"
#if APR_PROC_MUTEX_IS_GLOBAL
#include "fspr_proc_mutex.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup APR_GlobalMutex Global Locking Routines
 * @ingroup APR 
 * @{
 */

#if !APR_PROC_MUTEX_IS_GLOBAL || defined(DOXYGEN)

/** Opaque global mutex structure. */
typedef struct fspr_global_mutex_t fspr_global_mutex_t;

/*   Function definitions */

/**
 * Create and initialize a mutex that can be used to synchronize both
 * processes and threads. Note: There is considerable overhead in using
 * this API if only cross-process or cross-thread mutual exclusion is
 * required. See fspr_proc_mutex.h and fspr_thread_mutex.h for more
 * specialized lock routines.
 * @param mutex the memory address where the newly created mutex will be
 *        stored.
 * @param fname A file name to use if the lock mechanism requires one.  This
 *        argument should always be provided.  The lock code itself will
 *        determine if it should be used.
 * @param mech The mechanism to use for the interprocess lock, if any; one of
 * <PRE>
 *            APR_LOCK_FCNTL
 *            APR_LOCK_FLOCK
 *            APR_LOCK_SYSVSEM
 *            APR_LOCK_POSIXSEM
 *            APR_LOCK_PROC_PTHREAD
 *            APR_LOCK_DEFAULT     pick the default mechanism for the platform
 * </PRE>
 * @param pool the pool from which to allocate the mutex.
 * @warning Check APR_HAS_foo_SERIALIZE defines to see if the platform supports
 *          APR_LOCK_foo.  Only APR_LOCK_DEFAULT is portable.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_create(fspr_global_mutex_t **mutex,
                                                  const char *fname,
                                                  fspr_lockmech_e mech,
                                                  fspr_pool_t *pool);

/**
 * Re-open a mutex in a child process.
 * @param mutex The newly re-opened mutex structure.
 * @param fname A file name to use if the mutex mechanism requires one.  This
 *              argument should always be provided.  The mutex code itself will
 *              determine if it should be used.  This filename should be the 
 *              same one that was passed to fspr_global_mutex_create().
 * @param pool The pool to operate on.
 * @remark This function must be called to maintain portability, even
 *         if the underlying lock mechanism does not require it.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_child_init(
                              fspr_global_mutex_t **mutex,
                              const char *fname,
                              fspr_pool_t *pool);

/**
 * Acquire the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 * @param mutex the mutex on which to acquire the lock.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_lock(fspr_global_mutex_t *mutex);

/**
 * Attempt to acquire the lock for the given mutex. If the mutex has already
 * been acquired, the call returns immediately with APR_EBUSY. Note: it
 * is important that the APR_STATUS_IS_EBUSY(s) macro be used to determine
 * if the return value was APR_EBUSY, for portability reasons.
 * @param mutex the mutex on which to attempt the lock acquiring.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_trylock(fspr_global_mutex_t *mutex);

/**
 * Release the lock for the given mutex.
 * @param mutex the mutex from which to release the lock.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_unlock(fspr_global_mutex_t *mutex);

/**
 * Destroy the mutex and free the memory associated with the lock.
 * @param mutex the mutex to destroy.
 */
APR_DECLARE(fspr_status_t) fspr_global_mutex_destroy(fspr_global_mutex_t *mutex);

/**
 * Get the pool used by this global_mutex.
 * @return fspr_pool_t the pool
 */
APR_POOL_DECLARE_ACCESSOR(global_mutex);

#else /* APR_PROC_MUTEX_IS_GLOBAL */

/* Some platforms [e.g. Win32] have cross process locks that are truly
 * global locks, since there isn't the concept of cross-process locks.
 * Define these platforms in terms of an fspr_proc_mutex_t.
 */

#define fspr_global_mutex_t          fspr_proc_mutex_t
#define fspr_global_mutex_create     fspr_proc_mutex_create
#define fspr_global_mutex_child_init fspr_proc_mutex_child_init
#define fspr_global_mutex_lock       fspr_proc_mutex_lock
#define fspr_global_mutex_trylock    fspr_proc_mutex_trylock
#define fspr_global_mutex_unlock     fspr_proc_mutex_unlock
#define fspr_global_mutex_destroy    fspr_proc_mutex_destroy
#define fspr_global_mutex_pool_get   fspr_proc_mutex_pool_get

#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ndef APR_GLOBAL_MUTEX_H */
