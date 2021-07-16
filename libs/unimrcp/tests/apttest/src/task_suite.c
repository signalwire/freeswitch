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

#include "apt_test_suite.h"
#include "apt_task.h"
#include "apt_log.h"

static apt_bool_t task_main(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Do the Job");
	apt_task_delay(3000);
	return TRUE;
}

static apt_bool_t task_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Task");
	task = apt_task_create(NULL,NULL,suite->pool);
	if(!task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Task");
		return FALSE;
	}
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->run = task_main;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Start Task");
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Wait for Task to Complete");
	apt_task_wait_till_complete(task);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Task");
	apt_task_destroy(task);
	return TRUE;
}

apt_test_suite_t* task_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"task",NULL,task_test_run);
	return suite;
}
