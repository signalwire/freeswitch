/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: apt_consumer_task.h 2175 2014-09-12 04:56:09Z achaloyan@gmail.com $
 */

#ifndef APT_CONSUMER_TASK_H
#define APT_CONSUMER_TASK_H

/**
 * @file apt_consumer_task.h
 * @brief Consumer Task Definition
 */ 

#include "apt_task.h"
#include "apt_timer_queue.h"

APT_BEGIN_EXTERN_C

/** Opaque consumer task declaration */
typedef struct apt_consumer_task_t apt_consumer_task_t;

/**
 * Create consumer task.
 * @param obj the external object to associate with the task
 * @param msg_pool the pool of task messages
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_consumer_task_t*) apt_consumer_task_create(
									void *obj,
									apt_task_msg_pool_t *msg_pool,
									apr_pool_t *pool);

/**
 * Get task base.
 * @param task the consumer task to get base for
 */
APT_DECLARE(apt_task_t*) apt_consumer_task_base_get(const apt_consumer_task_t *task);

/**
 * Get task vtable.
 * @param task the consumer task to get vtable for
 */
APT_DECLARE(apt_task_vtable_t*) apt_consumer_task_vtable_get(const apt_consumer_task_t *task);

/**
 * Get consumer task object.
 * @param task the consumer task to get object from
 */
APT_DECLARE(void*) apt_consumer_task_object_get(const apt_consumer_task_t *task);

/**
 * Create timer.
 * @param task the consumer task to create timer for
 * @param proc the timer callback
 * @param obj the object to pass to callback
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_timer_t*) apt_consumer_task_timer_create(
								apt_consumer_task_t *task,
								apt_timer_proc_f proc,
								void *obj,
								apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* APT_CONSUMER_TASK_H */
