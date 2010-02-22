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
#include "rtsp_client.h"
#include "rtsp_stream.h"
#include "apt_net_client_task.h"
#include "apt_text_stream.h"
#include "apt_pool.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define RTSP_STREAM_BUFFER_SIZE 1024

typedef struct rtsp_client_connection_t rtsp_client_connection_t;

typedef enum {
	TERMINATION_STATE_NONE,
	TERMINATION_STATE_REQUESTED,
	TERMINATION_STATE_INPROGRESS
} termination_state_e;

/** RTSP client */
struct rtsp_client_t {
	apr_pool_t                 *pool;
	apt_net_client_task_t      *task;

	apr_pool_t                 *sub_pool;
	apt_obj_list_t             *connection_list;

	void                       *obj;
	const rtsp_client_vtable_t *vtable;
};

/** RTSP connection */
struct rtsp_client_connection_t {
	/** Connection base */
	apt_net_client_connection_t *base;
	/** RTSP client, connection belongs to */
	rtsp_client_t               *client;
	/** Element of the connection list in agent */
	apt_list_elem_t             *it;

	/** Handle table (rtsp_client_session_t*) */
	apr_hash_t                  *handle_table;
	/** Session table (rtsp_client_session_t*) */
	apr_hash_t                  *session_table;
	
	/** Inprogress request/session queue (rtsp_client_session_t*) */
	apt_obj_list_t              *inprogress_request_queue;

	/** Last CSeq sent */
	apr_size_t                   last_cseq;

	char                         rx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t            rx_stream;
	rtsp_parser_t               *parser;

	char                         tx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t            tx_stream;
	rtsp_generator_t            *generator;
};

/** RTSP session */
struct rtsp_client_session_t {
	apr_pool_t               *pool;
	void                     *obj;
	
	/** Connection */
	rtsp_client_connection_t *connection;
	/** Session identifier */
	apt_str_t                 id;

	apt_str_t                 server_ip;
	apr_port_t                server_port;
	apt_str_t                 resource_location;

	/** In-progress request */
	rtsp_message_t           *active_request;
	/** Pending request queue (rtsp_message_t*) */
	apt_obj_list_t           *pending_request_queue;

	/** Resource table */
	apr_hash_t               *resource_table;

	/** termination state (none -> requested -> terminating) */
	termination_state_e       term_state;
};

typedef enum {
	TASK_MSG_SEND_MESSAGE,
	TASK_MSG_TERMINATE_SESSION
} task_msg_data_type_e;

typedef struct task_msg_data_t task_msg_data_t;

struct task_msg_data_t {
	task_msg_data_type_e   type;
	rtsp_client_t         *client;
	rtsp_client_session_t *session;
	rtsp_message_t        *message;
};

static apt_bool_t rtsp_client_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t rtsp_client_message_receive(apt_net_client_task_t *task, apt_net_client_connection_t *connection);

static const apt_net_client_vtable_t client_vtable = {
	rtsp_client_message_receive
};

static apt_bool_t rtsp_client_message_send(rtsp_client_t *client, apt_net_client_connection_t *connection, rtsp_message_t *message);
static apt_bool_t rtsp_client_session_message_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);
static apt_bool_t rtsp_client_session_request_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);
static apt_bool_t rtsp_client_session_response_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response);

/** Create RTSP client */
RTSP_DECLARE(rtsp_client_t*) rtsp_client_create(
										apr_size_t max_connection_count,
										void *obj,
										const rtsp_client_vtable_t *handler,
										apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	rtsp_client_t *client;
	
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create RTSP Client [%"APR_SIZE_T_FMT"]",max_connection_count);
	client = apr_palloc(pool,sizeof(rtsp_client_t));
	client->pool = pool;
	client->obj = obj;
	client->vtable = handler;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(task_msg_data_t),pool);

	client->task = apt_net_client_task_create(max_connection_count,client,&client_vtable,msg_pool,pool);
	if(!client->task) {
		return NULL;
	}

	vtable = apt_net_client_task_vtable_get(client->task);
	if(vtable) {
		vtable->process_msg = rtsp_client_task_msg_process;
	}

	client->sub_pool = apt_subpool_create(pool);
	client->connection_list = NULL;
	return client;
}

/** Destroy RTSP client */
RTSP_DECLARE(apt_bool_t) rtsp_client_destroy(rtsp_client_t *client)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy RTSP Client");
	return apt_net_client_task_destroy(client->task);
}

/** Start connection agent */
RTSP_DECLARE(apt_bool_t) rtsp_client_start(rtsp_client_t *client)
{
	return apt_net_client_task_start(client->task);
}

/** Terminate connection agent */
RTSP_DECLARE(apt_bool_t) rtsp_client_terminate(rtsp_client_t *client)
{
	return apt_net_client_task_terminate(client->task);
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_client_task_get(rtsp_client_t *client)
{
	return apt_net_client_task_base_get(client->task);
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_client_object_get(rtsp_client_t *client)
{
	return client->obj;
}

/** Get object associated with the session */
RTSP_DECLARE(void*) rtsp_client_session_object_get(const rtsp_client_session_t *session)
{
	return session->obj;
}

/** Set object associated with the session */
RTSP_DECLARE(void) rtsp_client_session_object_set(rtsp_client_session_t *session, void *obj)
{
	session->obj = obj;
}

/** Get the session identifier */
RTSP_DECLARE(const apt_str_t*) rtsp_client_session_id_get(const rtsp_client_session_t *session)
{
	return &session->id;
}

/** Signal task message */
static apt_bool_t rtsp_client_control_message_signal(
								task_msg_data_type_e type,
								rtsp_client_t *client,
								rtsp_client_session_t *session,
								rtsp_message_t *message)
{
	apt_task_t *task = apt_net_client_task_base_get(client->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		task_msg_data_t *data = (task_msg_data_t*)task_msg->data;
		data->type = type;
		data->client = client;
		data->session = session;
		data->message = message;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
}

/** Create RTSP session handle */
RTSP_DECLARE(rtsp_client_session_t*) rtsp_client_session_create(
											rtsp_client_t *client,
											const char *server_ip, 
											apr_port_t server_port,
											const char *resource_location)
{
	rtsp_client_session_t *session;
	apr_pool_t *pool = apt_pool_create();
	session = apr_palloc(pool,sizeof(rtsp_client_session_t));
	session->pool = pool;
	session->obj = NULL;
	session->connection = NULL;
	session->active_request = NULL;
	session->pending_request_queue = apt_list_create(pool);
	session->resource_table = apr_hash_make(pool);
	session->term_state = TERMINATION_STATE_NONE;

	apt_string_assign(&session->server_ip,server_ip,pool);
	session->server_port = server_port;
	apt_string_assign(&session->resource_location,resource_location,pool);
	apt_string_reset(&session->id);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create RTSP Handle "APT_PTR_FMT,session);
	return session;
}

/** Destroy RTSP session handle */
RTSP_DECLARE(void) rtsp_client_session_destroy(rtsp_client_session_t *session)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy RTSP Handle "APT_PTR_FMT,session);
	if(session && session->pool) {
		apr_pool_destroy(session->pool);
	}
}

/** Signal terminate request */
RTSP_DECLARE(apt_bool_t) rtsp_client_session_terminate(rtsp_client_t *client, rtsp_client_session_t *session)
{
	return rtsp_client_control_message_signal(TASK_MSG_TERMINATE_SESSION,client,session,NULL);
}

/** Signal RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_client_session_request(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message)
{
	return rtsp_client_control_message_signal(TASK_MSG_SEND_MESSAGE,client,session,message);
}

/* Create RTSP connection */
static apt_bool_t rtsp_client_connection_create(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_client_connection_t *rtsp_connection;
	apt_net_client_connection_t *connection = apt_net_client_connect(client->task,session->server_ip.buf,session->server_port);
	if(!connection) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Connect to RTSP Server %s:%d",session->server_ip.buf,session->server_port);
		return FALSE;
	}
	rtsp_connection = apr_palloc(connection->pool,sizeof(rtsp_client_connection_t));
	rtsp_connection->handle_table = apr_hash_make(connection->pool);
	rtsp_connection->session_table = apr_hash_make(connection->pool);
	rtsp_connection->inprogress_request_queue = apt_list_create(connection->pool);
	apt_text_stream_init(&rtsp_connection->rx_stream,rtsp_connection->rx_buffer,sizeof(rtsp_connection->rx_buffer)-1);
	apt_text_stream_init(&rtsp_connection->tx_stream,rtsp_connection->tx_buffer,sizeof(rtsp_connection->tx_buffer)-1);
	rtsp_connection->parser = rtsp_parser_create(connection->pool);
	rtsp_connection->generator = rtsp_generator_create(connection->pool);
	rtsp_connection->last_cseq = 0;
	rtsp_connection->base = connection;
	connection->obj = rtsp_connection;
	if(!client->connection_list) {
		client->connection_list = apt_list_create(client->sub_pool);
	}
	rtsp_connection->client = client;
	rtsp_connection->it = apt_list_push_back(client->connection_list,rtsp_connection,connection->pool);
	session->connection = rtsp_connection;
	return TRUE;
}

/* Destroy RTSP connection */
static apt_bool_t rtsp_client_connection_destroy(rtsp_client_connection_t *rtsp_connection)
{
	rtsp_client_t *client = rtsp_connection->client;
	apt_list_elem_remove(client->connection_list,rtsp_connection->it);
	apt_net_client_disconnect(client->task,rtsp_connection->base);

	if(apt_list_is_empty(client->connection_list) == TRUE) {
		apr_pool_clear(client->sub_pool);
		client->connection_list = NULL;
	}
	return TRUE;
}

/* Respond to session termination request */
static apt_bool_t rtsp_client_session_terminate_respond(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_client_connection_t *rtsp_connection = session->connection;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove RTSP Handle "APT_PTR_FMT,session);
	apr_hash_set(rtsp_connection->handle_table,session,sizeof(session),NULL);

	session->term_state = TERMINATION_STATE_NONE;
	client->vtable->on_session_terminate_response(client,session);
	return TRUE;
}

/* Teardown session resources */
static apt_bool_t rtsp_client_session_resources_teardown(rtsp_client_t *client, rtsp_client_session_t *session)
{
	void *val;
	rtsp_message_t *setup_request;
	rtsp_message_t *teardown_request;
	apr_hash_index_t *it;

	/* set termination state to in-progress and teardown remaining resources */
	session->term_state = TERMINATION_STATE_INPROGRESS;
	it = apr_hash_first(session->pool,session->resource_table);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		setup_request = val;
		if(!setup_request) continue;

		teardown_request = rtsp_request_create(session->pool);
		teardown_request->start_line.common.request_line.resource_name = setup_request->start_line.common.request_line.resource_name;
		teardown_request->start_line.common.request_line.method_id = RTSP_METHOD_TEARDOWN;
		rtsp_client_session_message_process(client,session,teardown_request);
	}
	return TRUE;
}

/* Process session termination request */
static apt_bool_t rtsp_client_session_terminate_process(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_client_connection_t *rtsp_connection = session->connection;
	if(!rtsp_connection) {
		client->vtable->on_session_terminate_response(client,session);
		return FALSE;
	}

	if(session->active_request) {
		/* set termination state to requested */
		session->term_state = TERMINATION_STATE_REQUESTED;
	}
	else {
		rtsp_client_session_resources_teardown(client,session);
		
		/* respond immediately if no resources left */
		if(apr_hash_count(session->resource_table) == 0) {
			rtsp_client_session_terminate_respond(client,session);

			if(apr_hash_count(rtsp_connection->handle_table) == 0) {
				rtsp_client_connection_destroy(rtsp_connection);
			}
		}
	}

	return TRUE;
}

static apt_bool_t rtsp_client_session_url_generate(rtsp_client_session_t *session, rtsp_message_t *message)
{
	apt_str_t *url = &message->start_line.common.request_line.url;
	if(session->resource_location.length) {
		url->buf = apr_psprintf(message->pool,"rtsp://%s:%d/%s/%s",
						session->server_ip.buf,
						session->server_port,
						session->resource_location.buf,
						message->start_line.common.request_line.resource_name);
	}
	else {
		url->buf = apr_psprintf(message->pool,"rtsp://%s:%d/%s",
						session->server_ip.buf,
						session->server_port,
						message->start_line.common.request_line.resource_name);
	}
	url->length = strlen(url->buf);
	return TRUE;
}

static apt_bool_t rtsp_client_request_push(rtsp_client_connection_t *rtsp_connection, rtsp_client_session_t *session, rtsp_message_t *message)
{
	/* add request to inprogress request queue */
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Push RTSP Request to In-Progress Queue "APT_PTRSID_FMT" CSeq:%"APR_SIZE_T_FMT,
		session,
		message->header.session_id.buf ? message->header.session_id.buf : "new",
		message->header.cseq);
	apt_list_push_back(rtsp_connection->inprogress_request_queue,session,session->pool);
	session->active_request = message;
	return TRUE;
}

static apt_bool_t rtsp_client_request_pop(rtsp_client_connection_t *rtsp_connection, rtsp_message_t *response, rtsp_message_t **ret_request, rtsp_client_session_t **ret_session)
{
	rtsp_client_session_t *session;
	apt_list_elem_t *elem = apt_list_first_elem_get(rtsp_connection->inprogress_request_queue);
	while(elem) {
		session = apt_list_elem_object_get(elem);
		if(session->active_request && session->active_request->header.cseq == response->header.cseq) {
			if(ret_session) {
				*ret_session = session;
			}
			if(ret_request) {
				*ret_request = session->active_request;
			}
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Pop In-Progress RTSP Request "APT_PTR_FMT" CSeq:%"APR_SIZE_T_FMT, 
				session, 
				response->header.cseq);
			apt_list_elem_remove(rtsp_connection->inprogress_request_queue,elem);
			session->active_request = NULL;
			return TRUE;
		}
		elem = apt_list_next_elem_get(rtsp_connection->inprogress_request_queue,elem);
	}
	return FALSE;
}

/* Process outgoing RTSP request */
static apt_bool_t rtsp_client_session_request_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message)
{
	if(!session->connection) {
		/* create RTSP connection */
		if(rtsp_client_connection_create(client,session) == FALSE) {
			/* respond with error */
			return FALSE;
		}
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add RTSP Handle "APT_PTR_FMT,session);
		apr_hash_set(session->connection->handle_table,session,sizeof(session),session);
	}

	rtsp_client_session_url_generate(session,message);

	if(session->id.length) {
		message->header.session_id = session->id;
		rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
	}
	
	message->header.cseq = ++session->connection->last_cseq;
	rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_CSEQ);

	if(rtsp_client_message_send(client,session->connection->base,message) == FALSE) {
		/* respond with error */
		return FALSE;
	}

	return rtsp_client_request_push(session->connection,session,message);
}

/* Process pending RTSP requests */
static apt_bool_t rtsp_client_session_pending_requests_process(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_message_t *request = apt_list_pop_front(session->pending_request_queue);
	if(!request) {
		/* pending queue is empty, no in-progress request */
		return FALSE;
	}

	/* process pending request; get the next one, if current is failed */
	do {
		rtsp_message_t *response;
		if(rtsp_client_session_request_process(client,session,request) == TRUE) {
			return TRUE;
		}
			
		/* respond with error */
		response = rtsp_response_create(
							request,
							RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
							RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,
							session->pool);
		rtsp_client_session_response_process(client,session,request,response);
		
		/* process the next pending request / if any */
		request = apt_list_pop_front(session->pending_request_queue);
	}
	while(request);

	/* no in-progress request */
	return FALSE;
}


/* Process outgoing RTSP message */
static apt_bool_t rtsp_client_session_message_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message)
{
	if(session->active_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Push RTSP Request to Pending Queue "APT_PTR_FMT,session);
		apt_list_push_back(session->pending_request_queue,message,message->pool);
		return TRUE;
	}

	if(rtsp_client_session_request_process(client,session,message) == FALSE) {
		/* respond with error in case request cannot be processed */
		rtsp_message_t *response = rtsp_response_create(
							message,
							RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
							RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,
							session->pool);
		rtsp_client_session_response_process(client,session,message,response);
	}
	return TRUE;
}

/* Process incoming RTSP event (request) */
static apt_bool_t rtsp_client_session_event_process(rtsp_client_t *client, rtsp_client_connection_t *rtsp_connection, rtsp_message_t *message)
{
	rtsp_message_t *response = NULL;
	rtsp_client_session_t *session = NULL;
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
		/* find existing session */
		session = apr_hash_get(
					rtsp_connection->session_table,
					message->header.session_id.buf,
					message->header.session_id.length);
	}

	if(session) {
		response = rtsp_response_create(message,RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,message->pool);
		if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
			response->header.session_id = message->header.session_id;
			rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
		}
		client->vtable->on_session_event(client,session,message);
	}
	else {
		response = rtsp_response_create(message,RTSP_STATUS_CODE_NOT_FOUND,RTSP_REASON_PHRASE_NOT_FOUND,message->pool);
	}

	return rtsp_client_message_send(client,rtsp_connection->base,response);
}

/* Process incoming RTSP response */
static apt_bool_t rtsp_client_session_response_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response)
{
	const char *resource_name;
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP &&
		response->start_line.common.status_line.status_code == RTSP_STATUS_CODE_OK) {
		
		if(apr_hash_count(session->resource_table) == 0) {
			if(rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
				session->id = response->header.session_id;
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add RTSP Session "APT_PTRSID_FMT,
					session,
					session->id.buf);
				apr_hash_set(session->connection->session_table,session->id.buf,session->id.length,session);
			}
		}
		
		/* add resource */
		resource_name = request->start_line.common.request_line.resource_name;
		apr_hash_set(session->resource_table,resource_name,APR_HASH_KEY_STRING,request);
	}
	else if(request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		/* remove resource */
		resource_name = request->start_line.common.request_line.resource_name;
		apr_hash_set(session->resource_table,resource_name,APR_HASH_KEY_STRING,NULL);

		if(apr_hash_count(session->resource_table) == 0) {
			if(session->connection) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove RTSP Session "APT_PTRSID_FMT,
					session,
					session->id.buf);
				apr_hash_set(session->connection->session_table,session->id.buf,session->id.length,NULL);
			}
		}
	}

	if(session->term_state != TERMINATION_STATE_INPROGRESS) {
		client->vtable->on_session_response(client,session,request,response);
	}

	return TRUE;
}

/* Raise RTSP session terminate event */
static apt_bool_t rtsp_client_session_terminate_raise(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_message_t *request;
	rtsp_message_t *response;

	/* cancel pending requests */
	do {
		request = apt_list_pop_front(session->pending_request_queue);
		if(request) {
			response = rtsp_response_create(
							session->active_request,
							RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
							RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,
							session->pool);
			rtsp_client_session_response_process(client,session,request,response);
		}
	}
	while(request);

	
	if(session->term_state == TERMINATION_STATE_NONE) {
		client->vtable->on_session_terminate_event(client,session);
	}
	else {
		rtsp_client_session_terminate_respond(client,session);
	}
	return TRUE;
}

/* RTSP connection disconnected */
static apt_bool_t rtsp_client_on_disconnect(rtsp_client_t *client, rtsp_client_connection_t *rtsp_connection)
{
	rtsp_client_session_t *session;
	rtsp_message_t *request;
	rtsp_message_t *response;
	apr_size_t remaining_handles = 0;
	apr_size_t cancelled_requests = 0;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"TCP Peer Disconnected %s", rtsp_connection->base->id);
	apt_net_client_connection_close(client->task,rtsp_connection->base);

	/* Cancel in-progreess requests */
	do {
		session = apt_list_pop_front(rtsp_connection->inprogress_request_queue);
		if(session && session->active_request) {
			request = session->active_request;
			session->active_request = NULL;
			cancelled_requests++;

			response = rtsp_response_create(
								request,
								RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
								RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,
								session->pool);
			rtsp_client_session_response_process(client,session,request,response);
		}
	}
	while(session);

	/* Walk through RTSP handles and raise termination event for them */
	remaining_handles = apr_hash_count(rtsp_connection->handle_table);
	if(remaining_handles) {
		void *val;
		apr_hash_index_t *it;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Terminate Remaining RTSP Handles [%"APR_SIZE_T_FMT"]",remaining_handles);
		it = apr_hash_first(rtsp_connection->base->pool,rtsp_connection->session_table);
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,NULL,NULL,&val);
			session = val;
			if(session) {
				rtsp_client_session_terminate_raise(client,session);
			}
		}
		remaining_handles = apr_hash_count(rtsp_connection->session_table);
	}

	if(!remaining_handles && !cancelled_requests) {
		rtsp_client_connection_destroy(rtsp_connection);
	}
	return TRUE;
}

/* Send RTSP message through RTSP connection */
static apt_bool_t rtsp_client_message_send(rtsp_client_t *client, apt_net_client_connection_t *connection, rtsp_message_t *message)
{
	apt_bool_t status = FALSE;
	rtsp_client_connection_t *rtsp_connection;
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

/** return TRUE to proceed with the next message in the stream (if any) */
static apt_bool_t rtsp_client_message_handler(void *obj, rtsp_message_t *message, rtsp_stream_status_e status)
{
	rtsp_client_connection_t *rtsp_connection = obj;
	if(status != RTSP_STREAM_STATUS_COMPLETE) {
		/* message is not completely parsed, nothing to do */
		return TRUE;
	}
	/* process parsed message */
	if(message->start_line.message_type == RTSP_MESSAGE_TYPE_RESPONSE) {
		rtsp_message_t *request;
		rtsp_client_session_t *session;
		/* at first, pop in-progress request/session */
		if(rtsp_client_request_pop(rtsp_connection,message,&request,&session) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unexpected RTSP Response Received CSeq:%"APR_SIZE_T_FMT,
				message->header.cseq);
			return TRUE;
		}

		/* next, process session response */
		rtsp_client_session_response_process(rtsp_connection->client,session,request,message);

		/* process session pending requests */
		if(rtsp_client_session_pending_requests_process(rtsp_connection->client,session) == FALSE) {
			/* no in-progress request, check the termination state now */
			if(session->term_state != TERMINATION_STATE_NONE) {
				if(session->term_state == TERMINATION_STATE_REQUESTED) {
					rtsp_client_session_resources_teardown(rtsp_connection->client,session);
				}
				
				/* respond if no resources left */
				if(apr_hash_count(session->resource_table) == 0) {
					rtsp_client_session_terminate_respond(rtsp_connection->client,session);

					if(apr_hash_count(rtsp_connection->handle_table) == 0) {
						rtsp_client_connection_destroy(rtsp_connection);
						/* return FALSE to indicate connection has been destroyed */
						return FALSE;
					}
				}
			}
		}
	}
	else if(message->start_line.message_type == RTSP_MESSAGE_TYPE_REQUEST) {
		rtsp_client_session_event_process(rtsp_connection->client,rtsp_connection,message);
	}
	return TRUE;
}

/* Receive RTSP message through RTSP connection */
static apt_bool_t rtsp_client_message_receive(apt_net_client_task_t *task, apt_net_client_connection_t *connection)
{
	rtsp_client_t *client = apt_net_client_task_object_get(task);
	rtsp_client_connection_t *rtsp_connection;
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
		return rtsp_client_on_disconnect(client,rtsp_connection);
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
	return rtsp_stream_walk(rtsp_connection->parser,stream,rtsp_client_message_handler,rtsp_connection);
}

/* Process task message */
static apt_bool_t rtsp_client_task_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_net_client_task_t *net_task = apt_task_object_get(task);
	rtsp_client_t *client = apt_net_client_task_object_get(net_task);

	task_msg_data_t *data = (task_msg_data_t*) task_msg->data;
	switch(data->type) {
		case TASK_MSG_SEND_MESSAGE:
			rtsp_client_session_message_process(client,data->session,data->message);
			break;
		case TASK_MSG_TERMINATE_SESSION:
			rtsp_client_session_terminate_process(client,data->session);
			break;
	}

	return TRUE;
}
