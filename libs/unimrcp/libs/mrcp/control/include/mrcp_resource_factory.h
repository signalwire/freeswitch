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

#ifndef __MRCP_RESOURCE_FACTORY_H__
#define __MRCP_RESOURCE_FACTORY_H__

/**
 * @file mrcp_resource_factory.h
 * @brief Aggregation of MRCP Resources
 */ 

#include "apt_string_table.h"
#include "apt_text_stream.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Create MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_create(apr_size_t resource_count, apr_pool_t *pool);

/** Destroy MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_resource_factory_destroy(mrcp_resource_factory_t *resource_factory);

/** Set MRCP resource string table */
MRCP_DECLARE(apt_bool_t) mrcp_resource_string_table_set(mrcp_resource_factory_t *resource_factory, const apt_str_table_item_t *string_table);

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource, mrcp_resource_id resource_id);

/** Get MRCP resource by resource id */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_get(mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id);

/** Get resource name associated with specified resource id */
MRCP_DECLARE(const apt_str_t*) mrcp_resource_name_get(mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id);

/** Find resource id associated with specified resource name */
MRCP_DECLARE(mrcp_resource_id) mrcp_resource_id_find(mrcp_resource_factory_t *resource_factory, const apt_str_t *resource_name);


/** Associate MRCP resource specific data by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_id(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message);

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_name(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message);


APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_FACTORY_H__*/
