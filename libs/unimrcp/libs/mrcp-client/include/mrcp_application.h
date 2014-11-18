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
 * $Id: mrcp_application.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_APPLICATION_H
#define MRCP_APPLICATION_H

/**
 * @file mrcp_application.h
 * @brief MRCP User Level Application Interface
 */ 

#include "mrcp_client_types.h"
#include "mpf_rtp_descriptor.h"
#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** MRCP application message declaration */
typedef struct mrcp_app_message_t mrcp_app_message_t;

/** MRCP signaling message declaration */
typedef struct mrcp_sig_message_t mrcp_sig_message_t;

/** MRCP application message dispatcher declaration */
typedef struct mrcp_app_message_dispatcher_t mrcp_app_message_dispatcher_t;

/** MRCP application message handler */
typedef apt_bool_t (*mrcp_app_message_handler_f)(const mrcp_app_message_t *app_message);

/** Enumeration of MRCP signaling message types */
typedef enum {
	MRCP_SIG_MESSAGE_TYPE_REQUEST,  /**< request message */
	MRCP_SIG_MESSAGE_TYPE_RESPONSE, /**< response message */
	MRCP_SIG_MESSAGE_TYPE_EVENT     /**< event message */
} mrcp_sig_message_type_e;

/** Enumeration of MRCP signaling status codes */
typedef enum {
	MRCP_SIG_STATUS_CODE_SUCCESS,   /**< indicates success */
	MRCP_SIG_STATUS_CODE_FAILURE,   /**< request failed */
	MRCP_SIG_STATUS_CODE_TERMINATE, /**< request failed, session/channel/connection unexpectedly terminated */
	MRCP_SIG_STATUS_CODE_CANCEL     /**< request cancelled */
} mrcp_sig_status_code_e;


/** Enumeration of MRCP signaling commands (requests/responses) */
typedef enum {
	MRCP_SIG_COMMAND_SESSION_UPDATE,
	MRCP_SIG_COMMAND_SESSION_TERMINATE,
	MRCP_SIG_COMMAND_CHANNEL_ADD,
	MRCP_SIG_COMMAND_CHANNEL_REMOVE,
	MRCP_SIG_COMMAND_RESOURCE_DISCOVER
} mrcp_sig_command_e;

/** Enumeration of MRCP signaling events */
typedef enum {
	MRCP_SIG_EVENT_TERMINATE
} mrcp_sig_event_e;


/** Enumeration of MRCP application message types */
typedef enum {
	MRCP_APP_MESSAGE_TYPE_SIGNALING, /**< signaling message type */
	MRCP_APP_MESSAGE_TYPE_CONTROL    /**< control message type */
} mrcp_app_message_type_e;

/** MRCP signaling message definition */
struct mrcp_sig_message_t {
	/** Message type (request/response/event) */
	mrcp_sig_message_type_e message_type;
	/** Command (request/response) identifier */
	mrcp_sig_command_e      command_id;
	/** Event identifier */
	mrcp_sig_event_e        event_id;
	/** Status code used in response */
	mrcp_sig_status_code_e  status;
};


/** MRCP application message definition */
struct mrcp_app_message_t {
	/** Message type (signaling/control) */
	mrcp_app_message_type_e message_type;

	/** Application */
	mrcp_application_t        *application;
	/** Session */
	mrcp_session_t            *session;
	/** Channel */
	mrcp_channel_t            *channel;
	/** Session/resource descriptor */
	mrcp_session_descriptor_t *descriptor;

	/** MRCP signaling message (used if message_type == MRCP_APP_MESSAGE_SIGNALING) */
	mrcp_sig_message_t         sig_message;
	/** MRCP control message (used if message_type == MRCP_APP_MESSAGE_CONTROL) */
	mrcp_message_t            *control_message;
};

/** MRCP application message dispatcher interface */
struct mrcp_app_message_dispatcher_t {
	/** Response to mrcp_application_session_update()request */
	apt_bool_t (*on_session_update)(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
	/** Response to mrcp_application_session_terminate()request */
	apt_bool_t (*on_session_terminate)(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
	
	/** Response to mrcp_application_channel_add() request */
	apt_bool_t (*on_channel_add)(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
	/** Response to mrcp_application_channel_remove() request */
	apt_bool_t (*on_channel_remove)(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);

	/** Response (event) to mrcp_application_message_send() request */
	apt_bool_t (*on_message_receive)(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

	/** Event indicating unexpected session/channel termination */
	apt_bool_t (*on_terminate_event)(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel);

	/** Response to mrcp_application_resource_discover() request */
	apt_bool_t (*on_resource_discover)(mrcp_application_t *application, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_sig_status_code_e status);
};



/**
 * Create application instance.
 * @param handler the event handler
 * @param obj the external object
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(const mrcp_app_message_handler_f handler, void *obj, apr_pool_t *pool);

/**
 * Destroy application instance.
 * @param application the application to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_destroy(mrcp_application_t *application);

/**
 * Get external object associated with the application.
 * @param application the application to get object from
 */
MRCP_DECLARE(void*) mrcp_application_object_get(const mrcp_application_t *application);

/**
 * Get dir layout structure.
 * @param application the application to get dir layout from
 */
MRCP_DECLARE(const apt_dir_layout_t*) mrcp_application_dir_layout_get(const mrcp_application_t *application);

/**
 * Create session.
 * @param application the entire application
 * @param profile the name of the profile to use
 * @param obj the external object
 * @return the created session instance
 */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, const char *profile, void *obj);

/**
 * Get memory pool the session object is created out of.
 * @param session the session to get pool from
 */
MRCP_DECLARE(apr_pool_t*) mrcp_application_session_pool_get(const mrcp_session_t *session);

/**
 * Get session identifier.
 * @param session the session to get identifier of
 */
MRCP_DECLARE(const apt_str_t*) mrcp_application_session_id_get(const mrcp_session_t *session);

/**
 * Get SIP or RTSP response code.
 * @param session the session to use
 */
MRCP_DECLARE(int) mrcp_application_session_response_code_get(const mrcp_session_t *session);

/**
 * Get external object associated with the session.
 * @param session the session to get object from
 */
MRCP_DECLARE(void*) mrcp_application_session_object_get(const mrcp_session_t *session);

/**
 * Set (associate) external object to the session.
 * @param session the session to set object for
 * @param obj the object to set
 */
MRCP_DECLARE(void) mrcp_application_session_object_set(mrcp_session_t *session, void *obj);

/**
 * Set name of the session (informative only used for debugging).
 * @param session the session to set name for
 * @param name the name to set
 */
MRCP_DECLARE(void) mrcp_application_session_name_set(mrcp_session_t *session, const char *name);

/** 
 * Send session update request.
 * @param session the session to update
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_update(mrcp_session_t *session);

/** 
 * Send session termination request.
 * @param session the session to terminate
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_terminate(mrcp_session_t *session);

/** 
 * Destroy client session (session must be terminated prior to destroy).
 * @param session the session to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_destroy(mrcp_session_t *session);


/** 
 * Create control channel.
 * @param session the session to create channel for
 * @param resource_id the resource identifier of the channel
 * @param termination the media termination
 * @param rtp_descriptor the RTP termination descriptor (NULL by default)
 * @param obj the external object
 */
MRCP_DECLARE(mrcp_channel_t*) mrcp_application_channel_create(
									mrcp_session_t *session, 
									mrcp_resource_id resource_id, 
									mpf_termination_t *termination, 
									mpf_rtp_termination_descriptor_t *rtp_descriptor, 
									void *obj);

/**
 * Get external object associated with the channel.
 * @param channel the channel to get object from
 */
MRCP_DECLARE(void*) mrcp_application_channel_object_get(const mrcp_channel_t *channel);

/**
 * Get RTP termination descriptor.
 * @param channel the channel to get descriptor from
 */
MRCP_DECLARE(mpf_rtp_termination_descriptor_t*) mrcp_application_rtp_descriptor_get(const mrcp_channel_t *channel);

/**
 * Get codec descriptor of source stream.
 * @param channel the channel to get descriptor from
 */
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_source_descriptor_get(const mrcp_channel_t *channel);

/**
 * Get codec descriptor of sink stream.
 * @param channel the channel to get descriptor from
 */
MRCP_DECLARE(const mpf_codec_descriptor_t*) mrcp_application_sink_descriptor_get(const mrcp_channel_t *channel);

/**
 * Get associated audio stream.
 * @param channel the channel to get associated stream from
 */
MRCP_DECLARE(const mpf_audio_stream_t*) mrcp_application_audio_stream_get(const mrcp_channel_t *channel);

/** 
 * Send channel add request.
 * @param session the session to create channel for
 * @param channel the control channel
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_add(mrcp_session_t *session, mrcp_channel_t *channel);

/** 
 * Create MRCP message.
 * @param session the session
 * @param channel the control channel
 * @param method_id the method identifier of MRCP message
 */
MRCP_DECLARE(mrcp_message_t*) mrcp_application_message_create(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_method_id method_id);

/** 
 * Send MRCP message.
 * @param session the session
 * @param channel the control channel
 * @param message the MRCP message to send
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_send(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

/** 
 * Remove channel.
 * @param session the session to remove channel from
 * @param channel the control channel to remove
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_remove(mrcp_session_t *session, mrcp_channel_t *channel);

/** 
 * Discover resources.
 * @param session the session to use as communication object
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_resource_discover(mrcp_session_t *session);

/**
 * Dispatch application message.
 * @param dispatcher the dispatcher inteface
 * @param app_message the message to dispatch
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_dispatch(const mrcp_app_message_dispatcher_t *dispatcher, const mrcp_app_message_t *app_message);

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
										void *obj);
/** 
 * Create source media termination
 * @param session the session to create termination for
 * @param stream_vtable the virtual table of audio stream
 * @param codec_descriptor the descriptor of audio stream (NULL by default)
 * @param obj the external object
 * @deprecated @see mrcp_application_audio_termination_create()
 */
MRCP_DECLARE(mpf_termination_t*) mrcp_application_source_termination_create(
										mrcp_session_t *session,
										const mpf_audio_stream_vtable_t *stream_vtable,
										mpf_codec_descriptor_t *codec_descriptor,
										void *obj);
/** 
 * Create sink media termination
 * @param session the session to create termination for
 * @param stream_vtable the virtual table of audio stream
 * @param codec_descriptor the descriptor of audio stream (NULL by default)
 * @param obj the external object
 * @deprecated @see mrcp_application_audio_termination_create()
 */
MRCP_DECLARE(mpf_termination_t*) mrcp_application_sink_termination_create(
										mrcp_session_t *session,
										const mpf_audio_stream_vtable_t *stream_vtable,
										mpf_codec_descriptor_t *codec_descriptor,
										void *obj);

APT_END_EXTERN_C

#endif /* MRCP_APPLICATION_H */
