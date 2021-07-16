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

#include "mrcp_header_accessor.h"


/** Parse header field value */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_value_parse(mrcp_header_accessor_t *accessor, apt_header_field_t *header_field, apr_pool_t *pool)
{
	apr_size_t id;
	if(!accessor->vtable) {
		return FALSE;
	}

	id = apt_string_table_id_find(accessor->vtable->field_table,accessor->vtable->field_count,&header_field->name);
	if(id >= accessor->vtable->field_count) {
		return FALSE;
	}
	header_field->id = id;

	if(header_field->value.length) {
		if(accessor->vtable->parse_field(accessor,header_field->id,&header_field->value,pool) == FALSE) {
			return FALSE;
		}
	}
	
	return TRUE;
}

/** Generate header field value */
MRCP_DECLARE(apt_header_field_t*) mrcp_header_field_value_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_bool_t empty_value, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_str_t *name;

	if(!accessor->vtable) {
		return NULL;
	}
	
	header_field = apt_header_field_alloc(pool);
	name = apt_string_table_str_get(accessor->vtable->field_table,accessor->vtable->field_count,id);
	if(name) {
		header_field->name = *name;
	}

	if(empty_value == FALSE) {
		if(accessor->vtable->generate_field(accessor,id,&header_field->value,pool) == FALSE) {
			return NULL;
		}
	}

	return header_field;
}

/** Duplicate header field value */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_value_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src_accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	if(!accessor->vtable) {
		return FALSE;
	}
	
	if(value->length) {
		if(accessor->vtable->duplicate_field(accessor,src_accessor,id,value,pool) == FALSE) {
			return FALSE;
		}
	}

	return TRUE;
}
