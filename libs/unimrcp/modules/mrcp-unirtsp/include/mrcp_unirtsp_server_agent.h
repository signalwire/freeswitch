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

#ifndef __MRCP_UNIRTSP_SERVER_AGENT_H__
#define __MRCP_UNIRTSP_SERVER_AGENT_H__

/**
 * @file mrcp_unirtsp_server_agent.h
 * @brief Implementation of MRCP Signaling Interface using UniRTSP
 */ 

#include <apr_network_io.h>
#include "mrcp_sig_agent.h"

APT_BEGIN_EXTERN_C

/** UniRTSP config declaration */
typedef struct rtsp_server_config_t rtsp_server_config_t;

/** UniRTSP config */
struct rtsp_server_config_t {
	/** Local IP address to bind to */
	char        *local_ip;
	/** Local port to bind to */
	apr_port_t   local_port;

	/** Resource location */
	char        *resource_location;
	/** SDP origin */
	char        *origin;

	/** Map of the MRCP resource names */
	apr_table_t *resource_map;

	/** Number of max RTSP connections */
	apr_size_t   max_connection_count;

	/** Force destination ip address. Should be used only in case 
	SDP contains incorrect connection address (local IP address behind NAT) */
	apt_bool_t   force_destination;
};

/**
 * Create UniRTSP signaling agent.
 */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_unirtsp_server_agent_create(rtsp_server_config_t *config, apr_pool_t *pool);

/**
 * Allocate UniRTSP config.
 */
MRCP_DECLARE(rtsp_server_config_t*) mrcp_unirtsp_server_config_alloc(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /*__MRCP_UNIRTSP_SERVER_AGENT_H__*/
