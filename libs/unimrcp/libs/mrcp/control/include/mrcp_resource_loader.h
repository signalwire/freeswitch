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

#ifndef MRCP_RESOURCE_LOADER_H
#define MRCP_RESOURCE_LOADER_H

/**
 * @file mrcp_resource_loader.h
 * @brief MRCP Resource Loader
 */ 

#include "apt_string.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Opaque resource loader declaration */
typedef struct mrcp_resource_loader_t mrcp_resource_loader_t;


/** Create MRCP resource loader */
MRCP_DECLARE(mrcp_resource_loader_t*) mrcp_resource_loader_create(apt_bool_t load_all_resources, apr_pool_t *pool);

/** Load all MRCP resources */
MRCP_DECLARE(apt_bool_t) mrcp_resources_load(mrcp_resource_loader_t *loader);

/** Load MRCP resource by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_resource_load(mrcp_resource_loader_t *loader, const apt_str_t *name);

/** Load MRCP resource by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_resource_load_by_id(mrcp_resource_loader_t *loader, mrcp_resource_id id);

/** Get MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_get(const mrcp_resource_loader_t *loader);

APT_END_EXTERN_C

#endif /* MRCP_RESOURCE_LOADER_H */
