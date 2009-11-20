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

#ifndef __APT_TASK_H__
#define __APT_TASK_H__

/**
 * @file apt_task.h
 * @brief Thread Execution Abstraction
 */ 

#include "apt.h"
#include "apt_task_msg.h"

APT_BEGIN_EXTERN_C

/** Opaque task declaration */
typedef struct apt_task_t apt_task_t;
/** Opaque task virtual table declaration */
typedef struct apt_task_vtable_t apt_task_vtable_t;
/** Opaque task method declaration */
typedef apt_bool_t (*apt_task_method_f)(apt_task_t *task);
/** Opaque task event declaration */
typedef void (*apt_task_event_f)(apt_task_t *task);


/**
 * Create task.
 * @param obj the external object to associate with the task
 * @param msg_pool the pool of task messages
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_task_t*) apt_task_create(
								void *obj,
								apt_task_msg_pool_t *msg_pool,
								apr_pool_t *pool);

/**
 * Destroy task.
 * @param task the task to destroy
 */
APT_DECLARE(apt_bool_t) apt_task_destroy(apt_task_t *task);

/**
 * Add child task.
 * @param task the task to add child task to
 * @param child_task the child task to add
 */
APT_DECLARE(apt_bool_t) apt_task_add(apt_task_t *task, apt_task_t *child_task);

/**
 * Start task.
 * @param task the task to start
 */
APT_DECLARE(apt_bool_t) apt_task_start(apt_task_t *task);

/**
 * Terminate task.
 * @param task the task to terminate
 * @param wait_till_complete whether to wait for task to complete or
 *                           process termination asynchronously
 */
APT_DECLARE(apt_bool_t) apt_task_terminate(apt_task_t *task, apt_bool_t wait_till_complete);

/**
 * Start child tasks.
 * @param task the parent task
 */
APT_DECLARE(apt_bool_t) apt_task_child_start(apt_task_t *task);

/**
 * Terminate child tasks.
 * @param task the parent task
 */
APT_DECLARE(apt_bool_t) apt_task_child_terminate(apt_task_t *task);

/**
 * Wait for task till complete.
 * @param task the task to wait for
 */
APT_DECLARE(apt_bool_t) apt_task_wait_till_complete(apt_task_t *task);

/**
 * Get (acquire) task message.
 * @param task the task to get task message from
 */
APT_DECLARE(apt_task_msg_t*) apt_task_msg_get(apt_task_t *task);

/**
 * Signal (post) message to the task.
 * @param task the task to signal message to
 * @param msg the message to signal
 */
APT_DECLARE(apt_bool_t) apt_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);

/**
 * Signal (post) message to the parent of the specified task.
 * @param task the task to signal message to
 * @param msg the message to signal
 */
APT_DECLARE(apt_bool_t) apt_task_msg_parent_signal(apt_task_t *task, apt_task_msg_t *msg);

/**
 * Process message signaled to the task.
 * @param task the task to process message
 * @param msg the message to process
 */
APT_DECLARE(apt_bool_t) apt_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

/**
 * Get parent (master) task.
 * @param task the task to get parent from
 */
APT_DECLARE(apt_task_t*) apt_task_parent_get(apt_task_t *task);

/**
 * Get memory pool associated with task.
 * @param task the task to get pool from
 */
APT_DECLARE(apr_pool_t*) apt_task_pool_get(apt_task_t *task);

/**
 * Get external object associated with the task.
 * @param task the task to get object from
 */
APT_DECLARE(void*) apt_task_object_get(apt_task_t *task);

/**
 * Get task vtable.
 * @param task the task to get vtable from
 */
APT_DECLARE(apt_task_vtable_t*) apt_task_vtable_get(apt_task_t *task);

/**
 * Give a name to the task.
 * @param task the task to give name for
 * @param name the name to set
 */
APT_DECLARE(void) apt_task_name_set(apt_task_t *task, const char *name);

/**
 * Get task name.
 * @param task the task to get name from
 */
APT_DECLARE(const char*) apt_task_name_get(apt_task_t *task);

/**
 * Enable/disable auto ready mode.
 * @param task the task to set mode for
 * @param auto_ready the enabled/disabled auto ready mode
 */
APT_DECLARE(void) apt_task_auto_ready_set(apt_task_t *task, apt_bool_t auto_ready);

/**
 * Explicitly indicate task is ready to process messages.
 * @param task the task
 */
APT_DECLARE(apt_bool_t) apt_task_ready(apt_task_t *task);

/**
 * Hold task execution.
 * @param msec the time to hold
 */
APT_DECLARE(void) apt_task_delay(apr_size_t msec);


/** Table of task virtual methods */
struct apt_task_vtable_t {
	/** Virtual destroy method */
	apt_task_method_f destroy;
	/** Virtual start method*/
	apt_task_method_f start;
	/** Virtual terminate method */
	apt_task_method_f terminate;
	/** Virtual run method*/
	apt_task_method_f run;

	/** Virtual signal method  */
	apt_bool_t (*signal_msg)(apt_task_t *task, apt_task_msg_t *msg);
	/** Virtual process method */
	apt_bool_t (*process_msg)(apt_task_t *task, apt_task_msg_t *msg);

	/** Virtual pre-run event handler */
	apt_task_event_f on_pre_run;
	/** Virtual post-run event handler */
	apt_task_event_f on_post_run;
	/** Virtual start-complete event handler */
	apt_task_event_f on_start_complete;
	/** Virtual terminate-complete event handler */
	apt_task_event_f on_terminate_complete;
};

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
	vtable->on_start_complete = NULL;
	vtable->on_terminate_complete = NULL;
}

APT_END_EXTERN_C

#endif /*__APT_TASK_H__*/
