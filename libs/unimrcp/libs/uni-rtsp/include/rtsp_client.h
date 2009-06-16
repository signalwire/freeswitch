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

#ifndef __RTSP_CLIENT_H__
#define __RTSP_CLIENT_H__

/**
 * @file rtsp_client.h
 * @brief RTSP Client
 */ 

#include "apt_task.h"
#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP client declaration */
typedef struct rtsp_client_t rtsp_client_t;
/** Opaque RTSP client session declaration */
typedef struct rtsp_client_session_t rtsp_client_session_t;

/** RTSP client vtable declaration */
typedef struct rtsp_client_vtable_t rtsp_client_vtable_t;

/** RTSP client vtable */
struct rtsp_client_vtable_t {
	/** Sesssion termination response handler */
	apt_bool_t (*on_session_terminate_response)(rtsp_client_t *client, rtsp_client_session_t *session);
	/** Sesssion termination event handler */
	apt_bool_t (*on_session_terminate_event)(rtsp_client_t *client, rtsp_client_session_t *session);
	/** Sesssion setup response handler */
	apt_bool_t (*on_session_response)(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response);
	/** Sesssion event handler */
	apt_bool_t (*on_session_event)(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);
};

/**
 * Create RTSP client.
 * @param max_connection_count the number of max RTSP connections
 * @param obj the external object to send event to
 * @param handler the response/event handler
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_client_t*) rtsp_client_create(
								apr_size_t max_connection_count,
								void *obj,
								const rtsp_client_vtable_t *handler,
								apr_pool_t *pool);

/**
 * Destroy RTSP client.
 * @param client the client to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_destroy(rtsp_client_t *client);

/**
 * Start client and wait for incoming requests.
 * @param client the client to start
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_start(rtsp_client_t *client);

/**
 * Terminate client.
 * @param client the client to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_terminate(rtsp_client_t *client);

/**
 * Get task.
 * @param client the client to get task from
 */
RTSP_DECLARE(apt_task_t*) rtsp_client_task_get(rtsp_client_t *client);

/**
 * Get external object.
 * @param client the client to get object from
 */
RTSP_DECLARE(void*) rtsp_client_object_get(rtsp_client_t *client);


/**
 * Create RTSP session.
 * @param client the client to create session for
 * @param server_ip the IP address of RTSP server
 * @param server_port the port of RTSP server
 * @param resource_location the location of RTSP resource (path in RTSP URI)
 */
RTSP_DECLARE(rtsp_client_session_t*) rtsp_client_session_create(
											rtsp_client_t *client, 
											const char *server_ip, 
											apr_port_t server_port,
											const char *resource_location);

/**
 * Destroy RTSP session.
 * @param session the session to destroy
 */
RTSP_DECLARE(void) rtsp_client_session_destroy(rtsp_client_session_t *session);

/**
 * Terminate RTSP session.
 * @param client the client to use
 * @param session the session to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_session_terminate(rtsp_client_t *client, rtsp_client_session_t *session);

/**
 * Send RTSP message.
 * @param client the client to use
 * @param session the session to send RTSP request for
 * @param message the RTSP request to send
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_session_request(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);

/**
 * Get object associated with the session.
 * @param session the session to get object from
 */
RTSP_DECLARE(void*) rtsp_client_session_object_get(const rtsp_client_session_t *session);

/**
 * Set object associated with the session.
 * @param session the session to set object for
 * @param obj the object to set
 */
RTSP_DECLARE(void) rtsp_client_session_object_set(rtsp_client_session_t *session, void *obj);

/**
 * Get the session identifier.
 * @param session the session to get identifier from
 */
RTSP_DECLARE(const apt_str_t*) rtsp_client_session_id_get(const rtsp_client_session_t *session);

APT_END_EXTERN_C

#endif /*__RTSP_CLIENT_H__*/
