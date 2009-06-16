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

#include "mpf_timer.h"

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

struct mpf_timer_t {
	unsigned int     timer_id;
	mpf_timer_proc_f timer_proc;
	void            *obj;
};

#define MAX_MEDIA_TIMERS 10

static mpf_timer_t media_timer_set[MAX_MEDIA_TIMERS];

static void CALLBACK mm_timer_proc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

MPF_DECLARE(mpf_timer_t*) mpf_timer_start(unsigned long timeout, mpf_timer_proc_f timer_proc, void *obj, apr_pool_t *pool)
{
	mpf_timer_t *timer = NULL;
	size_t i;
	for(i = 0; i<MAX_MEDIA_TIMERS; i++) {
		if(!media_timer_set[i].timer_id) {
			timer = &media_timer_set[i];
			break;
		}
	}
		
	if(timer) {
		timer->timer_proc = timer_proc;
		timer->obj = obj;
		timer->timer_id = timeSetEvent(timeout, 0, mm_timer_proc, i, 
			TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
		if(!timer->timer_id) {
			timer = NULL;
		}
	}
	return timer;
}

MPF_DECLARE(void) mpf_timer_stop(mpf_timer_t *timer)
{
	if(timer) {
		timeKillEvent(timer->timer_id);
		timer->timer_id = 0;
		timer->timer_proc = NULL;
		timer->obj = NULL;
	}
}

static void CALLBACK mm_timer_proc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	mpf_timer_t *timer;
	if(dwUser >= MAX_MEDIA_TIMERS) {
		return;
	}
	timer = &media_timer_set[dwUser];
	timer->timer_proc(timer,timer->obj);
}

#else

#include <apr_thread_proc.h>

struct mpf_timer_t {
	apr_thread_t    *thread;
	apr_byte_t       running;

	unsigned long    timeout;
	mpf_timer_proc_f timer_proc;
	void            *obj;
};

static void* APR_THREAD_FUNC timer_thread_proc(apr_thread_t *thread, void *data);

MPF_DECLARE(mpf_timer_t*) mpf_timer_start(unsigned long timeout, mpf_timer_proc_f timer_proc, void *obj, apr_pool_t *pool)
{
	mpf_timer_t *timer = apr_palloc(pool,sizeof(mpf_timer_t));
	timer->timeout = timeout;
	timer->timer_proc = timer_proc;
	timer->obj = obj;
	timer->running = 1;
	if(apr_thread_create(&timer->thread,NULL,timer_thread_proc,timer,pool) != APR_SUCCESS) {
		return NULL;
	}
	return timer;
}

MPF_DECLARE(void) mpf_timer_stop(mpf_timer_t *timer)
{
	if(timer) {
		timer->running = 0;
		if(timer->thread) {
			apr_status_t s;
			apr_thread_join(&s,timer->thread);
			timer->thread = NULL;
		}
	}
}

static void* APR_THREAD_FUNC timer_thread_proc(apr_thread_t *thread, void *data)
{
	mpf_timer_t *timer = data;
	apr_interval_time_t timeout = timer->timeout * 1000;
	apr_interval_time_t time_drift = 0;
	apr_time_t time_now, time_last;
	
	time_now = apr_time_now();
	while(timer->running) {
		time_last = time_now;
		timer->timer_proc(timer,timer->obj);

		if(timeout > time_drift) {
			apr_sleep(timeout - time_drift);
		}

		time_now = apr_time_now();
		time_drift += time_now - time_last - timeout;
#if 0
		printf("time_drift=%d\n",time_drift);
#endif
	}
	
	return NULL;
}

#endif
