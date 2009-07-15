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

#ifndef __MRCP_RESOURCE_H__
#define __MRCP_RESOURCE_H__

/**
 * @file mrcp_resource.h
 * @brief Abstract MRCP Resource
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"
#include "mrcp_state_machine.h"

APT_BEGIN_EXTERN_C


/** MRCP resource definition */
struct mrcp_resource_t {
	/** MRCP resource identifier */
	mrcp_resource_id            id;

	/** Set resource specific data in a message by resource id */
	apt_bool_t (*resourcify_message_by_id)(mrcp_resource_t *resource, mrcp_message_t *message);
	/** Set resource specific data in a message by resource name */
	apt_bool_t (*resourcify_message_by_name)(mrcp_resource_t *resource, mrcp_message_t *message);

	/** Create client side state machine */
	mrcp_state_machine_t* (*create_client_state_machine)(void *obj, mrcp_version_e version, apr_pool_t *pool);
	/** Create server side state machine */
	mrcp_state_machine_t* (*create_server_state_machine)(void *obj, mrcp_version_e version, apr_pool_t *pool);
};

/** Initialize MRCP resource */
static APR_INLINE void mrcp_resource_init(mrcp_resource_t *resource)
{
	resource->resourcify_message_by_id = NULL;
	resource->resourcify_message_by_name = NULL;
	resource->create_client_state_machine = NULL;
	resource->create_server_state_machine = NULL;
}

/** Validate MRCP resource */
static APR_INLINE apt_bool_t mrcp_resource_validate(mrcp_resource_t *resource)
{
	return (resource->resourcify_message_by_id && 
		resource->resourcify_message_by_name) ? TRUE : FALSE;
}

APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_H__*/
