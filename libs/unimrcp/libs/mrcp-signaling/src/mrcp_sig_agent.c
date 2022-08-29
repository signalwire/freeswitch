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

#include "mrcp_sig_agent.h"
#include "mrcp_session.h"
#include "apt_pool.h"

/** Factory of MRCP signaling agents */
struct mrcp_sa_factory_t {
	/** Array of pointers to signaling agents */
	apr_array_header_t   *agents_arr;
	/** Index of the current agent */
	int                   index;
};

/** Create signaling agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(const char *id, void *obj, apr_pool_t *pool)
{
	mrcp_sig_agent_t *sig_agent = apr_palloc(pool,sizeof(mrcp_sig_agent_t));
	sig_agent->id = id;
	sig_agent->pool = pool;
	sig_agent->obj = obj;
	sig_agent->resource_factory = NULL;
	sig_agent->parent = NULL;
	sig_agent->task = NULL;
	sig_agent->msg_pool = NULL;
	sig_agent->create_server_session = NULL;
	sig_agent->create_client_session = NULL;
	return sig_agent;
}

/** Create factory of signaling agents */
MRCP_DECLARE(mrcp_sa_factory_t*) mrcp_sa_factory_create(apr_pool_t *pool)
{
	mrcp_sa_factory_t *sa_factory = apr_palloc(pool,sizeof(mrcp_sa_factory_t));
	sa_factory->agents_arr = apr_array_make(pool,1,sizeof(mrcp_sig_agent_t*));
	sa_factory->index = 0;
	return sa_factory;
}

/** Add signaling agent to pool */
MRCP_DECLARE(apt_bool_t) mrcp_sa_factory_agent_add(mrcp_sa_factory_t *sa_factory, mrcp_sig_agent_t *sig_agent)
{
	mrcp_sig_agent_t **slot;
	if(!sig_agent)
		return FALSE;

	slot = apr_array_push(sa_factory->agents_arr);
	*slot = sig_agent;
	return TRUE;
}

/** Determine whether factory is empty. */
MRCP_DECLARE(apt_bool_t) mrcp_sa_factory_is_empty(const mrcp_sa_factory_t *sa_factory)
{
	return apr_is_empty_array(sa_factory->agents_arr);
}

/** Select next available signaling agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sa_factory_agent_select(mrcp_sa_factory_t *sa_factory)
{
	mrcp_sig_agent_t *sig_agent = APR_ARRAY_IDX(sa_factory->agents_arr, sa_factory->index, mrcp_sig_agent_t*);
	if(++sa_factory->index == sa_factory->agents_arr->nelts) {
		sa_factory->index = 0;
	}
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
	apr_pool_t *pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}

	return mrcp_session_create_ex(pool,TRUE,padding);
}

MRCP_DECLARE(mrcp_session_t*) mrcp_session_create_ex(apr_pool_t *pool, apt_bool_t take_ownership, apr_size_t padding)
{
	mrcp_session_t *session;
	session = apr_palloc(pool,sizeof(mrcp_session_t)+padding);
	session->self_owned = take_ownership;
	session->pool = pool;
	session->obj = NULL;
	session->log_obj = NULL;
	session->name = NULL;
	session->signaling_agent = NULL;
	session->connection_agent = NULL;
	session->media_engine = NULL;
	session->rtp_factory = NULL;
	session->request_vtable = NULL;
	session->response_vtable = NULL;
	session->event_vtable = NULL;
	apt_string_reset(&session->id);
	session->last_request_id = 0;
	return session;
}

MRCP_DECLARE(void) mrcp_session_destroy(mrcp_session_t *session)
{
	if(session->pool && session->self_owned == TRUE) {
		apr_pool_destroy(session->pool);
	}
}
