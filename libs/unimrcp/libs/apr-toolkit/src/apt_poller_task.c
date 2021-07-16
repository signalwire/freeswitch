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

#include "apt_poller_task.h"
#include "apt_task.h"
#include "apt_pool.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"


/** Poller task */
struct apt_poller_task_t {
	apr_pool_t         *pool;
	apt_task_t         *base;
	
	void               *obj;
	apt_poll_signal_f   signal_handler;

	apr_thread_mutex_t *guard;
	apt_cyclic_queue_t *msg_queue;
	apt_pollset_t      *pollset;
	apt_timer_queue_t  *timer_queue;

	apr_pollfd_t       *desc_arr;
	apr_int32_t         desc_count;
	apr_int32_t         desc_index;

};

static apt_bool_t apt_poller_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t apt_poller_task_run(apt_task_t *task);
static apt_bool_t apt_poller_task_on_destroy(apt_task_t *task);


/** Create poller task */
APT_DECLARE(apt_poller_task_t*) apt_poller_task_create(
										apr_size_t max_pollset_size,
										apt_poll_signal_f signal_handler,
										void *obj,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_poller_task_t *task;

	if(!signal_handler) {
		return NULL;
	}
	
	task = apr_palloc(pool,sizeof(apt_poller_task_t));
	task->pool = pool;
	task->obj = obj;
	task->pollset = NULL;
	task->signal_handler = signal_handler;

	task->pollset = apt_pollset_create((apr_uint32_t)max_pollset_size,pool);
	if(!task->pollset) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return NULL;
	}

	task->base = apt_task_create(task,msg_pool,pool);
	if(!task->base) {
		apt_pollset_destroy(task->pollset);
		return NULL;
	}

	vtable = apt_task_vtable_get(task->base);
	if(vtable) {
		vtable->run = apt_poller_task_run;
		vtable->destroy = apt_poller_task_on_destroy;
		vtable->signal_msg = apt_poller_task_msg_signal;
	}
	apt_task_auto_ready_set(task->base,FALSE);

	task->msg_queue = apt_cyclic_queue_create(CYCLIC_QUEUE_DEFAULT_SIZE);
	apr_thread_mutex_create(&task->guard,APR_THREAD_MUTEX_UNNESTED,pool);

	task->timer_queue = apt_timer_queue_create(pool);
	task->desc_arr = NULL;
	task->desc_count = 0;
	task->desc_index = 0;
	return task;
}

/** Destroy poller task */
APT_DECLARE(apt_bool_t) apt_poller_task_destroy(apt_poller_task_t *task)
{
	return apt_task_destroy(task->base);
}

/** Cleanup poller task */
APT_DECLARE(void) apt_poller_task_cleanup(apt_poller_task_t *task)
{
	if(task->pollset) {
		apt_pollset_destroy(task->pollset);
		task->pollset = NULL;
	}
	if(task->guard) {
		apr_thread_mutex_destroy(task->guard);
		task->guard = NULL;
	}
	if(task->msg_queue) {
		apt_cyclic_queue_destroy(task->msg_queue);
		task->msg_queue = NULL;
	}
}

/** Virtual destroy handler */
static apt_bool_t apt_poller_task_on_destroy(apt_task_t *base)
{
	apt_poller_task_t *task = apt_task_object_get(base);
	apt_poller_task_cleanup(task);
	return TRUE;
}


/** Start poller task */
APT_DECLARE(apt_bool_t) apt_poller_task_start(apt_poller_task_t *task)
{
	return apt_task_start(task->base);
}

/** Terminate poller task */
APT_DECLARE(apt_bool_t) apt_poller_task_terminate(apt_poller_task_t *task)
{
	return apt_task_terminate(task->base,TRUE);
}

/** Get task */
APT_DECLARE(apt_task_t*) apt_poller_task_base_get(const apt_poller_task_t *task)
{
	return task->base;
}

/** Get task vtable */
APT_DECLARE(apt_task_vtable_t*) apt_poller_task_vtable_get(const apt_poller_task_t *task)
{
	return apt_task_vtable_get(task->base);
}

/** Get external object */
APT_DECLARE(void*) apt_poller_task_object_get(const apt_poller_task_t *task)
{
	return task->obj;
}

/** Add descriptor to pollset */
APT_DECLARE(apt_bool_t) apt_poller_task_descriptor_add(const apt_poller_task_t *task, const apr_pollfd_t *descriptor)
{
	if(task->pollset) {
		return apt_pollset_add(task->pollset,descriptor);
	}
	return FALSE;
}

/** Remove descriptor from pollset */
APT_DECLARE(apt_bool_t) apt_poller_task_descriptor_remove(const apt_poller_task_t *task, const apr_pollfd_t *descriptor)
{
	if(task->pollset) {
		apr_int32_t i = task->desc_index + 1;
		for(; i < task->desc_count; i++) {
			apr_pollfd_t *cur_descriptor = &task->desc_arr[i];
			if(cur_descriptor->client_data == descriptor->client_data) {
				cur_descriptor->client_data = NULL;
			}
		}
		return apt_pollset_remove(task->pollset,descriptor);
	}
	return FALSE;
}

/** Create timer */
APT_DECLARE(apt_timer_t*) apt_poller_task_timer_create(
									apt_poller_task_t *task, 
									apt_timer_proc_f proc, 
									void *obj, 
									apr_pool_t *pool)
{
	return apt_timer_create(task->timer_queue,proc,obj,pool);
}

static apt_bool_t apt_poller_task_wakeup_process(apt_poller_task_t *task)
{
	apt_bool_t running = TRUE;
	apt_task_msg_t *msg;

	do {
		apr_thread_mutex_lock(task->guard);
		msg = apt_cyclic_queue_pop(task->msg_queue);
		apr_thread_mutex_unlock(task->guard);
		if(msg) {
			apt_task_msg_process(task->base,msg);
		}
		else {
			running = FALSE;
		}
	}
	while(running == TRUE);
	return TRUE;
}

static apt_bool_t apt_poller_task_run(apt_task_t *base)
{
	apt_poller_task_t *task = apt_task_object_get(base);
	apt_bool_t *running;
	apr_status_t status;
	apr_interval_time_t timeout;
	apr_uint32_t queue_timeout;
	apr_time_t time_now, time_last = 0;
	const char *task_name;

	if(!task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Poller Task");
		return FALSE;
	}
	task_name = apt_task_name_get(task->base);

	running = apt_task_running_flag_get(task->base);
	if(!running) {
		return FALSE;
	}

	/* explicitly indicate task is ready to process messages */
	apt_task_ready(task->base);

	while(*running) {
		if(apt_timer_queue_timeout_get(task->timer_queue,&queue_timeout) == TRUE) {
			timeout = (apr_interval_time_t)queue_timeout * 1000;
			time_last = apr_time_now();
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Messages [%s] timeout [%u]",
				task_name, queue_timeout);
		}
		else {
			timeout = -1;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Messages [%s]",task_name);
		}
		status = apt_pollset_poll(task->pollset, timeout, &task->desc_count, (const apr_pollfd_t **) &task->desc_arr);
		if(status != APR_SUCCESS && status != APR_TIMEUP) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Poll [%s] status: %d",task_name,status);
			continue;
		}
		for(task->desc_index = 0; task->desc_index < task->desc_count; task->desc_index++) {
			const apr_pollfd_t *descriptor = &task->desc_arr[task->desc_index];
			if(apt_pollset_is_wakeup(task->pollset,descriptor)) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Poller Wakeup [%s]",task_name);
				apt_poller_task_wakeup_process(task);
				if(*running == FALSE) {
					break;
				}
				continue;
			}

			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Signalled Descriptor [%s]",task_name);
			task->signal_handler(task->obj,descriptor);
		}

		if(timeout != -1) {
			time_now = apr_time_now();
			if(time_now > time_last) {
				apt_timer_queue_advance(task->timer_queue,(apr_uint32_t)((time_now - time_last)/1000));
			}
			else {
				/* If NTP has drifted the clock backwards, advance the queue based on the set timeout but not actual time difference */
				if(status == APR_TIMEUP) {
					apt_timer_queue_advance(task->timer_queue,queue_timeout);
				}
			}
		}
	}

	return TRUE;
}

static apt_bool_t apt_poller_task_msg_signal(apt_task_t *base, apt_task_msg_t *msg)
{
	apt_bool_t status;
	apt_poller_task_t *task = apt_task_object_get(base);
	apr_thread_mutex_lock(task->guard);
	status = apt_cyclic_queue_push(task->msg_queue,msg);
	apr_thread_mutex_unlock(task->guard);
	if(apt_pollset_wakeup(task->pollset) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		status = FALSE;
	}
	return status;
}
