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
 * $Id: apt_timer_queue.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_TIMER_QUEUE_H
#define APT_TIMER_QUEUE_H

/**
 * @file apt_timer_queue.h
 * @brief Timer Queue
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Opaque timer declaration */
typedef struct apt_timer_t apt_timer_t;
/** Opaque timer queue declaration */
typedef struct apt_timer_queue_t apt_timer_queue_t;

/** Prototype of timer callback */
typedef void (*apt_timer_proc_f)(apt_timer_t *timer, void *obj);


/** Create timer queue */
APT_DECLARE(apt_timer_queue_t*) apt_timer_queue_create(apr_pool_t *pool);

/** Destroy timer queue */
APT_DECLARE(void) apt_timer_queue_destroy(apt_timer_queue_t *timer_queue);

/** Advance scheduled timers */
APT_DECLARE(void) apt_timer_queue_advance(apt_timer_queue_t *timer_queue, apr_uint32_t elapsed_time);

/** Is timer queue empty */
APT_DECLARE(apt_bool_t) apt_timer_queue_is_empty(const apt_timer_queue_t *timer_queue);

/** Get current timeout */
APT_DECLARE(apt_bool_t) apt_timer_queue_timeout_get(const apt_timer_queue_t *timer_queue, apr_uint32_t *timeout);


/** Create timer */
APT_DECLARE(apt_timer_t*) apt_timer_create(apt_timer_queue_t *timer_queue, apt_timer_proc_f proc, void *obj, apr_pool_t *pool);

/** Set one-shot timer */
APT_DECLARE(apt_bool_t) apt_timer_set(apt_timer_t *timer, apr_uint32_t timeout);

/** Kill timer */
APT_DECLARE(apt_bool_t) apt_timer_kill(apt_timer_t *timer);


APT_END_EXTERN_C

#endif /* APT_TIMER_QUEUE_H */
