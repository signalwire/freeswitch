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

#include <apr_queue.h>
#include "apt_consumer_task.h"
#include "apt_log.h"

struct apt_consumer_task_t {
	void        *obj;
	apt_task_t  *base;
	apr_queue_t *msg_queue;
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
	return consumer_task;
}

APT_DECLARE(apt_task_t*) apt_consumer_task_base_get(apt_consumer_task_t *task)
{
	return task->base;
}

APT_DECLARE(apt_task_vtable_t*) apt_consumer_task_vtable_get(apt_consumer_task_t *task)
{
	return apt_task_vtable_get(task->base);
}

APT_DECLARE(void*) apt_consumer_task_object_get(apt_consumer_task_t *task)
{
	return task->obj;
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
	apt_bool_t running = TRUE;
	apt_consumer_task_t *consumer_task;
	consumer_task = apt_task_object_get(task);
	if(!consumer_task) {
		return FALSE;
	}

	while(running) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for Task Messages [%s]",apt_task_name_get(task));
		rv = apr_queue_pop(consumer_task->msg_queue,&msg);
		if(rv == APR_SUCCESS) {
			if(msg) {
				apt_task_msg_t *task_msg = msg;
				if(apt_task_msg_process(consumer_task->base,task_msg) == FALSE) {
					running = FALSE;
				}
			}
		}
	}
	return TRUE;
}
