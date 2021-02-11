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

#ifndef APT_HEADER_FIELD_H
#define APT_HEADER_FIELD_H

/**
 * @file apt_header_field.h
 * @brief Header Field Declaration (RFC5322)
 */ 

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** Header field declaration */
typedef struct apt_header_field_t apt_header_field_t;
/** Header section declaration */
typedef struct apt_header_section_t apt_header_section_t;

/** Header field */
struct apt_header_field_t {
	/** Ring entry */
	APR_RING_ENTRY(apt_header_field_t) link;

	/** Name of the header field */
	apt_str_t  name;
	/** Value of the header field */
	apt_str_t  value;

	/** Numeric identifier associated with name */
	apr_size_t id;
};

/** 
 * Header section 
 * @remark The header section is a collection of header fields. 
 * The header fields are stored in both a ring and an array.
 * The goal is to ensure efficient access and manipulation on the header fields.
 */
struct apt_header_section_t {
	/** List of header fields (name-value pairs) */
	APR_RING_HEAD(apt_head_t, apt_header_field_t) ring;
	/** Array of pointers to header fields */
	apt_header_field_t **arr;
	/** Max number of header fields */
	apr_size_t           arr_size;
};


/**
 * Allocate an empty header field.
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_alloc(apr_pool_t *pool);

/**
 * Create a header field using given name and value APT strings.
 * @param name the name of the header field
 * @param value the value of the header field
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_create(const apt_str_t *name, const apt_str_t *value, apr_pool_t *pool);

/**
 * Create a header field using given name and value C strings.
 * @param name the name of the header field
 * @param value the value of the header field
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_create_c(const char *name, const char *value, apr_pool_t *pool);

/**
 * Create a header field from entire text line consisting of a name and value pair.
 * @param line the text line, which consists of a name and value pair
 * @param separator the name and value separator
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_create_from_line(const apt_str_t *line, char separator, apr_pool_t *pool);

/**
 * Copy specified header field.
 * @param src_header_field the header field to copy
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_copy(const apt_header_field_t *src_header_field, apr_pool_t *pool);

/**
 * Initialize header section (collection of header fields).
 * @param header the header section to initialize
 */
APT_DECLARE(void) apt_header_section_init(apt_header_section_t *header);

/**
 * Allocate header section to set/get header fields by numeric identifiers.
 * @param header the header section to allocate
 * @param max_field_count the max number of header fields in the section (protocol dependent)
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_bool_t) apt_header_section_array_alloc(apt_header_section_t *header, apr_size_t max_field_count, apr_pool_t *pool);

/**
 * Add (append) header field to header section.
 * @param header the header section to add field to
 * @param header_field the header field to add
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_add(apt_header_section_t *header, apt_header_field_t *header_field);

/**
 * Insert header field to header section based on numreic identifier if specified.
 * @param header the header section to insert field into
 * @param header_field the header field to insert
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_insert(apt_header_section_t *header, apt_header_field_t *header_field);

/**
 * Set header field in the array of header fields using associated numeric identifier.
 * @param header the header section to set field for
 * @param header_field the header field to set
 * @remark Typically, the header field should be already added to the header section using apt_header_section_field_add()
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_set(apt_header_section_t *header, apt_header_field_t *header_field);

/**
 * Remove header field from header section.
 * @param header the header section to remove field from
 * @param header_field the header field to remove
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_remove(apt_header_section_t *header, apt_header_field_t *header_field);

/**
 * Check whether specified header field is set.
 * @param header the header section to use
 * @param id the identifier associated with the header_field to check
 */
static APR_INLINE apt_bool_t apt_header_section_field_check(const apt_header_section_t *header, apr_size_t id)
{
	if(id < header->arr_size) {
		return header->arr[id] ? TRUE : FALSE;
	}
	return FALSE;
}

/**
 * Get header field by specified identifier.
 * @param header the header section to use
 * @param id the identifier associated with the header_field
 */
static APR_INLINE apt_header_field_t* apt_header_section_field_get(const apt_header_section_t *header, apr_size_t id)
{
	if(id < header->arr_size) {
		return header->arr[id];
	}
	return NULL;
}

APT_END_EXTERN_C

#endif /* APT_HEADER_FIELD_H */
