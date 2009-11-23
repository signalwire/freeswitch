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

#include <apr_thread_cond.h>
#include "apt_test_suite.h"
#include "mpf_engine.h"
#include "mpf_rtp_termination_factory.h"
#include "mpf_file_termination_factory.h"
#include "mpf_audio_file_descriptor.h"
#include "mpf_rtp_descriptor.h"
#include "apt_pool.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

typedef struct mpf_suite_session_t mpf_suite_session_t;
typedef struct mpf_suite_engine_t mpf_suite_engine_t;


/** Test suite session */
struct mpf_suite_session_t {
	/** Pool to allocate memory from */
	apr_pool_t        *pool;

	/** Media context associated with the session */
	mpf_context_t     *context;
	/** The first termination in the context */
	mpf_termination_t *termination1;
	/** The second termination in the context */
	mpf_termination_t *termination2;
	/** RTP or file termination mode */
	apt_bool_t         rtp_mode;
};

/** Test suite engine */
struct mpf_suite_engine_t {
	/** The main task of the test engine, which sends messages to MPF engine and 
	 * processes responses and events sent back from MPF engine */
	apt_consumer_task_t       *consumer_task;
	/** MPF engine */
	mpf_engine_t              *engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** File termination factory */
	mpf_termination_factory_t *file_termination_factory;

	/** Wait object, which is signalled to indicate shutdown */
	apr_thread_cond_t         *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t        *wait_object_mutex;
};

static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv);

static void mpf_suite_on_start_complete(apt_task_t *task);
static void mpf_suite_on_terminate_complete(apt_task_t *task);
static apt_bool_t mpf_suite_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static mpf_audio_file_descriptor_t* mpf_file_reader_descriptor_create(mpf_suite_session_t *session);
static mpf_audio_file_descriptor_t* mpf_file_writer_descriptor_create(mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_local_descriptor_create(mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_remote_descriptor_create(mpf_suite_session_t *session);


/** Create MPF test suite */
apt_test_suite_t* mpf_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"mpf",NULL,mpf_test_run);
	return suite;
}

/** Run MPF test suite */
static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mpf_suite_engine_t *suite_engine;
	mpf_codec_manager_t *codec_manager;
	mpf_rtp_config_t *config;
	mpf_engine_t *engine;

	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;

	suite_engine = apr_palloc(suite->pool,sizeof(mpf_suite_engine_t));

	engine = mpf_engine_create(suite->pool);
	if(!engine) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create MPF Engine");
		return FALSE;
	}
	codec_manager = mpf_engine_codec_manager_create(suite->pool);
	if(codec_manager) {
		mpf_engine_codec_manager_register(engine,codec_manager);
	}
	suite_engine->engine = engine;

	config = mpf_rtp_config_create(suite->pool);
	apt_string_set(&config->ip,"127.0.0.1");
	config->rtp_port_min = 5000;
	config->rtp_port_min = 6000;
	suite_engine->rtp_termination_factory = mpf_rtp_termination_factory_create(config,suite->pool);
	suite_engine->file_termination_factory = mpf_file_termination_factory_create(suite->pool);

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),suite->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Consumer Task");
	suite_engine->consumer_task = apt_consumer_task_create(suite_engine,msg_pool,suite->pool);
	if(!suite_engine->consumer_task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Consumer Task");
		return FALSE;
	}
	task = apt_consumer_task_base_get(suite_engine->consumer_task);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = mpf_suite_task_msg_process;
		vtable->on_start_complete = mpf_suite_on_start_complete;
		vtable->on_terminate_complete = mpf_suite_on_terminate_complete;
	}

	apt_task_add(task,mpf_task_get(engine));

	apr_thread_mutex_create(&suite_engine->wait_object_mutex,APR_THREAD_MUTEX_UNNESTED,suite->pool);
	apr_thread_cond_create(&suite_engine->wait_object,suite->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Start Task");
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	apr_thread_mutex_lock(suite_engine->wait_object_mutex);
	apr_thread_cond_wait(suite_engine->wait_object,suite_engine->wait_object_mutex);
	apr_thread_mutex_unlock(suite_engine->wait_object_mutex);
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate Task [wait till complete]");
	apt_task_terminate(task,TRUE);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Task");
	apt_task_destroy(task);

	apr_thread_cond_destroy(suite_engine->wait_object);
	apr_thread_mutex_destroy(suite_engine->wait_object_mutex);
	return TRUE;
}

/** Start execution of MPF test suite scenario  */
static void mpf_suite_on_start_complete(apt_task_t *task)
{
	mpf_suite_session_t *session;
	apt_task_t *consumer_task;
	mpf_suite_engine_t *suite_engine;
	mpf_task_msg_t *task_msg = NULL;
	void *descriptor;
	apr_pool_t *pool = NULL;

	consumer_task = apt_task_object_get(task);
	suite_engine = apt_task_object_get(consumer_task);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On MPF Suite Start");
	pool = apt_pool_create();
	session = apr_palloc(pool,sizeof(mpf_suite_session_t));
	session->pool = pool;
	session->context = NULL;
	session->termination1 = NULL;
	session->termination2 = NULL;
	session->rtp_mode = TRUE;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create MPF Context");
	session->context = mpf_engine_context_create(suite_engine->engine,session,2,pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [1]");
	session->termination1 = mpf_termination_create(suite_engine->file_termination_factory,session,session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [1]");
	descriptor = mpf_file_reader_descriptor_create(session);
	mpf_engine_termination_message_add(
			suite_engine->engine,
			MPF_ADD_TERMINATION,session->context,session->termination1,descriptor,
			&task_msg);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [2]");
	if(session->rtp_mode == TRUE) {
		session->termination2 = mpf_termination_create(suite_engine->rtp_termination_factory,session,session->pool);
	}
	else {
		session->termination2 = mpf_termination_create(suite_engine->file_termination_factory,session,session->pool);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [2]");
	descriptor = NULL;
	if(session->rtp_mode == TRUE) {
		descriptor = mpf_rtp_local_descriptor_create(session);
	}
	else {
		descriptor = mpf_file_writer_descriptor_create(session);
	}

	mpf_engine_termination_message_add(
			suite_engine->engine,
			MPF_ADD_TERMINATION,session->context,session->termination2,descriptor,
			&task_msg);

	mpf_engine_message_send(suite_engine->engine,&task_msg);
}

/** Execution of MPF test suite scenario is terminated  */
static void mpf_suite_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On MPF Suite Terminate");
}

/** Process MPF response  */
static apt_bool_t mpf_suite_response_process(mpf_suite_engine_t *suite_engine, const mpf_message_t *mpf_message)
{
	mpf_task_msg_t *task_msg = NULL;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Response");
	if(mpf_message->command_id == MPF_ADD_TERMINATION) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Add Termination");
		if(mpf_message->termination) {
			mpf_suite_session_t *session;
			session = mpf_termination_object_get(mpf_message->termination);
			if(session->termination2 == mpf_message->termination && session->rtp_mode == TRUE) {
				void *descriptor = mpf_rtp_remote_descriptor_create(session);
				mpf_engine_termination_message_add(
					suite_engine->engine,
					MPF_MODIFY_TERMINATION,session->context,session->termination2,descriptor,
					&task_msg);
			}
		}
	}
	else if(mpf_message->command_id == MPF_SUBTRACT_TERMINATION) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Subtract Termination");
		if(mpf_message->termination) {
			mpf_suite_session_t *session;
			session = mpf_termination_object_get(mpf_message->termination);
			if(session->termination1 == mpf_message->termination) {
				session->termination1 = NULL;
			}
			if(session->termination2 == mpf_message->termination) {
				session->termination2 = NULL;
			}
			mpf_termination_destroy(mpf_message->termination);

			if(!session->termination1 && !session->termination2) {
				mpf_engine_context_destroy(session->context);
				session->context = NULL;
				apr_pool_destroy(session->pool);

				apr_thread_mutex_lock(suite_engine->wait_object_mutex);
				apr_thread_cond_signal(suite_engine->wait_object);
				apr_thread_mutex_unlock(suite_engine->wait_object_mutex);
			}
		}
	}
	return mpf_engine_message_send(suite_engine->engine,&task_msg);
}

/** Process MPF event  */
static apt_bool_t mpf_suite_event_process(mpf_suite_engine_t *suite_engine, const mpf_message_t *mpf_message)
{
	mpf_task_msg_t *task_msg = NULL;
	mpf_suite_session_t *session;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Event");
	if(mpf_message->termination) {
		session = mpf_termination_object_get(mpf_message->termination);
		if(session->termination1) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Subtract Termination [1]");
			mpf_engine_termination_message_add(
				suite_engine->engine,
				MPF_SUBTRACT_TERMINATION,session->context,session->termination1,NULL,
				&task_msg);
		}
		if(session->termination2) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Subtract Termination [2]");
			mpf_engine_termination_message_add(
				suite_engine->engine,
				MPF_SUBTRACT_TERMINATION,session->context,session->termination2,NULL,
				&task_msg);
		}
	}
	return mpf_engine_message_send(suite_engine->engine,&task_msg);
}

/** Process task messages */
static apt_bool_t mpf_suite_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apr_size_t i;
	const mpf_message_t *mpf_message;
	apt_task_t *consumer_task = apt_task_object_get(task);
	mpf_suite_engine_t *suite_engine = apt_task_object_get(consumer_task);
	const mpf_message_container_t *container = (const mpf_message_container_t*) msg->data;
	for(i=0; i<container->count; i++) {
		mpf_message = &container->messages[i];
		if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
			mpf_suite_response_process(suite_engine,mpf_message);
		}
		else {
			mpf_suite_event_process(suite_engine,mpf_message);
		}
	}
	return TRUE;
}

/** Create sample file reader descriptor */
static mpf_audio_file_descriptor_t* mpf_file_reader_descriptor_create(mpf_suite_session_t *session)
{
	mpf_audio_file_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_audio_file_descriptor_t));
	descriptor->mask = FILE_READER;
	descriptor->read_handle = fopen("demo.pcm","rb");
	descriptor->write_handle = NULL;
	descriptor->codec_descriptor = mpf_codec_lpcm_descriptor_create(8000,1,session->pool);
	return descriptor;
}

/** Create sample file writer descriptor */
static mpf_audio_file_descriptor_t* mpf_file_writer_descriptor_create(mpf_suite_session_t *session)
{
	mpf_audio_file_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_audio_file_descriptor_t));
	descriptor->mask = FILE_WRITER;
	descriptor->max_write_size = 500000; /* 500Kb */
	descriptor->write_handle = fopen("demo_out.pcm","wb");
	descriptor->read_handle = NULL;
	descriptor->codec_descriptor = mpf_codec_lpcm_descriptor_create(8000,1,session->pool);
	return descriptor;
}

/** Create sample RTP local descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_local_descriptor_create(mpf_suite_session_t *session)
{
	mpf_rtp_stream_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(descriptor);
	descriptor->local = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(descriptor->local);
	descriptor->local->direction = STREAM_DIRECTION_NONE;
	apt_string_set(&descriptor->local->ip,"127.0.0.1");
	descriptor->local->port = 5000;
	return descriptor;
}

/** Create sample RTP remote descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_remote_descriptor_create(mpf_suite_session_t *session)
{
	mpf_codec_list_t *codec_list;
	mpf_codec_descriptor_t *codec_descriptor;
	mpf_rtp_stream_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(descriptor);
	descriptor->remote = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(descriptor->remote);
	descriptor->remote->direction = STREAM_DIRECTION_DUPLEX;
	apt_string_set(&descriptor->remote->ip,"127.0.0.1");
	descriptor->remote->port = 5002;
	codec_list = &descriptor->remote->codec_list;
	mpf_codec_list_init(codec_list,2,session->pool);
	codec_descriptor = mpf_codec_list_add(codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 0;
	}
	codec_descriptor = mpf_codec_list_add(codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 96;
		apt_string_set(&codec_descriptor->name,"PCMU");
		codec_descriptor->sampling_rate = 16000;
		codec_descriptor->channel_count = 1;
	}

	return descriptor;
}
