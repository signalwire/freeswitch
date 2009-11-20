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

#ifndef __MRCP_START_LINE_H__
#define __MRCP_START_LINE_H__

/**
 * @file mrcp_start_line.h
 * @brief MRCP Start Line Definition
 */ 

#include "mrcp_types.h"
#include "apt_text_stream.h"

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

/** Start-line of MRCP message */
struct mrcp_start_line_t {
	/** MRCP message type */
	mrcp_message_type_e  message_type;
	/** Version of protocol in use */
	mrcp_version_e       version;
	/** Specify the length of the message, including the start-line (v2) */
	apr_size_t           length;
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

/** Parse MRCP request-id */
MRCP_DECLARE(mrcp_request_id) mrcp_request_id_parse(const apt_str_t *field);
/** Generate MRCP request-id */
MRCP_DECLARE(apt_bool_t) mrcp_request_id_generate(mrcp_request_id request_id, apt_text_stream_t *stream);


APT_END_EXTERN_C

#endif /*__MRCP_START_LINE_H__*/
