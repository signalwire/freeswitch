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

#ifndef __MRCP_CLIENT_SESSION_H__
#define __MRCP_CLIENT_SESSION_H__

/**
 * @file mrcp_client_session.h
 * @brief MRCP Client Session
 */ 

#include "mrcp_client_types.h"
#include "mrcp_application.h"
#include "mrcp_session.h"
#include "mpf_engine.h"
#include "apt_task_msg.h"
#include "apt_obj_list.h"

APT_BEGIN_EXTERN_C

/** RTP termination slot declaration */
typedef struct rtp_termination_slot_t rtp_termination_slot_t;

/** MRCP client session declaration */
typedef struct mrcp_client_session_t mrcp_client_session_t;

/** Client session states */
typedef enum {
	SESSION_STATE_NONE,
	SESSION_STATE_GENERATING_OFFER,
	SESSION_STATE_PROCESSING_ANSWER,
	SESSION_STATE_TERMINATING,
	SESSION_STATE_DISCOVERING
} mrcp_client_session_state_e;

/** MRCP client session */
struct mrcp_client_session_t {
	/** Session base */
	mrcp_session_t              base;
	/** Application session belongs to */
	mrcp_application_t         *application;
	/** External object associated with session */
	void                       *app_obj;
	/** Profile to use */
	mrcp_profile_t             *profile;

	/** Media context */
	mpf_context_t              *context;
	/** Codec manager */
	const mpf_codec_manager_t  *codec_manager;


	/** RTP termination array (mrcp_termination_slot_t) */
	apr_array_header_t         *terminations;
	/** MRCP control channel array (mrcp_channel_t*) */
	apr_array_header_t         *channels;

	/** Indicates whether session is already added to session table */
	apt_bool_t                  registered;

	/** In-progress offer */
	mrcp_session_descriptor_t  *offer;
	/** In-progress answer */
	mrcp_session_descriptor_t  *answer;

	/** MRCP application active request */
	const mrcp_app_message_t   *active_request;
	/** MRCP application request queue */
	apt_obj_list_t             *request_queue;

	/** MPF task message, which construction is in progress */
	mpf_task_msg_t             *mpf_task_msg;

	/** Session state */
	mrcp_client_session_state_e state;
	/** Status code of the app response to be generated */
	mrcp_sig_status_code_e      status;
	/** Number of in-progress sub requests */
	apr_size_t                  subrequest_count;
};

/** MRCP channel */
struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t             *pool;
	/** External object associated with channel */
	void                   *obj;
	/** MRCP resource */
	mrcp_resource_t        *resource;
	/** MRCP session entire channel belongs to */
	mrcp_session_t         *session;
	/** MRCP control channel */
	mrcp_control_channel_t *control_channel;
	/** Media termination */
	mpf_termination_t      *termination;
	/** Associated RTP termination slot */
	rtp_termination_slot_t *rtp_termination_slot;

	/** waiting state of control channel */
	apt_bool_t              waiting_for_channel;         
	/** waiting state of media termination */
	apt_bool_t              waiting_for_termination;
};

/** RTP termination slot */
struct rtp_termination_slot_t {
	/** waiting state */
	apt_bool_t                        waiting;
	/** RTP termination */
	mpf_termination_t                *termination;
	/** RTP termination descriptor */
	mpf_rtp_termination_descriptor_t *descriptor;
	/** Associated MRCP channel */
	mrcp_channel_t                   *channel;
	/** media descriptor id (index of media in session descriptor) */
	apr_size_t                        id;
};


/** MRCP profile */
struct mrcp_profile_t {
	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** Media processing engine */
	mpf_engine_t              *media_engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** Signaling agent */
	mrcp_sig_agent_t          *signaling_agent;
	/** Connection agent */
	mrcp_connection_agent_t   *connection_agent;
};

/** MRCP application */
struct mrcp_application_t {
	/** External object associated with the application */
	void                      *obj;
	/** Application message handler */
	mrcp_app_message_handler_f handler;
	/** MRCP client */
	mrcp_client_t             *client;
	/** Application task message pool */
	apt_task_msg_pool_t       *msg_pool;
};

/** Create client session */
mrcp_client_session_t* mrcp_client_session_create(mrcp_application_t *application, void *obj);
/** Create channel */
mrcp_channel_t* mrcp_client_channel_create(
					mrcp_session_t *session, 
					mrcp_resource_t *resource, 
					mpf_termination_t *termination, 
					mpf_rtp_termination_descriptor_t *rtp_descriptor, 
					void *obj);

/** Create signaling app_message_t request */
mrcp_app_message_t* mrcp_client_app_signaling_request_create(mrcp_sig_command_e command_id, apr_pool_t *pool);
/** Create signaling app_message_t event */
mrcp_app_message_t* mrcp_client_app_signaling_event_create(mrcp_sig_event_e event_id, apr_pool_t *pool);
/** Create control app_message_t */
mrcp_app_message_t* mrcp_client_app_control_message_create(apr_pool_t *pool);

/** Process application message */
apt_bool_t mrcp_client_app_message_process(mrcp_app_message_t *app_message);
/** Process MPF message */
apt_bool_t mrcp_client_mpf_message_process(mpf_message_container_t *mpf_message_container);

/** Process session answer */
apt_bool_t mrcp_client_session_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
/** Process session termination response */
apt_bool_t mrcp_client_session_terminate_response_process(mrcp_client_session_t *session);
/** Process session control response */
apt_bool_t mrcp_client_session_control_response_process(mrcp_client_session_t *session, mrcp_message_t *message);
/** Process resource discovery response */
apt_bool_t mrcp_client_session_discover_response_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
/** Process session termination event */
apt_bool_t mrcp_client_session_terminate_event_process(mrcp_client_session_t *session);

/** Process channel add event */
apt_bool_t mrcp_client_on_channel_add(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
/** Process channel modify event */
apt_bool_t mrcp_client_on_channel_modify(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
/** Process channel remove event */
apt_bool_t mrcp_client_on_channel_remove(mrcp_channel_t *channel, apt_bool_t status);
/** Process message receive event */
apt_bool_t mrcp_client_on_message_receive(mrcp_channel_t *channel, mrcp_message_t *message);
/** Process disconnect event */
apt_bool_t mrcp_client_on_disconnect(mrcp_channel_t *channel);

APT_END_EXTERN_C

#endif /*__MRCP_CLIENT_SESSION_H__*/
