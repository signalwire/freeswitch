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
 * $Id: apt_timer_queue.c 2174 2014-09-12 03:33:16Z achaloyan@gmail.com $
 */

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h> 
#include "apt_timer_queue.h"
#include "apt_log.h"

/** Timer queue */
struct apt_timer_queue_t {
	/** Ring head */
	APR_RING_HEAD(apt_timer_head_t, apt_timer_t) head;

	/** Elapsed time */
	apr_uint32_t  elapsed_time;
};

/** Timer */
struct apt_timer_t {
	/** Ring entry */
	APR_RING_ENTRY(apt_timer_t) link;

	/** Back pointer to queue */
	apt_timer_queue_t   *queue;
	/** Time next report is scheduled at */
	apr_uint32_t         scheduled_time;

	/** Timer proc */
	apt_timer_proc_f     proc;
	/** Timer object */
	void                *obj;
};

static apt_bool_t apt_timer_insert(apt_timer_queue_t *timer_queue, apt_timer_t *timer);
static apt_bool_t apt_timer_remove(apt_timer_queue_t *timer_queue, apt_timer_t *timer);
static void apt_timers_reschedule(apt_timer_queue_t *timer_queue);

/** Create timer queue */
APT_DECLARE(apt_timer_queue_t*) apt_timer_queue_create(apr_pool_t *pool)
{
	apt_timer_queue_t *timer_queue = apr_palloc(pool,sizeof(apt_timer_queue_t));
	APR_RING_INIT(&timer_queue->head, apt_timer_t, link);
	timer_queue->elapsed_time = 0;
	return timer_queue;
}

/** Destroy timer queue */
APT_DECLARE(void) apt_timer_queue_destroy(apt_timer_queue_t *timer_queue)
{
	/* nothing to destroy */
}

/** Advance scheduled timers */
APT_DECLARE(void) apt_timer_queue_advance(apt_timer_queue_t *timer_queue, apr_uint32_t elapsed_time)
{
	apt_timer_t *timer;

	if(APR_RING_EMPTY(&timer_queue->head, apt_timer_t, link)) {
		/* just return, nothing to do */
		return;
	}

	/* increment elapsed time */
	timer_queue->elapsed_time += elapsed_time;
	if(timer_queue->elapsed_time >= 0xFFFF) {
#ifdef APT_TIMER_DEBUG
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Reschedule Timers [%u]",timer_queue->elapsed_time);
#endif
		apt_timers_reschedule(timer_queue);
	}

	/* process timers */
	do {
		/* get first node (timer) */
		timer = APR_RING_FIRST(&timer_queue->head);

		if(timer->scheduled_time > timer_queue->elapsed_time) {
			/* scheduled time is not elapsed yet */
			break;
		}

#ifdef APT_TIMER_DEBUG
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Timer Elapsed 0x%x [%u]",timer,timer->scheduled_time);
#endif
		/* remove the elapsed timer from the list */
		APR_RING_REMOVE(timer, link);
		timer->scheduled_time = 0;
		/* process the elapsed timer */
		timer->proc(timer,timer->obj);
	}
	while(!APR_RING_EMPTY(&timer_queue->head, apt_timer_t, link));
}

/** Is timer queue empty */
APT_DECLARE(apt_bool_t) apt_timer_queue_is_empty(const apt_timer_queue_t *timer_queue)
{
	return APR_RING_EMPTY(&timer_queue->head, apt_timer_t, link) ? TRUE : FALSE;
}

/** Get current timeout */
APT_DECLARE(apt_bool_t) apt_timer_queue_timeout_get(const apt_timer_queue_t *timer_queue, apr_uint32_t *timeout)
{
	apt_timer_t *timer;
	/* is queue empty */
	if(APR_RING_EMPTY(&timer_queue->head, apt_timer_t, link)) {
		return FALSE;
	}

	/* get first node (timer) */
	timer = APR_RING_FIRST(&timer_queue->head);
	if(!timer) {
		return FALSE;
	}

	*timeout = timer->scheduled_time - timer_queue->elapsed_time;
	return TRUE;
}

/** Create timer */
APT_DECLARE(apt_timer_t*) apt_timer_create(apt_timer_queue_t *timer_queue, apt_timer_proc_f proc, void *obj, apr_pool_t *pool)
{
	apt_timer_t *timer = apr_palloc(pool,sizeof(apt_timer_t));
	APR_RING_ELEM_INIT(timer,link);
	timer->queue = timer_queue;
	timer->scheduled_time = 0;
	timer->proc = proc;
	timer->obj = obj;
	return timer;
}

/** Set one-shot timer */
APT_DECLARE(apt_bool_t) apt_timer_set(apt_timer_t *timer, apr_uint32_t timeout)

{
	apt_timer_queue_t *queue = timer->queue;

	if(timeout <= 0 || !timer->proc) {
		return FALSE;
	}

	if(timer->scheduled_time) {
		/* remove timer first */
		apt_timer_remove(queue,timer);
	}

	/* calculate time to elapse */
	timer->scheduled_time = queue->elapsed_time + timeout;
#ifdef APT_TIMER_DEBUG
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Set Timer 0x%x [%u]",timer,timer->scheduled_time);
#endif
	if(APR_RING_EMPTY(&queue->head, apt_timer_t, link)) {
		APR_RING_INSERT_TAIL(&queue->head,timer,apt_timer_t,link);
		return TRUE;
	}

	/* insert new node (timer) to sorted by scheduled time list */
	return apt_timer_insert(queue,timer);
}

/** Kill timer */
APT_DECLARE(apt_bool_t) apt_timer_kill(apt_timer_t *timer)
{
	if(!timer->scheduled_time) {
		return FALSE;
	}

#ifdef APT_TIMER_DEBUG
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Kill Timer 0x%x [%u]",timer,timer->scheduled_time);
#endif
	return apt_timer_remove(timer->queue,timer);
}

static apt_bool_t apt_timer_insert(apt_timer_queue_t *timer_queue, apt_timer_t *timer)
{
	apt_timer_t *it;
	for(it = APR_RING_LAST(&timer_queue->head);
			it != APR_RING_SENTINEL(&timer_queue->head, apt_timer_t, link);
				it = APR_RING_PREV(it, link)) {
		
		if(it->scheduled_time <= timer->scheduled_time) {
			APR_RING_INSERT_AFTER(it,timer,link);
			return TRUE;
		}
	}
	APR_RING_INSERT_HEAD(&timer_queue->head,timer,apt_timer_t,link);
	return TRUE;
}

static apt_bool_t apt_timer_remove(apt_timer_queue_t *timer_queue, apt_timer_t *timer)
{
	/* remove node (timer) from the list */
	APR_RING_REMOVE(timer,link);
	timer->scheduled_time = 0;

	if(APR_RING_EMPTY(&timer_queue->head, apt_timer_t, link)) {
		/* reset elapsed time if no timers set */
		timer_queue->elapsed_time = 0;
	}
	return TRUE;
}

static void apt_timers_reschedule(apt_timer_queue_t *timer_queue)
{
	apt_timer_t *it;
	for(it = APR_RING_LAST(&timer_queue->head);
			it != APR_RING_SENTINEL(&timer_queue->head, apt_timer_t, link);
				it = APR_RING_PREV(it, link)) {

		it->scheduled_time -= timer_queue->elapsed_time;
	}
	timer_queue->elapsed_time = 0;
}
