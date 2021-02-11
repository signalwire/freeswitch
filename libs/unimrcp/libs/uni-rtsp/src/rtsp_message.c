/*
 * Copyright 2008-2015 Arsen Chaloyan
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

#include "rtsp_message.h"

/** Initialize RTSP message */
static APR_INLINE void rtsp_message_init(rtsp_message_t *message, rtsp_message_type_e message_type, apr_pool_t *pool)
{
	message->pool = pool;
	rtsp_start_line_init(&message->start_line,message_type);
	rtsp_header_init(&message->header,pool);
	apt_string_reset(&message->body);
}

/** Create RTSP message */
RTSP_DECLARE(rtsp_message_t*) rtsp_message_create(rtsp_message_type_e message_type, apr_pool_t *pool)
{
	rtsp_message_t *message = apr_palloc(pool,sizeof(rtsp_message_t));
	rtsp_message_init(message,message_type,pool);
	return message;
}

/** Create RTSP request message */
RTSP_DECLARE(rtsp_message_t*) rtsp_request_create(apr_pool_t *pool)
{
	rtsp_message_t *request = rtsp_message_create(RTSP_MESSAGE_TYPE_REQUEST,pool);
	request->start_line.common.request_line.version = RTSP_VERSION_1;
	return request;
}

/** Create RTSP response message */
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(const rtsp_message_t *request, rtsp_status_code_e status_code, rtsp_reason_phrase_e reason, apr_pool_t *pool)
{
	const apt_str_t *reason_str;
	rtsp_status_line_t *status_line;
	rtsp_message_t *response = rtsp_message_create(RTSP_MESSAGE_TYPE_RESPONSE,pool);
	status_line = &response->start_line.common.status_line;
	status_line->version = request->start_line.common.request_line.version;
	status_line->status_code = status_code;
	reason_str = rtsp_reason_phrase_get(reason);
	if(reason_str) {
		apt_string_copy(&status_line->reason,reason_str,pool);
	}

	if(rtsp_header_property_check(&request->header,RTSP_HEADER_FIELD_CSEQ) == TRUE) {
		response->header.cseq = request->header.cseq;
		rtsp_header_property_add(&response->header,RTSP_HEADER_FIELD_CSEQ,response->pool);
	}

	if(rtsp_header_property_check(&request->header,RTSP_HEADER_FIELD_TRANSPORT) == TRUE) {
		const rtsp_transport_t *req_transport = &request->header.transport;
		rtsp_transport_t *res_transport = &response->header.transport;
		if(req_transport->mode.length) {
			apt_string_copy(&res_transport->mode,&req_transport->mode,pool);
		}
	}

	return response;
}

/** Destroy RTSP message */
RTSP_DECLARE(void) rtsp_message_destroy(rtsp_message_t *message)
{
	/* nothing to do message is allocated from pool */
}
