/*
 * Copyright 2008-2010 Arsen Chaloyan
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
 * $Id: mrcp_sig_agent.c 1792 2011-01-10 21:08:52Z achaloyan $
 */

#include "mrcp_sig_agent.h"
#include "mrcp_session.h"
#include "apt_pool.h"

MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(const char *id, void *obj, mrcp_version_e mrcp_version, apr_pool_t *pool)
{
	mrcp_sig_agent_t *sig_agent = apr_palloc(pool,sizeof(mrcp_sig_agent_t));
	sig_agent->id = id;
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

/** Allocate MRCP signaling settings */
MRCP_DECLARE(mrcp_sig_settings_t*) mrcp_signaling_settings_alloc(apr_pool_t *pool)
{
	mrcp_sig_settings_t *settings = apr_palloc(pool,sizeof(mrcp_sig_settings_t));
	settings->server_ip = NULL;
	settings->server_port = 0;
	settings->user_name = NULL;
	settings->resource_location = NULL;
	settings->resource_map = apr_table_make(pool,2);
	settings->force_destination = FALSE;
	settings->feature_tags = NULL;
	return settings;
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
	session->log_obj = NULL;
	session->name = NULL;
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
