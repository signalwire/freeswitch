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

#ifndef __MPF_TIMER_H__
#define __MPF_TIMER_H__

/**
 * @file mpf_timer.h
 * @brief MPF High Resolution Timer
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Opaque MPF timer declaration */
typedef struct mpf_timer_t mpf_timer_t;

/** Prototype of timer callback */
typedef void (*mpf_timer_proc_f)(mpf_timer_t *timer, void *obj);

/** Start periodic timer */
MPF_DECLARE(mpf_timer_t*) mpf_timer_start(unsigned long timeout, mpf_timer_proc_f timer_proc, void *obj, apr_pool_t *pool);

/** Stop timer */
MPF_DECLARE(void) mpf_timer_stop(mpf_timer_t *timer);


APT_END_EXTERN_C

#endif /*__MPF_TIMER_H__*/
