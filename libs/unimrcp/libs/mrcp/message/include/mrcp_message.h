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

#ifndef __MRCP_MESSAGE_H__
#define __MRCP_MESSAGE_H__

/**
 * @file mrcp_message.h
 * @brief MRCP Message Definition
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** Request-states used in MRCP response message */
typedef enum {
	/** The request was processed to completion and there will be no	
	    more events from that resource to the client with that request-id */
	MRCP_REQUEST_STATE_COMPLETE,
	/** The job has been placed on a queue and will be processed in first-in-first-out order */
	MRCP_REQUEST_STATE_INPROGRESS,
	/** Indicate that further event messages will be delivered with that request-id */
	MRCP_REQUEST_STATE_PENDING,
	
	/** Number of request states */
	MRCP_REQUEST_STATE_COUNT,
	/** Unknown request state */
	MRCP_REQUEST_STATE_UNKNOWN = MRCP_REQUEST_STATE_COUNT
} mrcp_request_state_e;

/** Status codes */
typedef enum {
	MRCP_STATUS_CODE_UNKNOWN                   = 0,
	/* success codes (2xx) */
	MRCP_STATUS_CODE_SUCCESS                   = 200,
	MRCP_STATUS_CODE_SUCCESS_WITH_IGNORE       = 201,
	/* failure codes (4xx) */
	MRCP_STATUS_CODE_METHOD_NOT_ALLOWED        = 401,
	MRCP_STATUS_CODE_METHOD_NOT_VALID          = 402,
	MRCP_STATUS_CODE_UNSUPPORTED_PARAM         = 403,
	MRCP_STATUS_CODE_ILLEGAL_PARAM_VALUE       = 404,
	MRCP_STATUS_CODE_NOT_FOUND                 = 405,
	MRCP_STATUS_CODE_MISSING_PARAM             = 406,
	MRCP_STATUS_CODE_METHOD_FAILED             = 407,
	MRCP_STATUS_CODE_UNRECOGNIZED_MESSAGE      = 408,
	MRCP_STATUS_CODE_UNSUPPORTED_PARAM_VALUE   = 409,
	MRCP_STATUS_CODE_RESOURCE_SPECIFIC_FAILURE = 421
} mrcp_status_code_e;

/** MRCP message types */
typedef enum {
	MRCP_MESSAGE_TYPE_UNKNOWN,
	MRCP_MESSAGE_TYPE_REQUEST,
	MRCP_MESSAGE_TYPE_RESPONSE,
	MRCP_MESSAGE_TYPE_EVENT
} mrcp_message_type_e;


/** MRCP start-line declaration */
typedef struct mrcp_start_line_t mrcp_start_line_t;
/** MRCP channel-id declaration */
typedef struct mrcp_channel_id mrcp_channel_id;
/** MRCP message header declaration */
typedef struct mrcp_message_header_t mrcp_message_header_t;


/** Start-line of MRCP message */
struct mrcp_start_line_t {
	/** MRCP message type */
	mrcp_message_type_e  message_type;
	/** Version of protocol in use */
	mrcp_version_e       version;
	/** Specify the length of the message, including the start-line (v2) */
	size_t               length;
	/** Unique identifier among client and server */
	mrcp_request_id      request_id;
	/** MRCP method name */
	apt_str_t            method_name;
	/** MRCP method id (associated with method name) */
	mrcp_method_id       method_id;
	/** Success or failure or other status of the request */
	mrcp_status_code_e   status_code;
	/** The state of the job initiated by the request */
	mrcp_request_state_e request_state;
};

/** Initialize MRCP start-line */
MRCP_DECLARE(void) mrcp_start_line_init(mrcp_start_line_t *start_line);
/** Parse MRCP start-line */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_parse(mrcp_start_line_t *start_line, apt_text_stream_t *text_stream, apr_pool_t *pool);
/** Generate MRCP start-line */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_generate(mrcp_start_line_t *start_line, apt_text_stream_t *text_stream);
/** Finalize MRCP start-line generation */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_finalize(mrcp_start_line_t *start_line, apr_size_t content_length, apt_text_stream_t *text_stream);


/** MRCP channel-identifier */
struct mrcp_channel_id {
	/** Unambiguous string identifying the MRCP session */
	apt_str_t        session_id;
	/** MRCP resource name */
	apt_str_t        resource_name;
	/** MRCP resource id (associated with resource name) */
	mrcp_resource_id resource_id;
};

/** Initialize MRCP channel-identifier */
MRCP_DECLARE(void) mrcp_channel_id_init(mrcp_channel_id *channel_id);

/** Parse MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_parse(mrcp_channel_id *channel_id, apt_text_stream_t *text_stream, apr_pool_t *pool);

/** Generate MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_generate(mrcp_channel_id *channel_id, apt_text_stream_t *text_stream);


/** MRCP message-header */
struct mrcp_message_header_t {
	/** MRCP generic-header */
	mrcp_header_accessor_t generic_header_accessor;
	/** MRCP resource specific header */
	mrcp_header_accessor_t resource_header_accessor;
};

/** Initialize MRCP message-header */
static APR_INLINE void mrcp_message_header_init(mrcp_message_header_t *message_header)
{
	mrcp_header_accessor_init(&message_header->generic_header_accessor);
	mrcp_header_accessor_init(&message_header->resource_header_accessor);
}

/** Destroy MRCP message-header */
static APR_INLINE void mrcp_message_header_destroy(mrcp_message_header_t *message_header)
{
	mrcp_header_destroy(&message_header->generic_header_accessor);
	mrcp_header_destroy(&message_header->resource_header_accessor);
}


/** Parse MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_parse(mrcp_message_header_t *message_header, apt_text_stream_t *text_stream, apr_pool_t *pool);

/** Generate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_generate(mrcp_message_header_t *message_header, apt_text_stream_t *text_stream);

/** Set MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_set(mrcp_message_header_t *message_header, const mrcp_message_header_t *src, apr_pool_t *pool);

/** Get MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_get(mrcp_message_header_t *message_header, const mrcp_message_header_t *src, apr_pool_t *pool);

/** Inherit MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_inherit(mrcp_message_header_t *message_header, const mrcp_message_header_t *parent, apr_pool_t *pool);



/** MRCP message */
struct mrcp_message_t {
	/** Start-line of MRCP message */
	mrcp_start_line_t      start_line;
	/** Channel-identifier of MRCP message */
	mrcp_channel_id        channel_id;
	/** Header of MRCP message */
	mrcp_message_header_t  header;
	/** Body of MRCP message */
	apt_str_t              body;

	/** Memory pool MRCP message is allocated from */
	apr_pool_t            *pool;
};

/** Create MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_message_create(apr_pool_t *pool);

/** Initialize MRCP message */
MRCP_DECLARE(void) mrcp_message_init(mrcp_message_t *message, apr_pool_t *pool);

/** Initialize MRCP response/event message by request message */
MRCP_DECLARE(void) mrcp_message_init_by_request(mrcp_message_t *message, const mrcp_message_t *request_message);

/** Create MRCP request message */
MRCP_DECLARE(mrcp_message_t*) mrcp_request_create(mrcp_resource_id resource_id, mrcp_method_id method_id, apr_pool_t *pool);
/** Create MRCP response message */
MRCP_DECLARE(mrcp_message_t*) mrcp_response_create(const mrcp_message_t *request_message, apr_pool_t *pool);
/** Create MRCP event message */
MRCP_DECLARE(mrcp_message_t*) mrcp_event_create(const mrcp_message_t *request_message, mrcp_method_id event_id, apr_pool_t *pool);

/** Validate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_validate(mrcp_message_t *message);

/** Destroy MRCP message */
MRCP_DECLARE(void) mrcp_message_destroy(mrcp_message_t *message);


/** Parse MRCP message-body */
MRCP_DECLARE(apt_bool_t) mrcp_body_parse(mrcp_message_t *message, apt_text_stream_t *text_stream, apr_pool_t *pool);
/** Generate MRCP message-body */
MRCP_DECLARE(apt_bool_t) mrcp_body_generate(mrcp_message_t *message, apt_text_stream_t *text_stream);

/** Get MRCP generic-header */
static APR_INLINE void* mrcp_generic_header_get(mrcp_message_t *mrcp_message)
{
	return mrcp_message->header.generic_header_accessor.data;
}

/** Prepare MRCP generic-header */
static APR_INLINE void* mrcp_generic_header_prepare(mrcp_message_t *mrcp_message)
{
	return mrcp_header_allocate(&mrcp_message->header.generic_header_accessor,mrcp_message->pool);
}

/** Add MRCP generic-header proprerty */
static APR_INLINE void mrcp_generic_header_property_add(mrcp_message_t *mrcp_message, size_t id)
{
	mrcp_header_property_add(&mrcp_message->header.generic_header_accessor,id);
}

/** Add MRCP generic-header name only proprerty (should be used to construct empty headers in case of GET-PARAMS request) */
static APR_INLINE void mrcp_generic_header_name_property_add(mrcp_message_t *mrcp_message, size_t id)
{
	mrcp_header_name_property_add(&mrcp_message->header.generic_header_accessor,id);
}

/** Check MRCP generic-header proprerty */
static APR_INLINE apt_bool_t mrcp_generic_header_property_check(mrcp_message_t *mrcp_message, size_t id)
{
	return mrcp_header_property_check(&mrcp_message->header.generic_header_accessor,id);
}


/** Get MRCP resource-header */
static APR_INLINE void* mrcp_resource_header_get(const mrcp_message_t *mrcp_message)
{
	return mrcp_message->header.resource_header_accessor.data;
}

/** Prepare MRCP resource-header */
static APR_INLINE void* mrcp_resource_header_prepare(mrcp_message_t *mrcp_message)
{
	return mrcp_header_allocate(&mrcp_message->header.resource_header_accessor,mrcp_message->pool);
}

/** Add MRCP resource-header proprerty */
static APR_INLINE void mrcp_resource_header_property_add(mrcp_message_t *mrcp_message, size_t id)
{
	mrcp_header_property_add(&mrcp_message->header.resource_header_accessor,id);
}

/** Add MRCP resource-header name only proprerty (should be used to construct empty headers in case of GET-PARAMS request) */
static APR_INLINE void mrcp_resource_header_name_property_add(mrcp_message_t *mrcp_message, size_t id)
{
	mrcp_header_name_property_add(&mrcp_message->header.resource_header_accessor,id);
}

/** Check MRCP resource-header proprerty */
static APR_INLINE apt_bool_t mrcp_resource_header_property_check(mrcp_message_t *mrcp_message, size_t id)
{
	return mrcp_header_property_check(&mrcp_message->header.resource_header_accessor,id);
}

APT_END_EXTERN_C

#endif /*__MRCP_MESSAGE_H__*/
