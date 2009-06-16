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

#ifndef __APT_NET_CLIENT_TASK_H__
#define __APT_NET_CLIENT_TASK_H__

/**
 * @file apt_net_client_task.h
 * @brief Network Client Task Base
 */ 

#include <apr_poll.h>
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Opaque network client task declaration */
typedef struct apt_net_client_task_t apt_net_client_task_t;
/** Network client connection declaration */
typedef struct apt_net_client_connection_t apt_net_client_connection_t;
/** Virtual table of network client events */
typedef struct apt_net_client_vtable_t apt_net_client_vtable_t;

/** Network client connection */
struct apt_net_client_connection_t {
	/** Memory pool */
	apr_pool_t   *pool;
	/** External object */
	void         *obj;
	/** Connected socket */
	apr_socket_t *sock;
	/** Socket poll descriptor */
	apr_pollfd_t  sock_pfd;
	/** String identifier used for traces */
	const char   *id;
};

/** Virtual table of network client events */
struct apt_net_client_vtable_t {
	/** Message receive handler */
	apt_bool_t (*on_receive)(apt_net_client_task_t *task, apt_net_client_connection_t *connection);
};


/**
 * Create network client task.
 * @param max_connection_count the number of max connections
 * @param obj the external object
 * @param client_vtable the table of virtual methods of the net client task
 * @param msg_pool the pool of task messages
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_net_client_task_t*) apt_net_client_task_create(
										apr_size_t max_connection_count,
										void *obj,
										const apt_net_client_vtable_t *client_vtable,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool);

/**
 * Destroy network client task.
 * @param task the task to destroy
 */
APT_DECLARE(apt_bool_t) apt_net_client_task_destroy(apt_net_client_task_t *task);

/**
 * Start network client task and wait for incoming requests.
 * @param task the task to start
 */
APT_DECLARE(apt_bool_t) apt_net_client_task_start(apt_net_client_task_t *task);

/**
 * Terminate connection task.
 * @param task the task to terminate
 */
APT_DECLARE(apt_bool_t) apt_net_client_task_terminate(apt_net_client_task_t *task);

/**
 * Get task base.
 * @param task the network client task to get task base from
 */
APT_DECLARE(apt_task_t*) apt_net_client_task_base_get(apt_net_client_task_t *task);

/**
 * Get task vtable.
 * @param task the network client task to get vtable from
 */
APT_DECLARE(apt_task_vtable_t*) apt_net_client_task_vtable_get(apt_net_client_task_t *task);

/**
 * Get external object.
 * @param task the task to get object from
 */
APT_DECLARE(void*) apt_net_client_task_object_get(apt_net_client_task_t *task);

/**
 * Create connection.
 */
APT_DECLARE(apt_net_client_connection_t*) apt_net_client_connect(apt_net_client_task_t *task, const char *ip, apr_port_t port);

/**
 * Close connection.
 */
APT_DECLARE(apt_bool_t) apt_net_client_connection_close(apt_net_client_task_t *task, apt_net_client_connection_t *connection);

/**
 * Close and destroy connection.
 */
APT_DECLARE(apt_bool_t) apt_net_client_disconnect(apt_net_client_task_t *task, apt_net_client_connection_t *connection);


APT_END_EXTERN_C

#endif /*__APT_NET_CLIENT_TASK_H__*/
