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
 * $Id: consumer_task_suite.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <apr_time.h>
#include "apt_test_suite.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

typedef struct {
	apr_time_t timestamp;
	int        number;
} sample_msg_data_t;

static void task_on_start_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On Task Start");
}

static void task_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On Task Terminate");
}

static apt_bool_t task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	sample_msg_data_t *data = (sample_msg_data_t*)msg->data;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Message [%d]",data->number);
	return TRUE;
}


static apt_bool_t consumer_task_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	apt_consumer_task_t *consumer_task;
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	apt_task_msg_t *msg;
	sample_msg_data_t *data;
	int i;
	
	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(sample_msg_data_t),suite->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Consumer Task");
	consumer_task = apt_consumer_task_create(NULL,msg_pool,suite->pool);
	if(!consumer_task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Consumer Task");
		return FALSE;
	}
	task = apt_consumer_task_base_get(consumer_task);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = task_msg_process;
		vtable->on_start_complete = task_on_start_complete;
		vtable->on_terminate_complete = task_on_terminate_complete;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Start Task");
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	for(i=0; i<10; i++) {
		msg = apt_task_msg_acquire(msg_pool);
		msg->type = TASK_MSG_USER;
		data = (sample_msg_data_t*) msg->data;

		data->number = i;
		data->timestamp = apr_time_now();
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Message [%d]",data->number);
		apt_task_msg_signal(task,msg);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate Task [wait till complete]");
	apt_task_terminate(task,TRUE);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Task");
	apt_task_destroy(task);
	return TRUE;
}

apt_test_suite_t* consumer_task_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"consumer",NULL,consumer_task_test_run);
	return suite;
}
