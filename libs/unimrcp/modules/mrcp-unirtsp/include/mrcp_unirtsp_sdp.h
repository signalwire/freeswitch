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

#ifndef __MRCP_UNIRTSP_SDP_H__
#define __MRCP_UNIRTSP_SDP_H__

/**
 * @file mrcp_unirtsp_sdp.h
 * @brief MRCP RTSP SDP Transformations
 */ 

#include "mrcp_session_descriptor.h"

APT_BEGIN_EXTERN_C

/** Generate MRCP descriptor by RTSP request */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_request(
											const rtsp_message_t *request,
											const char *force_destination_ip,
											const apr_table_t *resource_map,
											apr_pool_t *pool,
											su_home_t *home);

/** Generate MRCP descriptor by RTSP response */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_response(
											const rtsp_message_t *request,
											const rtsp_message_t *response,
											const char *force_destination_ip,
											const apr_table_t *resource_map,
											apr_pool_t *pool,
											su_home_t *home);

/** Generate RTSP request by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_request_generate_by_mrcp_descriptor(
											const mrcp_session_descriptor_t *descriptor, 
											const apr_table_t *resource_map, 
											apr_pool_t *pool);
/** Generate RTSP response by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_response_generate_by_mrcp_descriptor(
											const rtsp_message_t *request, 
											const mrcp_session_descriptor_t *descriptor, 
											const apr_table_t *resource_map, 
											apr_pool_t *pool);

/** Generate RTSP resource discovery request */
MRCP_DECLARE(rtsp_message_t*) rtsp_resource_discovery_request_generate(
											const char *resource_name,
											const apr_table_t *resource_map,
											apr_pool_t *pool);

/** Generate resource discovery descriptor by RTSP response */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_resource_discovery_response_generate(
											const rtsp_message_t *request, 
											const rtsp_message_t *response,
											const apr_table_t *resource_map,
											apr_pool_t *pool,
											su_home_t *home);

/** Generate RTSP resource discovery response */
MRCP_DECLARE(rtsp_message_t*) rtsp_resource_discovery_response_generate(
											const rtsp_message_t *request,
											const char *ip,
											const char *origin,
											apr_pool_t *pool);


/** Get MRCP resource name by RTSP resource name */
MRCP_DECLARE(const char*) mrcp_name_get_by_rtsp_name(const apr_table_t *resource_map, const char *rtsp_name);
/** Get RTSP resource name by MRCP resource name */
MRCP_DECLARE(const char*) rtsp_name_get_by_mrcp_name(const apr_table_t *resource_map, const char *mrcp_name);

APT_END_EXTERN_C

#endif /*__MRCP_UNIRTSP_SDP_H__*/
