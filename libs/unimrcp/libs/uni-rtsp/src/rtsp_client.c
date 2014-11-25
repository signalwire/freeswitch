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
 * $Id: rtsp_client.c 2249 2014-11-19 05:26:24Z achaloyan@gmail.com $
 */

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include <apr_hash.h>
#include "rtsp_client.h"
#include "rtsp_stream.h"
#include "apt_poller_task.h"
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
	apt_poller_task_t          *task;

	/** List (ring) of RTSP connections */
	APR_RING_HEAD(rtsp_client_connection_head_t, rtsp_client_connection_t) connection_list;

	apr_uint32_t                request_timeout;

	void                       *obj;
	const rtsp_client_vtable_t *vtable;
};

/** RTSP connection */
struct rtsp_client_connection_t {
	/** Ring entry */
	APR_RING_ENTRY(rtsp_client_connection_t) link;

	/** Memory pool */
	apr_pool_t       *pool;
	/** Connected socket */
	apr_socket_t     *sock;
	/** Socket poll descriptor */
	apr_pollfd_t      sock_pfd;
	/** String identifier used for traces */
	const char       *id;
	/** RTSP client, connection belongs to */
	rtsp_client_t    *client;

	/** Handle table (rtsp_client_session_t*) */
	apr_hash_t       *handle_table;
	/** Session table (rtsp_client_session_t*) */
	apr_hash_t       *session_table;
	
	/** Inprogress request/session queue (rtsp_client_session_t*) */
	apt_obj_list_t   *inprogress_request_queue;

	/** Last CSeq sent */
	apr_size_t        last_cseq;

	char              rx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t rx_stream;
	rtsp_parser_t    *parser;

	char              tx_buffer[RTSP_STREAM_BUFFER_SIZE];
	apt_text_stream_t tx_stream;
	rtsp_generator_t *generator;
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

	/** Timer used for request timeouts */
	apt_timer_t              *request_timer;

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

static apt_bool_t rtsp_client_poller_signal_process(void *obj, const apr_pollfd_t *descriptor);

static apt_bool_t rtsp_client_message_handler(rtsp_client_connection_t *rtsp_connection, rtsp_message_t *message, apt_message_status_e status);
static apt_bool_t rtsp_client_message_send(rtsp_client_t *client, rtsp_client_connection_t *connection, rtsp_message_t *message);
static apt_bool_t rtsp_client_session_message_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);
static apt_bool_t rtsp_client_session_response_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response);

static void rtsp_client_timer_proc(apt_timer_t *timer, void *obj);

/** Get string identifier */
static const char* rtsp_client_id_get(const rtsp_client_t *client)
{
	apt_task_t *task = apt_poller_task_base_get(client->task);
	return apt_task_name_get(task);
}

/** Create RTSP client */
RTSP_DECLARE(rtsp_client_t*) rtsp_client_create(
									const char *id,
									apr_size_t max_connection_count,
									apr_size_t request_timeout,
									void *obj,
									const rtsp_client_vtable_t *handler,
									apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;
	rtsp_client_t *client;

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create RTSP Client [%s] [%"APR_SIZE_T_FMT"]",
			id, max_connection_count);
	client = apr_palloc(pool,sizeof(rtsp_client_t));
	client->pool = pool;
	client->obj = obj;
	client->vtable = handler;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(task_msg_data_t),pool);

	client->task = apt_poller_task_create(
						max_connection_count,
						rtsp_client_poller_signal_process,
						client,
						msg_pool,
						pool);
	if(!client->task) {
		return NULL;
	}

	task = apt_poller_task_base_get(client->task);
	if(task) {
		apt_task_name_set(task,id);
	}

	vtable = apt_poller_task_vtable_get(client->task);
	if(vtable) {
		vtable->process_msg = rtsp_client_task_msg_process;
	}

	APR_RING_INIT(&client->connection_list, rtsp_client_connection_t, link);
	client->request_timeout = (apr_uint32_t)request_timeout;
	return client;
}

/** Destroy RTSP client */
RTSP_DECLARE(apt_bool_t) rtsp_client_destroy(rtsp_client_t *client)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy RTSP Client [%s]",
			rtsp_client_id_get(client));
	return apt_poller_task_destroy(client->task);
}

/** Start connection agent */
RTSP_DECLARE(apt_bool_t) rtsp_client_start(rtsp_client_t *client)
{
	return apt_poller_task_start(client->task);
}

/** Terminate connection agent */
RTSP_DECLARE(apt_bool_t) rtsp_client_terminate(rtsp_client_t *client)
{
	return apt_poller_task_terminate(client->task);
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_client_task_get(const rtsp_client_t *client)
{
	return apt_poller_task_base_get(client->task);
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_client_object_get(const rtsp_client_t *client)
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
	apt_task_t *task = apt_poller_task_base_get(client->task);
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
	session->request_timer = apt_poller_task_timer_create(
								client->task,
								rtsp_client_timer_proc,
								session,
								pool);
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


/** Create connection */
static apt_bool_t rtsp_client_connect(rtsp_client_t *client, rtsp_client_connection_t *connection, const char *ip, apr_port_t port)
{
	char *local_ip = NULL;
	char *remote_ip = NULL;
	apr_sockaddr_t *l_sockaddr = NULL;
	apr_sockaddr_t *r_sockaddr = NULL;

	if(apr_sockaddr_info_get(&r_sockaddr,ip,APR_INET,port,0,connection->pool) != APR_SUCCESS) {
		return FALSE;
	}

	if(apr_socket_create(&connection->sock,r_sockaddr->family,SOCK_STREAM,APR_PROTO_TCP,connection->pool) != APR_SUCCESS) {
		return FALSE;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock,r_sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		connection->sock = NULL;
		return FALSE;
	}

	if(apr_socket_addr_get(&l_sockaddr,APR_LOCAL,connection->sock) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		connection->sock = NULL;
		return FALSE;
	}

	apr_sockaddr_ip_get(&local_ip,l_sockaddr);
	apr_sockaddr_ip_get(&remote_ip,r_sockaddr);
	connection->id = apr_psprintf(connection->pool,"%s:%hu <-> %s:%hu",
		local_ip,l_sockaddr->port,
		remote_ip,r_sockaddr->port);

	memset(&connection->sock_pfd,0,sizeof(apr_pollfd_t));
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_poller_task_descriptor_add(client->task,&connection->sock_pfd) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add to Pollset %s",connection->id);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Established RTSP Connection %s",connection->id);
	return TRUE;
}

/** Close connection */
static apt_bool_t rtsp_client_connection_close(rtsp_client_t *client, rtsp_client_connection_t *connection)
{
	if(connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close RTSP Connection %s",connection->id);
		apt_poller_task_descriptor_remove(client->task,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	return TRUE;
}


/* Create RTSP connection */
static apt_bool_t rtsp_client_connection_create(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_client_connection_t *rtsp_connection;
	apr_pool_t *pool = apt_pool_create();
	if(!pool) {
		return FALSE;
	}

	rtsp_connection = apr_palloc(pool,sizeof(rtsp_client_connection_t));
	rtsp_connection->pool = pool;
	rtsp_connection->sock = NULL;
	APR_RING_ELEM_INIT(rtsp_connection,link);

	if(rtsp_client_connect(client,rtsp_connection,session->server_ip.buf,session->server_port) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Connect to RTSP Server %s:%hu",
			session->server_ip.buf,session->server_port);
		apr_pool_destroy(pool);
		return FALSE;
	}
	rtsp_connection->handle_table = apr_hash_make(pool);
	rtsp_connection->session_table = apr_hash_make(pool);
	rtsp_connection->inprogress_request_queue = apt_list_create(pool);
	apt_text_stream_init(&rtsp_connection->rx_stream,rtsp_connection->rx_buffer,sizeof(rtsp_connection->rx_buffer)-1);
	apt_text_stream_init(&rtsp_connection->tx_stream,rtsp_connection->tx_buffer,sizeof(rtsp_connection->tx_buffer)-1);
	rtsp_connection->parser = rtsp_parser_create(pool);
	rtsp_connection->generator = rtsp_generator_create(pool);
	rtsp_connection->last_cseq = 0;

	rtsp_connection->client = client;
	APR_RING_INSERT_TAIL(&client->connection_list,rtsp_connection,rtsp_client_connection_t,link);
	session->connection = rtsp_connection;
	return TRUE;
}

/* Destroy RTSP connection */
static apt_bool_t rtsp_client_connection_destroy(rtsp_client_connection_t *rtsp_connection)
{
	rtsp_client_t *client = rtsp_connection->client;
	APR_RING_REMOVE(rtsp_connection,link);
	rtsp_client_connection_close(client,rtsp_connection);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy RTSP Connection %s",rtsp_connection->id);
	apr_pool_destroy(rtsp_connection->pool);

	return TRUE;
}

/* Respond to session termination request */
static apt_bool_t rtsp_client_session_terminate_respond(rtsp_client_t *client, rtsp_client_session_t *session)
{
	rtsp_client_connection_t *rtsp_connection = session->connection;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove RTSP Handle "APT_PTR_FMT,session);
	apr_hash_set(rtsp_connection->handle_table,session,sizeof(void*),NULL);

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
		url->buf = apr_psprintf(message->pool,"rtsp://%s:%hu/%s/%s",
						session->server_ip.buf,
						session->server_port,
						session->resource_location.buf,
						message->start_line.common.request_line.resource_name);
	}
	else {
		url->buf = apr_psprintf(message->pool,"rtsp://%s:%hu/%s",
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
	if(rtsp_connection->client->request_timeout) {
		apt_timer_set(session->request_timer,rtsp_connection->client->request_timeout);
	}
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
			apt_timer_kill(session->request_timer);
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
		apr_hash_set(session->connection->handle_table,session,sizeof(void*),session);
	}

	rtsp_client_session_url_generate(session,message);

	if(session->id.length) {
		message->header.session_id = session->id;
		rtsp_header_property_add(&message->header,RTSP_HEADER_FIELD_SESSION_ID,message->pool);
	}

	message->header.cseq = ++session->connection->last_cseq;
	rtsp_header_property_add(&message->header,RTSP_HEADER_FIELD_CSEQ,message->pool);

	if(rtsp_client_message_send(client,session->connection,message) == FALSE) {
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
	if(rtsp_header_property_check(&message->header,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
		/* find existing session */
		session = apr_hash_get(
					rtsp_connection->session_table,
					message->header.session_id.buf,
					message->header.session_id.length);
	}

	if(session) {
		response = rtsp_response_create(message,RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,message->pool);
		if(rtsp_header_property_check(&message->header,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
			response->header.session_id = message->header.session_id;
			rtsp_header_property_add(&response->header,RTSP_HEADER_FIELD_SESSION_ID,message->pool);
		}
		client->vtable->on_session_event(client,session,message);
	}
	else {
		response = rtsp_response_create(message,RTSP_STATUS_CODE_NOT_FOUND,RTSP_REASON_PHRASE_NOT_FOUND,message->pool);
	}

	return rtsp_client_message_send(client,rtsp_connection,response);
}

/* Process incoming RTSP response */
static apt_bool_t rtsp_client_session_response_process(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response)
{
	const char *resource_name;
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP &&
		response->start_line.common.status_line.status_code == RTSP_STATUS_CODE_OK) {

		if(apr_hash_count(session->resource_table) == 0) {
			if(rtsp_header_property_check(&response->header,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
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

/* Cancel RTSP request */
static apt_bool_t rtsp_client_request_cancel(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_status_code_e status_code, rtsp_reason_phrase_e reason)
{
	rtsp_message_t *request;
	rtsp_message_t *response;
	if(!session->active_request) {
		return FALSE;
	}

	request = session->active_request;
	response = rtsp_response_create(
						request,
						status_code,
						reason,
						session->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Cancel RTSP Request "APT_PTRSID_FMT" CSeq:%"APR_SIZE_T_FMT" [%d]",
		session,
		request->header.session_id.buf ? request->header.session_id.buf : "new",
		request->header.cseq,
		status_code);
	
	return rtsp_client_message_handler(session->connection, response, APT_MESSAGE_STATUS_COMPLETE);
}

/* RTSP connection disconnected */
static apt_bool_t rtsp_client_on_disconnect(rtsp_client_t *client, rtsp_client_connection_t *rtsp_connection)
{
	rtsp_client_session_t *session;
	apr_size_t remaining_handles;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"RTSP Peer Disconnected %s", rtsp_connection->id);
	rtsp_client_connection_close(client,rtsp_connection);

	/* Cancel in-progreess requests */
	do {
		session = apt_list_pop_front(rtsp_connection->inprogress_request_queue);
		if(session) {
			if(rtsp_client_request_cancel(
						client,
						session,
						RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,
						RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR) == TRUE) {
				apt_timer_kill(session->request_timer);
			}
		}
	}
	while(session);

	/* Walk through RTSP handles and raise termination event for them */
	remaining_handles = apr_hash_count(rtsp_connection->handle_table);
	if(remaining_handles) {
		void *val;
		apr_hash_index_t *it;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Terminate Remaining RTSP Handles [%"APR_SIZE_T_FMT"]",remaining_handles);
		it = apr_hash_first(rtsp_connection->pool,rtsp_connection->session_table);
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,NULL,NULL,&val);
			session = val;
			if(session) {
				rtsp_client_session_terminate_raise(client,session);
			}
		}
	}

	return TRUE;
}

/* Send RTSP message through RTSP connection */
static apt_bool_t rtsp_client_message_send(rtsp_client_t *client, rtsp_client_connection_t *rtsp_connection, rtsp_message_t *message)
{
	apt_bool_t status = FALSE;
	apt_text_stream_t *stream;
	apt_message_status_e result;

	if(!rtsp_connection || !rtsp_connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No RTSP Connection");
		return FALSE;
	}
	stream = &rtsp_connection->tx_stream;

	do {
		stream->text.length = sizeof(rtsp_connection->tx_buffer)-1;
		apt_text_stream_reset(stream);
		result = rtsp_generator_run(rtsp_connection->generator,message,stream);
		if(result != APT_MESSAGE_STATUS_INVALID) {
			stream->text.length = stream->pos - stream->text.buf;
			*stream->pos = '\0';

			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send RTSP Data %s [%"APR_SIZE_T_FMT" bytes]\n%s",
				rtsp_connection->id,
				stream->text.length,
				stream->text.buf);
			if(apr_socket_send(rtsp_connection->sock,stream->text.buf,&stream->text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send RTSP Data");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate RTSP Data");
		}
	}
	while(result == APT_MESSAGE_STATUS_INCOMPLETE);

	return status;
}

/** Return TRUE to proceed with the next message in the stream (if any) */
static apt_bool_t rtsp_client_message_handler(rtsp_client_connection_t *rtsp_connection, rtsp_message_t *message, apt_message_status_e status)
{
	if(status != APT_MESSAGE_STATUS_COMPLETE) {
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
static apt_bool_t rtsp_client_poller_signal_process(void *obj, const apr_pollfd_t *descriptor)
{
	rtsp_client_t *client = obj;
	rtsp_client_connection_t *rtsp_connection = descriptor->client_data;
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;
	rtsp_message_t *message;
	apt_message_status_e msg_status;

	if(!rtsp_connection || !rtsp_connection->sock) {
		return FALSE;
	}
	stream = &rtsp_connection->rx_stream;

	/* calculate offset remaining from the previous receive / if any */
	offset = stream->pos - stream->text.buf;
	/* calculate available length */
	length = sizeof(rtsp_connection->rx_buffer) - 1 - offset;

	status = apr_socket_recv(rtsp_connection->sock,stream->pos,&length);
	if(status == APR_EOF || length == 0) {
		return rtsp_client_on_disconnect(client,rtsp_connection);
	}

	/* calculate actual length of the stream */
	stream->text.length = offset + length;
	stream->pos[length] = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive RTSP Data %s [%"APR_SIZE_T_FMT" bytes]\n%s",
		rtsp_connection->id,
		length,
		stream->pos);

	/* reset pos */
	apt_text_stream_reset(stream);

	do {
		msg_status = rtsp_parser_run(rtsp_connection->parser,stream,&message);
		if(rtsp_client_message_handler(rtsp_connection,message,msg_status) == FALSE) {
			return FALSE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	/* scroll remaining stream */
	apt_text_stream_scroll(stream);
	return TRUE;
}

/* Process task message */
static apt_bool_t rtsp_client_task_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_poller_task_t *poller_task = apt_task_object_get(task);
	rtsp_client_t *client = apt_poller_task_object_get(poller_task);

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

/* Timer callback */
static void rtsp_client_timer_proc(apt_timer_t *timer, void *obj)
{
	rtsp_client_session_t *session = obj;
	if(!session || !session->connection || !session->connection->client) {
		return;
	}

	if(session->request_timer == timer) {
		rtsp_client_request_cancel(
				session->connection->client,
				session,
				RTSP_STATUS_CODE_REQUEST_TIMEOUT,
				RTSP_REASON_PHRASE_REQUEST_TIMEOUT);
	}
}
