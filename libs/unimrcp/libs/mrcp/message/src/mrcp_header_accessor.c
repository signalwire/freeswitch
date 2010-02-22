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

#include <stdio.h>
#include "mrcp_header_accessor.h"

typedef enum {
	MRCP_HEADER_FIELD_NONE       = 0x0,
	MRCP_HEADER_FIELD_NAME       = 0x1,
	MRCP_HEADER_FIELD_VALUE      = 0x2,
	MRCP_HEADER_FIELD_NAME_VALUE = MRCP_HEADER_FIELD_NAME | MRCP_HEADER_FIELD_VALUE
} mrcp_header_property_e;


MRCP_DECLARE(apt_bool_t) mrcp_header_parse(mrcp_header_accessor_t *accessor, const apt_pair_t *pair, apr_pool_t *pool)
{
	size_t id;
	if(!accessor->vtable) {
		return FALSE;
	}

	id = apt_string_table_id_find(accessor->vtable->field_table,accessor->vtable->field_count,&pair->name);
	if(id >= accessor->vtable->field_count) {
		return FALSE;
	}

	if(!pair->value.length) {
		mrcp_header_name_property_add(accessor,id);
		return TRUE;
	}

	if(accessor->vtable->parse_field(accessor,id,&pair->value,pool) == FALSE) {
		return FALSE;
	}
	
	mrcp_header_property_add(accessor,id);
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_generate(mrcp_header_accessor_t *accessor, apt_text_stream_t *text_stream)
{
	const apt_str_t *name;
	apr_size_t i,j;
	char prop;

	if(!accessor->vtable) {
		return FALSE;
	}

	for(i=0, j=0; i<accessor->vtable->field_count && j<accessor->counter; i++) {
		prop = accessor->properties[i];
		if((prop & MRCP_HEADER_FIELD_NAME) == MRCP_HEADER_FIELD_NAME) {
			j++;
			name = apt_string_table_str_get(accessor->vtable->field_table,accessor->vtable->field_count,i);
			if(!name) continue;
			
			apt_text_header_name_generate(name,text_stream);
			if((prop & MRCP_HEADER_FIELD_VALUE) == MRCP_HEADER_FIELD_VALUE) {
				accessor->vtable->generate_field(accessor,i,text_stream);
			}
			apt_text_eol_insert(text_stream);
		}
	}

	return TRUE;
}

MRCP_DECLARE(void) mrcp_header_property_add(mrcp_header_accessor_t *accessor, apr_size_t id)
{
	if(!accessor->vtable) {
		return;
	}

	if(id < accessor->vtable->field_count) {
		char *prop = &accessor->properties[id];
		if((*prop & MRCP_HEADER_FIELD_NAME) != MRCP_HEADER_FIELD_NAME) {
			accessor->counter++;
		}
		*prop = MRCP_HEADER_FIELD_NAME_VALUE;
	}
}

MRCP_DECLARE(void) mrcp_header_name_property_add(mrcp_header_accessor_t *accessor, apr_size_t id)
{
	if(!accessor->vtable) {
		return;
	}

	if(id < accessor->vtable->field_count) {
		char *prop = &accessor->properties[id];
		if((*prop & MRCP_HEADER_FIELD_NAME) != MRCP_HEADER_FIELD_NAME) {
			*prop = MRCP_HEADER_FIELD_NAME;
			accessor->counter++;
		}
	}
}


MRCP_DECLARE(void) mrcp_header_property_remove(mrcp_header_accessor_t *accessor, apr_size_t id)
{
	if(!accessor->vtable) {
		return;
	}

	if(id < accessor->vtable->field_count) {
		char *prop = &accessor->properties[id];
		if((*prop & MRCP_HEADER_FIELD_NAME) == MRCP_HEADER_FIELD_NAME) {
			accessor->counter--;
		}
		*prop = MRCP_HEADER_FIELD_NONE;
	}
}

MRCP_DECLARE(apt_bool_t) mrcp_header_property_check(mrcp_header_accessor_t *accessor, apr_size_t id)
{
	if(!accessor->vtable) {
		return FALSE;
	}

	if((id < accessor->vtable->field_count) && accessor->properties) {
		if((accessor->properties[id] & MRCP_HEADER_FIELD_NAME) == MRCP_HEADER_FIELD_NAME) {
			return TRUE;
		}
	}
	return FALSE;
}


MRCP_DECLARE(apt_bool_t) mrcp_header_set(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, const mrcp_header_accessor_t *mask, apr_pool_t *pool)
{
	apr_size_t i,j;

	if(!accessor->vtable || !src->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(accessor,pool);

	for(i=0, j=0; i < src->vtable->field_count && j < src->counter; i++) {
		if((mask->properties[i] & src->properties[i] & MRCP_HEADER_FIELD_NAME) == MRCP_HEADER_FIELD_NAME) {
			j++;
			if((src->properties[i] & MRCP_HEADER_FIELD_VALUE) == MRCP_HEADER_FIELD_VALUE) {
				accessor->vtable->duplicate_field(accessor,src,i,pool);
				mrcp_header_property_add(accessor,i);
			}
			else {
				mrcp_header_name_property_add(accessor,i);
			}
		}
	}

	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_inherit(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *parent, apr_pool_t *pool)
{
	apr_size_t i,j;

	if(!accessor->vtable || !parent->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(accessor,pool);

	for(i=0, j=0; i<parent->vtable->field_count && j < parent->counter; i++) {
		if((parent->properties[i] & MRCP_HEADER_FIELD_NAME) == MRCP_HEADER_FIELD_NAME) {
			j++;
			if((accessor->properties[i] & MRCP_HEADER_FIELD_NAME) != MRCP_HEADER_FIELD_NAME) {
				if((parent->properties[i] & MRCP_HEADER_FIELD_VALUE) == MRCP_HEADER_FIELD_VALUE) {
					accessor->vtable->duplicate_field(accessor,parent,i,pool);
					mrcp_header_property_add(accessor,i);
				}
				else {
					mrcp_header_name_property_add(accessor,i);
				}
			}
		}
	}

	return TRUE;
}

/** Generate completion-cause */
MRCP_DECLARE(apt_bool_t) mrcp_completion_cause_generate(const apt_str_table_item_t table[], apr_size_t size, apr_size_t cause, apt_text_stream_t *stream)
{
	int length;
	const apt_str_t *name = apt_string_table_str_get(table,size,cause);
	if(!name) {
		return FALSE;
	}
	length = sprintf(stream->pos,"%03"APR_SIZE_T_FMT" ",cause);
	if(length <= 0) {
		return FALSE;
	}
	stream->pos += length;

	memcpy(stream->pos,name->buf,name->length);
	stream->pos += name->length;
	return TRUE;
}
