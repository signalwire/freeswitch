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
 * $Id: apt_task.c 2219 2014-11-11 02:35:14Z achaloyan@gmail.com $
 */

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h> 
#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include <apr_portable.h>
#include "apt_task.h"
#include "apt_log.h"

/** Internal states of the task */
typedef enum {
	TASK_STATE_IDLE,               /**< no task activity */
	TASK_STATE_START_REQUESTED,    /**< start of the task has been requested, but it's not running yet */
	TASK_STATE_RUNNING,            /**< task is running */
	TASK_STATE_TERMINATE_REQUESTED /**< termination of the task has been requested, but it's still running */
} apt_task_state_e;

struct apt_task_t {
	APR_RING_ENTRY(apt_task_t) link;                 /* entry to parent task ring */
	APR_RING_HEAD(apt_task_head_t, apt_task_t) head; /* head of child tasks ring */

	const char          *name;          /* name of the task */
	void                *obj;           /* external object associated with the task */
	apr_pool_t          *pool;          /* memory pool to allocate task data from */
	apt_task_msg_pool_t *msg_pool;      /* message pool to allocate task messages from */
	apr_thread_mutex_t  *data_guard;    /* mutex to protect task data */
	apr_thread_t        *thread_handle; /* thread handle */
	apt_task_state_e     state;         /* current task state */
	apt_task_vtable_t    vtable;        /* table of virtual methods */
	apt_task_t          *parent_task;   /* parent (master) task */
	apr_size_t           pending_start; /* number of pending start requests */
	apr_size_t           pending_term;  /* number of pending terminate requests */
	apr_size_t           pending_off;   /* number of pending taking-offline requests */
	apr_size_t           pending_on;    /* number of pending bringing-online requests */
	apt_bool_t           running;       /* task is running (TRUE if even terminate has already been requested) */
	apt_bool_t           auto_ready;    /* if TRUE, task is implicitly ready to process messages */
};

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data);
static APR_INLINE void apt_task_vtable_reset(apt_task_vtable_t *vtable);

static apt_bool_t apt_task_core_msg_signal(apt_task_t *task, apt_task_msg_pool_t *msg_pool, apt_core_task_msg_type_e type);

static apt_bool_t apt_task_terminate_request(apt_task_t *task);

static apt_bool_t apt_task_start_process_internal(apt_task_t *task);
static apt_bool_t apt_task_terminate_process_internal(apt_task_t *task);
static apt_bool_t apt_task_offline_request_process(apt_task_t *task);
static apt_bool_t apt_task_online_request_process(apt_task_t *task);

static apt_bool_t apt_task_offline_request_complete(apt_task_t *task);
static apt_bool_t apt_task_online_request_complete(apt_task_t *task);

static void apt_task_start_complete_raise(apt_task_t *task);
static void apt_task_terminate_complete_raise(apt_task_t *task);
static void apt_task_offline_complete_raise(apt_task_t *task);
static void apt_task_online_complete_raise(apt_task_t *task);


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

	/* reset vtable */
	apt_task_vtable_reset(&task->vtable);
	task->vtable.terminate = apt_task_terminate_request;
	task->vtable.process_start = apt_task_start_process_internal;
	task->vtable.process_terminate = apt_task_terminate_process_internal;
	
	APR_RING_ELEM_INIT(task, link);
	APR_RING_INIT(&task->head, apt_task_t, link);

	task->parent_task = NULL;
	task->pending_start = 0;
	task->pending_term = 0;
	task->pending_off = 0;
	task->pending_on = 0;
	task->auto_ready = TRUE;
	task->name = "Task";
	return task;
}

APT_DECLARE(apt_bool_t) apt_task_destroy(apt_task_t *task)
{
	apt_task_t *child_task;
	APR_RING_FOREACH(child_task, &task->head, apt_task_t, link) {
		apt_task_destroy(child_task);
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
	if(!child_task)
		return FALSE;

	child_task->parent_task = task;
	APR_RING_INSERT_TAIL(&task->head,child_task,apt_task_t,link);
	return TRUE;
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
			/* invoke virtual start method */
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

APT_DECLARE(apt_bool_t) apt_task_offline(apt_task_t *task)
{
	return apt_task_core_msg_signal(task,task->msg_pool,CORE_TASK_MSG_TAKEOFFLINE_REQUEST);
}

APT_DECLARE(apt_bool_t) apt_task_online(apt_task_t *task)
{
	return apt_task_core_msg_signal(task,task->msg_pool,CORE_TASK_MSG_BRINGONLINE_REQUEST);
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
		/* invoke virtual terminate method */
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
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Message to [%s] ["APT_PTR_FMT";%d;%d]",
		task->name, msg, msg->type, msg->sub_type);
	if(task->vtable.signal_msg) {
		if(task->vtable.signal_msg(task,msg) == TRUE) {
			return TRUE;
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Task Message [%s] [0x%x;%d;%d]",
		task->name, msg, msg->type, msg->sub_type);
	apt_task_msg_release(msg);
	return FALSE;
}

APT_DECLARE(apt_bool_t) apt_task_msg_parent_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_task_t *parent_task = task->parent_task;
	if(parent_task) {
		return apt_task_msg_signal(parent_task,msg);
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Null Parent Task [%s]",task->name);
	apt_task_msg_release(msg);
	return FALSE;
}

static apt_bool_t apt_task_core_msg_signal(apt_task_t *task, apt_task_msg_pool_t *msg_pool, apt_core_task_msg_type_e type)
{
	if(task && msg_pool) {
		apt_task_msg_t *msg = apt_task_msg_acquire(msg_pool);
		/* signal core task message */
		msg->type = TASK_MSG_CORE;
		msg->sub_type = type;
		return apt_task_msg_signal(task,msg);
	}
	return FALSE;
}

static apt_bool_t apt_core_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	switch(msg->sub_type) {
		case CORE_TASK_MSG_START_COMPLETE:
			apt_task_start_request_remove(task);
			break;
		case CORE_TASK_MSG_TERMINATE_REQUEST:
			if(task->vtable.process_terminate) {
				task->vtable.process_terminate(task);
			}
			break;
		case CORE_TASK_MSG_TERMINATE_COMPLETE:
			apt_task_terminate_request_remove(task);
			break;
		case CORE_TASK_MSG_TAKEOFFLINE_REQUEST:
			apt_task_offline_request_process(task);
			break;
		case CORE_TASK_MSG_TAKEOFFLINE_COMPLETE:
			apt_task_offline_request_complete(task);
			break;
		case CORE_TASK_MSG_BRINGONLINE_REQUEST:
			apt_task_online_request_process(task);
			break;
		case CORE_TASK_MSG_BRINGONLINE_COMPLETE:
			apt_task_online_request_complete(task);
			break;
		default: break;
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_bool_t status = FALSE;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Message [%s] ["APT_PTR_FMT";%d;%d]",
		task->name, msg, msg->type, msg->sub_type);
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
	return apt_task_core_msg_signal(task,task->msg_pool,CORE_TASK_MSG_TERMINATE_REQUEST);
}

APT_DECLARE(apt_bool_t) apt_task_start_request_process(apt_task_t *task)
{
	return apt_task_start_process_internal(task);
}

static apt_bool_t apt_task_start_process_internal(apt_task_t *task)
{
	apt_task_t *child_task;
	APR_RING_FOREACH(child_task, &task->head, apt_task_t, link) {
		if(apt_task_start(child_task) == TRUE) {
			task->pending_start++;
		}
	}

	if(!task->pending_start) {
		/* no child task to start, just raise start-complete event */
		apt_task_start_complete_raise(task);
	}
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_terminate_request_process(apt_task_t *task)
{
	return apt_task_terminate_process_internal(task);
}

static apt_bool_t apt_task_terminate_process_internal(apt_task_t *task)
{
	apt_task_t *child_task;
	APR_RING_FOREACH(child_task, &task->head, apt_task_t, link) {
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

	if(!task->pending_term) {
		/* no child task to terminate, just raise terminate-complete event */
		apt_task_terminate_complete_raise(task);
		task->running = FALSE;
	}
	return TRUE;
}

static apt_bool_t apt_task_offline_request_process(apt_task_t *task)
{
	apt_task_t *child_task;
	APR_RING_FOREACH(child_task, &task->head, apt_task_t, link) {
		if(apt_task_offline(child_task) == TRUE) {
			task->pending_off++;
		}
	}

	if(!task->pending_off) {
		/* no child task, just raise offline-complete event */
		apt_task_offline_complete_raise(task);
	}
	return TRUE;
}

static apt_bool_t apt_task_online_request_process(apt_task_t *task)
{
	apt_task_t *child_task;
	APR_RING_FOREACH(child_task, &task->head, apt_task_t, link) {
		if(apt_task_online(child_task) == TRUE) {
			task->pending_on++;
		}
	}

	if(!task->pending_on) {
		/* no child task, just raise online-complete event */
		apt_task_online_complete_raise(task);
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
	if(task->vtable.process_start) {
		task->vtable.process_start(task);
	}
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

static apt_bool_t apt_task_offline_request_complete(apt_task_t *task)
{
	if(!task->pending_off) {
		/* error case, no pending request */
		return FALSE;
	}
	task->pending_off--;
	if(!task->pending_off) {
		apt_task_offline_complete_raise(task);
	}
	return TRUE;
}

static apt_bool_t apt_task_online_request_complete(apt_task_t *task)
{
	if(!task->pending_on) {
		/* error case, no pending request */
		return FALSE;
	}
	task->pending_on--;
	if(!task->pending_on) {
		apt_task_online_complete_raise(task);
	}
	return TRUE;
}

static void apt_task_start_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Started [%s]",task->name);
	if(task->vtable.on_start_complete) {
		task->vtable.on_start_complete(task);
	}
	apt_task_core_msg_signal(task->parent_task,task->msg_pool,CORE_TASK_MSG_START_COMPLETE);
}

static void apt_task_terminate_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Terminated [%s]",task->name);
	if(task->vtable.on_terminate_complete) {
		task->vtable.on_terminate_complete(task);
	}
#ifdef ENABLE_SIMULT_TASK_TERMINATION
	apt_task_core_msg_signal(task->parent_task,task->msg_pool,CORE_TASK_MSG_TERMINATE_COMPLETE);
#endif
}

static void apt_task_offline_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Taken Offline [%s]",task->name);
	if(task->vtable.on_offline_complete) {
		task->vtable.on_offline_complete(task);
	}
	apt_task_core_msg_signal(task->parent_task,task->msg_pool,CORE_TASK_MSG_TAKEOFFLINE_COMPLETE);
}

static void apt_task_online_complete_raise(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Task Brought Online [%s]",task->name);
	if(task->vtable.on_online_complete) {
		task->vtable.on_online_complete(task);
	}
	apt_task_core_msg_signal(task->parent_task,task->msg_pool,CORE_TASK_MSG_BRINGONLINE_COMPLETE);
}

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data)
{
	apt_task_t *task = data;
	
#if APR_HAS_SETTHREADNAME
	apr_thread_name_set(task->name);
#endif
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
		if(task->vtable.process_start) {
			task->vtable.process_start(task);
		}
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
	vtable->process_start = NULL;
	vtable->process_terminate = NULL;
	vtable->on_pre_run = NULL;
	vtable->on_post_run = NULL;
	vtable->on_start_complete = NULL;
	vtable->on_terminate_complete = NULL;
	vtable->on_offline_complete = NULL;
	vtable->on_online_complete = NULL;
}
