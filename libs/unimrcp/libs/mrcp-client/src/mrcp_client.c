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
#include <apr_hash.h>
#include "mrcp_client.h"
#include "mrcp_resource_factory.h"
#include "mrcp_resource.h"
#include "mrcp_sig_agent.h"
#include "mrcp_client_session.h"
#include "mrcp_client_connection.h"
#include "mrcp_message.h"
#include "mpf_engine.h"
#include "mpf_termination_factory.h"
#include "mpf_codec_manager.h"
#include "apt_pool.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define CLIENT_TASK_NAME "MRCP Client"

/** MRCP client */
struct mrcp_client_t {
	/** Main message processing task */
	apt_consumer_task_t     *task;

	/** MRCP resource factory */
	mrcp_resource_factory_t *resource_factory;
	/** Codec manager */
	mpf_codec_manager_t     *codec_manager;
	/** Table of media processing engines (mpf_engine_t*) */
	apr_hash_t              *media_engine_table;
	/** Table of RTP termination factories (mpf_termination_factory_t*) */
	apr_hash_t              *rtp_factory_table;
	/** Table of signaling agents (mrcp_sig_agent_t*) */
	apr_hash_t              *sig_agent_table;
	/** Table of connection agents (mrcp_connection_agent_t*) */
	apr_hash_t              *cnt_agent_table;
	/** Table of profiles (mrcp_profile_t*) */
	apr_hash_t              *profile_table;

	/** Table of applications (mrcp_application_t*) */
	apr_hash_t              *app_table;

	/** Table of sessions/handles */
	apr_hash_t              *session_table;

	/** Connection task message pool */
	apt_task_msg_pool_t     *cnt_msg_pool;
	
	/** Event handler used in case of async start  */
	mrcp_client_handler_f    on_start_complete;
	/** Wait object used in case of synch start  */
	apr_thread_cond_t       *sync_start_object;
	/** Mutex to protect sync start routine */
	apr_thread_mutex_t      *sync_start_mutex;
	
	/** Dir layout structure */
	apt_dir_layout_t        *dir_layout;
	/** Memory pool */
	apr_pool_t              *pool;
};


typedef enum {
	MRCP_CLIENT_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_CLIENT_CONNECTION_TASK_MSG,
	MRCP_CLIENT_MEDIA_TASK_MSG,
	MRCP_CLIENT_APPLICATION_TASK_MSG
} mrcp_client_task_msg_type_e;

/* Signaling agent interface */
typedef enum {
	SIG_AGENT_TASK_MSG_ANSWER,
	SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE,
	SIG_AGENT_TASK_MSG_CONTROL_RESPONSE,
	SIG_AGENT_TASK_MSG_DISCOVER_RESPONSE,
	SIG_AGENT_TASK_MSG_TERMINATE_EVENT
} sig_agent_task_msg_type_e;

typedef struct sig_agent_task_msg_data_t sig_agent_task_msg_data_t;
struct sig_agent_task_msg_data_t {
	mrcp_client_session_t     *session;
	mrcp_session_descriptor_t *descriptor;
	mrcp_message_t            *message;
};

static apt_bool_t mrcp_client_answer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_terminate_response_signal(mrcp_session_t *session);
static apt_bool_t mrcp_client_control_response_signal(mrcp_session_t *session, mrcp_message_t *message);
static apt_bool_t mrcp_client_discover_response_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);

static apt_bool_t mrcp_client_terminate_event_signal(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_client_answer_signal,
	mrcp_client_terminate_response_signal,
	mrcp_client_control_response_signal,
	mrcp_client_discover_response_signal
};

static const mrcp_session_event_vtable_t session_event_vtable = {
	mrcp_client_terminate_event_signal
};

/* Connection agent interface */
typedef enum {
	CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
	CONNECTION_AGENT_TASK_MSG_DISCONNECT
} connection_agent_task_msg_type_e ;

typedef struct connection_agent_task_msg_data_t connection_agent_task_msg_data_t;
struct connection_agent_task_msg_data_t {
	mrcp_channel_t            *channel;
	mrcp_control_descriptor_t *descriptor;
	mrcp_message_t            *message;
	apt_bool_t                 status;
};

static apt_bool_t mrcp_client_channel_add_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
static apt_bool_t mrcp_client_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
static apt_bool_t mrcp_client_channel_remove_signal(mrcp_control_channel_t *channel, apt_bool_t status);
static apt_bool_t mrcp_client_message_signal(mrcp_control_channel_t *channel, mrcp_message_t *message);
static apt_bool_t mrcp_client_disconnect_signal(mrcp_control_channel_t *channel);

static const mrcp_connection_event_vtable_t connection_method_vtable = {
	mrcp_client_channel_add_signal,
	mrcp_client_channel_modify_signal,
	mrcp_client_channel_remove_signal,
	mrcp_client_message_signal,
	mrcp_client_disconnect_signal
};

/* Task interface */
static void mrcp_client_on_start_complete(apt_task_t *task);
static void mrcp_client_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t mrcp_app_signaling_task_msg_signal(mrcp_sig_command_e command_id, mrcp_session_t *session, mrcp_channel_t *channel);
static apt_bool_t mrcp_app_control_task_msg_signal(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);


/** Create MRCP client instance */
MRCP_DECLARE(mrcp_client_t*) mrcp_client_create(apt_dir_layout_t *dir_layout)
{
	mrcp_client_t *client;
	apr_pool_t *pool;
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	
	pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "CLIENT_TASK_NAME);
	client = apr_palloc(pool,sizeof(mrcp_client_t));
	client->pool = pool;
	client->dir_layout = dir_layout;
	client->resource_factory = NULL;
	client->media_engine_table = NULL;
	client->rtp_factory_table = NULL;
	client->sig_agent_table = NULL;
	client->cnt_agent_table = NULL;
	client->profile_table = NULL;
	client->app_table = NULL;
	client->session_table = NULL;
	client->cnt_msg_pool = NULL;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);
	client->task = apt_consumer_task_create(client,msg_pool,pool);
	if(!client->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Client Task");
		return NULL;
	}
	task = apt_consumer_task_base_get(client->task);
	apt_task_name_set(task,CLIENT_TASK_NAME);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = mrcp_client_msg_process;
		vtable->on_start_complete = mrcp_client_on_start_complete;
		vtable->on_terminate_complete = mrcp_client_on_terminate_complete;
	}

	client->media_engine_table = apr_hash_make(client->pool);
	client->rtp_factory_table = apr_hash_make(client->pool);
	client->sig_agent_table = apr_hash_make(client->pool);
	client->cnt_agent_table = apr_hash_make(client->pool);
	client->profile_table = apr_hash_make(client->pool);
	client->app_table = apr_hash_make(client->pool);
	
	client->session_table = apr_hash_make(client->pool);

	client->on_start_complete = NULL;
	client->sync_start_object = NULL;
	client->sync_start_mutex = NULL;
	return client;
}

/** Set asynchronous start mode */
MRCP_DECLARE(void) mrcp_client_async_start_set(mrcp_client_t *client, mrcp_client_handler_f handler)
{
	if(client) {
		client->on_start_complete = handler;
	}
}

/** Start message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_client_start(mrcp_client_t *client)
{
	apt_bool_t sync_start = TRUE;
	apt_task_t *task;
	if(!client || !client->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Client");
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);

	if(client->on_start_complete) {
		sync_start = FALSE;
	}

	if(sync_start == TRUE) {
		/* get prepared to start stack synchronously */
		apr_thread_mutex_create(&client->sync_start_mutex,APR_THREAD_MUTEX_DEFAULT,client->pool);
		apr_thread_cond_create(&client->sync_start_object,client->pool);
		
		apr_thread_mutex_lock(client->sync_start_mutex);
	}

	if(apt_task_start(task) == FALSE) {
		if(sync_start == TRUE) {
			apr_thread_mutex_unlock(client->sync_start_mutex);
		}
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Client Task");
		return FALSE;
	}
	
	if(sync_start == TRUE) {
		/* wait for start complete */
		apr_thread_cond_wait(client->sync_start_object,client->sync_start_mutex);
		apr_thread_mutex_unlock(client->sync_start_mutex);
	}

	return TRUE;
}

/** Shutdown message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_client_shutdown(mrcp_client_t *client)
{
	apt_task_t *task;
	if(!client || !client->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Client");
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	if(apt_task_terminate(task,TRUE) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Shutdown Client Task");
		return FALSE;
	}
	client->session_table = NULL;

	if(client->sync_start_object) {
		apr_thread_cond_destroy(client->sync_start_object);
		client->sync_start_object = NULL;
	}
	if(client->sync_start_mutex) {
		apr_thread_mutex_destroy(client->sync_start_mutex);
		client->sync_start_mutex = NULL;
	}

	return TRUE;
}

/** Destroy MRCP client */
MRCP_DECLARE(apt_bool_t) mrcp_client_destroy(mrcp_client_t *client)
{
	apt_task_t *task;
	if(!client || !client->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Client");
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	apt_task_destroy(task);

	apr_pool_destroy(client->pool);
	return TRUE;
}


/** Register MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_client_resource_factory_register(mrcp_client_t *client, mrcp_resource_factory_t *resource_factory)
{
	if(!resource_factory) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Resource Factory");
	client->resource_factory = resource_factory;
	return TRUE;
}

/** Register codec manager */
MRCP_DECLARE(apt_bool_t) mrcp_client_codec_manager_register(mrcp_client_t *client, mpf_codec_manager_t *codec_manager)
{
	if(!codec_manager) {
		return FALSE;
	}
	client->codec_manager = codec_manager;
	return TRUE;
}

/** Get registered codec manager */
MRCP_DECLARE(const mpf_codec_manager_t*) mrcp_client_codec_manager_get(mrcp_client_t *client)
{
	return client->codec_manager;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_client_media_engine_register(mrcp_client_t *client, mpf_engine_t *media_engine, const char *name)
{
	if(!media_engine || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Media Engine [%s]",name);
	mpf_engine_codec_manager_register(media_engine,client->codec_manager);
	apr_hash_set(client->media_engine_table,name,APR_HASH_KEY_STRING,media_engine);
	mpf_engine_task_msg_type_set(media_engine,MRCP_CLIENT_MEDIA_TASK_MSG);
	if(client->task) {
		apt_task_t *media_task = mpf_task_get(media_engine);
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,media_task);
	}
	return TRUE;
}

/** Get media engine by name */
MRCP_DECLARE(mpf_engine_t*) mrcp_client_media_engine_get(mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->media_engine_table,name,APR_HASH_KEY_STRING);
}

/** Register RTP termination factory */
MRCP_DECLARE(apt_bool_t) mrcp_client_rtp_factory_register(mrcp_client_t *client, mpf_termination_factory_t *rtp_termination_factory, const char *name)
{
	if(!rtp_termination_factory || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register RTP Termination Factory [%s]",name);
	apr_hash_set(client->rtp_factory_table,name,APR_HASH_KEY_STRING,rtp_termination_factory);
	return TRUE;
}

/** Get RTP termination factory by name */
MRCP_DECLARE(mpf_termination_factory_t*) mrcp_client_rtp_factory_get(mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->rtp_factory_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_agent_register(mrcp_client_t *client, mrcp_sig_agent_t *signaling_agent, const char *name)
{
	if(!signaling_agent || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Signaling Agent [%s]",name);
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(sig_agent_task_msg_data_t),client->pool);
	signaling_agent->parent = client;
	signaling_agent->resource_factory = client->resource_factory;
	apr_hash_set(client->sig_agent_table,name,APR_HASH_KEY_STRING,signaling_agent);
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Get signaling agent by name */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_client_signaling_agent_get(mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->sig_agent_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_register(mrcp_client_t *client, mrcp_connection_agent_t *connection_agent, const char *name)
{
	if(!connection_agent || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Connection Agent [%s]",name);
	mrcp_client_connection_resource_factory_set(connection_agent,client->resource_factory);
	mrcp_client_connection_agent_handler_set(connection_agent,client,&connection_method_vtable);
	client->cnt_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_task_msg_data_t),client->pool);
	apr_hash_set(client->cnt_agent_table,name,APR_HASH_KEY_STRING,connection_agent);
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_t *connection_task = mrcp_client_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
	return TRUE;
}

/** Get connection agent by name */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_get(mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->cnt_agent_table,name,APR_HASH_KEY_STRING);
}

/** Create MRCP profile */
MRCP_DECLARE(mrcp_profile_t*) mrcp_client_profile_create(
									mrcp_resource_factory_t *resource_factory,
									mrcp_sig_agent_t *signaling_agent,
									mrcp_connection_agent_t *connection_agent,
									mpf_engine_t *media_engine,
									mpf_termination_factory_t *rtp_factory,
									apr_pool_t *pool)
{
	mrcp_profile_t *profile = apr_palloc(pool,sizeof(mrcp_profile_t));
	profile->resource_factory = resource_factory;
	profile->media_engine = media_engine;
	profile->rtp_termination_factory = rtp_factory;
	profile->signaling_agent = signaling_agent;
	profile->connection_agent = connection_agent;
	return profile;
}

/** Register MRCP profile */
MRCP_DECLARE(apt_bool_t) mrcp_client_profile_register(mrcp_client_t *client, mrcp_profile_t *profile, const char *name)
{
	if(!profile || !name) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile: no name",name);
		return FALSE;
	}
	if(!profile->resource_factory) {
		profile->resource_factory = client->resource_factory;
	}
	if(!profile->signaling_agent) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing signaling agent",name);
		return FALSE;
	}
	if(profile->signaling_agent->mrcp_version == MRCP_VERSION_2 &&
		!profile->connection_agent) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing connection agent",name);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Profile [%s]",name);
	apr_hash_set(client->profile_table,name,APR_HASH_KEY_STRING,profile);
	return TRUE;
}

/** Get profile by name */
MRCP_DECLARE(mrcp_profile_t*) mrcp_client_profile_get(mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->profile_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP application */
MRCP_DECLARE(apt_bool_t) mrcp_client_application_register(mrcp_client_t *client, mrcp_application_t *application, const char *name)
{
	if(!application || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Application [%s]",name);
	application->client = client;
	application->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mrcp_app_message_t*),client->pool);
	apr_hash_set(client->app_table,name,APR_HASH_KEY_STRING,application);
	return TRUE;
}

/** Get memory pool */
MRCP_DECLARE(apr_pool_t*) mrcp_client_memory_pool_get(mrcp_client_t *client)
{
	return client->pool;
}


/** Create application instance */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(const mrcp_app_message_handler_f handler, void *obj, apr_pool_t *pool)
{
	mrcp_application_t *application;
	if(!handler) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Application");
	application = apr_palloc(pool,sizeof(mrcp_application_t));
	application->obj = obj;
	application->handler = handler;
	application->client = NULL;
	return application;
}

/** Destroy application instance */
MRCP_DECLARE(apt_bool_t) mrcp_application_destroy(mrcp_application_t *application)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Application");
	return TRUE;
}

/** Get external object associated with the application */
MRCP_DECLARE(void*) mrcp_application_object_get(mrcp_application_t *application)
{
	return application->obj;
}

/** Get dir layout structure */
MRCP_DECLARE(const apt_dir_layout_t*) mrcp_application_dir_layout_get(mrcp_application_t *application)
{
	return application->client->dir_layout;
}



/** Create client session */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, const char *profile_name, void *obj)
{
	mrcp_profile_t *profile;
	mrcp_client_session_t *session;
	if(!application || !application->client) {
		return NULL;
	}

	profile = mrcp_client_profile_get(application->client,profile_name);
	if(!profile) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such Profile [%s]",profile_name);
		return NULL;
	}

	session = mrcp_client_session_create(application,obj);
	if(!session) {
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCP Handle "APT_PTR_FMT" [%s]",
		MRCP_SESSION_PTR(session),
		profile_name);
	session->profile = profile;
	session->codec_manager = application->client->codec_manager;
	session->base.response_vtable = &session_response_vtable;
	session->base.event_vtable = &session_event_vtable;
	return &session->base;
}

/** Get memory pool the session object is created out of */
MRCP_DECLARE(apr_pool_t*) mrcp_application_session_pool_get(mrcp_session_t *session)
{
	if(!session) {
		return NULL;
	}
	return session->pool;
}

/** Get session identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_application_session_id_get(mrcp_session_t *session)
{
	if(!session) {
		return NULL;
	}
	return &session->id;
}

/** Get external object associated with the session */
MRCP_DECLARE(void*) mrcp_application_session_object_get(mrcp_session_t *session)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(!client_session) {
		return NULL;
	}
	return client_session->app_obj;
}

/** Set (associate) external object to the session */
MRCP_DECLARE(void) mrcp_application_session_object_set(mrcp_session_t *session, void *obj)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(client_session) {
		client_session->app_obj = obj;
	}
}

/** Send session update request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_update(mrcp_session_t *session)
{
	if(!session) {
		return FALSE;
	}
	return mrcp_app_signaling_task_msg_signal(MRCP_SIG_COMMAND_SESSION_UPDATE,session,NULL);
}

/** Send session termination request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_terminate(mrcp_session_t *session)
{
	if(!session) {
		return FALSE;
	}
	return mrcp_app_signaling_task_msg_signal(MRCP_SIG_COMMAND_SESSION_TERMINATE,session,NULL);
}

/** Destroy client session (session must be terminated prior to destroy) */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_destroy(mrcp_session_t *session)
{
	if(!session) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy MRCP Handle "APT_PTR_FMT,MRCP_SESSION_PTR(session));
	mrcp_session_destroy(session);
	return TRUE;
}


/** Create control channel */
MRCP_DECLARE(mrcp_channel_t*) mrcp_application_channel_create(
									mrcp_session_t *session, 
									mrcp_resource_id resource_id, 
									mpf_termination_t *termination, 
									mpf_rtp_termination_descriptor_t *rtp_descriptor, 
									void *obj)
{
	mrcp_resource_t *resource;
	mrcp_profile_t *profile;
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(!client_session || !client_session->profile) {
		/* Invalid params */
		return FALSE;
	}
	profile = client_session->profile;

	if(!profile->resource_factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Channel: invalid profile");
		return FALSE;
	}
	resource = mrcp_resource_get(profile->resource_factory,resource_id);
	if(!resource) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Channel: no such resource");
		return FALSE;
	}

	if(termination) {
		/* Media engine and RTP factory must be specified in this case */
		if(!profile->media_engine || !profile->rtp_termination_factory) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Channel: invalid profile");
			return FALSE;
		}
	}
	else {
		/* Either termination or rtp_descriptor must be specified */
		if(!rtp_descriptor) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Channel: missing both termination and RTP descriptor");
			return FALSE;
		}
	}

	return mrcp_client_channel_create(session,resource,termination,rtp_descriptor,obj);
}

/** Get external object associated with the channel */
MRCP_DECLARE(void*) mrcp_application_channel_object_get(mrcp_channel_t *channel)
{
	if(!channel) {
		return FALSE;
	}
	return channel->obj;
}

/** Get RTP termination descriptor */
MRCP_DECLARE(mpf_rtp_termination_descriptor_t*) mrcp_application_rtp_descriptor_get(mrcp_channel_t *channel)
{
	if(!channel || !channel->rtp_termination_slot) {
		return NULL;
	}
	return channel->rtp_termination_slot->descriptor;
}

/** Get codec descriptor of source stream */
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_source_descriptor_get(mrcp_channel_t *channel)
{
	mpf_audio_stream_t *audio_stream;
	if(!channel || !channel->termination) {
		return NULL;
	}
	audio_stream = mpf_termination_audio_stream_get(channel->termination);
	if(!audio_stream) {
		return NULL;
	}
	return audio_stream->rx_descriptor;
}

/** Get codec descriptor of sink stream */
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_sink_descriptor_get(mrcp_channel_t *channel)
{
	mpf_audio_stream_t *audio_stream;
	if(!channel || !channel->termination) {
		return NULL;
	}
	audio_stream = mpf_termination_audio_stream_get(channel->termination);
	if(!audio_stream) {
		return NULL;
	}
	return audio_stream->tx_descriptor;
}


/** Send channel add request */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_add(mrcp_session_t *session, mrcp_channel_t *channel)
{
	if(!session || !channel) {
		return FALSE;
	}
	return mrcp_app_signaling_task_msg_signal(MRCP_SIG_COMMAND_CHANNEL_ADD,session,channel);
}

/** Send channel removal request */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_remove(mrcp_session_t *session, mrcp_channel_t *channel)
{
	if(!session || !channel) {
		return FALSE;
	}
	return mrcp_app_signaling_task_msg_signal(MRCP_SIG_COMMAND_CHANNEL_REMOVE,session,channel);
}

/** Send resource discovery request */
MRCP_DECLARE(apt_bool_t) mrcp_application_resource_discover(mrcp_session_t *session)
{
	if(!session) {
		return FALSE;
	}
	return mrcp_app_signaling_task_msg_signal(MRCP_SIG_COMMAND_RESOURCE_DISCOVER,session,NULL);
}

/** Create MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_application_message_create(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_method_id method_id)
{
	mrcp_message_t *mrcp_message;
	mrcp_profile_t *profile;
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(!client_session || !channel || !channel->resource) {
		return NULL;
	}
	profile = client_session->profile;
	if(!profile || !profile->resource_factory) {
		return NULL;
	}
	mrcp_message = mrcp_request_create(
						channel->resource,
						profile->signaling_agent->mrcp_version,
						method_id,
						session->pool);
	return mrcp_message;
}

/** Send MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_send(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	if(!session || !channel || !message) {
		return FALSE;
	}
	return mrcp_app_control_task_msg_signal(session,channel,message);
}

/** 
 * Create audio termination
 * @param session the session to create termination for
 * @param stream_vtable the virtual table of audio stream
 * @param capabilities the capabilities of the stream
 * @param obj the external object
 */
MRCP_DECLARE(mpf_termination_t*) mrcp_application_audio_termination_create(
										mrcp_session_t *session,
										const mpf_audio_stream_vtable_t *stream_vtable,
										mpf_stream_capabilities_t *capabilities,
										void *obj)
{
	mpf_audio_stream_t *audio_stream;

	if(!capabilities) {
		return NULL;
	}

	if(mpf_codec_capabilities_validate(&capabilities->codecs) == FALSE) {
		return NULL;
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			obj,                  /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			session->pool);       /* memory pool to allocate memory from */
	if(!audio_stream) {
		return NULL;
	}

	/* create raw termination */
	return mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			session->pool);       /* memory pool to allocate memory from */
}

/** Create source media termination */
MRCP_DECLARE(mpf_termination_t*) mrcp_application_source_termination_create(
										mrcp_session_t *session,
										const mpf_audio_stream_vtable_t *stream_vtable,
										mpf_codec_descriptor_t *codec_descriptor,
										void *obj)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_audio_stream_t *audio_stream;

	capabilities = mpf_source_stream_capabilities_create(session->pool);
	if(codec_descriptor) {
		mpf_codec_capabilities_add(
						&capabilities->codecs,
						mpf_sample_rate_mask_get(codec_descriptor->sampling_rate),
						codec_descriptor->name.buf);
	}
	else {
		mpf_codec_default_capabilities_add(&capabilities->codecs);
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			obj,                  /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			session->pool);       /* memory pool to allocate memory from */

	if(!audio_stream) {
		return NULL;
	}

	audio_stream->rx_descriptor = codec_descriptor;

	/* create raw termination */
	return mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			session->pool);       /* memory pool to allocate memory from */
}

MRCP_DECLARE(mpf_termination_t*) mrcp_application_sink_termination_create(
										mrcp_session_t *session,
										const mpf_audio_stream_vtable_t *stream_vtable,
										mpf_codec_descriptor_t *codec_descriptor,
										void *obj)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_audio_stream_t *audio_stream;

	capabilities = mpf_sink_stream_capabilities_create(session->pool);
	if(codec_descriptor) {
		mpf_codec_capabilities_add(
						&capabilities->codecs,
						mpf_sample_rate_mask_get(codec_descriptor->sampling_rate),
						codec_descriptor->name.buf);
	}
	else {
		mpf_codec_default_capabilities_add(&capabilities->codecs);
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			obj,                  /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			session->pool);       /* memory pool to allocate memory from */
	if(!audio_stream) {
		return NULL;
	}

	audio_stream->tx_descriptor = codec_descriptor;

	/* create raw termination */
	return mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			session->pool);       /* memory pool to allocate memory from */
}

void mrcp_client_session_add(mrcp_client_t *client, mrcp_client_session_t *session)
{
	if(session) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add MRCP Handle "APT_PTR_FMT,MRCP_SESSION_PTR(session));
		apr_hash_set(client->session_table,session,sizeof(session),session);
	}
}

void mrcp_client_session_remove(mrcp_client_t *client, mrcp_client_session_t *session)
{
	if(session) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove MRCP Handle "APT_PTR_FMT,MRCP_SESSION_PTR(session));
		apr_hash_set(client->session_table,session,sizeof(session),NULL);
	}
}

static void mrcp_client_on_start_complete(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_client_t *client = apt_consumer_task_object_get(consumer_task);
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,CLIENT_TASK_NAME" Started");
	if(client->on_start_complete) {
		/* async start */
		client->on_start_complete(TRUE);
	}
	else {
		/* sync start */
		apr_thread_mutex_lock(client->sync_start_mutex);
		apr_thread_cond_signal(client->sync_start_object);
		apr_thread_mutex_unlock(client->sync_start_mutex);
	}
}

static void mrcp_client_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,CLIENT_TASK_NAME" Terminated");
}


static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_client_t *client = apt_consumer_task_object_get(consumer_task);
	if(!client) {
		return FALSE;
	}
	switch(msg->type) {
		case MRCP_CLIENT_SIGNALING_TASK_MSG:
		{
			const sig_agent_task_msg_data_t *sig_message = (const sig_agent_task_msg_data_t*)msg->data;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receive Signaling Task Message [%d]", msg->sub_type);
			switch(msg->sub_type) {
				case SIG_AGENT_TASK_MSG_ANSWER:
					mrcp_client_session_answer_process(sig_message->session,sig_message->descriptor);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE:
					mrcp_client_session_terminate_response_process(sig_message->session);
					break;
				case SIG_AGENT_TASK_MSG_CONTROL_RESPONSE:
					mrcp_client_session_control_response_process(sig_message->session,sig_message->message);
					break;
				case SIG_AGENT_TASK_MSG_DISCOVER_RESPONSE:
					mrcp_client_session_discover_response_process(sig_message->session,sig_message->descriptor);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE_EVENT:
					mrcp_client_session_terminate_event_process(sig_message->session);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_CLIENT_CONNECTION_TASK_MSG:
		{
			const connection_agent_task_msg_data_t *data = (const connection_agent_task_msg_data_t*)msg->data;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receive Connection Task Message [%d]", msg->sub_type);
			switch(msg->sub_type) {
				case CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL:
					mrcp_client_on_channel_add(data->channel,data->descriptor,data->status);
					break;
				case CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL:
					mrcp_client_on_channel_modify(data->channel,data->descriptor,data->status);
					break;
				case CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL:
					mrcp_client_on_channel_remove(data->channel,data->status);
					break;
				case CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE:
					mrcp_client_on_message_receive(data->channel,data->message);
					break;
				case CONNECTION_AGENT_TASK_MSG_DISCONNECT:
					mrcp_client_on_disconnect(data->channel);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_CLIENT_MEDIA_TASK_MSG:
		{
			mpf_message_container_t *container = (mpf_message_container_t*) msg->data;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receive Media Task Message");
			mrcp_client_mpf_message_process(container);
			break;
		}
		case MRCP_CLIENT_APPLICATION_TASK_MSG:
		{
			mrcp_app_message_t **app_message = (mrcp_app_message_t**) msg->data;
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receive Application Task Message [%d]", (*app_message)->message_type);
			mrcp_client_app_message_process(*app_message);
			break;
		}
		default:
		{
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Receive Unknown Task Message [%d]", msg->type);
			break;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_app_signaling_task_msg_signal(mrcp_sig_command_e command_id, mrcp_session_t *session, mrcp_channel_t *channel)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	mrcp_application_t *application = client_session->application;
	apt_task_t *task = apt_consumer_task_base_get(application->client->task);
	apt_task_msg_t *task_msg = apt_task_msg_acquire(application->msg_pool);
	if(task_msg) {
		mrcp_app_message_t **slot = ((mrcp_app_message_t**)task_msg->data);
		mrcp_app_message_t *app_message;
		task_msg->type = MRCP_CLIENT_APPLICATION_TASK_MSG;

		app_message = mrcp_client_app_signaling_request_create(command_id,session->pool);
		app_message->application = client_session->application;
		app_message->session = session;
		app_message->channel = channel;
		app_message->control_message = NULL;
		app_message->descriptor = NULL;
		*slot = app_message;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Application Task Message");
	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_app_control_task_msg_signal(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	mrcp_application_t *application = client_session->application;
	apt_task_t *task = apt_consumer_task_base_get(application->client->task);
	apt_task_msg_t *task_msg = apt_task_msg_acquire(application->msg_pool);
	if(task_msg) {
		mrcp_app_message_t **slot = ((mrcp_app_message_t**)task_msg->data);
		mrcp_app_message_t *app_message;
		task_msg->type = MRCP_CLIENT_APPLICATION_TASK_MSG;

		app_message = mrcp_client_app_control_message_create(session->pool);
		app_message->application = client_session->application;
		app_message->session = session;
		app_message->channel = channel;
		app_message->control_message = message;
		*slot = app_message;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Application Task Message");
	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_client_signaling_task_msg_signal(sig_agent_task_msg_type_e type, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_message_t *message)
{
	sig_agent_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_CLIENT_SIGNALING_TASK_MSG;
	task_msg->sub_type = type;
	data = (sig_agent_task_msg_data_t*) task_msg->data;
	data->session = (mrcp_client_session_t*)session;
	data->descriptor = descriptor;
	data->message = message;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Signaling Task Message");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_client_connection_task_msg_signal(
							connection_agent_task_msg_type_e type,
							mrcp_connection_agent_t         *agent, 
							mrcp_control_channel_t          *channel,
							mrcp_control_descriptor_t       *descriptor,
							mrcp_message_t                  *message,
							apt_bool_t                       status)
{
	apt_task_t *task;
	apt_task_msg_t *task_msg;
	connection_agent_task_msg_data_t *data;
	mrcp_client_t *client = mrcp_client_connection_agent_object_get(agent);
	if(!client || !client->cnt_msg_pool) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	task_msg = apt_task_msg_acquire(client->cnt_msg_pool);
	task_msg->type = MRCP_CLIENT_CONNECTION_TASK_MSG;
	task_msg->sub_type = type;
	data = (connection_agent_task_msg_data_t*) task_msg->data;
	data->channel = channel ? channel->obj : NULL;
	data->descriptor = descriptor;
	data->message = message;
	data->status = status;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Signal Connection Task Message");
	return apt_task_msg_signal(task,task_msg);
}



static apt_bool_t mrcp_client_answer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return mrcp_client_signaling_task_msg_signal(SIG_AGENT_TASK_MSG_ANSWER,session,descriptor,NULL);
}

static apt_bool_t mrcp_client_terminate_response_signal(mrcp_session_t *session)
{
	return mrcp_client_signaling_task_msg_signal(SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE,session,NULL,NULL);
}

static apt_bool_t mrcp_client_control_response_signal(mrcp_session_t *session, mrcp_message_t *message)
{
	return mrcp_client_signaling_task_msg_signal(SIG_AGENT_TASK_MSG_CONTROL_RESPONSE,session,NULL,message);
}

static apt_bool_t mrcp_client_discover_response_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return mrcp_client_signaling_task_msg_signal(SIG_AGENT_TASK_MSG_DISCOVER_RESPONSE,session,descriptor,NULL);
}

static apt_bool_t mrcp_client_terminate_event_signal(mrcp_session_t *session)
{
	return mrcp_client_signaling_task_msg_signal(SIG_AGENT_TASK_MSG_TERMINATE_EVENT,session,NULL,NULL);
}


static apt_bool_t mrcp_client_channel_add_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	return mrcp_client_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL,
								channel->agent,
								channel,
								descriptor,
								NULL,
								status);
}

static apt_bool_t mrcp_client_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	return mrcp_client_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL,
								channel->agent,
								channel,
								descriptor,
								NULL,
								status);
}

static apt_bool_t mrcp_client_channel_remove_signal(mrcp_control_channel_t *channel, apt_bool_t status)
{
	return mrcp_client_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL,
								channel->agent,
								channel,
								NULL,
								NULL,
								status);
}

static apt_bool_t mrcp_client_message_signal(mrcp_control_channel_t *channel, mrcp_message_t *mrcp_message)
{
	return mrcp_client_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
								channel->agent,
								channel,
								NULL,
								mrcp_message,
								TRUE);
}

static apt_bool_t mrcp_client_disconnect_signal(mrcp_control_channel_t *channel)
{
	return mrcp_client_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_DISCONNECT,
								channel->agent,
								channel,
								NULL,
								NULL,
								TRUE);
}
