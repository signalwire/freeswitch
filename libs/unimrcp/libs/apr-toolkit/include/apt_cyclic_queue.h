/*
 * Copyright 2008 Arsen Chaloyan
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

#ifndef __APT_CYCLIC_QUEUE_H__
#define __APT_CYCLIC_QUEUE_H__

/**
 * @file apt_cyclic_queue.h
 * @brief Cyclic FIFO Queue of Opaque void* Objects
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Default size (number of elements) of cyclic queue */
#define CYCLIC_QUEUE_DEFAULT_SIZE	100

/** Opaque cyclic queue declaration */
typedef struct apt_cyclic_queue_t apt_cyclic_queue_t;

/**
 * Create cyclic queue.
 * @param size the initial size of the queue
 * @return the created queue
 */
APT_DECLARE(apt_cyclic_queue_t*) apt_cyclic_queue_create(apr_size_t size);

/**
 * Destroy cyclic queue.
 * @param queue the queue to destroy
 */
APT_DECLARE(void) apt_cyclic_queue_destroy(apt_cyclic_queue_t *queue);

/**
 * Push object to the queue.
 * @param queue the queue to push object to
 * @param obj the object to push
 */
APT_DECLARE(apt_bool_t) apt_cyclic_queue_push(apt_cyclic_queue_t *queue, void *obj);

/**
 * Pop object from the queue.
 * @param queue the queue to pop message from
 */
APT_DECLARE(void*) apt_cyclic_queue_pop(apt_cyclic_queue_t *queue);

/**
 * Clear the queue (remove all the elements from the queue).
 * @param queue the queue to clear
 */
APT_DECLARE(void) apt_cyclic_queue_clear(apt_cyclic_queue_t *queue);

/**
 * Query whether the queue is empty.
 * @param queue the queue to query
 * @return TRUE if empty, otherwise FALSE
 */
APT_DECLARE(apt_bool_t) apt_cyclic_queue_is_empty(apt_cyclic_queue_t *queue);


APT_END_EXTERN_C

#endif /*__APT_CYCLIC_QUEUE_H__*/
