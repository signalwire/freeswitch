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
 * $Id: mrcp_client_connection.c 2249 2014-11-19 05:26:24Z achaloyan@gmail.com $
 */

#include "mrcp_connection.h"
#include "mrcp_client_connection.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "apt_text_stream.h"
#include "apt_poller_task.h"
#include "apt_log.h"


struct mrcp_connection_agent_t {
	/** List (ring) of MRCP connections */
	APR_RING_HEAD(mrcp_connection_head_t, mrcp_connection_t) connection_list;

	apr_pool_t                           *pool;
	apt_poller_task_t                    *task;
	const mrcp_resource_factory_t        *resource_factory;

	apr_uint32_t                          request_timeout;
	apt_bool_t                            offer_new_connection;
	apr_size_t                            tx_buffer_size;
	apr_size_t                            rx_buffer_size;

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


static apt_bool_t mrcp_client_agent_msg_process(apt_task_t *task, apt_task_msg_t *task_msg);
static apt_bool_t mrcp_client_poller_signal_process(void *obj, const apr_pollfd_t *descriptor);
static void mrcp_client_timer_proc(apt_timer_t *timer, void *obj);

/** Create connection agent. */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_create(
											const char *id,
											apr_size_t max_connection_count, 
											apt_bool_t offer_new_connection,
											apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	mrcp_connection_agent_t *agent;

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create MRCPv2 Agent [%s] [%"APR_SIZE_T_FMT"]",
				id,	max_connection_count);
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->request_timeout = 0;
	agent->offer_new_connection = offer_new_connection;
	agent->rx_buffer_size = MRCP_STREAM_BUFFER_SIZE;
	agent->tx_buffer_size = MRCP_STREAM_BUFFER_SIZE;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_task_msg_t),pool);

	agent->task = apt_poller_task_create(
					max_connection_count,
					mrcp_client_poller_signal_process,
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
		vtable->process_msg = mrcp_client_agent_msg_process;
	}

	APR_RING_INIT(&agent->connection_list, mrcp_connection_t, link);
	return agent;
}

/** Destroy connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy MRCPv2 Agent [%s]",
		mrcp_client_connection_agent_id_get(agent));
	return apt_poller_task_destroy(agent->task);
}

/** Start connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_poller_task_start(agent->task);
}

/** Terminate connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_poller_task_terminate(agent->task);
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
								const mrcp_resource_factory_t *resource_factroy)
{
	agent->resource_factory = resource_factroy;
}

/** Set rx buffer size */
MRCP_DECLARE(void) mrcp_client_connection_rx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size)
{
	if(size < MRCP_STREAM_BUFFER_SIZE) {
		size = MRCP_STREAM_BUFFER_SIZE;
	}
	agent->rx_buffer_size = size;
}

/** Set tx buffer size */
MRCP_DECLARE(void) mrcp_client_connection_tx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size)
{
	if(size < MRCP_STREAM_BUFFER_SIZE) {
		size = MRCP_STREAM_BUFFER_SIZE;
	}
	agent->tx_buffer_size = size;
}

/** Set request timeout */
MRCP_DECLARE(void) mrcp_client_connection_timeout_set(
								mrcp_connection_agent_t *agent,
								apr_size_t timeout)
{
	agent->request_timeout = (apr_uint32_t)timeout;
}

/** Get task */
MRCP_DECLARE(apt_task_t*) mrcp_client_connection_agent_task_get(const mrcp_connection_agent_t *agent)
{
	return apt_poller_task_base_get(agent->task);
}

/** Get external object */
MRCP_DECLARE(void*) mrcp_client_connection_agent_object_get(const mrcp_connection_agent_t *agent)
{
	return agent->obj;
}

/** Get string identifier */
MRCP_DECLARE(const char*) mrcp_client_connection_agent_id_get(const mrcp_connection_agent_t *agent)
{
	apt_task_t *task = apt_poller_task_base_get(agent->task);
	return apt_task_name_get(task);
}


/** Create control channel */
MRCP_DECLARE(mrcp_control_channel_t*) mrcp_client_control_channel_create(mrcp_connection_agent_t *agent, void *obj, apr_pool_t *pool)
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

	channel->request_timer = apt_poller_task_timer_create(
								agent->task,
								mrcp_client_timer_proc,
								channel,
								pool);
	return channel;
}

/** Set the logger object */
MRCP_DECLARE(void) mrcp_client_control_channel_log_obj_set(mrcp_control_channel_t *channel, void *log_obj)
{
	channel->log_obj = log_obj;
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

/** Signal task message */
static apt_bool_t mrcp_client_control_message_signal(
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
	if(apt_poller_task_descriptor_add(agent->task, &connection->sock_pfd) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add to Pollset %s",connection->id);
		apr_socket_close(connection->sock);
		mrcp_connection_destroy(connection);
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Established TCP/MRCPv2 Connection %s",connection->id);
	connection->agent = agent;
	APR_RING_INSERT_TAIL(&agent->connection_list,connection,mrcp_connection_t,link);
	
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

	return connection;
}

static mrcp_connection_t* mrcp_client_agent_connection_find(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	apr_sockaddr_t *sockaddr;
	mrcp_connection_t *connection;

	for(connection = APR_RING_FIRST(&agent->connection_list);
			connection != APR_RING_SENTINEL(&agent->connection_list, mrcp_connection_t, link);
				connection = APR_RING_NEXT(connection, link)) {
		if(apr_sockaddr_info_get(&sockaddr,descriptor->ip.buf,APR_INET,descriptor->port,0,connection->pool) == APR_SUCCESS) {
			if(apr_sockaddr_equal(sockaddr,connection->r_sockaddr) != 0 && 
				descriptor->port == connection->r_sockaddr->port) {
				return connection;
			}
		}
	}

	return NULL;
}

static apt_bool_t mrcp_client_agent_connection_remove(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	/* remove from the list */
	APR_RING_REMOVE(connection,link);

	if(connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close TCP/MRCPv2 Connection %s",connection->id);
		apt_poller_task_descriptor_remove(agent->task,&connection->sock_pfd);
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
		if(APR_RING_EMPTY(&agent->connection_list, mrcp_connection_t, link)) {
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
					apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Found No Existing TCP/MRCPv2 Connection");
				}
			}
			if(!connection) {
				/* create new connection */
				connection = mrcp_client_agent_connection_create(agent,descriptor);
				if(!connection) {
					apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Failed to Establish TCP/MRCPv2 Connection");
				}
			}

			if(connection) {
				mrcp_connection_channel_add(connection,channel);
				apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,channel->log_obj,"Add Control Channel <%s> %s [%d]",
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
		apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,channel->log_obj,"Remove Control Channel <%s> [%d]",
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

static apt_bool_t mrcp_client_agent_request_cancel(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	mrcp_message_t *response;
	apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Cancel MRCP Request <%s@%s> [%d]",
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	response = mrcp_response_create(message,message->pool);
	response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
	return mrcp_connection_message_receive(agent->vtable,channel,response);
}

static apt_bool_t mrcp_client_agent_disconnect_raise(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	mrcp_control_channel_t *channel;
	void *val;
	apr_hash_index_t *it = apr_hash_first(connection->pool,connection->channel_table);
	/* walk through the list of channels and raise disconnect event for them */
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		channel = val;
		if(!channel) continue;

		if(channel->active_request) {
			mrcp_client_agent_request_cancel(channel->agent,channel,channel->active_request);
			channel->active_request = NULL;
			if(channel->request_timer) {
				apt_timer_kill(channel->request_timer);
			}
		}
		else if(agent->vtable->on_disconnect){
			agent->vtable->on_disconnect(channel);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_agent_messsage_send(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	apt_bool_t status = FALSE;
	mrcp_connection_t *connection = channel->connection;
	apt_text_stream_t stream;
	apt_message_status_e result;

	if(!connection || !connection->sock) {
		apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Null MRCPv2 Connection "APT_SIDRES_FMT,MRCP_MESSAGE_SIDRES(message));
		mrcp_client_agent_request_cancel(agent,channel,message);
		return FALSE;
	}

	do {
		apt_text_stream_init(&stream,connection->tx_buffer,connection->tx_buffer_size);
		result = mrcp_generator_run(connection->generator,message,&stream);
		if(result != APT_MESSAGE_STATUS_INVALID) {
			stream.text.length = stream.pos - stream.text.buf;
			*stream.pos = '\0';

			apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,channel->log_obj,"Send MRCPv2 Data %s [%"APR_SIZE_T_FMT" bytes]\n%.*s",
				connection->id,
				stream.text.length,
				connection->verbose == TRUE ? stream.text.length : 0,
				stream.text.buf);

			if(apr_socket_send(connection->sock,stream.text.buf,&stream.text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Failed to Send MRCPv2 Data %s",
					connection->id);
			}
		}
		else {
			apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Failed to Generate MRCPv2 Data %s",
				connection->id);
		}
	}
	while(result == APT_MESSAGE_STATUS_INCOMPLETE);

	if(status == TRUE) {
		channel->active_request = message;
		if(channel->request_timer && agent->request_timeout) {
			apt_timer_set(channel->request_timer,agent->request_timeout);
		}
	}
	else {
		mrcp_client_agent_request_cancel(agent,channel,message);
	}
	return status;
}

static apt_bool_t mrcp_client_message_handler(mrcp_connection_t *connection, mrcp_message_t *message, apt_message_status_e status)
{
	if(status == APT_MESSAGE_STATUS_COMPLETE) {
		/* message is completely parsed */
		mrcp_control_channel_t *channel;
		apt_str_t identifier;
		apt_id_resource_generate(&message->channel_id.session_id,&message->channel_id.resource_name,'@',&identifier,message->pool);
		channel = mrcp_connection_channel_find(connection,&identifier);
		if(channel) {
			mrcp_connection_agent_t *agent = connection->agent;
			if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
				if(!channel->active_request || 
					channel->active_request->start_line.request_id != message->start_line.request_id) {
					apt_obj_log(APT_LOG_MARK,APT_PRIO_WARNING,channel->log_obj,"Unexpected MRCP Response "APT_SIDRES_FMT" [%d]",
						MRCP_MESSAGE_SIDRES(message),
						message->start_line.request_id);
					return FALSE;
				}
				if(channel->request_timer) {
					apt_timer_kill(channel->request_timer);
				}
				channel->active_request = NULL;
			}

			mrcp_connection_message_receive(agent->vtable,channel,message);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Find Channel "APT_SIDRES_FMT" in Connection %s [%d]",
				MRCP_MESSAGE_SIDRES(message),
				connection->id,
				apr_hash_count(connection->channel_table));
		}
	}
	return TRUE;
}

/* Receive MRCP message through TCP/MRCPv2 connection */
static apt_bool_t mrcp_client_poller_signal_process(void *obj, const apr_pollfd_t *descriptor)
{
	mrcp_connection_agent_t *agent = obj;
	mrcp_connection_t *connection = descriptor->client_data;
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;
	mrcp_message_t *message;
	apt_message_status_e msg_status;

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
		apt_poller_task_descriptor_remove(agent->task,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;

		mrcp_client_agent_disconnect_raise(agent,connection);
		return TRUE;
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
		if(mrcp_client_message_handler(connection,message,msg_status) == FALSE) {
			return FALSE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	/* scroll remaining stream */
	apt_text_stream_scroll(stream);
	return TRUE;
}

/* Process task message */
static apt_bool_t mrcp_client_agent_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_poller_task_t *poller_task = apt_task_object_get(task);
	mrcp_connection_agent_t *agent = apt_poller_task_object_get(poller_task);
	connection_task_msg_t *msg = (connection_task_msg_t*) task_msg->data;

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
	}

	return TRUE;
}

/* Timer callback */
static void mrcp_client_timer_proc(apt_timer_t *timer, void *obj)
{
	mrcp_control_channel_t *channel = obj;
	if(!channel) {
		return;
	}

	if(channel->request_timer == timer) {
		if(channel->active_request) {
			mrcp_client_agent_request_cancel(channel->agent,channel,channel->active_request);
			channel->active_request = NULL;
		}
	}
}
