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
 * $Id: mrcp_ca_factory.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <apr_tables.h>
#include "mrcp_ca_factory.h"

/** Factory of MRCPv2 connection agents */
struct mrcp_ca_factory_t {
	/** Array of pointers to agents */
	apr_array_header_t   *agent_arr;
	/** Index of the current agent */
	int                   index;
};

/** Create factory of connection agents. */
MRCP_DECLARE(mrcp_ca_factory_t*) mrcp_ca_factory_create(apr_pool_t *pool)
{
	mrcp_ca_factory_t *factory = apr_palloc(pool,sizeof(mrcp_ca_factory_t));
	factory->agent_arr = apr_array_make(pool,1,sizeof(mrcp_connection_agent_t*));
	factory->index = 0;
	return factory;
}

/** Add connection agent to factory. */
MRCP_DECLARE(apt_bool_t) mrcp_ca_factory_agent_add(mrcp_ca_factory_t *factory, mrcp_connection_agent_t *agent)
{
	mrcp_connection_agent_t **slot;
	if(!agent)
		return FALSE;

	slot = apr_array_push(factory->agent_arr);
	*slot = agent;
	return TRUE;
}

/** Determine whether factory is empty. */
MRCP_DECLARE(apt_bool_t) mrcp_ca_factory_is_empty(const mrcp_ca_factory_t *factory)
{
	return apr_is_empty_array(factory->agent_arr);
}

/** Select next available agent. */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_ca_factory_agent_select(mrcp_ca_factory_t *factory)
{
	mrcp_connection_agent_t *agent = APR_ARRAY_IDX(factory->agent_arr, factory->index, mrcp_connection_agent_t*);
	if(++factory->index == factory->agent_arr->nelts) {
		factory->index = 0;
	}
	return agent;
}
