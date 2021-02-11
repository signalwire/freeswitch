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

#ifndef MPF_SCHEDULER_H
#define MPF_SCHEDULER_H

/**
 * @file mpf_scheduler.h
 * @brief MPF Scheduler (High Resolution Clock for Media Processing and Timer)
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Prototype of scheduler callback */
typedef void (*mpf_scheduler_proc_f)(mpf_scheduler_t *scheduler, void *obj);

/** Create scheduler */
MPF_DECLARE(mpf_scheduler_t*) mpf_scheduler_create(apr_pool_t *pool);

/** Destroy scheduler */
MPF_DECLARE(void) mpf_scheduler_destroy(mpf_scheduler_t *scheduler);

/** Set media processing clock */
MPF_DECLARE(apt_bool_t) mpf_scheduler_media_clock_set(
								mpf_scheduler_t *scheduler,
								unsigned long resolution,
								mpf_scheduler_proc_f proc,
								void *obj);

/** Set timer clock */
MPF_DECLARE(apt_bool_t) mpf_scheduler_timer_clock_set(
								mpf_scheduler_t *scheduler,
								unsigned long resolution,
								mpf_scheduler_proc_f proc,
								void *obj);

/** Set scheduler rate (n times faster than real-time) */
MPF_DECLARE(apt_bool_t) mpf_scheduler_rate_set(
								mpf_scheduler_t *scheduler,
								unsigned long rate);

/** Start scheduler */
MPF_DECLARE(apt_bool_t) mpf_scheduler_start(mpf_scheduler_t *scheduler);

/** Stop scheduler */
MPF_DECLARE(apt_bool_t) mpf_scheduler_stop(mpf_scheduler_t *scheduler);


APT_END_EXTERN_C

#endif /* MPF_SCHEDULER_H */
