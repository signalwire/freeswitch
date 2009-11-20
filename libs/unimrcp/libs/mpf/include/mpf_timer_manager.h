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

#ifndef __MPF_TIMER_MANAGER_H__
#define __MPF_TIMER_MANAGER_H__

/**
 * @file mpf_timer_manager.h
 * @brief MPF Timer Management
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C


/** Prototype of timer callback */
typedef void (*mpf_timer_proc_f)(mpf_timer_t *timer, void *obj);


/** Create timer manager */
MPF_DECLARE(mpf_timer_manager_t*) mpf_timer_manager_create(mpf_scheduler_t *scheduler, apr_pool_t *pool);

/** Destroy timer manager */
MPF_DECLARE(void) mpf_timer_manager_destroy(mpf_timer_manager_t *timer_manager);


/** Create timer */
MPF_DECLARE(mpf_timer_t*) mpf_timer_create(mpf_timer_manager_t *timer_manager, mpf_timer_proc_f proc, void *obj, apr_pool_t *pool);

/** Set one-shot timer */
MPF_DECLARE(apt_bool_t) mpf_timer_set(mpf_timer_t *timer, apr_uint32_t timeout);

/** Kill timer */
MPF_DECLARE(apt_bool_t) mpf_timer_kill(mpf_timer_t *timer);


APT_END_EXTERN_C

#endif /*__MPF_TIMER_MANAGER_H__*/
