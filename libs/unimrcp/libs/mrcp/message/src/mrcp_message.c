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
 * $Id: mrcp_message.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_resource.h"
#include "apt_text_message.h"
#include "apt_log.h"

/** Associate MRCP resource with message */
static apt_bool_t mrcp_message_resource_set_by_id(mrcp_message_t *message, const mrcp_resource_t *resource)
{
	if(!resource) {
		return FALSE;
	}
	message->resource = resource;
	message->channel_id.resource_name = resource->name;
	mrcp_message_header_data_alloc(
		&message->header,
		mrcp_generic_header_vtable_get(message->start_line.version),
		resource->get_resource_header_vtable(message->start_line.version),
		message->pool);

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}

	return TRUE;
}

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_resource_set(mrcp_message_t *message, const mrcp_resource_t *resource)
{
	if(!resource) {
		return FALSE;
	}
	message->resource = resource;
	mrcp_message_header_data_alloc(
		&message->header,
		mrcp_generic_header_vtable_get(message->start_line.version),
		resource->get_resource_header_vtable(message->start_line.version),
		message->pool);
	
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->method_count) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->event_count) {
			return FALSE;
		}
	}

	return TRUE;
}

/** Create an MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_message_create(apr_pool_t *pool)
{
	mrcp_message_t *message = apr_palloc(pool,sizeof(mrcp_message_t));
	mrcp_start_line_init(&message->start_line);
	mrcp_channel_id_init(&message->channel_id);
	mrcp_message_header_init(&message->header);
	apt_string_reset(&message->body);
	message->resource = NULL;
	message->pool = pool;
	return message;
}

/** Create an MRCP request message */
MRCP_DECLARE(mrcp_message_t*) mrcp_request_create(const mrcp_resource_t *resource, mrcp_version_e version, mrcp_method_id method_id, apr_pool_t *pool)
{
	mrcp_message_t *request_message = mrcp_message_create(pool);
	request_message->start_line.message_type = MRCP_MESSAGE_TYPE_REQUEST;
	request_message->start_line.version = version;
	request_message->start_line.method_id = method_id;
	mrcp_message_resource_set_by_id(request_message,resource);
	return request_message;
}

/** Create an MRCP response message */
MRCP_DECLARE(mrcp_message_t*) mrcp_response_create(const mrcp_message_t *request_message, apr_pool_t *pool)
{
	mrcp_message_t *response_message = mrcp_message_create(pool);
	response_message->start_line.message_type = MRCP_MESSAGE_TYPE_RESPONSE;
	response_message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
	response_message->start_line.status_code = MRCP_STATUS_CODE_SUCCESS;
	if(request_message) {
		response_message->channel_id = request_message->channel_id;
		response_message->start_line.request_id = request_message->start_line.request_id;
		response_message->start_line.version = request_message->start_line.version;
		response_message->start_line.method_id = request_message->start_line.method_id;
		response_message->start_line.method_name = request_message->start_line.method_name;
		mrcp_message_resource_set_by_id(response_message,request_message->resource);
	}
	return response_message;
}

/** Create an MRCP event message */
MRCP_DECLARE(mrcp_message_t*) mrcp_event_create(const mrcp_message_t *request_message, mrcp_method_id event_id, apr_pool_t *pool)
{
	mrcp_message_t *event_message = mrcp_message_create(pool);
	event_message->start_line.message_type = MRCP_MESSAGE_TYPE_EVENT;
	event_message->start_line.method_id = event_id;
	if(request_message) {
		event_message->channel_id = request_message->channel_id;
		event_message->start_line.request_id = request_message->start_line.request_id;
		event_message->start_line.version = request_message->start_line.version;
		mrcp_message_resource_set_by_id(event_message,request_message->resource);
	}
	return event_message;
}

/** Destroy MRCP message */
MRCP_DECLARE(void) mrcp_message_destroy(mrcp_message_t *message)
{
	apt_string_reset(&message->body);
	mrcp_message_header_destroy(&message->header);
}

/** Validate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_validate(mrcp_message_t *message)
{
	if(message->body.length) {
		/* content length must be specified */
		mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
		if(!generic_header) {
			return FALSE;
		}
		if(mrcp_generic_header_property_check(message,GENERIC_HEADER_CONTENT_LENGTH) != TRUE ||
		  !generic_header->content_length) {
			generic_header->content_length = message->body.length;
			mrcp_generic_header_property_add(message,GENERIC_HEADER_CONTENT_LENGTH);
		}
	}

	return TRUE;
}

/** Add MRCP generic header field by specified property (numeric identifier) */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_property_add(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = mrcp_header_field_value_generate(
										&message->header.generic_header_accessor,
										id,
										FALSE,
										message->pool);
	if(!header_field) {
		return FALSE;
	}
	header_field->id = id;
	return apt_header_section_field_add(&message->header.header_section,header_field);
}

/** Add only the name of MRCP generic header field specified by property (numeric identifier) */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_name_property_add(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = mrcp_header_field_value_generate(
										&message->header.generic_header_accessor,
										id,
										TRUE,
										message->pool);
	if(!header_field) {
		return FALSE;
	}
	header_field->id = id;
	return apt_header_section_field_add(&message->header.header_section,header_field);
}

/** Add MRCP resource header field by specified property (numeric identifier) */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_property_add(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = mrcp_header_field_value_generate(
										&message->header.resource_header_accessor,
										id,
										FALSE,
										message->pool);
	if(!header_field) {
		return FALSE;
	}
	header_field->id = id + GENERIC_HEADER_COUNT;
	return apt_header_section_field_add(&message->header.header_section,header_field);
}

/** Add only the name of MRCP resource header field specified by property (numeric identifier) */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_name_property_add(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = mrcp_header_field_value_generate(
										&message->header.resource_header_accessor,
										id,
										TRUE,
										message->pool);
	if(!header_field) {
		return FALSE;
	}
	header_field->id = id + GENERIC_HEADER_COUNT;
	return apt_header_section_field_add(&message->header.header_section,header_field);
}

/** Get the next MRCP header field */
MRCP_DECLARE(apt_header_field_t*) mrcp_message_next_header_field_get(const mrcp_message_t *message, apt_header_field_t *header_field)
{
	const apt_header_section_t *header_section = &message->header.header_section;
	if(header_field) {
		apt_header_field_t *next = APR_RING_NEXT(header_field,link);
		if(next == APR_RING_SENTINEL(&header_section->ring,apt_header_field_t,link)) {
			return NULL;
		}
		return next;
	}
	
	if(APR_RING_EMPTY(&header_section->ring,apt_header_field_t,link)) {
		return NULL;
	}
	return APR_RING_FIRST(&header_section->ring);
}
