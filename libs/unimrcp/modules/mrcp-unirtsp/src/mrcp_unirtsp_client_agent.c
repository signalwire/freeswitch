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

#include <apr_general.h>
#include <sofia-sip/sdp.h>

#include "mrcp_unirtsp_client_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_message.h"
#include "mrcp_stream.h"
#include "mrcp_resource_factory.h"
#include "rtsp_client.h"
#include "mrcp_unirtsp_sdp.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define UNIRTSP_TASK_NAME "UniRTSP Agent"

typedef struct mrcp_unirtsp_agent_t mrcp_unirtsp_agent_t;
typedef struct mrcp_unirtsp_session_t mrcp_unirtsp_session_t;

struct mrcp_unirtsp_agent_t {
	mrcp_sig_agent_t     *sig_agent;
	rtsp_client_t        *rtsp_client;

	rtsp_client_config_t *config;
};

struct mrcp_unirtsp_session_t {
	mrcp_message_t        *mrcp_message;
	mrcp_session_t        *mrcp_session;
	rtsp_client_session_t *rtsp_session;
	su_home_t             *home;
};


static apt_bool_t mrcp_unirtsp_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_unirtsp_session_terminate(mrcp_session_t *session);
static apt_bool_t mrcp_unirtsp_session_control(mrcp_session_t *session, mrcp_message_t *message);
static apt_bool_t mrcp_unirtsp_resource_discover(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);

static const mrcp_session_request_vtable_t session_request_vtable = {
	mrcp_unirtsp_session_offer,
	mrcp_unirtsp_session_terminate,
	mrcp_unirtsp_session_control,
	mrcp_unirtsp_resource_discover
};

static apt_bool_t mrcp_unirtsp_on_session_terminate_response(rtsp_client_t *client, rtsp_client_session_t *session);
static apt_bool_t mrcp_unirtsp_on_session_terminate_event(rtsp_client_t *client, rtsp_client_session_t *session);
static apt_bool_t mrcp_unirtsp_on_session_response(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *request, rtsp_message_t *response);
static apt_bool_t mrcp_unirtsp_on_session_event(rtsp_client_t *client, rtsp_client_session_t *session, rtsp_message_t *message);

static const rtsp_client_vtable_t session_response_vtable = {
	mrcp_unirtsp_on_session_terminate_response,
	mrcp_unirtsp_on_session_terminate_event,
	mrcp_unirtsp_on_session_response,
	mrcp_unirtsp_on_session_event
};

static apt_bool_t mrcp_unirtsp_session_create(mrcp_session_t *session);
static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_client_config_t *config, apr_pool_t *pool);
static apt_bool_t mrcp_unirtsp_on_resource_discover(mrcp_unirtsp_agent_t *agent, mrcp_unirtsp_session_t *session, rtsp_message_t *request, rtsp_message_t *response);


/** Create UniRTSP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_unirtsp_client_agent_create(rtsp_client_config_t *config, apr_pool_t *pool)
{
	apt_task_t *task;
	mrcp_unirtsp_agent_t *agent;
	agent = apr_palloc(pool,sizeof(mrcp_unirtsp_agent_t));
	agent->sig_agent = mrcp_signaling_agent_create(agent,MRCP_VERSION_1,pool);
	agent->sig_agent->create_client_session = mrcp_unirtsp_session_create;
	agent->config = config;

	if(rtsp_config_validate(agent,config,pool) == FALSE) {
		return NULL;
	}

	agent->rtsp_client = rtsp_client_create(config->max_connection_count,
										agent,&session_response_vtable,pool);
	if(!agent->rtsp_client) {
		return NULL;
	}

	task = rtsp_client_task_get(agent->rtsp_client);
	apt_task_name_set(task,UNIRTSP_TASK_NAME);
	agent->sig_agent->task = task;

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "UNIRTSP_TASK_NAME" %s:%hu [%"APR_SIZE_T_FMT"]",
								config->server_ip,
								config->server_port,
								config->max_connection_count);
	return agent->sig_agent;
}

/** Allocate UniRTSP config */
MRCP_DECLARE(rtsp_client_config_t*) mrcp_unirtsp_client_config_alloc(apr_pool_t *pool)
{
	rtsp_client_config_t *config = apr_palloc(pool,sizeof(rtsp_client_config_t));
	config->server_ip = NULL;
	config->server_port = 0;
	config->resource_location = NULL;
	config->origin = NULL;
	config->resource_map = apr_table_make(pool,2);
	config->max_connection_count = 100;
	config->force_destination = FALSE;
	return config;
}


static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_client_config_t *config, apr_pool_t *pool)
{
	if(!config->server_ip) {
		return FALSE;
	}
	agent->config = config;
	return TRUE;
}

static APR_INLINE mrcp_unirtsp_agent_t* client_agent_get(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_unirtsp_agent_t *agent = apt_consumer_task_object_get(consumer_task);
	return agent;
}

static apt_bool_t mrcp_unirtsp_session_create(mrcp_session_t *mrcp_session)
{
	mrcp_unirtsp_agent_t *agent = mrcp_session->signaling_agent->obj;
	mrcp_unirtsp_session_t *session;
	mrcp_session->request_vtable = &session_request_vtable;

	session = apr_palloc(mrcp_session->pool,sizeof(mrcp_unirtsp_session_t));
	session->home = su_home_new(sizeof(*session->home));
	session->mrcp_message = NULL;
	session->mrcp_session = mrcp_session;
	mrcp_session->obj = session;
	
	session->rtsp_session = rtsp_client_session_create(
									agent->rtsp_client,
									agent->config->server_ip,
									agent->config->server_port,
									agent->config->resource_location);
	if(!session->rtsp_session) {
		su_home_unref(session->home);
		return FALSE;
	}
	rtsp_client_session_object_set(session->rtsp_session,session);
	return TRUE;
}

static void mrcp_unirtsp_session_destroy(mrcp_unirtsp_session_t *session)
{
	if(session->home) {
		su_home_unref(session->home);
		session->home = NULL;
	}
	rtsp_client_session_object_set(session->rtsp_session,NULL);
	rtsp_client_session_destroy(session->rtsp_session);
}

static apt_bool_t mrcp_unirtsp_on_session_terminate_response(rtsp_client_t *rtsp_client, rtsp_client_session_t *rtsp_session)
{
	mrcp_unirtsp_session_t *session	= rtsp_client_session_object_get(rtsp_session);

	mrcp_unirtsp_session_destroy(session);
	mrcp_session_terminate_response(session->mrcp_session);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_session_terminate_event(rtsp_client_t *rtsp_client, rtsp_client_session_t *rtsp_session)
{
	mrcp_unirtsp_session_t *session	= rtsp_client_session_object_get(rtsp_session);
	mrcp_session_terminate_event(session->mrcp_session);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_announce_response(mrcp_unirtsp_agent_t *agent, mrcp_unirtsp_session_t *session, rtsp_message_t *message, const char *resource_name)
{
	mrcp_message_t *mrcp_message = NULL;

	if(!session || !resource_name) {
		return FALSE;
	}
	
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
		message->header.content_type == RTSP_CONTENT_TYPE_MRCP &&
		rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
		message->header.content_length > 0) {

		apt_text_stream_t text_stream;
		mrcp_parser_t *parser;
		apt_str_t resource_name_str;

		text_stream.text = message->body;
		apt_text_stream_reset(&text_stream);
		apt_string_set(&resource_name_str,resource_name);

		parser = mrcp_parser_create(agent->sig_agent->resource_factory,session->mrcp_session->pool);
		mrcp_parser_resource_name_set(parser,&resource_name_str);
		if(mrcp_parser_run(parser,&text_stream) == MRCP_STREAM_STATUS_COMPLETE) {
			mrcp_message = mrcp_parser_message_get(parser);
			mrcp_message->channel_id.session_id = message->header.session_id;
		}
		else {
			/* error case */
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse MRCPv1 Message");
		}
	}
	else {
		/* error case */
	}

	if(!mrcp_message) {
		if(!session->mrcp_message) {
			return FALSE;
		}
		mrcp_message = mrcp_response_create(session->mrcp_message,session->mrcp_session->pool);
		mrcp_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
	}

	session->mrcp_message = NULL;
	mrcp_session_control_response(session->mrcp_session,mrcp_message);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_session_response(rtsp_client_t *rtsp_client, rtsp_client_session_t *rtsp_session, rtsp_message_t *request, rtsp_message_t *response)
{
	apt_bool_t status = FALSE;
	mrcp_unirtsp_agent_t *agent = rtsp_client_object_get(rtsp_client);
	mrcp_unirtsp_session_t *session	= rtsp_client_session_object_get(rtsp_session);
	if(!agent || !session) {
		return FALSE;
	}

	switch(request->start_line.common.request_line.method_id) {
		case RTSP_METHOD_SETUP:
		{
			const apt_str_t *session_id;
			const char *force_destination_ip = NULL;
			mrcp_session_descriptor_t *descriptor;

			if(agent->config->force_destination == TRUE) {
				force_destination_ip = agent->config->server_ip;
			}

			descriptor = mrcp_descriptor_generate_by_rtsp_response(
							request,
							response,
							force_destination_ip,
							agent->config->resource_map,
							session->mrcp_session->pool,
							session->home);
			if(!descriptor) {
				return FALSE;
			}
			session_id = rtsp_client_session_id_get(session->rtsp_session);
			if(session_id) {
				apt_string_copy(
					&session->mrcp_session->id,
					session_id,
					session->mrcp_session->pool);
			}
			status = mrcp_session_answer(session->mrcp_session,descriptor);
			break;
		}
		case RTSP_METHOD_TEARDOWN:
		{
			mrcp_session_descriptor_t *descriptor;
			descriptor = mrcp_descriptor_generate_by_rtsp_response(
							request,
							response,
							NULL,
							agent->config->resource_map,
							session->mrcp_session->pool,
							session->home);
			if(!descriptor) {
				return FALSE;
			}
			status = mrcp_session_answer(session->mrcp_session,descriptor);
			break;
		}
		case RTSP_METHOD_ANNOUNCE:
		{
			mrcp_unirtsp_agent_t *agent = rtsp_client_object_get(rtsp_client);
			const char *resource_name = mrcp_name_get_by_rtsp_name(
				agent->config->resource_map,
				request->start_line.common.request_line.resource_name);
			mrcp_unirtsp_on_announce_response(agent,session,response,resource_name);
			break;
		}
		case RTSP_METHOD_DESCRIBE:
		{
			mrcp_unirtsp_agent_t *agent = rtsp_client_object_get(rtsp_client);
			mrcp_unirtsp_on_resource_discover(agent,session,request,response);
			break;
		}
		default:
			break;
	}

	return status;
}

static apt_bool_t mrcp_unirtsp_on_session_event(rtsp_client_t *rtsp_client, rtsp_client_session_t *rtsp_session, rtsp_message_t *message)
{
	mrcp_unirtsp_agent_t *agent = rtsp_client_object_get(rtsp_client);
	mrcp_unirtsp_session_t *session	= rtsp_client_session_object_get(rtsp_session);
	const char *resource_name = mrcp_name_get_by_rtsp_name(
		agent->config->resource_map,
		message->start_line.common.request_line.resource_name);
	if(!session || !resource_name) {
		return FALSE;
	}

	mrcp_unirtsp_on_announce_response(agent,session,message,resource_name);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_session_offer(mrcp_session_t *mrcp_session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_unirtsp_session_t *session = mrcp_session->obj;
	mrcp_unirtsp_agent_t *agent = mrcp_session->signaling_agent->obj;
	rtsp_message_t *request;

	if(agent->config->origin) {
		apt_string_set(&descriptor->origin,agent->config->origin);
	}

	request = rtsp_request_generate_by_mrcp_descriptor(descriptor,agent->config->resource_map,mrcp_session->pool);
	return rtsp_client_session_request(agent->rtsp_client,session->rtsp_session,request);
}

static apt_bool_t mrcp_unirtsp_session_terminate(mrcp_session_t *mrcp_session)
{
	mrcp_unirtsp_session_t *session = mrcp_session->obj;
	mrcp_unirtsp_agent_t *agent = mrcp_session->signaling_agent->obj;

	return rtsp_client_session_terminate(agent->rtsp_client,session->rtsp_session);
}

static apt_bool_t mrcp_unirtsp_session_control(mrcp_session_t *mrcp_session, mrcp_message_t *mrcp_message)
{
	mrcp_unirtsp_session_t *session = mrcp_session->obj;
	mrcp_unirtsp_agent_t *agent = mrcp_session->signaling_agent->obj;

	char buffer[500];
	apt_text_stream_t stream;
	rtsp_message_t *rtsp_message = NULL;
	apt_str_t *body;

	apt_text_stream_init(&stream,buffer,sizeof(buffer));

	mrcp_message->start_line.version = MRCP_VERSION_1;
	if(mrcp_message_generate(agent->sig_agent->resource_factory,mrcp_message,&stream) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate MRCPv1 Message");
		return FALSE;
	}
	stream.text.length = stream.pos - stream.text.buf;

	rtsp_message = rtsp_request_create(mrcp_session->pool);
	rtsp_message->start_line.common.request_line.resource_name = rtsp_name_get_by_mrcp_name(
									agent->config->resource_map,
									mrcp_message->channel_id.resource_name.buf);
	rtsp_message->start_line.common.request_line.method_id = RTSP_METHOD_ANNOUNCE;

	body = &rtsp_message->body;
	body->length = mrcp_message->start_line.length;
	body->buf = apr_palloc(rtsp_message->pool,body->length+1);
	memcpy(body->buf,stream.text.buf,stream.text.length);
	if(mrcp_message->body.length) {
		memcpy(body->buf+stream.text.length,mrcp_message->body.buf,mrcp_message->body.length);
	}
	body->buf[body->length] = '\0';

	rtsp_message->header.content_type = RTSP_CONTENT_TYPE_MRCP;
	rtsp_header_property_add(&rtsp_message->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
	rtsp_message->header.content_length = body->length;
	rtsp_header_property_add(&rtsp_message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);

	session->mrcp_message = mrcp_message;
	rtsp_client_session_request(agent->rtsp_client,session->rtsp_session,rtsp_message);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_resource_discover(mrcp_session_t *mrcp_session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_unirtsp_session_t *session = mrcp_session->obj;
	mrcp_unirtsp_agent_t *agent = mrcp_session->signaling_agent->obj;
	rtsp_message_t *request;

	if(!descriptor) {
		return FALSE;
	}
	request = rtsp_resource_discovery_request_generate(
							descriptor->resource_name.buf,
							agent->config->resource_map,
							mrcp_session->pool);
	if(request) {
		rtsp_client_session_request(agent->rtsp_client,session->rtsp_session,request);
	}
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_resource_discover(mrcp_unirtsp_agent_t *agent, mrcp_unirtsp_session_t *session, rtsp_message_t *request, rtsp_message_t *response)
{
	mrcp_session_descriptor_t *descriptor;
	if(!session) {
		return FALSE;
	}

	descriptor = mrcp_resource_discovery_response_generate(
									request,
									response,
									agent->config->resource_map,
									session->mrcp_session->pool,
									session->home);
	if(descriptor) {
		mrcp_session_discover_response(session->mrcp_session,descriptor);
	}
	return TRUE;
}
