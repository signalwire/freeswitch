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

#include "mrcp_application.h"
#include "mrcp_client.h"
#include "mrcp_client_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_message.h"
#include "mrcp_sig_agent.h"
#include "mrcp_resource_factory.h"
#include "mpf_termination_factory.h"
#include "apt_dir_layout.h"
#include "apt_pool.h"
#include "apt_log.h"

mrcp_client_session_t* mrcp_client_session_create_ex(mrcp_client_t *client, apt_bool_t take_ownership, apr_pool_t *pool);

apt_bool_t mrcp_app_signaling_task_msg_signal(mrcp_sig_command_e command_id, mrcp_session_t *session, mrcp_channel_t *channel);
apt_bool_t mrcp_app_control_task_msg_signal(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);


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
MRCP_DECLARE(void*) mrcp_application_object_get(const mrcp_application_t *application)
{
	return application->obj;
}

/** Get dir layout structure */
MRCP_DECLARE(const apt_dir_layout_t*) mrcp_application_dir_layout_get(const mrcp_application_t *application)
{
	return mrcp_client_dir_layout_get(application->client);
}



/** Create client session */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, const char *profile_name, void *obj)
{
	mrcp_session_t *session;
	apr_pool_t *pool;
	pool = apt_pool_create();
	if(!pool) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Memory Pool");
		return NULL;
	}
	session = mrcp_application_session_create_ex(application,profile_name,obj,TRUE,pool);
	if(!session) {
		apr_pool_destroy(pool);
	}
	return session;
}

/** Create session using the provided memory pool */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create_ex(
								mrcp_application_t *application,
								const char *profile_name,
								void *obj, 
								apt_bool_t take_ownership,
								apr_pool_t *pool)
{
	mrcp_client_profile_t *profile;
	mrcp_client_session_t *session;
	if(!application || !application->client || !profile_name) {
		return NULL;
	}

	profile = mrcp_client_profile_get(application->client,profile_name);
	if(!profile) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such Profile [%s]",profile_name);
		return NULL;
	}

	session = mrcp_client_session_create_ex(application->client,take_ownership,pool);
	if(!session) {
		return NULL;
	}
	session->application = application;
	session->app_obj = obj;
	session->base.log_obj = obj;
	session->profile = profile;
	
	apt_obj_log(APT_LOG_MARK,APT_PRIO_NOTICE,session->base.log_obj,"Create MRCP Handle " APT_PTR_FMT " [%s]",
		MRCP_SESSION_PTR(session),
		profile_name);
	return &session->base;
}

/** Get memory pool the session object is created out of */
MRCP_DECLARE(apr_pool_t*) mrcp_application_session_pool_get(const mrcp_session_t *session)
{
	if(!session) {
		return NULL;
	}
	return session->pool;
}

/** Get session identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_application_session_id_get(const mrcp_session_t *session)
{
	if(!session) {
		return NULL;
	}
	return &session->id;
}

/** Get SIP or RTSP response code */
MRCP_DECLARE(int) mrcp_application_session_response_code_get(const mrcp_session_t *session)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(!client_session || !client_session->answer) {
		return 0;
	}
	return client_session->answer->response_code;
}

/** Get external object associated with the session */
MRCP_DECLARE(void*) mrcp_application_session_object_get(const mrcp_session_t *session)
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

/** Set name of the session (informative only used for debugging) */
MRCP_DECLARE(void) mrcp_application_session_name_set(mrcp_session_t *session, const char *name)
{
	if(session && name) {
		session->name = apr_pstrdup(session->pool,name);
	}
}

/** Set session attributes */
MRCP_DECLARE(void) mrcp_application_session_attribs_set(mrcp_session_t *session, mrcp_session_attribs_t *attribs)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	if(client_session) {
		client_session->attribs = attribs;
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
	apt_obj_log(APT_LOG_MARK,APT_PRIO_NOTICE,session->log_obj,"Destroy MRCP Handle %s",session->name);
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
	mrcp_client_profile_t *profile;
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
		if(!profile->mpf_factory || !profile->rtp_termination_factory) {
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

	return mrcp_client_channel_create(client_session,resource,termination,rtp_descriptor,obj);
}

/** Get external object associated with the channel */
MRCP_DECLARE(void*) mrcp_application_channel_object_get(const mrcp_channel_t *channel)
{
	if(!channel) {
		return FALSE;
	}
	return channel->obj;
}

/** Get RTP termination descriptor */
MRCP_DECLARE(mpf_rtp_termination_descriptor_t*) mrcp_application_rtp_descriptor_get(const mrcp_channel_t *channel)
{
	if(!channel || !channel->rtp_termination_slot) {
		return NULL;
	}
	return channel->rtp_termination_slot->descriptor;
}

/** Get codec descriptor of source stream */
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_source_descriptor_get(const mrcp_channel_t *channel)
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
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_sink_descriptor_get(const mrcp_channel_t *channel)
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

/** Get associated audio stream */
MRCP_DECLARE(const mpf_audio_stream_t*) mrcp_application_audio_stream_get(const mrcp_channel_t *channel)
{
	if(!channel || !channel->termination) {
		return NULL;
	}

	return mpf_termination_audio_stream_get(channel->termination);
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
	mrcp_client_profile_t *profile;
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
						profile->mrcp_version,
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

/** Create audio termination */
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

/** Create sink media termination */
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

/** Dispatch application message */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_dispatch(const mrcp_app_message_dispatcher_t *dispatcher, const mrcp_app_message_t *app_message)
{
	apt_bool_t status = FALSE;
	switch(app_message->message_type) {
		case MRCP_APP_MESSAGE_TYPE_SIGNALING:
		{
			if(app_message->sig_message.message_type == MRCP_SIG_MESSAGE_TYPE_RESPONSE) {
				switch(app_message->sig_message.command_id) {
					case MRCP_SIG_COMMAND_SESSION_UPDATE:
						if(dispatcher->on_session_update) {
							status = dispatcher->on_session_update(
										app_message->application,
										app_message->session,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_SESSION_TERMINATE:
						if(dispatcher->on_session_terminate) {
							status = dispatcher->on_session_terminate(
										app_message->application,
										app_message->session,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_CHANNEL_ADD:
						if(dispatcher->on_channel_add) {
							status = dispatcher->on_channel_add(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_CHANNEL_REMOVE:
						if(dispatcher->on_channel_remove) {
							status = dispatcher->on_channel_remove(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_RESOURCE_DISCOVER:
						if(dispatcher->on_resource_discover) {
							status = dispatcher->on_resource_discover(
										app_message->application,
										app_message->session,
										app_message->descriptor,
										app_message->sig_message.status);
						}
						break;
					default:
						break;
				}
			}
			else if(app_message->sig_message.message_type == MRCP_SIG_MESSAGE_TYPE_EVENT) {
				switch(app_message->sig_message.event_id) {
					case MRCP_SIG_EVENT_TERMINATE:
						if(dispatcher->on_terminate_event) {
							status = dispatcher->on_terminate_event(
										app_message->application,
										app_message->session,
										app_message->channel);
						}
						break;
					default:
						break;
				}
			}
			break;
		}
		case MRCP_APP_MESSAGE_TYPE_CONTROL:
		{
			if(dispatcher->on_message_receive) {
				status = dispatcher->on_message_receive(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->control_message);
			}
			break;
		}
	}
	return status;
}
