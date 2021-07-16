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

#ifndef MRCP_HEADER_ACCESSOR_H
#define MRCP_HEADER_ACCESSOR_H

/**
 * @file mrcp_header_accessor.h
 * @brief Abstract MRCP Header Accessor
 */ 

#include "apt_text_stream.h"
#include "apt_header_field.h"
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

	/** Parse header field value */
	apt_bool_t (*parse_field)(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool);
	/** Generate header field value */
	apt_bool_t (*generate_field)(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_str_t *value, apr_pool_t *pool);
	/** Duplicate header field value */
	apt_bool_t (*duplicate_field)(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, const apt_str_t *value, apr_pool_t *pool);

	/** Table of fields  */
	const apt_str_table_item_t *field_table;
	/** Number of fields  */
	apr_size_t                  field_count;
};

/** MRCP header accessor */
struct mrcp_header_accessor_t {
	/** Actual header data allocated by accessor */
	void                       *data;
	/** Header accessor interface */
	const mrcp_header_vtable_t *vtable;
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


/** Initialize header accessor */
static APR_INLINE void mrcp_header_accessor_init(mrcp_header_accessor_t *accessor)
{
	accessor->data = NULL;
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


/** Parse header field value */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_value_parse(mrcp_header_accessor_t *accessor, apt_header_field_t *header_field, apr_pool_t *pool);

/** Generate header field value */
MRCP_DECLARE(apt_header_field_t*) mrcp_header_field_value_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_bool_t empty_value, apr_pool_t *pool);

/** Duplicate header field value */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_value_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src_accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MRCP_HEADER_ACCESSOR_H */
