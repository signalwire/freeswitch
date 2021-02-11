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

#ifndef MRCP_RESOURCE_H
#define MRCP_RESOURCE_H

/**
 * @file mrcp_resource.h
 * @brief Abstract MRCP Resource
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C


/** MRCP resource definition */
struct mrcp_resource_t {
	/** MRCP resource identifier */
	mrcp_resource_id id;
	/** MRCP resource name */
	apt_str_t        name;

	/** Get string table of methods */
	const apt_str_table_item_t* (*get_method_str_table)(mrcp_version_e version);
	/** Number of methods */
	apr_size_t       method_count;

	/** Get string table of events */
	const apt_str_table_item_t* (*get_event_str_table)(mrcp_version_e version);
	/** Number of events */
	apr_size_t       event_count;

	/** Get vtable of resource header */
	const mrcp_header_vtable_t* (*get_resource_header_vtable)(mrcp_version_e version);
};

/** Initialize MRCP resource */
static APR_INLINE mrcp_resource_t* mrcp_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = (mrcp_resource_t*) apr_palloc(pool, sizeof(mrcp_resource_t));
	resource->id = 0;
	apt_string_reset(&resource->name);
	resource->method_count = 0;
	resource->event_count = 0;
	resource->get_method_str_table = NULL;
	resource->get_event_str_table = NULL;
	resource->get_resource_header_vtable = NULL;
	return resource;
}

/** Validate MRCP resource */
static APR_INLINE apt_bool_t mrcp_resource_validate(mrcp_resource_t *resource)
{
	if(resource->method_count && resource->event_count &&
		resource->get_method_str_table && resource->get_event_str_table &&
		 resource->get_resource_header_vtable &&
		 resource->name.buf && resource->name.length) {
		return TRUE;
	}
	return FALSE;
}

APT_END_EXTERN_C

#endif /* MRCP_RESOURCE_H */
