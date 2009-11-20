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

#include "apt_net_client_task.h"
#include "apt_task.h"
#include "apt_pool.h"
#include "apt_pollset.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"


/** Network client task */
struct apt_net_client_task_t {
	apr_pool_t                    *pool;
	apt_task_t                    *base;
	void                          *obj;

	apr_size_t                     max_connection_count;

	apr_thread_mutex_t            *guard;
	apt_cyclic_queue_t            *msg_queue;
	apt_pollset_t                 *pollset;

	const apt_net_client_vtable_t *client_vtable;
};

static apt_bool_t apt_net_client_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t apt_net_client_task_run(apt_task_t *task);
static apt_bool_t apt_net_client_task_on_destroy(apt_task_t *task);

/** Create connection task */
APT_DECLARE(apt_net_client_task_t*) apt_net_client_task_create(
										apr_size_t max_connection_count,
										void *obj,
										const apt_net_client_vtable_t *client_vtable,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_net_client_task_t *task;
	
	task = apr_palloc(pool,sizeof(apt_net_client_task_t));
	task->pool = pool;
	task->obj = obj;
	task->pollset = NULL;
	task->max_connection_count = max_connection_count;

	if(!client_vtable || !client_vtable->on_receive) {
		return NULL;
	}
	task->client_vtable = client_vtable;

	task->base = apt_task_create(task,msg_pool,pool);
	if(!task->base) {
		return NULL;
	}

	vtable = apt_task_vtable_get(task->base);
	if(vtable) {
		vtable->run = apt_net_client_task_run;
		vtable->destroy = apt_net_client_task_on_destroy;
		vtable->signal_msg = apt_net_client_task_msg_signal;
	}
	apt_task_auto_ready_set(task->base,FALSE);

	task->msg_queue = apt_cyclic_queue_create(CYCLIC_QUEUE_DEFAULT_SIZE);
	apr_thread_mutex_create(&task->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return task;
}

/** Virtual destroy handler */
static apt_bool_t apt_net_client_task_on_destroy(apt_task_t *base)
{
	apt_net_client_task_t *task = apt_task_object_get(base);
	if(task->guard) {
		apr_thread_mutex_destroy(task->guard);
		task->guard = NULL;
	}
	if(task->msg_queue) {
		apt_cyclic_queue_destroy(task->msg_queue);
		task->msg_queue = NULL;
	}
	return TRUE;
}

/** Destroy connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_destroy(apt_net_client_task_t *task)
{
	return apt_task_destroy(task->base);
}

/** Start connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_start(apt_net_client_task_t *task)
{
	return apt_task_start(task->base);
}

/** Terminate connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_terminate(apt_net_client_task_t *task)
{
	return apt_task_terminate(task->base,TRUE);
}

/** Get task */
APT_DECLARE(apt_task_t*) apt_net_client_task_base_get(apt_net_client_task_t *task)
{
	return task->base;
}

/** Get task vtable */
APT_DECLARE(apt_task_vtable_t*) apt_net_client_task_vtable_get(apt_net_client_task_t *task)
{
	return apt_task_vtable_get(task->base);
}

/** Get external object */
APT_DECLARE(void*) apt_net_client_task_object_get(apt_net_client_task_t *task)
{
	return task->obj;
}

/** Create connection */
APT_DECLARE(apt_net_client_connection_t*) apt_net_client_connect(apt_net_client_task_t *task, const char *ip, apr_port_t port)
{
	char *local_ip = NULL;
	char *remote_ip = NULL;
	apr_sockaddr_t *l_sockaddr = NULL;
	apr_sockaddr_t *r_sockaddr = NULL;
	apt_net_client_connection_t *connection;
	apr_pool_t *pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(apt_net_client_connection_t));
	connection->pool = pool;
	connection->obj = NULL;
	connection->sock = NULL;

	if(apr_sockaddr_info_get(&r_sockaddr,ip,APR_INET,port,0,connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return NULL;
	}

	if(apr_socket_create(&connection->sock,r_sockaddr->family,SOCK_STREAM,APR_PROTO_TCP,connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return NULL;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock,r_sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}

	if(apr_socket_addr_get(&l_sockaddr,APR_LOCAL,connection->sock) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}

	apr_sockaddr_ip_get(&local_ip,l_sockaddr);
	apr_sockaddr_ip_get(&remote_ip,r_sockaddr);
	connection->id = apr_psprintf(pool,"%s:%hu <-> %s:%hu",
		local_ip,l_sockaddr->port,
		remote_ip,r_sockaddr->port);

	memset(&connection->sock_pfd,0,sizeof(apr_pollfd_t));
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_pollset_add(task->pollset,&connection->sock_pfd) != TRUE) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Established TCP Connection %s",connection->id);
	return connection;
}

/** Close connection */
APT_DECLARE(apt_bool_t) apt_net_client_connection_close(apt_net_client_task_t *task, apt_net_client_connection_t *connection)
{
	if(connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close TCP Connection %s",connection->id);
		apt_pollset_remove(task->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	return TRUE;
}

/** Close and destroy connection */
APT_DECLARE(apt_bool_t) apt_net_client_disconnect(apt_net_client_task_t *task, apt_net_client_connection_t *connection)
{
	apt_net_client_connection_close(task,connection);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP Connection %s",connection->id);
	apr_pool_destroy(connection->pool);
	return TRUE;
}

/** Create the pollset */
static apt_bool_t apt_net_client_task_pollset_create(apt_net_client_task_t *task)
{
	/* create pollset */
	task->pollset = apt_pollset_create((apr_uint32_t)task->max_connection_count, task->pool);
	if(!task->pollset) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	return TRUE;
}

/** Destroy the pollset */
static void apt_net_client_task_pollset_destroy(apt_net_client_task_t *task)
{
	if(task->pollset) {
		apt_pollset_destroy(task->pollset);
		task->pollset = NULL;
	}
}

static apt_bool_t apt_net_client_task_process(apt_net_client_task_t *task)
{
	apt_bool_t status = TRUE;
	apt_bool_t running = TRUE;
	apt_task_msg_t *msg;

	do {
		apr_thread_mutex_lock(task->guard);
		msg = apt_cyclic_queue_pop(task->msg_queue);
		apr_thread_mutex_unlock(task->guard);
		if(msg) {
			status = apt_task_msg_process(task->base,msg);
		}
		else {
			running = FALSE;
		}
	}
	while(running == TRUE);
	return status;
}

static apt_bool_t apt_net_client_task_run(apt_task_t *base)
{
	apt_net_client_task_t *task = apt_task_object_get(base);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Network Client Task");
		return FALSE;
	}

	if(apt_net_client_task_pollset_create(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* explicitly indicate task is ready to process messages */
	apt_task_ready(task->base);

	while(running) {
		status = apt_pollset_poll(task->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(apt_pollset_is_wakeup(task->pollset,&ret_pfd[i])) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Control Message");
				if(apt_net_client_task_process(task) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Message");
			task->client_vtable->on_receive(task,ret_pfd[i].client_data);
		}
	}

	apt_net_client_task_pollset_destroy(task);
	return TRUE;
}

static apt_bool_t apt_net_client_task_msg_signal(apt_task_t *base, apt_task_msg_t *msg)
{
	apt_bool_t status;
	apt_net_client_task_t *task = apt_task_object_get(base);
	apr_thread_mutex_lock(task->guard);
	status = apt_cyclic_queue_push(task->msg_queue,msg);
	apr_thread_mutex_unlock(task->guard);
	if(apt_pollset_wakeup(task->pollset) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		status = FALSE;
	}
	return status;
}
