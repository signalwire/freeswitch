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
 * $Id: rtsp_message.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RTSP_MESSAGE_H
#define RTSP_MESSAGE_H

/**
 * @file rtsp_message.h
 * @brief RTSP Message Definition
 */ 

#include "rtsp_start_line.h"
#include "rtsp_header.h"

APT_BEGIN_EXTERN_C

/** RTSP message declaration */
typedef struct rtsp_message_t rtsp_message_t;

/** RTSP message */
struct rtsp_message_t {
	/** RTSP mesage type (request/response) */
	rtsp_start_line_t start_line;     
	/** RTSP header */
	rtsp_header_t     header;
	/** RTSP message body */
	apt_str_t         body;

	/** Pool to allocate memory from */
	apr_pool_t       *pool;
};

/** 
 * Create RTSP message.
 * @param message_type the message type
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_message_create(rtsp_message_type_e message_type, apr_pool_t *pool);

/** 
 * Create RTSP request message.
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_request_create(apr_pool_t *pool);

/** 
 * Create RTSP response message.
 * @param request the request to create response to
 * @param status_code the status code of the response
 * @param reason the reason phrase id of the response
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(const rtsp_message_t *request, rtsp_status_code_e status_code, rtsp_reason_phrase_e reason, apr_pool_t *pool);

/** 
 * Destroy RTSP message 
 * @param message the message to destroy
 */
RTSP_DECLARE(void) rtsp_message_destroy(rtsp_message_t *message);

APT_END_EXTERN_C

#endif /* RTSP_MESSAGE_H */
