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

#ifndef __MRCP_ENGINE_FACTORY_H__
#define __MRCP_ENGINE_FACTORY_H__

/**
 * @file mrcp_engine_factory.h
 * @brief Factory of MRCP Engines
 */ 

#include "mrcp_engine_iface.h"

APT_BEGIN_EXTERN_C

/** Opaque engine factory declaration */
typedef struct mrcp_engine_factory_t mrcp_engine_factory_t;

/** Create engine factory */
MRCP_DECLARE(mrcp_engine_factory_t*) mrcp_engine_factory_create(apr_pool_t *pool);

/** Destroy registered engines and the factory */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_destroy(mrcp_engine_factory_t *factory);

/** Open registered engines */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_open(mrcp_engine_factory_t *factory);

/** Close registered engines */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_close(mrcp_engine_factory_t *factory);


/** Register engine */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_engine_register(mrcp_engine_factory_t *factory, mrcp_engine_t *engine, const char *name);

/** Get engine by name */
MRCP_DECLARE(mrcp_engine_t*) mrcp_engine_factory_engine_get(mrcp_engine_factory_t *factory, const char *name);

/** Find engine by resource identifier */
MRCP_DECLARE(mrcp_engine_t*) mrcp_engine_factory_engine_find(mrcp_engine_factory_t *factory, mrcp_resource_id resource_id);


APT_END_EXTERN_C

#endif /*__MRCP_ENGINE_FACTORY_H__*/
