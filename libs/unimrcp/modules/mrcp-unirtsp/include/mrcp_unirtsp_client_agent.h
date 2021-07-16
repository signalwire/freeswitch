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

#ifndef MRCP_UNIRTSP_CLIENT_AGENT_H
#define MRCP_UNIRTSP_CLIENT_AGENT_H

/**
 * @file mrcp_unirtsp_client_agent.h
 * @brief Implementation of MRCP Signaling Interface using UniRTSP
 */ 

#include <apr_network_io.h>
#include "apt_string.h"
#include "mrcp_sig_agent.h"

APT_BEGIN_EXTERN_C

/** Declaration of UniRTSP agent config */
typedef struct rtsp_client_config_t rtsp_client_config_t;

/** Configuration of UniRTSP agent */
struct rtsp_client_config_t {
	/** SDP origin */
	char        *origin;
	/** Number of max RTSP connections */
	apr_size_t   max_connection_count;
	/** Request timeout */
	apr_size_t   request_timeout;
};

/**
 * Create UniRTSP signaling agent.
 */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_unirtsp_client_agent_create(const char *id, rtsp_client_config_t *config, apr_pool_t *pool);

/**
 * Allocate UniRTSP config.
 */
MRCP_DECLARE(rtsp_client_config_t*) mrcp_unirtsp_client_config_alloc(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MRCP_UNIRTSP_CLIENT_AGENT_H */
