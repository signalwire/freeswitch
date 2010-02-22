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

#ifndef __MRCP_HEADER_ACCESSOR_H__
#define __MRCP_HEADER_ACCESSOR_H__

/**
 * @file mrcp_header_accessor.h
 * @brief Abstract MRCP Header Accessor
 */ 

#include "apt_string_table.h"
#include "apt_text_stream.h"
#include "mrcp.h"

APT_BEGIN_EXTERN_C

/** MRCP header accessor declaration */
typedef struct mrcp_header_accessor_t mrcp_header_accessor_t;
/** MRCP header vtable declaration */
typedef struct mrcp_header_vtable_t mrcp_header_vtable_t;

/** MRCP header accessor interface */
struct mrcp_header_vtable_t {
	/** Allocate actual header data */
	void* (*allocate)(mrcp_header_accessor_t *accessor, apr_pool_t *pool);
	/** Destroy header data */
	void (*destroy)(mrcp_header_accessor_t *accessor);

	/** Parse header field */
	apt_bool_t (*parse_field)(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool);
	/** Generate header field */
	apt_bool_t (*generate_field)(mrcp_header_accessor_t *accessor, apr_size_t id, apt_text_stream_t *value);
	/** Duplicate header field */
	apt_bool_t (*duplicate_field)(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, apr_pool_t *pool);

	/** Table of fields  */
	const apt_str_table_item_t *field_table;
	/** Number of fields  */
	apr_size_t                  field_count;
};


/** Initialize header vtable */
static APR_INLINE void mrcp_header_vtable_init(mrcp_header_vtable_t *vtable)
{
	vtable->allocate = NULL;
	vtable->destroy = NULL;
	vtable->parse_field = NULL;
	vtable->generate_field = NULL;
	vtable->duplicate_field = NULL;
	vtable->field_table = NULL;
	vtable->field_count = 0;
}

/** Validate header vtable */
static APR_INLINE apt_bool_t mrcp_header_vtable_validate(const mrcp_header_vtable_t *vtable)
{
	return (vtable->allocate && vtable->destroy && 
		vtable->parse_field && vtable->generate_field &&
		vtable->duplicate_field && vtable->field_table && 
		vtable->field_count) ?	TRUE : FALSE;
}


/** MRCP header accessor */
struct mrcp_header_accessor_t {
	/** Actual header data allocated by accessor */
	void                       *data;

	/** Array properties (mrcp_header_property_e) */
	char                       *properties;
	/** Number of filled properties (header fields) */
	apr_size_t                  counter;
	
	/** Header accessor interface */
	const mrcp_header_vtable_t *vtable;
};

/** Initialize header accessor */
static APR_INLINE void mrcp_header_accessor_init(mrcp_header_accessor_t *accessor)
{
	accessor->data = NULL;
	accessor->properties = NULL;
	accessor->counter = 0;
	accessor->vtable = NULL;
}


/** Allocate header data */
static APR_INLINE void* mrcp_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	if(accessor->data) {
		return accessor->data;
	}
	if(!accessor->vtable || !accessor->vtable->allocate) {
		return NULL;
	}
	accessor->properties = (char*)apr_pcalloc(pool,sizeof(char)*accessor->vtable->field_count);
	accessor->counter = 0;
	return accessor->vtable->allocate(accessor,pool);
}

/** Destroy header data */
static APR_INLINE void mrcp_header_destroy(mrcp_header_accessor_t *accessor)
{
	if(!accessor->vtable || !accessor->vtable->destroy) {
		return;
	}
	accessor->vtable->destroy(accessor);
}


/** Parse header */
MRCP_DECLARE(apt_bool_t) mrcp_header_parse(mrcp_header_accessor_t *accessor, const apt_pair_t *pair, apr_pool_t *pool);

/** Generate header */
MRCP_DECLARE(apt_bool_t) mrcp_header_generate(mrcp_header_accessor_t *accessor, apt_text_stream_t *text_stream);

/** Set header */
MRCP_DECLARE(apt_bool_t) mrcp_header_set(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, const mrcp_header_accessor_t *mask, apr_pool_t *pool);

/** Inherit header */
MRCP_DECLARE(apt_bool_t) mrcp_header_inherit(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *parent, apr_pool_t *pool);


/** Add name:value property */
MRCP_DECLARE(void) mrcp_header_property_add(mrcp_header_accessor_t *accessor, apr_size_t id);

/** Remove property */
MRCP_DECLARE(void) mrcp_header_property_remove(mrcp_header_accessor_t *accessor, apr_size_t id);

/** Check the property */
MRCP_DECLARE(apt_bool_t) mrcp_header_property_check(mrcp_header_accessor_t *accessor, apr_size_t id);

/** Add name only property */
MRCP_DECLARE(void) mrcp_header_name_property_add(mrcp_header_accessor_t *accessor, apr_size_t id);


/** Generate completion-cause */
MRCP_DECLARE(apt_bool_t) mrcp_completion_cause_generate(const apt_str_table_item_t table[], apr_size_t size, apr_size_t cause, apt_text_stream_t *stream);

APT_END_EXTERN_C

#endif /*__MRCP_HEADER_ACCESSOR_H__*/
