/*
 * Copyright 2008-2015 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APT_POLLSET_H
#define APT_POLLSET_H

/**
 * @file apt_pollset.h
 * @brief Interruptable APR-Pollset
 */ 

/**
 * Wakeup builtin API of the pollset is introduced only in APR-1.4
 * and it is not available for APR-1.2 and APR-1.3 versions. Thus
 * apt_pollset_t is an extension of apr_pollset_t and provides
 * pollset wakeup capabilities the similar way as it's implemented
 * in APR-1.4 trunk
 */

#include <apr_poll.h>
#include "apt.h"

APT_BEGIN_EXTERN_C

/** Opaque pollset declaration */
typedef struct apt_pollset_t apt_pollset_t;

/**
 * Create interruptable pollset on top of APR pollset.
 * @param size the maximum number of descriptors pollset can hold
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_pollset_t*) apt_pollset_create(apr_uint32_t size, apr_pool_t *pool);

/**
 * Destroy pollset.
 * @param pollset the pollset to destroy
 */
APT_DECLARE(apt_bool_t) apt_pollset_destroy(apt_pollset_t *pollset);

/**
 * Add pollset descriptor to a pollset.
 * @param pollset the pollset to add the descriptor to
 * @param descriptor the descriptor to add
 */
APT_DECLARE(apt_bool_t) apt_pollset_add(apt_pollset_t *pollset, const apr_pollfd_t *descriptor);

/**
 * Remove pollset descriptor from a pollset.
 * @param pollset the pollset to remove the descriptor from
 * @param descriptor the descriptor to remove
 */
APT_DECLARE(apt_bool_t) apt_pollset_remove(apt_pollset_t *pollset, const apr_pollfd_t *descriptor);

/**
 * Block for activity on the descriptor(s) in a pollset.
 * @param pollset the pollset to use
 * @param timeout the timeout in microseconds
 * @param num the number of signalled descriptors (output parameter)
 * @param descriptors the array of signalled descriptors (output parameter)
 */
APT_DECLARE(apr_status_t) apt_pollset_poll(
								apt_pollset_t *pollset,
								apr_interval_time_t timeout,
								apr_int32_t *num,
								const apr_pollfd_t **descriptors);

/**
 * Interrupt the blocked poll call.
 * @param pollset the pollset to use
 */
APT_DECLARE(apt_bool_t) apt_pollset_wakeup(apt_pollset_t *pollset);

/**
 * Match against builtin wake up descriptor in a pollset.
 * @param pollset the pollset to use
 * @param descriptor the descriptor to match
 */
APT_DECLARE(apt_bool_t) apt_pollset_is_wakeup(apt_pollset_t *pollset, const apr_pollfd_t *descriptor);

APT_END_EXTERN_C

#endif /* APT_POLLSET_H */
