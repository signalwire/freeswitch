/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_mutex.h -- Mutex Locking
 *
 */
/*! \file switch_mutex.h
    \brief Mutex Locking
*/

#ifndef SWITCH_MUTEX_H
#define SWITCH_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/**
 * @defgroup switch_thread_mutex Thread Mutex Routines
 * @ingroup FREESWITCH 
 * @{
 */

/** Opaque thread-local mutex structure */
typedef apr_thread_mutex_t switch_mutex_t;

/** Lock Flags */
typedef enum {
	SWITCH_MUTEX_DEFAULT = APR_THREAD_MUTEX_DEFAULT	/**< platform-optimal lock behavior */,
	SWITCH_MUTEX_NESTED = APR_THREAD_MUTEX_NESTED	/**< enable nested (recursive) locks */,
	SWITCH_MUTEX_UNNESTED = APR_THREAD_MUTEX_UNNESTED	/**< disable nested locks */
} switch_lock_flag;

/**
 * Create and initialize a mutex that can be used to synchronize threads.
 * @param lock the memory address where the newly created mutex will be
 *        stored.
 * @param flags Or'ed value of:
 * <PRE>
 *           SWITCH_THREAD_MUTEX_DEFAULT   platform-optimal lock behavior.
 *           SWITCH_THREAD_MUTEX_NESTED    enable nested (recursive) locks.
 *           SWITCH_THREAD_MUTEX_UNNESTED  disable nested locks (non-recursive).
 * </PRE>
 * @param pool the pool from which to allocate the mutex.
 * @warning Be cautious in using SWITCH_THREAD_MUTEX_DEFAULT.  While this is the
 * most optimial mutex based on a given platform's performance charateristics,
 * it will behave as either a nested or an unnested lock.
 */
SWITCH_DECLARE(switch_status) switch_mutex_init(switch_mutex_t **lock,
												switch_lock_flag flags,
												switch_memory_pool *pool);

/**
 * Destroy the mutex and free the memory associated with the lock.
 * @param lock the mutex to destroy.
 */
SWITCH_DECLARE(switch_status) switch_mutex_destroy(switch_mutex_t *lock);

/**
 * Acquire the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 * @param lock the mutex on which to acquire the lock.
 */
SWITCH_DECLARE(switch_status) switch_mutex_lock(switch_mutex_t *lock);

/**
 * Release the lock for the given mutex.
 * @param lock the mutex from which to release the lock.
 */
SWITCH_DECLARE(switch_status) switch_mutex_unlock(switch_mutex_t *lock);

/**
 * Attempt to acquire the lock for the given mutex. If the mutex has already
 * been acquired, the call returns immediately with APR_EBUSY. Note: it
 * is important that the APR_STATUS_IS_EBUSY(s) macro be used to determine
 * if the return value was APR_EBUSY, for portability reasons.
 * @param lock the mutex on which to attempt the lock acquiring.
 */
SWITCH_DECLARE(switch_status) switch_mutex_trylock(switch_mutex_t *lock);

/** @} */
#ifdef __cplusplus
}
#endif


#endif
