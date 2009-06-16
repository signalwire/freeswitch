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

#include "mrcp_sig_agent.h"
#include "mrcp_session.h"
#include "apt_pool.h"

MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(void *obj, mrcp_version_e mrcp_version, apr_pool_t *pool)
{
	mrcp_sig_agent_t *sig_agent = apr_palloc(pool,sizeof(mrcp_sig_agent_t));
	sig_agent->pool = pool;
	sig_agent->obj = obj;
	sig_agent->mrcp_version = mrcp_version;
	sig_agent->resource_factory = NULL;
	sig_agent->parent = NULL;
	sig_agent->task = NULL;
	sig_agent->msg_pool = NULL;
	sig_agent->create_server_session = NULL;
	sig_agent->create_client_session = NULL;
	return sig_agent;
}

MRCP_DECLARE(mrcp_session_t*) mrcp_session_create(apr_size_t padding)
{
	mrcp_session_t *session;
	apr_pool_t *pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}
	session = apr_palloc(pool,sizeof(mrcp_session_t)+padding);
	session->pool = pool;
	session->obj = NULL;
	session->signaling_agent = NULL;
	session->request_vtable = NULL;
	session->response_vtable = NULL;
	session->event_vtable = NULL;
	apt_string_reset(&session->id);
	session->last_request_id = 0;
	return session;
}

MRCP_DECLARE(void) mrcp_session_destroy(mrcp_session_t *session)
{
	if(session->pool) {
		apr_pool_destroy(session->pool);
	}
}
