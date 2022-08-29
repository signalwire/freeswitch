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

#ifndef MRCP_RESOURCE_FACTORY_H
#define MRCP_RESOURCE_FACTORY_H

/**
 * @file mrcp_resource_factory.h
 * @brief Aggregation of MRCP Resources
 */ 

#include "apt_text_stream.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Create MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_create(apr_size_t resource_count, apr_pool_t *pool);

/** Destroy MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_resource_factory_destroy(mrcp_resource_factory_t *resource_factory);

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource);

/** Get MRCP resource by resource id */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_get(const mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id);

/** Find MRCP resource by resource name */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_find(const mrcp_resource_factory_t *resource_factory, const apt_str_t *name);


APT_END_EXTERN_C

#endif /* MRCP_RESOURCE_FACTORY_H */
