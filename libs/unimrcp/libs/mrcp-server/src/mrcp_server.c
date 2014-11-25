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
 * $Id: mrcp_server.c 2251 2014-11-21 02:36:44Z achaloyan@gmail.com $
 */

#include "mrcp_server.h"
#include "mrcp_server_session.h"
#include "mrcp_message.h"
#include "mrcp_resource_factory.h"
#include "mrcp_resource.h"
#include "mrcp_engine_factory.h"
#include "mrcp_engine_loader.h"
#include "mrcp_sig_agent.h"
#include "mrcp_server_connection.h"
#include "mpf_termination_factory.h"
#include "apt_pool.h"
#include "apt_consumer_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define SERVER_TASK_NAME "MRCP Server"

/** MRCP server */
struct mrcp_server_t {
	/** Main message processing task */
	apt_consumer_task_t     *task;

	/** MRCP resource factory */
	mrcp_resource_factory_t *resource_factory;
	/** MRCP engine factory */
	mrcp_engine_factory_t   *engine_factory;
	/** Loader of plugins for MRCP engines */
	mrcp_engine_loader_t    *engine_loader;

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
	/** Table of RTP settings (mpf_rtp_settings_t*) */
	apr_hash_t              *rtp_settings_table;
	/** Table of profiles (mrcp_server_profile_t*) */
	apr_hash_t              *profile_table;

	/** Table of sessions */
	apr_hash_t              *session_table;

	/** Connection task message pool */
	apt_task_msg_pool_t     *connection_msg_pool;
	/** Engine task message pool */
	apt_task_msg_pool_t     *engine_msg_pool;

	/** Dir layout structure */
	apt_dir_layout_t        *dir_layout;
	/** Time server started at */
	apr_time_t               start_time;
	/** Memory pool */
	apr_pool_t              *pool;
};


typedef enum {
	MRCP_SERVER_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_SERVER_CONNECTION_TASK_MSG,
	MRCP_SERVER_ENGINE_TASK_MSG,
	MRCP_SERVER_MEDIA_TASK_MSG
} mrcp_server_task_msg_type_e;


static apt_bool_t mrcp_server_offer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_server_terminate_signal(mrcp_session_t *session);
static apt_bool_t mrcp_server_control_signal(mrcp_session_t *session, mrcp_message_t *message);

static const mrcp_session_request_vtable_t session_request_vtable = {
	mrcp_server_offer_signal,
	mrcp_server_terminate_signal,
	mrcp_server_control_signal,
	NULL /* mrcp_server_discover_signal */
};


/* Connection agent interface */
typedef enum {
	CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL,
	CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
	CONNECTION_AGENT_TASK_MSG_DISCONNECT
} connection_agent_task_msg_type_e;

typedef struct connection_agent_task_msg_data_t connection_agent_task_msg_data_t;
struct connection_agent_task_msg_data_t {
	mrcp_channel_t            *channel;
	mrcp_control_descriptor_t *descriptor;
	mrcp_message_t            *message;
	apt_bool_t                 status;
};

static apt_bool_t mrcp_server_channel_add_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
static apt_bool_t mrcp_server_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
static apt_bool_t mrcp_server_channel_remove_signal(mrcp_control_channel_t *channel, apt_bool_t status);
static apt_bool_t mrcp_server_message_signal(mrcp_control_channel_t *channel, mrcp_message_t *message);
static apt_bool_t mrcp_server_disconnect_signal(mrcp_control_channel_t *channel);

static const mrcp_connection_event_vtable_t connection_method_vtable = {
	mrcp_server_channel_add_signal,
	mrcp_server_channel_modify_signal,
	mrcp_server_channel_remove_signal,
	mrcp_server_message_signal,
	mrcp_server_disconnect_signal
};


/* MRCP engine interface */
typedef enum {
	ENGINE_TASK_MSG_OPEN_ENGINE,
	ENGINE_TASK_MSG_CLOSE_ENGINE,
	ENGINE_TASK_MSG_OPEN_CHANNEL,
	ENGINE_TASK_MSG_CLOSE_CHANNEL,
	ENGINE_TASK_MSG_MESSAGE
} engine_task_msg_type_e;

typedef struct engine_task_msg_data_t engine_task_msg_data_t;
struct engine_task_msg_data_t {
	mrcp_engine_t  *engine;
	mrcp_channel_t *channel;
	apt_bool_t      status;
	mrcp_message_t *mrcp_message;
};

static apt_bool_t mrcp_server_engine_open_signal(mrcp_engine_t *engine, apt_bool_t status);
static apt_bool_t mrcp_server_engine_close_signal(mrcp_engine_t *engine);

const mrcp_engine_event_vtable_t engine_vtable = {
	mrcp_server_engine_open_signal,
	mrcp_server_engine_close_signal,
};

static apt_bool_t mrcp_server_channel_open_signal(mrcp_engine_channel_t *channel, apt_bool_t status);
static apt_bool_t mrcp_server_channel_close_signal(mrcp_engine_channel_t *channel);
static apt_bool_t mrcp_server_channel_message_signal(mrcp_engine_channel_t *channel, mrcp_message_t *message);

const mrcp_engine_channel_event_vtable_t engine_channel_vtable = {
	mrcp_server_channel_open_signal,
	mrcp_server_channel_close_signal,
	mrcp_server_channel_message_signal
};

/* Task interface */
static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t mrcp_server_start_request_process(apt_task_t *task);
static apt_bool_t mrcp_server_terminate_request_process(apt_task_t *task);
static void mrcp_server_on_start_complete(apt_task_t *task);
static void mrcp_server_on_terminate_complete(apt_task_t *task);

static mrcp_session_t* mrcp_server_sig_agent_session_create(mrcp_sig_agent_t *signaling_agent);


/** Create MRCP server instance */
MRCP_DECLARE(mrcp_server_t*) mrcp_server_create(apt_dir_layout_t *dir_layout)
{
	mrcp_server_t *server;
	apr_pool_t *pool;
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	
	pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "SERVER_TASK_NAME);
	server = apr_palloc(pool,sizeof(mrcp_server_t));
	server->pool = pool;
	server->dir_layout = dir_layout;
	server->resource_factory = NULL;
	server->engine_factory = NULL;
	server->engine_loader = NULL;
	server->media_engine_table = NULL;
	server->rtp_factory_table = NULL;
	server->sig_agent_table = NULL;
	server->cnt_agent_table = NULL;
	server->rtp_settings_table = NULL;
	server->profile_table = NULL;
	server->session_table = NULL;
	server->connection_msg_pool = NULL;
	server->engine_msg_pool = NULL;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);

	server->task = apt_consumer_task_create(server,msg_pool,pool);
	if(!server->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Server Task");
		return NULL;
	}
	task = apt_consumer_task_base_get(server->task);
	apt_task_name_set(task,SERVER_TASK_NAME);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = mrcp_server_msg_process;
		vtable->process_start = mrcp_server_start_request_process;
		vtable->process_terminate = mrcp_server_terminate_request_process;
		vtable->on_start_complete = mrcp_server_on_start_complete;
		vtable->on_terminate_complete = mrcp_server_on_terminate_complete;
	}

	server->engine_factory = mrcp_engine_factory_create(server->pool);
	server->engine_loader = mrcp_engine_loader_create(server->pool);

	server->media_engine_table = apr_hash_make(server->pool);
	server->rtp_factory_table = apr_hash_make(server->pool);
	server->rtp_settings_table = apr_hash_make(server->pool);
	server->sig_agent_table = apr_hash_make(server->pool);
	server->cnt_agent_table = apr_hash_make(server->pool);

	server->profile_table = apr_hash_make(server->pool);
	
	server->session_table = apr_hash_make(server->pool);
	return server;
}

/** Start message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_server_start(mrcp_server_t *server)
{
	apt_task_t *task;
	if(!server || !server->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Server");
		return FALSE;
	}
	server->start_time = apr_time_now();
	task = apt_consumer_task_base_get(server->task);
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Server Task");
		return FALSE;
	}
	return TRUE;
}

/** Shutdown message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_server_shutdown(mrcp_server_t *server)
{
	apt_task_t *task;
	apr_time_t uptime;
	if(!server || !server->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Server");
		return FALSE;
	}
	task = apt_consumer_task_base_get(server->task);
	if(apt_task_terminate(task,TRUE) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Shutdown Server Task");
		return FALSE;
	}
	server->session_table = NULL;
	uptime = apr_time_now() - server->start_time;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Server Uptime [%"APR_TIME_T_FMT" sec]", apr_time_sec(uptime));
	return TRUE;
}

/** Destroy MRCP server */
MRCP_DECLARE(apt_bool_t) mrcp_server_destroy(mrcp_server_t *server)
{
	apt_task_t *task;
	if(!server || !server->task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Server");
		return FALSE;
	}

	mrcp_engine_factory_destroy(server->engine_factory);
	mrcp_engine_loader_destroy(server->engine_loader);

	task = apt_consumer_task_base_get(server->task);
	apt_task_destroy(task);

	apr_pool_destroy(server->pool);
	return TRUE;
}

/** Register MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_server_resource_factory_register(mrcp_server_t *server, mrcp_resource_factory_t *resource_factory)
{
	if(!resource_factory) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Resource Factory");
	server->resource_factory = resource_factory;
	return TRUE;
}

/** Register MRCP engine */
MRCP_DECLARE(apt_bool_t) mrcp_server_engine_register(mrcp_server_t *server, mrcp_engine_t *engine)
{
	if(!engine || !engine->id) {
		return FALSE;
	}
	
	if(!server->engine_msg_pool) {
		server->engine_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(engine_task_msg_data_t),server->pool);
	}
	engine->codec_manager = server->codec_manager;
	engine->dir_layout = server->dir_layout;
	engine->event_vtable = &engine_vtable;
	engine->event_obj = server;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register MRCP Engine [%s]",engine->id);
	return mrcp_engine_factory_engine_register(server->engine_factory,engine);
}

/** Register codec manager */
MRCP_DECLARE(apt_bool_t) mrcp_server_codec_manager_register(mrcp_server_t *server, mpf_codec_manager_t *codec_manager)
{
	if(!codec_manager) {
		return FALSE;
	}
	server->codec_manager = codec_manager;
	return TRUE;
}

/** Get registered codec manager */
MRCP_DECLARE(const mpf_codec_manager_t*) mrcp_server_codec_manager_get(const mrcp_server_t *server)
{
	return server->codec_manager;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_server_media_engine_register(mrcp_server_t *server, mpf_engine_t *media_engine)
{
	const char *id;
	if(!media_engine) {
		return FALSE;
	}
	id = mpf_engine_id_get(media_engine);
	if(!id) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Media Engine [%s]",id);
	mpf_engine_codec_manager_register(media_engine,server->codec_manager);
	apr_hash_set(server->media_engine_table,id,APR_HASH_KEY_STRING,media_engine);
	mpf_engine_task_msg_type_set(media_engine,MRCP_SERVER_MEDIA_TASK_MSG);
	if(server->task) {
		apt_task_t *media_task = mpf_task_get(media_engine);
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,media_task);
	}
	return TRUE;
}

/** Get media engine by name */
MRCP_DECLARE(mpf_engine_t*) mrcp_server_media_engine_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->media_engine_table,name,APR_HASH_KEY_STRING);
}

/** Register RTP termination factory */
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_factory_register(mrcp_server_t *server, mpf_termination_factory_t *rtp_termination_factory, const char *name)
{
	if(!rtp_termination_factory || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register RTP Termination Factory [%s]",name);
	apr_hash_set(server->rtp_factory_table,name,APR_HASH_KEY_STRING,rtp_termination_factory);
	return TRUE;
}

/** Get RTP termination factory by name */
MRCP_DECLARE(mpf_termination_factory_t*) mrcp_server_rtp_factory_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->rtp_factory_table,name,APR_HASH_KEY_STRING);
}

/** Register RTP settings */
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_settings_register(mrcp_server_t *server, mpf_rtp_settings_t *rtp_settings, const char *name)
{
	if(!rtp_settings || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register RTP Settings [%s]",name);
	apr_hash_set(server->rtp_settings_table,name,APR_HASH_KEY_STRING,rtp_settings);
	return TRUE;
}

/** Get RTP settings by name */
MRCP_DECLARE(mpf_rtp_settings_t*) mrcp_server_rtp_settings_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->rtp_settings_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_server_signaling_agent_register(mrcp_server_t *server, mrcp_sig_agent_t *signaling_agent)
{
	if(!signaling_agent || !signaling_agent->id) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Signaling Agent [%s]",signaling_agent->id);
	signaling_agent->parent = server;
	signaling_agent->resource_factory = server->resource_factory;
	signaling_agent->create_server_session = mrcp_server_sig_agent_session_create;
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mrcp_signaling_message_t*),server->pool);
	apr_hash_set(server->sig_agent_table,signaling_agent->id,APR_HASH_KEY_STRING,signaling_agent);
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Get signaling agent by name */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_server_signaling_agent_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->sig_agent_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_register(mrcp_server_t *server, mrcp_connection_agent_t *connection_agent)
{
	const char *id;
	if(!connection_agent) {
		return FALSE;
	}
	id = mrcp_server_connection_agent_id_get(connection_agent);
	if(!id) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Connection Agent [%s]",id);
	mrcp_server_connection_resource_factory_set(connection_agent,server->resource_factory);
	mrcp_server_connection_agent_handler_set(connection_agent,server,&connection_method_vtable);
	server->connection_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_task_msg_data_t),server->pool);
	apr_hash_set(server->cnt_agent_table,id,APR_HASH_KEY_STRING,connection_agent);
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_t *connection_task = mrcp_server_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
	return TRUE;
}

/** Get connection agent by name */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->cnt_agent_table,name,APR_HASH_KEY_STRING);
}

/** Create MRCP profile */
MRCP_DECLARE(mrcp_server_profile_t*) mrcp_server_profile_create(
										const char *id,
										mrcp_version_e mrcp_version,
										mrcp_resource_factory_t *resource_factory,
										mrcp_sig_agent_t *signaling_agent,
										mrcp_connection_agent_t *connection_agent,
										mpf_engine_t *media_engine,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										apr_pool_t *pool)
{
	mrcp_server_profile_t *profile = apr_palloc(pool,sizeof(mrcp_server_profile_t));
	profile->id = id;
	profile->mrcp_version = mrcp_version;
	profile->resource_factory = resource_factory;
	profile->engine_table = NULL;
	profile->media_engine = media_engine;
	profile->rtp_termination_factory = rtp_factory;
	profile->rtp_settings = rtp_settings;
	profile->signaling_agent = signaling_agent;
	profile->connection_agent = connection_agent;

	mpf_termination_factory_engine_assign(rtp_factory,media_engine);
	return profile;
}

static apt_bool_t mrcp_server_engine_table_make(mrcp_server_t *server, mrcp_server_profile_t *profile, apr_table_t *plugin_map)
{
	int i;
	mrcp_resource_t *resource;
	const char *plugin_name = NULL;
	mrcp_engine_t *engine;

	profile->engine_table = apr_hash_make(server->pool);
	for(i=0; i<MRCP_RESOURCE_TYPE_COUNT; i++) {
		resource = mrcp_resource_get(server->resource_factory,i);
		if(!resource) continue;
		
		engine = NULL;
		/* first, try to find engine by name specified in plugin map (if available) */
		if(plugin_map) {
			plugin_name = apr_table_get(plugin_map,resource->name.buf);
			if(plugin_name) {
				engine = mrcp_engine_factory_engine_get(server->engine_factory,plugin_name);
			}
		}

		/* next, if no engine found or specified, try to find the first available one */
		if(!engine) {
			engine = mrcp_engine_factory_engine_find(server->engine_factory,i);
		}
		
		if(engine) {
			if(engine->id) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Assign MRCP Engine [%s] [%s]",resource->name.buf,engine->id);
			}
			apr_hash_set(profile->engine_table,resource->name.buf,resource->name.length,engine);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No MRCP Engine Available [%s]",resource->name.buf);
		}
	}

	return TRUE;
}

/** Register MRCP profile */
MRCP_DECLARE(apt_bool_t) mrcp_server_profile_register(
							mrcp_server_t *server,
							mrcp_server_profile_t *profile,
							apr_table_t *plugin_map)
{
	if(!profile || !profile->id) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile: no name");
		return FALSE;
	}
	if(!profile->resource_factory) {
		if(!server->resource_factory) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing resource factory",profile->id);
			return FALSE;
		}
		profile->resource_factory = server->resource_factory;
	}
	mrcp_server_engine_table_make(server,profile,plugin_map);
	
	if(!profile->signaling_agent) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing signaling agent",profile->id);
		return FALSE;
	}
	if(profile->mrcp_version == MRCP_VERSION_2 &&
		!profile->connection_agent) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing connection agent",profile->id);
		return FALSE;
	}
	if(!profile->media_engine) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing media engine",profile->id);
		return FALSE;
	}
	if(!profile->rtp_termination_factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing RTP factory",profile->id);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Profile [%s]",profile->id);
	apr_hash_set(server->profile_table,profile->id,APR_HASH_KEY_STRING,profile);
	return TRUE;
}

/** Get profile by name */
MRCP_DECLARE(mrcp_server_profile_t*) mrcp_server_profile_get(const mrcp_server_t *server, const char *name)
{
	return apr_hash_get(server->profile_table,name,APR_HASH_KEY_STRING);
}

/** Load MRCP engine */
MRCP_DECLARE(mrcp_engine_t*) mrcp_server_engine_load(
								mrcp_server_t *server,
								const char *id,
								const char *path,
								mrcp_engine_config_t *config)
{
	mrcp_engine_t *engine;
	if(!id || !path || !config) {
		return FALSE;
	}

	engine = mrcp_engine_loader_plugin_load(server->engine_loader,id,path,config);
	if(!engine) {
		return FALSE;
	}

	return engine;
}

MRCP_DECLARE(apr_pool_t*) mrcp_server_memory_pool_get(const mrcp_server_t *server)
{
	return server->pool;
}

void mrcp_server_session_add(mrcp_server_session_t *session)
{
	if(session->base.id.buf) {
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Add Session "APT_SID_FMT,MRCP_SESSION_SID(&session->base));
		apr_hash_set(session->server->session_table,session->base.id.buf,session->base.id.length,session);
	}
}

void mrcp_server_session_remove(mrcp_server_session_t *session)
{
	if(session->base.id.buf) {
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Remove Session "APT_SID_FMT,MRCP_SESSION_SID(&session->base));
		apr_hash_set(session->server->session_table,session->base.id.buf,session->base.id.length,NULL);
	}
}

static APR_INLINE mrcp_server_session_t* mrcp_server_session_find(mrcp_server_t *server, const apt_str_t *session_id)
{
	return apr_hash_get(server->session_table,session_id->buf,session_id->length);
}

static apt_bool_t mrcp_server_start_request_process(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);

	mrcp_engine_t *engine;
	apr_hash_index_t *it;
	void *val;
	it = mrcp_engine_factory_engine_first(server->engine_factory);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine) {
			if(mrcp_engine_virtual_open(engine) == TRUE) {
				apt_task_start_request_add(task);
			}
		}
	}

	return apt_task_start_request_process(task);
}

static apt_bool_t mrcp_server_terminate_request_process(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);

	mrcp_engine_t *engine;
	apr_hash_index_t *it;
	void *val;
	it = mrcp_engine_factory_engine_first(server->engine_factory);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine) {
			if(mrcp_engine_virtual_close(engine) == TRUE) {
				apt_task_terminate_request_add(task);
			}
		}
	}

	return apt_task_terminate_request_process(task);
}

static void mrcp_server_on_start_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,SERVER_TASK_NAME" Started");
}

static void mrcp_server_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,SERVER_TASK_NAME" Terminated");
}

static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	switch(msg->type) {
		case MRCP_SERVER_SIGNALING_TASK_MSG:
		{
			mrcp_signaling_message_t **signaling_message = (mrcp_signaling_message_t**) msg->data;
			mrcp_server_signaling_message_process(*signaling_message);
			break;
		}
		case MRCP_SERVER_CONNECTION_TASK_MSG:
		{
			const connection_agent_task_msg_data_t *connection_message = (const connection_agent_task_msg_data_t*)msg->data;
			switch(msg->sub_type) {
				case CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL:
				{
					mrcp_server_on_channel_modify(connection_message->channel,connection_message->descriptor,connection_message->status);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL:
				{
					mrcp_server_on_channel_modify(connection_message->channel,connection_message->descriptor,connection_message->status);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL:
				{
					mrcp_server_on_channel_remove(connection_message->channel,connection_message->status);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE:
				{
					mrcp_server_on_channel_message(connection_message->channel, connection_message->message);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_DISCONNECT:
				{
					mrcp_server_on_channel_message(connection_message->channel, connection_message->message);
					break;
				}
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_ENGINE_TASK_MSG:
		{
			engine_task_msg_data_t *data = (engine_task_msg_data_t*)msg->data;
			switch(msg->sub_type) {
				case ENGINE_TASK_MSG_OPEN_ENGINE:
					mrcp_engine_on_open(data->engine,data->status);
					apt_task_start_request_remove(task);
					break;
				case ENGINE_TASK_MSG_CLOSE_ENGINE:
					mrcp_engine_on_close(data->engine);
					apt_task_terminate_request_remove(task);
					break;
				case ENGINE_TASK_MSG_OPEN_CHANNEL:
					mrcp_server_on_engine_channel_open(data->channel,data->status);
					break;
				case ENGINE_TASK_MSG_CLOSE_CHANNEL:
					mrcp_server_on_engine_channel_close(data->channel);
					break;
				case ENGINE_TASK_MSG_MESSAGE:
					mrcp_server_on_engine_channel_message(data->channel,data->mrcp_message);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_MEDIA_TASK_MSG:
		{
			mpf_message_container_t *mpf_message_container = (mpf_message_container_t*) msg->data;
			mrcp_server_mpf_message_process(mpf_message_container);
			break;
		}
		default:
		{
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown Task Message Received [%d;%d]", msg->type,msg->sub_type);
			break;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_signaling_task_msg_signal(mrcp_signaling_message_type_e type, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_message_t *message)
{
	mrcp_signaling_message_t *signaling_message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	mrcp_signaling_message_t **slot = ((mrcp_signaling_message_t**)task_msg->data);
	task_msg->type = MRCP_SERVER_SIGNALING_TASK_MSG;
	task_msg->sub_type = type;
	
	signaling_message = apr_palloc(session->pool,sizeof(mrcp_signaling_message_t));
	signaling_message->type = type;
	signaling_message->session = (mrcp_server_session_t*)session;
	signaling_message->descriptor = descriptor;
	signaling_message->channel = NULL;
	signaling_message->message = message;
	*slot = signaling_message;
	
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_server_connection_task_msg_signal(
							connection_agent_task_msg_type_e type,
							mrcp_connection_agent_t         *agent,
							mrcp_control_channel_t          *channel,
							mrcp_control_descriptor_t       *descriptor,
							mrcp_message_t                  *message,
							apt_bool_t                       status)
{
	mrcp_server_t *server = mrcp_server_connection_agent_object_get(agent);
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	connection_agent_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->connection_msg_pool);
	task_msg->type = MRCP_SERVER_CONNECTION_TASK_MSG;
	task_msg->sub_type = type;
	data = (connection_agent_task_msg_data_t*) task_msg->data;
	data->channel = channel ? channel->obj : NULL;
	data->descriptor = descriptor;
	data->message = message;
	data->status = status;

	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_server_engine_task_msg_signal(
							engine_task_msg_type_e  type,
							mrcp_engine_t          *engine,
							apt_bool_t              status)
{
	mrcp_server_t *server = engine->event_obj;
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	engine_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->engine_msg_pool);
	task_msg->type = MRCP_SERVER_ENGINE_TASK_MSG;
	task_msg->sub_type = type;
	data = (engine_task_msg_data_t*) task_msg->data;
	data->engine = engine;
	data->channel = NULL;
	data->status = status;
	data->mrcp_message = NULL;

	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_server_channel_task_msg_signal(
							engine_task_msg_type_e  type,
							mrcp_engine_channel_t  *engine_channel,
							apt_bool_t              status,
							mrcp_message_t         *message)
{
	mrcp_channel_t *channel = engine_channel->event_obj;
	mrcp_session_t *session = mrcp_server_channel_session_get(channel);
	mrcp_server_t *server = session->signaling_agent->parent;
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	engine_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->engine_msg_pool);
	task_msg->type = MRCP_SERVER_ENGINE_TASK_MSG;
	task_msg->sub_type = type;
	data = (engine_task_msg_data_t*) task_msg->data;
	data->engine = engine_channel->engine;
	data->channel = channel;
	data->status = status;
	data->mrcp_message = message;

	return apt_task_msg_signal(task,task_msg);
}

static mrcp_server_profile_t* mrcp_server_profile_get_by_agent(mrcp_server_t *server, mrcp_server_session_t *session, const mrcp_sig_agent_t *signaling_agent)
{
	mrcp_server_profile_t *profile;
	apr_hash_index_t *it;
	void *val;
	it = apr_hash_first(session->base.pool,server->profile_table);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		profile = val;
		if(profile && profile->signaling_agent == signaling_agent) {
			return profile;
		}
	}
	return NULL;
}

static mrcp_session_t* mrcp_server_sig_agent_session_create(mrcp_sig_agent_t *signaling_agent)
{
	mrcp_server_t *server = signaling_agent->parent;
	mrcp_server_session_t *session = mrcp_server_session_create();
	session->server = server;
	session->profile = mrcp_server_profile_get_by_agent(server,session,signaling_agent);
	if(!session->profile) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Find Profile by Agent "APT_NAMESID_FMT,
			session->base.name,
			MRCP_SESSION_SID(&session->base));
		mrcp_session_destroy(&session->base);
		return NULL;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Session "APT_NAMESID_FMT" [%s]",
			session->base.name,
			MRCP_SESSION_SID(&session->base), 
			session->profile->id);
	session->base.signaling_agent = signaling_agent;
	session->base.request_vtable = &session_request_vtable;
	return &session->base;
}

static apt_bool_t mrcp_server_offer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return mrcp_server_signaling_task_msg_signal(SIGNALING_MESSAGE_OFFER,session,descriptor,NULL);
}

static apt_bool_t mrcp_server_terminate_signal(mrcp_session_t *session)
{
	return mrcp_server_signaling_task_msg_signal(SIGNALING_MESSAGE_TERMINATE,session,NULL,NULL);
}

static apt_bool_t mrcp_server_control_signal(mrcp_session_t *session, mrcp_message_t *message)
{
	return mrcp_server_signaling_task_msg_signal(SIGNALING_MESSAGE_CONTROL,session,NULL,message);
}

static apt_bool_t mrcp_server_channel_add_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_ADD_CHANNEL,
								agent,
								channel,
								descriptor,
								NULL,
								status);
}

static apt_bool_t mrcp_server_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_MODIFY_CHANNEL,
								agent,
								channel,
								descriptor,
								NULL,
								status);
}

static apt_bool_t mrcp_server_channel_remove_signal(mrcp_control_channel_t *channel, apt_bool_t status)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_REMOVE_CHANNEL,
								agent,
								channel,
								NULL,
								NULL,
								status);
}

static apt_bool_t mrcp_server_message_signal(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
								agent,
								channel,
								NULL,
								message,
								TRUE);
}

static apt_bool_t mrcp_server_disconnect_signal(mrcp_control_channel_t *channel)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_DISCONNECT,
								agent,
								channel,
								NULL,
								NULL,
								TRUE);
}

static apt_bool_t mrcp_server_engine_open_signal(mrcp_engine_t *engine, apt_bool_t status)
{
	return mrcp_server_engine_task_msg_signal(
								ENGINE_TASK_MSG_OPEN_ENGINE,
								engine,
								status);
}

static apt_bool_t mrcp_server_engine_close_signal(mrcp_engine_t *engine)
{
	return mrcp_server_engine_task_msg_signal(
								ENGINE_TASK_MSG_CLOSE_ENGINE,
								engine,
								TRUE);
}

static apt_bool_t mrcp_server_channel_open_signal(mrcp_engine_channel_t *channel, apt_bool_t status)
{
	return mrcp_server_channel_task_msg_signal(
								ENGINE_TASK_MSG_OPEN_CHANNEL,
								channel,
								status,
								NULL);
}

static apt_bool_t mrcp_server_channel_close_signal(mrcp_engine_channel_t *channel)
{
	return mrcp_server_channel_task_msg_signal(
								ENGINE_TASK_MSG_CLOSE_CHANNEL,
								channel,
								TRUE,
								NULL);
}

static apt_bool_t mrcp_server_channel_message_signal(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_server_channel_task_msg_signal(
								ENGINE_TASK_MSG_MESSAGE,
								channel,
								TRUE,
								message);
}
