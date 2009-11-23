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
#include "mpf_context.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mpf_scheduler.h"
#include "mpf_codec_descriptor.h"
#include "mpf_codec_manager.h"
#include "mpf_timer_manager.h"
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
	mpf_context_factory_t     *context_factory;
	mpf_scheduler_t           *scheduler;
	mpf_timer_manager_t       *timer_manager;
	const mpf_codec_manager_t *codec_manager;
};

static void mpf_engine_main(mpf_scheduler_t *scheduler, void *data);
static apt_bool_t mpf_engine_destroy(apt_task_t *task);
static apt_bool_t mpf_engine_start(apt_task_t *task);
static apt_bool_t mpf_engine_terminate(apt_task_t *task);
static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t mpf_engine_msg_process(apt_task_t *task, apt_task_msg_t *msg);


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
	engine->context_factory = NULL;
	engine->codec_manager = NULL;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_container_t),pool);

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

	engine->context_factory = mpf_context_factory_create(engine->pool);
	engine->request_queue = apt_cyclic_queue_create(CYCLIC_QUEUE_DEFAULT_SIZE);
	apr_thread_mutex_create(&engine->request_queue_guard,APR_THREAD_MUTEX_UNNESTED,engine->pool);

	engine->scheduler = mpf_scheduler_create(engine->pool);
	mpf_scheduler_media_clock_set(engine->scheduler,CODEC_FRAME_TIME_BASE,mpf_engine_main,engine);

	engine->timer_manager = mpf_timer_manager_create(engine->scheduler,engine->pool);
	return engine;
}

MPF_DECLARE(mpf_context_t*) mpf_engine_context_create(
								mpf_engine_t *engine, 
								void *obj, 
								apr_size_t max_termination_count, 
								apr_pool_t *pool)
{
	return mpf_context_create(engine->context_factory,obj,max_termination_count,pool);
}

MPF_DECLARE(apt_bool_t) mpf_engine_context_destroy(mpf_context_t *context)
{
	return mpf_context_destroy(context);
}

MPF_DECLARE(void*) mpf_engine_context_object_get(mpf_context_t *context)
{
	return mpf_context_object_get(context);
}

MPF_DECLARE(apt_task_t*) mpf_task_get(mpf_engine_t *engine)
{
	return engine->task;
}

MPF_DECLARE(void) mpf_engine_task_msg_type_set(mpf_engine_t *engine, apt_task_msg_type_e type)
{
	engine->task_msg_type = type;
}

static mpf_message_t* mpf_engine_message_get(mpf_engine_t *engine, mpf_task_msg_t **task_msg)
{
	mpf_message_container_t *container;
	mpf_message_t *mpf_message;
	if(*task_msg) {
		container = (mpf_message_container_t*) (*task_msg)->data;
		if(container->count >= MAX_MPF_MESSAGE_COUNT) {
			/* container has been already filled,
			implicitly send the requests and get new task message */
			mpf_engine_message_send(engine,task_msg);
			return mpf_engine_message_get(engine,task_msg);
		}
	}
	else {
		*task_msg = apt_task_msg_get(engine->task);
		container = (mpf_message_container_t*) (*task_msg)->data;
		container->count = 0;
	}

	mpf_message = &container->messages[container->count];
	container->count++;
	return mpf_message;
}


MPF_DECLARE(apt_bool_t) mpf_engine_termination_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_termination_t *termination,
							void *descriptor,
							mpf_task_msg_t **task_msg)
{
	mpf_message_t *mpf_message = mpf_engine_message_get(engine,task_msg);
	if(!mpf_message) {
		return FALSE;
	}
	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = termination;
	mpf_message->assoc_termination = NULL;
	mpf_message->descriptor = descriptor;
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_engine_assoc_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_termination_t *termination,
							mpf_termination_t *assoc_termination,
							mpf_task_msg_t **task_msg)
{
	mpf_message_t *mpf_message = mpf_engine_message_get(engine,task_msg);
	if(!mpf_message) {
		return FALSE;
	}
	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = termination;
	mpf_message->assoc_termination = assoc_termination;
	mpf_message->descriptor = NULL;
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_engine_topology_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_task_msg_t **task_msg)
{
	mpf_message_t *mpf_message = mpf_engine_message_get(engine,task_msg);
	if(!mpf_message) {
		return FALSE;
	}
	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = NULL;
	mpf_message->assoc_termination = NULL;
	mpf_message->descriptor = NULL;
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_engine_message_send(mpf_engine_t *engine, mpf_task_msg_t **task_msg)
{
	apt_bool_t status = FALSE;
	if(*task_msg) {
		status = apt_task_msg_signal(engine->task,*task_msg);
		*task_msg = NULL;
	}
	return status;
}

static apt_bool_t mpf_engine_destroy(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	mpf_timer_manager_destroy(engine->timer_manager);
	mpf_scheduler_destroy(engine->scheduler);
	mpf_context_factory_destroy(engine->context_factory);
	apt_cyclic_queue_destroy(engine->request_queue);
	apr_thread_mutex_destroy(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_start(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	mpf_scheduler_start(engine->scheduler);
	apt_task_child_start(task);
	return TRUE;
}

static apt_bool_t mpf_engine_terminate(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	mpf_scheduler_stop(engine->scheduler);
	apt_task_child_terminate(task);
	return TRUE;
}

static apt_bool_t mpf_engine_event_raise(mpf_termination_t *termination, int event_id, void *descriptor)
{
	apt_task_msg_t *task_msg;
	mpf_message_container_t *event_msg;
	mpf_message_t *mpf_message;
	mpf_engine_t *engine;
	engine = termination->event_handler_obj;
	if(!engine) {
		return FALSE;
	}

	task_msg = apt_task_msg_get(engine->task);
	task_msg->type = engine->task_msg_type;
	event_msg = (mpf_message_container_t*) task_msg->data;
	mpf_message = event_msg->messages;
	event_msg->count = 1;

	mpf_message->command_id = event_id;
	mpf_message->message_type = MPF_MESSAGE_TYPE_EVENT;
	mpf_message->status_code = MPF_STATUS_CODE_SUCCESS;
	mpf_message->context = NULL;
	mpf_message->termination = termination;
	mpf_message->descriptor = descriptor;
	
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
	apr_size_t i;
	mpf_engine_t *engine = apt_task_object_get(task);
	apt_task_msg_t *response_msg;
	mpf_message_container_t *response;
	mpf_message_t *mpf_response;
	mpf_context_t *context;
	mpf_termination_t *termination;
	const mpf_message_t *mpf_request;
	const mpf_message_container_t *request = (const mpf_message_container_t*) msg->data;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Message");

	response_msg = apt_task_msg_get(engine->task);
	response_msg->type = engine->task_msg_type;
	response = (mpf_message_container_t*) response_msg->data;
	*response = *request;
	for(i=0; i<request->count; i++) {
		mpf_request = &request->messages[i];
		mpf_response = &response->messages[i];

		if(mpf_request->message_type != MPF_MESSAGE_TYPE_REQUEST) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid MPF Message Type [%d]",mpf_request->message_type);
			continue;
		}

		mpf_response->message_type = MPF_MESSAGE_TYPE_RESPONSE;
		mpf_response->status_code = MPF_STATUS_CODE_SUCCESS;
		context = mpf_request->context;
		termination = mpf_request->termination;
		switch(mpf_request->command_id) {
			case MPF_ADD_TERMINATION:
			{
				termination->event_handler_obj = engine;
				termination->event_handler = mpf_engine_event_raise;
				termination->codec_manager = engine->codec_manager;
				termination->timer_manager = engine->timer_manager;

				mpf_termination_add(termination,mpf_request->descriptor);
				if(mpf_context_termination_add(context,termination) == FALSE) {
					mpf_termination_subtract(termination);
					mpf_response->status_code = MPF_STATUS_CODE_FAILURE;
					break;
				}
				break;
			}
			case MPF_MODIFY_TERMINATION:
			{
				mpf_termination_modify(termination,mpf_request->descriptor);
				break;
			}
			case MPF_SUBTRACT_TERMINATION:
			{
				if(mpf_context_termination_subtract(context,termination) == FALSE) {
					mpf_response->status_code = MPF_STATUS_CODE_FAILURE;
					break;
				}
				mpf_termination_subtract(termination);
				break;
			}
			case MPF_ADD_ASSOCIATION:
			{
				mpf_context_association_add(context,termination,mpf_request->assoc_termination);
				break;
			}
			case MPF_REMOVE_ASSOCIATION:
			{
				mpf_context_association_remove(context,termination,mpf_request->assoc_termination);
				break;
			}
			case MPF_RESET_ASSOCIATIONS:
			{
				mpf_context_associations_reset(context);
				break;
			}
			case MPF_APPLY_TOPOLOGY:
			{
				mpf_context_topology_apply(context);
				break;
			}
			case MPF_DESTROY_TOPOLOGY:
			{
				mpf_context_topology_destroy(context);
				break;
			}
			default:
			{
				mpf_response->status_code = MPF_STATUS_CODE_FAILURE;
			}
		}
	}

	return apt_task_msg_parent_signal(engine->task,response_msg);
}

static void mpf_engine_main(mpf_scheduler_t *scheduler, void *data)
{
	mpf_engine_t *engine = data;
	apt_task_msg_t *msg;

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

	/* process factory of media contexts */
	mpf_context_factory_process(engine->context_factory);
}

MPF_DECLARE(mpf_codec_manager_t*) mpf_engine_codec_manager_create(apr_pool_t *pool)
{
	mpf_codec_manager_t *codec_manager = mpf_codec_manager_create(4,pool);
	if(codec_manager) {
		mpf_codec_t *codec;

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

MPF_DECLARE(apt_bool_t) mpf_engine_scheduler_rate_set(mpf_engine_t *engine, unsigned long rate)
{
	return mpf_scheduler_rate_set(engine->scheduler,rate);
}
