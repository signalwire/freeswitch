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

#ifndef __MRCP_SIG_AGENT_H__
#define __MRCP_SIG_AGENT_H__

/**
 * @file mrcp_sig_agent.h
 * @brief Abstract MRCP Signaling Agent
 */ 

#include "mrcp_sig_types.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C


/** MRCP signaling agent  */
struct mrcp_sig_agent_t {
	/** Memory pool to allocate memory from */
	apr_pool_t          *pool;
	/** External object associated with agent */
	void                *obj;
	/** Parent object (client/server) */
	void                *parent;
	/** MRCP version */
	mrcp_version_e       mrcp_version;
	/** MRCP resource factory */
	mrcp_resource_factory_t *resource_factory;
	/** Task interface */
	apt_task_t          *task;
	/** Task message pool used to allocate signaling agent messages */
	apt_task_msg_pool_t *msg_pool;

	/** Virtual create_server_session */
	mrcp_session_t* (*create_server_session)(mrcp_sig_agent_t *signaling_agent);
	/** Virtual create_client_session */
	apt_bool_t (*create_client_session)(mrcp_session_t *session);
};

/** Create signaling agent. */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(void *obj, mrcp_version_e mrcp_version, apr_pool_t *pool);

APT_END_EXTERN_C

#endif /*__MRCP_SIG_AGENT_H__*/
