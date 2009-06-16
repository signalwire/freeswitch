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

#ifndef __APT_NET_SERVER_TASK_H__
#define __APT_NET_SERVER_TASK_H__

/**
 * @file apt_net_server_task.h
 * @brief Network Server Task Base
 */ 

#include <apr_poll.h>
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Opaque network server task declaration */
typedef struct apt_net_server_task_t apt_net_server_task_t;
/** Network server connection declaration */
typedef struct apt_net_server_connection_t apt_net_server_connection_t;
/** Virtual table of network server events */
typedef struct apt_net_server_vtable_t apt_net_server_vtable_t;

/** Network server connection */
struct apt_net_server_connection_t {
	/** Memory pool */
	apr_pool_t   *pool;
	/** External object */
	void         *obj;
	/** Client IP address */
	char         *client_ip;
	/** Accepted socket */
	apr_socket_t *sock;
	/** Socket poll descriptor */
	apr_pollfd_t  sock_pfd;
	/** String identifier used for traces */
	const char   *id;
};

/** Virtual table of network server events */
struct apt_net_server_vtable_t {
	/** Connect event handler */
	apt_bool_t (*on_connect)(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
	/** Disconnect event handler */
	apt_bool_t (*on_disconnect)(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
	/** Message receive handler */
	apt_bool_t (*on_receive)(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
};


/**
 * Create network server task.
 * @param listen_ip the listen IP address
 * @param listen_port the listen port
 * @param max_connection_count the number of max connections to accept
 * @param obj the external object
 * @param server_vtable the table of virtual methods of the net server task
 * @param msg_pool the pool of task messages
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_net_server_task_t*) apt_net_server_task_create(
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_size_t max_connection_count,
										void *obj,
										const apt_net_server_vtable_t *server_vtable,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool);

/**
 * Destroy network server task.
 * @param task the task to destroy
 */
APT_DECLARE(apt_bool_t) apt_net_server_task_destroy(apt_net_server_task_t *task);

/**
 * Start network server task and wait for incoming requests.
 * @param task the task to start
 */
APT_DECLARE(apt_bool_t) apt_net_server_task_start(apt_net_server_task_t *task);

/**
 * Terminate connection task.
 * @param task the task to terminate
 */
APT_DECLARE(apt_bool_t) apt_net_server_task_terminate(apt_net_server_task_t *task);

/**
 * Get task base.
 * @param task the network server task to get task base from
 */
APT_DECLARE(apt_task_t*) apt_net_server_task_base_get(apt_net_server_task_t *task);

/**
 * Get task vtable.
 * @param task the network server task to get vtable from
 */
APT_DECLARE(apt_task_vtable_t*) apt_net_server_task_vtable_get(apt_net_server_task_t *task);

/**
 * Get external object.
 * @param task the task to get object from
 */
APT_DECLARE(void*) apt_net_server_task_object_get(apt_net_server_task_t *task);

/**
 * Close connection.
 */
APT_DECLARE(apt_bool_t) apt_net_server_connection_close(apt_net_server_task_t *task, apt_net_server_connection_t *connection);

/**
 * Destroy connection.
 */
APT_DECLARE(void) apt_net_server_connection_destroy(apt_net_server_connection_t *connection);


APT_END_EXTERN_C

#endif /*__APT_NET_SERVER_TASK_H__*/
