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
 * $Id: mpf_suite.c 2214 2014-11-06 03:05:51Z achaloyan@gmail.com $
 */

#include <apr_thread_cond.h>
#include "apt_test_suite.h"
#include "apt_pool.h"
#include "apt_consumer_task.h"
#include "apt_dir_layout.h"
#include "apt_log.h"
#include "mpf_engine.h"
#include "mpf_rtp_termination_factory.h"
#include "mpf_file_termination_factory.h"
#include "mpf_audio_file_descriptor.h"
#include "mpf_rtp_descriptor.h"
#include "mpf_codec_manager.h"

typedef struct mpf_suite_session_t mpf_suite_session_t;
typedef struct mpf_suite_agent_t mpf_suite_agent_t;

/** Test suite session */
struct mpf_suite_session_t {
	/** Pool to allocate memory from */
	apr_pool_t        *pool;

	/** Media context associated with the session */
	mpf_context_t     *context;
	/** The first termination in the context */
	mpf_termination_t *file_termination;
	/** The second termination in the context */
	mpf_termination_t *rtp_termination;
};

/** Test suite agent */
struct mpf_suite_agent_t {
	/** Directory layout */
	apt_dir_layout_t          *dir_layout;
	/** The main task of the test agent, which sends messages to MPF engine and 
	 * processes responses and events sent back from MPF engine */
	apt_consumer_task_t       *consumer_task;
	/** MPF engine */
	mpf_engine_t              *engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** File termination factory */
	mpf_termination_factory_t *file_termination_factory;
	/* Configuration of RTP termination factory */
	mpf_rtp_config_t          *rtp_config;
	/* RTP stream settings */
	mpf_rtp_settings_t        *rtp_settings;

	/** RTP receiver -> File */
	mpf_suite_session_t       *rx_session;
	/** File -> RTP transmitter */
	mpf_suite_session_t       *tx_session;

	/** Wait object, which is signalled to indicate shutdown */
	apr_thread_cond_t         *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t        *wait_object_mutex;
};

static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv);

static void mpf_suite_on_start_complete(apt_task_t *task);
static void mpf_suite_on_terminate_complete(apt_task_t *task);
static apt_bool_t mpf_suite_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static mpf_audio_file_descriptor_t* mpf_file_reader_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);
static mpf_audio_file_descriptor_t* mpf_file_writer_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_rx_local_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_rx_remote_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_tx_local_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);
static mpf_rtp_stream_descriptor_t* mpf_rtp_tx_remote_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session);


/** Create MPF test suite */
apt_test_suite_t* mpf_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"mpf",NULL,mpf_test_run);
	return suite;
}

/** Run MPF test suite */
static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mpf_suite_agent_t *agent;
	mpf_codec_manager_t *codec_manager;
	mpf_rtp_config_t *rtp_config;
	mpf_rtp_settings_t *rtp_settings;
	mpf_engine_t *engine;

	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;

	agent = apr_palloc(suite->pool,sizeof(mpf_suite_agent_t));

	agent->dir_layout = apt_default_dir_layout_create(NULL,suite->pool);
	engine = mpf_engine_create("MPF-Engine",suite->pool);
	if(!engine) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create MPF Engine");
		return FALSE;
	}

	codec_manager = mpf_engine_codec_manager_create(suite->pool);
	if(!codec_manager) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Codec Manager");
		return FALSE;
	}
		
	mpf_engine_codec_manager_register(engine,codec_manager);
	agent->engine = engine;

	rtp_config = mpf_rtp_config_alloc(suite->pool);
	apt_string_set(&rtp_config->ip,"127.0.0.1");
	rtp_config->rtp_port_min = 5000;
	rtp_config->rtp_port_max = 6000;

	agent->rtp_config = rtp_config;

	rtp_settings = mpf_rtp_settings_alloc(suite->pool);
	rtp_settings->ptime = 20;
	rtp_settings->jb_config.adaptive = 1;
	rtp_settings->jb_config.time_skew_detection = 1;
	rtp_settings->jb_config.min_playout_delay = 0;
	rtp_settings->jb_config.initial_playout_delay = 50;
	rtp_settings->jb_config.max_playout_delay = 800;
	mpf_codec_manager_codec_list_load(codec_manager,&rtp_settings->codec_list,"PCMU",suite->pool);

	agent->rtp_settings = rtp_settings;

	agent->rtp_termination_factory = mpf_rtp_termination_factory_create(rtp_config,suite->pool);
	agent->file_termination_factory = mpf_file_termination_factory_create(suite->pool);

	agent->rx_session = NULL;
	agent->tx_session = NULL;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),suite->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Consumer Task");
	agent->consumer_task = apt_consumer_task_create(agent,msg_pool,suite->pool);
	if(!agent->consumer_task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Consumer Task");
		return FALSE;
	}
	task = apt_consumer_task_base_get(agent->consumer_task);
	apt_task_name_set(task,"MPF-Tester");
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = mpf_suite_task_msg_process;
		vtable->on_start_complete = mpf_suite_on_start_complete;
		vtable->on_terminate_complete = mpf_suite_on_terminate_complete;
	}

	apt_task_add(task,mpf_task_get(engine));

	apr_thread_mutex_create(&agent->wait_object_mutex,APR_THREAD_MUTEX_UNNESTED,suite->pool);
	apr_thread_cond_create(&agent->wait_object,suite->pool);

	if(apt_task_start(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	apr_thread_mutex_lock(agent->wait_object_mutex);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Wait for Task to Complete");
	apr_thread_cond_wait(agent->wait_object,agent->wait_object_mutex);
	apr_thread_mutex_unlock(agent->wait_object_mutex);
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate Task [wait till complete]");
	apt_task_terminate(task,TRUE);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Task");
	apt_task_destroy(task);

	apr_thread_cond_destroy(agent->wait_object);
	apr_thread_mutex_destroy(agent->wait_object_mutex);
	return TRUE;
}

static mpf_suite_session_t* mpf_suite_rx_session_create(const mpf_suite_agent_t *agent)
{
	mpf_task_msg_t *task_msg = NULL;
	void *descriptor;
	apr_pool_t *pool;
	mpf_suite_session_t *session;

	pool = apt_pool_create();
	session = apr_palloc(pool,sizeof(mpf_suite_session_t));
	session->pool = pool;
	session->context = NULL;
	session->file_termination = NULL;
	session->rtp_termination = NULL;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create MPF Context [Rx]");
	session->context = mpf_engine_context_create(agent->engine,NULL,session,2,pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [RTP Rx]");
	session->rtp_termination = mpf_termination_create(agent->rtp_termination_factory,session,session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [RTP Rx]");
	descriptor = mpf_rtp_rx_local_descriptor_create(agent,session);
	mpf_engine_termination_message_add(
			agent->engine,
			MPF_ADD_TERMINATION,session->context,session->rtp_termination,descriptor,
			&task_msg);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [File Writer]");
	session->file_termination = mpf_termination_create(agent->file_termination_factory,session,session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [File Writer]");
	descriptor = mpf_file_writer_descriptor_create(agent,session);
	mpf_engine_termination_message_add(
			agent->engine,
			MPF_ADD_TERMINATION,session->context,session->file_termination,descriptor,
			&task_msg);

	mpf_engine_message_send(agent->engine,&task_msg);
	return session;
}

static mpf_suite_session_t* mpf_suite_tx_session_create(const mpf_suite_agent_t *agent)
{
	mpf_task_msg_t *task_msg = NULL;
	void *descriptor;
	apr_pool_t *pool;
	mpf_suite_session_t *session;

	pool = apt_pool_create();
	session = apr_palloc(pool,sizeof(mpf_suite_session_t));
	session->pool = pool;
	session->context = NULL;
	session->file_termination = NULL;
	session->rtp_termination = NULL;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create MPF Context [Tx]");
	session->context = mpf_engine_context_create(agent->engine,NULL,session,2,pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [File Reader]");
	session->file_termination = mpf_termination_create(agent->file_termination_factory,session,session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [File Reader]");
	descriptor = mpf_file_reader_descriptor_create(agent,session);
	mpf_engine_termination_message_add(
			agent->engine,
			MPF_ADD_TERMINATION,session->context,session->file_termination,descriptor,
			&task_msg);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Termination [RTP Tx]");
	session->rtp_termination = mpf_termination_create(agent->rtp_termination_factory,session,session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Termination [RTP Tx]");
	descriptor = mpf_rtp_tx_local_descriptor_create(agent,session);
	mpf_engine_termination_message_add(
			agent->engine,
			MPF_ADD_TERMINATION,session->context,session->rtp_termination,descriptor,
			&task_msg);

	mpf_engine_message_send(agent->engine,&task_msg);
	return session;
}

static void mpf_suite_session_destroy(mpf_suite_agent_t *agent, mpf_suite_session_t* session)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Destroy MPF Context");
	mpf_engine_context_destroy(session->context);
	session->context = NULL;

	if(agent->rx_session == session) {
		agent->rx_session = NULL;
	}
	else if(agent->tx_session == session) {
		agent->tx_session = NULL;
	}

	apr_pool_destroy(session->pool);

	if(!agent->tx_session && !agent->rx_session) {
		apr_thread_mutex_lock(agent->wait_object_mutex);
		apr_thread_cond_signal(agent->wait_object);
		apr_thread_mutex_unlock(agent->wait_object_mutex);
	}
}

/** Start execution of MPF test suite scenario  */
static void mpf_suite_on_start_complete(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task;
	mpf_suite_agent_t *agent;

	consumer_task = apt_task_object_get(task);
	agent = apt_consumer_task_object_get(consumer_task);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On MPF Suite Start");

	agent->rx_session = mpf_suite_rx_session_create(agent);
	agent->tx_session = mpf_suite_tx_session_create(agent);
}

/** Execution of MPF test suite scenario is terminated  */
static void mpf_suite_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On MPF Suite Terminate");
}

/** Process MPF response  */
static apt_bool_t mpf_suite_response_process(mpf_suite_agent_t *agent, const mpf_message_t *mpf_message)
{
	mpf_task_msg_t *task_msg = NULL;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Response");
	if(mpf_message->command_id == MPF_ADD_TERMINATION) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Add Termination");
		if(mpf_message->termination) {
			mpf_suite_session_t *session;
			session = mpf_termination_object_get(mpf_message->termination);
			if(session->rtp_termination == mpf_message->termination) {
				mpf_rtp_stream_descriptor_t *descriptor = NULL;
				if(session == agent->rx_session) {
					descriptor = mpf_rtp_rx_remote_descriptor_create(agent,session);
				}
				if(session == agent->tx_session) {
					descriptor = mpf_rtp_tx_remote_descriptor_create(agent,session);
				}

				if(descriptor) {
					mpf_engine_termination_message_add(
						agent->engine,
						MPF_MODIFY_TERMINATION,session->context,session->rtp_termination,descriptor,
						&task_msg);
				}

				mpf_engine_assoc_message_add(
						agent->engine,
						MPF_ADD_ASSOCIATION,session->context,session->file_termination,session->rtp_termination,
						&task_msg);

				mpf_engine_topology_message_add(
						agent->engine,
						MPF_APPLY_TOPOLOGY,session->context,
						&task_msg);
			}
		}
	}
	else if(mpf_message->command_id == MPF_SUBTRACT_TERMINATION) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Subtract Termination");
		if(mpf_message->termination) {
			mpf_suite_session_t *session;
			session = mpf_termination_object_get(mpf_message->termination);
			if(session->file_termination == mpf_message->termination) {
				session->file_termination = NULL;
			}
			else if(session->rtp_termination == mpf_message->termination) {
				session->rtp_termination = NULL;
			}
			mpf_termination_destroy(mpf_message->termination);

			if(!session->file_termination && !session->rtp_termination) {
				mpf_suite_session_destroy(agent,session);
			}
		}
	}
	return mpf_engine_message_send(agent->engine,&task_msg);
}

/** Process MPF event  */
static apt_bool_t mpf_suite_event_process(mpf_suite_agent_t *agent, const mpf_message_t *mpf_message)
{
	mpf_task_msg_t *task_msg = NULL;
	mpf_suite_session_t *session;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Event");
	if(mpf_message->termination) {
		session = mpf_termination_object_get(mpf_message->termination);
		/* first destroy existing topology */
		mpf_engine_topology_message_add(
					agent->engine,
					MPF_DESTROY_TOPOLOGY,session->context,
					&task_msg);

		if(session->file_termination) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Subtract Termination [File]");
			mpf_engine_termination_message_add(
				agent->engine,
				MPF_SUBTRACT_TERMINATION,session->context,session->file_termination,NULL,
				&task_msg);
		}
		if(session->rtp_termination) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Subtract Termination [RTP]");
			mpf_engine_termination_message_add(
				agent->engine,
				MPF_SUBTRACT_TERMINATION,session->context,session->rtp_termination,NULL,
				&task_msg);
		}
	}
	return mpf_engine_message_send(agent->engine,&task_msg);
}

/** Process task messages */
static apt_bool_t mpf_suite_task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apr_size_t i;
	const mpf_message_t *mpf_message;
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mpf_suite_agent_t *agent = apt_consumer_task_object_get(consumer_task);
	const mpf_message_container_t *container = (const mpf_message_container_t*) msg->data;
	for(i=0; i<container->count; i++) {
		mpf_message = &container->messages[i];
		if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
			mpf_suite_response_process(agent,mpf_message);
		}
		else {
			mpf_suite_event_process(agent,mpf_message);
		}
	}
	return TRUE;
}

/** Create file reader descriptor */
static mpf_audio_file_descriptor_t* mpf_file_reader_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	const char *file_path = apt_datadir_filepath_get(agent->dir_layout,"demo-8kHz.pcm",session->pool);
	mpf_audio_file_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_audio_file_descriptor_t));
	descriptor->mask = FILE_READER;
	descriptor->read_handle = NULL;
	descriptor->write_handle = NULL;
	descriptor->codec_descriptor = mpf_codec_lpcm_descriptor_create(8000,1,session->pool);
	if(file_path) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s] for Reading",file_path);
		descriptor->read_handle = fopen(file_path,"rb");
		if(!descriptor->read_handle) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File [%s]",file_path);
		}
	}
	return descriptor;
}

/** Create file writer descriptor */
static mpf_audio_file_descriptor_t* mpf_file_writer_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	const char *file_path = apt_vardir_filepath_get(agent->dir_layout,"output-8kHz.pcm",session->pool);
	mpf_audio_file_descriptor_t *descriptor = apr_palloc(session->pool,sizeof(mpf_audio_file_descriptor_t));
	descriptor->mask = FILE_WRITER;
	descriptor->max_write_size = 500000; /* ~500Kb */
	descriptor->write_handle = NULL;
	descriptor->read_handle = NULL;
	descriptor->codec_descriptor = mpf_codec_lpcm_descriptor_create(8000,1,session->pool);
	if(file_path) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s] for Writing",file_path);
		descriptor->write_handle = fopen(file_path,"wb");
		if(!descriptor->write_handle) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File [%s] for Writing",file_path);
		}
	}
	return descriptor;
}

/** Create RTP rx local descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_rx_local_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	mpf_rtp_media_descriptor_t *media_descriptor;
	mpf_rtp_stream_descriptor_t *stream_descriptor;

	media_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(media_descriptor);
	media_descriptor->state = MPF_MEDIA_ENABLED;
	media_descriptor->direction = STREAM_DIRECTION_RECEIVE;
	apt_string_set(&media_descriptor->ip,"127.0.0.1");
	media_descriptor->port = 5000;

	stream_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(stream_descriptor);
	stream_descriptor->local = media_descriptor;
	stream_descriptor->settings = agent->rtp_settings;
	return stream_descriptor;
}

/** Create RTP rx remote descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_rx_remote_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	mpf_codec_list_t *codec_list;
	mpf_codec_descriptor_t *codec_descriptor;
	mpf_rtp_media_descriptor_t *media_descriptor;
	mpf_rtp_stream_descriptor_t *stream_descriptor;

	media_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(media_descriptor);
	media_descriptor->state = MPF_MEDIA_ENABLED;
	media_descriptor->direction = STREAM_DIRECTION_SEND;
	apt_string_set(&media_descriptor->ip,"127.0.0.1");
	media_descriptor->port = 5002;
	codec_list = &media_descriptor->codec_list;
	mpf_codec_list_init(codec_list,1,session->pool);
	codec_descriptor = mpf_codec_list_add(codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 0;
		apt_string_set(&codec_descriptor->name,"PCMU");
		codec_descriptor->sampling_rate = 8000;
		codec_descriptor->channel_count = 1;
	}

	stream_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(stream_descriptor);
	stream_descriptor->remote = media_descriptor;
	stream_descriptor->settings = agent->rtp_settings;
	return stream_descriptor;
}

/** Create RTP tx local descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_tx_local_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	mpf_rtp_media_descriptor_t *media_descriptor;
	mpf_rtp_stream_descriptor_t *stream_descriptor;

	media_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(media_descriptor);
	media_descriptor->state = MPF_MEDIA_ENABLED;
	media_descriptor->direction = STREAM_DIRECTION_SEND;
	apt_string_set(&media_descriptor->ip,"127.0.0.1");
	media_descriptor->port = 5002;

	stream_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(stream_descriptor);
	stream_descriptor->local = media_descriptor;
	stream_descriptor->settings = agent->rtp_settings;
	return stream_descriptor;
}

/** Create RTP tx remote descriptor */
static mpf_rtp_stream_descriptor_t* mpf_rtp_tx_remote_descriptor_create(const mpf_suite_agent_t *agent, const mpf_suite_session_t *session)
{
	mpf_codec_list_t *codec_list;
	mpf_codec_descriptor_t *codec_descriptor;
	mpf_rtp_media_descriptor_t *media_descriptor;
	mpf_rtp_stream_descriptor_t *stream_descriptor;

	media_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(media_descriptor);
	media_descriptor->state = MPF_MEDIA_ENABLED;
	media_descriptor->direction = STREAM_DIRECTION_RECEIVE;
	apt_string_set(&media_descriptor->ip,"127.0.0.1");
	media_descriptor->port = 5000;
	codec_list = &media_descriptor->codec_list;
	mpf_codec_list_init(codec_list,1,session->pool);
	codec_descriptor = mpf_codec_list_add(codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 0;
		apt_string_set(&codec_descriptor->name,"PCMU");
		codec_descriptor->sampling_rate = 8000;
		codec_descriptor->channel_count = 1;
	}

	stream_descriptor = apr_palloc(session->pool,sizeof(mpf_rtp_stream_descriptor_t));
	mpf_rtp_stream_descriptor_init(stream_descriptor);
	stream_descriptor->remote = media_descriptor;
	stream_descriptor->settings = agent->rtp_settings;
	return stream_descriptor;
}
