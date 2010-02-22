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

#include <apr_hash.h>
#include "rtsp_server.h"
#include "rtsp_stream.h"
#include "apt_net_server_task.h"
#include "apt_text_stream.h"
#include "apt_pool.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define RTSP_SESSION_ID_HEX_STRING_LENGTH 16
#define RTSP_STREAM_BUFFER_SIZE 1024

typedef struct rtsp_server_connection_t rtsp_server_connection_t;

/** RTSP server */
struct rtsp_server_t {
	apr_pool_t                 *pool;
	apt_net_server_task_t      *task;

	apr_pool_t                 *sub_pool;
	apt_obj_list_t             *connection_list;

	void                       *obj;
	const rtsp_server_vtable_t *vtable;
};

/** RTSP connection */
struct rtsp_server_connection_t {
	/** Connection base */
	apt_net_server_connection_t *base;

	/** RTSP server, connection belongs to */
	rtsp_server_t               *server;
	/** Element of the connection list in agent */
	apt_list_elem_t             *it;

	/** Session table (rtsp_server_session_t*) */
	apr_hash_t                  *session_table;

	char                         rx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t            rx_stream;
	rtsp_parser_t               *parser;

	char                         tx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t            tx_stream;
	rtsp_generator_t            *generator;
};

/** RTSP session */
struct rtsp_server_session_t {
	apr_pool_t               *pool;
	void                     *obj;
	rtsp_server_connection_t *connection;

	/** Session identifier */
	apt_str_t                 id;

	/** Last cseq sent */
	apr_size_t                last_cseq;

	/** In-progress request */
	rtsp_message_t           *active_request;
	/** request queue */
	apt_obj_list_t           *request_queue;

	/** Resource table */
	apr_hash_t               *resource_table;

	/** In-progress termination request */
	apt_bool_t                terminating;
};

typedef enum {
	TASK_MSG_SEND_MESSAGE,
	TASK_MSG_TERMINATE_SESSION
} task_msg_data_type_e;

typedef struct task_msg_data_t task_msg_data_t;

struct task_msg_data_t {
	task_msg_data_type_e   type;
	rtsp_server_t         *server;
	rtsp_server_session_t *session;
	rtsp_message_t        *message;
};

static apt_bool_t rtsp_server_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t rtsp_server_on_connect(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
static apt_bool_t rtsp_server_on_disconnect(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
static apt_bool_t rtsp_server_message_receive(apt_net_server_task_t *task, apt_net_server_connection_t *connection);

static const apt_net_server_vtable_t server_vtable = {
	rtsp_server_on_connect,
	rtsp_server_on_disconnect,
	rtsp_server_message_receive
};

static apt_bool_t rtsp_server_message_send(rtsp_server_t *server, apt_net_server_connection_t *connection, rtsp_message_t *message);

/** Create RTSP server */
RTSP_DECLARE(rtsp_server_t*) rtsp_server_create(
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										void *obj,
										const rtsp_server_vtable_t *handler,
										apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	rtsp_server_t *server;

	if(!listen_ip) {
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create RTSP Server %s:%hu [%"APR_SIZE_T_FMT"]",
		listen_ip,listen_port,max_connection_count);
	server = apr_palloc(pool,sizeof(rtsp_server_t));
	server->pool = pool;
	server->obj = obj;
	server->vtable = handler;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(task_msg_data_t),pool);

	server->task = apt_net_server_task_create(
						listen_ip,listen_port,max_connection_count,
						server,&server_vtable,msg_pool,pool);
	if(!server->task) {
		return NULL;
	}

	vtable = apt_net_server_task_vtable_get(server->task);
	if(vtable) {
		vtable->process_msg = rtsp_server_task_msg_process;
	}

	server->sub_pool = apt_subpool_create(pool);
	server->connection_list = NULL;
	return server;
}

/** Destroy RTSP server */
RTSP_DECLARE(apt_bool_t) rtsp_server_destroy(rtsp_server_t *server)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy RTSP Server");
	return apt_net_server_task_destroy(server->task);
}

/** Start connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_start(rtsp_server_t *server)
{
	return apt_net_server_task_start(server->task);
}

/** Terminate connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_terminate(rtsp_server_t *server)
{
	return apt_net_server_task_terminate(server->task);
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_server_task_get(rtsp_server_t *server)
{
	return apt_net_server_task_base_get(server->task);
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_server_object_get(rtsp_server_t *server)
{
	return server->obj;
}

/** Get object associated with the session */
RTSP_DECLARE(void*) rtsp_server_session_object_get(const rtsp_server_session_t *session)
{
	return session->obj;
}

/** Set object associated with the session */
RTSP_DECLARE(void) rtsp_server_session_object_set(rtsp_server_session_t *session, void *obj)
{
	session->obj = obj;
}

/** Get the session identifier */
RTSP_DECLARE(const apt_str_t*) rtsp_server_session_id_get(const rtsp_server_session_t *session)
{
	return &session->id;
}

/** Get active request */
RTSP_DECLARE(const rtsp_message_t*) rtsp_server_session_request_get(const rtsp_server_session_t *session)
{
	return session->active_request;
}

/** Get the session destination (client) ip address */
RTSP_DECLARE(const char*) rtsp_server_session_destination_get(const rtsp_server_session_t *session)
{
	if(session->connection) {
		return session->connection->base->client_ip;
	}
	return NULL;
}

/** Signal task message */
static apt_bool_t rtsp_server_control_message_signal(
								task_msg_data_type_e type,
								rtsp_server_t *server,
								rtsp_server_session_t *session,
								rtsp_message_t *message)
{
	apt_task_t *task = apt_net_server_task_base_get(server->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		task_msg_data_t *data = (task_msg_data_t*)task_msg->data;
		data->type = type;
		data->server = server;
		data->session = session;
		data->message = message;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
}

/** Signal RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_server_session_respond(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message)
{
	return rtsp_server_control_message_signal(TASK_MSG_SEND_MESSAGE,server,session,message);
}

/** Signal terminate response */
RTSP_DECLARE(apt_bool_t) rtsp_server_session_terminate(rtsp_server_t *server, rtsp_server_session_t *session)
{
	return rtsp_server_control_message_signal(TASK_MSG_TERMINATE_SESSION,server,session,NULL);
}

/* Create RTSP session */
static rtsp_server_session_t* rtsp_server_session_create(rtsp_server_t *server)
{
	rtsp_server_session_t *session;
	apr_pool_t *pool = apt_pool_create();
	session = apr_palloc(pool,sizeof(rtsp_server_session_t));
	session->pool = pool;
	session->obj = NULL;
	session->last_cseq = 0;
	session->active_request = NULL;
	session->request_queue = apt_list_create(pool);
	session->resource_table = apr_hash_make(pool);
	session->terminating = FALSE;

	apt_unique_id_generate(&session->id,RTSP_SESSION_ID_HEX_STRING_LENGTH,pool);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create RTSP Session "APT_SID_FMT,session->id.buf);
	if(server->vtable->create_session(server,session) != TRUE) {
		apr_pool_destroy(pool);
		return NULL;
	}
	return session;
}

/* Destroy RTSP session */
static void rtsp_server_session_destroy(rtsp_server_session_t *session)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy RTSP Session "APT_SID_FMT,session->id.buf);
	if(session && session->pool) {
		apr_pool_destroy(session->pool);
	}
}

/* Finally terminate RTSP session */
static apt_bool_t rtsp_server_session_do_terminate(rtsp_server_t *server, rtsp_server_session_t *session)
{		
	rtsp_server_connection_t *rtsp_connection = session->connection;

	if(session->active_request) {
		rtsp_message_t *response = rtsp_response_create(session->active_request,
			RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,session->active_request->pool);
		if(response) {
			if(session->id.buf) {
				response->header.session_id = session->id;
				rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
			}

			if(rtsp_connection) {
				rtsp_server_message_send(server,rtsp_connection->base,response);
			}
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove RTSP Session "APT_SID_FMT,session->id.buf);
	apr_hash_set(rtsp_connection->session_table,session->id.buf,session->id.length,NULL);
	rtsp_server_session_destroy(session);

	if(rtsp_connection && !rtsp_connection->it) {
		if(apr_hash_count(rtsp_connection->session_table) == 0) {
			apt_net_server_connection_destroy(rtsp_connection->base);
		}
	}
	return TRUE;
}

static apt_bool_t rtsp_server_error_respond(rtsp_server_t *server, rtsp_server_connection_t *rtsp_connection, rtsp_message_t *request, 
											rtsp_status_code_e status_code, rtsp_reason_phrase_e reason)
{
	/* send error response to client */
	rtsp_message_t *response = rtsp_response_create(request,status_code,reason,request->pool);
	if(rtsp_server_message_send(server,rtsp_connection->base,response) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send RTSP Response");
		return FALSE;
	}
	return TRUE;
}

static apt_bool_t rtsp_server_session_terminate_request(rtsp_server_t *server, rtsp_server_session_t *session)
{
	session->terminating = TRUE;
	return server->vtable->terminate_session(server,session);
}

static apt_bool_t rtsp_server_session_message_handle(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message)
{
	if(message->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		/* remove resource */
		const char *resource_name = message->start_line.common.request_line.resource_name;
		apr_hash_set(session->resource_table,resource_name,APR_HASH_KEY_STRING,NULL);

		if(apr_hash_count(session->resource_table) == 0) {
			rtsp_server_session_terminate_request(server,session);
			return TRUE;
		}
	}

	if(server->vtable->handle_message(server,session,message) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Handle Message "APT_SID_FMT,session->id.buf);
		rtsp_server_error_respond(server,session->connection,message,
								RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
								RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR);
		return FALSE;
	}
	return TRUE;
}

/* Process incoming SETUP/DESCRIBE request */
static rtsp_server_session_t* rtsp_server_session_setup_process(rtsp_server_t *server, rtsp_server_connection_t *rtsp_connection, rtsp_message_t *message)
{
	rtsp_server_session_t *session = NULL;
	if(message->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
		/* create new session */
		session = rtsp_server_session_create(server);
		session->connection = rtsp_connection;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add RTSP Session "APT_SID_FMT,session->id.buf);
		apr_hash_set(rtsp_connection->session_table,session->id.buf,session->id.length,session);
	}
	else if(message->start_line.common.request_line.method_id == RTSP_METHOD_DESCRIBE) {
		/* create new session as a communication object */
		session = rtsp_server_session_create(server);
		session->connection = rtsp_connection;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add RTSP Session "APT_SID_FMT,session->id.buf);
		apr_hash_set(rtsp_connection->session_table,session->id.buf,session->id.length,session);
	}
	else {
		/* error case */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing RTSP Session-ID");
		rtsp_server_error_respond(server,rtsp_connection,message,
								RTSP_STATUS_CODE_BAD_REQUEST,
								RTSP_REASON_PHRASE_BAD_REQUEST);
	}
	return session;
}

/* Process incoming RTSP request */
static apt_bool_t rtsp_server_session_request_process(rtsp_server_t *server, rtsp_server_connection_t *rtsp_connection, rtsp_message_t *message)
{
	rtsp_server_session_t *session = NULL;
	if(message->start_line.message_type != RTSP_MESSAGE_TYPE_REQUEST) {
		/* received response to ANNOUNCE request/event */
		return TRUE;
	}

	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID) != TRUE) {
		/* no session-id specified */
		session = rtsp_server_session_setup_process(server,rtsp_connection,message);
		if(session) {
			session->active_request = message;
			if(rtsp_server_session_message_handle(server,session,message) != TRUE) {
				rtsp_server_session_destroy(session);
			}
		}
		return TRUE;
	}

	/* existing session */
	session = apr_hash_get(
				rtsp_connection->session_table,
				message->header.session_id.buf,
				message->header.session_id.length);
	if(!session) {
		/* error case, no such session */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such RTSP Session "APT_SID_FMT,message->header.session_id.buf);
		return rtsp_server_error_respond(server,rtsp_connection,message,
								RTSP_STATUS_CODE_NOT_FOUND,
								RTSP_REASON_PHRASE_NOT_FOUND);
	}
	
	if(session->terminating == TRUE) {
		/* error case, session is being terminated */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Not Acceptable Request "APT_SID_FMT,message->header.session_id.buf);
		return rtsp_server_error_respond(server,rtsp_connection,message,
								RTSP_STATUS_CODE_NOT_ACCEPTABLE,
								RTSP_REASON_PHRASE_NOT_ACCEPTABLE);
	}
	
	if(session->active_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Push RTSP Request to Queue "APT_SID_FMT,session->id.buf);
		apt_list_push_back(session->request_queue,message,message->pool);
		return TRUE;
	}

	/* handle the request */
	session->active_request = message;
	rtsp_server_session_message_handle(server,session,message);
	return TRUE;
}

/* Process outgoing RTSP response */
static apt_bool_t rtsp_server_session_response_process(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message)
{
	apt_bool_t terminate = FALSE;
	rtsp_message_t *request = NULL;
	if(message->start_line.message_type == RTSP_MESSAGE_TYPE_REQUEST) {
		/* RTSP ANNOUNCE request (asynch event) */
		const char *resource_name = message->start_line.common.request_line.resource_name;
		if(resource_name) {
			request = apr_hash_get(session->resource_table,resource_name,APR_HASH_KEY_STRING);
		}
		if(!request) {
			return FALSE;
		}
		message->start_line.common.request_line.url = request->start_line.common.request_line.url;
		message->header.cseq = session->last_cseq;
		rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_CSEQ);
		
		if(session->id.buf) {
			message->header.session_id = session->id;
			rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
		}
		rtsp_server_message_send(server,session->connection->base,message);
		return TRUE;
	}

	if(!session->active_request) {
		/* unexpected response */
		return FALSE;
	}

	request = session->active_request;
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_DESCRIBE) {
		terminate = TRUE;
	}
	else {
		if(session->id.buf) {
			message->header.session_id = session->id;
			rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
		}
		if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
			if(message->start_line.common.status_line.status_code == RTSP_STATUS_CODE_OK) {
				/* add resource */
				const char *resource_name = request->start_line.common.request_line.resource_name;
				apr_hash_set(session->resource_table,resource_name,APR_HASH_KEY_STRING,request);
			}
			else if(apr_hash_count(session->resource_table) == 0) {
				terminate = TRUE;
			}
		}
	}

	session->last_cseq = message->header.cseq;
	rtsp_server_message_send(server,session->connection->base,message);

	if(terminate == TRUE) {
		session->active_request = NULL;
		rtsp_server_session_terminate_request(server,session);
		return TRUE;
	}

	session->active_request = apt_list_pop_front(session->request_queue);
	if(session->active_request) {
		rtsp_server_session_message_handle(server,session,session->active_request);
	}
	return TRUE;
}

/* Send RTSP message through RTSP connection */
static apt_bool_t rtsp_server_message_send(rtsp_server_t *server, apt_net_server_connection_t *connection, rtsp_message_t *message)
{
	apt_bool_t status = FALSE;
	rtsp_server_connection_t *rtsp_connection;
	apt_text_stream_t *stream;
	rtsp_stream_status_e result;

	if(!connection || !connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No RTSP Connection");
		return FALSE;
	}
	rtsp_connection = connection->obj;
	stream = &rtsp_connection->tx_stream;
		
	rtsp_generator_message_set(rtsp_connection->generator,message);
	do {
		stream->text.length = sizeof(rtsp_connection->tx_buffer)-1;
		apt_text_stream_reset(stream);
		result = rtsp_generator_run(rtsp_connection->generator,stream);
		if(result != RTSP_STREAM_STATUS_INVALID) {
			stream->text.length = stream->pos - stream->text.buf;
			*stream->pos = '\0';

			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send RTSP Stream %s [%lu bytes]\n%s",
				connection->id,
				stream->text.length,
				stream->text.buf);
			if(apr_socket_send(connection->sock,stream->text.buf,&stream->text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send RTSP Stream");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate RTSP Stream");
		}
	}
	while(result == RTSP_STREAM_STATUS_INCOMPLETE);

	return status;
}

static apt_bool_t rtsp_server_message_handler(void *obj, rtsp_message_t *message, rtsp_stream_status_e status)
{
	rtsp_server_connection_t *rtsp_connection = obj;
	if(status == RTSP_STREAM_STATUS_COMPLETE) {
		/* message is completely parsed */
		apt_str_t *destination;
		rtsp_message_t *message = rtsp_parser_message_get(rtsp_connection->parser);
		destination = &message->header.transport.destination;
		if(!destination->buf && rtsp_connection->base->client_ip) {
			apt_string_assign(destination,rtsp_connection->base->client_ip,rtsp_connection->base->pool);
		}
		rtsp_server_session_request_process(rtsp_connection->server,rtsp_connection,message);
	}
	else if(status == RTSP_STREAM_STATUS_INVALID) {
		/* error case */
		rtsp_message_t *response;
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse RTSP Stream");
		if(message) {
			response = rtsp_response_create(message,RTSP_STATUS_CODE_BAD_REQUEST,
									RTSP_REASON_PHRASE_BAD_REQUEST,message->pool);
			if(rtsp_server_message_send(rtsp_connection->server,rtsp_connection->base,response) == FALSE) {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send RTSP Response");
			}
		}
	}
	return TRUE;
}

/* Receive RTSP message through RTSP connection */
static apt_bool_t rtsp_server_message_receive(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	rtsp_server_connection_t *rtsp_connection;
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;

	if(!connection || !connection->sock) {
		return FALSE;
	}
	rtsp_connection = connection->obj;
	stream = &rtsp_connection->rx_stream;

	/* init length of the stream */
	stream->text.length = sizeof(rtsp_connection->rx_buffer)-1;
	/* calculate offset remaining from the previous receive / if any */
	offset = stream->pos - stream->text.buf;
	/* calculate available length */
	length = stream->text.length - offset;
	status = apr_socket_recv(connection->sock,stream->pos,&length);
	if(status == APR_EOF || length == 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"TCP Peer Disconnected %s",connection->id);
		return apt_net_server_connection_close(task,connection);
	}
	/* calculate actual length of the stream */
	stream->text.length = offset + length;
	stream->pos[length] = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive RTSP Stream %s [%lu bytes]\n%s",
		connection->id,
		length,
		stream->pos);

	/* reset pos */
	apt_text_stream_reset(stream);
	/* walk through the stream parsing RTSP messages */
	return rtsp_stream_walk(rtsp_connection->parser,stream,rtsp_server_message_handler,rtsp_connection);
}

/* New RTSP connection accepted */
static apt_bool_t rtsp_server_on_connect(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	rtsp_server_t *server = apt_net_server_task_object_get(task);
	rtsp_server_connection_t *rtsp_connection = apr_palloc(connection->pool,sizeof(rtsp_server_connection_t));
	rtsp_connection->session_table = apr_hash_make(connection->pool);
	apt_text_stream_init(&rtsp_connection->rx_stream,rtsp_connection->rx_buffer,sizeof(rtsp_connection->rx_buffer)-1);
	apt_text_stream_init(&rtsp_connection->tx_stream,rtsp_connection->tx_buffer,sizeof(rtsp_connection->tx_buffer)-1);
	rtsp_connection->parser = rtsp_parser_create(connection->pool);
	rtsp_connection->generator = rtsp_generator_create(connection->pool);
	rtsp_connection->base = connection;
	connection->obj = rtsp_connection;
	if(!server->connection_list) {
		server->connection_list = apt_list_create(server->sub_pool);
	}
	rtsp_connection->server = server;
	rtsp_connection->it = apt_list_push_back(server->connection_list,rtsp_connection,connection->pool);
	return TRUE;
}

/* RTSP connection disconnected */
static apt_bool_t rtsp_server_on_disconnect(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	apr_size_t remaining_sessions = 0;
	rtsp_server_t *server = apt_net_server_task_object_get(task);
	rtsp_server_connection_t *rtsp_connection = connection->obj;
	apt_list_elem_remove(server->connection_list,rtsp_connection->it);
	rtsp_connection->it = NULL;
	if(apt_list_is_empty(server->connection_list) == TRUE) {
		apr_pool_clear(server->sub_pool);
		server->connection_list = NULL;
	}

	remaining_sessions = apr_hash_count(rtsp_connection->session_table);
	if(remaining_sessions) {
		rtsp_server_session_t *session;
		void *val;
		apr_hash_index_t *it;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Terminate Remaining RTSP Sessions [%"APR_SIZE_T_FMT"]",
			remaining_sessions);
		it = apr_hash_first(connection->pool,rtsp_connection->session_table);
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,NULL,NULL,&val);
			session = val;
			if(session && session->terminating == FALSE) {
				rtsp_server_session_terminate_request(server,session);
			}
		}
	}
	else {
		apt_net_server_connection_destroy(connection);
	}
	return TRUE;
}

/* Process task message */
static apt_bool_t rtsp_server_task_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_net_server_task_t *net_task = apt_task_object_get(task);
	rtsp_server_t *server = apt_net_server_task_object_get(net_task);

	task_msg_data_t *data = (task_msg_data_t*) task_msg->data;
	switch(data->type) {
		case TASK_MSG_SEND_MESSAGE:
			rtsp_server_session_response_process(server,data->session,data->message);
			break;
		case TASK_MSG_TERMINATE_SESSION:
			rtsp_server_session_do_terminate(server,data->session);
			break;
	}

	return TRUE;
}
