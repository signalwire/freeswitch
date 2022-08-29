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

#ifndef MRCP_SERVER_SESSION_H
#define MRCP_SERVER_SESSION_H

/**
 * @file mrcp_server_session.h
 * @brief MRCP Server Session
 */ 

#include <apr_hash.h>
#include "mrcp_session.h"
#include "mpf_engine.h"
#include "apt_task.h"
#include "apt_obj_list.h"


APT_BEGIN_EXTERN_C

/** Opaque MRCP channel declaration */
typedef struct mrcp_channel_t mrcp_channel_t;
/** MRCP server session declaration */
typedef struct mrcp_server_session_t mrcp_server_session_t;
/** MRCP signaling message declaration */
typedef struct mrcp_signaling_message_t mrcp_signaling_message_t;

/** Enumeration of signaling task messages */
typedef enum {
	SIGNALING_MESSAGE_OFFER,
	SIGNALING_MESSAGE_CONTROL,
	SIGNALING_MESSAGE_TERMINATE,
} mrcp_signaling_message_type_e;

/** MRCP signaling message */
struct mrcp_signaling_message_t {
	/** Signaling message type */
	mrcp_signaling_message_type_e type;

	/** Session */
	mrcp_server_session_t        *session;
	/** Descriptor */
	mrcp_session_descriptor_t    *descriptor;

	/** Channel */
	mrcp_channel_t               *channel;
	/** MRCP message */
	mrcp_message_t               *message;
};

/** Server session states */
typedef enum {
	SESSION_STATE_NONE,              /**< initial state */
	SESSION_STATE_GENERATING_ANSWER, /**< received offer, generating answer now */
	SESSION_STATE_INITIALIZING,      /**< answer is ready, finally initializing channels now */
	SESSION_STATE_DEACTIVATING,      /**< received session termination request, deinitializing channels now */
	SESSION_STATE_TERMINATING        /**< finally terminating session */
} mrcp_server_session_state_e;


/** MRCP server session */
struct mrcp_server_session_t {
	/** Session base */
	mrcp_session_t              base;
	/** MRCP server */
	mrcp_server_t              *server;
	/** MRCP profile */
	mrcp_server_profile_t      *profile;

	/** Media context */
	mpf_context_t              *context;

	/** Media termination array */
	apr_array_header_t         *terminations;
	/** MRCP control channel array */
	apr_array_header_t         *channels;

	/** In-progress signaling request */
	mrcp_signaling_message_t   *active_request;
	/** Signaling request queue */
	apt_obj_list_t             *request_queue;

	/** In-progress offer */
	mrcp_session_descriptor_t  *offer;
	/** In-progres answer */
	mrcp_session_descriptor_t  *answer;

	/** Last offer received */
	mrcp_session_descriptor_t  *last_offer;
	/** Last answer sent */
	mrcp_session_descriptor_t  *last_answer;

	/** MPF task message, which construction is in progress */
	mpf_task_msg_t             *mpf_task_msg;

	/** Session state */
	mrcp_server_session_state_e state;
	/** Number of in-progress sub requests */
	apr_size_t                  subrequest_count;
};

/** MRCP server profile */
struct mrcp_server_profile_t {
	/** Identifier of the profile */
	const char                *id;
	/** MRCP version */
	mrcp_version_e             mrcp_version;
	/** Table of engines (mrcp_engine_settings_t*) */
	apr_hash_t                *engine_table;
	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** Media processing engine */
	mpf_engine_t              *media_engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** RTP settings */
	mpf_rtp_settings_t        *rtp_settings;
	/** Signaling agent */
	mrcp_sig_agent_t          *signaling_agent;
	/** Connection agent */
	mrcp_connection_agent_t   *connection_agent;
};

/** Create server session */
mrcp_server_session_t* mrcp_server_session_create(void);

/** Process signaling message */
apt_bool_t mrcp_server_signaling_message_process(mrcp_signaling_message_t *signaling_message);
/** Process MPF message */
apt_bool_t mrcp_server_mpf_message_process(mpf_message_container_t *mpf_message_container);

/** Process channel modify event */
apt_bool_t mrcp_server_on_channel_modify(mrcp_channel_t *channel, mrcp_control_descriptor_t *answer, apt_bool_t status);
/** Process channel remove event */
apt_bool_t mrcp_server_on_channel_remove(mrcp_channel_t *channel, apt_bool_t status);
/** Process channel message receive */
apt_bool_t mrcp_server_on_channel_message(mrcp_channel_t *channel, mrcp_message_t *message);
/** Process connection disconnect event */
apt_bool_t mrcp_server_on_disconnect(mrcp_channel_t *channel);

/** Process channel open event */
apt_bool_t mrcp_server_on_engine_channel_open(mrcp_channel_t *channel, apt_bool_t status);
/** Process channel close event */
apt_bool_t mrcp_server_on_engine_channel_close(mrcp_channel_t *channel);
/** Process message receive event */
apt_bool_t mrcp_server_on_engine_channel_message(mrcp_channel_t *channel, mrcp_message_t *message);

/** Get session by channel */
mrcp_session_t* mrcp_server_channel_session_get(mrcp_channel_t *channel);

APT_END_EXTERN_C

#endif /* MRCP_SERVER_SESSION_H */
