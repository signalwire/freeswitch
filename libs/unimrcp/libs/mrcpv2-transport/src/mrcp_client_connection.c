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

#include "mrcp_connection.h"
#include "mrcp_client_connection.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "apt_text_stream.h"
#include "apt_task.h"
#include "apt_pollset.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"

#define MRCPV2_CONNECTION_TASK_NAME "TCP/MRCPv2 Connection Agent"

struct mrcp_connection_agent_t {
	apr_pool_t              *pool;
	apt_task_t              *task;

	mrcp_resource_factory_t *resource_factory;

	apt_obj_list_t          *connection_list;

	apt_bool_t               offer_new_connection;
	apr_size_t               max_connection_count;

	apr_thread_mutex_t      *guard;
	apt_cyclic_queue_t      *msg_queue;
	apt_pollset_t           *pollset;

	void                                 *obj;
	const mrcp_connection_event_vtable_t *vtable;
};

typedef enum {
	CONNECTION_TASK_MSG_ADD_CHANNEL,
	CONNECTION_TASK_MSG_MODIFY_CHANNEL,
	CONNECTION_TASK_MSG_REMOVE_CHANNEL,
	CONNECTION_TASK_MSG_SEND_MESSAGE,
	CONNECTION_TASK_MSG_TERMINATE
} connection_task_msg_type_e;

typedef struct connection_task_msg_t connection_task_msg_t;
struct connection_task_msg_t {
	connection_task_msg_type_e type;
	mrcp_connection_agent_t   *agent;
	mrcp_control_channel_t    *channel;
	mrcp_control_descriptor_t *descriptor;
	mrcp_message_t            *message;
};


static apt_bool_t mrcp_client_agent_task_run(apt_task_t *task);
static apt_bool_t mrcp_client_agent_task_terminate(apt_task_t *task);
static apt_bool_t mrcp_client_agent_task_on_destroy(apt_task_t *task);

/** Create connection agent. */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_create(
											apr_size_t max_connection_count, 
											apt_bool_t offer_new_connection,
											apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	mrcp_connection_agent_t *agent;
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "MRCPV2_CONNECTION_TASK_NAME" [%"APR_SIZE_T_FMT"]",max_connection_count);
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->pollset = NULL;
	agent->max_connection_count = max_connection_count;
	agent->offer_new_connection = offer_new_connection;

	agent->task = apt_task_create(agent,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	apt_task_name_set(agent->task,MRCPV2_CONNECTION_TASK_NAME);
	vtable = apt_task_vtable_get(agent->task);
	if(vtable) {
		vtable->run = mrcp_client_agent_task_run;
		vtable->terminate = mrcp_client_agent_task_terminate;
		vtable->destroy = mrcp_client_agent_task_on_destroy;
	}
	apt_task_auto_ready_set(agent->task,FALSE);

	agent->connection_list = apt_list_create(pool);

	agent->msg_queue = apt_cyclic_queue_create(CYCLIC_QUEUE_DEFAULT_SIZE);
	apr_thread_mutex_create(&agent->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return agent;
}

/** Virtual destroy handler. */
static apt_bool_t mrcp_client_agent_task_on_destroy(apt_task_t *task)
{
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	if(agent->guard) {
		apr_thread_mutex_destroy(agent->guard);
		agent->guard = NULL;
	}
	if(agent->msg_queue) {
		apt_cyclic_queue_destroy(agent->msg_queue);
		agent->msg_queue = NULL;
	}
	return TRUE;
}

/** Destroy connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy MRCPv2 Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}

/** Set connection event handler. */
MRCP_DECLARE(void) mrcp_client_connection_agent_handler_set(
									mrcp_connection_agent_t *agent, 
									void *obj, 
									const mrcp_connection_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Set MRCP resource factory */
MRCP_DECLARE(void) mrcp_client_connection_resource_factory_set(
								mrcp_connection_agent_t *agent, 
								mrcp_resource_factory_t *resource_factroy)
{
	agent->resource_factory = resource_factroy;
}

/** Get task */
MRCP_DECLARE(apt_task_t*) mrcp_client_connection_agent_task_get(mrcp_connection_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
MRCP_DECLARE(void*) mrcp_client_connection_agent_object_get(mrcp_connection_agent_t *agent)
{
	return agent->obj;
}


/** Create control channel */
MRCP_DECLARE(mrcp_control_channel_t*) mrcp_client_control_channel_create(mrcp_connection_agent_t *agent, void *obj, apr_pool_t *pool)
{
	mrcp_control_channel_t *channel = apr_palloc(pool,sizeof(mrcp_control_channel_t));
	channel->agent = agent;
	channel->connection = NULL;
	channel->removed = FALSE;
	channel->obj = obj;
	channel->pool = pool;
	return channel;
}

/** Destroy MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_destroy(mrcp_control_channel_t *channel)
{
	if(channel && channel->connection && channel->removed == TRUE) {
		mrcp_connection_t *connection = channel->connection;
		channel->connection = NULL;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP/MRCPv2 Connection %s",connection->id);
		mrcp_connection_destroy(connection);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_control_message_signal(
								connection_task_msg_type_e type,
								mrcp_connection_agent_t *agent,
								mrcp_control_channel_t *channel,
								mrcp_control_descriptor_t *descriptor,
								mrcp_message_t *message)
{
	apt_bool_t status;
	connection_task_msg_t *msg = apr_palloc(channel->pool,sizeof(connection_task_msg_t));
	msg->type = type;
	msg->agent = agent;
	msg->channel = channel;
	msg->descriptor = descriptor;
	msg->message = message;

	apr_thread_mutex_lock(agent->guard);
	status = apt_cyclic_queue_push(agent->msg_queue,msg);
	apr_thread_mutex_unlock(agent->guard);
	if(apt_pollset_wakeup(agent->pollset) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		status = FALSE;
	}
	return status;
}

/** Add MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_add(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_client_control_message_signal(CONNECTION_TASK_MSG_ADD_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Modify MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_modify(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_client_control_message_signal(CONNECTION_TASK_MSG_MODIFY_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Remove MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_remove(mrcp_control_channel_t *channel)
{
	return mrcp_client_control_message_signal(CONNECTION_TASK_MSG_REMOVE_CHANNEL,channel->agent,channel,NULL,NULL);
}

/** Send MRCPv2 message */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_message_send(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_client_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,channel->agent,channel,NULL,message);
}

static mrcp_connection_t* mrcp_client_agent_connection_create(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	char *local_ip = NULL;
	char *remote_ip = NULL;
	mrcp_connection_t *connection = mrcp_connection_create();

	apr_sockaddr_info_get(&connection->r_sockaddr,descriptor->ip.buf,APR_INET,descriptor->port,0,connection->pool);
	if(!connection->r_sockaddr) {
		mrcp_connection_destroy(connection);
		return NULL;
	}

	if(apr_socket_create(&connection->sock,connection->r_sockaddr->family,SOCK_STREAM,APR_PROTO_TCP,connection->pool) != APR_SUCCESS) {
		mrcp_connection_destroy(connection);
		return NULL;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock, connection->r_sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return NULL;
	}

	if(apr_socket_addr_get(&connection->l_sockaddr,APR_LOCAL,connection->sock) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return NULL;
	}

	apr_sockaddr_ip_get(&local_ip,connection->l_sockaddr);
	apr_sockaddr_ip_get(&remote_ip,connection->r_sockaddr);
	connection->id = apr_psprintf(connection->pool,"%s:%hu <-> %s:%hu",
		local_ip,connection->l_sockaddr->port,
		remote_ip,connection->r_sockaddr->port);

	memset(&connection->sock_pfd,0,sizeof(apr_pollfd_t));
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_pollset_add(agent->pollset, &connection->sock_pfd) != TRUE) {
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Established TCP/MRCPv2 Connection %s",connection->id);
	connection->agent = agent;
	connection->it = apt_list_push_back(agent->connection_list,connection,connection->pool);
	connection->parser = mrcp_parser_create(agent->resource_factory,connection->pool);
	connection->generator = mrcp_generator_create(agent->resource_factory,connection->pool);
	return connection;
}

static mrcp_connection_t* mrcp_client_agent_connection_find(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	apr_sockaddr_t *sockaddr;
	mrcp_connection_t *connection = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(agent->connection_list);
	/* walk through the list of connections */
	while(elem) {
		connection = apt_list_elem_object_get(elem);
		if(connection) {
			if(apr_sockaddr_info_get(&sockaddr,descriptor->ip.buf,APR_INET,descriptor->port,0,connection->pool) == APR_SUCCESS) {
				if(apr_sockaddr_equal(sockaddr,connection->r_sockaddr) != 0 && 
					descriptor->port == connection->r_sockaddr->port) {
					return connection;
				}
			}
		}
		elem = apt_list_next_elem_get(agent->connection_list,elem);
	}
	return NULL;
}

static apt_bool_t mrcp_client_agent_connection_remove(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	/* remove from the list */
	if(connection->it) {
		apt_list_elem_remove(agent->connection_list,connection->it);
		connection->it = NULL;
	}
	apt_pollset_remove(agent->pollset,&connection->sock_pfd);
	if(connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close TCP/MRCPv2 Connection %s",connection->id);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	return TRUE;
}


static apt_bool_t mrcp_client_agent_channel_add(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	if(agent->offer_new_connection == TRUE) {
		descriptor->connection_type = MRCP_CONNECTION_TYPE_NEW;
	}
	else {
		descriptor->connection_type = MRCP_CONNECTION_TYPE_EXISTING;
		if(apt_list_is_empty(agent->connection_list) == TRUE) {
			/* offer new connection if there is no established connection yet */
			descriptor->connection_type = MRCP_CONNECTION_TYPE_NEW;
		}
	}
	/* send response */
	return mrcp_control_channel_add_respond(agent->vtable,channel,descriptor,TRUE);
}

static apt_bool_t mrcp_client_agent_channel_modify(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	apt_bool_t status = TRUE;
	if(descriptor->port) {
		if(!channel->connection) {
			mrcp_connection_t *connection = NULL;
			apt_id_resource_generate(&descriptor->session_id,&descriptor->resource_name,'@',&channel->identifier,channel->pool);
			/* no connection yet */
			if(descriptor->connection_type == MRCP_CONNECTION_TYPE_EXISTING) {
				/* try to find existing connection */
				connection = mrcp_client_agent_connection_find(agent,descriptor);
				if(!connection) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Found No Existing TCP/MRCPv2 Connection");
				}
			}
			if(!connection) {
				/* create new connection */
				connection = mrcp_client_agent_connection_create(agent,descriptor);
				if(!connection) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Establish TCP/MRCPv2 Connection");
				}
			}

			if(connection) {
				mrcp_connection_channel_add(connection,channel);
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Control Channel <%s> %s [%d]",
						channel->identifier.buf,
						connection->id,
						apr_hash_count(connection->channel_table));
				if(descriptor->connection_type == MRCP_CONNECTION_TYPE_NEW) {
					/* set connection type to existing for the next offers / if any */
					descriptor->connection_type = MRCP_CONNECTION_TYPE_EXISTING;
				}
			}
			else {
				descriptor->port = 0;
				status = FALSE;
			}
		}
	}
	/* send response */
	return mrcp_control_channel_modify_respond(agent->vtable,channel,descriptor,status);
}

static apt_bool_t mrcp_client_agent_channel_remove(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel)
{
	if(channel->connection) {
		mrcp_connection_t *connection = channel->connection;
		mrcp_connection_channel_remove(connection,channel);
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Control Channel <%s> [%d]",
				channel->identifier.buf,
				apr_hash_count(connection->channel_table));
		if(!connection->access_count) {
			mrcp_client_agent_connection_remove(agent,connection);
			/* set connection to be destroyed on channel destroy */
			channel->connection = connection;
			channel->removed = TRUE;
		}
	}
	
	/* send response */
	return mrcp_control_channel_remove_respond(agent->vtable,channel,TRUE);
}

static apt_bool_t mrcp_client_agent_messsage_send(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	apt_bool_t status = FALSE;
	mrcp_connection_t *connection = channel->connection;
	apt_text_stream_t *stream;
	mrcp_stream_status_e result;

	if(!connection || !connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No MRCPv2 Connection");
		return FALSE;
	}
	stream = &connection->tx_stream;

	mrcp_generator_message_set(connection->generator,message);
	do {
		apt_text_stream_init(&connection->tx_stream,connection->tx_buffer,sizeof(connection->tx_buffer)-1);
		result = mrcp_generator_run(connection->generator,stream);
		if(result != MRCP_STREAM_STATUS_INVALID) {
			stream->text.length = stream->pos - stream->text.buf;
			*stream->pos = '\0';

			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send MRCPv2 Stream %s [%lu bytes]\n%s",
				connection->id,
				stream->text.length,
				stream->text.buf);
			if(apr_socket_send(connection->sock,stream->text.buf,&stream->text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send MRCPv2 Stream");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate MRCPv2 Stream");
		}
	}
	while(result == MRCP_STREAM_STATUS_INCOMPLETE);

	if(status == FALSE) {
		mrcp_message_t *response = mrcp_response_create(message,message->pool);
		response->start_line.method_id = message->start_line.method_id;
		response->start_line.method_name = message->start_line.method_name;
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		mrcp_connection_message_receive(agent->vtable,channel,response);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_message_handler(void *obj, mrcp_message_t *message, mrcp_stream_status_e status)
{
	if(status == MRCP_STREAM_STATUS_COMPLETE) {
		/* message is completely parsed */
		mrcp_connection_t *connection = obj;
		mrcp_control_channel_t *channel;
		apt_str_t identifier;
		apt_id_resource_generate(&message->channel_id.session_id,&message->channel_id.resource_name,'@',&identifier,message->pool);
		channel = mrcp_connection_channel_find(connection,&identifier);
		if(channel) {
			mrcp_connection_agent_t *agent = connection->agent;
			mrcp_connection_message_receive(agent->vtable,channel,message);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Find Channel <%s@%s> in Connection %s [%d]",
				message->channel_id.session_id.buf,
				message->channel_id.resource_name.buf,
				connection->id,
				apr_hash_count(connection->channel_table));
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_agent_messsage_receive(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;

	if(!connection || !connection->sock) {
		return FALSE;
	}
	stream = &connection->rx_stream;

	/* init length of the stream */
	stream->text.length = sizeof(connection->rx_buffer)-1;
	/* calculate offset remaining from the previous receive / if any */
	offset = stream->pos - stream->text.buf;
	/* calculate available length */
	length = stream->text.length - offset;
	status = apr_socket_recv(connection->sock,stream->pos,&length);
	if(status == APR_EOF || length == 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"TCP/MRCPv2 Peer Disconnected %s",connection->id);
		apt_pollset_remove(agent->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;

		mrcp_connection_disconnect_raise(connection,agent->vtable);
		return TRUE;
	}
	/* calculate actual length of the stream */
	stream->text.length = offset + length;
	stream->pos[length] = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive MRCPv2 Stream %s [%lu bytes]\n%s",
		connection->id,
		length,
		stream->pos);

	/* reset pos */
	apt_text_stream_reset(stream);
	/* walk through the stream parsing MRCP messages */
	return mrcp_stream_walk(connection->parser,stream,mrcp_client_message_handler,connection);
}

static apt_bool_t mrcp_client_agent_control_process(mrcp_connection_agent_t *agent)
{
	apt_bool_t status = TRUE;
	apt_bool_t running = TRUE;
	connection_task_msg_t *msg;

	do {
		apr_thread_mutex_lock(agent->guard);
		msg = apt_cyclic_queue_pop(agent->msg_queue);
		apr_thread_mutex_unlock(agent->guard);
		if(msg) {
			switch(msg->type) {
				case CONNECTION_TASK_MSG_ADD_CHANNEL:
					mrcp_client_agent_channel_add(agent,msg->channel,msg->descriptor);
					break;
				case CONNECTION_TASK_MSG_MODIFY_CHANNEL:
					mrcp_client_agent_channel_modify(agent,msg->channel,msg->descriptor);
					break;
				case CONNECTION_TASK_MSG_REMOVE_CHANNEL:
					mrcp_client_agent_channel_remove(agent,msg->channel);
					break;
				case CONNECTION_TASK_MSG_SEND_MESSAGE:
					mrcp_client_agent_messsage_send(agent,msg->channel,msg->message);
					break;
				case CONNECTION_TASK_MSG_TERMINATE:
					status = FALSE;
					break;
			}
		}
		else {
			running = FALSE;
		}
	}
	while(running == TRUE);
	return status;
}

static apt_bool_t mrcp_client_agent_task_run(apt_task_t *task)
{
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!agent) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start MRCPv2 Agent");
		return FALSE;
	}

	agent->pollset = apt_pollset_create((apr_uint32_t)agent->max_connection_count,agent->pool);
	if(!agent->pollset) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* explicitly indicate task is ready to process messages */
	apt_task_ready(agent->task);

	while(running) {
		status = apt_pollset_poll(agent->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(apt_pollset_is_wakeup(agent->pollset,&ret_pfd[i])) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Control Message");
				if(mrcp_client_agent_control_process(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			mrcp_client_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	apt_pollset_destroy(agent->pollset);
	agent->pollset = NULL;

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t mrcp_client_agent_task_terminate(apt_task_t *task)
{
	apt_bool_t status = FALSE;
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	if(agent->pollset) {
		connection_task_msg_t *msg = apr_pcalloc(agent->pool,sizeof(connection_task_msg_t));
		msg->type = CONNECTION_TASK_MSG_TERMINATE;

		apr_thread_mutex_lock(agent->guard);
		status = apt_cyclic_queue_push(agent->msg_queue,msg);
		apr_thread_mutex_unlock(agent->guard);
		if(apt_pollset_wakeup(agent->pollset) == TRUE) {
			status = TRUE;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		}
	}
	return status;
}
