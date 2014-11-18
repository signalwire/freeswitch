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
 * $Id: mrcp_message.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_MESSAGE_H
#define MRCP_MESSAGE_H

/**
 * @file mrcp_message.h
 * @brief MRCP Message Definition
 */ 

#include "mrcp_types.h"
#include "mrcp_start_line.h"
#include "mrcp_header.h"
#include "mrcp_generic_header.h"

APT_BEGIN_EXTERN_C

/** Macro to log channel identifier of the message */
#define MRCP_MESSAGE_SIDRES(message) \
	(message)->channel_id.session_id.buf, (message)->channel_id.resource_name.buf

/** MRCP message */
struct mrcp_message_t {
	/** Start-line of MRCP message */
	mrcp_start_line_t      start_line;
	/** Channel-identifier of MRCP message */
	mrcp_channel_id        channel_id;
	/** Header of MRCP message */
	mrcp_message_header_t  header;
	/** Body of MRCP message */
	apt_str_t              body;

	/** Associated MRCP resource */
	const mrcp_resource_t *resource;
	/** Memory pool to allocate memory from */
	apr_pool_t            *pool;
};

/**
 * Create an MRCP message.
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_message_t*) mrcp_message_create(apr_pool_t *pool);

/**
 * Create an MRCP request message.
 * @param resource the MRCP resource to use
 * @param version the MRCP version to use
 * @param method_id the MRCP resource specific method identifier
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_message_t*) mrcp_request_create(
								const mrcp_resource_t *resource, 
								mrcp_version_e version, 
								mrcp_method_id method_id, 
								apr_pool_t *pool);

/**
 * Create an MRCP response message based on given request message.
 * @param request_message the MRCP request message to create a response for
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_message_t*) mrcp_response_create(const mrcp_message_t *request_message, apr_pool_t *pool);

/**
 * Create an MRCP event message based on given requuest message.
 * @param request_message the MRCP request message to create an event for
 * @param event_id the MRCP resource specific event identifier
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_message_t*) mrcp_event_create(
								const mrcp_message_t *request_message, 
								mrcp_method_id event_id, 
								apr_pool_t *pool);

/**
 * Associate MRCP resource with message.
 * @param message the message to associate resource with
 * @param resource the resource to associate
 */
MRCP_DECLARE(apt_bool_t) mrcp_message_resource_set(mrcp_message_t *message, const mrcp_resource_t *resource);

/** 
 * Validate MRCP message.
 * @param message the message to validate
 */
MRCP_DECLARE(apt_bool_t) mrcp_message_validate(mrcp_message_t *message);

/**
 * Destroy MRCP message.
 * @param message the message to destroy
 */
MRCP_DECLARE(void) mrcp_message_destroy(mrcp_message_t *message);


/**
 * Get MRCP generic header.
 * @param message the message to get generic header from
 */
static APR_INLINE mrcp_generic_header_t* mrcp_generic_header_get(const mrcp_message_t *message)
{
	return (mrcp_generic_header_t*) message->header.generic_header_accessor.data;
}

/**
 * Allocate (if not allocated) and get MRCP generic header.
 * @param message the message to prepare generic header for
 */
static APR_INLINE mrcp_generic_header_t* mrcp_generic_header_prepare(mrcp_message_t *message)
{
	return (mrcp_generic_header_t*) mrcp_header_allocate(&message->header.generic_header_accessor,message->pool);
}

/**
 * Add MRCP generic header field by specified property (numeric identifier).
 * @param message the message to add property for
 * @param id the numeric identifier to add
 */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_property_add(mrcp_message_t *message, apr_size_t id);

/**
 * Add only the name of MRCP generic header field specified by property (numeric identifier).
 * @param message the message to add property for
 * @param id the numeric identifier to add
 * @remark Should be used to construct empty header fiedls for GET-PARAMS requests
 */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_name_property_add(mrcp_message_t *message, apr_size_t id);

/**
 * Remove MRCP generic header field by specified property (numeric identifier).
 * @param message the message to remove property from
 * @param id the numeric identifier to remove
 */
static APR_INLINE apt_bool_t mrcp_generic_header_property_remove(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = apt_header_section_field_get(&message->header.header_section,id);
	if(header_field) {
		return apt_header_section_field_remove(&message->header.header_section,header_field);
	}
	return FALSE;
}

/**
 * Check whether specified by property (numeric identifier) MRCP generic header field is set or not.
 * @param message the message to use
 * @param id the numeric identifier to check
 */
static APR_INLINE apt_bool_t mrcp_generic_header_property_check(const mrcp_message_t *message, apr_size_t id)
{
	return apt_header_section_field_check(&message->header.header_section,id);
}


/**
 * Get MRCP resource header.
 * @param message the message to get resource header from
 */
static APR_INLINE void* mrcp_resource_header_get(const mrcp_message_t *message)
{
	return message->header.resource_header_accessor.data;
}

/**
 * Allocate (if not allocated) and get MRCP resource header.
 * @param message the message to prepare resource header for
 */
static APR_INLINE void* mrcp_resource_header_prepare(mrcp_message_t *mrcp_message)
{
	return mrcp_header_allocate(&mrcp_message->header.resource_header_accessor,mrcp_message->pool);
}

/**
 * Add MRCP resource header field by specified property (numeric identifier).
 * @param message the message to add property for
 * @param id the numeric identifier to add
 */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_property_add(mrcp_message_t *message, apr_size_t id);

/** 
 * Add only the name of MRCP resource header field specified by property (numeric identifier).
 * @param message the message to add property for
 * @param id the numeric identifier to add
 * @remark Should be used to construct empty header fiedls for GET-PARAMS requests
 */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_name_property_add(mrcp_message_t *message, apr_size_t id);

/**
 * Remove MRCP resource header field by specified property (numeric identifier).
 * @param message the message to remove property from
 * @param id the numeric identifier to remove
 */
static APR_INLINE apt_bool_t mrcp_resource_header_property_remove(mrcp_message_t *message, apr_size_t id)
{
	apt_header_field_t *header_field = apt_header_section_field_get(&message->header.header_section,id + GENERIC_HEADER_COUNT);
	if(header_field) {
		return apt_header_section_field_remove(&message->header.header_section,header_field);
	}
	return FALSE;
}

/**
 * Check whether specified by property (numeric identifier) MRCP resource header field is set or not.
 * @param message the message to use
 * @param id the numeric identifier to check
 */
static APR_INLINE apt_bool_t mrcp_resource_header_property_check(const mrcp_message_t *message, apr_size_t id)
{
	return apt_header_section_field_check(&message->header.header_section,id + GENERIC_HEADER_COUNT);
}

/**
 * Add MRCP header field.
 * @param message the message to add header field for
 * @param header_field the header field to add
 */
static APR_INLINE apt_bool_t mrcp_message_header_field_add(mrcp_message_t *message, apt_header_field_t *header_field)
{
	return mrcp_header_field_add(&message->header,header_field,message->pool);
}

/**
 * Get the next MRCP header field.
 * @param message the message to use
 * @param header_field current header field
 * @remark Should be used to iterate on header fields
 *
 *	apt_header_field_t *header_field = NULL;
 *	while( (header_field = mrcp_message_next_header_field_get(message,header_field)) != NULL ) {
 *  }
 */
MRCP_DECLARE(apt_header_field_t*) mrcp_message_next_header_field_get(
										const mrcp_message_t *message, 
										apt_header_field_t *header_field);

APT_END_EXTERN_C

#endif /* MRCP_MESSAGE_H */
