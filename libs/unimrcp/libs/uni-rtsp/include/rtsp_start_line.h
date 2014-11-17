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
 * $Id: rtsp_start_line.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RTSP_START_LINE_H
#define RTSP_START_LINE_H

/**
 * @file rtsp_start_line.h
 * @brief RTSP Start Line (request-line/status-line)
 */ 

#include "rtsp.h"
#include "apt_text_stream.h"

APT_BEGIN_EXTERN_C

/** Protocol version */
typedef enum {
	/** Unknown version */
	RTSP_VERSION_UNKNOWN = 0, 
	/** RTSP 1.0 */
	RTSP_VERSION_1 = 1, 
} rtsp_version_e;

/** RTSP message types */
typedef enum {
	RTSP_MESSAGE_TYPE_UNKNOWN,
	RTSP_MESSAGE_TYPE_REQUEST,
	RTSP_MESSAGE_TYPE_RESPONSE
} rtsp_message_type_e;

/** RTSP methods */
typedef enum{
	RTSP_METHOD_SETUP,
	RTSP_METHOD_ANNOUNCE,
	RTSP_METHOD_TEARDOWN,
	RTSP_METHOD_DESCRIBE,

	RTSP_METHOD_COUNT,
	RTSP_METHOD_UNKNOWN = RTSP_METHOD_COUNT
} rtsp_method_id;

/** Status codes */
typedef enum {
	RTSP_STATUS_CODE_UNKNOWN                   = 0,
	/** Success codes (2xx) */
	RTSP_STATUS_CODE_OK                        = 200,
	RTSP_STATUS_CODE_CREATED                   = 201,
	/** Failure codec (4xx) */
	RTSP_STATUS_CODE_BAD_REQUEST               = 400,
	RTSP_STATUS_CODE_UNAUTHORIZED              = 401,
	RTSP_STATUS_CODE_NOT_FOUND                 = 404,
	RTSP_STATUS_CODE_METHOD_NOT_ALLOWED        = 405,
	RTSP_STATUS_CODE_NOT_ACCEPTABLE            = 406,
	RTSP_STATUS_CODE_PROXY_AUTH_REQUIRED       = 407,
	RTSP_STATUS_CODE_REQUEST_TIMEOUT           = 408,
	RTSP_STATUS_CODE_SESSION_NOT_FOUND         = 454,

	RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR     = 500,
	RTSP_STATUS_CODE_NOT_IMPLEMENTED           = 501,
} rtsp_status_code_e;

/** Reason phrases */
typedef enum {
	RTSP_REASON_PHRASE_OK,
	RTSP_REASON_PHRASE_CREATED,
	RTSP_REASON_PHRASE_BAD_REQUEST,
	RTSP_REASON_PHRASE_UNAUTHORIZED,
	RTSP_REASON_PHRASE_NOT_FOUND,
	RTSP_REASON_PHRASE_METHOD_NOT_ALLOWED,
	RTSP_REASON_PHRASE_NOT_ACCEPTABLE,
	RTSP_REASON_PHRASE_PROXY_AUTH_REQUIRED,
	RTSP_REASON_PHRASE_REQUEST_TIMEOUT,
	RTSP_REASON_PHRASE_SESSION_NOT_FOUND,
	RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,
	RTSP_REASON_PHRASE_NOT_IMPLEMENTED,
	RTSP_REASON_PHRASE_COUNT,

	/** Unknown reason phrase */
	RTSP_REASON_PHRASE_UNKNOWN = RTSP_REASON_PHRASE_COUNT
} rtsp_reason_phrase_e;


/** RTSP request-line declaration */
typedef struct rtsp_request_line_t rtsp_request_line_t;
/** RTSP status-line declaration */
typedef struct rtsp_status_line_t rtsp_status_line_t;
/** RTSP start-line declaration */
typedef struct rtsp_start_line_t rtsp_start_line_t;

/** RTSP request-line */
struct rtsp_request_line_t {
	/** Method name */
	apt_str_t      method_name;
	/** Method id */
	rtsp_method_id method_id;
	/** RTSP URL */
	apt_str_t      url;
	/** Resource name parsed from RTSP URL */
	const char    *resource_name;
	/** Version of protocol in use */
	rtsp_version_e version;
};

/** RTSP status-line */
struct rtsp_status_line_t {
	/** Version of protocol in use */
	rtsp_version_e     version;
	/** success or failure or other status of the request */
	rtsp_status_code_e status_code;
	/** Reason phrase */
	apt_str_t          reason;
};

/** RTSP start-line */
struct rtsp_start_line_t {
	/** RTSP message type */
	rtsp_message_type_e     message_type;
	/** RTSP start-line */
	union {
		rtsp_request_line_t request_line;
		rtsp_status_line_t  status_line;
	} common;
};


static APR_INLINE void rtsp_request_line_init(rtsp_request_line_t *request_line)
{
	apt_string_reset(&request_line->method_name);
	request_line->method_id = RTSP_METHOD_UNKNOWN;
	apt_string_reset(&request_line->url);
	request_line->resource_name = NULL;
	request_line->version = RTSP_VERSION_1;
}

static APR_INLINE void rtsp_status_line_init(rtsp_status_line_t *status_line)
{
	status_line->version = RTSP_VERSION_1;
	status_line->status_code = RTSP_STATUS_CODE_OK;
	apt_string_reset(&status_line->reason);
}

/** Initialize RTSP start-line */
static APR_INLINE void rtsp_start_line_init(rtsp_start_line_t *start_line, rtsp_message_type_e message_type)
{
	start_line->message_type = message_type;
	if(message_type == RTSP_MESSAGE_TYPE_REQUEST) {
		rtsp_request_line_init(&start_line->common.request_line);
	}
	else if(message_type == RTSP_MESSAGE_TYPE_RESPONSE) {
		rtsp_status_line_init(&start_line->common.status_line);
	}
}

/** Parse RTSP start-line */
RTSP_DECLARE(apt_bool_t) rtsp_start_line_parse(rtsp_start_line_t *start_line, apt_str_t *str, apr_pool_t *pool);

/** Generate RTSP start-line */
RTSP_DECLARE(apt_bool_t) rtsp_start_line_generate(rtsp_start_line_t *start_line, apt_text_stream_t *text_stream);

/** Get reason phrase by status code */
RTSP_DECLARE(const apt_str_t*) rtsp_reason_phrase_get(rtsp_reason_phrase_e reason);

APT_END_EXTERN_C

#endif /* RTSP_START_LINE_H */
