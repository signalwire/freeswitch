/*
 * Copyright 2008-2015 Arsen Chaloyan
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
#include "mrcp_sig_agent.h"
#include "mrcp_client_session.h"
#include "mrcp_client_connection.h"
#include "mrcp_ca_factory.h"
#include "mpf_engine_factory.h"
#include "apt_consumer_task.h"
#include "apt_pool.h"
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
	/** Table of signaling settings (mrcp_sig_settings_t*) */
	apr_hash_t              *sig_settings_table;
	/** Table of connection agents (mrcp_connection_agent_t*) */
	apr_hash_t              *cnt_agent_table;
	/** Table of RTP settings (mpf_rtp_settings_t*) */
	apr_hash_t              *rtp_settings_table;
	/** Table of profiles (mrcp_client_profile_t*) */
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
	client->sig_settings_table = NULL;
	client->cnt_agent_table = NULL;
	client->rtp_settings_table = NULL;
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
	client->sig_settings_table = apr_hash_make(client->pool);
	client->cnt_agent_table = apr_hash_make(client->pool);
	client->rtp_settings_table = apr_hash_make(client->pool);
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
MRCP_DECLARE(const mpf_codec_manager_t*) mrcp_client_codec_manager_get(const mrcp_client_t *client)
{
	return client->codec_manager;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_client_media_engine_register(mrcp_client_t *client, mpf_engine_t *media_engine)
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
	mpf_engine_codec_manager_register(media_engine,client->codec_manager);
	apr_hash_set(client->media_engine_table,id,APR_HASH_KEY_STRING,media_engine);
	mpf_engine_task_msg_type_set(media_engine,MRCP_CLIENT_MEDIA_TASK_MSG);
	if(client->task) {
		apt_task_t *media_task = mpf_task_get(media_engine);
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,media_task);
	}
	return TRUE;
}

/** Get media engine by name */
MRCP_DECLARE(mpf_engine_t*) mrcp_client_media_engine_get(const mrcp_client_t *client, const char *name)
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
MRCP_DECLARE(mpf_termination_factory_t*) mrcp_client_rtp_factory_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->rtp_factory_table,name,APR_HASH_KEY_STRING);
}

/** Register RTP settings */
MRCP_DECLARE(apt_bool_t) mrcp_client_rtp_settings_register(mrcp_client_t *client, mpf_rtp_settings_t *rtp_settings, const char *name)
{
	if(!rtp_settings || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register RTP Settings [%s]",name);
	apr_hash_set(client->rtp_settings_table,name,APR_HASH_KEY_STRING,rtp_settings);
	return TRUE;
}

/** Get RTP settings by name */
MRCP_DECLARE(mpf_rtp_settings_t*) mrcp_client_rtp_settings_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->rtp_settings_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_agent_register(mrcp_client_t *client, mrcp_sig_agent_t *signaling_agent)
{
	if(!signaling_agent || !signaling_agent->id) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Signaling Agent [%s]",signaling_agent->id);
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(sig_agent_task_msg_data_t),client->pool);
	signaling_agent->parent = client;
	signaling_agent->resource_factory = client->resource_factory;
	apr_hash_set(client->sig_agent_table,signaling_agent->id,APR_HASH_KEY_STRING,signaling_agent);
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Get signaling agent by name */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_client_signaling_agent_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->sig_agent_table,name,APR_HASH_KEY_STRING);
}

/** Register MRCP signaling settings */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_settings_register(mrcp_client_t *client, mrcp_sig_settings_t *signaling_settings, const char *name)
{
	if(!signaling_settings || !name) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Signaling Settings [%s]",name);
	apr_hash_set(client->sig_settings_table,name,APR_HASH_KEY_STRING,signaling_settings);
	return TRUE;
}

/** Get signaling settings by name */
MRCP_DECLARE(mrcp_sig_settings_t*) mrcp_client_signaling_settings_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->sig_settings_table,name,APR_HASH_KEY_STRING);
}


/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_register(mrcp_client_t *client, mrcp_connection_agent_t *connection_agent)
{
	const char *id;
	if(!connection_agent) {
		return FALSE;
	}
	id = mrcp_client_connection_agent_id_get(connection_agent);
	if(!id) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Connection Agent [%s]",id);
	mrcp_client_connection_resource_factory_set(connection_agent,client->resource_factory);
	mrcp_client_connection_agent_handler_set(connection_agent,client,&connection_method_vtable);
	client->cnt_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_task_msg_data_t),client->pool);
	apr_hash_set(client->cnt_agent_table,id,APR_HASH_KEY_STRING,connection_agent);
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_t *connection_task = mrcp_client_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
	return TRUE;
}

/** Get connection agent by name */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->cnt_agent_table,name,APR_HASH_KEY_STRING);
}

/** Create MRCP profile */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_create(
										mrcp_resource_factory_t *resource_factory,
										mrcp_sig_agent_t *signaling_agent,
										mrcp_connection_agent_t *connection_agent,
										mpf_engine_t *media_engine,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										mrcp_sig_settings_t *signaling_settings,
										apr_pool_t *pool)
{
	mrcp_sa_factory_t *sa_factory = NULL;
	mrcp_ca_factory_t *ca_factory = NULL;
	mpf_engine_factory_t *mpf_factory = NULL;
	mrcp_version_e mrcp_version = MRCP_VERSION_2;
	if(!connection_agent)
		mrcp_version = MRCP_VERSION_1;

	if(signaling_agent) {
		sa_factory = mrcp_sa_factory_create(pool);
		mrcp_sa_factory_agent_add(sa_factory,signaling_agent);
	}

	if(connection_agent) {
		ca_factory = mrcp_ca_factory_create(pool);
		mrcp_ca_factory_agent_add(ca_factory,connection_agent);
	}

	if(media_engine) {
		mpf_factory = mpf_engine_factory_create(pool);
		mpf_engine_factory_engine_add(mpf_factory,media_engine);
	}

	return mrcp_client_profile_create_ex(
				mrcp_version,
				resource_factory,
				sa_factory,
				ca_factory,
				mpf_factory,
				rtp_factory,
				rtp_settings,
				signaling_settings,
				pool);
}

/** Create MRCP profile (extended version) */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_create_ex(
										mrcp_version_e mrcp_version,
										mrcp_resource_factory_t *resource_factory,
										mrcp_sa_factory_t *sa_factory,
										mrcp_ca_factory_t *ca_factory,
										mpf_engine_factory_t *mpf_factory,
										mpf_termination_factory_t *rtp_factory,
										mpf_rtp_settings_t *rtp_settings,
										mrcp_sig_settings_t *signaling_settings,
										apr_pool_t *pool)
{
	mrcp_client_profile_t *profile = apr_palloc(pool,sizeof(mrcp_client_profile_t));
	profile->name = NULL;
	profile->tag = NULL;
	profile->mrcp_version = mrcp_version;
	profile->resource_factory = resource_factory;
	profile->mpf_factory = mpf_factory;
	profile->rtp_termination_factory = rtp_factory;
	profile->rtp_settings = rtp_settings;
	profile->sa_factory = sa_factory;
	profile->ca_factory = ca_factory;
	profile->signaling_settings = signaling_settings;

	if(mpf_factory && rtp_factory)
		mpf_engine_factory_rtp_factory_assign(mpf_factory,rtp_factory);
	return profile;
}

/** Set a tag to the profile */
MRCP_DECLARE(void) mrcp_client_profile_tag_set(mrcp_client_profile_t *profile, const char *tag)
{
	if(profile) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Profile Tag [%s]",tag);
		profile->tag = tag;
	}
}

/** Register MRCP profile */
MRCP_DECLARE(apt_bool_t) mrcp_client_profile_register(mrcp_client_t *client, mrcp_client_profile_t *profile, const char *name)
{
	if(!profile || !name) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile: no name");
		return FALSE;
	}
	if(!profile->resource_factory) {
		profile->resource_factory = client->resource_factory;
	}
	if(!profile->sa_factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing signaling agent factory",name);
		return FALSE;
	}
	if(mrcp_sa_factory_is_empty(profile->sa_factory) == TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: empty signaling agent factory",name);
		return FALSE;
	}
	if(profile->mrcp_version == MRCP_VERSION_2) {
		if(!profile->ca_factory) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing connection agent factory",name);
			return FALSE;
		}
		if(mrcp_ca_factory_is_empty(profile->ca_factory) == TRUE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: empty connection agent factory",name);
			return FALSE;
		}
	}

	/* mpf_factory may not be specified; but if it is specified, it must not be empty */
	if(profile->mpf_factory) {
		if(mpf_engine_factory_is_empty(profile->mpf_factory) == TRUE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: empty media engine factory",name);
			return FALSE;
		}
	}

	if(!profile->signaling_settings) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Register Profile [%s]: missing signaling settings",name);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Register Profile [%s]",name);
	apr_hash_set(client->profile_table,name,APR_HASH_KEY_STRING,profile);
	profile->name = name;
	return TRUE;
}

/** Get profile by name */
MRCP_DECLARE(mrcp_client_profile_t*) mrcp_client_profile_get(const mrcp_client_t *client, const char *name)
{
	return apr_hash_get(client->profile_table,name,APR_HASH_KEY_STRING);
}

/** Get available profiles */
MRCP_DECLARE(apt_bool_t) mrcp_client_profiles_get(const mrcp_client_t *client, mrcp_client_profile_t *profiles[], apr_size_t *count, const char *tag)
{
	apr_hash_index_t *it;
	void *val;
	mrcp_client_profile_t *profile;
	apr_size_t i = 0;
	apt_bool_t status = TRUE;

	if(!profiles || !count) {
		return FALSE;
	}

	for(it = apr_hash_first(client->pool, client->profile_table); it; it = apr_hash_next(it)) {
		apr_hash_this(it, NULL, NULL, &val);
		if(!val) continue;

		if(i >= *count) {
			status = FALSE;
			break;
		}
		
		profile = val;
		if(!tag || (profile->tag && strcasecmp(tag,profile->tag) == 0)) {
			profiles[i] = profile;
			i++;
		}
	}
	*count = i;
	return status;
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
MRCP_DECLARE(apr_pool_t*) mrcp_client_memory_pool_get(const mrcp_client_t *client)
{
	return client->pool;
}

/** Get directory layout */
MRCP_DECLARE(apt_dir_layout_t*) mrcp_client_dir_layout_get(const mrcp_client_t *client)
{
	return client->dir_layout;
}

mrcp_client_session_t* mrcp_client_session_create_ex(mrcp_client_t *client, apt_bool_t take_ownership, apr_pool_t *pool)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*) mrcp_session_create_ex(pool,take_ownership,sizeof(mrcp_client_session_t)-sizeof(mrcp_session_t));

	session->base.name = apr_psprintf(pool,"0x%pp",session);
	session->base.response_vtable = &session_response_vtable;
	session->base.event_vtable = &session_event_vtable;

	session->application = NULL;
	session->app_obj = NULL;
	session->profile = NULL;
	session->context = NULL;
	session->codec_manager = client->codec_manager;
	session->terminations = apr_array_make(pool,2,sizeof(rtp_termination_slot_t));
	session->channels = apr_array_make(pool,2,sizeof(mrcp_channel_t*));
	session->registered = FALSE;
	session->offer = NULL;
	session->answer = NULL;
	session->active_request = NULL;
	session->request_queue = apt_list_create(pool);
	session->mpf_task_msg = NULL;
	session->subrequest_count = 0;
	session->disconnected = FALSE;
	session->state = SESSION_STATE_NONE;
	session->status = MRCP_SIG_STATUS_CODE_SUCCESS;
	session->attribs = NULL;
	return session;
}

void mrcp_client_session_add(mrcp_client_t *client, mrcp_client_session_t *session)
{
	if(session) {
		apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->base.log_obj,"Add MRCP Handle " APT_NAMESID_FMT,
			session->base.name,
			MRCP_SESSION_SID(&session->base));
		apr_hash_set(client->session_table,session,sizeof(void*),session);
	}
}

void mrcp_client_session_remove(mrcp_client_t *client, mrcp_client_session_t *session)
{
	if(session) {
		apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->base.log_obj,"Remove MRCP Handle " APT_NAMESID_FMT,
			session->base.name,
			MRCP_SESSION_SID(&session->base));
		apr_hash_set(client->session_table,session,sizeof(void*),NULL);
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
			mrcp_client_mpf_message_process(container);
			break;
		}
		case MRCP_CLIENT_APPLICATION_TASK_MSG:
		{
			mrcp_app_message_t **app_message = (mrcp_app_message_t**) msg->data;
			mrcp_client_app_message_process(*app_message);
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

apt_bool_t mrcp_app_signaling_task_msg_signal(mrcp_sig_command_e command_id, mrcp_session_t *session, mrcp_channel_t *channel)
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
		return apt_task_msg_signal(task,task_msg);
	}
	return FALSE;
}

apt_bool_t mrcp_app_control_task_msg_signal(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
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
		return apt_task_msg_signal(task,task_msg);
	}
	return FALSE;
}

static apt_bool_t mrcp_client_signaling_task_msg_signal(sig_agent_task_msg_type_e type, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_message_t *message)
{
	sig_agent_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	if(task_msg) {
		task_msg->type = MRCP_CLIENT_SIGNALING_TASK_MSG;
		task_msg->sub_type = type;
		data = (sig_agent_task_msg_data_t*) task_msg->data;
		data->session = (mrcp_client_session_t*)session;
		data->descriptor = descriptor;
		data->message = message;
		return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
	}
	return FALSE;
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
	if(task_msg) {
		task_msg->type = MRCP_CLIENT_CONNECTION_TASK_MSG;
		task_msg->sub_type = type;
		data = (connection_agent_task_msg_data_t*) task_msg->data;
		data->channel = channel ? channel->obj : NULL;
		data->descriptor = descriptor;
		data->message = message;
		data->status = status;
		return apt_task_msg_signal(task,task_msg);
	}
	return FALSE;
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
