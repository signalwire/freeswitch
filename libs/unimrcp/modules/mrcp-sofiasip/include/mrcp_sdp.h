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
 * $Id: mrcp_sdp.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_SDP_H
#define MRCP_SDP_H

/**
 * @file mrcp_sdp.h
 * @brief MRCP SDP Transformations
 */ 

#include "mrcp_sig_types.h"

APT_BEGIN_EXTERN_C

/** Generate SDP string by MRCP descriptor */
MRCP_DECLARE(apr_size_t) sdp_string_generate_by_mrcp_descriptor(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, apt_bool_t offer);

/** Generate MRCP descriptor by SDP session */
MRCP_DECLARE(apt_bool_t) mrcp_descriptor_generate_by_sdp_session(mrcp_session_descriptor_t* descriptor, const sdp_session_t *sdp, const char *force_destination_ip, apr_pool_t *pool);

/** Generate SDP resource discovery string */
MRCP_DECLARE(apr_size_t) sdp_resource_discovery_string_generate(const char *ip, const char *origin, char *buffer, apr_size_t size);

APT_END_EXTERN_C

#endif /* MRCP_SDP_H */
