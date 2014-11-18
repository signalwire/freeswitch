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
 * $Id: mpf_scheduler.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_scheduler.h"

#ifdef WIN32
#define ENABLE_MULTIMEDIA_TIMERS
#endif

#ifdef ENABLE_MULTIMEDIA_TIMERS

#pragma warning(disable:4201)
#include <mmsystem.h>
#include <windows.h>

#ifndef TIME_KILL_SYNCHRONOUS
#define TIME_KILL_SYNCHRONOUS   0x0100
#endif

#else
#include <apr_thread_proc.h>
#endif


struct mpf_scheduler_t {
	apr_pool_t          *pool;
	unsigned long        resolution; /* scheduler resolution */

	unsigned long        media_resolution;
	mpf_scheduler_proc_f media_proc;
	void                *media_obj;
	
	unsigned long        timer_resolution;
	unsigned long        timer_elapsed_time;
	mpf_scheduler_proc_f timer_proc;
	void                *timer_obj;

#ifdef ENABLE_MULTIMEDIA_TIMERS
	unsigned int         timer_id;
#else
	apr_thread_t        *thread;
	apt_bool_t           running;
#endif
};

static APR_INLINE void mpf_scheduler_init(mpf_scheduler_t *scheduler);

/** Create scheduler */
MPF_DECLARE(mpf_scheduler_t*) mpf_scheduler_create(apr_pool_t *pool)
{
	mpf_scheduler_t *scheduler = apr_palloc(pool,sizeof(mpf_scheduler_t));
	mpf_scheduler_init(scheduler);
	scheduler->pool = pool;
	scheduler->resolution = 0;

	scheduler->media_resolution = 0;
	scheduler->media_obj = NULL;
	scheduler->media_proc = NULL;

	scheduler->timer_resolution = 0;
	scheduler->timer_elapsed_time = 0;
	scheduler->timer_obj = NULL;
	scheduler->timer_proc = NULL;
	return scheduler;
}

/** Destroy scheduler */
MPF_DECLARE(void) mpf_scheduler_destroy(mpf_scheduler_t *scheduler)
{
	/* nothing to destroy */
}

/** Set media processing clock */
MPF_DECLARE(apt_bool_t) mpf_scheduler_media_clock_set(
								mpf_scheduler_t *scheduler,
								unsigned long resolution,
								mpf_scheduler_proc_f proc,
								void *obj)
{
	scheduler->media_resolution = resolution;
	scheduler->media_proc = proc;
	scheduler->media_obj = obj;
	return TRUE;
}

/** Set timer clock */
MPF_DECLARE(apt_bool_t) mpf_scheduler_timer_clock_set(
								mpf_scheduler_t *scheduler,
								unsigned long resolution,
								mpf_scheduler_proc_f proc,
								void *obj)
{
	scheduler->timer_resolution = resolution;
	scheduler->timer_elapsed_time = 0;
	scheduler->timer_proc = proc;
	scheduler->timer_obj = obj;
	return TRUE;
}

/** Set scheduler rate (n times faster than real-time) */
MPF_DECLARE(apt_bool_t) mpf_scheduler_rate_set(
								mpf_scheduler_t *scheduler,
								unsigned long rate)
{
	if(rate == 0 || rate > 10) {
		/* rate shows how many times scheduler should be faster than real-time,
		1 is the defualt and probably the only reasonable value, 
		however, the rates up to 10 times faster should be acceptable */
		rate = 1;
	}
	
	scheduler->media_resolution /= rate;
	scheduler->timer_resolution /= rate;
	return TRUE;
}

static APR_INLINE void mpf_scheduler_resolution_set(mpf_scheduler_t *scheduler)
{
	if(scheduler->media_resolution) {
		scheduler->resolution = scheduler->media_resolution;
	}
	else if(scheduler->timer_resolution) {
		scheduler->resolution = scheduler->timer_resolution;
	}
}



#ifdef ENABLE_MULTIMEDIA_TIMERS

static APR_INLINE void mpf_scheduler_init(mpf_scheduler_t *scheduler)
{
	scheduler->timer_id = 0;
}

static void CALLBACK mm_timer_proc(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	mpf_scheduler_t *scheduler = (mpf_scheduler_t*) dwUser;
	if(scheduler->media_proc) {
		scheduler->media_proc(scheduler,scheduler->media_obj);
	}

	if(scheduler->timer_proc) {
		scheduler->timer_elapsed_time += scheduler->resolution;
		if(scheduler->timer_elapsed_time >= scheduler->timer_resolution) {
			scheduler->timer_elapsed_time = 0;
			scheduler->timer_proc(scheduler,scheduler->timer_obj);
		}
	}
}

/** Start scheduler */
MPF_DECLARE(apt_bool_t) mpf_scheduler_start(mpf_scheduler_t *scheduler)
{
	mpf_scheduler_resolution_set(scheduler);
	scheduler->timer_id = timeSetEvent(
					scheduler->resolution, 0, mm_timer_proc, (DWORD_PTR) scheduler, 
					TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
	return scheduler->timer_id ? TRUE : FALSE;
}

/** Stop scheduler */
MPF_DECLARE(apt_bool_t) mpf_scheduler_stop(mpf_scheduler_t *scheduler)
{
	if(!scheduler) {
		return FALSE;
	}

	timeKillEvent(scheduler->timer_id);
	scheduler->timer_id = 0;
	return TRUE;
}

#else

#include "apt_task.h"

static APR_INLINE void mpf_scheduler_init(mpf_scheduler_t *scheduler)
{
	scheduler->thread = NULL;
	scheduler->running = FALSE;
}

static void* APR_THREAD_FUNC timer_thread_proc(apr_thread_t *thread, void *data)
{
	mpf_scheduler_t *scheduler = data;
	apr_interval_time_t timeout = scheduler->resolution * 1000;
	apr_interval_time_t time_drift = 0;
	apr_time_t time_now, time_last;
	
#if APR_HAS_SETTHREADNAME
	apr_thread_name_set("MPF Scheduler");
#endif
	time_now = apr_time_now();
	while(scheduler->running == TRUE) {
		time_last = time_now;

		if(scheduler->media_proc) {
			scheduler->media_proc(scheduler,scheduler->media_obj);
		}

		if(scheduler->timer_proc) {
			scheduler->timer_elapsed_time += scheduler->resolution;
			if(scheduler->timer_elapsed_time >= scheduler->timer_resolution) {
				scheduler->timer_elapsed_time = 0;
				scheduler->timer_proc(scheduler,scheduler->timer_obj);
			}
		}

		if(timeout > time_drift) {
			apr_sleep(timeout - time_drift);
		}

		time_now = apr_time_now();
		time_drift += time_now - time_last - timeout;
#if 0
		printf("time_drift=%d\n",time_drift);
#endif
	}
	
	apr_thread_exit(thread,APR_SUCCESS);
	return NULL;
}

MPF_DECLARE(apt_bool_t) mpf_scheduler_start(mpf_scheduler_t *scheduler)
{
	mpf_scheduler_resolution_set(scheduler);
	
	scheduler->running = TRUE;
	if(apr_thread_create(&scheduler->thread,NULL,timer_thread_proc,scheduler,scheduler->pool) != APR_SUCCESS) {
		scheduler->running = FALSE;
		return FALSE;
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_scheduler_stop(mpf_scheduler_t *scheduler)
{
	if(!scheduler) {
		return FALSE;
	}

	scheduler->running = FALSE;
	if(scheduler->thread) {
		apr_status_t s;
		apr_thread_join(&s,scheduler->thread);
		scheduler->thread = NULL;
	}
	return TRUE;
}

#endif
