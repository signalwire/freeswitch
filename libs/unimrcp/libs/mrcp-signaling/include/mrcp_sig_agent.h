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

#ifndef MRCP_SIG_AGENT_H
#define MRCP_SIG_AGENT_H

/**
 * @file mrcp_sig_agent.h
 * @brief Abstract MRCP Signaling Agent
 */ 

#include <apr_network_io.h>
#include <apr_tables.h>
#include "mrcp_sig_types.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Signaling settings */
struct mrcp_sig_settings_t {
	/** Server IP address */
	char        *server_ip;
	/** Server port */
	apr_port_t   server_port;
	/** Server SIP user name (v2 only) */
	char        *user_name;
	/** Resource location (v1 only) */
	char        *resource_location;
	/** Map of the MRCP resource names (v1 only) */
	apr_table_t *resource_map;
	/** Force destination IP address. Should be used only in case 
	SDP contains incorrect connection address (local IP address behind NAT) */
	apt_bool_t   force_destination;
	/** Optional feature tags */
	char        *feature_tags;
};

/** MRCP signaling agent  */
struct mrcp_sig_agent_t {
	/** Agent identifier */
	const char              *id;
	/** Memory pool to allocate memory from */
	apr_pool_t              *pool;
	/** External object associated with agent */
	void                    *obj;
	/** Parent object (client/server) */
	void                    *parent;
	/** MRCP resource factory */
	mrcp_resource_factory_t *resource_factory;
	/** Task interface */
	apt_task_t              *task;
	/** Task message pool used to allocate signaling agent messages */
	apt_task_msg_pool_t     *msg_pool;

	/** Virtual create_server_session */
	mrcp_session_t* (*create_server_session)(mrcp_sig_agent_t *signaling_agent);
	/** Virtual create_client_session */
	apt_bool_t (*create_client_session)(mrcp_session_t *session, const mrcp_sig_settings_t *settings, const mrcp_session_attribs_t *attribs);
};

/** Create signaling agent. */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(const char *id, void *obj, apr_pool_t *pool);

/** Create factory of signaling agents. */
MRCP_DECLARE(mrcp_sa_factory_t*) mrcp_sa_factory_create(apr_pool_t *pool);

/** Add signaling agent to factory. */
MRCP_DECLARE(apt_bool_t) mrcp_sa_factory_agent_add(mrcp_sa_factory_t *sa_factory, mrcp_sig_agent_t *sig_agent);

/** Determine whether factory is empty. */
MRCP_DECLARE(apt_bool_t) mrcp_sa_factory_is_empty(const mrcp_sa_factory_t *sa_factory);

/** Select next available signaling agent. */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sa_factory_agent_select(mrcp_sa_factory_t *sa_factory);

/** Allocate MRCP signaling settings. */
MRCP_DECLARE(mrcp_sig_settings_t*) mrcp_signaling_settings_alloc(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MRCP_SIG_AGENT_H */
