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

#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include "apt_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

/** Internal states of the task */
typedef enum {
	TASK_STATE_IDLE,               /**< no task activity */
	TASK_STATE_START_REQUESTED,    /**< task start is requested and is in progress */
	TASK_STATE_RUNNING,            /**< task is running */
	TASK_STATE_TERMINATE_REQUESTED /**< task termination is requested and is in progress */
} apt_task_state_t;

struct apt_task_t {
	void                *obj;           /* external object associated with the task */
	apr_pool_t          *pool;          /* memory pool to allocate task data from */
	apt_task_msg_pool_t *msg_pool;      /* message pool to allocate task messages from */
	apr_thread_mutex_t  *data_guard;    /* mutex to protect task data */
	apr_thread_t        *thread_handle; /* thread handle */
	apt_task_state_t     state;         /* current task state */
	apt_task_vtable_t    vtable;        /* table of virtual methods */
	apt_task_t          *parent_task;   /* parent (master) task */
	apt_obj_list_t      *child_tasks;   /* list of the child (slave) tasks */
	apr_size_t           pending_start; /* number of pending start requests */
	apr_size_t           pending_term;  /* number of pending terminate requests */
	apt_bool_t           auto_ready;    /* if TRUE, task is implicitly ready to process messages */
	const char          *name;          /* name of the task */
};

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data);
static apt_bool_t apt_task_terminate_request(apt_task_t *task);


APT_DECLARE(apt_task_t*) apt_task_create(
								void *obj,
								apt_task_msg_pool_t *msg_pool,
								apr_pool_t *pool)
{
	apt_task_t *task = apr_palloc(pool,sizeof(apt_task_t));
	task->obj = obj;
	task->pool = pool;
	task->msg_pool = msg_pool;

	if(!task->msg_pool) {
		task->msg_pool = apt_task_msg_pool_create_dynamic(0,pool);
	}

	task->state = TASK_STATE_IDLE;
	task->thread_handle = NULL;
	if(apr_thread_mutex_create(&task->data_guard, APR_THREAD_MUTEX_DEFAULT, task->pool) != APR_SUCCESS) {
		return NULL;
	}

	/* reset and copy vtable */
	apt_task_vtable_reset(&task->vtable);
	task->vtable.terminate = apt_task_terminate_request;
	
	task->parent_task = NULL;
	task->child_tasks = apt_list_create(pool);
	task->pending_start = 0;
	task->pending_term = 0;
	task->auto_ready = TRUE;
	task->name = "Task";
	return task;
}

APT_DECLARE(apt_bool_t) apt_task_destroy(apt_task_t *task)
{
	apt_task_t *child_task = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(task->child_tasks);
	/* walk through the list of the child tasks and destroy them */
	while(elem) {
		child_task = apt_list_elem_object_get(elem);
		if(child_task) {
			apt_task_destroy(child_task);
		}
		elem = apt_list_next_elem_get(task->child_tasks,elem);
	}

	if(task->state != TASK_STATE_IDLE) {
		apt_task_wait_till_complete(task);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Destroy %s",task->name);
	if(task->vtable.destroy) {
		task->vtable.destroy(task);
	}
	
	apr_thread_mutex_destroy(task->data_guard);
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_add(apt_task_t *task, apt_task_t *child_task)
{
	child_task->parent_task = task;
	return (apt_list_push_back(task->child_tasks,child_task, child_task->pool) ? TRUE : FALSE);
}

APT_DECLARE(apt_bool_t) apt_task_start(apt_task_t *task)
{
	apt_bool_t status = TRUE;
	apr_thread_mutex_lock(task->data_guard);
	if(task->state == TASK_STATE_IDLE) {
		apr_status_t rv;
		task->state = TASK_STATE_START_REQUESTED;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Start %s",task->name);
		if(task->vtable.start) {
			/* raise virtual start method */
			task->vtable.start(task);
		}
		else {
			/* start new thread by default */
			rv = apr_thread_create(&task->thread_handle,NULL,apt_task_run,task,task->pool);
			if(rv != APR_SUCCESS) {
				task->state = TASK_STATE_IDLE;
				status = FALSE;
			}
		}
	}
	else {
		status = FALSE;
	}
	apr_thread_mutex_unlock(task->data_guard);
	return status;
}

APT_DECLARE(apt_bool_t) apt_task_terminate(apt_task_t *task, apt_bool_t wait_till_complete)
{
	apt_bool_t status = FALSE;
	apr_thread_mutex_lock(task->data_guard);
	if(task->state == TASK_STATE_START_REQUESTED || task->state == TASK_STATE_RUNNING) {
		task->state = TASK_STATE_TERMINATE_REQUESTED;
	}
	apr_thread_mutex_unlock(task->data_guard);

	if(task->state == TASK_STATE_TERMINATE_REQUESTED) {
		/* raise virtual terminate method */
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate %s",task->name);
		if(task->vtable.terminate) {
			status = task->vtable.terminate(task);
		}

		if(wait_till_complete == TRUE && status == TRUE) {
			apt_task_wait_till_complete(task);
		}
	}

	return status;
}

APT_DECLARE(apt_bool_t) apt_task_wait_till_complete(apt_task_t *task)
{
	if(task->thread_handle) {
		apr_status_t s;
		apr_thread_join(&s,task->thread_handle);
		task->thread_handle = NULL;
	}
	return TRUE;
}

APT_DECLARE(void) apt_task_delay(apr_size_t msec)
{
	apr_sleep(1000*msec);
}

APT_DECLARE(apt_task_t*) apt_task_parent_get(apt_task_t *task)
{
	return task->parent_task;
}

APT_DECLARE(apr_pool_t*) apt_task_pool_get(apt_task_t *task)
{
	return task->pool;
}

APT_DECLARE(void*) apt_task_object_get(apt_task_t *task)
{
	return task->obj;
}

APT_DECLARE(apt_task_vtable_t*) apt_task_vtable_get(apt_task_t *task)
{
	return &task->vtable;
}

APT_DECLARE(void) apt_task_name_set(apt_task_t *task, const char *name)
{
	task->name = name;
}

APT_DECLARE(const char*) apt_task_name_get(apt_task_t *task)
{
	return task->name;
}

APT_DECLARE(apt_task_msg_t*) apt_task_msg_get(apt_task_t *task)
{
	if(task->msg_pool) {
		return apt_task_msg_acquire(task->msg_pool);
	}
	return NULL;
}

APT_DECLARE(apt_bool_t) apt_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	if(task->vtable.signal_msg) {
		return task->vtable.signal_msg(task,msg);
	}
	return FALSE;
}

APT_DECLARE(apt_bool_t) apt_task_msg_parent_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_task_t *parent_task = task->parent_task;
	if(parent_task) {
		if(parent_task->vtable.signal_msg) {
			return parent_task->vtable.signal_msg(parent_task,msg);
		}
	}
	return FALSE;
}


APT_DECLARE(apt_bool_t) apt_core_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_bool_t running = TRUE;
	switch(msg->sub_type) {
		case CORE_TASK_MSG_START_COMPLETE: 
		{
			if(!task->pending_start) {
				/* error case, no pending start */
				break;
			}
			task->pending_start--;
			if(!task->pending_start) {
				if(task->vtable.on_start_complete) {
					task->vtable.on_start_complete(task);
				}
				if(task->parent_task) {
					/* signal start-complete message */
					apt_task_msg_signal(task->parent_task,msg);
				}
			}
			break;
		}
		case CORE_TASK_MSG_TERMINATE_REQUEST:
		{
			apt_task_child_terminate(task);
			if(!task->pending_term) {
				running = FALSE;
			}
			break;
		}
		case CORE_TASK_MSG_TERMINATE_COMPLETE:
		{
			if(!task->pending_term) {
				/* error case, no pending terminate */
				break;
			}
			task->pending_term--;
			if(!task->pending_term) {
				if(task->vtable.on_terminate_complete) {
					task->vtable.on_terminate_complete(task);
				}
				if(task->parent_task) {
					/* signal terminate-complete message */
					apt_task_msg_signal(task->parent_task,msg);
				}
				running = FALSE;
			}
			break;
		}
		default: break;
	}
	return running;
}

APT_DECLARE(apt_bool_t) apt_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_bool_t running = TRUE;
	if(msg->type == TASK_MSG_CORE) {
		running = apt_core_task_msg_process(task,msg);
	}
	else {
		if(task->vtable.process_msg) {
			task->vtable.process_msg(task,msg);
		}
	}
	
	apt_task_msg_release(msg);
	return running;
}

static apt_bool_t apt_task_terminate_request(apt_task_t *task)
{
	if(task->msg_pool) {
		apt_task_msg_t *msg = apt_task_msg_acquire(task->msg_pool);
		/* signal terminate-request message */
		msg->type = TASK_MSG_CORE;
		msg->sub_type = CORE_TASK_MSG_TERMINATE_REQUEST;
		return apt_task_msg_signal(task,msg);
	}
	return FALSE;
}

APT_DECLARE(apt_bool_t) apt_task_child_start(apt_task_t *task)
{
	apt_task_t *child_task = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(task->child_tasks);
	task->pending_start = 0;
	/* walk through the list of the child tasks and start them */
	while(elem) {
		child_task = apt_list_elem_object_get(elem);
		if(child_task) {
			if(apt_task_start(child_task) == TRUE) {
				task->pending_start++;
			}
		}
		elem = apt_list_next_elem_get(task->child_tasks,elem);
	}

	if(!task->pending_start) {
		/* no child task to start, just raise start-complete event */
		if(task->vtable.on_start_complete) {
			task->vtable.on_start_complete(task);
		}
		if(task->parent_task) {
			if(task->msg_pool) {
				apt_task_msg_t *msg = apt_task_msg_acquire(task->msg_pool);
				/* signal start-complete message */
				msg->type = TASK_MSG_CORE;
				msg->sub_type = CORE_TASK_MSG_START_COMPLETE;
				apt_task_msg_signal(task->parent_task,msg);
			}
		}
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_child_terminate(apt_task_t *task)
{
	apt_task_t *child_task = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(task->child_tasks);
	task->pending_term = 0;
	/* walk through the list of the child tasks and terminate them */
	while(elem) {
		child_task = apt_list_elem_object_get(elem);
		if(child_task) {
#ifdef ENABLE_SIMULT_TASK_TERMINATION
			if(child_task->thread_handle) {
				apr_thread_detach(child_task->thread_handle);
				child_task->thread_handle = NULL;
			}
			if(apt_task_terminate(child_task,FALSE) == TRUE) {
				task->pending_term++;
			}
#else
			apt_task_terminate(child_task,TRUE);
#endif
		}
		elem = apt_list_next_elem_get(task->child_tasks,elem);
	}

	if(!task->pending_term) {
		/* no child task to terminate, just raise terminate-complete event */
		if(task->vtable.on_terminate_complete) {
			task->vtable.on_terminate_complete(task);
		}
#ifdef ENABLE_SIMULT_TASK_TERMINATION
		if(task->parent_task) {
			if(task->msg_pool) {
				apt_task_msg_t *msg = apt_task_msg_acquire(task->msg_pool);
				/* signal terminate-complete message */
				msg->type = TASK_MSG_CORE;
				msg->sub_type = CORE_TASK_MSG_TERMINATE_COMPLETE;
				apt_task_msg_signal(task->parent_task,msg);
			}
		}
#endif
	}
	return TRUE;
}

APT_DECLARE(void) apt_task_auto_ready_set(apt_task_t *task, apt_bool_t auto_ready)
{
	task->auto_ready = auto_ready;
}

APT_DECLARE(apt_bool_t) apt_task_ready(apt_task_t *task)
{
	if(task->auto_ready == TRUE) {
		return FALSE;
	}

	/* start child tasks (if any) */
	apt_task_child_start(task);
	return TRUE;
}


static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data)
{
	apt_task_t *task = data;
	
	/* raise pre-run event */
	if(task->vtable.on_pre_run) {
		task->vtable.on_pre_run(task);
	}
	apr_thread_mutex_lock(task->data_guard);
	task->state = TASK_STATE_RUNNING;
	apr_thread_mutex_unlock(task->data_guard);

	if(task->auto_ready == TRUE) {
		/* start child tasks (if any) */
		apt_task_child_start(task);
	}

	/* run task */
	if(task->vtable.run) {
		task->vtable.run(task);
	}

	apr_thread_mutex_lock(task->data_guard);
	task->state = TASK_STATE_IDLE;
	apr_thread_mutex_unlock(task->data_guard);
	/* raise post-run event */
	if(task->vtable.on_post_run) {
		task->vtable.on_post_run(task);
	}

	apr_thread_exit(thread_handle,APR_SUCCESS);
	return NULL;
}
