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

#include "mpf_engine.h"
#include "mpf_user.h"
#include "mpf_context.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mpf_timer.h"
#include "mpf_codec_descriptor.h"
#include "mpf_codec_manager.h"
#include "apt_obj_list.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"

#define MPF_TASK_NAME "Media Processing Engine"

struct mpf_engine_t {
	apr_pool_t                *pool;
	apt_task_t                *task;
	apt_task_msg_type_e        task_msg_type;
	apr_thread_mutex_t        *request_queue_guard;
	apt_cyclic_queue_t        *request_queue;
	apt_obj_list_t            *contexts;
	mpf_timer_t               *timer;
	const mpf_codec_manager_t *codec_manager;
};

static void mpf_engine_main(mpf_timer_t *timer, void *data);
static apt_bool_t mpf_engine_destroy(apt_task_t *task);
static apt_bool_t mpf_engine_start(apt_task_t *task);
static apt_bool_t mpf_engine_terminate(apt_task_t *task);
static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t mpf_engine_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t mpf_engine_contexts_destroy(mpf_engine_t *engine);

mpf_codec_t* mpf_codec_lpcm_create(apr_pool_t *pool);
mpf_codec_t* mpf_codec_l16_create(apr_pool_t *pool);
mpf_codec_t* mpf_codec_g711u_create(apr_pool_t *pool);
mpf_codec_t* mpf_codec_g711a_create(apr_pool_t *pool);

MPF_DECLARE(mpf_engine_t*) mpf_engine_create(apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	mpf_engine_t *engine = apr_palloc(pool,sizeof(mpf_engine_t));
	engine->pool = pool;
	engine->request_queue = NULL;
	engine->contexts = NULL;
	engine->codec_manager = NULL;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),pool);

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "MPF_TASK_NAME);
	engine->task = apt_task_create(engine,msg_pool,pool);
	if(!engine->task) {
		return NULL;
	}

	apt_task_name_set(engine->task,MPF_TASK_NAME);

	vtable = apt_task_vtable_get(engine->task);
	if(vtable) {
		vtable->destroy = mpf_engine_destroy;
		vtable->start = mpf_engine_start;
		vtable->terminate = mpf_engine_terminate;
		vtable->signal_msg = mpf_engine_msg_signal;
		vtable->process_msg = mpf_engine_msg_process;
	}

	engine->task_msg_type = TASK_MSG_USER;

	engine->request_queue = apt_cyclic_queue_create(CYCLIC_QUEUE_DEFAULT_SIZE);
	apr_thread_mutex_create(&engine->request_queue_guard,APR_THREAD_MUTEX_UNNESTED,engine->pool);

	engine->contexts = apt_list_create(engine->pool);

	return engine;
}

MPF_DECLARE(apt_task_t*) mpf_task_get(mpf_engine_t *engine)
{
	return engine->task;
}

MPF_DECLARE(void) mpf_engine_task_msg_type_set(mpf_engine_t *engine, apt_task_msg_type_e type)
{
	engine->task_msg_type = type;
}

static apt_bool_t mpf_engine_destroy(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	apt_list_destroy(engine->contexts);

	apt_cyclic_queue_destroy(engine->request_queue);
	apr_thread_mutex_destroy(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_start(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	engine->timer = mpf_timer_start(CODEC_FRAME_TIME_BASE,mpf_engine_main,engine,engine->pool);
	apt_task_child_start(task);
	return TRUE;
}

static apt_bool_t mpf_engine_terminate(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	mpf_timer_stop(engine->timer);
	mpf_engine_contexts_destroy(engine);
	apt_task_child_terminate(task);
	return TRUE;
}

static apt_bool_t mpf_engine_contexts_destroy(mpf_engine_t *engine)
{
	mpf_context_t *context;
	context = apt_list_pop_front(engine->contexts);
	while(context) {
		mpf_context_destroy(context);
		
		context = apt_list_pop_front(engine->contexts);
	}
	return TRUE;
}

static apt_bool_t mpf_engine_event_raise(mpf_termination_t *termination, int event_id, void *descriptor)
{
	apt_task_msg_t *task_msg;
	mpf_message_t *event_msg;
	mpf_engine_t *engine;
	engine = termination->event_handler_obj;
	if(!engine) {
		return FALSE;
	}

	task_msg = apt_task_msg_get(engine->task);
	task_msg->type = engine->task_msg_type;
	event_msg = (mpf_message_t*) task_msg->data;
	event_msg->command_id = event_id;
	event_msg->message_type = MPF_MESSAGE_TYPE_EVENT;
	event_msg->status_code = MPF_STATUS_CODE_SUCCESS;
	event_msg->context = NULL;
	event_msg->termination = termination;
	event_msg->descriptor = descriptor;
	
	return apt_task_msg_parent_signal(engine->task,task_msg);
}

static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	mpf_engine_t *engine = apt_task_object_get(task);
	
	apr_thread_mutex_lock(engine->request_queue_guard);
	if(apt_cyclic_queue_push(engine->request_queue,msg) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"MPF Request Queue is Full");
	}
	apr_thread_mutex_unlock(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	mpf_engine_t *engine = apt_task_object_get(task);
	apt_task_msg_t *response_msg;
	mpf_message_t *response;
	mpf_context_t *context;
	mpf_termination_t *termination;
	const mpf_message_t *request = (const mpf_message_t*) msg->data;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Message");
	if(request->message_type != MPF_MESSAGE_TYPE_REQUEST) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid MPF Message Type [%d]",request->message_type);
		return FALSE;
	}

	response_msg = apt_task_msg_get(engine->task);
	response_msg->type = engine->task_msg_type;
	response = (mpf_message_t*) response_msg->data;
	*response = *request;
	response->message_type = MPF_MESSAGE_TYPE_RESPONSE;
	response->status_code = MPF_STATUS_CODE_SUCCESS;
	context = request->context;
	termination = request->termination;
	switch(request->command_id) {
		case MPF_COMMAND_ADD:
		{
			termination->event_handler_obj = engine;
			termination->event_handler = mpf_engine_event_raise;
			termination->codec_manager = engine->codec_manager;
			if(request->descriptor) {
				mpf_termination_modify(termination,request->descriptor);
			}
			mpf_termination_validate(termination);
			if(mpf_context_termination_add(context,termination) == FALSE) {
				response->status_code = MPF_STATUS_CODE_FAILURE;
				break;
			}
			mpf_context_topology_apply(context,termination);
			if(context->termination_count == 1) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Add Context");
				context->elem = apt_list_push_back(engine->contexts,context,context->pool);
			}
			break;
		}
		case MPF_COMMAND_SUBTRACT:
		{
			mpf_context_topology_destroy(context,termination);
			if(mpf_context_termination_subtract(context,termination) == FALSE) {
				response->status_code = MPF_STATUS_CODE_FAILURE;
				break;
			}
			if(context->termination_count == 0) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Remove Context");
				apt_list_elem_remove(engine->contexts,context->elem);
				context->elem = NULL;
			}
			break;
		}
		case MPF_COMMAND_MODIFY:
		{
			if(request->descriptor) {
				mpf_context_topology_destroy(context,termination);
				mpf_termination_modify(termination,request->descriptor);
				mpf_termination_validate(termination);
				mpf_context_topology_apply(context,termination);
			}
			break;
		}
		default:
		{
			response->status_code = MPF_STATUS_CODE_FAILURE;
		}
	}

	return apt_task_msg_parent_signal(engine->task,response_msg);
}

static void mpf_engine_main(mpf_timer_t *timer, void *data)
{
	mpf_engine_t *engine = data;
	apt_task_msg_t *msg;
	apt_list_elem_t *elem;
	mpf_context_t *context;

	/* process request queue */
	apr_thread_mutex_lock(engine->request_queue_guard);
	msg = apt_cyclic_queue_pop(engine->request_queue);
	while(msg) {
		apr_thread_mutex_unlock(engine->request_queue_guard);
		apt_task_msg_process(engine->task,msg);
		apr_thread_mutex_lock(engine->request_queue_guard);
		msg = apt_cyclic_queue_pop(engine->request_queue);
	}
	apr_thread_mutex_unlock(engine->request_queue_guard);

	/* process contexts */
	elem = apt_list_first_elem_get(engine->contexts);
	while(elem) {
		context = apt_list_elem_object_get(elem);
		if(context) {
			mpf_context_process(context);
		}
		elem = apt_list_next_elem_get(engine->contexts,elem);
	}
}

MPF_DECLARE(mpf_codec_manager_t*) mpf_engine_codec_manager_create(apr_pool_t *pool)
{
	mpf_codec_manager_t *codec_manager = mpf_codec_manager_create(3,pool);
	if(codec_manager) {
		mpf_codec_t *codec;
		codec = mpf_codec_lpcm_create(pool);
		mpf_codec_manager_codec_register(codec_manager,codec);

		codec = mpf_codec_g711u_create(pool);
		mpf_codec_manager_codec_register(codec_manager,codec);

		codec = mpf_codec_g711a_create(pool);
		mpf_codec_manager_codec_register(codec_manager,codec);

		codec = mpf_codec_l16_create(pool);
		mpf_codec_manager_codec_register(codec_manager,codec);
	}
	return codec_manager;
}

MPF_DECLARE(apt_bool_t) mpf_engine_codec_manager_register(mpf_engine_t *engine, const mpf_codec_manager_t *codec_manager)
{
	engine->codec_manager = codec_manager;
	return TRUE;
}
