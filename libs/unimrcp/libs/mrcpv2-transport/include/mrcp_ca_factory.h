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

#ifndef MRCP_CA_FACTORY_H
#define MRCP_CA_FACTORY_H

/**
 * @file mrcp_ca_factory.h
 * @brief Factory of MRCPv2 Connection Agents
 */ 

#include "mrcp_connection_types.h"

APT_BEGIN_EXTERN_C

/** Create factory of connection agents. */
MRCP_DECLARE(mrcp_ca_factory_t*) mrcp_ca_factory_create(apr_pool_t *pool);

/** Add connection agent to factory. */
MRCP_DECLARE(apt_bool_t) mrcp_ca_factory_agent_add(mrcp_ca_factory_t *mpf_factory, mrcp_connection_agent_t *agent);

/** Determine whether factory is empty. */
MRCP_DECLARE(apt_bool_t) mrcp_ca_factory_is_empty(const mrcp_ca_factory_t *factory);

/** Select next available agent. */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_ca_factory_agent_select(mrcp_ca_factory_t *factory);

APT_END_EXTERN_C

#endif /* MRCP_CA_FACTORY_H */
