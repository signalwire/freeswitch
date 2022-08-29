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

#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

/**
 * @file rtsp_server.h
 * @brief RTSP Server
 */ 

#include "apt_task.h"
#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP server declaration */
typedef struct rtsp_server_t rtsp_server_t;
/** Opaque RTSP server session declaration */
typedef struct rtsp_server_session_t rtsp_server_session_t;

/** RTSP server vtable declaration */
typedef struct rtsp_server_vtable_t rtsp_server_vtable_t;

/** RTSP server vtable declaration */
struct rtsp_server_vtable_t {
	/** Virtual create session */
	apt_bool_t (*create_session)(rtsp_server_t *server, rtsp_server_session_t *session);
	/** Virtual terminate session */
	apt_bool_t (*terminate_session)(rtsp_server_t *server, rtsp_server_session_t *session);
	/** Virtual message handler */
	apt_bool_t (*handle_message)(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message);
};

/**
 * Create RTSP server.
 * @param id the identifier of the server
 * @param listen_ip the listen IP address
 * @param listen_port the listen port
 * @param max_connection_count the number of max RTSP connections
 * @param connection_timeout the inactivity timeout for an RTSP connection [sec]
 * @param obj the external object to send events to
 * @param handler the request handler
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_server_t*) rtsp_server_create(
									const char *id,
									const char *listen_ip,
									apr_port_t listen_port,
									apr_size_t max_connection_count,
									apr_size_t connection_timeout,
									void *obj,
									const rtsp_server_vtable_t *handler,
									apr_pool_t *pool);

/**
 * Destroy RTSP server.
 * @param server the server to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_destroy(rtsp_server_t *server);

/**
 * Start server and wait for incoming requests.
 * @param server the server to start
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_start(rtsp_server_t *server);

/**
 * Terminate server.
 * @param server the server to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_terminate(rtsp_server_t *server);

/**
 * Get task.
 * @param server the server to get task from
 */
RTSP_DECLARE(apt_task_t*) rtsp_server_task_get(const rtsp_server_t *server);

/**
 * Get external object.
 * @param server the server to get object from
 */
RTSP_DECLARE(void*) rtsp_server_object_get(const rtsp_server_t *server);

/**
 * Send RTSP message.
 * @param server the server to use
 * @param session the session to send RTSP response for
 * @param message the RTSP response to send
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_session_respond(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message);

/**
 * Terminate RTSP session (respond to terminate request).
 * @param server the server to use
 * @param session the session to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_session_terminate(rtsp_server_t *server, rtsp_server_session_t *session);

/**
 * Release RTSP session (internal release event/request).
 * @param server the server to use
 * @param session the session to release
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_session_release(rtsp_server_t *server, rtsp_server_session_t *session);

/**
 * Get object associated with the session.
 * @param session the session to get object from
 */
RTSP_DECLARE(void*) rtsp_server_session_object_get(const rtsp_server_session_t *session);

/**
 * Set object associated with the session.
 * @param session the session to set object for
 * @param obj the object to set
 */
RTSP_DECLARE(void) rtsp_server_session_object_set(rtsp_server_session_t *session, void *obj);

/**
 * Get the session identifier.
 * @param session the session to get identifier from
 */
RTSP_DECLARE(const apt_str_t*) rtsp_server_session_id_get(const rtsp_server_session_t *session);

/**
 * Get active (in-progress) session request.
 * @param session the session to get from
 */
RTSP_DECLARE(const rtsp_message_t*) rtsp_server_session_request_get(const rtsp_server_session_t *session);

/**
 * Get the session destination (client) IP address.
 * @param session the session to get IP address from
 */
RTSP_DECLARE(const char*) rtsp_server_session_destination_get(const rtsp_server_session_t *session);

APT_END_EXTERN_C

#endif /* RTSP_SERVER_H */
