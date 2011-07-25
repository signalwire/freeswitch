/*
 * Copyright 2008-2010 Arsen Chaloyan
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
 * $Id: apt_task.c 1696 2010-05-20 15:44:16Z achaloyan $
 */

#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include "apt_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

/** Internal states of the task */
typedef enum {
	TASK_STATE_IDLE,               /**< no task activity */
	TASK_STATE_START_REQUESTED,    /**< start of the task has been requested, but it's not running yet */
	TASK_STATE_RUNNING,            /**< task is running */
	TASK_STATE_TERMINATE_REQUESTED /**< termination of the task has been requested, but it's still running */
} apt_task_state_e;

struct apt_task_t {
	void                *obj;           /* external object associated with the task */
	apr_pool_t          *pool;          /* memory pool to allocate task data from */
	apt_task_msg_pool_t *msg_pool;      /* message pool to allocate task messages from */
	apr_thread_mutex_t  *data_guard;    /* mutex to protect task data */
	apr_thread_t        *thread_handle; /* thread handle */
	apt_task_state_e     state;         /* current task state */
	apt_task_vtable_t    vtable;        /* table of virtual methods */
	apt_task_t          *parent_task;   /* parent (master) task */
	apt_obj_list_t      *child_tasks;   /* list of the child (slave) tasks */
	apr_size_t           pending_start; /* number of pending start requests */
	apr_size_t           pending_term;  /* number of pending terminate requests */
	apt_bool_t           running;       /* task is running (TRUE if even terminate has already been requested) */
	apt_bool_t           auto_ready;    /* if TRUE, task is implicitly ready to process messages */
	const char          *name;          /* name of the task */
};

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data);
static APR_INLINE void apt_task_vtable_reset(apt_task_vtable_t *vtable);
static apt_bool_t apt_task_terminate_request(apt_task_t *task);
static void apt_task_start_complete_raise(apt_task_t *task);
static void apt_task_terminate_complete_raise(apt_task_t *task);


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

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Destroy Task [%s]",task->name);
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
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Start Task [%s]",task->name);
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
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate Task [%s]",task->name);
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

APT_DECLARE(apt_task_t*) apt_task_parent_get(const apt_task_t *task)
{
	return task->parent_task;
}

APT_DECLARE(apr_pool_t*) apt_task_pool_get(const apt_task_t *task)
{
	return task->pool;
}

APT_DECLARE(void*) apt_task_object_get(const apt_task_t *task)
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

APT_DECLARE(const char*) apt_task_name_get(const apt_task_t *task)
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
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Message to [%s] [%d;%d]",
		task->name, msg->type, msg->sub_type);
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


static apt_bool_t apt_core_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	switch(msg->sub_type) {
		case CORE_TASK_MSG_START_COMPLETE:
		{
			apt_task_start_request_remove(task);
			break;
		}
		case CORE_TASK_MSG_TERMINATE_REQUEST:
		{
			apt_task_child_terminate(task);
			if(!task->pending_term) {
				task->running = FALSE;
			}
			break;
		}
		case CORE_TASK_MSG_TERMINATE_COMPLETE:
		{
			apt_task_terminate_request_remove(task);
			break;
		}
		default: break;
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_bool_t status = FALSE;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Message [%s] [%d;%d]",
		task->name, msg->type, msg->sub_type);
	if(msg->type == TASK_MSG_CORE) {
		status = apt_core_task_msg_process(task,msg);
	}
	else {
		if(task->vtable.process_msg) {
			status = task->vtable.process_msg(task,msg);
		}
	}
	
	apt_task_msg_release(msg);
	return status;
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
	if(task->vtable.on_start_request) {
		task->vtable.on_start_request(task);
	}
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
		apt_task_start_complete_raise(task);
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_child_terminate(apt_task_t *task)
{
	apt_task_t *child_task = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(task->child_tasks);
	task->pending_term = 0;
	if(task->vtable.on_terminate_request) {
		task->vtable.on_terminate_request(task);
	}
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
		apt_task_terminate_complete_raise(task);
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

APT_DECLARE(apt_bool_t*) apt_task_running_flag_get(apt_task_t *task)
{
	return &task->running;
}

APT_DECLARE(apt_bool_t) apt_task_start_request_add(apt_task_t *task)
{
	task->pending_start++;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_start_request_remove(apt_task_t *task)
{
	if(!task->pending_start) {
		/* error case, no pending start */
		return FALSE;
	}
	task->pending_start--;
	if(!task->pending_start) {
		apt_task_start_complete_raise(task);
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_terminate_request_add(apt_task_t *task)
{
	task->pending_term++;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_terminate_request_remove(apt_task_t *task)
{
	if(!task->pending_term) {
		/* error case, no pending terminate */
		return FALSE;
	}
	task->pending_term--;
	if(!task->pending_term) {
		apt_task_terminate_complete_raise(task);
		task->running = FALSE;
	}
	return TRUE;
}

static void apt_task_start_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Started [%s]",task->name);
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

static void apt_task_terminate_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Terminated [%s]",task->name);
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

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data)
{
	apt_task_t *task = data;
	
	/* raise pre-run event */
	if(task->vtable.on_pre_run) {
		task->vtable.on_pre_run(task);
	}
	apr_thread_mutex_lock(task->data_guard);
	task->state = TASK_STATE_RUNNING;
	task->running = TRUE;
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
	task->running = FALSE;
	apr_thread_mutex_unlock(task->data_guard);
	/* raise post-run event */
	if(task->vtable.on_post_run) {
		task->vtable.on_post_run(task);
	}

	apr_thread_exit(thread_handle,APR_SUCCESS);
	return NULL;
}

static APR_INLINE void apt_task_vtable_reset(apt_task_vtable_t *vtable)
{
	vtable->destroy = NULL;
	vtable->start = NULL;
	vtable->terminate = NULL;
	vtable->run = NULL;
	vtable->signal_msg = NULL;
	vtable->process_msg = NULL;
	vtable->on_pre_run = NULL;
	vtable->on_post_run = NULL;
	vtable->on_start_request = NULL;
	vtable->on_start_complete = NULL;
	vtable->on_terminate_request = NULL;
	vtable->on_terminate_complete = NULL;
}
