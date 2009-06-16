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

#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "mrcp_resource.h"
#include "mrcp_generic_header.h"

/** Resource factory definition (aggregation of resources) */
struct mrcp_resource_factory_t {
	/** Array of MRCP resources */
	mrcp_resource_t           **resource_array;
	/** Number of MRCP resources */
	apr_size_t                  resource_count;
	/** String table of MRCP resource names */
	const apt_str_table_item_t *string_table;
};

/** Create MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_create(apr_size_t resource_count, apr_pool_t *pool)
{
	apr_size_t i;
	mrcp_resource_factory_t *resource_factory;
	if(resource_count == 0) {
		return NULL;
	}

	resource_factory = apr_palloc(pool,sizeof(mrcp_resource_factory_t));
	resource_factory->resource_count = resource_count;
	resource_factory->resource_array = apr_palloc(pool,sizeof(mrcp_resource_t*)*resource_count);
	for(i=0; i<resource_count; i++) {
		resource_factory->resource_array[i] = NULL;
	}
	resource_factory->string_table = NULL;
	
	return resource_factory;
}

/** Destroy MRCP resource container */
MRCP_DECLARE(apt_bool_t) mrcp_resource_factory_destroy(mrcp_resource_factory_t *resource_factory)
{
	if(resource_factory->resource_array) {
		resource_factory->resource_array = NULL;
	}
	resource_factory->resource_count = 0;
	return TRUE;
}

/** Set MRCP resource string table */
MRCP_DECLARE(apt_bool_t) mrcp_resource_string_table_set(mrcp_resource_factory_t *resource_factory, const apt_str_table_item_t *string_table)
{
	resource_factory->string_table = string_table;
	return TRUE;
}

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource, mrcp_resource_id resource_id)
{	
	if(!resource || resource_id >= resource_factory->resource_count) {
		/* invalid params */
		return FALSE;
	}
	if(resource_factory->resource_array[resource_id]) {
		/* resource with specified id already exists */
		return FALSE;
	}
	resource->id = resource_id;
	if(mrcp_resource_validate(resource) != TRUE) {
		/* invalid resource */
		return FALSE;
	}
	resource_factory->resource_array[resource->id] = resource;
	return TRUE;
}

/** Get MRCP resource by resource id */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_get(mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id)
{
	if(resource_id >= resource_factory->resource_count) {
		return NULL;
	}
	return resource_factory->resource_array[resource_id];
}


/** Set header accessor interface */
static APR_INLINE void mrcp_generic_header_accessor_set(mrcp_message_t *message)
{
	message->header.generic_header_accessor.vtable = mrcp_generic_header_vtable_get(message->start_line.version);
}

/** Associate MRCP resource specific data by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_id(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;
	const apt_str_t *name;
	resource = mrcp_resource_get(resource_factory,message->channel_id.resource_id);
	if(!resource) {
		return FALSE;
	}
	name = mrcp_resource_name_get(resource_factory,resource->id);
	if(!name) {
		return FALSE;
	}
	/* associate resource_name and resource_id */
	message->channel_id.resource_name = *name;

	mrcp_generic_header_accessor_set(message);
	return resource->resourcify_message_by_id(resource,message);
}

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_name(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;
	/* associate resource_name and resource_id */
	const apt_str_t *name = &message->channel_id.resource_name;
	message->channel_id.resource_id = mrcp_resource_id_find(resource_factory,name);
	resource = mrcp_resource_get(resource_factory,message->channel_id.resource_id);
	if(!resource) {
		return FALSE;
	}

	mrcp_generic_header_accessor_set(message);
	return resource->resourcify_message_by_name(resource,message);
}

/** Get resource name associated with specified resource id */
MRCP_DECLARE(const apt_str_t*) mrcp_resource_name_get(mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id)
{
	return apt_string_table_str_get(resource_factory->string_table,resource_factory->resource_count,resource_id);
}

/** Find resource id associated with specified resource name */
MRCP_DECLARE(mrcp_resource_id) mrcp_resource_id_find(mrcp_resource_factory_t *resource_factory, const apt_str_t *resource_name)
{
	return apt_string_table_id_find(resource_factory->string_table,resource_factory->resource_count,resource_name);
}
