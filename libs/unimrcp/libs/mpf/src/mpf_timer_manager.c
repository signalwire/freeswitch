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

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h> 
#include "mpf_timer_manager.h"
#include "mpf_scheduler.h"
#include "apt_log.h"

/** MPF timer manager */
struct mpf_timer_manager_t {
	/** Ring head */
	APR_RING_HEAD(mpf_timer_head_t, mpf_timer_t) head;

	/** Clock resolution */
	apr_uint32_t  resolution;
	/** Elapsed time */
	apr_uint32_t  elapsed_time;
};

/** MPF timer */
struct mpf_timer_t {
	/** Ring entry */
	APR_RING_ENTRY(mpf_timer_t) link;

	/** Back pointer to manager */
	mpf_timer_manager_t *manager;
	/** Time next report is scheduled at */
	apr_uint32_t         scheduled_time;

	/** Timer proc */
	mpf_timer_proc_f     proc;
	/** Timer object */
	void                *obj;
};

static void mpf_scheduler_proc(mpf_scheduler_t *scheduler, void *obj);

/** Create timer manager */
MPF_DECLARE(mpf_timer_manager_t*) mpf_timer_manager_create(mpf_scheduler_t *scheduler, apr_pool_t *pool)
{
	mpf_timer_manager_t *timer_manager = apr_palloc(pool,sizeof(mpf_timer_manager_t));
	APR_RING_INIT(&timer_manager->head, mpf_timer_t, link);
	timer_manager->elapsed_time = 0;
	timer_manager->resolution = 100; /* 100 ms */

	mpf_scheduler_timer_clock_set(scheduler,timer_manager->resolution,mpf_scheduler_proc,timer_manager);
	return timer_manager;
}

/** Destroy timer manager */
MPF_DECLARE(void) mpf_timer_manager_destroy(mpf_timer_manager_t *timer_manager)
{
}


/** Create timer */
MPF_DECLARE(mpf_timer_t*) mpf_timer_create(mpf_timer_manager_t *timer_manager, mpf_timer_proc_f proc, void *obj, apr_pool_t *pool)
{
	mpf_timer_t *timer = apr_palloc(pool,sizeof(mpf_timer_t));
	timer->manager = timer_manager;
	timer->scheduled_time = 0;
	timer->proc = proc;
	timer->obj = obj;
	return timer;
}

static APR_INLINE apt_bool_t mpf_timer_insert(mpf_timer_manager_t *manager, mpf_timer_t *timer)
{
	mpf_timer_t *it;
	for(it = APR_RING_LAST(&manager->head);
			it != APR_RING_SENTINEL(&manager->head, mpf_timer_t, link);
				it = APR_RING_PREV(it, link)) {
		
		if(it->scheduled_time <= timer->scheduled_time) {
			APR_RING_INSERT_AFTER(it,timer,link);
			return TRUE;
		}
	}
	APR_RING_INSERT_HEAD(&manager->head,timer,mpf_timer_t,link);
	return TRUE;
}

/** Set one-shot timer */
MPF_DECLARE(apt_bool_t) mpf_timer_set(mpf_timer_t *timer, apr_uint32_t timeout)

{
	mpf_timer_manager_t *manager = timer->manager;

	if(timeout <= 0 || !timer->proc) {
		return FALSE;
	}
	
	/* calculate time to elapse */
	timer->scheduled_time = manager->elapsed_time + timeout;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Set Timer 0x%x [%lu]",timer,timer->scheduled_time);

	if(APR_RING_EMPTY(&timer->manager->head, mpf_timer_t, link)) {
		APR_RING_INSERT_TAIL(&manager->head,timer,mpf_timer_t,link);
		return TRUE;
	}

	/* insert new node (timer) to sorted by scheduled time list */
	return mpf_timer_insert(manager,timer);
}

/** Kill timer */
MPF_DECLARE(apt_bool_t) mpf_timer_kill(mpf_timer_t *timer)
{
	if(!timer->scheduled_time) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Kill Timer 0x%x [%lu]",timer,timer->scheduled_time);
	/* remove node (timer) from the list */
	APR_RING_REMOVE(timer,link);
	timer->scheduled_time = 0;

	if(APR_RING_EMPTY(&timer->manager->head, mpf_timer_t, link)) {
		/* reset elapsed time if no timers set */
		timer->manager->elapsed_time = 0;
	}
	return TRUE;
}

static void mpf_timers_reschedule(mpf_timer_manager_t *manager)
{
	mpf_timer_t *it;
	for(it = APR_RING_LAST(&manager->head);
			it != APR_RING_SENTINEL(&manager->head, mpf_timer_t, link);
				it = APR_RING_PREV(it, link)) {
		
		it->scheduled_time -= manager->elapsed_time;
	}
	manager->elapsed_time = 0;
}

static void mpf_scheduler_proc(mpf_scheduler_t *scheduler, void *obj)
{
	mpf_timer_manager_t *manager = obj;
	mpf_timer_t *timer;

	if(APR_RING_EMPTY(&manager->head, mpf_timer_t, link)) {
		/* just return, nothing to do */
		return;
	}

	/* increment elapsed time */
	manager->elapsed_time += manager->resolution;
	if(manager->elapsed_time >= 0xFFFF) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Reschedule Timers [%lu]",manager->elapsed_time);
		mpf_timers_reschedule(manager);
	}

	/* process timers */
	do {
		/* get first node (timer) */
		timer = APR_RING_FIRST(&manager->head);

		if(timer->scheduled_time > manager->elapsed_time) {
			/* scheduled time is not elapsed yet */
			break;
		}
		
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Timer Elapsed 0x%x [%lu]",timer,timer->scheduled_time);
		/* remove the elapsed timer from the list */
		APR_RING_REMOVE(timer, link);
		timer->scheduled_time = 0;
		/* process the elapsed timer */
		timer->proc(timer,timer->obj);
	}
	while(!APR_RING_EMPTY(&manager->head, mpf_timer_t, link));
}
