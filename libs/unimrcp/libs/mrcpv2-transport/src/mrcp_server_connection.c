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

#include "mrcp_connection.h"
#include "mrcp_server_connection.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "apt_text_stream.h"
#include "apt_poller_task.h"
#include "apt_pool.h"
#include "apt_log.h"


struct mrcp_connection_agent_t {
	apr_pool_t                           *pool;
	apt_poller_task_t                    *task;
	const mrcp_resource_factory_t        *resource_factory;

	/** List (ring) of MRCP connections */
	APR_RING_HEAD(mrcp_connection_head_t, mrcp_connection_t) connection_list;
	/** Table of pending control channels */
	apr_hash_t                           *pending_channel_table;

	apt_bool_t                            force_new_connection;
	apr_size_t                            max_shared_use_count;
	apr_size_t                            tx_buffer_size;
	apr_size_t                            rx_buffer_size;
	apr_uint32_t                          inactivity_timeout;
	apr_uint32_t                          termination_timeout;

	/* Listening socket */
	apr_sockaddr_t                       *sockaddr;
	apr_socket_t                         *listen_sock;
	apr_pollfd_t                          listen_sock_pfd;

	void                                 *obj;
	const mrcp_connection_event_vtable_t *vtable;
};

typedef enum {
	CONNECTION_TASK_MSG_ADD_CHANNEL,
	CONNECTION_TASK_MSG_MODIFY_CHANNEL,
	CONNECTION_TASK_MSG_REMOVE_CHANNEL,
	CONNECTION_TASK_MSG_SEND_MESSAGE
} connection_task_msg_type_e;

typedef struct connection_task_msg_t connection_task_msg_t;
struct connection_task_msg_t {
	connection_task_msg_type_e type;
	mrcp_connection_agent_t   *agent;
	mrcp_control_channel_t    *channel;
	mrcp_control_descriptor_t *descriptor;
	mrcp_message_t            *message;
};

static apt_bool_t mrcp_server_agent_on_destroy(apt_task_t *task);
static apt_bool_t mrcp_server_agent_msg_process(apt_task_t *task, apt_task_msg_t *task_msg);
static apt_bool_t mrcp_server_poller_signal_process(void *obj, const apr_pollfd_t *descriptor);

static apt_bool_t mrcp_server_agent_listening_socket_create(mrcp_connection_agent_t *agent);
static void mrcp_server_agent_listening_socket_destroy(mrcp_connection_agent_t *agent);

static void mrcp_server_inactivity_timer_proc(apt_timer_t *timer, void *obj);
static void mrcp_server_termination_timer_proc(apt_timer_t *timer, void *obj);

/** Create connection agent */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_create(
										const char *id,
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										apt_bool_t force_new_connection,
										apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	mrcp_connection_agent_t *agent;

	if(!listen_ip) {
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCPv2 Agent [%s] %s:%hu [%"APR_SIZE_T_FMT"]",
		id,listen_ip,listen_port,max_connection_count);
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->sockaddr = NULL;
	agent->listen_sock = NULL;
	agent->force_new_connection = force_new_connection;
	agent->max_shared_use_count = 100;
	agent->rx_buffer_size = MRCP_STREAM_BUFFER_SIZE;
	agent->tx_buffer_size = MRCP_STREAM_BUFFER_SIZE;
	agent->inactivity_timeout = 600000; /* 10 min */
	agent->termination_timeout = 3000; /* 3 sec */

	apr_sockaddr_info_get(&agent->sockaddr,listen_ip,APR_INET,listen_port,0,pool);
	if(!agent->sockaddr) {
		return NULL;
	}

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_task_msg_t),pool);
	
	agent->task = apt_poller_task_create(
					max_connection_count + 1,
					mrcp_server_poller_signal_process,
					agent,
					msg_pool,
					pool);
	if(!agent->task) {
		return NULL;
	}

	task = apt_poller_task_base_get(agent->task);
	if(task) {
		apt_task_name_set(task,id);
	}

	vtable = apt_poller_task_vtable_get(agent->task);
	if(vtable) {
		vtable->destroy = mrcp_server_agent_on_destroy;
		vtable->process_msg = mrcp_server_agent_msg_process;
	}

	APR_RING_INIT(&agent->connection_list, mrcp_connection_t, link);
	agent->pending_channel_table = apr_hash_make(pool);

	if(mrcp_server_agent_listening_socket_create(agent) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Listening Socket [%s] %s:%hu", 
				id,
				listen_ip,
				listen_port);
	}
	return agent;
}

static apt_bool_t mrcp_server_agent_on_destroy(apt_task_t *task)
{
	apt_poller_task_t *poller_task = apt_task_object_get(task);
	mrcp_connection_agent_t *agent = apt_poller_task_object_get(poller_task);

	mrcp_server_agent_listening_socket_destroy(agent);
	apt_poller_task_cleanup(poller_task);
	return TRUE;
}

/** Destroy connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy MRCPv2 Agent [%s]",
		mrcp_server_connection_agent_id_get(agent));
	return apt_poller_task_destroy(agent->task);
}

/** Start connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_poller_task_start(agent->task);
}

/** Terminate connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_poller_task_terminate(agent->task);
}

/** Set connection event handler. */
MRCP_DECLARE(void) mrcp_server_connection_agent_handler_set(
									mrcp_connection_agent_t *agent,
									void *obj,
									const mrcp_connection_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Set MRCP resource factory */
MRCP_DECLARE(void) mrcp_server_connection_resource_factory_set(
								mrcp_connection_agent_t *agent,
								const mrcp_resource_factory_t *resource_factroy)
{
	agent->resource_factory = resource_factroy;
}

/** Set rx buffer size */
MRCP_DECLARE(void) mrcp_server_connection_rx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size)
{
	if(size < MRCP_STREAM_BUFFER_SIZE) {
		size = MRCP_STREAM_BUFFER_SIZE;
	}
	agent->rx_buffer_size = size;
}

/** Set tx buffer size */
MRCP_DECLARE(void) mrcp_server_connection_tx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size)
{
	if(size < MRCP_STREAM_BUFFER_SIZE) {
		size = MRCP_STREAM_BUFFER_SIZE;
	}
	agent->tx_buffer_size = size;
}

/** Set max shared use count for an MRCPv2 connection */
MRCP_DECLARE(void) mrcp_server_connection_max_shared_use_set(
								mrcp_connection_agent_t *agent,
								apr_size_t max_shared_use_count)
{
	agent->max_shared_use_count = max_shared_use_count;
}

/** Set inactivity timeout for an MRCPv2 connection */
MRCP_DECLARE(void) mrcp_server_connection_timeout_set(
								mrcp_connection_agent_t *agent,
								apr_size_t timeout)
{
	agent->inactivity_timeout = (apr_uint32_t)timeout * 1000;
}

/** Set termination timeout for an MRCPv2 connection */
MRCP_DECLARE(void) mrcp_server_connection_term_timeout_set(
								mrcp_connection_agent_t *agent,
								apr_size_t timeout)
{
	agent->termination_timeout = (apr_uint32_t)timeout * 1000;
}


/** Get task */
MRCP_DECLARE(apt_task_t*) mrcp_server_connection_agent_task_get(const mrcp_connection_agent_t *agent)
{
	return apt_poller_task_base_get(agent->task);
}

/** Get external object */
MRCP_DECLARE(void*) mrcp_server_connection_agent_object_get(const mrcp_connection_agent_t *agent)
{
	return agent->obj;
}

/** Get string identifier */
MRCP_DECLARE(const char*) mrcp_server_connection_agent_id_get(const mrcp_connection_agent_t *agent)
{
	apt_task_t *task = apt_poller_task_base_get(agent->task);
	return apt_task_name_get(task);
}


/** Create MRCPv2 control channel */
MRCP_DECLARE(mrcp_control_channel_t*) mrcp_server_control_channel_create(mrcp_connection_agent_t *agent, void *obj, apr_pool_t *pool)
{
	mrcp_control_channel_t *channel = apr_palloc(pool,sizeof(mrcp_control_channel_t));
	channel->agent = agent;
	channel->connection = NULL;
	channel->active_request = NULL;
	channel->request_timer = NULL;
	channel->removed = FALSE;
	channel->obj = obj;
	channel->log_obj = NULL;
	channel->pool = pool;
	return channel;
}

/** Destroy MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_destroy(mrcp_control_channel_t *channel)
{
	if(channel && channel->connection && channel->removed == TRUE) {
		mrcp_connection_t *connection = channel->connection;
		channel->connection = NULL;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP/MRCPv2 Connection %s",connection->id);
		mrcp_connection_destroy(connection);
	}
	return TRUE;
}

/** Signal task message */
static apt_bool_t mrcp_server_control_message_signal(
								connection_task_msg_type_e type,
								mrcp_connection_agent_t *agent,
								mrcp_control_channel_t *channel,
								mrcp_control_descriptor_t *descriptor,
								mrcp_message_t *message)
{
	apt_task_t *task = apt_poller_task_base_get(agent->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		connection_task_msg_t *msg = (connection_task_msg_t*)task_msg->data;
		msg->type = type;
		msg->agent = agent;
		msg->channel = channel;
		msg->descriptor = descriptor;
		msg->message = message;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
}

/** Add MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_add(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_ADD_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Modify MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_modify(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_MODIFY_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Remove MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_remove(mrcp_control_channel_t *channel)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_REMOVE_CHANNEL,channel->agent,channel,NULL,NULL);
}

/** Send MRCPv2 message */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_message_send(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,channel->agent,channel,NULL,message);
}

/** Create listening socket and add it to pollset */
static apt_bool_t mrcp_server_agent_listening_socket_create(mrcp_connection_agent_t *agent)
{
	apr_status_t status;
	if(!agent->sockaddr) {
		return FALSE;
	}

	/* create listening socket */
	status = apr_socket_create(&agent->listen_sock, agent->sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, agent->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}

	apr_socket_opt_set(agent->listen_sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(agent->listen_sock, -1);
	apr_socket_opt_set(agent->listen_sock, APR_SO_REUSEADDR, 1);

	status = apr_socket_bind(agent->listen_sock, agent->sockaddr);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}
	status = apr_socket_listen(agent->listen_sock, SOMAXCONN);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}

	/* add listening socket to pollset */
	memset(&agent->listen_sock_pfd,0,sizeof(apr_pollfd_t));
	agent->listen_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->listen_sock_pfd.reqevents = APR_POLLIN;
	agent->listen_sock_pfd.desc.s = agent->listen_sock;
	agent->listen_sock_pfd.client_data = agent->listen_sock;
	if(apt_poller_task_descriptor_add(agent->task, &agent->listen_sock_pfd) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add Listening Socket to Pollset [%s]",
			apt_task_name_get(apt_poller_task_base_get(agent->task)));
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}

	return TRUE;
}

/** Remove from pollset and destroy listening socket */
static void mrcp_server_agent_listening_socket_destroy(mrcp_connection_agent_t *agent)
{
	if(agent->listen_sock) {
		apt_poller_task_descriptor_remove(agent->task,&agent->listen_sock_pfd);
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
	}
}

/** Associate control channel with MRCPv2 connection */
static mrcp_control_channel_t* mrcp_connection_channel_associate(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, const mrcp_message_t *message)
{
	apt_str_t identifier;
	mrcp_control_channel_t *channel;
	if(!connection || !message) {
		return NULL;
	}
	apt_id_resource_generate(&message->channel_id.session_id,&message->channel_id.resource_name,'@',&identifier,connection->pool);
	channel = mrcp_connection_channel_find(connection,&identifier);
	if(!channel) {
		channel = apr_hash_get(agent->pending_channel_table,identifier.buf,identifier.length);
		if(channel) {
			apr_hash_set(agent->pending_channel_table,identifier.buf,identifier.length,NULL);
			mrcp_connection_channel_add(connection,channel);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Assign Control Channel <%s> to Connection %s [%d] -> [%d]",
				channel->identifier.buf,
				connection->id,
				apr_hash_count(agent->pending_channel_table),
				apr_hash_count(connection->channel_table));
		}
	}
	return channel;
}

static mrcp_connection_t* mrcp_connection_find(mrcp_connection_agent_t *agent, const apt_str_t *remote_ip)
{
	mrcp_connection_t *connection;
	if(!agent || !remote_ip) {
		return NULL;
	}

	for(connection = APR_RING_FIRST(&agent->connection_list);
			connection != APR_RING_SENTINEL(&agent->connection_list, mrcp_connection_t, link);
				connection = APR_RING_NEXT(connection, link)) {
		if(apt_string_compare(&connection->remote_ip,remote_ip) == TRUE) {
			return connection;
		}
	}

	return NULL;
}

static apt_bool_t mrcp_connection_add(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	APR_RING_INSERT_TAIL(&agent->connection_list,connection,mrcp_connection_t,link);
	if(connection->inactivity_timer) {
		apt_timer_set(connection->inactivity_timer,agent->inactivity_timeout);
	}
	return TRUE;
}

static apt_bool_t mrcp_connection_remove(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	if(connection->inactivity_timer) {
		apt_timer_kill(connection->inactivity_timer);
	}
	APR_RING_REMOVE(connection,link);
	return TRUE;
}

static apt_bool_t mrcp_server_agent_connection_accept(mrcp_connection_agent_t *agent)
{
	char *local_ip = NULL;
	char *remote_ip = NULL;
	
	mrcp_connection_t *connection = mrcp_connection_create();

	if(apr_socket_accept(&connection->sock,agent->listen_sock,connection->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Accept Connection");
		mrcp_connection_destroy(connection);
		return FALSE;
	}

	if(apr_socket_addr_get(&connection->r_sockaddr,APR_REMOTE,connection->sock) != APR_SUCCESS ||
		apr_socket_addr_get(&connection->l_sockaddr,APR_LOCAL,connection->sock) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Socket Address");
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return FALSE;
	}

	apr_sockaddr_ip_get(&local_ip,connection->l_sockaddr);
	apr_sockaddr_ip_get(&remote_ip,connection->r_sockaddr);
	apt_string_set(&connection->remote_ip,remote_ip);
	connection->id = apr_psprintf(connection->pool,"%s:%hu <-> %s:%hu",
		local_ip,connection->l_sockaddr->port,
		remote_ip,connection->r_sockaddr->port);

	if(apr_hash_count(agent->pending_channel_table) == 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Reject Unexpected TCP/MRCPv2 Connection %s",connection->id);
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return FALSE;
	}

	memset(&connection->sock_pfd,0,sizeof(apr_pollfd_t));
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_poller_task_descriptor_add(agent->task, &connection->sock_pfd) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add to Pollset %s",connection->id);
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Accepted TCP/MRCPv2 Connection %s",connection->id);
	connection->agent = agent;

	connection->parser = mrcp_parser_create(agent->resource_factory,connection->pool);
	connection->generator = mrcp_generator_create(agent->resource_factory,connection->pool);

	connection->tx_buffer_size = agent->tx_buffer_size;
	connection->tx_buffer = apr_palloc(connection->pool,connection->tx_buffer_size+1);

	connection->rx_buffer_size = agent->rx_buffer_size;
	connection->rx_buffer = apr_palloc(connection->pool,connection->rx_buffer_size+1);
	apt_text_stream_init(&connection->rx_stream,connection->rx_buffer,connection->rx_buffer_size);

	if(apt_log_masking_get() != APT_LOG_MASKING_NONE) {
		connection->verbose = FALSE;
		mrcp_parser_verbose_set(connection->parser,TRUE);
		mrcp_generator_verbose_set(connection->generator,TRUE);
	}

	if(agent->inactivity_timeout) {
		connection->inactivity_timer = apt_poller_task_timer_create(
										agent->task,
										mrcp_server_inactivity_timer_proc,
										connection,
										connection->pool);
	}

	mrcp_connection_add(agent,connection);
	return TRUE;
}

static apt_bool_t mrcp_server_agent_connection_close(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, apt_bool_t timedout)
{
	if(connection->sock) {
		apt_poller_task_descriptor_remove(agent->task,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	mrcp_connection_remove(agent,connection);
	if(connection->access_count) {
		if(timedout == TRUE) {
			mrcp_connection_disconnect_raise(connection,agent->vtable);
		}
		else {
			if(agent->termination_timeout) {
				connection->termination_timer = apt_poller_task_timer_create(
												agent->task,
												mrcp_server_termination_timer_proc,
												connection,
												connection->pool);
				if(connection->termination_timer) {
					apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Set Termination Timer %s timeout [%d]",connection->id,agent->termination_timeout);
					apt_timer_set(connection->termination_timer,agent->termination_timeout);
				}
			}
		}
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP/MRCPv2 Connection %s",connection->id);
		mrcp_connection_destroy(connection);
	}
	return TRUE;
}

/* Timer callback */
static void mrcp_server_inactivity_timer_proc(apt_timer_t *timer, void *obj)
{
	mrcp_connection_t *connection = obj;
	if(!connection) return;

	if(connection->inactivity_timer == timer) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"TCP/MRCPv2 Connection Timed Out %s",connection->id);
		mrcp_server_agent_connection_close(connection->agent, connection, TRUE);
	}
}

/* Timer callback */
static void mrcp_server_termination_timer_proc(apt_timer_t *timer, void *obj)
{
	mrcp_connection_t *connection = obj;
	if(!connection) return;

	if(connection->termination_timer == timer) {
		mrcp_connection_agent_t *agent = connection->agent;
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Termination Timeout Elapsed %s",connection->id);
		mrcp_connection_disconnect_raise(connection,agent->vtable);
	}
}

static apt_bool_t mrcp_server_agent_channel_add(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *offer)
{
	mrcp_control_descriptor_t *answer = mrcp_control_answer_create(offer,channel->pool);
	apt_id_resource_generate(&offer->session_id,&offer->resource_name,'@',&channel->identifier,channel->pool);
	if(offer->port) {
		answer->port = agent->sockaddr->port;
	}
	if(offer->connection_type == MRCP_CONNECTION_TYPE_EXISTING) {
		if(agent->force_new_connection == TRUE) {
			/* force client to establish new connection */
			answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
		}
		else {
			mrcp_connection_t *connection = NULL;
			/* try to find any existing connection */
			connection = mrcp_connection_find(agent,&offer->ip);
			if(connection) {
				if(agent->max_shared_use_count && connection->use_count >= agent->max_shared_use_count) {
					/* do not allow the same connection to be used infinitely, force a new one */
					apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Max Use Count Reached for Connection %s [%d]",
						connection->id,
						connection->use_count);
					answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
				}
			}
			else {
				/* no existing conection found, force a new one */
				answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
			}
		}
	}

	apr_hash_set(agent->pending_channel_table,channel->identifier.buf,channel->identifier.length,channel);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Pending Control Channel <%s> [%d]",
			channel->identifier.buf,
			apr_hash_count(agent->pending_channel_table));
	/* send response */
	return mrcp_control_channel_add_respond(agent->vtable,channel,answer,TRUE);
}

static apt_bool_t mrcp_server_agent_channel_modify(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *offer)
{
	mrcp_control_descriptor_t *answer = mrcp_control_answer_create(offer,channel->pool);
	if(offer->port) {
		answer->port = agent->sockaddr->port;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Modify Control Channel <%s>",channel->identifier.buf);
	/* send response */
	return mrcp_control_channel_modify_respond(agent->vtable,channel,answer,TRUE);
}

static apt_bool_t mrcp_server_agent_channel_remove(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel)
{
	mrcp_connection_t *connection = channel->connection;
	if(connection) {
		mrcp_connection_channel_remove(connection,channel);
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Control Channel <%s> [%d]",
				channel->identifier.buf,
				apr_hash_count(connection->channel_table));
		if(!connection->access_count) {
			if(!connection->sock) {
				/* set connection to be destroyed on channel destroy */
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Mark TCP/MRCPv2 Connection for Destruction %s",connection->id);
				channel->connection = connection;
				channel->removed = TRUE;

				if(connection->termination_timer) {
					apt_timer_kill(connection->termination_timer);
				}
			}
		}
	}
	else {
		apr_hash_set(agent->pending_channel_table,channel->identifier.buf,channel->identifier.length,NULL);
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Pending Control Channel <%s> [%d]",
				channel->identifier.buf,
				apr_hash_count(agent->pending_channel_table));
	}
	/* send response */
	return mrcp_control_channel_remove_respond(agent->vtable,channel,TRUE);
}

static apt_bool_t mrcp_server_agent_messsage_send(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, mrcp_message_t *message)
{
	apt_bool_t status = FALSE;
	apt_text_stream_t stream;
	apt_message_status_e result;
	if(!connection || !connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Null MRCPv2 Connection " APT_SIDRES_FMT,MRCP_MESSAGE_SIDRES(message));
		return FALSE;
	}

	do {
		apt_text_stream_init(&stream,connection->tx_buffer,connection->tx_buffer_size);
		result = mrcp_generator_run(connection->generator,message,&stream);
		if(result != APT_MESSAGE_STATUS_INVALID) {
			stream.text.length = stream.pos - stream.text.buf;
			*stream.pos = '\0';

			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send MRCPv2 Data %s [%"APR_SIZE_T_FMT" bytes]\n%.*s",
					connection->id,
					stream.text.length,
					connection->verbose == TRUE ? stream.text.length : 0,
					stream.text.buf);

			if(apr_socket_send(connection->sock,stream.text.buf,&stream.text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send MRCPv2 Data");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate MRCPv2 Data");
		}
	}
	while(result == APT_MESSAGE_STATUS_INCOMPLETE);

	return status;
}

static apt_bool_t mrcp_server_message_handler(mrcp_connection_t *connection, mrcp_message_t *message, apt_message_status_e status)
{
	mrcp_connection_agent_t *agent = connection->agent;
	if(status == APT_MESSAGE_STATUS_COMPLETE) {
		/* message is completely parsed */
		mrcp_control_channel_t *channel = mrcp_connection_channel_associate(agent,connection,message);
		if(channel) {
			/* (re)set inactivity timer on every message received */
			if(connection->inactivity_timer) {
				apt_timer_set(connection->inactivity_timer,agent->inactivity_timeout);
			}

			mrcp_connection_message_receive(agent->vtable,channel,message);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Find Channel " APT_SIDRES_FMT " in Connection %s",
				MRCP_MESSAGE_SIDRES(message),
				connection->id);
		}
	}
	else if(status == APT_MESSAGE_STATUS_INVALID) {
		/* error case */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse MRCPv2 Data");
		if(message && message->resource) {
			mrcp_message_t *response;
			response = mrcp_response_create(message,message->pool);
			response->start_line.status_code = MRCP_STATUS_CODE_UNRECOGNIZED_MESSAGE;
			if(mrcp_server_agent_messsage_send(agent,connection,response) == FALSE) {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send MRCPv2 Response");
			}
		}
	}
	return TRUE;
}

/* Receive MRCP message through TCP/MRCPv2 connection */
static apt_bool_t mrcp_server_poller_signal_process(void *obj, const apr_pollfd_t *descriptor)
{
	mrcp_connection_agent_t *agent = obj;
	mrcp_connection_t *connection = descriptor->client_data;
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;
	mrcp_message_t *message;
	apt_message_status_e msg_status;

	if(descriptor->desc.s == agent->listen_sock) {
		return mrcp_server_agent_connection_accept(agent);
	}

	if(!connection || !connection->sock) {
		return FALSE;
	}
	stream = &connection->rx_stream;

	/* calculate offset remaining from the previous receive / if any */
	offset = stream->pos - stream->text.buf;
	/* calculate available length */
	length = connection->rx_buffer_size - offset;

	status = apr_socket_recv(connection->sock,stream->pos,&length);
	if(status == APR_EOF || length == 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"TCP/MRCPv2 Peer Disconnected %s",connection->id);
		return mrcp_server_agent_connection_close(agent,connection,FALSE);
	}

	/* calculate actual length of the stream */
	stream->text.length = offset + length;
	stream->pos[length] = '\0';

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive MRCPv2 Data %s [%"APR_SIZE_T_FMT" bytes]\n%.*s",
			connection->id,
			length,
			connection->verbose == TRUE ? length : 0,
			stream->pos);

	/* reset pos */
	apt_text_stream_reset(stream);

	do {
		msg_status = mrcp_parser_run(connection->parser,stream,&message);
		if(mrcp_server_message_handler(connection,message,msg_status) == FALSE) {
			return FALSE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	/* scroll remaining stream */
	apt_text_stream_scroll(stream);
	return TRUE;
}

/* Process task message */
static apt_bool_t mrcp_server_agent_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_poller_task_t *poller_task = apt_task_object_get(task);
	mrcp_connection_agent_t *agent = apt_poller_task_object_get(poller_task);
	connection_task_msg_t *msg = (connection_task_msg_t*) task_msg->data;
	switch(msg->type) {
		case CONNECTION_TASK_MSG_ADD_CHANNEL:
			mrcp_server_agent_channel_add(agent,msg->channel,msg->descriptor);
			break;
		case CONNECTION_TASK_MSG_MODIFY_CHANNEL:
			mrcp_server_agent_channel_modify(agent,msg->channel,msg->descriptor);
			break;
		case CONNECTION_TASK_MSG_REMOVE_CHANNEL:
			mrcp_server_agent_channel_remove(agent,msg->channel);
			break;
		case CONNECTION_TASK_MSG_SEND_MESSAGE:
			mrcp_server_agent_messsage_send(agent,msg->channel->connection,msg->message);
			break;
	}

	return TRUE;
}
