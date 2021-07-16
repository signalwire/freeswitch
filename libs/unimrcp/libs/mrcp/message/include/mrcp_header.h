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

#ifndef MRCP_HEADER_H
#define MRCP_HEADER_H

/**
 * @file mrcp_header.h
 * @brief MRCP Message Header Definition
 */ 

#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** 
 * Allows external applications to trigger whether 
 * transaprent header fields are supported or not
 */
#define TRANSPARENT_HEADER_FIELDS_SUPPORT

/** MRCP message header declaration */
typedef struct mrcp_message_header_t mrcp_message_header_t;
/** MRCP channel-id declaration */
typedef struct mrcp_channel_id mrcp_channel_id;


/** MRCP message-header */
struct mrcp_message_header_t {
	/** MRCP generic-header */
	mrcp_header_accessor_t generic_header_accessor;
	/** MRCP resource specific header */
	mrcp_header_accessor_t resource_header_accessor;

	/** Header section (collection of header fields)*/
	apt_header_section_t   header_section;
};

/** MRCP channel-identifier */
struct mrcp_channel_id {
	/** Unambiguous string identifying the MRCP session */
	apt_str_t        session_id;
	/** MRCP resource name */
	apt_str_t        resource_name;
};


/** Initialize MRCP message-header */
static APR_INLINE void mrcp_message_header_init(mrcp_message_header_t *header)
{
	mrcp_header_accessor_init(&header->generic_header_accessor);
	mrcp_header_accessor_init(&header->resource_header_accessor);
	apt_header_section_init(&header->header_section);
}

/** Allocate MRCP message-header data */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_data_alloc(
						mrcp_message_header_t *header,
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool);

/** Create MRCP message-header */
MRCP_DECLARE(mrcp_message_header_t*) mrcp_message_header_create(
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool);

/** Destroy MRCP message-header */
static APR_INLINE void mrcp_message_header_destroy(mrcp_message_header_t *header)
{
	mrcp_header_destroy(&header->generic_header_accessor);
	mrcp_header_destroy(&header->resource_header_accessor);
}

/** Add MRCP header field */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_add(mrcp_message_header_t *header, apt_header_field_t *header_field, apr_pool_t *pool);


/** Set (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_set(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool);

/** Get (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_get(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, const mrcp_message_header_t *mask_header, apr_pool_t *pool);

/** Inherit (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_inherit(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool);

/** Parse MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_parse(mrcp_message_header_t *header, apr_pool_t *pool);


/** Initialize MRCP channel-identifier */
MRCP_DECLARE(void) mrcp_channel_id_init(mrcp_channel_id *channel_id);

/** Parse MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_parse(mrcp_channel_id *channel_id, mrcp_message_header_t *header, apr_pool_t *pool);

/** Generate MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_generate(mrcp_channel_id *channel_id, apt_text_stream_t *text_stream);



APT_END_EXTERN_C

#endif /* MRCP_HEADER_H */
