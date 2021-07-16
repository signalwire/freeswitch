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

#include <apr_hash.h>
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "mrcp_resource.h"
#include "mrcp_generic_header.h"

/** Resource factory definition (aggregation of resources) */
struct mrcp_resource_factory_t {
	/** Array of MRCP resources (reference by id) */
	mrcp_resource_t **resource_array;
	/** Number of MRCP resources */
	apr_size_t        resource_count;
	/** Hash of MRCP resources (reference by name) */
	apr_hash_t       *resource_hash;
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
	resource_factory->resource_hash = apr_hash_make(pool);
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

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource)
{	
	if(!resource || resource->id >= resource_factory->resource_count) {
		/* invalid params */
		return FALSE;
	}
	if(resource_factory->resource_array[resource->id]) {
		/* resource with specified id already exists */
		return FALSE;
	}
	if(mrcp_resource_validate(resource) != TRUE) {
		/* invalid resource */
		return FALSE;
	}
	resource_factory->resource_array[resource->id] = resource;
	apr_hash_set(resource_factory->resource_hash,resource->name.buf,resource->name.length,resource);
	return TRUE;
}

/** Get MRCP resource by resource id */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_get(const mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id)
{
	if(resource_id >= resource_factory->resource_count) {
		return NULL;
	}
	return resource_factory->resource_array[resource_id];
}

/** Find MRCP resource by resource name */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_find(const mrcp_resource_factory_t *resource_factory, const apt_str_t *name)
{
	if(!name->buf || !name->length) {
		return NULL;
	}

	return apr_hash_get(resource_factory->resource_hash,name->buf,name->length);
}
