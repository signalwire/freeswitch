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

#include <apr_time.h>
#include <apr_queue.h>
#include "apt_consumer_task.h"
#include "apt_log.h"

struct apt_consumer_task_t {
	void              *obj;
	apt_task_t        *base;
	apr_queue_t       *msg_queue;
#if APR_HAS_QUEUE_TIMEOUT
	apt_timer_queue_t *timer_queue;
#endif
};

static apt_bool_t apt_consumer_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t apt_consumer_task_run(apt_task_t *task);

APT_DECLARE(apt_consumer_task_t*) apt_consumer_task_create(
									void *obj,
									apt_task_msg_pool_t *msg_pool,
									apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_consumer_task_t *consumer_task = apr_palloc(pool,sizeof(apt_consumer_task_t));
	consumer_task->obj = obj;
	consumer_task->msg_queue = NULL;
	if(apr_queue_create(&consumer_task->msg_queue,1024,pool) != APR_SUCCESS) {
		return NULL;
	}
	
	consumer_task->base = apt_task_create(consumer_task,msg_pool,pool);
	if(!consumer_task->base) {
		return NULL;
	}

	vtable = apt_task_vtable_get(consumer_task->base);
	if(vtable) {
		vtable->run = apt_consumer_task_run;
		vtable->signal_msg = apt_consumer_task_msg_signal;
	}

#if APR_HAS_QUEUE_TIMEOUT
	consumer_task->timer_queue = apt_timer_queue_create(pool);
#endif

	return consumer_task;
}

APT_DECLARE(apt_task_t*) apt_consumer_task_base_get(const apt_consumer_task_t *task)
{
	return task->base;
}

APT_DECLARE(apt_task_vtable_t*) apt_consumer_task_vtable_get(const apt_consumer_task_t *task)
{
	return apt_task_vtable_get(task->base);
}

APT_DECLARE(void*) apt_consumer_task_object_get(const apt_consumer_task_t *task)
{
	return task->obj;
}

APT_DECLARE(apt_timer_t*) apt_consumer_task_timer_create(
									apt_consumer_task_t *task, 
									apt_timer_proc_f proc, 
									void *obj, 
									apr_pool_t *pool)
{
#if APR_HAS_QUEUE_TIMEOUT
	return apt_timer_create(task->timer_queue,proc,obj,pool);
#else
	return NULL;
#endif
}

static apt_bool_t apt_consumer_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	return (apr_queue_push(consumer_task->msg_queue,msg) == APR_SUCCESS) ? TRUE : FALSE;
}

static apt_bool_t apt_consumer_task_run(apt_task_t *task)
{
	apr_status_t rv;
	void *msg;
	apt_bool_t *running;
	apt_consumer_task_t *consumer_task;
#if APR_HAS_QUEUE_TIMEOUT
	apr_interval_time_t timeout;
	apr_uint32_t queue_timeout;
	apr_time_t time_now, time_last = 0;
#endif
	const char *task_name;

	consumer_task = apt_task_object_get(task);
	if(!consumer_task) {
		return FALSE;
	}
	task_name = apt_task_name_get(consumer_task->base),

	running = apt_task_running_flag_get(task);
	if(!running) {
		return FALSE;
	}

	while(*running) {
#if APR_HAS_QUEUE_TIMEOUT
		if(apt_timer_queue_timeout_get(consumer_task->timer_queue,&queue_timeout) == TRUE) {
			timeout = (apr_interval_time_t)queue_timeout * 1000;
			time_last = apr_time_now();
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Messages [%s] timeout [%u]",
				task_name, queue_timeout);
			rv = apr_queue_timedpop(consumer_task->msg_queue,timeout,&msg);
		}
		else
		{
			timeout = -1;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Messages [%s]",task_name);
			rv = apr_queue_pop(consumer_task->msg_queue,&msg);
		}
#else
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Messages [%s]",task_name);
		rv = apr_queue_pop(consumer_task->msg_queue,&msg);
#endif
		if(rv == APR_SUCCESS) {
			if(msg) {
				apt_task_msg_t *task_msg = msg;
				apt_task_msg_process(consumer_task->base,task_msg);
			}
		}
		else if(rv != APR_TIMEUP) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Pop Message [%s] status: %d",task_name,rv);
		}

#if APR_HAS_QUEUE_TIMEOUT
		if(timeout != -1) {
			time_now = apr_time_now();
			if(time_now > time_last) {
				apt_timer_queue_advance(consumer_task->timer_queue,(apr_uint32_t)((time_now - time_last)/1000));
			}
			else {
				/* If NTP has drifted the clock backwards, advance the queue based on the set timeout but not actual time difference */
				if(rv == APR_TIMEUP) {
					apt_timer_queue_advance(consumer_task->timer_queue,queue_timeout);
				}
			}
		}
#endif
	}
	return TRUE;
}
